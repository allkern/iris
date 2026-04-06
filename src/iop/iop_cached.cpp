#include <stdlib.h>
#include <string.h>

#include "iop.h"
#include "iop_def.hpp"
#include "iop_dis.h"

#include "iop_export.h"

#ifndef _IOP_CACHE_PAGESIZE
#define _IOP_CACHE_PAGESIZE 512
#endif

#define IOP_CACHE_PAGECOUNT (0x20000000u / _IOP_CACHE_PAGESIZE)

bool iop_is_executable_region(uint32_t addr) {
    // RAM and BIOS
    return (addr < 0x2000000) || ((addr >= 0x1fc00000) && (addr < 0x20000000));
}

void iop_invalidate_cache_page(struct iop_state* iop, uint32_t addr) {
    uint32_t page = addr / _IOP_CACHE_PAGESIZE;

    if (iop->block_cache[page] && iop_is_executable_region(addr)) {
        iop->block_cache_dirty[page] = 1;
    }
}

#define IOP_INVALIDATE_PAGE(addr) { \
    iop_invalidate_cache_page(iop, addr); \
}

const uint32_t iop_bus_region_mask_table[] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0x7fffffff, 0x1fffffff, 0xffffffff, 0xffffffff
};

static inline uint32_t iop_translate_addr(uint32_t addr) {
    return addr & 0x1fffffff;

    //KSEG0
    if (addr >= 0x80000000 && addr < 0xA0000000)
        return addr - 0x80000000;

    //KSEG1
    if (addr >= 0xA0000000 && addr < 0xC0000000)
        return addr - 0xA0000000;

    //KUSEG, KSEG2
    return addr;
}

static inline uint32_t iop_bus_read8(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read8(iop->bus.udata, iop_translate_addr(addr));
}

static inline uint32_t iop_bus_read16(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read16(iop->bus.udata, iop_translate_addr(addr));
}

static inline uint32_t iop_bus_read32(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read32(iop->bus.udata, iop_translate_addr(addr));
}

static inline void iop_bus_write8(struct iop_state* iop, uint32_t addr, uint32_t data) {
    addr = iop_translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write8(iop->bus.udata, addr, data);
}

static inline void iop_bus_write16(struct iop_state* iop, uint32_t addr, uint32_t data) {
    addr = iop_translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write16(iop->bus.udata, addr, data);
}

static inline void iop_bus_write32(struct iop_state* iop, uint32_t addr, uint32_t data) {
    addr = iop_translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write32(iop->bus.udata, addr, data);
}

// External functions
uint32_t iop_read8(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read8(iop->bus.udata, iop_translate_addr(addr));
}

uint32_t iop_read16(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read16(iop->bus.udata, iop_translate_addr(addr));
}

uint32_t iop_read32(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read32(iop->bus.udata, iop_translate_addr(addr));
}

void iop_write8(struct iop_state* iop, uint32_t addr, uint32_t data) {
    addr = iop_translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write8(iop->bus.udata, iop_translate_addr(addr), data);
}

void iop_write16(struct iop_state* iop, uint32_t addr, uint32_t data) {
    addr = iop_translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write16(iop->bus.udata, iop_translate_addr(addr), data);
}

void iop_write32(struct iop_state* iop, uint32_t addr, uint32_t data) {
    addr = iop_translate_addr(addr);

    IOP_INVALIDATE_PAGE(addr);

    iop->bus.write32(iop->bus.udata, iop_translate_addr(addr), data);
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

#define BRANCH(offset) { \
    iop->next_pc = iop->next_pc + (offset); \
    iop->next_pc = iop->next_pc - 4; \
    iop->branch = 1; \
    iop->branch_taken = 1; }

struct iop_state* iop_create(void) {
    return new iop_state();
}

void iop_destroy(struct iop_state* iop) {
    iop_flush_cache(iop);

    delete iop;
}

void iop_init(struct iop_state* iop, struct iop_bus_s bus) {
    iop_reset(iop);

    iop->bus = bus;
    iop->pc = 0xbfc00000;
    iop->next_pc = iop->pc + 4;

    iop->cop0_r[COP0_SR] = 0x10900000;
    iop->cop0_r[COP0_PRID] = 0x0000001f;

    iop->block_cache.resize(IOP_CACHE_PAGECOUNT);
    iop->block_cache_dirty.resize(IOP_CACHE_PAGECOUNT);
}

