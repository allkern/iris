#include <asmjit/ujit.h>

#include "vu.hpp"
#include "vu_def.hpp"
#include "vu_jit.hpp"
#include "vu_emit.hpp"

#include "jit_call.hpp"
#include "profile_counters.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <deque>

namespace iris::vu::jit {

using namespace asmjit;

struct AsyncJob {
    Vu* vu = nullptr;

    uint32_t tpc = 0;
    int cycles = 0;
    uint32_t src_len = 0;
    uint64_t src_hash = 0;

    std::vector <BlockEntry> entries;
};

struct AsyncResult {
    Vu* vu = nullptr;

    uint32_t tpc = 0;
    uint32_t src_len = 0;
    uint64_t src_hash = 0;

    std::vector <BlockEntry> entries;

    void (*func)(Vu*) = nullptr;
    uint32_t code_size = 0;
    bool failed = false;
};

struct Victim {
    uint32_t tpc = 0;
    uint32_t src_len = 0;
    uint64_t src_hash = 0;

    void (*func)(Vu*) = nullptr;
    uint32_t code_size = 0;

    std::vector <BlockEntry> entries;
};

constexpr size_t VICTIM_MAX = 8192;
constexpr size_t HOT_RUNS_MAX = 16384;

struct Jit {
    JitRuntime rt;

    ujit::Gp state;

    size_t code_size = 0;

    bool wants_flush = false;

    uint64_t compiled = 0;
    uint64_t failed = 0;

    bool stopping = false;

    std::thread worker;
    std::mutex mu;
    std::condition_variable cv;
    std::condition_variable idle_cv;

    std::deque <AsyncJob> queue;
    std::vector <AsyncResult> done;

    int in_flight = 0;

    std::unordered_map <uint64_t, Victim> victims;
    std::unordered_map <uint32_t, std::vector <uint32_t>> victim_lengths_by_tpc;
    std::unordered_map <uint64_t, uint32_t> hot_runs;
};

#define VU(m) ujit::mem_ptr(jit->state, offsetof(Vu, m))
#define VU_MEM(e, m) (e).mem(offsetof(Vu, m))

#define LANE(x, y, z, w) { (x) ? 0xffffffffu : 0u, (y) ? 0xffffffffu : 0u, \
                           (z) ? 0xffffffffu : 0u, (w) ? 0xffffffffu : 0u }

alignas(16) const uint32_t g_vu_const[K_COUNT][4] = {
    // Masks/offsets
    { 0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000 },
    { 0x80000000, 0x80000000, 0x80000000, 0x80000000 },
    { 0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff },
    { 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff },
    { 8, 4, 2, 1 },
    { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff },
    { 0, 0, 0, 0 },
    { 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff },
    { 0x4f000000, 0x4f000000, 0x4f000000, 0x4f000000 },

    // FTOI
    { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 },
    { 0x41800000, 0x41800000, 0x41800000, 0x41800000 },
    { 0x45800000, 0x45800000, 0x45800000, 0x45800000 },
    { 0x47000000, 0x47000000, 0x47000000, 0x47000000 },
    { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 },
    { 0x3d800000, 0x3d800000, 0x3d800000, 0x3d800000 },
    { 0x39800000, 0x39800000, 0x39800000, 0x39800000 },
    { 0x38000000, 0x38000000, 0x38000000, 0x38000000 },

    // Write masks
    LANE(0, 0, 0, 0), LANE(0, 0, 0, 1), LANE(0, 0, 1, 0), LANE(0, 0, 1, 1),
    LANE(0, 1, 0, 0), LANE(0, 1, 0, 1), LANE(0, 1, 1, 0), LANE(0, 1, 1, 1),
    LANE(1, 0, 0, 0), LANE(1, 0, 0, 1), LANE(1, 0, 1, 0), LANE(1, 0, 1, 1),
    LANE(1, 1, 0, 0), LANE(1, 1, 0, 1), LANE(1, 1, 1, 0), LANE(1, 1, 1, 1),

    // CLIP
    { 1, 4, 16, 0 },
    { 2, 8, 32, 0 },

    // MAC underflow and overflow bits
    { 0x100, 0x100, 0x100, 0x100 },
    { 0x1000, 0x1000, 0x1000, 0x1000 }
};

