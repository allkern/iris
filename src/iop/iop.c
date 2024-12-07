#include <stdlib.h>
#include <string.h>

#include "iop.h"
#include "iop_dis.h"

static int p = 0;

const uint32_t iop_bus_region_mask_table[] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0x7fffffff, 0x1fffffff, 0xffffffff, 0xffffffff
};

static inline uint32_t iop_bus_read8(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read8(iop->bus.udata, addr & iop_bus_region_mask_table[addr >> 29]);
}

static inline uint32_t iop_bus_read16(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read16(iop->bus.udata, addr & iop_bus_region_mask_table[addr >> 29]);
}

static inline uint32_t iop_bus_read32(struct iop_state* iop, uint32_t addr) {
    return iop->bus.read32(iop->bus.udata, addr & iop_bus_region_mask_table[addr >> 29]);
}

static inline void iop_bus_write8(struct iop_state* iop, uint32_t addr, uint32_t data) {
    iop->bus.write8(iop->bus.udata, addr & iop_bus_region_mask_table[addr >> 29], data);
}

static inline void iop_bus_write16(struct iop_state* iop, uint32_t addr, uint32_t data) {
    iop->bus.write16(iop->bus.udata, addr & iop_bus_region_mask_table[addr >> 29], data);
}

static inline void iop_bus_write32(struct iop_state* iop, uint32_t addr, uint32_t data) {
    iop->bus.write32(iop->bus.udata, addr & iop_bus_region_mask_table[addr >> 29], data);
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

#define OP ((iop->opcode >> 26) & 0x3f)
#define S ((iop->opcode >> 21) & 0x1f)
#define T ((iop->opcode >> 16) & 0x1f)
#define D ((iop->opcode >> 11) & 0x1f)
#define IMM5 ((iop->opcode >> 6) & 0x1f)
#define CMT ((iop->opcode >> 6) & 0xfffff)
#define SOP (iop->opcode & 0x3f)
#define IMM26 (iop->opcode & 0x3ffffff)
#define IMM16 (iop->opcode & 0xffff)
#define IMM16S ((int32_t)((int16_t)IMM16))

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
    return (struct iop_state*)malloc(sizeof(struct iop_state));
}

void iop_destroy(struct iop_state* iop) {
    free(iop);
}

// static FILE* file;

// int qty = 1000000;
// int loop = 0;

void iop_init(struct iop_state* iop, struct iop_bus_s bus) {
    memset(iop, 0, sizeof(struct iop_state));

    iop->bus = bus;
    iop->pc = 0xbfc00000;
    iop->next_pc = iop->pc + 4;

    iop->cop0_r[COP0_SR] = 0x10900000;
    iop->cop0_r[COP0_PRID] = 0x0000001f;
}

void iop_init_kputchar(struct iop_state* iop, void (*kputchar)(void*, char), void* udata) {
    iop->kputchar = kputchar;
    iop->kputchar_udata = udata;
}

static inline int iop_check_irq(struct iop_state* iop) {
    return (iop->cop0_r[COP0_SR] & SR_IEC) &&
           (iop->cop0_r[COP0_SR] & iop->cop0_r[COP0_CAUSE] & 0x00000400);
}

static inline void iop_print_disassembly(struct iop_state* iop) {
    char buf[128];
    struct iop_dis_state state;

    state.print_address = 1;
    state.print_opcode = 1;
    state.addr = iop->pc;

    puts(iop_disassemble(buf, iop->opcode, &state));
}

