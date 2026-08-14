#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

#include <fmt/format.h>

#include "common.hpp"

#include "iop/iop.hpp"
#include "iop/iop_def.hpp"
#include "iop/iop_dis.hpp"

namespace iris::iop { Instruction decode(uint32_t opcode); }

namespace jitdiff {

using namespace iris;

namespace {

constexpr uint32_t RAM_SIZE = 0x200000;
constexpr uint32_t CODE_PC = 0x00100000;

// See the note on the EE window: the base register sits in the middle so a
// negative displacement is still inside it.
constexpr uint32_t SCRATCH = 0x00080000;
constexpr uint32_t SCRATCH_SIZE = 0x4000;
constexpr uint32_t SCRATCH_MID = SCRATCH + SCRATCH_SIZE / 2;

constexpr int MAX_BLOCK = 64;

// Registers the generator never assigns to, so a base or jump target survives
// the rest of the case. See the same three on the EE side.
constexpr uint32_t R_BASE  = 29;
constexpr uint32_t R_CONST = 28;
constexpr uint32_t R_JMP   = 30;

bool is_reserved(uint32_t r) {
    return r == R_BASE || r == R_CONST || r == R_JMP;
}

uint8_t* g_ram = nullptr;

uint32_t ram_read(uint32_t addr, int bytes) {
    uint32_t v = 0;

    for (int i = 0; i < bytes; i++)
        v |= (uint32_t)g_ram[(addr + i) & (RAM_SIZE - 1)] << (i * 8);

    return v;
}

void ram_write(uint32_t addr, uint32_t data, int bytes) {
    for (int i = 0; i < bytes; i++)
        g_ram[(addr + i) & (RAM_SIZE - 1)] = (uint8_t)(data >> (i * 8));
}

uint32_t bus_read8(void*, uint32_t addr) { return ram_read(addr, 1); }
uint32_t bus_read16(void*, uint32_t addr) { return ram_read(addr, 2); }
uint32_t bus_read32(void*, uint32_t addr) { return ram_read(addr, 4); }
void bus_write8(void*, uint32_t addr, uint32_t data) { ram_write(addr, data, 1); }
void bus_write16(void*, uint32_t addr, uint32_t data) { ram_write(addr, data, 2); }
void bus_write32(void*, uint32_t addr, uint32_t data) { ram_write(addr, data, 4); }

// What goes into the core before a case runs.
struct Seed {
    uint32_t r[32];
    uint32_t hi, lo;
    uint32_t cop0_r[32];
    uint8_t mem[SCRATCH_SIZE];
};

// What comes out, and the only thing compared. Scratch memory is hashed rather
// than kept verbatim so a recording stays small.
struct Result {
    uint32_t r[32];
    uint32_t hi, lo;
    uint32_t pc, next_pc;
    uint32_t branch, branch_taken, delay_slot;
    uint32_t cop0_r[32];
    uint64_t mem_hash;
};

void capture(iop::Iop* iop, Result* s) {
    memcpy(s->r, iop->r, sizeof(s->r));
    memcpy(s->cop0_r, iop->cop0_r, sizeof(s->cop0_r));

    s->hi = iop->hi;
    s->lo = iop->lo;
    s->pc = iop->pc;
    s->next_pc = iop->next_pc;

    s->branch = (uint32_t)iop->branch;
    s->branch_taken = (uint32_t)iop->branch_taken;
    s->delay_slot = (uint32_t)iop->delay_slot;

    s->mem_hash = hash64(g_ram + SCRATCH, SCRATCH_SIZE);
}

void apply(iop::Iop* iop, const Seed* s) {
    memcpy(iop->r, s->r, sizeof(s->r));
    memcpy(iop->cop0_r, s->cop0_r, sizeof(s->cop0_r));

    iop->hi = s->hi;
    iop->lo = s->lo;

    iop->pc = CODE_PC;
    iop->next_pc = CODE_PC + 4;
    iop->branch = 0;
    iop->branch_taken = 0;
    iop->delay_slot = 0;

    memcpy(g_ram + SCRATCH, s->mem, SCRATCH_SIZE);
}

void serialize(Blob* b, uint32_t tag, int steps, const Result& r) {
    b->put_u32(tag);
    b->put_u32((uint32_t)steps);

    for (int i = 0; i < 32; i++) b->put_u32(r.r[i]);

    b->put_u32(r.hi);
    b->put_u32(r.lo);
    b->put_u32(r.pc);
    b->put_u32(r.next_pc);
    b->put_u32(r.branch);
    b->put_u32(r.branch_taken);
    b->put_u32(r.delay_slot);

    for (int i = 0; i < 32; i++) b->put_u32(r.cop0_r[i]);

    b->put_u64(r.mem_hash);
}

void deserialize(Blob* b, uint32_t* tag, int* steps, Result* r) {
    *tag = b->get_u32();
    *steps = (int)b->get_u32();

    for (int i = 0; i < 32; i++) r->r[i] = b->get_u32();

    r->hi = b->get_u32();
    r->lo = b->get_u32();
    r->pc = b->get_u32();
    r->next_pc = b->get_u32();
    r->branch = b->get_u32();
    r->branch_taken = b->get_u32();
    r->delay_slot = b->get_u32();

    for (int i = 0; i < 32; i++) r->cop0_r[i] = b->get_u32();

    r->mem_hash = b->get_u64();
}

// rs holds the base address.
bool is_mem(uint32_t id) {
    switch (id) {
        case iop::IOP_I_LB:  case iop::IOP_I_LH:  case iop::IOP_I_LWL:
        case iop::IOP_I_LW:  case iop::IOP_I_LBU: case iop::IOP_I_LHU:
        case iop::IOP_I_LWR: case iop::IOP_I_SB:  case iop::IOP_I_SH:
        case iop::IOP_I_SWL: case iop::IOP_I_SW:  case iop::IOP_I_SWR:
        case iop::IOP_I_LWC0: case iop::IOP_I_LWC1:
        case iop::IOP_I_LWC2: case iop::IOP_I_LWC3:
        case iop::IOP_I_SWC0: case iop::IOP_I_SWC1:
        case iop::IOP_I_SWC2: case iop::IOP_I_SWC3:
            return true;

        default:
            return false;
    }
}

bool is_trapping_arith(uint32_t id) {
    return id == iop::IOP_I_ADD || id == iop::IOP_I_ADDI || id == iop::IOP_I_SUB;
}

// Leaves for the exception vector, so nothing after it in the case runs.
bool is_exception(uint32_t id) {
    return id == iop::IOP_I_SYSCALL || id == iop::IOP_I_BREAK;
}

enum Flow {
    FLOW_NONE,
    FLOW_PCREL,
    FLOW_JIMM,
    FLOW_JREG
};

Flow flow_of(uint32_t id) {
    switch (id) {
        case iop::IOP_I_BLTZ:   case iop::IOP_I_BGEZ:
        case iop::IOP_I_BLTZAL: case iop::IOP_I_BGEZAL:
        case iop::IOP_I_BEQ:    case iop::IOP_I_BNE:
        case iop::IOP_I_BLEZ:   case iop::IOP_I_BGTZ:
            return FLOW_PCREL;

        case iop::IOP_I_J:
        case iop::IOP_I_JAL:
            return FLOW_JIMM;

        case iop::IOP_I_JR:
        case iop::IOP_I_JALR:
            return FLOW_JREG;

        default:
            return FLOW_NONE;
    }
}

// Whether the decoder gives this encoding a delay slot. flow_of() has to agree
// with this or a case can put two of them in a row, which is the one encoding
// the recompiler refuses outright.
bool takes_delay_slot(uint32_t opcode) {
    return iop::decode(opcode).branch == 1;
}

struct Template {
    uint32_t opcode;
    uint32_t id;
};

const bool g_trace = getenv("JITDIFF_TRACE") != nullptr;

// The decoder decides what an encoding means, so ask it rather than keeping a
// hand-written list next to it that has to be remembered when one changes. The
// list this replaces was missing the COP load/store group entirely.
void discover(std::vector <Template>* out) {
    std::set <uint32_t> seen;

    auto probe = [&](uint32_t opcode) {
        iop::Instruction i = iop::decode(opcode);

        if (i.id == iop::IOP_I_INVALID) return;
        if (seen.count(i.id)) return;

        seen.insert(i.id);
        out->push_back({ opcode, i.id });
    };

    for (uint32_t op = 0; op < 64; op++) {
        for (uint32_t funct = 0; funct < 64; funct++) {
            probe((op << 26) | funct);

            for (uint32_t sel = 0; sel < 32; sel++) {
                // rs selects the COP operation, rt the branch condition
                probe((op << 26) | (sel << 21) | funct);
                probe((op << 26) | (sel << 16) | funct);
            }
        }
    }
}

struct RegPool {
    uint32_t r[32];
    uint32_t n = 0;

