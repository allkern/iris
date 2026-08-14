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

// Every memory instruction a case generates is steered into this window, so a
// store can never land on the code being executed or on anything else a later
// case depends on. The window is 16 KB and the base register sits in the middle
// of it, which is what leaves room for negative displacements - AArch64 encodes
// those differently from positive ones and they are worth reaching.
constexpr uint32_t SCRATCH_PHYS = 0x00080000;
constexpr uint32_t SCRATCH_PC = 0x80000000 | SCRATCH_PHYS;
constexpr uint32_t SCRATCH_SIZE = 0x4000;
constexpr uint32_t SCRATCH_MID = SCRATCH_PC + SCRATCH_SIZE / 2;

constexpr uint32_t SPR_PC = 0x70000000;
constexpr uint32_t SPR_SIZE = 0x4000;

// Room for the longest case plus its terminating jump and delay slot.
constexpr int MAX_BLOCK = 64;

// Status register bits ERET arbitrates on.
constexpr uint32_t SR_EXL_BIT = 1u << 1;
constexpr uint32_t SR_ERL_BIT = 1u << 2;

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

// The recompiler reaches the fastmem tables by reinterpreting the bus udata as
// an ee::bus::Bus. Left zeroed, every lookup misses and the slow callbacks above
// are the only path that ever runs - which is not what a shipped build does, and
// leaves the inline load/store sequence and the constant-address folding path
// emitted but never executed.
//
// The whole 32 MB RAM window is mirrored onto the 2 MB buffer, matching the mask
// the callbacks apply. Both paths have to agree about what an address holds, or
// the fast path is not a fast path, it is a second memory.
void map_fastmem(ee::bus::Bus* bus, bool on) {
    memset(bus->fastmem_r_table, 0, sizeof(bus->fastmem_r_table));
    memset(bus->fastmem_w_table, 0, sizeof(bus->fastmem_w_table));

    if (!on)
        return;

    for (uint32_t i = 0; i < 0x1000; i++) {
        void* p = g_ram + ((i * 0x2000) & (RAM_SIZE - 1));

        bus->fastmem_r_table[i] = p;
        bus->fastmem_w_table[i] = p;
    }
}

// Registers the generator never assigns to.
//
// A block of twenty instructions writing wherever they like will not leave a
// randomly chosen base register holding an address by the time the memory
// instruction that needs it executes. These three are held out of the operand
// pool so what apply() puts in them is still there.
constexpr uint32_t R_BASE  = 29;    // a scratch address, not known at compile time
constexpr uint32_t R_CONST = 28;    // a scratch address, re-materialised by a LUI
constexpr uint32_t R_JMP   = 30;    // a code address inside the case

bool is_reserved(uint32_t r) {
    return r == R_BASE || r == R_CONST || r == R_JMP;
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
    uint32_t acc, fcr, pc, next_pc;
    uint32_t branch, branch_taken, delay_slot;
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
    s->next_pc = ee->next_pc;

    s->branch = (uint32_t)ee->branch;
    s->branch_taken = (uint32_t)ee->branch_taken;
    s->delay_slot = (uint32_t)ee->delay_slot;

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

    // Counters run_block() consults. Left to accumulate across cases they cross
    // their idle-skip thresholds part way through a run, and every case after
    // that point silently stops executing.
    ee->exit_req = 0;
    ee->fmv_skip = 0;
    ee->exception = 0;
    ee->intc_reads = 0;
    ee->csr_reads = 0;
    ee->eenull_counter = 0;

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
    b->put_u32(r.next_pc);
    b->put_u32(r.branch);
    b->put_u32(r.branch_taken);
    b->put_u32(r.delay_slot);

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
    r->next_pc = b->get_u32();
    r->branch = b->get_u32();
    r->branch_taken = b->get_u32();
    r->delay_slot = b->get_u32();

    for (int i = 0; i < 32; i++) r->cop0_r[i] = b->get_u32();

    r->mem_hash = b->get_u64();
    r->spr_hash = b->get_u64();
}