static inline void iop_exception(struct iop_state* iop, uint32_t cause) {
    // Set excode and clear 3 LSBs
    iop->cop0_r[COP0_CAUSE] &= 0xffffff80;
    iop->cop0_r[COP0_CAUSE] |= cause;

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

void iop_cycle(struct iop_state* iop) {
    iop->last_cycles = 0;

    iop->saved_pc = iop->pc;
    iop->delay_slot = iop->branch;
    iop->branch = 0;
    iop->branch_taken = 0;

    if (iop->saved_pc & 3)
        iop_exception(iop, CAUSE_ADEL);

    iop->opcode = iop_bus_read32(iop, iop->pc);
    iop->last_cycles = 0;

    uint32_t pc = iop->next_pc;

    if ((pc == 0x12C48) || (pc == 0x1420C) || (pc == 0x1430C)) {
        uint32_t ptr = iop->r[5];
        uint32_t size = iop->r[6];

        while (size--) {
            iop->kputchar(iop->kputchar_udata, iop_bus_read8(iop, ptr++));
        }
    }

    if (iop->p) {
        iop_print_disassembly(iop);

        --iop->p;
    }

    iop->pc = iop->next_pc;
    iop->next_pc += 4;

    if (iop_check_irq(iop)) {
        iop->r[0] = 0;

        // printf("iop: irq pc=%08x next_pc=%08x saved_pc=%08x\n", iop->pc, iop->next_pc, iop->saved_pc);

        iop_exception(iop, CAUSE_INT);

        return;
    }

    int cyc = iop_execute(iop);

    if (!cyc) {
        printf("iop: Illegal instruction %08x at %08x (next=%08x, saved=%08x)\n", iop->opcode, iop->pc, iop->next_pc, iop->saved_pc);

        iop_exception(iop, CAUSE_RI);
    }

    iop->last_cycles += cyc;
    iop->total_cycles += iop->last_cycles;

    iop->r[0] = 0;
}

void iop_reset(struct iop_state* iop) {
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
}

void iop_set_irq_pending(struct iop_state* iop) {
    iop->cop0_r[COP0_CAUSE] |= SR_IM2;
}

static inline void iop_i_invalid(struct iop_state* iop) {
    printf("%08x: Illegal instruction %08x", iop->pc - 8, iop->opcode);

    iop_exception(iop, CAUSE_RI);
}

static inline void iop_i_bltz(struct iop_state* iop) {
    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    if ((int32_t)s < (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bgez(struct iop_state* iop) {
    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    if ((int32_t)s >= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bltzal(struct iop_state* iop) {
    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    R_RA = iop->next_pc;

    if ((int32_t)s < (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bgezal(struct iop_state* iop) {
    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    R_RA = iop->next_pc;

    if ((int32_t)s >= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_j(struct iop_state* iop) {
    iop->branch = 1;

    DO_PENDING_LOAD;

    iop->next_pc = (iop->next_pc & 0xf0000000) | (IMM26 << 2);

    // if ((iop->next_pc & 0xFFFF) == 0x1EC8 || (iop->next_pc & 0xFFFF) == 0x1F64) {
    //     uint32_t ptr = iop->r[4];
    //     uint16_t version = iop_bus_read16(iop, ptr + 8);

    //     char name[9];

    //     for (int i = 0; i < 8; i++)
    //         name[i] = iop_bus_read8(iop, ptr + 12 + i);

    //     name[8] = 0;

    //     printf("iop: Register library %s version %d.0%d\n", name, version >> 8, version & 0xff);
    // }
}

static inline void iop_i_jal(struct iop_state* iop) {
    iop->branch = 1;

    DO_PENDING_LOAD;

    R_RA = iop->next_pc;

    iop->next_pc = (iop->next_pc & 0xf0000000) | (IMM26 << 2);
}

static inline void iop_i_beq(struct iop_state* iop) {
    iop->branch = 1;
    iop->branch_taken = 0;

    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    if (s == t)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bne(struct iop_state* iop) {
    iop->branch = 1;
    iop->branch_taken = 0;

    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    if (s != t)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_blez(struct iop_state* iop) {
    iop->branch = 1;
    iop->branch_taken = 0;

    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    if ((int32_t)s <= (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_bgtz(struct iop_state* iop) {
    iop->branch = 1;
    iop->branch_taken = 0;

    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    if ((int32_t)s > (int32_t)0)
        BRANCH(IMM16S << 2);
}

static inline void iop_i_addi(struct iop_state* iop) {
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

static inline void iop_i_addiu(struct iop_state* iop) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s + IMM16S;
}

static inline void iop_i_slti(struct iop_state* iop) {
    int32_t s = (int32_t)iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s < IMM16S;
}

static inline void iop_i_sltiu(struct iop_state* iop) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s < IMM16S;
}

static inline void iop_i_andi(struct iop_state* iop) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s & IMM16;
}

static inline void iop_i_ori(struct iop_state* iop) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s | IMM16;
}

static inline void iop_i_xori(struct iop_state* iop) {
    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[T] = s ^ IMM16;
}

static inline void iop_i_lui(struct iop_state* iop) {
    DO_PENDING_LOAD;

    iop->r[T] = IMM16 << 16;
}

static inline void iop_i_lb(struct iop_state* iop) {
    uint32_t s = iop->r[S];

    if (iop->load_d != T)
        DO_PENDING_LOAD;

    iop->load_d = T;
    iop->load_v = SE8(iop_bus_read8(iop, s + IMM16S));
}

static inline void iop_i_lh(struct iop_state* iop) {
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

static inline void iop_i_lwl(struct iop_state* iop) {
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

static inline void iop_i_lw(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t addr = s + IMM16S;

    if (iop->load_d != T)
        DO_PENDING_LOAD;

    if (addr & 0x3) {
        iop_exception(iop, CAUSE_ADEL);
    } else {
        if (addr == 0xfffe0130) {
            iop->load_v = 0xffffffff;
            iop->load_d = T;

            return;
        }

        iop->load_d = T;
        iop->load_v = iop_bus_read32(iop, addr);
    }
}

static inline void iop_i_lbu(struct iop_state* iop) {
    uint32_t s = iop->r[S];

    if (iop->load_d != T)
        DO_PENDING_LOAD;

    iop->load_d = T;
    iop->load_v = iop_bus_read8(iop, s + IMM16S);
}

static inline void iop_i_lhu(struct iop_state* iop) {
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

static inline void iop_i_lwr(struct iop_state* iop) {
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

static inline void iop_i_sb(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    // Cache isolated
    if (iop->cop0_r[COP0_SR] & SR_ISC) {
        return;
    }

    iop_bus_write8(iop, s + IMM16S, t);
}

static inline void iop_i_sh(struct iop_state* iop) {
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

static inline void iop_i_swl(struct iop_state* iop) {
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

static inline void iop_i_sw(struct iop_state* iop) {
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

static inline void iop_i_swr(struct iop_state* iop) {
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

static inline void iop_i_lwc0(struct iop_state* iop) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_lwc1(struct iop_state* iop) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_lwc2(struct iop_state* iop) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_lwc3(struct iop_state* iop) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_swc0(struct iop_state* iop) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_swc1(struct iop_state* iop) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_swc2(struct iop_state* iop) {
    iop_exception(iop, CAUSE_CPU);
}

static inline void iop_i_swc3(struct iop_state* iop) {
    iop_exception(iop, CAUSE_CPU);
}

// Secondary
static inline void iop_i_sll(struct iop_state* iop) {
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t << IMM5;
}

static inline void iop_i_srl(struct iop_state* iop) {
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t >> IMM5;
}

static inline void iop_i_sra(struct iop_state* iop) {
    int32_t t = (int32_t)iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t >> IMM5;
}

static inline void iop_i_sllv(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t << (s & 0x1f);
}

static inline void iop_i_srlv(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t >> (s & 0x1f);
}

static inline void iop_i_srav(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    int32_t t = (int32_t)iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = t >> (s & 0x1f);
}

static inline void iop_i_jr(struct iop_state* iop) {
    iop->branch = 1;

    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->next_pc = s;
}

static inline void iop_i_jalr(struct iop_state* iop) {
    iop->branch = 1;

    uint32_t s = iop->r[S];

    DO_PENDING_LOAD;

    iop->r[D] = iop->next_pc;

    iop->next_pc = s;
}

static inline void iop_i_syscall(struct iop_state* iop) {
    DO_PENDING_LOAD;

    iop_exception(iop, CAUSE_SYSCALL);
}

static inline void iop_i_break(struct iop_state* iop) {
    DO_PENDING_LOAD;

    iop_exception(iop, CAUSE_BP);
}

static inline void iop_i_mfhi(struct iop_state* iop) {
    DO_PENDING_LOAD;

    iop->r[D] = iop->hi;
}

static inline void iop_i_mthi(struct iop_state* iop) {
    DO_PENDING_LOAD;

    iop->hi = iop->r[S];
}

static inline void iop_i_mflo(struct iop_state* iop) {
    DO_PENDING_LOAD;

    iop->r[D] = iop->lo;
}

static inline void iop_i_mtlo(struct iop_state* iop) {
    DO_PENDING_LOAD;

    iop->lo = iop->r[S];
}

static inline void iop_i_mult(struct iop_state* iop) {
    int64_t s = (int64_t)((int32_t)iop->r[S]);
    int64_t t = (int64_t)((int32_t)iop->r[T]);

    DO_PENDING_LOAD;

    uint64_t r = s * t;

    iop->hi = r >> 32;
    iop->lo = r & 0xffffffff;
}

static inline void iop_i_multu(struct iop_state* iop) {
    uint64_t s = (uint64_t)iop->r[S];
    uint64_t t = (uint64_t)iop->r[T];

    DO_PENDING_LOAD;

    uint64_t r = s * t;

    iop->hi = r >> 32;
    iop->lo = r & 0xffffffff;
}

static inline void iop_i_div(struct iop_state* iop) {
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

static inline void iop_i_divu(struct iop_state* iop) {
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

static inline void iop_i_add(struct iop_state* iop) {
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

static inline void iop_i_addu(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s + t;
}

static inline void iop_i_sub(struct iop_state* iop) {
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

static inline void iop_i_subu(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s - t;
}

static inline void iop_i_and(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s & t;
}

static inline void iop_i_or(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s | t;
}

static inline void iop_i_xor(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = (s ^ t);
}

static inline void iop_i_nor(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = ~(s | t);
}

static inline void iop_i_slt(struct iop_state* iop) {
    int32_t s = (int32_t)iop->r[S];
    int32_t t = (int32_t)iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s < t;
}

static inline void iop_i_sltu(struct iop_state* iop) {
    uint32_t s = iop->r[S];
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->r[D] = s < t;
}

// COP0
static inline void iop_i_mfc0(struct iop_state* iop) {
    DO_PENDING_LOAD;

    iop->load_v = iop->cop0_r[D];
    iop->load_d = T;
}

static inline void iop_i_mtc0(struct iop_state* iop) {
    uint32_t t = iop->r[T];

    DO_PENDING_LOAD;

    iop->cop0_r[D] = t & g_iop_cop0_write_mask_table[D];
}

static inline void iop_i_rfe(struct iop_state* iop) {
    DO_PENDING_LOAD;

    uint32_t mode = iop->cop0_r[COP0_SR] & 0x3f;

    iop->cop0_r[COP0_SR] &= 0xfffffff0;
    iop->cop0_r[COP0_SR] |= mode >> 2;
}

int iop_execute(struct iop_state* iop) {
    switch ((iop->opcode & 0xfc000000) >> 26) {
        case 0x00000000 >> 26: {
            switch (iop->opcode & 0x0000003f) {
                case 0x00000000: iop_i_sll(iop); return 2;
                case 0x00000002: iop_i_srl(iop); return 2;
                case 0x00000003: iop_i_sra(iop); return 2;
                case 0x00000004: iop_i_sllv(iop); return 2;
                case 0x00000006: iop_i_srlv(iop); return 2;
                case 0x00000007: iop_i_srav(iop); return 2;
                case 0x00000008: iop_i_jr(iop); return 2;
                case 0x00000009: iop_i_jalr(iop); return 2;
                case 0x0000000c: iop_i_syscall(iop); return 2;
                case 0x0000000d: iop_i_break(iop); return 2;
                case 0x00000010: iop_i_mfhi(iop); return 2;
                case 0x00000011: iop_i_mthi(iop); return 2;
                case 0x00000012: iop_i_mflo(iop); return 2;
                case 0x00000013: iop_i_mtlo(iop); return 2;
                case 0x00000018: iop_i_mult(iop); return 2;
                case 0x00000019: iop_i_multu(iop); return 2;
                case 0x0000001a: iop_i_div(iop); return 2;
                case 0x0000001b: iop_i_divu(iop); return 2;
                case 0x00000020: iop_i_add(iop); return 2;
                case 0x00000021: iop_i_addu(iop); return 2;
                case 0x00000022: iop_i_sub(iop); return 2;
                case 0x00000023: iop_i_subu(iop); return 2;
                case 0x00000024: iop_i_and(iop); return 2;
                case 0x00000025: iop_i_or(iop); return 2;
                case 0x00000026: iop_i_xor(iop); return 2;
                case 0x00000027: iop_i_nor(iop); return 2;
                case 0x0000002a: iop_i_slt(iop); return 2;
                case 0x0000002b: iop_i_sltu(iop); return 2;
            } break;
        } break;
        case 0x04000000 >> 26: {
            iop->branch = 1;
            iop->branch_taken = 0;

            switch ((iop->opcode & 0x001f0000) >> 16) {
                case 0x00000000 >> 16: iop_i_bltz(iop); return 2;
                case 0x00010000 >> 16: iop_i_bgez(iop); return 2;
                case 0x00100000 >> 16: iop_i_bltzal(iop); return 2;
                case 0x00110000 >> 16: iop_i_bgezal(iop); return 2;
                // bltz/bgez dupes
                default: {
                    if (iop->opcode & 0x00010000) {
                        iop_i_bgez(iop);
                    } else {
                        iop_i_bltz(iop);
                    }
                } return 2;
            } break;
        } break;
        case 0x08000000 >> 26: iop_i_j(iop); return 2;
        case 0x0c000000 >> 26: iop_i_jal(iop); return 2;
        case 0x10000000 >> 26: iop_i_beq(iop); return 2;
        case 0x14000000 >> 26: iop_i_bne(iop); return 2;
        case 0x18000000 >> 26: iop_i_blez(iop); return 2;
        case 0x1c000000 >> 26: iop_i_bgtz(iop); return 2;
        case 0x20000000 >> 26: iop_i_addi(iop); return 2;
        case 0x24000000 >> 26: iop_i_addiu(iop); return 2;
        case 0x28000000 >> 26: iop_i_slti(iop); return 2;
        case 0x2c000000 >> 26: iop_i_sltiu(iop); return 2;
        case 0x30000000 >> 26: iop_i_andi(iop); return 2;
        case 0x34000000 >> 26: iop_i_ori(iop); return 2;
        case 0x38000000 >> 26: iop_i_xori(iop); return 2;
        case 0x3c000000 >> 26: iop_i_lui(iop); return 2;
        case 0x40000000 >> 26: {
            switch ((iop->opcode & 0x03e00000) >> 21) {
                case 0x00000000 >> 21: iop_i_mfc0(iop); return 2;
                case 0x00800000 >> 21: iop_i_mtc0(iop); return 2;
                case 0x02000000 >> 21: iop_i_rfe(iop); return 2;
            }
        } break;
        case 0x48000000 >> 26: iop_i_invalid(iop); return 2;
        case 0x80000000 >> 26: iop_i_lb(iop); return 2;
        case 0x84000000 >> 26: iop_i_lh(iop); return 2;
        case 0x88000000 >> 26: iop_i_lwl(iop); return 2;
        case 0x8c000000 >> 26: iop_i_lw(iop); return 2;
        case 0x90000000 >> 26: iop_i_lbu(iop); return 2;
        case 0x94000000 >> 26: iop_i_lhu(iop); return 2;
        case 0x98000000 >> 26: iop_i_lwr(iop); return 2;
        case 0xa0000000 >> 26: iop_i_sb(iop); return 2;
        case 0xa4000000 >> 26: iop_i_sh(iop); return 2;
        case 0xa8000000 >> 26: iop_i_swl(iop); return 2;
        case 0xac000000 >> 26: iop_i_sw(iop); return 2;
        case 0xb8000000 >> 26: iop_i_swr(iop); return 2;
        case 0xc0000000 >> 26: iop_i_lwc0(iop); return 2;
        case 0xc4000000 >> 26: iop_i_lwc1(iop); return 2;
        case 0xc8000000 >> 26: iop_i_lwc2(iop); return 2;
        case 0xcc000000 >> 26: iop_i_lwc3(iop); return 2;
        case 0xe0000000 >> 26: iop_i_swc0(iop); return 2;
        case 0xe4000000 >> 26: iop_i_swc1(iop); return 2;
        case 0xe8000000 >> 26: iop_i_swc2(iop); return 2;
        case 0xec000000 >> 26: iop_i_swc3(iop); return 2;
    }

    return 0;
}

#undef R_R0
#undef R_A0
#undef R_RA

#undef OP
#undef S
#undef T
#undef D
#undef IMM5
#undef CMT
#undef SOP
#undef IMM26
#undef IMM16
#undef IMM16S

#undef DO_PENDING_LOAD

#undef DEBUG_ALL

#undef SE8
#undef SE16