#undef LANE

constexpr size_t CODE_BUDGET = 96 * 1024 * 1024;

static void store_imm32(ujit::UniCompiler& uc, const ujit::Mem& dst, uint32_t value) {
#if defined(ASMJIT_UJIT_AARCH64)
    ujit::Gp tmp = uc.new_gp32();

    uc.mov(tmp, Imm(value));
    uc.store_u32(dst, tmp);
#else
    ujit::Mem m = dst;

    m.set_size(4);

    uc.cc->mov(m, Imm(value));
#endif
}

static void store_imm8(ujit::UniCompiler& uc, const ujit::Mem& dst, uint8_t value) {
#if defined(ASMJIT_UJIT_AARCH64)
    ujit::Gp tmp = uc.new_gp32();

    uc.mov(tmp, Imm(value));
    uc.store_u8(dst, tmp);
#else
    ujit::Mem m = dst;

    m.set_size(1);

    uc.cc->mov(m, Imm(value));
#endif
}

static void worker_main(Jit* jit);

Jit* create() {
    Jit* jit = new Jit();

    jit->worker = std::thread(worker_main, jit);

    return jit;
}

uint64_t blocks_compiled(Jit* jit) {
    return jit->compiled;
}

uint64_t blocks_failed(Jit* jit) {
    return jit->failed;
}

void destroy(Jit* jit) {
    if (jit->worker.joinable()) {
        {
            std::lock_guard <std::mutex> lock(jit->mu);

            jit->stopping = true;
        }

        jit->cv.notify_all();
        jit->worker.join();
    }

    delete jit;
}

static uint64_t victim_key(uint32_t tpc, uint64_t src_hash) {
    return ((uint64_t)tpc << 48) ^ src_hash;
}

static void remember_victim_length(Jit* jit, uint32_t tpc, uint32_t src_len) {
    std::vector <uint32_t>& lengths = jit->victim_lengths_by_tpc[tpc];

    for (uint32_t length : lengths) {
        if (length == src_len) {
            return;
        }
    }

    lengths.push_back(src_len);
}

uint32_t victim_lengths(Vu* vu, uint32_t tpc, uint32_t* lengths, uint32_t capacity) {
    Jit* jit = vu->jit;

    if (!jit || jit->victims.empty()) {
        return 0;
    }

    auto it = jit->victim_lengths_by_tpc.find(tpc);

    if (it == jit->victim_lengths_by_tpc.end()) {
        return 0;
    }

    uint32_t count = 0;

    for (uint32_t length : it->second) {
        if (count == capacity) {
            break;
        }

        lengths[count] = length;

        count++;
    }

    return count;
}

void save_runs(Vu* vu, Block* block) {
    Jit* jit = vu->jit;

    if (!jit || block->func || !block->src_len || !block->runs) {
        return;
    }

    if (jit->hot_runs.size() >= HOT_RUNS_MAX) {
        jit->hot_runs.clear();
    }

    jit->hot_runs[victim_key(block->tpc, block->src_hash)] = block->runs;
}

void restore_runs(Vu* vu, Block* block) {
    Jit* jit = vu->jit;

    if (!jit || block->func || !block->src_len) {
        return;
    }

    auto it = jit->hot_runs.find(victim_key(block->tpc, block->src_hash));

    if (it == jit->hot_runs.end()) {
        return;
    }

    block->runs = it->second;

    jit->hot_runs.erase(it);
}

void stash_block(Vu* vu, Block* block) {
    Jit* jit = vu->jit;

    if (!jit) {
        return;
    }

    if (!block->func || block->region_blocks > 1 || !block->src_len || jit->victims.size() >= VICTIM_MAX) {
        release_block(jit, block);

        return;
    }

    uint64_t key = victim_key(block->tpc, block->src_hash);

    if (jit->victims.count(key)) {
        release_block(jit, block);

        return;
    }

    Victim v;

    v.tpc = block->tpc;
    v.src_len = block->src_len;
    v.src_hash = block->src_hash;
    v.func = block->func;
    v.code_size = block->code_size;

    v.entries = std::move(block->entries);

    jit->victims.emplace(key, std::move(v));

    remember_victim_length(jit, block->tpc, block->src_len);

    block->func = nullptr;
    block->code_size = 0;
    block->jit_failed = false;
    block->compile_pending = false;
}