// Instructions whose whole purpose is to leave the block: an exception taken
// part way through a block is a separate (architecture independent) question
// from whether the block computed the right values, and testing it here would
// only produce noise.
std::string disassemble(uint32_t opcode, uint32_t pc = CODE_PC) {
    char buf[192];
    ee::dis::Dis ds;

    ds.print_address = 0;
    ds.print_opcode = 0;
    ds.pseudo_instructions = 0;
    ds.pc = pc;

    return ee::dis::disassemble(buf, opcode, &ds);
}

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

// The set Shape::Except runs, and the set the other two shapes leave out. The
// TLB group is deliberately not in either: TLBWI and TLBWR edit the page table,
// which outlives the case that ran them, and a case has to start from the state
// the seed describes.
bool is_exception_op(int id) {
    switch (id) {
        case ee::I_SYSCALL:
        case ee::I_BREAK:
        case ee::I_ERET:

        // The other eight trap instructions - TGEI, TGEIU, TGEU, TLT, TLTI,
        // TLTIU, TLTU, TNEI - are not implemented in the core. Each one logs a
        // fatal error and leaves the block uncompiled, so a guest that used one
        // would stop making progress. Running them here only buries every other
        // result under a thousand copies of the same message, so they are left
        // out until the core grows them; that is a gap in the core, not in the
        // set this shape is meant to cover.
        case ee::I_TGE:  case ee::I_TNE:
        case ee::I_TEQ:  case ee::I_TEQI:
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

// CTC2 is not just a register write. Writing VI31 (CMSAR1) starts a VU1
// microprogram at that address, and the interlocking form (opcode bit 0) starts
// a VU0 one when VU0 is interlocked. Neither VU has a microprogram loaded here,
// so what runs is a page of zeroes with no E bit in it, and the core never comes
// back - the same reason VCALLMS is kept out (see discover()).
//
// This is a gap, not a fix: the VU recompiler in vu_cached.cpp still has no
// harness of its own.
bool runs_vu_microcode(uint32_t opcode, int id) {
    if (id != ee::I_CTC2)
        return false;

    return ((opcode >> 11) & 0x1f) == 31 || (opcode & 1);
}

// How an instruction leaves the sub-block it ends. The recompiler decides where
// the edges of a region go from exactly this classification, so a case that
// wants a particular control-flow shape has to encode the target the same way
// the recompiler will read it back.
enum Flow {
    FLOW_NONE,
    FLOW_PCREL,   // 16-bit displacement off the delay slot
    FLOW_JIMM,    // 26-bit absolute target within the 256 MB region
    FLOW_JREG     // target comes from rs
};

Flow flow_of(int id) {
    switch (id) {
        case ee::I_BEQ:    case ee::I_BNE:    case ee::I_BLEZ:   case ee::I_BGTZ:
        case ee::I_BEQL:   case ee::I_BNEL:   case ee::I_BLEZL:  case ee::I_BGTZL:
        case ee::I_BLTZ:   case ee::I_BGEZ:   case ee::I_BLTZL:  case ee::I_BGEZL:
        case ee::I_BLTZAL: case ee::I_BGEZAL: case ee::I_BLTZALL: case ee::I_BGEZALL:
        case ee::I_BC0F:   case ee::I_BC0T:   case ee::I_BC0FL:  case ee::I_BC0TL:
        case ee::I_BC1F:   case ee::I_BC1T:   case ee::I_BC1FL:  case ee::I_BC1TL:
        case ee::I_BC2F:   case ee::I_BC2T:   case ee::I_BC2FL:  case ee::I_BC2TL:
            return FLOW_PCREL;

        case ee::I_J:
        case ee::I_JAL:
            return FLOW_JIMM;

        case ee::I_JR:
        case ee::I_JALR:
            return FLOW_JREG;

        default:
            return FLOW_NONE;
    }
}

// Whether the decoder gives this encoding a delay slot. The list in flow_of()
// has to agree with this or a case can put two of them in a row, which is the
// one encoding the recompiler refuses outright - so ask the decoder rather than
// trusting the list, and drop anything the list does not know how to aim.
bool takes_delay_slot(uint32_t opcode) {
    int b = ee::decode(opcode).branch;

    return b == 1 || b == 3;
}

struct Template {
    uint32_t opcode;
    int id;
};

const bool g_trace = getenv("JITDIFF_TRACE") != nullptr;

// The decoder is the only authority on which encodings mean what, so ask it:
// walk the selector fields, keep one representative encoding per instruction.
void discover(std::vector <Template>* out, Shape shape) {
    std::set <int> seen;

    const bool want_exceptions = shape == Shape::Except;

    auto probe = [&](uint32_t opcode) {
        // COP2 with the CO bit set is a VU0 macro-mode instruction. The
        // recompiler hands every one of these to the same interpreter routine
        // the interpreter uses, so there is no codegen to compare - and
        // VCALLMS would run VU0 microcode that never reaches an E bit and hang
        // the test. CFC2/CTC2/QMFC2/QMTC2 have the CO bit clear and stay in.
        if ((opcode >> 26) == 0x12 && (opcode & 0x02000000)) return;

        ee::Instruction i = ee::decode(opcode);

        if (want_exceptions ? !is_exception_op(i.id) : excluded(i.id)) return;
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

// The registers the generator is allowed to write. Shape::Block holds three of
// them back to keep base and jump-target registers intact across a case; the
// single-instruction shape has nothing to protect and uses all of them.
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

// Fill in operand fields, then confirm the instruction still decodes to the
// same thing - the same bits are operands in one group and sub-opcode selectors
// in another, so the decoder has to arbitrate.
//
// A candidate is picked at random among those that survive, not first-match.
// The shift-amount field matters here: the template always carries sa=0
// (discover() finds it first), and first-match would pin every MMI shift to a
// shift of zero forever. Zero is worth testing - it is the one AArch64 cannot
// encode for a right shift - but so is everything else.
uint32_t randomise(const Template& t, Rng& rng, const RegPool& pool, uint32_t* out_rs) {
    // Picking the operands out of the pool is not on its own enough to keep the
    // reserved registers out of an encoding. The immediate-form candidate below
    // drops sixteen random bits over the rd and sa fields, and for an R-type
    // instruction that names a destination register the pool never chose - which
    // is how a case ends up with "nor $30, ...", overwriting the register the
    // jump three instructions later depends on.
    const bool avoid_reserved = pool.n < 31;
    uint32_t rs = pool.pick(rng);
    uint32_t rt = pool.pick(rng);
    uint32_t rd = pool.pick(rng);
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

// Overwrite the base register field and re-check the decode. Safe for the
// groups this is used on - a memory op and JR/JALR both take a plain register
// there - but the check is what makes it safe to say so.
bool set_rs(uint32_t* opcode, uint32_t rs, int id) {
    uint32_t patched = (*opcode & ~(0x1fu << 21)) | (rs << 21);

    if (ee::decode(patched).id != id)
        return false;

    *opcode = patched;

    return true;
}

bool set_imm16(uint32_t* opcode, uint32_t imm, int id) {
    uint32_t patched = (*opcode & ~0xffffu) | (imm & 0xffff);

    if (ee::decode(patched).id != id)
        return false;

    *opcode = patched;

    return true;
}

// One generated case: the words to run, and which registers have to be seeded
// with what for those words to mean anything.
struct Case {
    uint32_t words[MAX_BLOCK];
    int n = 0;

    // Register the memory instructions use as a base, and what it must hold.
    // Zero when the case has no memory instruction that needs seeding.
    uint32_t base_reg = 0;
    uint32_t base_val = 0;

    bool has_trapping_arith = false;
};

// Where a memory instruction points. Both ends of the scratch window have to be
// reachable from the base register for a negative displacement to be legal, so
// the base sits in the middle and the displacement covers half the window either
// way. A quarter of them are aimed at the scratchpad instead, which is a
// separate branch inside the address decode that nothing else here reaches.
void steer_mem_base(Case* c, uint32_t base_reg, Rng& rng) {
    c->base_reg = base_reg;
    c->base_val = SCRATCH_MID + (rng.u32() % 0x400);

    if ((rng.u32() & 3) == 0)
        c->base_val = SPR_PC + (SPR_SIZE / 2) + (rng.u32() % 0x400);
}

void steer_mem(Case* c, uint32_t base_reg, uint32_t* opcode, int id, Rng& rng) {
    steer_mem_base(c, base_reg, rng);

    int32_t off = (int32_t)(rng.u32() % (SCRATCH_SIZE / 2)) - (int32_t)(SCRATCH_SIZE / 4);

    set_imm16(opcode, (uint32_t)off & 0xffff, id);
}

// A single instruction, a nop, and a jump back to the start. The nop matters:
// if the instruction under test is itself a branch, that nop is its delay slot,
// and a branch there is an encoding the recompiler explicitly refuses.
bool build_single(Case* c, const Template& t, Rng& rng, const RegPool& pool) {
    uint32_t rs = 0;
    uint32_t opcode = randomise(t, rng, pool, &rs);

    if (!opcode)
        return false;

    if (runs_vu_microcode(opcode, t.id))
        return false;

    if (is_mem(t.id))
        steer_mem(c, rs, &opcode, t.id, rng);

    c->words[c->n++] = opcode;
    c->words[c->n++] = 0;
    c->words[c->n++] = (0x02u << 26) | ((CODE_PC >> 2) & 0x3fffffu);
    c->words[c->n++] = 0;

    c->has_trapping_arith = is_trapping_arith(t.id);

    return true;
}

// A run of instructions with the branches among them pointed back into the run,
// terminated by a jump to the start.
//
// Everything here exists to keep the case bounded and self-contained: branch
// displacements are recomputed to land on a slot inside the case, jump registers
// come from a register nothing writes, and memory instructions are steered at
// the scratch window. Left alone, a random 16-bit displacement leaves the page
// and the case turns into a walk through whatever happens to be in RAM.
bool build_block(Case* c, const std::vector <Template>& templates, const Template& first,
                 Rng& rng, const RegPool& pool, int size) {
    if (size < 2) size = 2;
    if (size > MAX_BLOCK - 4) size = MAX_BLOCK - 4;

    // Length varies case to case rather than being fixed. A short case keeps
    // every live value in a register; a long one runs the allocator out of them
    // and makes it spill, and the two are not the same code path.
    if (size > 3)
        size = 3 + (int)(rng.u32() % (uint32_t)(size - 2));

    // Slot the terminating jump lands on, and the ceiling for branch targets.
    const int last = size;

    bool prev_was_branch = false;

    // Whether the case wants a base register that holds an address across the
    // whole run, or one a LUI re-materialises immediately before each use. The
    // second is what puts a compile-time-known address in the register cache,
    // which is the only way the constant-folded pointer path is reached.
    const bool const_base = (rng.u32() & 1) != 0;

    steer_mem_base(c, R_BASE, rng);

    // Each case leads with one instruction from the round-robin, so a long run
    // still covers every opcode the decoder knows rather than only whatever the
    // uniform draw below happened to reach. One attempt: if that instruction
    // does not fit here it will come round again on a later case.
    bool used_first = false;

    // The draw can reject: a branch with no room for its delay slot, an operand
    // set that stops decoding to the template's instruction, a base register the
    // group will not accept. Bounded so a case can never spin on that.
    int attempts = 0;

    while (c->n < last && attempts < 64 * MAX_BLOCK) {
        ++attempts;

        const Template& t = used_first ? templates[rng.u32() % templates.size()] : first;

        used_first = true;

        // Overflow traps leave for the exception vector and the rest of the case
        // never runs. They have their own shape; here they would only cost
        // coverage of everything after them.
        if (is_trapping_arith(t.id))
            continue;

        Flow flow = flow_of(t.id);

        // A branch needs its delay slot inside the case, and a branch in a delay
        // slot is an encoding the recompiler refuses outright.
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

        if (runs_vu_microcode(opcode, t.id))
            continue;

        if (is_mem(t.id)) {
            // A LUI of the window base gives exactly SCRATCH_PC, so the whole
            // window is reachable with a non-negative displacement - and the
            // recompiler sees a register whose value it knows.
            if (const_base) {
                if (c->n + 1 >= last)
                    continue;

                if (!set_rs(&opcode, R_CONST, t.id))
                    continue;

                if (!set_imm16(&opcode, rng.u32() % (SCRATCH_SIZE - 0x20), t.id))
                    continue;

                c->words[c->n++] = (0x0fu << 26) | (R_CONST << 16) | (SCRATCH_PC >> 16);
            } else {
                if (!set_rs(&opcode, R_BASE, t.id))
                    continue;

                // Signed, so both directions off the middle of the window.
                int32_t off = (int32_t)(rng.u32() % (SCRATCH_SIZE / 2)) - (int32_t)(SCRATCH_SIZE / 4);

                if (!set_imm16(&opcode, (uint32_t)off & 0xffff, t.id))
                    continue;
            }
        }

        switch (flow) {
            case FLOW_PCREL: {
                // successors() reads the target as (delay slot address) + off,
                // so that is what has to be encoded for the recompiler to build
                // the edge the case is trying to create.
                int32_t target_slot = (int32_t)(rng.u32() % (uint32_t)last);
                int32_t delay_slot = c->n + 1;

                if (!set_imm16(&opcode, (uint32_t)(target_slot - delay_slot) & 0xffff, t.id))
                    continue;
            } break;

            case FLOW_JIMM: {
                uint32_t target = CODE_PC + 4 * (rng.u32() % (uint32_t)last);
                uint32_t patched = (opcode & ~0x03ffffffu) | ((target >> 2) & 0x03ffffffu);

                if (ee::decode(patched).id != t.id)
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

        // The next draw fills the delay slot. It is a real instruction, not a
        // nop - a delay slot is where the register cache has to stay correct
        // across an edge, which is the whole reason to generate one.
        prev_was_branch = flow != FLOW_NONE;
    }

    // The terminating jump must not land in a delay slot. Nothing above lets
    // that happen, but the attempt limit could.
    if (prev_was_branch)
        c->words[c->n++] = 0;

    c->words[c->n++] = (0x02u << 26) | ((CODE_PC >> 2) & 0x3fffffu);
    c->words[c->n++] = 0;

    return c->n > 2;
}

const char* g_cop0_name[32] = {
    "index", "random", "entrylo0", "entrylo1", "context", "pagemask", "wired", "r7",
    "badvaddr", "count", "entryhi", "compare", "status", "cause", "epc", "prid",
    "config", "r17", "r18", "r19", "r20", "r21", "r22", "badpaddr",
    "debug", "perf", "r26", "r27", "taglo", "taghi", "errorepc", "r31"
};

void diff(Report* rep, const Options& opt, const std::string& name, const std::string& where,
          const Result& a, const Result& b, int steps) {
    auto note = [&](const std::string& what, uint64_t ia, uint64_t ib) {
        rep->fail(
            fmt::format("ee {} {}", name, what),
            fmt::format("{} steps={} {}={:016x} {}={:016x}",
                        where, steps, opt.label, ia, opt.other_label, ib));
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


// The name a divergence is filed under. A single-instruction case is named
// after the instruction it runs. A block is a mix of them, so it is filed under
// its length and located by the tag in the detail line, which the seed and the
// case index reproduce.
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

}

int run_ee_tests(Logger* logger, const Options& opt, Report* out) {
    g_ram = (uint8_t*)calloc(RAM_SIZE, 1);

    // vfast_page_base() reinterprets bus udata as an ee::bus::Bus to reach the
    // fastmem tables.
    ee::bus::Bus* fake_bus = (ee::bus::Bus*)calloc(1, sizeof(ee::bus::Bus));

    map_fastmem(fake_bus, opt.fastmem);

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

    out->core = fmt::format("ee/{}", opt.pass);
    out->seed = opt.seed;

    std::vector <Template> templates;

    discover(&templates, opt.shape);

    printf("  ee opcodes discovered: %d\n", (int)templates.size());

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

        // Clear first: cases are not all the same length, and a shorter one must
        // not be able to fall off its own end into the tail of a longer one.
        memset(g_ram + CODE_PHYS, 0, MAX_BLOCK * 4);

        for (int k = 0; k < c.n; k++)
            ram_write(CODE_PHYS + 4 * k, c.words[k], 4);

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

        if (opt.shape == Shape::Except) {
            // ERET reads EPC or ErrorEPC depending on ERL, so both have to hold
            // an address worth returning to, and ERL and EXL have to take both
            // values or only one of the two arms is ever compiled and run.
            s0.cop0_r[14] = CODE_PC + 8;
            s0.cop0_r[30] = CODE_PC + 8;
            s0.cop0_r[12] |= (rng.u32() & 1) ? SR_EXL_BIT : 0;
            s0.cop0_r[12] |= (rng.u32() & 1) ? SR_ERL_BIT : 0;
        }

        if (c.has_trapping_arith) {
            for (int i = 1; i < 32; i++)
                s0.r[i].u64[0] = (uint64_t)(int64_t)(int32_t)(rng.u32() & 0x3fffffff);
        }

        if (c.base_reg) {
            s0.r[c.base_reg].u64[0] = (uint64_t)(int64_t)(int32_t)c.base_val;
            s0.r[c.base_reg].u64[1] = 0;
        }

        if (opt.shape == Shape::Block) {
            // JR/JALR go to the top of the case. Anywhere else is a valid guest
            // program too, but the top is the one that keeps the case bounded.
            s0.r[R_JMP].u64[0] = (uint64_t)(int64_t)(int32_t)CODE_PC;
            s0.r[R_JMP].u64[1] = 0;

            // R_CONST normally comes from the LUI in front of the memory
            // instruction that uses it. A branch is allowed to land between the
            // two, though, and then the LUI never runs - so give it the same
            // address up front and the skipped LUI becomes a no-op instead of
            // pointing the access at whatever the seed left there.
            s0.r[R_CONST].u64[0] = (uint64_t)(int64_t)(int32_t)SCRATCH_PC;
            s0.r[R_CONST].u64[1] = 0;
        }

        // Last word on r0, after every other seeding step. r0 reads as zero,
        // and the recompiler is entitled to assume that.
        s0.r[0].u64[0] = 0;
        s0.r[0].u64[1] = 0;

        for (uint32_t i = 0; i < SCRATCH_SIZE; i++)
            s0.mem[i] = (uint8_t)(rng.u32() & 0xff);

        for (uint32_t i = 0; i < SPR_SIZE; i++)
            s0.spr[i] = (uint8_t)(rng.u32() & 0xff);

        const uint32_t fmv_skip_bit = rng.u32();

        // Identifies the case in the report. A block is identified by the hash
        // of everything in it, since no single opcode names it.
        uint32_t tag = opt.shape == Shape::Block
            ? (uint32_t)hash64(c.words, c.n * sizeof(uint32_t))
            : c.words[0];

        if (g_trace) {
            printf("    case %d tag=%08x words=%d\n", it, tag, c.n);

            for (int k = 0; k < c.n; k++)
                printf("      %08x: %08x  %s\n", CODE_PC + 4 * k, c.words[k],
                       disassemble(c.words[k], CODE_PC + 4 * k).c_str());

            fflush(stdout);
        }

        apply(ee, &s0);

        // The ERET path is guarded on this: zero stores the return address
        // straight away, non-zero goes through skip_fmv first. Both arms are
        // emitted, and only one of them was ever taken.
        if (opt.shape == Shape::Except)
            ee->fmv_skip = (int)(fmv_skip_bit & 1);

        // Targeted: flush_cache() walks a million cache pages and clears the
        // 8 MB fastmem table, which would dominate the run.
        ee::invalidate_block(ee, CODE_PHYS);

        int steps = ee::run_block(ee, opt.budget);

        capture(ee, &jit_result);

        ++out->cases;

        out->note_steps(steps, opt.budget);

        if (opt.mode == Mode::Record) {
            serialize(opt.blob, tag, steps, jit_result);

            continue;
        }

        int other_steps = steps;

        if (opt.mode == Mode::Compare) {
            uint32_t recorded_tag = 0;

            deserialize(opt.blob, &recorded_tag, &other_steps, &other_result);

            if (!opt.blob->ok) {
                out->fail("ee recording truncated", "ran out of recorded cases");

                break;
            }

            if (recorded_tag != tag) {
                out->fail("ee recording mismatch",
                          fmt::format("case {} is {:08x} here, {:08x} in the recording",
                                      it, tag, recorded_tag));

                break;
            }
        } else {
            apply(ee, &s0);

            for (int k = 0; k < steps; k++)
                ee::step(ee);

            capture(ee, &other_result);
        }

        std::string name = case_name(opt, c);
        std::string where = case_where(opt, c, tag, it);

        if (other_steps != steps)
            out->fail(fmt::format("ee {} step count", name),
                      fmt::format("{} {}={} {}={}", where,
                                  opt.label, steps, opt.other_label, other_steps));

        diff(out, opt, name, where, jit_result, other_result, steps);
    }

    ee::destroy(ee);

    free(fake_bus);
    free(g_ram);

    g_ram = nullptr;

    return out->failures;
}

}
