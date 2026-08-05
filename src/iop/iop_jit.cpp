
#include <tuple>
#include <utility>

#include "iop.hpp"
#include "iop_def.hpp"
#include "iop_dis.hpp"

#include "iop_export.hpp"
#include <functional>
#include <type_traits>

namespace iris::iop {

bool is_executable_region(uint32_t addr) {
    // RAM and BIOS
    return (addr < 0x2000000) || ((addr >= 0x1fc00000) && (addr < 0x20000000));
}

void invalidate_cache_page(Iop* iop, uint32_t addr) {
    if (!is_executable_region(addr))
        return;

    uint32_t page = addr / _IOP_CACHE_PAGESIZE;

    if (iop->block_cache[page].dirty || !iop->block_cache[page].valid)
        return;

    // Note: Skip writes that land outside the code actually cached in this page.
    //       addr is physical here (translated by the caller), so min/max_code_addr
    //       must be tracked in physical space too (see cache_block).
    // 
    // Note: We might eventually have to clamp blocks to page boundaries, otherwise
    //       a block that crosses a page boundary might not be invalidated by a write to
    //       the adjacent page. This applies to the EE as well.
    if (addr < iop->block_cache[page].min_code_addr || addr >= (iop->block_cache[page].max_code_addr + 4))
        return;

    // iris_debug(iop, "Invalidating page at addr={:08x} page={} ({:08x}) min={:08x} max={:08x}", addr, page, (addr / _IOP_CACHE_PAGESIZE) * _IOP_CACHE_PAGESIZE, iop->block_cache[page].min_code_addr, iop->block_cache[page].max_code_addr);

    iop->block_cache[page].dirty = true;
    iop->block_lut_gen++;
}

#define IOP_INVALIDATE_PAGE(addr) { \
    invalidate_cache_page(iop, addr); \
}

const uint32_t bus_region_mask_table[] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0x7fffffff, 0x1fffffff, 0xffffffff, 0xffffffff
};

static inline uint32_t translate_addr(uint32_t addr) {
    // KSEG0
    if (addr >= 0x80000000 && addr < 0xA0000000)
        return addr - 0x80000000;

    // KSEG1
    if (addr >= 0xA0000000 && addr < 0xC0000000)
        return addr - 0xA0000000;

    // KUSEG, KSEG2
    return addr;
}

static inline uint32_t bus_read8(Iop* iop, uint32_t addr) {
    return iop->bus.read8(iop->bus.udata, translate_addr(addr));
}

static inline uint32_t bus_read16(Iop* iop, uint32_t addr) {
    return iop->bus.read16(iop->bus.udata, translate_addr(addr));
}

static inline uint32_t bus_read32(Iop* iop, uint32_t addr) {
    return iop->bus.read32(iop->bus.udata, translate_addr(addr));
}

static inline void bus_write8(Iop* iop, uint32_t addr, uint32_t data) {
    addr = translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write8(iop->bus.udata, addr, data);
}

static inline void bus_write16(Iop* iop, uint32_t addr, uint32_t data) {
    addr = translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write16(iop->bus.udata, addr, data);
}

static inline void bus_write32(Iop* iop, uint32_t addr, uint32_t data) {
    addr = translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write32(iop->bus.udata, addr, data);
}

// External functions
uint32_t read8(Iop* iop, uint32_t addr) {
    return iop->bus.read8(iop->bus.udata, translate_addr(addr));
}

uint32_t read16(Iop* iop, uint32_t addr) {
    return iop->bus.read16(iop->bus.udata, translate_addr(addr));
}

uint32_t read32(Iop* iop, uint32_t addr) {
    return iop->bus.read32(iop->bus.udata, translate_addr(addr));
}

void write8(Iop* iop, uint32_t addr, uint32_t data) {
    addr = translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write8(iop->bus.udata, translate_addr(addr), data);
}

void write16(Iop* iop, uint32_t addr, uint32_t data) {
    addr = translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write16(iop->bus.udata, translate_addr(addr), data);
}

void write32(Iop* iop, uint32_t addr, uint32_t data) {
    addr = translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write32(iop->bus.udata, translate_addr(addr), data);
}

static const uint32_t g_iop_cop0_write_mask_table[] = {
    0x00000000, // cop0r0   - N/A
    0x00000000, // cop0r1   - N/A
    0x00000000, // cop0r2   - N/A
    0xffffffff, // BPC      - Breakpoint on execute (R/W)
    0x00000000, // cop0r4   - N/A
    0xffffffff, // BDA      - Breakpoint on data access (R/W)
    0x00000000, // JUMPDEST - Randomly memorized jump address (R)
    0xffc0f03f, // DCIC     - Breakpoint control (R/W)
    0x00000000, // BadVaddr - Bad Virtual Address (R)
    0xffffffff, // BDAM     - Data Access breakpoint mask (R/W)
    0x00000000, // cop0r10  - N/A
    0xffffffff, // BPCM     - Execute breakpoint mask (R/W)
    0xffffffff, // SR       - System status register (R/W)
    0x00000300, // CAUSE    - Describes the most recently recognised exception (R)
    0x00000000, // EPC      - Return Address from Trap (R)
    0x00000000  // PRID     - Processor ID (R)
};

#define S (ins.rs)
#define T (ins.rt)
#define D (ins.rd)
#define IMM5 (ins.imm5)
#define IMM26 (ins.imm26)
#define IMM16 (ins.imm16)
#define IMM16S (ins.imm16s)

#define R_R0 (iop->r[0])
#define R_A0 (iop->r[4])
#define R_RA (iop->r[31])

#define DO_PENDING_LOAD { \
    iop->r[iop->load_d] = iop->load_v; \
    R_R0 = 0; \
    iop->load_v = 0xffffffff; \
    iop->load_d = 0; }

#define SE8(v) ((int32_t)((int8_t)v))
#define SE16(v) ((int32_t)((int16_t)v))

#define BRANCH(offset) { iop->next_pc = iop->pc + (offset); }

Iop* create(logger::Logger* logger) {
    Iop* iop = new Iop();

    iop->logger = logger;
    iop->logger_id = logger::register_source(logger, "iop");
    iop->jit_logger = new asmjit::FileLogger(stdout);
    iop->block_arena = arena::create(1024 * 1024 * 32);

    reset(iop);

    return iop;
}

void destroy(Iop* iop) {
    arena::destroy(iop->block_arena);

    flush_cache(iop);

    delete iop;
}

void connect(Iop* iop, iop::bus::Iface bus) {
    iop->bus = bus;
}

void init_kputchar(Iop* iop, void (*kputchar)(void*, char), void* udata) {
    iop->kputchar = kputchar;
    iop->kputchar_udata = udata;
}

void init_sm_putchar(Iop* iop, void (*sm_putchar)(void*, char), void* udata) {
    iop->sm_putchar = sm_putchar;
    iop->sm_putchar_udata = udata;
}

static inline int check_irq(Iop* iop) {
    return (iop->cop0_r[COP0_SR] & SR_IEC) &&
           (iop->cop0_r[COP0_SR] & iop->cop0_r[COP0_CAUSE] & 0x00000400);
}

static inline void print_disassembly(uint32_t pc, uint32_t opcode) {
    char buf[128];
    iop::dis::Dis state;

    state.print_address = 1;
    state.print_opcode = 1;
    state.addr = pc;

    puts(iop::dis::disassemble(buf, opcode, &state));
}

static inline void exception(Iop* iop, uint32_t cause) {
    if ((cause != CAUSE_SYSCALL) && (cause != CAUSE_INT))
        iris_debug(iop, "Crashed with cause {:02x} at pc={:08x} next={:08x} saved={:08x}", cause >> 2, iop->pc, iop->saved_pc, iop->saved_pc);

    // Set excode and clear 3 LSBs
    iop->cop0_r[COP0_CAUSE] &= 0xffffff80;
    iop->cop0_r[COP0_CAUSE] |= cause;

    // iris_debug(iop, "Exception with cause {:02x} at pc={:08x} next={:08x} saved={:08x}", cause >> 2, iop->pc, iop->next_pc, iop->saved_pc);

    iop->cop0_r[COP0_EPC] = iop->pc;

    if (iop->delay_slot) {
        iop->cop0_r[COP0_EPC] -= 4;
        iop->cop0_r[COP0_CAUSE] |= 0x80000000;
    }

    // Do exception stack push
    uint32_t mode = iop->cop0_r[COP0_SR] & 0x3f;

    iop->cop0_r[COP0_SR] &= 0xffffffc0;
    iop->cop0_r[COP0_SR] |= (mode << 2) & 0x3f;

    // Set PC to the vector selected on BEV
    iop->pc = (iop->cop0_r[COP0_SR] & SR_BEV) ? 0xbfc00180 : 0x80000080;

    iop->next_pc = iop->pc + 4;
}

Instruction decode(uint32_t opcode);

void cycle(Iop* iop) {
    iop->saved_pc = iop->pc;
    iop->delay_slot = iop->branch;
    iop->branch = 0;
    iop->branch_taken = 0;

    iop->opcode = bus_read32(iop, iop->pc);

    iop->pc = iop->next_pc;
    iop->next_pc += 4;

    if (check_irq(iop)) {
        exception(iop, CAUSE_INT);

        return;
    }

    Instruction i = decode(iop->opcode);

    i.func(iop, i);

    iop->last_cycles += 1;
    iop->total_cycles += 1;

    iop->r[0] = 0;
}

void reset(Iop* iop) {
    iop::hle::ioman::reset();

    for (CachePage& page : iop->block_cache) {
        page.blocks = nullptr;
        page.valid = false;
        page.dirty = false;
    }

    if (iop->block_arena) arena::reset(iop->block_arena);

    iop->block_lut_gen++;

    iop->rt.reset(asmjit::ResetPolicy::kHard);

    for (int i = 0; i < 32; i++)
        iop->r[i] = 0;

    for (int i = 0; i < 16; i++)
        iop->cop0_r[i] = 0;

    iop->pc = 0xbfc00000;
    iop->next_pc = iop->pc + 4;

    iop->cop0_r[COP0_SR] = 0x10900000;
    iop->cop0_r[COP0_PRID] = 0x0000001f;

    iop->opcode = 0;
    iop->hi = 0;
    iop->lo = 0;
    iop->load_d = 0;
    iop->load_v = 0;
    iop->last_cycles = 0;
    iop->total_cycles = 0;
    iop->biu_config = 0;
    iop->branch = 0;
    iop->delay_slot = 0;
    iop->branch_taken = 0;
    iop->saved_pc = 0;
    iop->last_cached_block = nullptr;
    iop->last_cached_block_pc = 0;
    iop->executing_cache_page = 0xffffffff;
    iop->deferred_invalidate_page = 0xffffffff;
}