    uint32_t pick(Rng& rng) const { return r[rng.u32() % n]; }
};

RegPool make_pool(bool hold_back_reserved) {
    RegPool p;

    for (uint32_t i = 1; i < 32; i++) {
        if (hold_back_reserved && is_reserved(i))
            continue;

        p.r[p.n++] = i;
    }

    return p;
}

// Fill in the operand fields, then confirm the encoding still decodes to the
// template's instruction - the same bits select a sub-opcode in one group and
// carry an operand in another.
uint32_t randomise(const Template& t, Rng& rng, const RegPool& pool, uint32_t* out_rs) {
    // See the note on the EE side: the immediate-form candidate below drops
    // sixteen random bits over the rd field, so operands chosen from the pool
    // are not on their own enough to keep a reserved register out of a
    // destination.
    const bool avoid_reserved = pool.n < 31;
    uint32_t rs = pool.pick(rng);
    uint32_t rt = pool.pick(rng);
    uint32_t rd = pool.pick(rng);
    uint32_t sa = rng.u32() & 0x1f;
    uint32_t imm = rng.u32() & 0xffff;

    *out_rs = (t.opcode >> 21) & 0x1f;

    uint32_t candidates[5];
    int n = 0;

    candidates[n++] = t.opcode | (rs << 21) | (rt << 16) | (rd << 11) | (sa << 6);
    candidates[n++] = t.opcode | (rs << 21) | (rt << 16) | (rd << 11);
    candidates[n++] = t.opcode | (rs << 21) | (rt << 16) | imm;
    candidates[n++] = t.opcode | (rt << 16) | (rd << 11);
    candidates[n++] = t.opcode;

    uint32_t valid[5];
    int v = 0;

    for (int k = 0; k < n; k++) {
        if (iop::decode(candidates[k]).id != t.id)
            continue;

        if (is_mem(t.id) && ((candidates[k] >> 21) & 0x1f) == 0)
            continue;

        if (avoid_reserved && (is_reserved((candidates[k] >> 21) & 0x1f) ||
                               is_reserved((candidates[k] >> 16) & 0x1f) ||
                               is_reserved((candidates[k] >> 11) & 0x1f)))
            continue;

        valid[v++] = candidates[k];
    }

    if (!v)
        return 0;

    uint32_t chosen = valid[rng.u32() % (uint32_t)v];

    *out_rs = (chosen >> 21) & 0x1f;

    return chosen;
}

bool set_rs(uint32_t* opcode, uint32_t rs, uint32_t id) {
    uint32_t patched = (*opcode & ~(0x1fu << 21)) | (rs << 21);

    if (iop::decode(patched).id != id)
        return false;

    *opcode = patched;

    return true;
}

bool set_imm16(uint32_t* opcode, uint32_t imm, uint32_t id) {
    uint32_t patched = (*opcode & ~0xffffu) | (imm & 0xffff);

    if (iop::decode(patched).id != id)
        return false;

    *opcode = patched;

    return true;
}

struct Case {
    uint32_t words[MAX_BLOCK];
    int n = 0;

