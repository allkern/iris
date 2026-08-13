#include <cstring>

#include <fmt/format.h>

#include "common.hpp"

#include "iop/iop.hpp"
#include "iop/iop_def.hpp"
#include "iop/iop_dis.hpp"

namespace jitdiff {

using namespace iris;

namespace {

constexpr uint32_t RAM_SIZE = 0x200000;
constexpr uint32_t CODE_PC = 0x00100000;
constexpr uint32_t SCRATCH = 0x00080000;
constexpr uint32_t SCRATCH_SIZE = 0x100;

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

struct State {
    uint32_t r[32];
    uint32_t hi, lo;
    uint32_t pc;
    uint32_t cop0_r[16];
    uint8_t mem[SCRATCH_SIZE];
};

void capture(iop::Iop* iop, State* s) {
    memcpy(s->r, iop->r, sizeof(s->r));
    memcpy(s->cop0_r, iop->cop0_r, sizeof(s->cop0_r));

    s->hi = iop->hi;
    s->lo = iop->lo;
    s->pc = iop->pc;

    memcpy(s->mem, g_ram + SCRATCH, SCRATCH_SIZE);
}

void restore(iop::Iop* iop, const State* s) {
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

enum Kind { ALU, MEM, BRANCH, JUMP, COP0, TRAP };

struct Entry {
    const char* name;
    uint32_t op;        // primary opcode, 0x00 for SPECIAL
    uint32_t funct;     // SPECIAL funct, REGIMM rt selector, or COP0 rs selector
    Kind kind;
    bool uses_shamt;
};

const Entry g_entries[] = {
    { "sll",     0x00, 0x00, ALU,    true  },
    { "srl",     0x00, 0x02, ALU,    true  },
    { "sra",     0x00, 0x03, ALU,    true  },
    { "sllv",    0x00, 0x04, ALU,    false },
    { "srlv",    0x00, 0x06, ALU,    false },
    { "srav",    0x00, 0x07, ALU,    false },
    { "jr",      0x00, 0x08, JUMP,   false },
    { "jalr",    0x00, 0x09, JUMP,   false },
    { "syscall", 0x00, 0x0c, TRAP,   false },
    { "break",   0x00, 0x0d, TRAP,   false },
    { "mfhi",    0x00, 0x10, ALU,    false },
    { "mthi",    0x00, 0x11, ALU,    false },
    { "mflo",    0x00, 0x12, ALU,    false },
    { "mtlo",    0x00, 0x13, ALU,    false },
    { "mult",    0x00, 0x18, ALU,    false },
    { "multu",   0x00, 0x19, ALU,    false },
    { "div",     0x00, 0x1a, ALU,    false },
    { "divu",    0x00, 0x1b, ALU,    false },
    { "add",     0x00, 0x20, ALU,    false },
    { "addu",    0x00, 0x21, ALU,    false },
    { "sub",     0x00, 0x22, ALU,    false },
    { "subu",    0x00, 0x23, ALU,    false },
    { "and",     0x00, 0x24, ALU,    false },
    { "or",      0x00, 0x25, ALU,    false },
    { "xor",     0x00, 0x26, ALU,    false },
    { "nor",     0x00, 0x27, ALU,    false },
    { "slt",     0x00, 0x2a, ALU,    false },
    { "sltu",    0x00, 0x2b, ALU,    false },

    { "bltz",    0x01, 0x00, BRANCH, false },
    { "bgez",    0x01, 0x01, BRANCH, false },
    { "bltzal",  0x01, 0x10, BRANCH, false },
    { "bgezal",  0x01, 0x11, BRANCH, false },

    { "j",       0x02, 0x00, JUMP,   false },
    { "jal",     0x03, 0x00, JUMP,   false },
    { "beq",     0x04, 0x00, BRANCH, false },
    { "bne",     0x05, 0x00, BRANCH, false },
    { "blez",    0x06, 0x00, BRANCH, false },
    { "bgtz",    0x07, 0x00, BRANCH, false },
    { "addi",    0x08, 0x00, ALU,    false },
    { "addiu",   0x09, 0x00, ALU,    false },
    { "slti",    0x0a, 0x00, ALU,    false },
    { "sltiu",   0x0b, 0x00, ALU,    false },
    { "andi",    0x0c, 0x00, ALU,    false },
    { "ori",     0x0d, 0x00, ALU,    false },
    { "xori",    0x0e, 0x00, ALU,    false },
    { "lui",     0x0f, 0x00, ALU,    false },
    { "lb",      0x20, 0x00, MEM,    false },
    { "lh",      0x21, 0x00, MEM,    false },
    { "lwl",     0x22, 0x00, MEM,    false },
    { "lw",      0x23, 0x00, MEM,    false },
    { "lbu",     0x24, 0x00, MEM,    false },
    { "lhu",     0x25, 0x00, MEM,    false },
    { "lwr",     0x26, 0x00, MEM,    false },
    { "sb",      0x28, 0x00, MEM,    false },
    { "sh",      0x29, 0x00, MEM,    false },
    { "swl",     0x2a, 0x00, MEM,    false },
    { "sw",      0x2b, 0x00, MEM,    false },
    { "swr",     0x2e, 0x00, MEM,    false },

    { "mfc0",    0x10, 0x00, COP0,   false },
    { "mtc0",    0x10, 0x04, COP0,   false },
    { "rfe",     0x10, 0x10, COP0,   false }
};

constexpr int ENTRY_COUNT = (int)(sizeof(g_entries) / sizeof(*g_entries));

uint32_t encode(const Entry& e, Rng& rng, uint32_t* out_rs) {
    uint32_t rs = 1 + rng.u32() % 31;
    uint32_t rt = 1 + rng.u32() % 31;
    uint32_t rd = 1 + rng.u32() % 31;
    uint32_t sa = rng.u32() % 32;
    uint32_t imm = rng.u32() & 0xffff;

    *out_rs = rs;

    switch (e.op) {
        case 0x00:
            if (e.uses_shamt) rs = 0;

            return (rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | e.funct;

        case 0x01:
            // Keep branch displacements tiny so the target stays inside RAM.
            return (0x01u << 26) | (rs << 21) | (e.funct << 16) | (rng.u32() & 0x3f);

        case 0x02:
        case 0x03:
            return (e.op << 26) | ((CODE_PC >> 2) & 0x3fffffu);

        case 0x10:
            // rs selects the COP0 operation, rd the COP0 register.
            return (0x10u << 26) | (e.funct << 21) | (rt << 16) | ((rng.u32() % 16) << 11) |
                   (e.funct == 0x10 ? 0x10u : 0u);

        default:
            break;
    }

    if (e.kind == BRANCH)
        imm = rng.u32() & 0x3f;

    if (e.kind == MEM)
        imm = rng.u32() & 0x1f;

    return (e.op << 26) | (rs << 21) | (rt << 16) | imm;
}

const char* g_cop0_name[16] = {
    "0", "1", "2", "BPC", "4", "BDA", "JUMPDEST", "DCIC",
    "BADVADDR", "BDAM", "10", "BPCM", "SR", "CAUSE", "EPC", "PRID"
};

void diff(Report* rep, const Entry& e, uint32_t opcode,
          const State& a, const State& b, int steps) {
    char buf[128];
    iop::dis::Dis ds;

    ds.print_address = 0;
    ds.print_opcode = 0;
    ds.addr = CODE_PC;

    std::string text = iop::dis::disassemble(buf, opcode, &ds);

    auto note = [&](const std::string& what, uint64_t ia, uint64_t ib) {
        rep->fail(
            fmt::format("iop {} {}", e.name, what),
            fmt::format("op={:08x} [{}] steps={} jit={:08x} interp={:08x}",
                        opcode, text, steps, ia, ib));
    };

    for (int i = 0; i < 32; i++)
        if (a.r[i] != b.r[i])
            note("gpr", a.r[i], b.r[i]);

    if (a.hi != b.hi) note("hi", a.hi, b.hi);
    if (a.lo != b.lo) note("lo", a.lo, b.lo);
    if (a.pc != b.pc) note("pc", a.pc, b.pc);

    for (int i = 0; i < 16; i++)
        if (a.cop0_r[i] != b.cop0_r[i])
            note(fmt::format("cop0.{}", g_cop0_name[i]), a.cop0_r[i], b.cop0_r[i]);

    if (memcmp(a.mem, b.mem, SCRATCH_SIZE) != 0) {
        for (uint32_t i = 0; i < SCRATCH_SIZE; i++) {
            if (a.mem[i] != b.mem[i]) {
                note("mem", a.mem[i], b.mem[i]);

                break;
            }
        }
    }
}

}

int run_iop_tests(logger::Logger* logger, uint64_t seed, int iterations, Report* out) {
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

    out->core = "iop";
    out->seed = seed;

    Rng rng(seed);

    State s0, jit_state, int_state;

    for (int it = 0; it < iterations; it++) {
        const Entry& e = g_entries[it % ENTRY_COUNT];

        uint32_t rs = 0;
        uint32_t opcode = encode(e, rng, &rs);

        ram_write(CODE_PC, opcode, 4);
        ram_write(CODE_PC + 4, 0, 4);

        for (int i = 1; i < 32; i++)
            s0.r[i] = rng.interesting32();

        s0.r[0] = 0;
        s0.hi = rng.interesting32();
        s0.lo = rng.interesting32();
        s0.pc = CODE_PC;

        memset(s0.cop0_r, 0, sizeof(s0.cop0_r));

        // Interrupts masked. An IRQ taken mid-test would desynchronise the two
        // runs for reasons that have nothing to do with codegen.
        s0.cop0_r[iop::COP0_SR] = 0;
        s0.cop0_r[iop::COP0_PRID] = 0x0000001f;

        if (e.kind == MEM)
            s0.r[rs] = SCRATCH + (rng.u32() % (SCRATCH_SIZE - 64));

        if (e.kind == JUMP && e.op == 0x00)
            s0.r[rs] = CODE_PC;

        // Last word on r0, after the base-register steering above: r0 reads as
        // zero and the recompiler is entitled to assume that.
        s0.r[0] = 0;

        for (uint32_t i = 0; i < SCRATCH_SIZE; i++)
            s0.mem[i] = (uint8_t)(rng.u32() & 0xff);

        restore(iop, &s0);

        // Targeted: flush_cache() walks a million cache pages per call.
        iop::invalidate_block(iop, CODE_PC);

        int steps = iop::run_block(iop, 1);

        capture(iop, &jit_state);

        restore(iop, &s0);

        for (int k = 0; k < steps; k++)
            iop::cycle(iop);

        capture(iop, &int_state);

        ++out->cases;

        diff(out, e, opcode, jit_state, int_state, steps);

    }

    iop::destroy(iop);

    free(g_ram);

    g_ram = nullptr;

    return out->failures;
}

}