void set_irq_pending(Iop* iop, int value) {
    if (value) {
        iop->cop0_r[COP0_CAUSE] |= SR_IM2;
    } else {
        iop->cop0_r[COP0_CAUSE] &= ~SR_IM2;
    }
}

static inline void i_invalid(Iop* iop, Instruction& ins) {
    iris_fatal_error(iop, "{:08x}: Illegal instruction {:08x}", iop->pc - 8, ins.opcode);

    exception(iop, CAUSE_RI);
}

static inline void i_bltz(Iop* iop, Instruction& ins) {
    iop->branch = 1;

    if ((int32_t)iop->r[S] < (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void i_bgez(Iop* iop, Instruction& ins) {
    iop->branch = 1;

    if ((int32_t)iop->r[S] >= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void i_bltzal(Iop* iop, Instruction& ins) {
    iop->branch = 1;

    int32_t s = (int32_t)iop->r[S];

    R_RA = iop->next_pc;

    if ((int32_t)s < (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void i_bgezal(Iop* iop, Instruction& ins) {
    iop->branch = 1;

    int32_t s = (int32_t)iop->r[S];

    R_RA = iop->next_pc;

    if ((int32_t)s >= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void i_j(Iop* iop, Instruction& ins) {
    iop->branch = 1;

    // If we get a 1 that means the call has been HLE'd
    if (test_module_hooks(iop))
        return;

    iop->next_pc = (iop->next_pc & 0xf0000000) | (IMM26 << 2);
}

static inline void i_jal(Iop* iop, Instruction& ins) {
    iop->branch = 1;

    R_RA = iop->next_pc;

    iop->next_pc = (iop->next_pc & 0xf0000000) | (IMM26 << 2);
}

static inline void i_beq(Iop* iop, Instruction& ins) {
    iop->branch = 1;
    iop->branch_taken = 0;

    if (iop->r[S] == iop->r[T])
        BRANCH(IMM16S << 2);
}

static inline void i_bne(Iop* iop, Instruction& ins) {
    iop->branch = 1;
    iop->branch_taken = 0;

    if (iop->r[S] != iop->r[T])
        BRANCH(IMM16S << 2);
}

static inline void i_blez(Iop* iop, Instruction& ins) {
    iop->branch = 1;
    iop->branch_taken = 0;

    if ((int32_t)iop->r[S] <= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void i_bgtz(Iop* iop, Instruction& ins) {
    iop->branch = 1;
    iop->branch_taken = 0;

    if ((int32_t)iop->r[S] > (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void i_addi(Iop* iop, Instruction& ins) {
    iop->r[T] = iop->r[S] + IMM16S;
}

static inline void i_addiu(Iop* iop, Instruction& ins) {
    iop->r[T] = iop->r[S] + IMM16S;
}

static inline void i_slti(Iop* iop, Instruction& ins) {
    iop->r[T] = (int32_t)iop->r[S] < (int32_t)IMM16S;
}

static inline void i_sltiu(Iop* iop, Instruction& ins) {
    iop->r[T] = iop->r[S] < IMM16S;
}

static inline void i_andi(Iop* iop, Instruction& ins) {
    iop->r[T] = iop->r[S] & IMM16;
}

static inline void i_ori(Iop* iop, Instruction& ins) {
    iop->r[T] = iop->r[S] | IMM16;
}

static inline void i_xori(Iop* iop, Instruction& ins) {
    iop->r[T] = iop->r[S] ^ IMM16;
}

static inline void i_lui(Iop* iop, Instruction& ins) {
    iop->r[T] = IMM16 << 16;
}

static inline void i_lb(Iop* iop, Instruction& ins) {
    iop->r[T] = SE8(bus_read8(iop, iop->r[S] + IMM16S));
}

static inline void i_lh(Iop* iop, Instruction& ins) {
    iop->r[T] = SE16(bus_read16(iop, iop->r[S] + IMM16S));
}

static inline void i_lwl(Iop* iop, Instruction& ins) {
    const uint32_t addr = iop->r[S] + IMM16S;
	const uint32_t shift = (addr & 0x3) << 3;
	const uint32_t load = bus_read32(iop, addr & 0xfffffffc);
    const uint32_t mask = 0x00ffffff >> shift;

	iop->r[T] = (iop->r[T] & mask) | (load << (24 - shift));
}

static inline void i_lw(Iop* iop, Instruction& ins) {
    iop->r[T] = bus_read32(iop, iop->r[S] + IMM16S);
}

static inline void i_lbu(Iop* iop, Instruction& ins) {
    iop->r[T] = bus_read8(iop, iop->r[S] + IMM16S);
}

static inline void i_lhu(Iop* iop, Instruction& ins) {
    iop->r[T] = bus_read16(iop, iop->r[S] + IMM16S);
}

static inline void i_lwr(Iop* iop, Instruction& ins) {
    const uint32_t addr = iop->r[S] + IMM16S;
	const uint32_t shift = (addr & 0x3) << 3;
	const uint32_t load = bus_read32(iop, addr & 0xfffffffc);
    const uint32_t mask = 0xffffff00 << (24 - shift);

	iop->r[T] = (iop->r[T] & mask) | (load >> shift);
}

static inline void i_sb(Iop* iop, Instruction& ins) {
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    bus_write8(iop, iop->r[S] + IMM16S, iop->r[T]);
}

static inline void i_sh(Iop* iop, Instruction& ins) {
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    bus_write16(iop, iop->r[S] + IMM16S, iop->r[T]);
}

static inline void i_swl(Iop* iop, Instruction& ins) {
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    const uint32_t addr = iop->r[S] + IMM16S;
	const uint32_t shift = (addr & 0x3) << 3;
	const uint32_t load = bus_read32(iop, addr & 0xfffffffc);
    const uint32_t mask = 0xffffff00 << shift;
    const uint32_t value = (iop->r[T] >> (24 - shift)) | (load & mask);

    bus_write32(iop, addr & 0xfffffffc, value);
}

static inline void i_sw(Iop* iop, Instruction& ins) {
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    const uint32_t s = iop->r[S];
    const uint32_t t = iop->r[T];
    const uint32_t addr = s + IMM16S;

    if (addr == 0xfffe0130) {
        iop->biu_config = t;

        return;
    }

    bus_write32(iop, addr, t);
}

static inline void i_swr(Iop* iop, Instruction& ins) {
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    const uint32_t addr = iop->r[S] + IMM16S;
	const uint32_t shift = (addr & 0x3) << 3;
	const uint32_t load = bus_read32(iop, addr & 0xfffffffc);
    const uint32_t mask = 0x00ffffff >> (24 - shift);
    const uint32_t value = (iop->r[T] << shift) | (load & mask);

    bus_write32(iop, addr & 0xfffffffc, value);
}

// Secondary
static inline void i_sll(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[T] << IMM5;
}

static inline void i_srl(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[T] >> IMM5;
}

static inline void i_sra(Iop* iop, Instruction& ins) {
    iop->r[D] = (int32_t)iop->r[T] >> IMM5;
}

static inline void i_sllv(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[T] << (iop->r[S] & 0x1f);
}

static inline void i_srlv(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[T] >> (iop->r[S] & 0x1f);
}

static inline void i_srav(Iop* iop, Instruction& ins) {
    iop->r[D] = (int32_t)iop->r[T] >> (iop->r[S] & 0x1f);
}

static inline void i_jr(Iop* iop, Instruction& ins) {
    iop->branch = 1;

    iop->next_pc = iop->r[S];
}

static inline void i_jalr(Iop* iop, Instruction& ins) {
    iop->branch = 1;

    uint32_t s = iop->r[S];

    iop->r[D] = iop->next_pc;

    iop->next_pc = s;
}

static inline void i_syscall(Iop* iop, Instruction& ins) {
    exception(iop, CAUSE_SYSCALL);
}

static inline void i_break(Iop* iop, Instruction& ins) {
    // exception(iop, CAUSE_BP);
}

static inline void i_mfhi(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->hi;
}

static inline void i_mthi(Iop* iop, Instruction& ins) {
    iop->hi = iop->r[S];
}

static inline void i_mflo(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->lo;
}

static inline void i_mtlo(Iop* iop, Instruction& ins) {
    iop->lo = iop->r[S];
}

static inline void i_mult(Iop* iop, Instruction& ins) {
    const int64_t s = (int64_t)((int32_t)iop->r[S]);
    const int64_t t = (int64_t)((int32_t)iop->r[T]);

    const uint64_t r = s * t;

    iop->hi = r >> 32;
    iop->lo = r & 0xffffffff;
}

static inline void i_multu(Iop* iop, Instruction& ins) {
    const uint64_t s = (uint64_t)iop->r[S];
    const uint64_t t = (uint64_t)iop->r[T];

    const uint64_t r = s * t;

    iop->hi = r >> 32;
    iop->lo = r & 0xffffffff;
}

static inline void i_div(Iop* iop, Instruction& ins) {
    const int32_t s = (int32_t)iop->r[S];
    const int32_t t = (int32_t)iop->r[T];

    if (!t) {
        iop->hi = s;
        iop->lo = (s >= 0) ? 0xffffffff : 1;
    } else if ((((uint32_t)s) == 0x80000000) && (t == -1)) {
        iop->hi = 0;
        iop->lo = 0x80000000;
    } else {
        iop->hi = (uint32_t)(s % t);
        iop->lo = (uint32_t)(s / t);
    }
}

static inline void i_divu(Iop* iop, Instruction& ins) {
    const uint32_t s = iop->r[S];
    const uint32_t t = iop->r[T];

    if (!t) {
        iop->hi = s;
        iop->lo = 0xffffffff;
    } else {
        iop->hi = s % t;
        iop->lo = s / t;
    }
}

static inline void i_add(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[S] + iop->r[T];
}

static inline void i_addu(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[S] + iop->r[T];
}

static inline void i_sub(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[S] - iop->r[T];
}

static inline void i_subu(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[S] - iop->r[T];
}

static inline void i_and(Iop* iop, Instruction& ins) {
    iop->r[D] =iop->r[S] & iop->r[T];
}

static inline void i_or(Iop* iop, Instruction& ins) {
    iop->r[D] =iop->r[S] | iop->r[T];
}

static inline void i_xor(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[S] ^ iop->r[T];
}

static inline void i_nor(Iop* iop, Instruction& ins) {
    iop->r[D] = ~(iop->r[S] | iop->r[T]);
}

static inline void i_slt(Iop* iop, Instruction& ins) {
    iop->r[D] = (int32_t)iop->r[S] < (int32_t)iop->r[T];
}

static inline void i_sltu(Iop* iop, Instruction& ins) {
    iop->r[D] = iop->r[S] < iop->r[T];
}

static inline void i_mfc0(Iop* iop, Instruction& ins) {
    iop->r[T] = iop->cop0_r[D];
}

static inline void i_mtc0(Iop* iop, Instruction& ins) {
    iop->cop0_r[D] = iop->r[T] & g_iop_cop0_write_mask_table[D];
}

static inline void i_rfe(Iop* iop, Instruction& ins) {
    uint32_t mode = iop->cop0_r[COP0_SR] & 0x3f;

    iop->cop0_r[COP0_SR] &= 0xfffffff0;
    iop->cop0_r[COP0_SR] |= mode >> 2;
}

static inline void i_nop(Iop* iop, Instruction& ins) {
    // Do nothing
}

Instruction decode(uint32_t opcode) {
    Instruction i = { 0 };

    i.opcode = opcode;
    i.rs = (opcode >> 21) & 0x1f;
    i.rt = (opcode >> 16) & 0x1f;
    i.rd = (opcode >> 11) & 0x1f;
    i.imm5 = (opcode >> 6) & 0x1f;
    i.imm16 = opcode & 0xffff;
    i.imm16s = (int32_t)(int16_t)i.imm16;
    i.imm26 = opcode & 0x3ffffff;
    i.dst = 0;
    i.src1 = 0;
    i.src2 = 0;
    i.branch = 0;

    switch ((opcode & 0xfc000000) >> 26) {
        case 0x00000000 >> 26: {
            switch (opcode & 0x0000003f) {
                case 0x00000000: i.dst = i.rd; i.src1 = i.rt; i.id = IOP_I_SLL; i.func = i_sll; return i;
                case 0x00000002: i.dst = i.rd; i.src1 = i.rt; i.id = IOP_I_SRL; i.func = i_srl; return i;
                case 0x00000003: i.dst = i.rd; i.src1 = i.rt; i.id = IOP_I_SRA; i.func = i_sra; return i;
                case 0x00000004: i.dst = i.rd; i.src1 = i.rt; i.src2 = i.rs; i.id = IOP_I_SLLV; i.func = i_sllv; return i;
                case 0x00000006: i.dst = i.rd; i.src1 = i.rt; i.src2 = i.rs; i.id = IOP_I_SRLV; i.func = i_srlv; return i;
                case 0x00000007: i.dst = i.rd; i.src1 = i.rt; i.src2 = i.rs; i.id = IOP_I_SRAV; i.func = i_srav; return i;
                case 0x00000008: i.branch = 1; i.id = IOP_I_JR; i.func = i_jr; return i;
                case 0x00000009: i.branch = 1; i.id = IOP_I_JALR; i.func = i_jalr; return i;
                case 0x0000000c: i.branch = 2; i.id = IOP_I_SYSCALL; i.func = i_syscall; return i;
                case 0x0000000d: i.branch = 2; i.id = IOP_I_BREAK; i.func = i_break; return i;
                case 0x00000010: i.dst = i.rd; i.id = IOP_I_MFHI; i.func = i_mfhi; return i;
                case 0x00000011: i.src1 = i.rs; i.id = IOP_I_MTHI; i.func = i_mthi; return i;
                case 0x00000012: i.dst = i.rd; i.id = IOP_I_MFLO; i.func = i_mflo; return i;
                case 0x00000013: i.src1 = i.rs; i.id = IOP_I_MTLO; i.func = i_mtlo; return i;
                case 0x00000018: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_MULT; i.func = i_mult; return i;
                case 0x00000019: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_MULTU; i.func = i_multu; return i;
                case 0x0000001a: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_DIV; i.func = i_div; return i;
                case 0x0000001b: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_DIVU; i.func = i_divu; return i;
                case 0x00000020: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_ADD; i.func = i_add; return i;
                case 0x00000021: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_ADDU; i.func = i_addu; return i;
                case 0x00000022: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SUB; i.func = i_sub; return i;
                case 0x00000023: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SUBU; i.func = i_subu; return i;
                case 0x00000024: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_AND; i.func = i_and; return i;
                case 0x00000025: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_OR; i.func = i_or; return i;
                case 0x00000026: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_XOR; i.func = i_xor; return i;
                case 0x00000027: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_NOR; i.func = i_nor; return i;
                case 0x0000002a: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SLT; i.func = i_slt; return i;
                case 0x0000002b: i.dst = i.rd; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SLTU; i.func = i_sltu; return i;
            } break;
        } break;
        case 0x04000000 >> 26: {
            switch ((opcode & 0x001f0000) >> 16) {
                case 0x00000000 >> 16: i.src1 = i.rs; i.branch = 1; i.id = IOP_I_BLTZ; i.func = i_bltz; return i;
                case 0x00010000 >> 16: i.src1 = i.rs; i.branch = 1; i.id = IOP_I_BGEZ; i.func = i_bgez; return i;
                case 0x00100000 >> 16: i.src1 = i.rs; i.branch = 1; i.id = IOP_I_BLTZAL; i.func = i_bltzal; return i;
                case 0x00110000 >> 16: i.src1 = i.rs; i.branch = 1; i.id = IOP_I_BGEZAL; i.func = i_bgezal; return i;
                // bltz/bgez dupes
                default: {
                    i.src1 = i.rs;
                    i.branch = 1;

                    if (opcode & 0x00010000) {
                        i.id = IOP_I_BGEZ; i.func = i_bgez;
                    } else {
                        i.id = IOP_I_BLTZ; i.func = i_bltz;
                    }

                    return i;
                }
            } break;
        } break;
        case 0x08000000 >> 26: i.branch = 1; i.id = IOP_I_J; i.func = i_j; return i;
        case 0x0c000000 >> 26: i.branch = 1; i.id = IOP_I_JAL; i.func = i_jal; return i;
        case 0x10000000 >> 26: i.src1 = i.rt; i.src2 = i.rs; i.branch = 1; i.id = IOP_I_BEQ; i.func = i_beq; return i;
        case 0x14000000 >> 26: i.src1 = i.rt; i.src2 = i.rs; i.branch = 1; i.id = IOP_I_BNE; i.func = i_bne; return i;
        case 0x18000000 >> 26: i.src1 = i.rs; i.branch = 1; i.id = IOP_I_BLEZ; i.func = i_blez; return i;
        case 0x1c000000 >> 26: i.src1 = i.rs; i.branch = 1; i.id = IOP_I_BGTZ; i.func = i_bgtz; return i;
        case 0x20000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_ADDI; i.func = i_addi; return i;
        case 0x24000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_ADDIU; i.func = i_addiu; return i;
        case 0x28000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_SLTI; i.func = i_slti; return i;
        case 0x2c000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_SLTIU; i.func = i_sltiu; return i;
        case 0x30000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_ANDI; i.func = i_andi; return i;
        case 0x34000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_ORI; i.func = i_ori; return i;
        case 0x38000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_XORI; i.func = i_xori; return i;
        case 0x3c000000 >> 26: i.dst = i.rt; i.id = IOP_I_LUI; i.func = i_lui; return i;
        case 0x40000000 >> 26: {
            switch ((opcode & 0x03e00000) >> 21) {
                case 0x00000000 >> 21: i.dst = i.rt; i.id = IOP_I_MFC0; i.func = i_mfc0; return i;
                case 0x00800000 >> 21: i.src1 = i.rt; i.id = IOP_I_MTC0; i.func = i_mtc0; return i;
                case 0x02000000 >> 21: i.id = IOP_I_RFE; i.func = i_rfe; return i;
            }
        } break;
        case 0x48000000 >> 26: i.id = IOP_I_INVALID; i.func = i_invalid; return i;
        case 0x80000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_LB; i.func = i_lb; return i;
        case 0x84000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_LH; i.func = i_lh; return i;
        case 0x88000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_LWL; i.func = i_lwl; return i;
        case 0x8c000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_LW; i.func = i_lw; return i;
        case 0x90000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_LBU; i.func = i_lbu; return i;
        case 0x94000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.id = IOP_I_LHU; i.func = i_lhu; return i;
        case 0x98000000 >> 26: i.dst = i.rt; i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_LWR; i.func = i_lwr; return i;
        case 0xa0000000 >> 26: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SB; i.func = i_sb; return i;
        case 0xa4000000 >> 26: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SH; i.func = i_sh; return i;
        case 0xa8000000 >> 26: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SWL; i.func = i_swl; return i;
        case 0xac000000 >> 26: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SW; i.func = i_sw; return i;
        case 0xb8000000 >> 26: i.src1 = i.rs; i.src2 = i.rt; i.id = IOP_I_SWR; i.func = i_swr; return i;
    }

    i.func = i_invalid;

    return i;
}

#define IOP(m) ujit::mem_ptr(iop->ptr, offsetof(Iop, m))

static inline void flush_reg_cache(Iop* iop, asmjit::ujit::UniCompiler* uc) {
    using namespace asmjit;

    for (int i = 1; i < 32; i++) {
        if (iop->reg_cache[i].valid && iop->reg_cache[i].dirty) {
            ujit::Gp reg = iop->reg_cache[i].reg;

            if (iop->reg_cache[i].constant) {
                reg = uc->new_gp32();

                uc->mov(reg, iop->reg_cache[i].value);
            }

            uc->store_u32(ujit::mem_ptr(iop->ptr, offsetof(Iop, r) + i * sizeof(uint32_t)), reg);
        }

        iop->reg_cache[i].valid = false;
        iop->reg_cache[i].dirty = false;
        iop->reg_cache[i].constant = false;
    }
}



template<typename F, typename = void>
struct function_traits;

// specialization for function types
template<typename R, typename ... A>
struct function_traits<R(A...)> {
    using return_type = R;
    using args_type = std::tuple<A...>;
};

// specialization for function pointer types
template<typename R, typename ... A>
struct function_traits<R(*)(A...)> {
    using return_type = R;
    using args_type = std::tuple<A...>;
};

template <class R, class... A>
static inline asmjit::FuncSignature jit_build_signature(R(*)(A...)) {
    return asmjit::FuncSignature::build<R, A...>();
}

template <class Func, class... Args, std::size_t... I>
static inline void jit_function_call_impl(asmjit::ujit::UniCompiler* uc, Func func, std::index_sequence<I...>, Args&&... args) {
    using namespace asmjit;
    using R = typename function_traits<Func>::return_type;

    InvokeNode* call;

    uc->cc->invoke(
        Out(call),
        (uintptr_t)func,
        jit_build_signature(func)
    );

    auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

    if constexpr (std::is_same_v<R, void>) {
        // All variadic args are call arguments.
        (call->set_arg(I, std::get<I>(args_tuple)), ...);
    } else {
        // The first variadic arg is the return-value destination; the rest are
        // the call arguments. I ranges over [0, sizeof...(Args) - 1).
        call->set_ret(0, std::get<0>(args_tuple));

        (call->set_arg(I, std::get<I + 1>(args_tuple)), ...);
    }
}

template <class Func, class... Args>
static inline void jit_function_call(asmjit::ujit::UniCompiler* uc, Func func, Args&&... args) {
    using R = typename function_traits<Func>::return_type;

    // Non-void calls reserve the first variadic arg for the return destination,
    // so they emit one fewer set_arg() than they have variadic args.
    constexpr std::size_t arg_count =
        std::is_same_v<R, void> ? sizeof...(Args) : sizeof...(Args) - 1;

    jit_function_call_impl(uc, func, std::make_index_sequence<arg_count>{}, std::forward<Args>(args)...);
}

static inline CachedReg& alloc_load_reg(Iop* iop, asmjit::ujit::UniCompiler* uc, int reg) {
    using namespace asmjit;

    if (iop->reg_cache[reg].valid)
        return iop->reg_cache[reg];

    iop->reg_cache[reg].reg = uc->new_gp32();
    iop->reg_cache[reg].valid = true;
    iop->reg_cache[reg].dirty = false;
    iop->reg_cache[reg].constant = false;

    uc->load_u32(iop->reg_cache[reg].reg, IOP(r[reg]));

    return iop->reg_cache[reg];
}

static inline CachedReg& alloc_store_reg(Iop* iop, asmjit::ujit::UniCompiler* uc, int reg) {
    using namespace asmjit;

    if (reg == 0) {
        iris_fatal_error(iop, "Warning: Attempting to allocate register 0 for writing. This is a no-op.");
    }

    if (iop->reg_cache[reg].valid) {
        if (iop->reg_cache[reg].constant) {
            iop->reg_cache[reg].reg = uc->new_gp32();

            // We don't need to move the constant into the newly allocated reg here
            // because the value will be overwritten anyways
            // uc->mov(iop->reg_cache[reg].reg, Imm(iop->reg_cache[reg].value));
        }

        iop->reg_cache[reg].constant = false;
        iop->reg_cache[reg].dirty = true;

        return iop->reg_cache[reg];
    }

    iop->reg_cache[reg].reg = uc->new_gp32();
    iop->reg_cache[reg].valid = true;
    iop->reg_cache[reg].dirty = true;
    iop->reg_cache[reg].constant = false;

    return iop->reg_cache[reg];
}

static inline CachedReg& alloc_const_reg(Iop* iop, asmjit::ujit::UniCompiler* uc, int reg, uint32_t value) {
    using namespace asmjit;

    if (reg == 0) {
        iris_fatal_error(iop, "Warning: Attempting to allocate register 0 for writing. This is a no-op.");
    }

    if (iop->reg_cache[reg].valid) {
        iop->reg_cache[reg].dirty = true;
        iop->reg_cache[reg].constant = true;
        iop->reg_cache[reg].value = value;

        return iop->reg_cache[reg];
    }

    // iop->reg_cache[reg].reg = uc->new_gp32();
    iop->reg_cache[reg].valid = true;
    iop->reg_cache[reg].dirty = true;
    iop->reg_cache[reg].constant = true;
    iop->reg_cache[reg].value = value;

    return iop->reg_cache[reg];
}

static inline void sextn(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Gp& reg, uint32_t bits) {
    uint32_t shift = 32u - bits;

    uc.shl(reg.r32(), reg.r32(), asmjit::Imm(shift));
    uc.sar(reg.r32(), reg.r32(), asmjit::Imm(shift));
}

// Materialize a multiply operand into a freshly-allocated 64-bit register,
// extended from 32 bits (sign-extended for MULT, zero-extended for MULTU).
// Constant cache entries are folded into the (already extended) immediate.
static inline asmjit::ujit::Gp mul_load_operand(asmjit::ujit::UniCompiler& uc, const CachedReg& c, bool is_signed) {
    asmjit::ujit::Gp r = uc.new_gp64();

    if (c.constant) {
        uint64_t v = is_signed ? (uint64_t)(int64_t)(int32_t)c.value : (uint64_t)(uint32_t)c.value;

        uc.mov(r, asmjit::Imm(v));
    } else {
        // Place the value in the low half, then extend with a 64-bit shift pair.
        // The shl writes the full 64-bit register (discarding any stale upper
        // bits), so the result never depends on implicit zero-extension.
        uc.mov(r.r32(), c.reg);
        uc.shl(r, r, asmjit::Imm(32));

        if (is_signed) {
            uc.sar(r, r, asmjit::Imm(32));
        } else {
            uc.shr(r, r, asmjit::Imm(32));
        }
    }

    return r;
}

// Return a 32-bit register holding the cached value: the cache register itself
// when live, or a freshly-materialized constant. The result is read-only, so
// callers must not write through it (it may alias a cached register).
static inline asmjit::ujit::Gp value_to_gp32(asmjit::ujit::UniCompiler& uc, const CachedReg& c) {
    if (!c.constant)
        return c.reg;

    asmjit::ujit::Gp r = uc.new_gp32();

    uc.mov(r, asmjit::Imm(c.value));

    return r;
}

void compile_block(Iop* iop, Block* block) {
    using namespace asmjit;

    CodeHolder code;

    code.init(iop->rt.environment(), iop->rt.cpu_features());
    // code.set_logger(iop->jit_logger);

    ujit::BackendCompiler bc(&code);
    ujit::UniCompiler uc(&bc, iop->rt.cpu_features(), iop->rt.cpu_hints());

    FuncNode* func = uc.add_func(FuncSignature::build<void, Iop*>());

    iop->ptr = uc.new_gp_ptr();

    func->set_arg(0, iop->ptr);

    asmjit::Label block_exit = uc.new_label();

    iop->reg_cache[0].valid = true;
    iop->reg_cache[0].dirty = false;
    iop->reg_cache[0].constant = true;
    iop->reg_cache[0].value = 0;

    // iris_debug(iop, "\nCompiling block at 0x{:08x} with {} instructions", block->start_pc, block->instructions.size());

    int index = 0;

    for (int i = 0; i < iop->instruction_buf_index; i++) {
        const Instruction& ins = iop->instruction_buf[i];

        bool first = index == 0;
        bool last = index == (iop->instruction_buf_index - 1);

        switch (ins.id) {
            case IOP_I_LB:
            case IOP_I_LBU:
            case IOP_I_LH:
            case IOP_I_LHU:
            case IOP_I_LW: {
                uint32_t (*func)(Iop*, uint32_t) = nullptr;

                switch (ins.id) {
                    case IOP_I_LB:
                    case IOP_I_LBU: func = bus_read8; break;
                    case IOP_I_LH:
                    case IOP_I_LHU: func = bus_read16; break;
                    case IOP_I_LW: func = bus_read32; break;
                }

                // If RS and RT are the same, alloc_store_reg will change the constant bool
                // to indicate that the register is no longer constant. We assign the result
                // of alloc_load_reg by value to preserve the constant bool and the
                // constant value for the following call.
                CachedReg s = alloc_load_reg(iop, &uc, S);

                // If RT is zero we should emit a call and discard the result
                if (!T) {
                    InvokeNode* call;

                    bc.invoke(
                        Out(call),
                        (uintptr_t)func,
                        FuncSignature::build<void, Iop*, uint32_t>()
                    );

                    call->set_arg(0, iop->ptr);

                    if (s.constant) {
                        call->set_arg(1, Imm(s.value + ins.imm16s));
                    } else {
                        ujit::Gp addr = uc.new_gp32();

                        uc.add(addr, s.reg, Imm(ins.imm16s));

                        call->set_arg(1, addr);
                    }

                    break;                    
                }

                CachedReg& t = alloc_store_reg(iop, &uc, T);

                if (s.constant) {
                    jit_function_call(&uc, func, t.reg, iop->ptr, Imm(s.value + ins.imm16s));
                } else {
                    ujit::Gp addr = uc.new_gp32();

                    uc.add(addr, s.reg, Imm(ins.imm16s));

                    jit_function_call(&uc, func, t.reg, iop->ptr, addr);
                }

                if (ins.id == IOP_I_LB || ins.id == IOP_I_LH) {
                    sextn(uc, t.reg, ins.id == IOP_I_LB ? 8 : 16);
                }
            } break;

            case IOP_I_LWL:
            case IOP_I_LWR: {
                const bool is_lwl = ins.id == IOP_I_LWL;

                // Copy by value so the constant flags survive the store-reg
                // allocation below (which may reallocate when S aliases T).
                CachedReg s = alloc_load_reg(iop, &uc, S);
                CachedReg t = alloc_load_reg(iop, &uc, T);

                // LWL/LWR read the aligned word and splice it into the current
                // RT value (RT is both source and destination). The bus read
                // stays a call; the byte merge is emitted inline. This matches
                // the interpreter's no-load-delay behaviour: RT updates now.
                if (s.constant) {
                    // The address (and thus the byte shift) is known at compile
                    // time, so the mask and shifts all fold to immediates.
                    const uint32_t addr = s.value + ins.imm16s;
                    const uint32_t aligned = addr & 0xfffffffc;
                    const uint32_t shift = (addr & 0x3) << 3;
                    const uint32_t mask = is_lwl ? (0x00ffffffu >> shift)
                                                 : (0xffffff00u << (24 - shift));

                    // RT == $zero: read for side effects and discard the merge.
                    if (!T) {
                        InvokeNode* call;

                        bc.invoke(
                            Out(call),
                            (uintptr_t)bus_read32,
                            FuncSignature::build<void, Iop*, uint32_t>()
                        );

                        call->set_arg(0, iop->ptr);
                        call->set_arg(1, Imm(aligned));

                        break;
                    }

                    ujit::Gp load = uc.new_gp32();

                    jit_function_call(&uc, bus_read32, load, iop->ptr, Imm(aligned));

                    // loaded_part = LWL ? load << (24 - shift) : load >> shift
                    if (is_lwl) {
                        if (uint32_t c = 24 - shift) uc.shl(load, load, Imm(c));
                    } else {
                        if (shift) uc.shr(load, load, Imm(shift));
                    }

                    CachedReg& d = alloc_store_reg(iop, &uc, T);

                    if (t.constant) {
                        uc.or_(d.reg, load, Imm(t.value & mask));
                    } else {
                        ujit::Gp masked = uc.new_gp32();

                        uc.and_(masked, t.reg, Imm(mask));
                        uc.or_(d.reg, load, masked);
                    }

                    break;
                }

                // General case: the byte shift depends on the runtime address.
                ujit::Gp addr = uc.new_gp32();

                uc.add(addr, s.reg, Imm(ins.imm16s));

                ujit::Gp aligned = uc.new_gp32();

                uc.and_(aligned, addr, Imm(0xfffffffc));

                // shift = (addr & 3) << 3 ; inv = 24 - shift
                ujit::Gp shift = uc.new_gp32();

                uc.and_(shift, addr, Imm(0x3));
                uc.shl(shift, shift, Imm(3));

                ujit::Gp inv = uc.new_gp32();

                uc.mov(inv, Imm(24));
                uc.sub(inv, inv, shift);

                // RT == $zero: read for side effects and discard the merge.
                if (!T) {
                    InvokeNode* call;

                    bc.invoke(
                        Out(call),
                        (uintptr_t)bus_read32,
                        FuncSignature::build<void, Iop*, uint32_t>()
                    );

                    call->set_arg(0, iop->ptr);
                    call->set_arg(1, aligned);

                    break;
                }

                // Capture the old RT value before allocating the destination
                // (which may reuse t's register).
                ujit::Gp rt = value_to_gp32(uc, t);

                ujit::Gp load = uc.new_gp32();

                jit_function_call(&uc, bus_read32, load, iop->ptr, aligned);

                ujit::Gp mask = uc.new_gp32();

                if (is_lwl) {
                    // mask = 0x00ffffff >> shift ; load <<= (24 - shift)
                    uc.mov(mask, Imm(0x00ffffff));
                    uc.shr(mask, mask, shift);
                    uc.shl(load, load, inv);
                } else {
                    // mask = 0xffffff00 << (24 - shift) ; load >>= shift
                    uc.mov(mask, Imm(0xffffff00));
                    uc.shl(mask, mask, inv);
                    uc.shr(load, load, shift);
                }

                CachedReg& d = alloc_store_reg(iop, &uc, T);

                ujit::Gp masked = uc.new_gp32();

                uc.and_(masked, rt, mask);
                uc.or_(d.reg, load, masked);
            } break;

            case IOP_I_SB:
            case IOP_I_SH:
            case IOP_I_SW: {
                void (*func)(Iop*, uint32_t, uint32_t) = nullptr;

                switch (ins.id) {
                    case IOP_I_SB: func = bus_write8; break;
                    case IOP_I_SH: func = bus_write16; break;
                    case IOP_I_SW: func = bus_write32; break;
                }

                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                uint32_t mask = 0xffffffff;

                switch (ins.id) {
                    case IOP_I_SB: mask = 0xff; break;
                    case IOP_I_SH: mask = 0xffff; break;
                }

                asmjit::Label skip = uc.new_label();
                ujit::Gp sr = uc.new_gp32();

                uc.load_u32(sr, IOP(cop0_r[COP0_SR]));
                uc.and_(sr, sr, Imm(SR_ISC));
                uc.j(skip, ujit::test_nz(sr));

                if (s.constant && t.constant) {
                    uint32_t addr = s.value + ins.imm16s;

                    jit_function_call(&uc, func, iop->ptr, Imm(addr), Imm(t.value & mask));
                } else if (s.constant) {
                    jit_function_call(&uc, func, iop->ptr, Imm(s.value + ins.imm16s), t.reg);
                } else if (t.constant) {
                    ujit::Gp addr = uc.new_gp32();

                    uc.add(addr, s.reg, Imm(ins.imm16s));

                    jit_function_call(&uc, func, iop->ptr, addr, Imm(t.value & mask));
                } else {
                    ujit::Gp addr = uc.new_gp32();

                    uc.add(addr, s.reg, Imm(ins.imm16s));

                    jit_function_call(&uc, func, iop->ptr, addr, t.reg);
                }

                uc.bind(skip);
            } break;

            case IOP_I_SWL:
            case IOP_I_SWR: {
                const bool is_swl = ins.id == IOP_I_SWL;

                CachedReg s = alloc_load_reg(iop, &uc, S);
                CachedReg t = alloc_load_reg(iop, &uc, T);

                // SWL/SWR are read-modify-write: read the aligned word, splice
                // in the shifted RT bytes, write it back. The bus accesses stay
                // calls (they route through the bus + SMC invalidation); the
                // byte splicing is emitted inline. No SR_ISC guard here: SWL/SWR
                // are never emitted by the cache-flush routines that run with the
                // cache isolated, so the check would only ever cost cycles.
                if (s.constant) {
                    // The address (and thus the byte shift) is known at compile
                    // time, so the masks and shifts all fold to immediates.
                    const uint32_t addr = s.value + ins.imm16s;
                    const uint32_t aligned = addr & 0xfffffffc;
                    const uint32_t shift = (addr & 0x3) << 3;
                    const uint32_t mask = is_swl ? (0xffffff00u << shift)
                                                 : (0x00ffffffu >> (24 - shift));

                    ujit::Gp load = uc.new_gp32();

                    jit_function_call(&uc, bus_read32, load, iop->ptr, Imm(aligned));

                    ujit::Gp value = uc.new_gp32();

                    uc.and_(value, load, Imm(mask));

                    if (t.constant) {
                        const uint32_t part = is_swl ? (t.value >> (24 - shift))
                                                     : (t.value << shift);

                        uc.or_(value, value, Imm(part));
                    } else {
                        ujit::Gp part = uc.new_gp32();

                        if (is_swl) {
                            uc.shr(part, t.reg, Imm(24 - shift));
                        } else {
                            uc.shl(part, t.reg, Imm(shift));
                        }

                        uc.or_(value, value, part);
                    }

                    jit_function_call(&uc, bus_write32, iop->ptr, Imm(aligned), value);

                    break;
                }

                // General case: the byte shift depends on the runtime address,
                // so the masks and merges use variable shifts.
                ujit::Gp addr = uc.new_gp32();

                uc.add(addr, s.reg, Imm(ins.imm16s));

                ujit::Gp aligned = uc.new_gp32();

                uc.and_(aligned, addr, Imm(0xfffffffc));

                // shift = (addr & 3) << 3 ; inv = 24 - shift
                ujit::Gp shift = uc.new_gp32();

                uc.and_(shift, addr, Imm(0x3));
                uc.shl(shift, shift, Imm(3));

                ujit::Gp inv = uc.new_gp32();

                uc.mov(inv, Imm(24));
                uc.sub(inv, inv, shift);

                ujit::Gp load = uc.new_gp32();

                jit_function_call(&uc, bus_read32, load, iop->ptr, aligned);

                ujit::Gp t_reg = value_to_gp32(uc, t);
                ujit::Gp mask = uc.new_gp32();
                ujit::Gp value = uc.new_gp32();

                if (is_swl) {
                    // mask = 0xffffff00 << shift ; value = t >> (24 - shift)
                    uc.mov(mask, Imm(0xffffff00));
                    uc.shl(mask, mask, shift);
                    uc.shr(value, t_reg, inv);
                } else {
                    // mask = 0x00ffffff >> (24 - shift) ; value = t << shift
                    uc.mov(mask, Imm(0x00ffffff));
                    uc.shr(mask, mask, inv);
                    uc.shl(value, t_reg, shift);
                }

                uc.and_(load, load, mask);
                uc.or_(value, value, load);

                jit_function_call(&uc, bus_write32, iop->ptr, aligned, value);
            } break;

            case IOP_I_LUI: {
                if (!T) break;

                alloc_const_reg(iop, &uc, T, ins.imm16 << 16);
            } break;

            case IOP_I_ADDI:
            case IOP_I_ADDIU:
            case IOP_I_ANDI:
            case IOP_I_ORI:
            case IOP_I_XORI: {
                if (!T) break;

                CachedReg& s = alloc_load_reg(iop, &uc, S);

                if (s.constant) {
                    uint32_t result = 0;

                    switch (ins.id) {
                        // ADDI and ADDIU use a sign-extended immediate
                        case IOP_I_ADDI:
                        case IOP_I_ADDIU: result = s.value + ins.imm16s; break;

                        case IOP_I_ANDI: result = s.value & ins.imm16; break;
                        case IOP_I_ORI: result = s.value | ins.imm16; break;
                        case IOP_I_XORI: result = s.value ^ ins.imm16; break;
                    }

                    alloc_const_reg(iop, &uc, T, result);
                } else {
                    CachedReg& t = alloc_store_reg(iop, &uc, T);

                    switch (ins.id) {
                        // ADDI and ADDIU use a sign-extended immediate
                        case IOP_I_ADDI:
                        case IOP_I_ADDIU: uc.add(t.reg, s.reg, Imm(ins.imm16s)); break;

                        case IOP_I_ANDI: uc.and_(t.reg, s.reg, Imm(ins.imm16)); break;
                        case IOP_I_ORI: uc.or_(t.reg, s.reg, Imm(ins.imm16)); break;
                        case IOP_I_XORI: uc.xor_(t.reg, s.reg, Imm(ins.imm16)); break;
                    }
                }
            } break;

            case IOP_I_ADD:
            case IOP_I_ADDU:
            case IOP_I_SUB:
            case IOP_I_SUBU: {
                if (!D) break;

                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (s.constant && t.constant) {
                    uint32_t result = 0;

                    switch (ins.id) {
                        case IOP_I_ADD:
                        case IOP_I_ADDU: result = s.value + t.value; break;
                        case IOP_I_SUB:
                        case IOP_I_SUBU: result = s.value - t.value; break;
                    }

                    alloc_const_reg(iop, &uc, D, result);
                } else if (s.constant) {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    ujit::Gp tmp = uc.new_gp32();

                    switch (ins.id) {
                        case IOP_I_ADD:
                        case IOP_I_ADDU: {
                            uc.add(tmp, t.reg, Imm(s.value));
                        } break;

                        case IOP_I_SUB:
                        case IOP_I_SUBU: {
                            uc.mov(tmp, Imm(s.value));
                            uc.sub(tmp, tmp, t.reg);
                        } break;
                    }

                    uc.mov(d.reg, tmp);
                } else if (t.constant) {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    ujit::Gp tmp = uc.new_gp32();

                    switch (ins.id) {
                        case IOP_I_ADD:
                        case IOP_I_ADDU: {
                            uc.add(tmp, s.reg, Imm(t.value));
                        } break;

                        case IOP_I_SUB:
                        case IOP_I_SUBU: {
                            uc.sub(tmp, s.reg, Imm(t.value));
                        } break;
                    }

                    uc.mov(d.reg, tmp);
                } else {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    ujit::Gp tmp = uc.new_gp32();

                    switch (ins.id) {
                        case IOP_I_ADD:
                        case IOP_I_ADDU: uc.add(tmp, s.reg, t.reg); break;
                        case IOP_I_SUB:
                        case IOP_I_SUBU: uc.sub(tmp, s.reg, t.reg); break;
                    }

                    uc.mov(d.reg, tmp);
                }
            } break;

            case IOP_I_SLT:
            case IOP_I_SLTU: {
                if (!D) break;

                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (s.constant && t.constant) {
                    uint32_t result = 0;

                    switch (ins.id) {
                        case IOP_I_SLT: result = (int32_t)s.value < (int32_t)t.value; break;
                        case IOP_I_SLTU: result = s.value < t.value; break;
                    }

                    alloc_const_reg(iop, &uc, D, result);
                } else if (s.constant) {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm(s.value));

                    switch (ins.id) {
                        case IOP_I_SLT: uc.select(d.reg, Imm(1), Imm(0), ujit::scmp_lt(tmp, t.reg)); break;
                        case IOP_I_SLTU: uc.select(d.reg, Imm(1), Imm(0), ujit::ucmp_lt(tmp, t.reg)); break;
                    }
                } else if (t.constant) {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    switch (ins.id) {
                        case IOP_I_SLT: uc.select(d.reg, Imm(1), Imm(0), ujit::scmp_lt(s.reg, Imm(t.value))); break;
                        case IOP_I_SLTU: uc.select(d.reg, Imm(1), Imm(0), ujit::ucmp_lt(s.reg, Imm(t.value))); break;
                    }
                } else {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    switch (ins.id) {
                        case IOP_I_SLT: uc.select(d.reg, Imm(1), Imm(0), ujit::scmp_lt(s.reg, t.reg)); break;
                        case IOP_I_SLTU: uc.select(d.reg, Imm(1), Imm(0), ujit::ucmp_lt(s.reg, t.reg)); break;
                    }
                }
            } break;

            case IOP_I_SLTI:
            case IOP_I_SLTIU: {
                if (!T) break;

                CachedReg& s = alloc_load_reg(iop, &uc, S);

                if (s.constant) {
                    uint32_t result = 0;

                    switch (ins.id) {
                        case IOP_I_SLTI: result = (int32_t)s.value < (int32_t)ins.imm16s; break;
                        case IOP_I_SLTIU: result = s.value < (uint32_t)ins.imm16s; break;
                    }

                    alloc_const_reg(iop, &uc, T, result);
                } else {
                    CachedReg& t = alloc_store_reg(iop, &uc, T);

                    switch (ins.id) {
                        case IOP_I_SLTI: uc.select(t.reg, Imm(1), Imm(0), ujit::scmp_lt(s.reg, Imm(ins.imm16s))); break;
                        case IOP_I_SLTIU: uc.select(t.reg, Imm(1), Imm(0), ujit::ucmp_lt(s.reg, Imm(ins.imm16s))); break;
                    }
                }
            } break;

            case IOP_I_AND:
            case IOP_I_OR:
            case IOP_I_XOR:
            case IOP_I_NOR: {
                if (!D) break;

                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (s.constant && t.constant) {
                    uint32_t result = 0;

                    switch (ins.id) {
                        case IOP_I_AND: result = s.value & t.value; break;
                        case IOP_I_OR: result = s.value | t.value; break;
                        case IOP_I_XOR: result = s.value ^ t.value; break;
                        case IOP_I_NOR: result = ~(s.value | t.value); break;
                    }

                    alloc_const_reg(iop, &uc, D, result);
                } else if (s.constant) {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    switch (ins.id) {
                        case IOP_I_AND: uc.and_(d.reg, t.reg, Imm(s.value)); break;
                        case IOP_I_OR: uc.or_(d.reg, t.reg, Imm(s.value)); break;
                        case IOP_I_XOR: uc.xor_(d.reg, t.reg, Imm(s.value)); break;
                        case IOP_I_NOR: {
                            uc.or_(d.reg, t.reg, Imm(s.value));
                            uc.not_(d.reg, d.reg);
                        } break;
                    }
                } else if (t.constant) {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    switch (ins.id) {
                        case IOP_I_AND: uc.and_(d.reg, s.reg, Imm(t.value)); break;
                        case IOP_I_OR: uc.or_(d.reg, s.reg, Imm(t.value)); break;
                        case IOP_I_XOR: uc.xor_(d.reg, s.reg, Imm(t.value)); break;
                        case IOP_I_NOR: {
                            uc.or_(d.reg, s.reg, Imm(t.value));
                            uc.not_(d.reg, d.reg);
                        } break;
                    }
                } else {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    ujit::Gp tmp = uc.new_gp32();

                    switch (ins.id) {
                        case IOP_I_AND: uc.and_(tmp, s.reg, t.reg); break;
                        case IOP_I_OR: uc.or_(tmp, s.reg, t.reg); break;
                        case IOP_I_XOR: uc.xor_(tmp, s.reg, t.reg); break;
                        case IOP_I_NOR: {
                            uc.or_(tmp, s.reg, t.reg);
                            uc.not_(tmp, tmp);
                        } break;
                    }

                    // Workaround for a bug in asmjit
                    uc.mov(d.reg, tmp);
                }
            } break;

            case IOP_I_J: {
                uint32_t slot;

                int module = get_module_for_address(iop, block->end_pc - 4, &slot);

                ujit::Gp tmp = uc.new_gp32();

                if (!module) {
                    uc.load_u32(tmp, IOP(next_pc));
                    uc.and_(tmp, tmp, Imm(0xf0000000));
                    uc.or_(tmp, tmp, Imm(ins.imm26 << 2));
                    uc.store_u32(IOP(next_pc), tmp);

                    break;
                }

                switch (module) {
                    case MODULE_IOMAN: {
                        jit_function_call(&uc, delegate_ioman, tmp, iop->ptr, Imm(slot), Imm(0));
                    } break;

                    case MODULE_IOMANX: {
                        jit_function_call(&uc, delegate_ioman, tmp, iop->ptr, Imm(slot), Imm(1));
                    } break;

                    case MODULE_LOADCORE: {
                        jit_function_call(&uc, delegate_loadcore, tmp, iop->ptr, Imm(slot));
                    } break;

                    case MODULE_SYSMEM: {
                        jit_function_call(&uc, delegate_sysmem, tmp, iop->ptr, Imm(slot));
                    } break;
                }

                asmjit::Label skip = uc.new_label();

                uc.j(skip, ujit::test_nz(tmp));

                uc.load_u32(tmp, IOP(next_pc));
                uc.and_(tmp, tmp, Imm(0xf0000000));
                uc.or_(tmp, tmp, Imm(ins.imm26 << 2));
                uc.store_u32(IOP(next_pc), tmp);

                uc.bind(skip);
            } break;

            case IOP_I_JR: {
                if (!S) {
                    uc.store_zero_u32(IOP(next_pc));

                    break;
                }

                CachedReg& s = alloc_load_reg(iop, &uc, S);

                ujit::Gp tmp = s.reg;

                if (s.constant) {
                    tmp = uc.new_gp32();

                    uc.mov(tmp, Imm(s.value));
                }

                uc.store_u32(IOP(next_pc), tmp);
            } break;

            case IOP_I_JAL: {
                alloc_const_reg(iop, &uc, 31, block->end_pc);

                ujit::Gp tmp = uc.new_gp32();

                uc.mov(tmp, Imm((block->end_pc & 0xf0000000) | (ins.imm26 << 2)));
                uc.store_u32(IOP(next_pc), tmp);
            } break;

            case IOP_I_JALR: {
                CachedReg& s = alloc_load_reg(iop, &uc, S);

                if (s.constant) {
                    if (D) alloc_const_reg(iop, &uc, D, block->end_pc);

                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm(s.value));
                    uc.store_u32(IOP(next_pc), tmp);

                    break;
                }

                uc.store_u32(IOP(next_pc), s.reg);

                if (D) alloc_const_reg(iop, &uc, D, block->end_pc);
            } break;

            case IOP_I_BEQ: {
                ujit::Gp tgt = uc.new_gp32();
                asmjit::Label skip = uc.new_label();

                if (S == T) {
                    uc.load_u32(tgt, IOP(pc));
                    uc.add(tgt, tgt, Imm(ins.imm16s << 2));
                    uc.store_u32(IOP(next_pc), tgt);

                    break;
                }

                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (s.constant && t.constant) {
                    if (s.value != t.value)
                        break;
                } else if (s.constant) {
                    uc.j(skip, ujit::cmp_ne(t.reg, Imm(s.value)));
                } else if (t.constant) {
                    uc.j(skip, ujit::cmp_ne(s.reg, Imm(t.value)));
                } else {
                    uc.j(skip, ujit::cmp_ne(s.reg, t.reg));
                }

                uc.load_u32(tgt, IOP(pc));
                uc.add(tgt, tgt, Imm(ins.imm16s << 2));
                uc.store_u32(IOP(next_pc), tgt);

                uc.bind(skip);
            } break;

            case IOP_I_BNE: {
                // RS != RT with S = T never happens
                if (S == T)
                    break;

                ujit::Gp tgt = uc.new_gp32();
                asmjit::Label skip = uc.new_label();

                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (s.constant && t.constant) {
                    if (s.value == t.value)
                        break;
                } else if (s.constant) {
                    uc.j(skip, ujit::cmp_eq(t.reg, Imm(s.value)));
                } else if (t.constant) {
                    uc.j(skip, ujit::cmp_eq(s.reg, Imm(t.value)));
                } else {
                    uc.j(skip, ujit::cmp_eq(s.reg, t.reg));
                }

                uc.load_u32(tgt, IOP(pc));
                uc.add(tgt, tgt, Imm(ins.imm16s << 2));
                uc.store_u32(IOP(next_pc), tgt);

                uc.bind(skip);
            } break;

            case IOP_I_BLTZ:
            case IOP_I_BLEZ:
            case IOP_I_BGTZ:
            case IOP_I_BGEZ: {
                CachedReg& s = alloc_load_reg(iop, &uc, S);
                asmjit::Label skip = uc.new_label();

                if (s.constant) {
                    bool cond = false;

                    switch (ins.id) {
                        case IOP_I_BLTZ: cond = (int32_t)s.value < (int32_t)0; break;
                        case IOP_I_BLEZ: cond = (int32_t)s.value <= (int32_t)0; break;
                        case IOP_I_BGTZ: cond = (int32_t)s.value > (int32_t)0; break;
                        case IOP_I_BGEZ: cond = (int32_t)s.value >= (int32_t)0; break;
                    }

                    if (!cond) break;
                } else {
                    switch (ins.id) {
                        case IOP_I_BLTZ: uc.j(skip, ujit::scmp_ge(s.reg, Imm(0))); break;
                        case IOP_I_BLEZ: uc.j(skip, ujit::scmp_gt(s.reg, Imm(0))); break;
                        case IOP_I_BGTZ: uc.j(skip, ujit::scmp_le(s.reg, Imm(0))); break;
                        case IOP_I_BGEZ: uc.j(skip, ujit::scmp_lt(s.reg, Imm(0))); break;
                    }
                }

                ujit::Gp tgt = uc.new_gp32();

                uc.load_u32(tgt, IOP(pc));
                uc.add(tgt, tgt, Imm(ins.imm16s << 2));
                uc.store_u32(IOP(next_pc), tgt);

                uc.bind(skip);
            } break;

            case IOP_I_BLTZAL:
            case IOP_I_BGEZAL: {
                CachedReg s = alloc_load_reg(iop, &uc, S);
                asmjit::Label skip = uc.new_label();

                if (s.constant) {
                    alloc_const_reg(iop, &uc, 31, block->end_pc);

                    bool cond = false;

                    switch (ins.id) {
                        case IOP_I_BLTZAL: cond = (int32_t)s.value < (int32_t)0; break;
                        case IOP_I_BGEZAL: cond = (int32_t)s.value >= (int32_t)0; break;
                    }

                    if (!cond) break;
                } else {
                    switch (ins.id) {
                        case IOP_I_BLTZAL: uc.j(skip, ujit::scmp_ge(s.reg, Imm(0))); break;
                        case IOP_I_BGEZAL: uc.j(skip, ujit::scmp_lt(s.reg, Imm(0))); break;
                    }
                }

                ujit::Gp tgt = uc.new_gp32();

                uc.load_u32(tgt, IOP(pc));
                uc.add(tgt, tgt, Imm(ins.imm16s << 2));
                uc.store_u32(IOP(next_pc), tgt);

                uc.bind(skip);

                alloc_const_reg(iop, &uc, 31, block->end_pc);
            } break;

            case IOP_I_SLL:
            case IOP_I_SRL:
            case IOP_I_SRA: {
                if (!D) break;

                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (t.constant) {
                    uint32_t result = 0;

                    switch (ins.id) {
                        case IOP_I_SLL: result = t.value << ins.imm5; break;
                        case IOP_I_SRL: result = t.value >> ins.imm5; break;
                        case IOP_I_SRA: result = (int32_t)t.value >> ins.imm5; break;
                    }

                    alloc_const_reg(iop, &uc, D, result);
                } else {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    switch (ins.id) {
                        case IOP_I_SLL: uc.shl(d.reg, t.reg, Imm(ins.imm5)); break;
                        case IOP_I_SRL: uc.shr(d.reg, t.reg, Imm(ins.imm5)); break;
                        case IOP_I_SRA: uc.sar(d.reg, t.reg, Imm(ins.imm5)); break;
                    }
                }
            } break;

            case IOP_I_SLLV:
            case IOP_I_SRLV:
            case IOP_I_SRAV: {
                if (!D) break;

                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (s.constant && t.constant) {
                    uint32_t result = 0;

                    switch (ins.id) {
                        case IOP_I_SLLV: result = t.value << (s.value & 0x1f); break;
                        case IOP_I_SRLV: result = t.value >> (s.value & 0x1f); break;
                        case IOP_I_SRAV: result = (int32_t)t.value >> (s.value & 0x1f); break;
                    }

                    alloc_const_reg(iop, &uc, D, result);
                } else if (s.constant) {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);

                    ujit::Gp tmp = uc.new_gp32();

                    switch (ins.id) {
                        case IOP_I_SLLV: uc.shl(tmp, t.reg, Imm(s.value & 0x1f)); break;
                        case IOP_I_SRLV: uc.shr(tmp, t.reg, Imm(s.value & 0x1f)); break;
                        case IOP_I_SRAV: uc.sar(tmp, t.reg, Imm(s.value & 0x1f)); break;
                    }

                    uc.mov(d.reg, tmp);
                } else if (t.constant) {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);
                    ujit::Gp shift = uc.new_gp32();

                    ujit::Gp tmp = uc.new_gp32();
                    uc.mov(tmp, Imm(t.value));
                    uc.and_(shift, s.reg, Imm(0x1f));

                    switch (ins.id) {
                        case IOP_I_SLLV: uc.shl(tmp, tmp, shift); break;
                        case IOP_I_SRLV: uc.shr(tmp, tmp, shift); break;
                        case IOP_I_SRAV: uc.sar(tmp, tmp, shift); break;
                    }

                    uc.mov(d.reg, tmp);
                } else {
                    CachedReg& d = alloc_store_reg(iop, &uc, D);
                    ujit::Gp shift = uc.new_gp32();
                    ujit::Gp tmp = uc.new_gp32();

                    uc.and_(shift, s.reg, Imm(0x1f));

                    switch (ins.id) {
                        case IOP_I_SLLV: uc.shl(tmp, t.reg, shift); break;
                        case IOP_I_SRLV: uc.shr(tmp, t.reg, shift); break;
                        case IOP_I_SRAV: uc.sar(tmp, t.reg, shift); break;
                    }

                    uc.mov(d.reg, tmp);
                }
            } break;

            case IOP_I_MTC0: {
                if (!T) {
                    uc.store_zero_u32(IOP(cop0_r[ins.rd]));

                    break;
                }

                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (t.constant) {
                    ujit::Gp tmp = uc.new_gp32();
                    
                    uc.mov(tmp, Imm(t.value));
                    uc.store_u32(IOP(cop0_r[ins.rd]), tmp);
                } else {
                    uc.store_u32(IOP(cop0_r[ins.rd]), t.reg);
                }
            } break;

            case IOP_I_MFC0: {
                if (!T) break;

                CachedReg& t = alloc_store_reg(iop, &uc, T);

                uc.load_u32(t.reg, IOP(cop0_r[ins.rd]));
            } break;

            case IOP_I_SYSCALL: {
                jit_function_call(&uc, exception, iop->ptr, Imm(ins.id == IOP_I_SYSCALL ? CAUSE_SYSCALL : CAUSE_BP));
            } break;

            case IOP_I_BREAK: { // Do nothing
                continue;
            } break;

            case IOP_I_RFE: {
                ujit::Gp sr = uc.new_gp32();
                ujit::Gp mode = uc.new_gp32();

                uc.load_u32(sr, IOP(cop0_r[COP0_SR]));
                uc.mov(mode, sr);
                uc.and_(mode, mode, Imm(0x3f));
                uc.shr(mode, mode, Imm(2));
                uc.and_(sr, sr, Imm(0xfffffff0));
                uc.or_(sr, sr, mode);
                uc.store_u32(IOP(cop0_r[COP0_SR]), sr);
            } break;

            case IOP_I_MFHI:
            case IOP_I_MFLO: {
                if (!D) break;

                CachedReg& d = alloc_store_reg(iop, &uc, D);

                if (ins.id == IOP_I_MFHI) {
                    uc.load_u32(d.reg, IOP(hi));
                } else {
                    uc.load_u32(d.reg, IOP(lo));
                }
            } break;

            case IOP_I_MTHI:
            case IOP_I_MTLO: {
                if (!S) {
                    if (ins.id == IOP_I_MTHI) {
                        uc.store_zero_u32(IOP(hi));
                    } else {
                        uc.store_zero_u32(IOP(lo));
                    }

                    break;
                }

                CachedReg& s = alloc_load_reg(iop, &uc, S);

                if (s.constant) {
                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm(s.value));

                    if (ins.id == IOP_I_MTHI) {
                        uc.store_u32(IOP(hi), tmp);
                    } else {
                        uc.store_u32(IOP(lo), tmp);
                    }
                } else {
                    if (ins.id == IOP_I_MTHI) {
                        uc.store_u32(IOP(hi), s.reg);
                    } else {
                        uc.store_u32(IOP(lo), s.reg);
                    }
                }
            } break;

            case IOP_I_MULT:
            case IOP_I_MULTU: {
                // const int64_t s = (int64_t)((int32_t)iop->r[S]);
                // const int64_t t = (int64_t)((int32_t)iop->r[T]);

                // const uint64_t r = s * t;

                // iop->hi = r >> 32;
                // iop->lo = r & 0xffffffff;

                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                const bool is_signed = ins.id == IOP_I_MULT;

                if (s.constant && t.constant) {
                    uint64_t r = is_signed
                        ? (uint64_t)((int64_t)(int32_t)s.value * (int64_t)(int32_t)t.value)
                        : (uint64_t)(uint32_t)s.value * (uint64_t)(uint32_t)t.value;

                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((uint32_t)r));
                    uc.store_u32(IOP(lo), tmp);
                    uc.mov(tmp, Imm((uint32_t)(r >> 32)));
                    uc.store_u32(IOP(hi), tmp);
                } else {
                    ujit::Gp a = mul_load_operand(uc, s, is_signed);
                    ujit::Gp b = mul_load_operand(uc, t, is_signed);
                    ujit::Gp r = uc.new_gp64();

                    uc.mul(r, a, b);

                    uc.store_u32(IOP(lo), r.r32());
                    uc.shr(r, r, Imm(32));
                    uc.store_u32(IOP(hi), r.r32());
                }
            } break;

            case IOP_I_DIV:
            case IOP_I_DIVU: {
                CachedReg& s = alloc_load_reg(iop, &uc, S);
                CachedReg& t = alloc_load_reg(iop, &uc, T);

                if (s.constant && t.constant) {
                    uint32_t lo, hi;

                    if (ins.id == IOP_I_DIV) {
                        int32_t sv = (int32_t)s.value;
                        int32_t tv = (int32_t)t.value;

                        if (!tv) {
                            hi = (uint32_t)sv;
                            lo = (sv >= 0) ? 0xffffffff : 1;
                        } else if (((uint32_t)sv == 0x80000000) && (tv == -1)) {
                            hi = 0;
                            lo = 0x80000000;
                        } else {
                            hi = (uint32_t)(sv % tv);
                            lo = (uint32_t)(sv / tv);
                        }
                    } else {
                        if (!t.value) {
                            hi = s.value;
                            lo = 0xffffffff;
                        } else {
                            hi = s.value % t.value;
                            lo = s.value / t.value;
                        }
                    }

                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm(lo));
                    uc.store_u32(IOP(lo), tmp);
                    uc.mov(tmp, Imm(hi));
                    uc.store_u32(IOP(hi), tmp);
                } else {
                    ujit::Gp sr = value_to_gp32(uc, s);
                    ujit::Gp tr = value_to_gp32(uc, t);

                    ujit::Gp lo = uc.new_gp32();
                    ujit::Gp hi = uc.new_gp32();

                    if (ins.id == IOP_I_DIVU) {
                        ujit::Gp td = uc.new_gp32();
                        uc.select(td, Imm(1), tr, ujit::cmp_eq(tr, Imm(0)));

                        ujit::Gp q = uc.new_gp32();
                        ujit::Gp r = uc.new_gp32();
                        uc.udiv(q, sr, td);
                        uc.umod(r, sr, td);

                        uc.select(lo, Imm(0xffffffff), q, ujit::cmp_eq(tr, Imm(0)));
                        uc.select(hi, sr, r, ujit::cmp_eq(tr, Imm(0)));
                    } else {
                        ujit::Gp us = uc.new_gp32();
                        ujit::Gp ut = uc.new_gp32();
                        uc.abs(us, sr);
                        uc.abs(ut, tr);

                        ujit::Gp td = uc.new_gp32();
                        uc.select(td, Imm(1), ut, ujit::cmp_eq(tr, Imm(0)));

                        ujit::Gp uq = uc.new_gp32();
                        ujit::Gp ur = uc.new_gp32();
                        uc.udiv(uq, us, td);
                        uc.umod(ur, us, td);

                        ujit::Gp sxt = uc.new_gp32();
                        ujit::Gp nq = uc.new_gp32();
                        ujit::Gp nr = uc.new_gp32();
                        uc.xor_(sxt, sr, tr);
                        uc.neg(nq, uq);
                        uc.neg(nr, ur);

                        ujit::Gp q = uc.new_gp32();
                        ujit::Gp r = uc.new_gp32();
                        uc.select(q, nq, uq, ujit::scmp_lt(sxt, Imm(0)));
                        uc.select(r, nr, ur, ujit::scmp_lt(sr, Imm(0)));

                        ujit::Gp lo_zero = uc.new_gp32();
                        uc.select(lo_zero, Imm(1), Imm(0xffffffff), ujit::scmp_lt(sr, Imm(0)));

                        uc.select(lo, lo_zero, q, ujit::cmp_eq(tr, Imm(0)));
                        uc.select(hi, sr, r, ujit::cmp_eq(tr, Imm(0)));
                    }

                    uc.store_u32(IOP(lo), lo);
                    uc.store_u32(IOP(hi), hi);
                }
            } break;

            default: {
                flush_reg_cache(iop, &uc);

                jit_function_call(&uc, ins.func, iop->ptr, Imm((uintptr_t)&ins));

                uc.store_zero_u32(IOP(r[0]));
            } break;
        }

        index++;
    }

    uc.bind(block_exit);

    flush_reg_cache(iop, &uc);

    uc.end_func();

    Error err = uc.finalize();

    // if (err != Error::kOk) {
    //     char buf[512];

    //     iop::dis::Dis dis;

    //     dis.hex_memory_offset = 1;
    //     dis.print_address = 1;
    //     dis.print_opcode = 1;

    //     dis.addr = block->start_pc;

    //     for (const Instruction& ins : block->instructions) {
    //         iris_debug(iop, "{}", iop::dis::disassemble(buf, ins.opcode, &dis));
    //         dis.addr += 4;
    //     }

    //     iris_debug(iop, "ee: Failed to finalize JIT compilation {}", err);

    //     exit(1);
    // }

    Error err1 = iop->rt.add(&block->func, &code);

    // if (err1 != Error::kOk) {
    //     iris_debug(iop, "ee: Failed to add JIT code to runtime {}", err1);

    //     exit(1);
    // }

    // if (code.logger()) {
    //     char buf[512];

    //     iop::dis::Dis dis;

    //     dis.hex_memory_offset = 1;
    //     dis.print_address = 1;
    //     dis.print_opcode = 1;

    //     dis.addr = block->start_pc;

    //     for (const Instruction& ins : block->instructions) {
    //         iris_debug(iop, "{}", iop::dis::disassemble(buf, ins.opcode, &dis));
    //         dis.addr += 4;
    //     }
    // }
}

Block* find_block(Iop* iop, uint32_t pc) {
    BlockLutEntry& lut = iop->block_lut[(pc >> 2) & IOP_BLOCK_LUT_MASK];

    if (lut.pc == pc && lut.gen == iop->block_lut_gen) {
        return lut.block;
    }

    uint32_t addr = translate_addr(pc);
    uint32_t page = addr / _IOP_CACHE_PAGESIZE;

    if (!iop->block_cache[page].valid) {
        return nullptr;
    }

    if (iop->block_cache[page].dirty) {
        iop->block_cache[page].blocks = nullptr;
        iop->block_cache[page].dirty = false;
        iop->block_cache[page].valid = false;
        iop->block_cache[page].min_code_addr = 0xffffffff;
        iop->block_cache[page].max_code_addr = 0;

        iop->block_lut_gen++;

        return nullptr;
    }

    uint32_t offset = (addr & (_IOP_CACHE_PAGESIZE - 1)) >> 2;

    Block* block = &iop->block_cache[page].blocks[offset];

    if (!block->cycles) {
        return nullptr;
    }

    lut.pc = pc;
    lut.gen = iop->block_lut_gen;
    lut.block = block;

    return block;
}

Block* cache_block(Iop* iop, uint32_t addr, int max_cycles) {
    uint32_t translated = translate_addr(addr);

    uint32_t page = translated / _IOP_CACHE_PAGESIZE;
    uint32_t offset = (translated & (_IOP_CACHE_PAGESIZE - 1)) >> 2;

    if (!iop->block_cache[page].valid) {
        void* blk = arena::alloc(iop->block_arena, sizeof(Block) * (_IOP_CACHE_PAGESIZE / 4));

        if (!blk) {
            for (int i = 0; i < IOP_CACHE_PAGECOUNT; i++) {
                iop->block_cache[i].valid = false;
                iop->block_cache[i].blocks = nullptr;
            }

            iop->last_cached_block = nullptr;
            iop->last_cached_block_pc = 0;
            iop->block_lut_gen++;

            blk = arena::alloc(iop->block_arena, sizeof(Block) * (_IOP_CACHE_PAGESIZE / 4));
        }

        memset(blk, 0, sizeof(Block) * (_IOP_CACHE_PAGESIZE / 4));

        iop->block_cache[page].blocks = (Block*)blk;
        iop->block_cache[page].dirty = false;
        iop->block_cache[page].valid = true;
        iop->block_cache[page].min_code_addr = translated;
        iop->block_cache[page].max_code_addr = translated;
    }

    Block& block = iop->block_cache[page].blocks[offset];

    if (translated < iop->block_cache[page].min_code_addr) {
        iop->block_cache[page].min_code_addr = translated;
    }

    Instruction i;

    block.start_pc = addr;
    block.end_pc = addr;

    block.cycles = 0;

    iop->instruction_buf_index = 0;

    // iris_debug(iop, "Caching block at {:08x}", block_pc);

    while (max_cycles) {
        uint32_t opcode = bus_read32(iop, block.end_pc);

        i = decode(opcode);

        iop->instruction_buf[iop->instruction_buf_index++] = i;

        block.cycles += 1;

        if (i.branch == 1) {
            max_cycles = 2;
        } else if (i.branch == 2) {
            max_cycles = 1;
        }

        max_cycles--;

        block.end_pc += 4;
    }

    uint32_t translated_end = translated + (block.end_pc - addr);

    if (iop->block_cache[page].max_code_addr < translated_end) {
        iop->block_cache[page].max_code_addr = translated_end;
    }

    // iris_debug(iop, "Caching block at pc={:08x} page={} min={:08x} max={:08x} max_cycles={}", addr, page, iop->block_cache[page].min_code_addr, iop->block_cache[page].max_code_addr, max_cycles);

    return &block;
}

int execute_block(Iop* iop, Block* block) {
    iop->delay_slot = 0;
    iop->branch = 0;

    if (check_irq(iop)) {
        exception(iop, CAUSE_INT);

        return 0;
    }

    iop->next_pc = block->end_pc;
    iop->pc = iop->next_pc - 4;

    block->func(iop);

    iop->total_cycles += block->cycles;
    iop->pc = iop->next_pc;

    return block->cycles;
}

int run_block(Iop* iop, int max_cycles) {
    if (max_cycles <= 0) {
        return 0;
    }

    // iris_debug(iop, "Running block at pc={:08x} max_cycles={}", iop->pc, max_cycles);
    int cycles = 0;

    while (cycles < max_cycles) {
        Block* block = find_block(iop, iop->pc);

        if (!block) {
            block = cache_block(iop, iop->pc, max_cycles);

            compile_block(iop, block);
        }

        cycles += execute_block(iop, block);
    }

    // if (iop->deferred_invalidate_page != 0xffffffff) {
    //     uint32_t page = iop->deferred_invalidate_page;

    //     if (iop->block_cache[page]) {
    //         delete[] iop->block_cache[page];
    //         iop->block_cache[page] = nullptr;
    //     }

    //     if (iop->last_cached_block && ((iop->last_cached_block_pc / _IOP_CACHE_PAGESIZE) == page)) {
    //         iop->last_cached_block = nullptr;
    //     }

    //     iop->deferred_invalidate_page = 0xffffffff;
    // }

    return cycles;
}

void flush_cache(Iop* iop) {
    for (CachePage& page : iop->block_cache) {
        page.dirty = true;
    }

    iop->last_cached_block = nullptr;
    iop->last_cached_block_pc = 0;
    iop->executing_cache_page = 0xffffffff;
    iop->deferred_invalidate_page = 0xffffffff;
    iop->block_lut_gen++;
}

void invalidate_block(Iop* iop, uint32_t addr) {
    addr = translate_addr(addr);

    uint32_t page = addr / _IOP_CACHE_PAGESIZE;

    if (is_executable_region(addr) && iop->block_cache[page].valid) {
        iop->block_cache[page].dirty = true;
        iop->block_lut_gen++;
    }
}

}