bool adopt_block(Vu* vu, Block* block) {
    Jit* jit = vu->jit;

    if (!jit || block->func || !block->src_len) {
        return false;
    }

    auto it = jit->victims.find(victim_key(block->tpc, block->src_hash));

    if (it == jit->victims.end()) {
        return false;
    }

    Victim& v = it->second;

    if (v.tpc != block->tpc || v.src_len != block->src_len || v.src_hash != block->src_hash) {
        return false;
    }

    block->entries = std::move(v.entries);
    block->func = v.func;
    block->code_size = v.code_size;
    block->region_blocks = 1;
    block->region_epoch = vu->region_epoch;

    block->region_deps.clear();

    jit->victims.erase(it);

    return true;
}

void release_block(Jit* jit, Block* block) {
    block->compile_pending = false;

    if (!block->func) {
        return;
    }

    jit->rt.release((void*)block->func);

    jit->code_size = jit->code_size >= block->code_size ? jit->code_size - block->code_size : 0;

    block->func = nullptr;
    block->code_size = 0;
    block->jit_failed = false;
}

static void enqueue_block(Vu* vu, Block* block);

static void wait(Vu* vu);

void flush_blocks(Vu* vu) {
    Jit* jit = vu->jit;

    if (!jit) {
        return;
    }

    wait(vu);

    for (Block& block : vu->block_cache) {
        block.func = nullptr;
        block.code_size = 0;
        block.jit_failed = false;
    }

    jit->victims.clear();
    jit->victim_lengths_by_tpc.clear();
    jit->hot_runs.clear();
    jit->rt.reset(ResetPolicy::kHard);

    jit->code_size = 0;
    jit->wants_flush = false;
}

void flush_if_needed(Vu* vu) {
    if (vu->jit && vu->jit->wants_flush) {
        profile::count(profile::VU_JIT_BUDGET_FLUSHES);

        flush_blocks(vu);
    }
}

static void emit_entry(Jit* jit, Emitter& e, const BlockEntry* entry, uint32_t next_tpc, uint32_t mask, bool may_delay, int* vi_shadow) {
    bool vi_shadow_live = *vi_shadow > 0;
    ujit::UniCompiler& uc = *e.uc;

    ujit::Gp arg;
    bool arg_ready = false;

    bool lower_called = false;

    auto entry_arg = [&]() {
        if (!arg_ready) {
            arg = uc.new_gp_ptr();

            uc.mov(arg, Imm((uintptr_t)entry));

            arg_ready = true;
        }

        return arg;
    };

    if (entry->is_mtir) {
        e.drop_flags();
        e.drop_scalars();

        jit_function_call(&uc, &vu::jit_entry_stall, e.state, entry_arg());
    }

    emit_prologue(e);

    auto upper = [&](bool after_lower) {
        if (upper_key(entry->upper.opcode) == UPPER_NOP && !emit_off(EMIT_NOP)) {
            return;
        }

        Fmac f;

        if (!emit_off(EMIT_FMAC) && classify_fmac(entry->upper.opcode, &f)) {
            emit_fmac(e, entry->upper, f, after_lower);

            return;
        }

        if (emit_upper_extra(e, entry->upper, after_lower)) {
            return;
        }

        e.flush_all();

        ujit::Gp ins = uc.new_gp_ptr();

        uc.mov(ins, Imm((uintptr_t)&entry->upper));

        InvokeNode* call = jit_invoke(uc, (uintptr_t)entry->upper.func,
                                      FuncSignature::build<void, void*, const void*>());

        call->set_arg(0, e.state);
        call->set_arg(1, ins);
    };

    auto lower = [&]() {
        if (emit_lower(e, entry->lower, next_tpc, mask, may_delay, vi_shadow_live)) {
            return;
        }

        lower_called = true;

        if (lower_reads_tpc(lower_key(entry->lower.opcode))) {
            store_imm32(uc, VU_MEM(e, tpc), next_tpc);
        }

        e.flush_for(entry->lower);

        ujit::Gp ins = uc.new_gp_ptr();

        uc.mov(ins, Imm((uintptr_t)&entry->lower));

        InvokeNode* call = jit_invoke(uc, (uintptr_t)entry->lower.func,
                                      FuncSignature::build<void, void*, const void*>());

        call->set_arg(0, e.state);
        call->set_arg(1, ins);
    };

    if (entry->i_bit) {
        upper(false);

        store_imm32(uc, VU_MEM(e, i), entry->lower.opcode);
    } else {
        if (entry->hazard3) {
            e.load_scalars();

            uc.mov(e.q_delay_reg, Imm(0));
        }

        if (!entry->upper.dst.reg) {
            upper(false);

            if (!entry->lower_is_nop) {
                lower();
            }
        } else if (entry->hazard0 || entry->hazard1 || entry->is_waitq) {
            lower();
            upper(true);
        } else if (entry->hazard2) {
            upper(false);

            ujit::Vec kept = e.get(entry->upper.dst.reg);
            uint8_t kept_clean = e.clean[entry->upper.dst.reg];

            lower();

            e.set(entry->upper.dst.reg, kept);

            e.clean[entry->upper.dst.reg] = kept_clean;
        } else {
            upper(false);

            if (!entry->lower_is_nop) {
                lower();
            }
        }
    }

    emit_epilogue(e, entry, vi_shadow_live || lower_called || e.shadow_left > 0);

    *vi_shadow = lower_called ? 2 : (*vi_shadow > 0 ? *vi_shadow - 1 : 0);

    if (lower_called) {
        e.shadow_left = 2;
        e.shadow_reg = -1;
    }

    if (e.shadow_left > 0) {
        e.shadow_left--;

        if (!e.shadow_left) {
            e.shadow_reg = -1;
        }
    }
}