    uint32_t base_reg = 0;
    uint32_t base_val = 0;

    bool has_trapping_arith = false;
};

void steer_mem_base(Case* c, uint32_t base_reg, Rng& rng) {
    c->base_reg = base_reg;
    c->base_val = SCRATCH_MID + (rng.u32() % 0x400);
}

void steer_mem(Case* c, uint32_t base_reg, uint32_t* opcode, uint32_t id, Rng& rng) {
    steer_mem_base(c, base_reg, rng);

    int32_t off = (int32_t)(rng.u32() % (SCRATCH_SIZE / 2)) - (int32_t)(SCRATCH_SIZE / 4);

    set_imm16(opcode, (uint32_t)off & 0xffff, id);
}

bool build_single(Case* c, const Template& t, Rng& rng, const RegPool& pool) {
    uint32_t rs = 0;
    uint32_t opcode = randomise(t, rng, pool, &rs);

    if (!opcode)
        return false;

    Flow flow = flow_of(t.id);

    switch (flow) {
        case FLOW_PCREL: {
            // Small displacement so the target stays inside RAM.
            if (!set_imm16(&opcode, rng.u32() & 0x3f, t.id))
                return false;
        } break;

        case FLOW_JIMM: {
            opcode = (opcode & ~0x03ffffffu) | ((CODE_PC >> 2) & 0x03ffffffu);
        } break;

        case FLOW_JREG: {
            c->base_reg = rs;
            c->base_val = CODE_PC;
        } break;

        case FLOW_NONE:
            break;
    }

    if (is_mem(t.id))
        steer_mem(c, rs, &opcode, t.id, rng);

    c->words[c->n++] = opcode;
    c->words[c->n++] = 0;

    c->has_trapping_arith = is_trapping_arith(t.id);

    return true;
}

// A run of instructions, branches pointed back into the run, terminated by a
// jump to the start.
//
// The IOP recompiler builds one straight-line block per branch rather than a
// region with edges, so what this shape reaches on the IOP is the register
// cache and constant folding across a long block, and the chaining of one block
// to the next through the dispatcher.
bool build_block(Case* c, const std::vector <Template>& templates, const Template& first,
                 Rng& rng, const RegPool& pool, int size) {
    if (size < 2) size = 2;
    if (size > MAX_BLOCK - 4) size = MAX_BLOCK - 4;

    const int last = size;

    bool prev_was_branch = false;

    const bool const_base = (rng.u32() & 1) != 0;

    steer_mem_base(c, R_BASE, rng);

    bool used_first = false;
    int attempts = 0;

    while (c->n < last && attempts < 64 * MAX_BLOCK) {
        ++attempts;

        const Template& t = used_first ? templates[rng.u32() % templates.size()] : first;

        used_first = true;

        if (is_trapping_arith(t.id) || is_exception(t.id))
            continue;

        Flow flow = flow_of(t.id);

        if (flow != FLOW_NONE && (prev_was_branch || c->n + 1 >= last))
            continue;

        uint32_t rs = 0;
        uint32_t opcode = randomise(t, rng, pool, &rs);

        if (!opcode)
            continue;

        // Something that takes a delay slot but is not in flow_of() cannot have
        // its target steered back into the case, so it does not belong in one.
        if (flow == FLOW_NONE && takes_delay_slot(opcode))
            continue;

        if (is_mem(t.id)) {
            if (const_base) {
                if (c->n + 1 >= last)
                    continue;

                if (!set_rs(&opcode, R_CONST, t.id))
                    continue;

                if (!set_imm16(&opcode, rng.u32() % (SCRATCH_SIZE - 0x20), t.id))
                    continue;

                c->words[c->n++] = (0x0fu << 26) | (R_CONST << 16) | (SCRATCH >> 16);
            } else {
                if (!set_rs(&opcode, R_BASE, t.id))
                    continue;

                int32_t off = (int32_t)(rng.u32() % (SCRATCH_SIZE / 2)) - (int32_t)(SCRATCH_SIZE / 4);

                if (!set_imm16(&opcode, (uint32_t)off & 0xffff, t.id))
                    continue;
            }
        }

        switch (flow) {
            case FLOW_PCREL: {
                int32_t target_slot = (int32_t)(rng.u32() % (uint32_t)last);
                int32_t delay_slot = c->n + 1;

                if (!set_imm16(&opcode, (uint32_t)(target_slot - delay_slot) & 0xffff, t.id))
                    continue;
            } break;

            case FLOW_JIMM: {
                uint32_t target = CODE_PC + 4 * (rng.u32() % (uint32_t)last);
                uint32_t patched = (opcode & ~0x03ffffffu) | ((target >> 2) & 0x03ffffffu);

                if (iop::decode(patched).id != t.id)
                    continue;

                opcode = patched;
            } break;

            case FLOW_JREG: {
                if (!set_rs(&opcode, R_JMP, t.id))
                    continue;
            } break;

            case FLOW_NONE:
                break;
        }

        c->words[c->n++] = opcode;

        prev_was_branch = flow != FLOW_NONE;
    }

    if (prev_was_branch)
        c->words[c->n++] = 0;

    c->words[c->n++] = (0x02u << 26) | ((CODE_PC >> 2) & 0x03ffffffu);
    c->words[c->n++] = 0;

    return c->n > 2;
}

const char* g_cop0_name[32] = {
    "0", "1", "2", "BPC", "4", "BDA", "JUMPDEST", "DCIC",
    "BADVADDR", "BDAM", "10", "BPCM", "SR", "CAUSE", "EPC", "PRID",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
};

std::string disassemble(uint32_t opcode) {
    char buf[128];
    iop::dis::Dis ds;

    ds.print_address = 0;
    ds.print_opcode = 0;
    ds.addr = CODE_PC;

    return iop::dis::disassemble(buf, opcode, &ds);
}

std::string case_name(const Options& opt, const Case& c) {
    if (opt.shape == Shape::Block)
        return fmt::format("block[{}]", c.n);

    std::string text = disassemble(c.words[0]);

    return text.substr(0, text.find(' '));
}

std::string case_where(const Options& opt, const Case& c, uint32_t tag, int it) {
    if (opt.shape == Shape::Block)
        return fmt::format("case={} tag={:08x} words={}", it, tag, c.n);

    return fmt::format("op={:08x} [{}]", c.words[0], disassemble(c.words[0]));
}

void diff(Report* rep, const Options& opt, const std::string& name, const std::string& where,
          const Result& a, const Result& b, int steps) {
    auto note = [&](const std::string& what, uint32_t ia, uint32_t ib) {
        rep->fail(
            fmt::format("iop {} {}", name, what),
            fmt::format("{} steps={} {}={:08x} {}={:08x}",
                        where, steps, opt.label, ia, opt.other_label, ib));
    };

    for (int i = 0; i < 32; i++)
        if (a.r[i] != b.r[i])
            note("gpr", a.r[i], b.r[i]);

    if (a.hi != b.hi) note("hi", a.hi, b.hi);
    if (a.lo != b.lo) note("lo", a.lo, b.lo);
    if (a.pc != b.pc) note("pc", a.pc, b.pc);
    // pc bookkeeping is compared only against another recompiler run. Within a
    // block the two engines keep it on different conventions - the recompiler
    // sets next_pc to where the block leaves, the interpreter to the instruction
    // after the one it just retired, and the branch flags only exist in the
    // interpreter - so against the interpreter this is noise of the same kind as
    // the EPC-off-by-4 in the README, not a codegen difference.
    if (opt.mode != Mode::Interpreter) {
        if (a.next_pc != b.next_pc) note("next_pc", a.next_pc, b.next_pc);
        if (a.branch != b.branch) note("branch", a.branch, b.branch);
        if (a.branch_taken != b.branch_taken) note("branch_taken", a.branch_taken, b.branch_taken);
        if (a.delay_slot != b.delay_slot) note("delay_slot", a.delay_slot, b.delay_slot);
    }

    for (int i = 0; i < 32; i++)
        if (a.cop0_r[i] != b.cop0_r[i])
            note(fmt::format("cop0.{}", g_cop0_name[i]), a.cop0_r[i], b.cop0_r[i]);

    if (a.mem_hash != b.mem_hash) note("mem", a.mem_hash, b.mem_hash);
}

}

int run_iop_tests(Logger* logger, const Options& opt, Report* out) {
    g_ram = (uint8_t*)calloc(RAM_SIZE, 1);

    iop::Iop* iop = iop::create(logger);

    iop::bus::Iface iface = {};

    iface.udata = nullptr;
    iface.read8 = bus_read8;
    iface.read16 = bus_read16;
    iface.read32 = bus_read32;
    iface.write8 = bus_write8;
    iface.write16 = bus_write16;
    iface.write32 = bus_write32;

    iop::connect(iop, iface);
    iop::reset(iop);

    out->core = fmt::format("iop/{}", opt.pass);
    out->seed = opt.seed;

    std::vector <Template> templates;

    discover(&templates);

    printf("  iop opcodes discovered: %d\n", (int)templates.size());

    const RegPool pool = make_pool(opt.shape == Shape::Block);

    Rng rng(opt.seed);

    Seed s0;
    Result jit_result, other_result;

    for (int it = 0; it < opt.iterations; it++) {
        const Template& t = templates[it % templates.size()];

        Case c;

        bool built = opt.shape == Shape::Block
            ? build_block(&c, templates, t, rng, pool, opt.block_size)
            : build_single(&c, t, rng, pool);

        if (!built)
            continue;

        // Cases are not all the same length, and a shorter one must not be able
        // to fall off its own end into the tail of a longer one.
        memset(g_ram + CODE_PC, 0, MAX_BLOCK * 4);

        for (int k = 0; k < c.n; k++)
            ram_write(CODE_PC + 4 * k, c.words[k], 4);

        for (int i = 1; i < 32; i++)
            s0.r[i] = rng.interesting32();

        s0.hi = rng.interesting32();
        s0.lo = rng.interesting32();

        memset(s0.cop0_r, 0, sizeof(s0.cop0_r));

        // Interrupts masked. An IRQ taken mid-test would desynchronise the two
        // runs for reasons that have nothing to do with codegen.
        s0.cop0_r[iop::COP0_SR] = 0;
        s0.cop0_r[iop::COP0_PRID] = 0x0000001f;

        if (c.has_trapping_arith)
            for (int i = 1; i < 32; i++)
                s0.r[i] = rng.u32() & 0x3fffffff;

        if (c.base_reg)
            s0.r[c.base_reg] = c.base_val;

        if (opt.shape == Shape::Block) {
            s0.r[R_JMP] = CODE_PC;

            // R_CONST normally comes from the LUI in front of the memory
            // instruction that uses it, but a branch is allowed to land between
            // the two. Seed it so a skipped LUI is a no-op rather than a wild
            // address.
            s0.r[R_CONST] = SCRATCH;
        }

        // Last word on r0, after the base-register steering above: r0 reads as
        // zero and the recompiler is entitled to assume that.
        s0.r[0] = 0;

        for (uint32_t i = 0; i < SCRATCH_SIZE; i++)
            s0.mem[i] = (uint8_t)(rng.u32() & 0xff);

        uint32_t tag = opt.shape == Shape::Block
            ? (uint32_t)hash64(c.words, c.n * sizeof(uint32_t))
            : c.words[0];

        if (g_trace) {
            printf("    case %d tag=%08x words=%d\n", it, tag, c.n);

            for (int k = 0; k < c.n; k++)
                printf("      %08x: %08x\n", CODE_PC + 4 * k, c.words[k]);

            fflush(stdout);
        }

        apply(iop, &s0);

        // Targeted: flush_cache() walks a million cache pages per call.
        iop::invalidate_block(iop, CODE_PC);

        int steps = iop::run_block(iop, opt.budget);

        capture(iop, &jit_result);

        ++out->cases;

        if (opt.mode == Mode::Record) {
            serialize(opt.blob, tag, steps, jit_result);

            continue;
        }

        int other_steps = steps;

        if (opt.mode == Mode::Compare) {
            uint32_t recorded_tag = 0;

            deserialize(opt.blob, &recorded_tag, &other_steps, &other_result);

            if (!opt.blob->ok) {
                out->fail("iop recording truncated", "ran out of recorded cases");

                break;
            }

            if (recorded_tag != tag) {
                out->fail("iop recording mismatch",
                          fmt::format("case {} is {:08x} here, {:08x} in the recording",
                                      it, tag, recorded_tag));

                break;
            }
        } else {
            apply(iop, &s0);

            for (int k = 0; k < steps; k++)
                iop::cycle(iop);

            capture(iop, &other_result);
        }

        std::string name = case_name(opt, c);
        std::string where = case_where(opt, c, tag, it);

        if (other_steps != steps)
            out->fail(fmt::format("iop {} step count", name),
                      fmt::format("{} {}={} {}={}", where,
                                  opt.label, steps, opt.other_label, other_steps));

        diff(out, opt, name, where, jit_result, other_result, steps);
    }

    iop::destroy(iop);

    free(g_ram);

    g_ram = nullptr;

    return out->failures;
}

}
