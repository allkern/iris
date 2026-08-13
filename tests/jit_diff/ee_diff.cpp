#include <cstdlib>
#include <cstring>
#include <set>

#include <fmt/format.h>

#include "common.hpp"

#include "ee/ee.hpp"
#include "ee/ee_def.hpp"
#include "ee/ee_dis.hpp"
#include "ee/bus.hpp"

namespace iris::ee { Instruction decode(uint32_t opcode); }

namespace jitdiff {

using namespace iris;

namespace {

constexpr uint32_t RAM_SIZE = 0x200000;
constexpr uint32_t CODE_PHYS = 0x00100000;
constexpr uint32_t CODE_PC = 0x80000000 | CODE_PHYS;   // KSEG0, so no TLB walk
constexpr uint32_t SCRATCH_PHYS = 0x00080000;
constexpr uint32_t SCRATCH_SIZE = 0x200;
constexpr uint32_t SPR_SIZE = 0x4000;

uint8_t* g_ram = nullptr;

uint64_t ram_read(uint32_t addr, int bytes) {
    uint64_t v = 0;

    for (int i = 0; i < bytes; i++)
        v |= (uint64_t)g_ram[(addr + i) & (RAM_SIZE - 1)] << (i * 8);

    return v;
}

void ram_write(uint32_t addr, uint64_t data, int bytes) {
    for (int i = 0; i < bytes; i++)
        g_ram[(addr + i) & (RAM_SIZE - 1)] = (uint8_t)(data >> (i * 8));
}

uint64_t bus_read8(void*, uint32_t addr) { return ram_read(addr, 1); }
uint64_t bus_read16(void*, uint32_t addr) { return ram_read(addr, 2); }
uint64_t bus_read32(void*, uint32_t addr) { return ram_read(addr, 4); }
uint64_t bus_read64(void*, uint32_t addr) { return ram_read(addr, 8); }

uint128_t bus_read128(void*, uint32_t addr) {
    uint128_t v;

    v.u64[0] = ram_read(addr, 8);
    v.u64[1] = ram_read(addr + 8, 8);

    return v;
}

void bus_write8(void*, uint32_t addr, uint64_t data) { ram_write(addr, data, 1); }
void bus_write16(void*, uint32_t addr, uint64_t data) { ram_write(addr, data, 2); }
void bus_write32(void*, uint32_t addr, uint64_t data) { ram_write(addr, data, 4); }
void bus_write64(void*, uint32_t addr, uint64_t data) { ram_write(addr, data, 8); }

void bus_write128(void*, uint32_t addr, uint128_t data) {
    ram_write(addr, data.u64[0], 8);
    ram_write(addr + 8, data.u64[1], 8);
}

// What goes into the core before a case runs.
struct Seed {
    uint128_t r[32];
    uint128_t hi, lo;
    uint64_t sa;
    uint32_t f[32];
    uint32_t acc, fcr;
    uint32_t cop0_r[32];
    uint8_t mem[SCRATCH_SIZE];
    uint8_t spr[SPR_SIZE];
};

// What comes out, and the only thing compared. Scratchpad and scratch memory
// are hashed rather than kept verbatim so a recording stays small.
struct Result {
    uint128_t r[32];
    uint128_t hi, lo;
    uint64_t sa;
    uint32_t f[32];
    uint32_t acc, fcr, pc;
    uint32_t cop0_r[32];
    uint64_t mem_hash, spr_hash;
};

void capture(ee::Ee* ee, Result* s) {
    memcpy(s->r, ee->r, sizeof(s->r));
    memcpy(s->cop0_r, ee->cop0_r, sizeof(s->cop0_r));

    for (int i = 0; i < 32; i++)
        s->f[i] = ee->f[i].u32;

    s->hi = ee->hi;
    s->lo = ee->lo;
    s->sa = ee->sa;
    s->acc = ee->a.u32;
    s->fcr = ee->fcr;
    s->pc = ee->pc;

    s->mem_hash = hash64(g_ram + SCRATCH_PHYS, SCRATCH_SIZE);
    s->spr_hash = hash64(ee::get_spr(ee)->buf, SPR_SIZE);
}

void apply(ee::Ee* ee, const Seed* s) {
    memcpy(ee->r, s->r, sizeof(s->r));
    memcpy(ee->cop0_r, s->cop0_r, sizeof(s->cop0_r));

    for (int i = 0; i < 32; i++)
        ee->f[i].u32 = s->f[i];

    ee->hi = s->hi;
    ee->lo = s->lo;
    ee->sa = s->sa;
    ee->a.u32 = s->acc;
    ee->fcr = s->fcr;

    ee->pc = CODE_PC;
    ee->next_pc = CODE_PC + 4;
    ee->branch = 0;
    ee->branch_taken = 0;
    ee->delay_slot = 0;

    memcpy(ee::get_spr(ee)->buf, s->spr, SPR_SIZE);
    memcpy(g_ram + SCRATCH_PHYS, s->mem, SCRATCH_SIZE);
}

void serialize(Blob* b, uint32_t opcode, int steps, const Result& r) {
    b->put_u32(opcode);
    b->put_u32((uint32_t)steps);

    for (int i = 0; i < 32; i++) {
        b->put_u64(r.r[i].u64[0]);
        b->put_u64(r.r[i].u64[1]);
    }

    b->put_u64(r.hi.u64[0]);
    b->put_u64(r.hi.u64[1]);
    b->put_u64(r.lo.u64[0]);
    b->put_u64(r.lo.u64[1]);
    b->put_u64(r.sa);

    for (int i = 0; i < 32; i++) b->put_u32(r.f[i]);

    b->put_u32(r.acc);
    b->put_u32(r.fcr);
    b->put_u32(r.pc);

    for (int i = 0; i < 32; i++) b->put_u32(r.cop0_r[i]);

    b->put_u64(r.mem_hash);
    b->put_u64(r.spr_hash);
}

void deserialize(Blob* b, uint32_t* opcode, int* steps, Result* r) {
    *opcode = b->get_u32();
    *steps = (int)b->get_u32();

    for (int i = 0; i < 32; i++) {
        r->r[i].u64[0] = b->get_u64();
        r->r[i].u64[1] = b->get_u64();
    }

    r->hi.u64[0] = b->get_u64();
    r->hi.u64[1] = b->get_u64();
    r->lo.u64[0] = b->get_u64();
    r->lo.u64[1] = b->get_u64();
    r->sa = b->get_u64();

    for (int i = 0; i < 32; i++) r->f[i] = b->get_u32();

    r->acc = b->get_u32();
    r->fcr = b->get_u32();
    r->pc = b->get_u32();

    for (int i = 0; i < 32; i++) r->cop0_r[i] = b->get_u32();

    r->mem_hash = b->get_u64();
    r->spr_hash = b->get_u64();
}

// Instructions whose whole purpose is to leave the block: an exception taken
// part way through a block is a separate (architecture independent) question
// from whether the block computed the right values, and testing it here would
// only produce noise.
bool excluded(int id) {
    switch (id) {
        case ee::I_INVALID:
        case ee::I_SYSCALL:
        case ee::I_BREAK:
        case ee::I_ERET:
        case ee::I_TGE:  case ee::I_TGEI:  case ee::I_TGEIU: case ee::I_TGEU:
        case ee::I_TLT:  case ee::I_TLTI:  case ee::I_TLTIU: case ee::I_TLTU:
        case ee::I_TNE:  case ee::I_TNEI:
        case ee::I_TEQ:  case ee::I_TEQI:
        case ee::I_TLBR: case ee::I_TLBWI: case ee::I_TLBWR: case ee::I_TLBP:
            return true;

        default:
            return false;
    }
}

// rs holds the base address.
bool is_mem(int id) {
    switch (id) {
        case ee::I_LB:  case ee::I_LBU: case ee::I_LH:  case ee::I_LHU:
        case ee::I_LW:  case ee::I_LWU: case ee::I_LD:  case ee::I_LQ:
        case ee::I_LWL: case ee::I_LWR: case ee::I_LDL: case ee::I_LDR:
        case ee::I_SB:  case ee::I_SH:  case ee::I_SW:  case ee::I_SD:
        case ee::I_SQ:  case ee::I_SWL: case ee::I_SWR: case ee::I_SDL:
        case ee::I_SDR: case ee::I_LWC1: case ee::I_SWC1:
        case ee::I_LQC2: case ee::I_SQC2:
        case ee::I_PREF: case ee::I_CACHE:
            return true;

        default:
            return false;
    }
}

// Signed arithmetic that traps on overflow. Seeded narrow so the trap never
// fires; see the note on excluded().
bool is_trapping_arith(int id) {
    switch (id) {
        case ee::I_ADD:  case ee::I_ADDI:  case ee::I_SUB:
        case ee::I_DADD: case ee::I_DADDI: case ee::I_DSUB:
            return true;

        default:
            return false;
    }
}

struct Template {
    uint32_t opcode;
    int id;
};

const bool g_trace = getenv("JITDIFF_TRACE") != nullptr;

// The decoder is the only authority on which encodings mean what, so ask it:
// walk the selector fields, keep one representative encoding per instruction.
void discover(std::vector <Template>* out) {
    std::set <int> seen;

    auto probe = [&](uint32_t opcode) {
        // COP2 with the CO bit set is a VU0 macro-mode instruction. The
        // recompiler hands every one of these to the same interpreter routine
        // the interpreter uses, so there is no codegen to compare - and
        // VCALLMS would run VU0 microcode that never reaches an E bit and hang
        // the test. CFC2/CTC2/QMFC2/QMTC2 have the CO bit clear and stay in.
        if ((opcode >> 26) == 0x12 && (opcode & 0x02000000)) return;

        ee::Instruction i = ee::decode(opcode);

        if (excluded(i.id)) return;
        if (seen.count(i.id)) return;

        seen.insert(i.id);
        out->push_back({ opcode, i.id });
    };

    for (uint32_t op = 0; op < 64; op++) {
        for (uint32_t funct = 0; funct < 64; funct++) {
            for (uint32_t sa = 0; sa < 32; sa++) {
                // sa doubles as the sub-opcode selector for the MMI groups
                probe((op << 26) | (sa << 6) | funct);

                for (uint32_t sel = 0; sel < 32; sel++) {
                    // rs selects the COP operation, rt the branch condition
                    probe((op << 26) | (sel << 21) | (sa << 6) | funct);
                    probe((op << 26) | (sel << 16) | (sa << 6) | funct);
                }
            }
        }
    }
}

// Fill in operand fields, then confirm the instruction still decodes to the
// same thing - the same bits are operands in one group and sub-opcode selectors
// in another, so the decoder has to arbitrate.
//
// A candidate is picked at random among those that survive, not first-match.
// The shift-amount field matters here: the template always carries sa=0
// (discover() finds it first), and first-match would pin every MMI shift to a
// shift of zero forever. Zero is worth testing - it is the one AArch64 cannot
// encode for a right shift - but so is everything else.
uint32_t randomise(const Template& t, Rng& rng, uint32_t* out_rs) {
    uint32_t rs = 1 + rng.u32() % 31;
    uint32_t rt = 1 + rng.u32() % 31;
    uint32_t rd = 1 + rng.u32() % 31;
    uint32_t imm = rng.u32() & 0xffff;
    uint32_t sa = rng.u32() & 0x1f;

    *out_rs = (t.opcode >> 21) & 0x1f;

    uint32_t candidates[5];
    int n = 0;

    candidates[n++] = t.opcode | (rs << 21) | (rt << 16) | (rd << 11) | (sa << 6);
    candidates[n++] = t.opcode | (rs << 21) | (rt << 16) | (rd << 11);
    candidates[n++] = t.opcode | (rs << 21) | (rt << 16) | (imm & 0xffff);
    candidates[n++] = t.opcode | (rt << 16) | (rd << 11);
    candidates[n++] = t.opcode;

    uint32_t valid[5];
    int v = 0;

    for (int k = 0; k < n; k++) {
        if (ee::decode(candidates[k]).id != t.id)
            continue;

        // A memory op has to keep a real base register. With r0 as the base the
        // address is just the offset, which lands in KUSEG with no TLB entry -
        // the access then faults, and a fault taken part way through a block is
        // out of scope here (see excluded()).
        if (is_mem(t.id) && ((candidates[k] >> 21) & 0x1f) == 0)
            continue;

        valid[v++] = candidates[k];
    }

    if (!v)
        return 0;

    uint32_t chosen = valid[rng.u32() % (uint32_t)v];

    *out_rs = (chosen >> 21) & 0x1f;

    return chosen;
}

const char* g_cop0_name[32] = {
    "index", "random", "entrylo0", "entrylo1", "context", "pagemask", "wired", "r7",
    "badvaddr", "count", "entryhi", "compare", "status", "cause", "epc", "prid",
    "config", "r17", "r18", "r19", "r20", "r21", "r22", "badpaddr",
    "debug", "perf", "r26", "r27", "taglo", "taghi", "errorepc", "r31"
};

void diff(Report* rep, const Options& opt, const char* name, uint32_t opcode,
          const Result& a, const Result& b, int steps) {
    char buf[192];
    ee::dis::Dis ds;

    ds.print_address = 0;
    ds.print_opcode = 0;
    ds.pseudo_instructions = 0;
    ds.pc = CODE_PC;

    std::string text = ee::dis::disassemble(buf, opcode, &ds);

    auto note = [&](const std::string& what, uint64_t ia, uint64_t ib) {
        rep->fail(
            fmt::format("ee {} {}", name, what),
            fmt::format("op={:08x} [{}] steps={} {}={:016x} {}={:016x}",
                        opcode, text, steps, opt.label, ia, opt.other_label, ib));
    };

    for (int i = 0; i < 32; i++) {
        if (a.r[i].u64[0] != b.r[i].u64[0]) note("gpr.lo", a.r[i].u64[0], b.r[i].u64[0]);
        if (a.r[i].u64[1] != b.r[i].u64[1]) note("gpr.hi", a.r[i].u64[1], b.r[i].u64[1]);
    }

    if (a.hi.u64[0] != b.hi.u64[0]) note("hi.lo", a.hi.u64[0], b.hi.u64[0]);
    if (a.hi.u64[1] != b.hi.u64[1]) note("hi.hi", a.hi.u64[1], b.hi.u64[1]);
    if (a.lo.u64[0] != b.lo.u64[0]) note("lo.lo", a.lo.u64[0], b.lo.u64[0]);
    if (a.lo.u64[1] != b.lo.u64[1]) note("lo.hi", a.lo.u64[1], b.lo.u64[1]);

    if (a.sa != b.sa) note("sa", a.sa, b.sa);
    if (a.acc != b.acc) note("fpu.acc", a.acc, b.acc);
    if (a.fcr != b.fcr) note("fcr", a.fcr, b.fcr);
    if (a.pc != b.pc) note("pc", a.pc, b.pc);

    for (int i = 0; i < 32; i++)
        if (a.f[i] != b.f[i])
            note("fpr", a.f[i], b.f[i]);

    for (int i = 0; i < 32; i++) {
        // COUNT is driven by the scheduler, not by the instruction stream
        if (i == 9) continue;

        if (a.cop0_r[i] != b.cop0_r[i])
            note(fmt::format("cop0.{}", g_cop0_name[i]), a.cop0_r[i], b.cop0_r[i]);
    }

    if (a.mem_hash != b.mem_hash) note("mem", a.mem_hash, b.mem_hash);
    if (a.spr_hash != b.spr_hash) note("spr", a.spr_hash, b.spr_hash);
}

}

int run_ee_tests(Logger* logger, const Options& opt, Report* out) {
    g_ram = (uint8_t*)calloc(RAM_SIZE, 1);

    // vfast_page_base() reinterprets bus udata as an ee::bus::Bus to reach the
    // fastmem tables. A zeroed one keeps every lookup a miss, so both engines
    // take the same slow path through the callbacks below.
    ee::bus::Bus* fake_bus = (ee::bus::Bus*)calloc(1, sizeof(ee::bus::Bus));

    ee::Ee* ee = ee::create(logger, RAM_SIZE);

    vu::Vu* vu0 = vu::create(logger, 0);
    vu::Vu* vu1 = vu::create(logger, 1);

    vu::connect(vu0, nullptr, nullptr, vu1);
    vu::connect(vu1, nullptr, nullptr, nullptr);

    ee::BusInterface iface = {};

    iface.udata = fake_bus;
    iface.read8 = bus_read8;
    iface.read16 = bus_read16;
    iface.read32 = bus_read32;
    iface.read64 = bus_read64;
    iface.read128 = bus_read128;
    iface.write8 = bus_write8;
    iface.write16 = bus_write16;
    iface.write32 = bus_write32;
    iface.write64 = bus_write64;
    iface.write128 = bus_write128;

    ee::connect(ee, vu0, vu1, iface);
    ee::reset(ee);

    out->core = "ee";
    out->seed = opt.seed;

    std::vector <Template> templates;

    discover(&templates);

    printf("  ee opcodes discovered: %d\n", (int)templates.size());

    Rng rng(opt.seed);

    Seed s0;
    Result jit_result, other_result;

    char name_buf[192];

    for (int it = 0; it < opt.iterations; it++) {
        const Template& t = templates[it % templates.size()];

        uint32_t rs = 0;
        uint32_t opcode = randomise(t, rng, &rs);

        if (!opcode)
            continue;

        // [test instruction][nop][j back to start][nop]
        //
        // The trailing jump bounds the sub-block so only the instruction under
        // test runs. The nop after it matters: if the instruction under test is
        // itself a branch, that nop is its delay slot, and a branch there is an
        // encoding the recompiler explicitly refuses.
        ram_write(CODE_PHYS, opcode, 4);
        ram_write(CODE_PHYS + 4, 0, 4);
        ram_write(CODE_PHYS + 8, (0x02u << 26) | ((CODE_PC >> 2) & 0x3fffffu), 4);
        ram_write(CODE_PHYS + 12, 0, 4);

        for (int i = 1; i < 32; i++) {
            s0.r[i].u64[0] = rng.interesting64();
            s0.r[i].u64[1] = rng.interesting64();
        }

        s0.hi.u64[0] = rng.interesting64();
        s0.hi.u64[1] = rng.interesting64();
        s0.lo.u64[0] = rng.interesting64();
        s0.lo.u64[1] = rng.interesting64();

        // SA only ever holds a byte offset; MTSAB/MTSAH mask it to 0..15.
        s0.sa = rng.u32() & 0xf;

        for (int i = 0; i < 32; i++)
            s0.f[i] = rng.interesting_f32();

        s0.acc = rng.interesting_f32();
        s0.fcr = 0x01000001;

        memset(s0.cop0_r, 0, sizeof(s0.cop0_r));

        // Kernel mode, no interrupt enabled, not already in an exception.
        s0.cop0_r[12] = 0x10000000;
        s0.cop0_r[15] = 0x00002e20;

        if (is_trapping_arith(t.id)) {
            for (int i = 1; i < 32; i++)
                s0.r[i].u64[0] = (uint64_t)(int64_t)(int32_t)(rng.u32() & 0x3fffffff);
        }

        if (is_mem(t.id)) {
            uint32_t base = (0x80000000u | SCRATCH_PHYS) + (rng.u32() % (SCRATCH_SIZE - 64));

            s0.r[rs].u64[0] = (uint64_t)(int64_t)(int32_t)base;
            s0.r[rs].u64[1] = 0;
        }

        // Last word on r0, after every other seeding step. r0 reads as zero,
        // and the recompiler is entitled to assume that.
        s0.r[0].u64[0] = 0;
        s0.r[0].u64[1] = 0;

        for (uint32_t i = 0; i < SCRATCH_SIZE; i++)
            s0.mem[i] = (uint8_t)(rng.u32() & 0xff);

        for (uint32_t i = 0; i < SPR_SIZE; i++)
            s0.spr[i] = (uint8_t)(rng.u32() & 0xff);

        ee::dis::Dis nds;

        nds.print_address = 0;
        nds.print_opcode = 0;
        nds.pseudo_instructions = 0;
        nds.pc = CODE_PC;

        ee::dis::disassemble(name_buf, opcode, &nds);

        char* space = strchr(name_buf, ' ');

        if (space) *space = 0;

        if (g_trace) {
            printf("    case %d op=%08x id=%d %s\n", it, opcode, t.id, name_buf);
            fflush(stdout);
        }

        apply(ee, &s0);

        // Targeted: flush_cache() walks a million cache pages and clears the
        // 8 MB fastmem table, which would dominate the run.
        ee::invalidate_block(ee, CODE_PHYS);

        int steps = ee::run_block(ee, 1);

        capture(ee, &jit_result);

        ++out->cases;

        if (opt.mode == Mode::Record) {
            serialize(opt.blob, opcode, steps, jit_result);

            continue;
        }

        int other_steps = steps;

        if (opt.mode == Mode::Compare) {
            uint32_t recorded_opcode = 0;

            deserialize(opt.blob, &recorded_opcode, &other_steps, &other_result);

            if (!opt.blob->ok) {
                out->fail("ee recording truncated", "ran out of recorded cases");

                break;
            }

            if (recorded_opcode != opcode) {
                out->fail("ee recording mismatch",
                          fmt::format("case {} is {:08x} here, {:08x} in the recording",
                                      it, opcode, recorded_opcode));

                break;
            }
        } else {
            apply(ee, &s0);

            for (int k = 0; k < steps; k++)
                ee::step(ee);

            capture(ee, &other_result);
        }

        if (other_steps != steps)
            out->fail("ee step count",
                      fmt::format("op={:08x} [{}] {}={} {}={}", opcode, name_buf,
                                  opt.label, steps, opt.other_label, other_steps));

        diff(out, opt, name_buf, opcode, jit_result, other_result, steps);
    }

    ee::destroy(ee);

    free(fake_bus);
    free(g_ram);

    g_ram = nullptr;

    return out->failures;
}

}