static void emit_retire(Jit* jit, Emitter& e, ujit::UniCompiler& uc, const Label& block_exit, bool need_branch, bool need_end, uint32_t next_tpc) {
    ujit::Gp taken;

    if (need_branch) {
        taken = uc.new_gp32();

        uc.mov(taken, Imm(0));

        Label done = uc.new_label();

        ujit::Gp delay = uc.new_gp32();

        uc.load_i32(delay, VU(branch_delay));
        uc.j(done, ujit::test_z(delay));

        uc.sub(delay, delay, Imm(1));
        uc.store_u32(VU(branch_delay), delay);
        uc.j(done, ujit::test_nz(delay));

        ujit::Gp target = uc.new_gp32();

        uc.load_u32(target, VU(branch_pc));
        uc.store_u32(VU(tpc), target);

        Label no_chain = uc.new_label();

        ujit::Gp chained = uc.new_gp32();

        uc.load_u8(chained, VU(delay_branch));
        uc.j(no_chain, ujit::test_z(chained));

        store_imm32(uc, VU(branch_delay), 1);

        ujit::Gp chain_pc = uc.new_gp32();

        uc.load_u32(chain_pc, VU(delay_branch_pc));
        uc.store_u32(VU(branch_pc), chain_pc);
        store_imm8(uc, VU(delay_branch), 0);

        uc.bind(no_chain);

        uc.mov(taken, Imm(1));

        uc.bind(done);
    }

    if (need_end) {
        Label done = uc.new_label();

        ujit::Gp left = uc.new_gp32();

        uc.load_i32(left, VU(e_bit));
        uc.j(done, ujit::test_z(left));

        uc.sub(left, left, Imm(1));
        uc.store_u32(VU(e_bit), left);
        uc.j(done, ujit::test_nz(left));

        store_imm32(uc, VU(jit_exit), VU_JIT_STOP);

        if (need_branch) {
            Label keep = uc.new_label();

            uc.j(keep, ujit::test_nz(taken));
            store_imm32(uc, VU(tpc), next_tpc);
            uc.bind(keep);
        } else {
            store_imm32(uc, VU(tpc), next_tpc);
        }

        e.store_dirty();

        uc.j(block_exit);

        uc.bind(done);
    }

    if (need_branch) {
        Label fall = uc.new_label();

        uc.j(fall, ujit::test_z(taken));

        e.store_dirty();

        uc.j(block_exit);

        uc.bind(fall);
    }
}