void iop_init_kputchar(struct iop_state* iop, void (*kputchar)(void*, char), void* udata) {
    iop->kputchar = kputchar;
    iop->kputchar_udata = udata;
}

void iop_init_sm_putchar(struct iop_state* iop, void (*sm_putchar)(void*, char), void* udata) {
    iop->sm_putchar = sm_putchar;
    iop->sm_putchar_udata = udata;
}

static inline int iop_check_irq(struct iop_state* iop) {
    return (iop->cop0_r[COP0_SR] & SR_IEC) &&
           (iop->cop0_r[COP0_SR] & iop->cop0_r[COP0_CAUSE] & 0x00000400);
}

static inline void iop_print_disassembly(uint32_t pc, uint32_t opcode) {
    char buf[128];
    struct iop_dis_state state;

    state.print_address = 1;
    state.print_opcode = 1;
    state.addr = pc;

    puts(iop_disassemble(buf, opcode, &state));
}

static inline void iop_exception(struct iop_state* iop, uint32_t cause) {
    if ((cause != CAUSE_SYSCALL) && (cause != CAUSE_INT))
        printf("iop: Crashed with cause %02x at pc=%08x next=%08x saved=%08x\n", cause >> 2, iop->pc, iop->saved_pc, iop->saved_pc);

    // Set excode and clear 3 LSBs
    iop->cop0_r[COP0_CAUSE] &= 0xffffff80;
    iop->cop0_r[COP0_CAUSE] |= cause;

    // printf("iop: Exception with cause %02x at pc=%08x next=%08x saved=%08x\n", cause >> 2, iop->pc, iop->next_pc, iop->saved_pc);

    iop->cop0_r[COP0_EPC] = iop->saved_pc;

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

// void iop_cycle(struct iop_state* iop) {
//     iop->saved_pc = iop->pc;
//     iop->delay_slot = iop->branch;
//     iop->branch = 0;
//     iop->branch_taken = 0;

//     iop->opcode = iop_bus_read32(iop, iop->pc);

//     iop->pc = iop->next_pc;
//     iop->next_pc += 4;

//     if (iop_check_irq(iop)) {
//         iop_exception(iop, CAUSE_INT);

//         return;
//     }

//     iop_execute(iop);

//     iop->last_cycles += 1;
//     iop->total_cycles += 1;

//     iop->r[0] = 0;
// }

void iop_reset(struct iop_state* iop) {
    iop_flush_cache(iop);

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

void iop_set_irq_pending(struct iop_state* iop, int value) {
    if (value) {
        iop->cop0_r[COP0_CAUSE] |= SR_IM2;
    } else {
        iop->cop0_r[COP0_CAUSE] &= ~SR_IM2;
    }
}

static inline void iop_i_invalid(struct iop_state* iop, iop_instruction& ins) {
    fprintf(stderr, "%08x: Illegal instruction %08x", iop->pc - 8, ins.opcode);

    exit(1);

    iop_exception(iop, CAUSE_RI);
}

static inline void iop_i_bltz(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;

    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    if ((int32_t)s < (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bgez(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;

    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    if ((int32_t)s >= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bltzal(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;

    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    R_RA = iop->next_pc;

    if ((int32_t)s < (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bgezal(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;

    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    R_RA = iop->next_pc;

    if ((int32_t)s >= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_j(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;

    DO_PENDING_LOAD;

    // If we get a 1 that means the call has been HLE'd
    if (iop_test_module_hooks(iop))
        return;

    iop->next_pc = (iop->next_pc & 0xf0000000) | (IMM26 << 2);
}

static inline void iop_i_jal(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;

    DO_PENDING_LOAD;

    R_RA = iop->next_pc;

    iop->next_pc = (iop->next_pc & 0xf0000000) | (IMM26 << 2);
}

static inline void iop_i_beq(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;
    iop->branch_taken = 0;

    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    if (s == t)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bne(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;
    iop->branch_taken = 0;

    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    if (s != t)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_blez(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;
    iop->branch_taken = 0;

    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    if ((int32_t)s <= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bgtz(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;
    iop->branch_taken = 0;

    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    if ((int32_t)s > (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_addi(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    uint32_t i = IMM16S;
    uint32_t r = s + i;
    uint32_t o = (s ^ r) & (i ^ r);

    if (o & 0x80000000) {
        iop_exception(iop, CAUSE_OV);
    } else {
        iop->r[T] = r;
    }
}

static inline void iop_i_addiu(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s + IMM16S;
}

static inline void iop_i_slti(struct iop_state* iop, iop_instruction& ins) {
    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s < IMM16S;
}

static inline void iop_i_sltiu(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s < IMM16S;
}

static inline void iop_i_andi(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s & IMM16;
}

static inline void iop_i_ori(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s | IMM16;
}

static inline void iop_i_xori(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s ^ IMM16;
}

static inline void iop_i_lui(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    iop->r[T] = IMM16 << 16;
}

static inline void iop_i_lb(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    if (iop->load_d != T)
        DO_PENDING_LOAD;

    iop->load_d = T;
    iop->load_v = SE8(iop_bus_read8(iop, s + IMM16S));
}

static inline void iop_i_lh(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    if (iop->load_d != T)
        DO_PENDING_LOAD;

    uint32_t addr = s + IMM16S;

    if (addr & 0x1) {
        iop_exception(iop, CAUSE_ADEL);
    } else {
        iop->load_d = T;
        iop->load_v = SE16(iop_bus_read16(iop, addr));
    }
}

static inline void iop_i_lwl(struct iop_state* iop, iop_instruction& ins) {
    uint32_t rt = T;
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[rt];

    uint32_t addr = s + IMM16S;
    uint32_t load = iop_bus_read32(iop, addr & 0xfffffffc);

    if (rt == iop->load_d) {
        t = iop->load_v;
    } else {
        DO_PENDING_LOAD;
    }

    int shift = (int)((addr & 0x3) << 3);
    uint32_t mask = (uint32_t)0x00FFFFFF >> shift;
    uint32_t value = (t & mask) | (load << (24 - shift)); 

    iop->load_d = rt;
    iop->load_v = value;

    // printf("lwl rt=%u s=%08x t=%08x addr=%08x load=%08x (%08x) shift=%u mask=%08x value=%08x\n",
    //     rt, s, t, addr, load, addr & 0xfffffffc, shift, mask, value
    // );
}

static inline void iop_i_lw(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t addr = s + IMM16S;

    if (iop->load_d != T)
        DO_PENDING_LOAD;

    if (addr & 0x3) {
        iop_exception(iop, CAUSE_ADEL);
    } else {
        iop->load_d = T;
        iop->load_v = iop_bus_read32(iop, addr);
    }
}

static inline void iop_i_lbu(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    if (iop->load_d != T)
        DO_PENDING_LOAD;

    iop->load_d = T;
    iop->load_v = iop_bus_read8(iop, s + IMM16S);
}

static inline void iop_i_lhu(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t addr = s + IMM16S;

    if (iop->load_d != T)
        DO_PENDING_LOAD;

    if (addr & 0x1) {
        iop_exception(iop, CAUSE_ADEL);
    } else {
        iop->load_d = T;
        iop->load_v = iop_bus_read16(iop, addr);
    }
}

static inline void iop_i_lwr(struct iop_state* iop, iop_instruction& ins) {
    uint32_t rt = T;
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[rt];

    uint32_t addr = s + IMM16S;
    uint32_t load = iop_bus_read32(iop, addr & 0xfffffffc);

    if (rt == iop->load_d) {
        t = iop->load_v;
    } else {
        DO_PENDING_LOAD;
    }

    int shift = (int)((addr & 0x3) << 3);
    uint32_t mask = 0xFFFFFF00 << (24 - shift);
    uint32_t value = (t & mask) | (load >> shift); 

    iop->load_d = rt;
    iop->load_v = value;

    // printf("lwr rt=%u s=%08x t=%08x addr=%08x load=%08x (%08x) shift=%u mask=%08x value=%08x\n",
    //     rt, s, t, addr, load, addr & 0xfffffffc, shift, mask, value
    // );
}

static inline void iop_i_sb(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    // Cache isolated
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    iop_bus_write8(iop, s + IMM16S, t);
}

static inline void iop_i_sh(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];
    uint32_t addr = s + IMM16S;

    DO_PENDING_LOAD;

    // Cache isolated
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    if (addr & 0x1) {
        iop_exception(iop, CAUSE_ADES);
    } else {
        iop_bus_write16(iop, addr, t);
    }
}

static inline void iop_i_swl(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    uint32_t addr = s + IMM16S;
    uint32_t aligned = addr & 0xfffffffc;
    uint32_t v = iop_bus_read32(iop, aligned);

    switch (addr & 0x3) {
        case 0: v = (v & 0xffffff00) | (iop->r[T] >> 24); break;
        case 1: v = (v & 0xffff0000) | (iop->r[T] >> 16); break;
        case 2: v = (v & 0xff000000) | (iop->r[T] >> 8 ); break;
        case 3: v =                     iop->r[T]       ; break;
    }

    iop_bus_write32(iop, aligned, v);
}

static inline void iop_i_sw(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];
    uint32_t addr = s + IMM16S;

    DO_PENDING_LOAD;

    // Cache isolated
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    if (addr & 0x3) {
        iop_exception(iop, CAUSE_ADES);
    } else {
        if (addr == 0xfffe0130) {
            iop->biu_config = t;

            return;
        }

        iop_bus_write32(iop, addr, t);
    }
}

static inline void iop_i_swr(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    uint32_t addr = s + IMM16S;
    uint32_t aligned = addr & 0xfffffffc;
    uint32_t v = iop_bus_read32(iop, aligned);

    switch (addr & 0x3) {
        case 0: v =                     iop->r[T]       ; break;
        case 1: v = (v & 0x000000ff) | (iop->r[T] << 8 ); break;
        case 2: v = (v & 0x0000ffff) | (iop->r[T] << 16); break;
        case 3: v = (v & 0x00ffffff) | (iop->r[T] << 24); break;
    }

    iop_bus_write32(iop, aligned, v);
}

static inline void iop_i_lwc0(struct iop_state* iop, iop_instruction& ins) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_lwc1(struct iop_state* iop, iop_instruction& ins) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_lwc2(struct iop_state* iop, iop_instruction& ins) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_lwc3(struct iop_state* iop, iop_instruction& ins) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_swc0(struct iop_state* iop, iop_instruction& ins) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_swc1(struct iop_state* iop, iop_instruction& ins) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_swc2(struct iop_state* iop, iop_instruction& ins) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_swc3(struct iop_state* iop, iop_instruction& ins) {
    iop_exception(iop, CAUSE_CPU);
}

// Secondary
static inline void iop_i_sll(struct iop_state* iop, iop_instruction& ins) {
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t << IMM5;
}

static inline void iop_i_srl(struct iop_state* iop, iop_instruction& ins) {
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t >> IMM5;
}

static inline void iop_i_sra(struct iop_state* iop, iop_instruction& ins) {
    int32_t t = (int32_t)iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t >> IMM5;
}

static inline void iop_i_sllv(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t << (s & 0x1f);
}

static inline void iop_i_srlv(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t >> (s & 0x1f);
}

static inline void iop_i_srav(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    int32_t t = (int32_t)iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t >> (s & 0x1f);
}

static inline void iop_i_jr(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;

    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->next_pc = s;
}

static inline void iop_i_jalr(struct iop_state* iop, iop_instruction& ins) {
    iop->branch = 1;

    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[D] = iop->next_pc;

    iop->next_pc = s;
}

static inline void iop_i_syscall(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    iop_exception(iop, CAUSE_SYSCALL);
}

static inline void iop_i_break(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    printf("iop: Crashed at pc=%08x next=%08x saved=%08x\n", iop->pc, iop->next_pc, iop->saved_pc);

    exit(1);

    // iop_exception(iop, CAUSE_BP);
}

static inline void iop_i_mfhi(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    iop->r[D] = iop->hi;
}

static inline void iop_i_mthi(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    iop->hi = iop->r[S];
}

static inline void iop_i_mflo(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    iop->r[D] = iop->lo;
}

static inline void iop_i_mtlo(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    iop->lo = iop->r[S];
}

static inline void iop_i_mult(struct iop_state* iop, iop_instruction& ins) {
    int64_t s = (int64_t)((int32_t)iop->r[S]);
    int64_t t = (int64_t)((int32_t)iop->r[T]);

    DO_PENDING_LOAD;

    uint64_t r = s * t;

    iop->hi = r >> 32;
    iop->lo = r & 0xffffffff;
}

static inline void iop_i_multu(struct iop_state* iop, iop_instruction& ins) {
    uint64_t s = (uint64_t)iop->r[S];
    uint64_t t = (uint64_t)iop->r[T];

    DO_PENDING_LOAD;

    uint64_t r = s * t;

    iop->hi = r >> 32;
    iop->lo = r & 0xffffffff;
}

static inline void iop_i_div(struct iop_state* iop, iop_instruction& ins) {
    int32_t s = (int32_t)iop->r[S];
    int32_t t = (int32_t)iop->r[T];

    DO_PENDING_LOAD;

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

static inline void iop_i_divu(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    if (!t) {
        iop->hi = s;
        iop->lo = 0xffffffff;
    } else {
        iop->hi = s % t;
        iop->lo = s / t;
    }
}

static inline void iop_i_add(struct iop_state* iop, iop_instruction& ins) {
    int32_t s = iop->r[S];
    int32_t t = iop->r[T];

    DO_PENDING_LOAD;

    int32_t r = s + t;
    uint32_t o = (s ^ r) & (t ^ r);

    if (o & 0x80000000) {
        iop_exception(iop, CAUSE_OV);
    } else {
        iop->r[D] = (uint32_t)r;
    }
}

static inline void iop_i_addu(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s + t;
}

static inline void iop_i_sub(struct iop_state* iop, iop_instruction& ins) {
    int32_t s = (int32_t)iop->r[S];
    int32_t t = (int32_t)iop->r[T];
    int32_t r;

    DO_PENDING_LOAD;

    int o = __builtin_ssub_overflow(s, t, &r);

    if (o) {
        iop_exception(iop, CAUSE_OV);
    } else {
        iop->r[D] = r;
    }
}

static inline void iop_i_subu(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s - t;
}

static inline void iop_i_and(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s & t;
}

static inline void iop_i_or(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s | t;
}

static inline void iop_i_xor(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = (s ^ t);
}

static inline void iop_i_nor(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = ~(s | t);
}

static inline void iop_i_slt(struct iop_state* iop, iop_instruction& ins) {
    int32_t s = (int32_t)iop->r[S];
    int32_t t = (int32_t)iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s < t;
}

static inline void iop_i_sltu(struct iop_state* iop, iop_instruction& ins) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s < t;
}

// COP0
static inline void iop_i_mfc0(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    iop->load_v = iop->cop0_r[D];
    iop->load_d = T;

    // printf("mfc0 rt=%u rd=%u value=%08x\n", T, D, iop->cop0_r[D]);
}

static inline void iop_i_mtc0(struct iop_state* iop, iop_instruction& ins) {
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->cop0_r[D] = t & g_iop_cop0_write_mask_table[D];
}

static inline void iop_i_rfe(struct iop_state* iop, iop_instruction& ins) {
    DO_PENDING_LOAD;

    uint32_t mode = iop->cop0_r[COP0_SR] & 0x3f;

    iop->cop0_r[COP0_SR] &= 0xfffffff0;
    iop->cop0_r[COP0_SR] |= mode >> 2;
}

iop_instruction iop_decode(uint32_t opcode) {
    iop_instruction i = { 0 };

    i.opcode = opcode;
    i.rs = (opcode >> 21) & 0x1f;
    i.rt = (opcode >> 16) & 0x1f;
    i.rd = (opcode >> 11) & 0x1f;
    i.imm5 = (opcode >> 6) & 0x1f;
    i.imm16 = opcode & 0xffff;
    i.imm16s = (int32_t)(int16_t)i.imm16;
    i.imm26 = opcode & 0x3ffffff;
    i.branch = 0;

    switch ((opcode & 0xfc000000) >> 26) {
        case 0x00000000 >> 26: {
            switch (opcode & 0x0000003f) {
                case 0x00000000: i.id = IOP_I_SLL; i.func = iop_i_sll; return i;
                case 0x00000002: i.id = IOP_I_SRL; i.func = iop_i_srl; return i;
                case 0x00000003: i.id = IOP_I_SRA; i.func = iop_i_sra; return i;
                case 0x00000004: i.id = IOP_I_SLLV; i.func = iop_i_sllv; return i;
                case 0x00000006: i.id = IOP_I_SRLV; i.func = iop_i_srlv; return i;
                case 0x00000007: i.id = IOP_I_SRAV; i.func = iop_i_srav; return i;
                case 0x00000008: i.branch = 1; i.id = IOP_I_JR; i.func = iop_i_jr; return i;
                case 0x00000009: i.branch = 1; i.id = IOP_I_JALR; i.func = iop_i_jalr; return i;
                case 0x0000000c: i.branch = 2; i.id = IOP_I_SYSCALL; i.func = iop_i_syscall; return i;
                case 0x0000000d: i.branch = 2; i.id = IOP_I_BREAK; i.func = iop_i_break; return i;
                case 0x00000010: i.id = IOP_I_MFHI; i.func = iop_i_mfhi; return i;
                case 0x00000011: i.id = IOP_I_MTHI; i.func = iop_i_mthi; return i;
                case 0x00000012: i.id = IOP_I_MFLO; i.func = iop_i_mflo; return i;
                case 0x00000013: i.id = IOP_I_MTLO; i.func = iop_i_mtlo; return i;
                case 0x00000018: i.id = IOP_I_MULT; i.func = iop_i_mult; return i;
                case 0x00000019: i.id = IOP_I_MULTU; i.func = iop_i_multu; return i;
                case 0x0000001a: i.id = IOP_I_DIV; i.func = iop_i_div; return i;
                case 0x0000001b: i.id = IOP_I_DIVU; i.func = iop_i_divu; return i;
                case 0x00000020: i.id = IOP_I_ADD; i.func = iop_i_add; return i;
                case 0x00000021: i.id = IOP_I_ADDU; i.func = iop_i_addu; return i;
                case 0x00000022: i.id = IOP_I_SUB; i.func = iop_i_sub; return i;
                case 0x00000023: i.id = IOP_I_SUBU; i.func = iop_i_subu; return i;
                case 0x00000024: i.id = IOP_I_AND; i.func = iop_i_and; return i;
                case 0x00000025: i.id = IOP_I_OR; i.func = iop_i_or; return i;
                case 0x00000026: i.id = IOP_I_XOR; i.func = iop_i_xor; return i;
                case 0x00000027: i.id = IOP_I_NOR; i.func = iop_i_nor; return i;
                case 0x0000002a: i.id = IOP_I_SLT; i.func = iop_i_slt; return i;
                case 0x0000002b: i.id = IOP_I_SLTU; i.func = iop_i_sltu; return i;
            } break;
        } break;
        case 0x04000000 >> 26: {
            switch ((opcode & 0x001f0000) >> 16) {
                case 0x00000000 >> 16: i.branch = 1; i.id = IOP_I_BLTZ; i.func = iop_i_bltz; return i;
                case 0x00010000 >> 16: i.branch = 1; i.id = IOP_I_BGEZ; i.func = iop_i_bgez; return i;
                case 0x00100000 >> 16: i.branch = 1; i.id = IOP_I_BLTZAL; i.func = iop_i_bltzal; return i;
                case 0x00110000 >> 16: i.branch = 1; i.id = IOP_I_BGEZAL; i.func = iop_i_bgezal; return i;
                // bltz/bgez dupes
                default: {
                    if (opcode & 0x00010000) {
                        i.branch = 1; i.id = IOP_I_BGEZ; i.func = iop_i_bgez;
                    } else {
                        i.branch = 1; i.id = IOP_I_BLTZ; i.func = iop_i_bltz;
                    }

                    return i;
                }
            } break;
        } break;
        case 0x08000000 >> 26: i.branch = 1; i.id = IOP_I_J; i.func = iop_i_j; return i;
        case 0x0c000000 >> 26: i.branch = 1; i.id = IOP_I_JAL; i.func = iop_i_jal; return i;
        case 0x10000000 >> 26: i.branch = 1; i.id = IOP_I_BEQ; i.func = iop_i_beq; return i;
        case 0x14000000 >> 26: i.branch = 1; i.id = IOP_I_BNE; i.func = iop_i_bne; return i;
        case 0x18000000 >> 26: i.branch = 1; i.id = IOP_I_BLEZ; i.func = iop_i_blez; return i;
        case 0x1c000000 >> 26: i.branch = 1; i.id = IOP_I_BGTZ; i.func = iop_i_bgtz; return i;
        case 0x20000000 >> 26: i.id = IOP_I_ADDI; i.func = iop_i_addi; return i;
        case 0x24000000 >> 26: i.id = IOP_I_ADDIU; i.func = iop_i_addiu; return i;
        case 0x28000000 >> 26: i.id = IOP_I_SLTI; i.func = iop_i_slti; return i;
        case 0x2c000000 >> 26: i.id = IOP_I_SLTIU; i.func = iop_i_sltiu; return i;
        case 0x30000000 >> 26: i.id = IOP_I_ANDI; i.func = iop_i_andi; return i;
        case 0x34000000 >> 26: i.id = IOP_I_ORI; i.func = iop_i_ori; return i;
        case 0x38000000 >> 26: i.id = IOP_I_XORI; i.func = iop_i_xori; return i;
        case 0x3c000000 >> 26: i.id = IOP_I_LUI; i.func = iop_i_lui; return i;
        case 0x40000000 >> 26: {
            switch ((opcode & 0x03e00000) >> 21) {
                case 0x00000000 >> 21: i.id = IOP_I_MFC0; i.func = iop_i_mfc0; return i;
                case 0x00800000 >> 21: i.id = IOP_I_MTC0; i.func = iop_i_mtc0; return i;
                case 0x02000000 >> 21: i.id = IOP_I_RFE; i.func = iop_i_rfe; return i;
            }
        } break;
        case 0x48000000 >> 26: i.id = IOP_I_INVALID; i.func = iop_i_invalid; return i;
        case 0x80000000 >> 26: i.id = IOP_I_LB; i.func = iop_i_lb; return i;
        case 0x84000000 >> 26: i.id = IOP_I_LH; i.func = iop_i_lh; return i;
        case 0x88000000 >> 26: i.id = IOP_I_LWL; i.func = iop_i_lwl; return i;
        case 0x8c000000 >> 26: i.id = IOP_I_LW; i.func = iop_i_lw; return i;
        case 0x90000000 >> 26: i.id = IOP_I_LBU; i.func = iop_i_lbu; return i;
        case 0x94000000 >> 26: i.id = IOP_I_LHU; i.func = iop_i_lhu; return i;
        case 0x98000000 >> 26: i.id = IOP_I_LWR; i.func = iop_i_lwr; return i;
        case 0xa0000000 >> 26: i.id = IOP_I_SB; i.func = iop_i_sb; return i;
        case 0xa4000000 >> 26: i.id = IOP_I_SH; i.func = iop_i_sh; return i;
        case 0xa8000000 >> 26: i.id = IOP_I_SWL; i.func = iop_i_swl; return i;
        case 0xac000000 >> 26: i.id = IOP_I_SW; i.func = iop_i_sw; return i;
        case 0xb8000000 >> 26: i.id = IOP_I_SWR; i.func = iop_i_swr; return i;
        case 0xc0000000 >> 26: i.id = IOP_I_LWC0; i.func = iop_i_lwc0; return i;
        case 0xc4000000 >> 26: i.id = IOP_I_LWC1; i.func = iop_i_lwc1; return i;
        case 0xc8000000 >> 26: i.id = IOP_I_LWC2; i.func = iop_i_lwc2; return i;
        case 0xcc000000 >> 26: i.id = IOP_I_LWC3; i.func = iop_i_lwc3; return i;
        case 0xe0000000 >> 26: i.id = IOP_I_SWC0; i.func = iop_i_swc0; return i;
        case 0xe4000000 >> 26: i.id = IOP_I_SWC1; i.func = iop_i_swc1; return i;
        case 0xe8000000 >> 26: i.id = IOP_I_SWC2; i.func = iop_i_swc2; return i;
        case 0xec000000 >> 26: i.id = IOP_I_SWC3; i.func = iop_i_swc3; return i;
    }

    i.func = iop_i_invalid;

    return i;
}

iop_block* iop_find_block(struct iop_state* iop, uint32_t pc) {
    uint32_t addr = iop_translate_addr(pc);
    uint32_t page = addr / _IOP_CACHE_PAGESIZE;

    if (!iop->block_cache[page]) {
        return nullptr;
    }

    if (iop->block_cache_dirty[page]) {
        delete[] iop->block_cache[page];

        iop->block_cache[page] = nullptr;
        iop->block_cache_dirty[page] = 0;

        if (iop->last_cached_block && ((iop->last_cached_block_pc / _IOP_CACHE_PAGESIZE) == page)) {
            iop->last_cached_block = nullptr;
        }

        return nullptr;
    }

    uint32_t offset = (addr & (_IOP_CACHE_PAGESIZE - 1)) >> 2;

    iop_block* block = &iop->block_cache[page][offset];

    if (!block->valid) {
        return nullptr;
    }

    iop->last_cached_block = block;
    iop->last_cached_block_pc = pc;

    return block;
}

iop_block* iop_cache_block(struct iop_state* iop, uint32_t addr, int max_cycles) {
    uint32_t translated = iop_translate_addr(addr);

    uint32_t page = translated / _IOP_CACHE_PAGESIZE;
    uint32_t offset = (translated & (_IOP_CACHE_PAGESIZE - 1)) >> 2;

    if (!iop->block_cache[page]) {
        iop->block_cache[page] = new iop_block[_IOP_CACHE_PAGESIZE >> 2];
        iop->block_cache_dirty[page] = 0;
    }

    struct iop_block& block = iop->block_cache[page][offset];

    uint32_t pc = addr;
    uint32_t block_pc = addr;

    iop_instruction i;

    block.cycles = 0;
    block.instructions.reserve(max_cycles);

    // fprintf(stderr, "Caching block at %08x\n", block_pc);

    while (max_cycles) {
        uint32_t opcode = iop_bus_read32(iop, pc);

        i = iop_decode(opcode);

        block.instructions.push_back(i);
        block.cycles += 1;

        if (i.branch == 1) {
            max_cycles = 2;
        } else if (i.branch == 2) {
            max_cycles = 1;
        }

        max_cycles--;

        pc += 4;
    }

    return &block;
}

int iop_execute_block(struct iop_state* iop, iop_block* block) {
    iop->delay_slot = 0;
    iop->branch = 0;

    if (iop_check_irq(iop)) {
        iop->saved_pc = iop->pc;

        iop_exception(iop, CAUSE_INT);

        return 0;
    }

    int cycles = 0;

    for (iop_instruction& ins : block->instructions) {
        iop->delay_slot = iop->branch;
        iop->branch = 0;
        iop->saved_pc = iop->pc;

        // iop_print_disassembly(iop->pc, ins.opcode);

        iop->pc = iop->next_pc;
        iop->next_pc += 4;

        // if (iop_check_irq(iop)) {
        //     iop_exception(iop, CAUSE_INT);

        //     return cycles;
        // }

        ins.func(iop, ins);

        cycles++;

        iop->total_cycles++;

        iop->r[0] = 0;
    }

    return cycles;
}

int iop_run_block(struct iop_state* iop, int max_cycles) {
    if (max_cycles <= 0) {
        return 0;
    }

    // printf("iop: Running block at pc=%08x max_cycles=%d\n", iop->pc, max_cycles);

    iop_block* block = iop_find_block(iop, iop->pc);

    if (!block) {
        block = iop_cache_block(iop, iop->pc, max_cycles);
    }

    int executed = iop_execute_block(iop, block);

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

    return executed;
}

void iop_flush_cache(struct iop_state* iop) {
    for (iop_block*& block : iop->block_cache) {
        if (!block) {
            continue;
        }

        delete[] block;
        block = nullptr;
    }

    iop->last_cached_block = nullptr;
    iop->last_cached_block_pc = 0;
    iop->executing_cache_page = 0xffffffff;
    iop->deferred_invalidate_page = 0xffffffff;
}