constexpr int REGION_MAX_BLOCKS = 16;

static int region_successors(const Vu* vu, const Block* b, uint32_t* out) {
    for (const BlockEntry& entry : b->entries) {
        if (entry.m_bit) {
            return 0;
        }
    }

    const size_t count = b->entries.size();

    if (count < 2) {
        return 0;
    }

    const uint32_t mask = (uint32_t)vu->micro_mem_size;
    const uint32_t fall = (b->tpc + (uint32_t)count) & mask;

    const BlockEntry& br = b->entries[count - 2];

    if (br.e_bit) {
        return 0;
    }

    if (!br.branch) {
        out[0] = fall;

        return 1;
    }

    const uint32_t target = ((b->tpc + (uint32_t)count - 1)
                          + (uint32_t)br.lower.ld_imm11) & mask;

    switch (lower_key(br.lower.opcode)) {
        case 0x20:
        case 0x21: {
            out[0] = target;

            return 1;
        }

        case 0x28:
        case 0x29:
        case 0x2c:
        case 0x2d:
        case 0x2e:
        case 0x2f: {
            out[0] = target;
            out[1] = fall;

            return 2;
        }
    }

    return 0;
}

static bool compile(Jit* jit, Vu* vu, Block** members, const uint32_t* member_tpc,
                    int member_count, void (**out_func)(Vu*), uint32_t* out_size);

static void publish_block(Vu* vu, Block* block, Block** members, const uint32_t* member_tpc,
                    int member_count, void (*compiled)(Vu*), uint32_t code_size);

void compile_block(Vu* vu, Block* block) {
    Jit* jit = vu->jit;

    profile::count(profile::VU_COMPILE_REQUESTS);

    if (adopt_block(vu, block)) {
        profile::count(profile::VU_COMPILES_ADOPTED);

        return;
    }

    release_block(jit, block);

    Block* members[REGION_MAX_BLOCKS];
    uint32_t member_tpc[REGION_MAX_BLOCKS];

    int member_count = 0;

    members[member_count] = block;
    member_tpc[member_count++] = block->tpc;

    int region_max = vu->region_limit > 0 && vu->region_limit < REGION_MAX_BLOCKS ? vu->region_limit : REGION_MAX_BLOCKS;

    if (block->region_churn >= 2) {
        region_max = 1;
    }

    for (int scan = 0; scan < member_count; scan++) {
        uint32_t succ[2];

        int n = region_successors(vu, members[scan], succ);

        for (int k = 0; k < n && member_count < region_max; k++) {
            bool seen = false;

            for (int j = 0; j < member_count; j++) {
                if (member_tpc[j] == succ[k]) {
                    seen = true;
                }
            }

            // Polling loops must return to run() so the EE can supply data.
            if (seen || get_poll_register(vu, succ[k])) {
                continue;
            }

            Block* b = find_block(vu, succ[k]);

            if (!b) {
                b = cache_block(vu, succ[k], 64);
            }

            if (!b || !b->cycles) {
                continue;
            }

            members[member_count] = b;
            member_tpc[member_count++] = succ[k];
        }
    }

    if (member_count == 1) {
        enqueue_block(vu, block);

        return;
    }

    void (*fn)(Vu*) = nullptr;

    uint32_t size = 0;

    if (!compile(jit, vu, members, member_tpc, member_count, &fn, &size)) {
        block->jit_failed = true;
        jit->failed++;

        return;
    }

    publish_block(vu, block, members, member_tpc, member_count, fn, size);
}

static bool compile(Jit* jit, Vu* vu, Block** members, const uint32_t* member_tpc,
                    int member_count, void (**out_func)(Vu*), uint32_t* out_size) {
    Block* block = members[0];

    CodeHolder code;

    code.init(jit->rt.environment(), jit->rt.cpu_features());

    // static FileLogger dump_logger(stdout);

    // bool dumping = dumps_left > 0;

    // if (dumping) {
    //     dumps_left--;

    //     printf("---- vu%d block %04x, %d entries, %d member(s) ----\n",
    //            vu->id, block->tpc, block->cycles, member_count);

    //     code.set_logger(&dump_logger);
    // }

    ujit::BackendCompiler bc(&code);
    ujit::UniCompiler uc(&bc, jit->rt.cpu_features(), jit->rt.cpu_hints());

    FuncNode* func = uc.add_func(FuncSignature::build<void, Vu*>());

    jit->state = uc.new_gp_ptr();

    func->set_arg(0, jit->state);

    Label trampoline = uc.new_label();
    Label region_exit = uc.new_label();

    Label labels[REGION_MAX_BLOCKS];

    for (int m = 0; m < member_count; m++) {
        labels[m] = uc.new_label();
    }

    const uint32_t mask = (uint32_t)vu->micro_mem_size;

    store_imm32(uc, VU(jit_exit), VU_JIT_CONTINUE);

    for (int m = 0; m < member_count; m++) {
        Block* b = members[m];

        uc.bind(labels[m]);

        Emitter e = {};

        e.vu = vu;
        e.uc = &uc;
        e.state = jit->state;
        e.consts = uc.new_gp_ptr();

        uc.mov(e.consts, Imm((uintptr_t)g_vu_const));

        const size_t count = b->entries.size();

        bool prev_branch = false;
        bool prev_end = false;

        int vi_shadow = 1;

        bool interlocked = false;

        for (size_t i = 0; i < count; i++) {
            const BlockEntry& entry = b->entries[i];

            if (entry.m_bit) {
                store_imm8(uc, VU(waiting_for_interlock), 1);
                store_imm32(uc, VU(jit_exit), VU_JIT_STOP);
                store_imm32(uc, VU(tpc), (b->tpc + (uint32_t)i) & mask);

                e.store_dirty();

                uc.j(trampoline);

                interlocked = true;

                break;
            }

            if (entry.e_bit) {
                store_imm32(uc, VU(e_bit), 2);
            }

            uint32_t next_tpc = (b->tpc + (uint32_t)i + 1) & mask;

            emit_entry(jit, e, &entry, next_tpc, mask, i == 0 || prev_branch, &vi_shadow);

            bool need_branch = i == 0 || entry.branch || prev_branch;
            bool need_end = i == 0 || entry.e_bit || prev_end;

            emit_retire(jit, e, uc, trampoline, need_branch, need_end, next_tpc);

            prev_branch = entry.branch != 0;
            prev_end = entry.e_bit != 0;
        }

        if (!interlocked) {
            store_imm32(uc, VU(tpc), (b->tpc + (uint32_t)count) & mask);

            e.store_dirty();

            uc.j(trampoline);
        }
    }

    uc.bind(trampoline);

    if (member_count > 1) {
        ujit::Gp jexit = uc.new_gp32();

        uc.load_i32(jexit, VU(jit_exit));
        uc.j(region_exit, ujit::test_nz(jexit));

        ujit::Gp cyc = uc.new_gp64();

        uc.load_u64(cyc, VU(vu_cycle));
        uc.j(region_exit, ujit::ucmp_ge(cyc, VU(run_deadline)));

        ujit::Gp t = uc.new_gp32();

        uc.load_u32(t, VU(tpc));

        for (int m = 0; m < member_count; m++) {
            uc.j(labels[m], ujit::cmp_eq(t, Imm(member_tpc[m])));
        }
    }

    uc.bind(region_exit);

    uc.ret();
    uc.end_func();

    Error err = uc.finalize();

    if (err != Error::kOk) {
        iris_error(vu, "Failed to finalize block at {:04x} ({})", block->tpc, DebugUtils::error_as_string(err));

        return false;
    }

    void (*compiled)(Vu*) = nullptr;

    Error added = jit->rt.add(&compiled, &code);

    if (added != Error::kOk) {
        iris_error(vu, "Failed to add block at {:04x} to the runtime ({})", block->tpc, DebugUtils::error_as_string(added));

        return false;
    }

    *out_func = compiled;
    *out_size = (uint32_t)code.code_size();

    return true;
}

static void publish_block(Vu* vu, Block* block, Block** members, const uint32_t* member_tpc,
                    int member_count, void (*compiled)(Vu*), uint32_t code_size) {
    Jit* jit = vu->jit;

    block->code_size = code_size;
    block->region_blocks = (uint16_t)member_count;
    block->region_epoch = vu->region_epoch;

    block->region_deps.clear();

    for (int m = 1; m < member_count; m++) {
        block->region_deps.push_back({
            member_tpc[m],
            members[m]->src_len,
            members[m]->src_hash,
            (const void*)members[m]->entries.data()
        });
    }

    jit->code_size += block->code_size;
    jit->compiled++;

    if (jit->code_size >= CODE_BUDGET) {
        jit->wants_flush = true;
    }

    block->func = compiled;
}

static void worker_main(Jit* jit) {
    for (;;) {
        AsyncJob job;

        {
            std::unique_lock <std::mutex> lock(jit->mu);

            jit->cv.wait(lock, [jit] { return jit->stopping || !jit->queue.empty(); });

            if (jit->queue.empty()) {
                return;
            }

            job = std::move(jit->queue.front());

            jit->queue.pop_front();
        }

        Block local;

        local.tpc = job.tpc;
        local.cycles = job.cycles;
        local.entries = std::move(job.entries);

        Block* members[1] = { &local };
        uint32_t member_tpc[1] = { job.tpc };

        AsyncResult r;

        r.vu = job.vu;
        r.tpc = job.tpc;
        r.src_len = job.src_len;
        r.src_hash = job.src_hash;

        r.failed = !compile(jit, job.vu, members, member_tpc, 1, &r.func, &r.code_size);

        r.entries = std::move(local.entries);

        {
            std::lock_guard <std::mutex> lock(jit->mu);

            jit->done.push_back(std::move(r));

            jit->in_flight--;
        }

        jit->idle_cv.notify_all();
    }
}

static void enqueue_block(Vu* vu, Block* block) {
    Jit* jit = vu->jit;

    AsyncJob job;

    job.vu = vu;
    job.tpc = block->tpc;
    job.cycles = block->cycles;
    job.src_len = block->src_len;
    job.src_hash = block->src_hash;
    job.entries = block->entries;

    block->compile_pending = true;

    {
        std::lock_guard <std::mutex> lock(jit->mu);

        jit->queue.push_back(std::move(job));

        jit->in_flight++;
    }

    jit->cv.notify_one();
}

void drain_blocks(Vu* vu) {
    Jit* jit = vu->jit;

    if (!jit) {
        return;
    }

    std::vector <AsyncResult> done;

    {
        std::lock_guard <std::mutex> lock(jit->mu);

        if (jit->done.empty()) {
            return;
        }

        done.swap(jit->done);
    }

    for (AsyncResult& r : done) {
        Block& block = vu->block_cache[r.tpc & vu->micro_mem_size];

        if (block.tpc == r.tpc) {
            block.compile_pending = false;
        }

        bool usable = !r.failed
                   && r.func
                   && block.tpc == r.tpc
                   && block.cycles
                   && block.src_hash == r.src_hash
                   && !block.func;

        if (!usable) {
            uint64_t key = victim_key(r.tpc, r.src_hash);

            if (!r.failed && r.func && r.src_len && !jit->victims.count(key)
                && jit->victims.size() < VICTIM_MAX) {
                Victim v;

                v.tpc = r.tpc;
                v.src_len = r.src_len;
                v.src_hash = r.src_hash;
                v.func = r.func;
                v.code_size = r.code_size;
                v.entries = std::move(r.entries);

                jit->victims.emplace(key, std::move(v));

                remember_victim_length(jit, r.tpc, r.src_len);

                jit->code_size += r.code_size;
                jit->compiled++;

                continue;
            }

            if (r.func) {
                jit->rt.release((void*)r.func);
            }

            continue;
        }

        block.entries = std::move(r.entries);

        block.func = r.func;
        block.code_size = r.code_size;
        block.region_blocks = 1;
        block.region_epoch = vu->region_epoch;

        block.region_deps.clear();

        jit->code_size += r.code_size;
        jit->compiled++;

        if (jit->code_size >= CODE_BUDGET) {
            jit->wants_flush = true;
        }
    }
}

static void wait(Vu* vu) {
    Jit* jit = vu->jit;

    {
        std::unique_lock <std::mutex> lock(jit->mu);

        jit->idle_cv.wait(lock, [jit] { return jit->in_flight == 0; });
    }

    drain_blocks(vu);
}

}
