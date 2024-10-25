#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ee.h"
#include "ee_dis.h"

#define EE_KUSEG 0
#define EE_KSEG0 1
#define EE_KSEG1 2
#define EE_KSSEG 3
#define EE_KSEG3 4

#define EE_D_RS ((ee->opcode >> 21) & 0x1f)
#define EE_D_RT ((ee->opcode >> 16) & 0x1f)
#define EE_D_RD ((ee->opcode >> 11) & 0x1f)
#define EE_D_SA ((ee->opcode >> 6) & 0x1f)
#define EE_D_I16 (ee->opcode & 0xffff)
#define EE_D_I26 (ee->opcode & 0x3ffffff)
#define EE_D_SI26 ((int32_t)(EE_D_I26 << 6) >> 4)
#define EE_D_SI16 ((int32_t)(EE_D_I16 << 16) >> 14)

#define EE_RT ee->r[EE_D_RT].ul64
#define EE_RD ee->r[EE_D_RD].ul64
#define EE_RS ee->r[EE_D_RS].ul64
#define EE_RT32 ee->r[EE_D_RT].ul32
#define EE_RD32 ee->r[EE_D_RD].ul32
#define EE_RS32 ee->r[EE_D_RS].ul32

#define EE_HI0 ee->hi.u64[0]
#define EE_LO0 ee->lo.u64[0]
#define EE_HI1 ee->hi.u64[1]
#define EE_LO1 ee->lo.u64[1]

#define BRANCH(cond, offset) if (cond) { \
    ee->next_pc = ee->next_pc + (offset); \
    ee->next_pc = ee->next_pc - 4; \
    ee->branch = 1; \
    ee->branch_taken = 1; }

#define BRANCH_LIKELY(cond, offset) \
    BRANCH(cond, offset) else { ee->pc += 4; ee->next_pc += 4; }

#define SE6432(v) ((int64_t)(int32_t)(v))
#define SE6416(v) ((int64_t)(int16_t)(v))
#define SE648(v) ((int64_t)(int8_t)(v))
#define SE3216(v) ((int32_t)(int16_t)(v))

static inline int ee_get_segment(uint32_t virt) {
    switch (virt & 0xe0000000) {
        case 0x00000000: return EE_KUSEG;
        case 0x20000000: return EE_KUSEG;
        case 0x40000000: return EE_KUSEG;
        case 0x60000000: return EE_KUSEG;
        case 0x80000000: return EE_KSEG0;
        case 0xa0000000: return EE_KSEG1;
        case 0xc0000000: return EE_KSSEG;
        case 0xe0000000: return EE_KSEG3;
    }

    return EE_KUSEG;
}

static inline int ee_translate_virt(uint32_t virt, uint32_t* phys) {
    int seg = ee_get_segment(virt);

    if (seg == EE_KSEG0 || seg == EE_KSEG1) {
        *phys = virt & 0x1fffffff;

        return 0;
    }

    // To-do: MMU mapping
    *phys = virt & 0x1fffffff;

    return 0;
}

#define BUS_READ_FUNC(b)                                                        \
    static inline uint64_t bus_read ## b(struct ee_state* ee, uint32_t addr) {  \
        if (addr >= 0x70000000 && addr <= 0x70003fff)                           \
            return ps2_ram_read ## b(ee->scratchpad, addr & 0x3fff);            \
        uint32_t phys;                                                          \
        if (ee_translate_virt(addr, &phys)) {                                   \
            printf("ee: TLB mapping error\n");                                  \
            exit(1);                                                            \
            return 0;                                                           \
        }                                                                       \
        return ee->bus.read ## b(ee->bus.udata, phys);                          \
    }

#define BUS_WRITE_FUNC(b)                                                                   \
    static inline void bus_write ## b(struct ee_state* ee, uint32_t addr, uint64_t data) {  \
        if (addr >= 0x70000000 && addr <= 0x70003fff)                                       \
            { ps2_ram_write ## b(ee->scratchpad, addr & 0x3fff, data); return; }            \
        uint32_t phys;                                                                      \
        if (ee_translate_virt(addr, &phys)) {                                               \
            printf("ee: TLB mapping error\n");                                              \
            exit(1);                                                                        \
            return;                                                                         \
        }                                                                                   \
        ee->bus.write ## b(ee->bus.udata, phys, data);                                      \
    }

BUS_READ_FUNC(8);
BUS_READ_FUNC(16);
BUS_READ_FUNC(32);
BUS_READ_FUNC(64);

static inline uint128_t bus_read128(struct ee_state* ee, uint32_t addr) {
    if (addr >= 0x70000000 && addr <= 0x70003fff)
        return ps2_ram_read128(ee->scratchpad, addr & 0x3fff);

    uint32_t phys;

    if (ee_translate_virt(addr, &phys)) {
        printf("ee: TLB mapping error\n");

        exit(1);

        return (uint128_t)0;
    }

    return ee->bus.read128(ee->bus.udata, phys);
}

BUS_WRITE_FUNC(8);
BUS_WRITE_FUNC(16);
BUS_WRITE_FUNC(32);
BUS_WRITE_FUNC(64);

static inline void bus_write128(struct ee_state* ee, uint32_t addr, uint128_t data) {
    if (addr >= 0x70000000 && addr <= 0x70003fff) {
        ps2_ram_write128(ee->scratchpad, addr & 0x3fff, data);

        return;
    }

    uint32_t phys;

    if (ee_translate_virt(addr, &phys)) {
        printf("ee: TLB mapping error\n");

        exit(1);
    }

    ee->bus.write128(ee->bus.udata, phys, data);
}

#undef BUS_READ_FUNC
#undef BUS_WRITE_FUNC

static inline void ee_set_pc(struct ee_state* ee, uint32_t addr) {
    ee->pc = addr;
    ee->next_pc = addr + 4;
}

static inline void ee_set_pc_delayed(struct ee_state* ee, uint32_t addr) {
    ee->next_pc = addr;
}

static inline void ee_exception_level1(struct ee_state* ee, uint32_t cause) {
    uint32_t vec = EE_VEC_COMMON;

    ee->cause &= ~EE_CAUSE_EXC;
    ee->cause |= cause;

    if (!(ee->status & EE_SR_EXL)) {
        ee->epc = ee->pc - 4;

        if (ee->delay_slot) {
            ee->epc -= 4;
            ee->cause |= EE_CAUSE_BD;
        } else {
            ee->cause &= ~EE_CAUSE_BD;
        }
    }

    ee->status |= EE_SR_EXL;

    if (cause == CAUSE_EXC1_INT)
        vec = EE_VEC_IRQ;

    uint32_t addr = ((ee->status & EE_SR_BEV) ? 0xbfc00200 : 0x80000000) + vec;

    ee_set_pc(ee, addr);
}

static inline void ee_exception_level2(struct ee_state* ee, uint32_t cause) {
    uint32_t vec;

    ee->cause &= ~EE_CAUSE_EXC2;
    ee->cause |= cause;

    ee->errorepc = ee->pc - 4;

    if (ee->delay_slot) {
        ee->errorepc -= 4;
        ee->cause |= EE_CAUSE_BD2;
    } else {
        ee->cause &= ~EE_CAUSE_BD2;
    }

    ee->status |= EE_SR_ERL;

    if ((cause == CAUSE_EXC2_RES) | (cause == CAUSE_EXC2_NMI)) {
        ee_set_pc(ee, EE_VEC_RESET);

        return;
    }

    if (cause == CAUSE_EXC2_PERFC) {
        vec = EE_VEC_COUNTER;
    } else {
        vec = EE_VEC_DEBUG;
    }

    ee_set_pc(ee, ((ee->status & EE_SR_DEV) ? 0xbfc00200 : 0x80000000) + vec);
}

void ee_set_int0(struct ee_state* ee) {
    ee->cause |= EE_CAUSE_IP2;
}

void ee_set_int1(struct ee_state* ee) {
    ee->cause |= EE_CAUSE_IP3;
}

static inline void ee_i_abss(struct ee_state* ee) { printf("ee: abss unimplemented\n"); exit(1); }
static inline void ee_i_add(struct ee_state* ee) { printf("ee: add unimplemented\n"); exit(1); }
static inline void ee_i_addas(struct ee_state* ee) {
    /* To-do: FPU */
}
static inline void ee_i_addi(struct ee_state* ee) { printf("ee: addi unimplemented\n"); exit(1); }
static inline void ee_i_addiu(struct ee_state* ee) {
    EE_RT = SE6432(EE_RS + SE6416(EE_D_I16));
}
static inline void ee_i_adds(struct ee_state* ee) { printf("ee: adds unimplemented\n"); exit(1); }
static inline void ee_i_addu(struct ee_state* ee) {
    EE_RD = SE6432(EE_RS + EE_RT);
}
static inline void ee_i_and(struct ee_state* ee) {
    EE_RD = EE_RS & EE_RT;
}
static inline void ee_i_andi(struct ee_state* ee) {
    EE_RT = EE_RS & EE_D_I16;
}
static inline void ee_i_bc0f(struct ee_state* ee) { printf("ee: bc0f unimplemented\n"); exit(1); }
static inline void ee_i_bc0fl(struct ee_state* ee) { printf("ee: bc0fl unimplemented\n"); exit(1); }
static inline void ee_i_bc0t(struct ee_state* ee) { printf("ee: bc0t unimplemented\n"); exit(1); }
static inline void ee_i_bc0tl(struct ee_state* ee) { printf("ee: bc0tl unimplemented\n"); exit(1); }
static inline void ee_i_bc1f(struct ee_state* ee) { printf("ee: bc1f unimplemented\n"); exit(1); }
static inline void ee_i_bc1fl(struct ee_state* ee) { printf("ee: bc1fl unimplemented\n"); exit(1); }
static inline void ee_i_bc1t(struct ee_state* ee) { printf("ee: bc1t unimplemented\n"); exit(1); }
static inline void ee_i_bc1tl(struct ee_state* ee) { printf("ee: bc1tl unimplemented\n"); exit(1); }
static inline void ee_i_bc2f(struct ee_state* ee) { printf("ee: bc2f unimplemented\n"); exit(1); }
static inline void ee_i_bc2fl(struct ee_state* ee) { printf("ee: bc2fl unimplemented\n"); exit(1); }
static inline void ee_i_bc2t(struct ee_state* ee) { printf("ee: bc2t unimplemented\n"); exit(1); }
static inline void ee_i_bc2tl(struct ee_state* ee) { printf("ee: bc2tl unimplemented\n"); exit(1); }
static inline void ee_i_beq(struct ee_state* ee) {
    BRANCH(EE_RS == EE_RT, EE_D_SI16);
}
static inline void ee_i_beql(struct ee_state* ee) {
    BRANCH_LIKELY(EE_RS == EE_RT, EE_D_SI16);
}
static inline void ee_i_bgez(struct ee_state* ee) {
    BRANCH((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgezal(struct ee_state* ee) { printf("ee: bgezal unimplemented\n"); exit(1); }
static inline void ee_i_bgezall(struct ee_state* ee) { printf("ee: bgezall unimplemented\n"); exit(1); }
static inline void ee_i_bgezl(struct ee_state* ee) {
    BRANCH_LIKELY((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgtz(struct ee_state* ee) {
    BRANCH((int64_t)EE_RS > (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgtzl(struct ee_state* ee) { printf("ee: bgtzl unimplemented\n"); exit(1); }
static inline void ee_i_blez(struct ee_state* ee) {
    BRANCH((int64_t)EE_RS <= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_blezl(struct ee_state* ee) { printf("ee: blezl unimplemented\n"); exit(1); }
static inline void ee_i_bltz(struct ee_state* ee) {
    BRANCH((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltzal(struct ee_state* ee) { printf("ee: bltzal unimplemented\n"); exit(1); }
static inline void ee_i_bltzall(struct ee_state* ee) { printf("ee: bltzall unimplemented\n"); exit(1); }
static inline void ee_i_bltzl(struct ee_state* ee) {
    BRANCH_LIKELY((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bne(struct ee_state* ee) {
    BRANCH(EE_RS != EE_RT, EE_D_SI16);
}
static inline void ee_i_bnel(struct ee_state* ee) {
    BRANCH_LIKELY(EE_RS != EE_RT, EE_D_SI16);
}
static inline void ee_i_break(struct ee_state* ee) { printf("ee: break unimplemented\n"); exit(1); }
static inline void ee_i_cache(struct ee_state* ee) {
    /* To-do: Cache emulation */
}
static inline void ee_i_callmsr(struct ee_state* ee) { printf("ee: callmsr unimplemented\n"); exit(1); }
static inline void ee_i_ceq(struct ee_state* ee) { printf("ee: ceq unimplemented\n"); exit(1); }
static inline void ee_i_cf(struct ee_state* ee) { printf("ee: cf unimplemented\n"); exit(1); }
static inline void ee_i_cfc1(struct ee_state* ee) { printf("ee: cfc1 unimplemented\n"); exit(1); }
static inline void ee_i_cfc2(struct ee_state* ee) {
    /* To-do: VU1 */
}
static inline void ee_i_cle(struct ee_state* ee) { printf("ee: cle unimplemented\n"); exit(1); }
static inline void ee_i_clt(struct ee_state* ee) { printf("ee: clt unimplemented\n"); exit(1); }
static inline void ee_i_ctc1(struct ee_state* ee) {
    /* To-do: FPU */
}
static inline void ee_i_ctc2(struct ee_state* ee) {
    /* To-do: VU1 */
}
static inline void ee_i_cvts(struct ee_state* ee) { printf("ee: cvts unimplemented\n"); exit(1); }
static inline void ee_i_cvtw(struct ee_state* ee) { printf("ee: cvtw unimplemented\n"); exit(1); }
static inline void ee_i_dadd(struct ee_state* ee) { printf("ee: dadd unimplemented\n"); exit(1); }
static inline void ee_i_daddi(struct ee_state* ee) { printf("ee: daddi unimplemented\n"); exit(1); }
static inline void ee_i_daddiu(struct ee_state* ee) {
    EE_RT = EE_RS + SE6416(EE_D_I16);
}
static inline void ee_i_daddu(struct ee_state* ee) {
    EE_RD = EE_RS + EE_RT;
}
static inline void ee_i_di(struct ee_state* ee) {
    int edi = ee->status & EE_SR_EDI;
    int exl = ee->status & EE_SR_EXL;
    int erl = ee->status & EE_SR_ERL;
    int ksu = ee->status & EE_SR_KSU;
    
    if (edi || exl || erl || !ksu)
        ee->status &= ~EE_SR_EIE;
}
static inline void ee_i_div(struct ee_state* ee) {
    int t = EE_D_RT;
    int s = EE_D_RS;

    EE_HI0 = SE6432(ee->r[s].sl32 % ee->r[t].sl32);
    EE_LO0 = SE6432(ee->r[s].sl32 / ee->r[t].sl32);

    ee->r[s].ul64 = EE_LO0;
}
static inline void ee_i_div1(struct ee_state* ee) {
    int t = EE_D_RT;
    int s = EE_D_RS;

    EE_HI1 = SE6432(ee->r[s].sl32 % ee->r[t].sl32);
    EE_LO1 = SE6432(ee->r[s].sl32 / ee->r[t].sl32);
}
static inline void ee_i_divs(struct ee_state* ee) { printf("ee: divs unimplemented\n"); exit(1); }
static inline void ee_i_divu(struct ee_state* ee) {
    int t = EE_D_RT;
    int s = EE_D_RS;

    if (!ee->r[t].ul32) {
        printf("ee: division by zero\n");

        exit(1);
    }

    EE_HI0 = SE6432(ee->r[s].ul32 % ee->r[t].ul32);
    EE_LO0 = SE6432(ee->r[s].ul32 / ee->r[t].ul32);
}
static inline void ee_i_divu1(struct ee_state* ee) {
    int t = EE_D_RT;
    int s = EE_D_RS;

    EE_HI1 = SE6432(ee->r[s].ul32 % ee->r[t].ul32);
    EE_LO1 = SE6432(ee->r[s].ul32 / ee->r[t].ul32);
}
static inline void ee_i_dsll(struct ee_state* ee) {
    EE_RD = EE_RT << EE_D_SA;
}
static inline void ee_i_dsll32(struct ee_state* ee) {
    EE_RD = EE_RT << (EE_D_SA + 32);
}
static inline void ee_i_dsllv(struct ee_state* ee) {
    EE_RD = EE_RT << (EE_RS & 0x3f);
}
static inline void ee_i_dsra(struct ee_state* ee) {
    EE_RD = ((int64_t)EE_RT) >> EE_D_SA;
}
static inline void ee_i_dsra32(struct ee_state* ee) {
    EE_RD = ((int64_t)EE_RT) >> (EE_D_SA + 32);
}
static inline void ee_i_dsrav(struct ee_state* ee) {
    EE_RD = ((int64_t)EE_RT) >> (EE_RS & 0x3f);
}
static inline void ee_i_dsrl(struct ee_state* ee) {
    EE_RD = EE_RT >> EE_D_SA;
}
static inline void ee_i_dsrl32(struct ee_state* ee) {
    EE_RD = EE_RT >> (EE_D_SA + 32);
}
static inline void ee_i_dsrlv(struct ee_state* ee) { printf("ee: dsrlv unimplemented\n"); exit(1); }
static inline void ee_i_dsub(struct ee_state* ee) { printf("ee: dsub unimplemented\n"); exit(1); }
static inline void ee_i_dsubu(struct ee_state* ee) { printf("ee: dsubu unimplemented\n"); exit(1); }
static inline void ee_i_ei(struct ee_state* ee) {
    int edi = ee->status & EE_SR_EDI;
    int exl = ee->status & EE_SR_EXL;
    int erl = ee->status & EE_SR_ERL;
    int ksu = ee->status & EE_SR_KSU;
    
    if (edi || exl || erl || !ksu)
        ee->status |= EE_SR_EIE;
}
static inline void ee_i_eret(struct ee_state* ee) {
    if (ee->status & EE_SR_ERL) {
        ee_set_pc(ee, ee->errorepc);

        ee->status &= ~EE_SR_ERL;
    } else {
        ee_set_pc(ee, ee->epc);

        ee->status &= ~EE_SR_EXL;
    }
}
static inline void ee_i_j(struct ee_state* ee) {
    ee->next_pc = (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2);
}
static inline void ee_i_jal(struct ee_state* ee) {
    ee->r[31].ul64 = ee->next_pc;
    // printf("%08x: jal rd=%08x t=%08x\n", ee->pc -4 , ee->r[31].ul64, (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2));
    ee->next_pc = (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2);
}
static inline void ee_i_jalr(struct ee_state* ee) {
    EE_RD = ee->next_pc;
    // printf("%08x: jalr rd=%08x rs=%08x\n", ee->pc, EE_RD32, EE_RS32);
    ee->next_pc = EE_RS32;
}
static inline void ee_i_jr(struct ee_state* ee) {
    // printf("%08x: jr rs=%08x v0=%08x\n", ee->pc, EE_RS32, ee->r[2].ul32);
    ee->next_pc = EE_RS32;
}
static inline void ee_i_lb(struct ee_state* ee) {
    EE_RT = SE648(bus_read8(ee, EE_RS32 + SE3216(EE_D_I16)));
}
static inline void ee_i_lbu(struct ee_state* ee) {
    EE_RT = bus_read8(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_ld(struct ee_state* ee) {
    EE_RT = bus_read64(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_ldl(struct ee_state* ee) {
    static const uint8_t ldl_shift[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
    static const uint64_t ldl_mask[8] = {
        0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
        0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
    };

    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t shift = addr & 7;
    uint64_t data = bus_read64(ee, addr & ~7);

	EE_RT = (EE_RT & ldl_mask[shift]) | (data << ldl_shift[shift]);
}
static inline void ee_i_ldr(struct ee_state* ee) {
    static const uint8_t ldr_shift[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
    static const uint64_t ldr_mask[8] = {
        0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL,
        0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL
    };

    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t shift = addr & 7;
    uint64_t data = bus_read64(ee, addr & ~7);

	EE_RT = (EE_RT & ldr_mask[shift]) | (data >> ldr_shift[shift]);
}
static inline void ee_i_lh(struct ee_state* ee) {
    EE_RT = SE6416(bus_read16(ee, EE_RS32 + SE3216(EE_D_I16)));
}
static inline void ee_i_lhu(struct ee_state* ee) {
    EE_RT = bus_read16(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_lq(struct ee_state* ee) {
    ee->r[EE_D_RT] = bus_read128(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_lqc2(struct ee_state* ee) { printf("ee: lqc2 unimplemented\n"); exit(1); }
static inline void ee_i_lui(struct ee_state* ee) {
    EE_RT = SE6432(EE_D_I16 << 16);
}
static inline void ee_i_lw(struct ee_state* ee) {
    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);

    EE_RT = SE6432(bus_read32(ee, addr));

    // printf("lw: addr=%08x value=%016lx\n", addr, EE_RT);
}
static inline void ee_i_lwc1(struct ee_state* ee) { printf("ee: lwc1 unimplemented\n"); exit(1); }
static inline void ee_i_lwl(struct ee_state* ee) { printf("ee: lwl unimplemented\n"); exit(1); }
static inline void ee_i_lwr(struct ee_state* ee) { printf("ee: lwr unimplemented\n"); exit(1); }
static inline void ee_i_lwu(struct ee_state* ee) {
    EE_RT = bus_read32(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_madd(struct ee_state* ee) { printf("ee: madd unimplemented\n"); exit(1); }
static inline void ee_i_madd1(struct ee_state* ee) { printf("ee: madd1 unimplemented\n"); exit(1); }
static inline void ee_i_maddas(struct ee_state* ee) { printf("ee: maddas unimplemented\n"); exit(1); }
static inline void ee_i_madds(struct ee_state* ee) { printf("ee: madds unimplemented\n"); exit(1); }
static inline void ee_i_maddu(struct ee_state* ee) { printf("ee: maddu unimplemented\n"); exit(1); }
static inline void ee_i_maddu1(struct ee_state* ee) { printf("ee: maddu1 unimplemented\n"); exit(1); }
static inline void ee_i_maxs(struct ee_state* ee) { printf("ee: maxs unimplemented\n"); exit(1); }
static inline void ee_i_mfc0(struct ee_state* ee) {
    EE_RT32 = ee->cop0_r[EE_D_RD];
}
static inline void ee_i_mfc1(struct ee_state* ee) { printf("ee: mfc1 unimplemented\n"); exit(1); }
static inline void ee_i_mfhi(struct ee_state* ee) {
    EE_RD = EE_HI0;
}
static inline void ee_i_mfhi1(struct ee_state* ee) {
    EE_RD = EE_HI1;
}
static inline void ee_i_mflo(struct ee_state* ee) {
    EE_RD = EE_LO0;
}
static inline void ee_i_mflo1(struct ee_state* ee) {
    EE_RD = EE_LO1;
}
static inline void ee_i_mfsa(struct ee_state* ee) {
    EE_RD = ee->sa;
}
static inline void ee_i_mins(struct ee_state* ee) { printf("ee: mins unimplemented\n"); exit(1); }
static inline void ee_i_movn(struct ee_state* ee) {
    if (EE_RT) EE_RD = EE_RS;
}
static inline void ee_i_movs(struct ee_state* ee) { printf("ee: movs unimplemented\n"); exit(1); }
static inline void ee_i_movz(struct ee_state* ee) {
    if (!EE_RT) EE_RD = EE_RS;
}
static inline void ee_i_msubas(struct ee_state* ee) { printf("ee: msubas unimplemented\n"); exit(1); }
static inline void ee_i_msubs(struct ee_state* ee) { printf("ee: msubs unimplemented\n"); exit(1); }
static inline void ee_i_mtc0(struct ee_state* ee) {
    ee->cop0_r[EE_D_RD] = EE_RT32;
}
static inline void ee_i_mtc1(struct ee_state* ee) {
    /* To-do: FPU */
}
static inline void ee_i_mthi(struct ee_state* ee) {
    EE_HI0 = EE_RS;
}
static inline void ee_i_mthi1(struct ee_state* ee) {
    EE_HI1 = EE_RS;
}
static inline void ee_i_mtlo(struct ee_state* ee) {
    EE_LO0 = EE_RS;
}
static inline void ee_i_mtlo1(struct ee_state* ee) {
    EE_LO1 = EE_RS;
}
static inline void ee_i_mtsa(struct ee_state* ee) {
    ee->sa = EE_RS;
}
static inline void ee_i_mtsab(struct ee_state* ee) { printf("ee: mtsab unimplemented\n"); exit(1); }
static inline void ee_i_mtsah(struct ee_state* ee) { printf("ee: mtsah unimplemented\n"); exit(1); }
static inline void ee_i_mulas(struct ee_state* ee) { printf("ee: mulas unimplemented\n"); exit(1); }
static inline void ee_i_muls(struct ee_state* ee) { printf("ee: muls unimplemented\n"); exit(1); }
static inline void ee_i_mult(struct ee_state* ee) {
    uint64_t r = SE6432(EE_RS32) * SE6432(EE_RT32);

    EE_LO0 = SE6432(r & 0xffffffff);
    EE_HI0 = SE6432(r >> 32);

    EE_RD = EE_LO0;
}
static inline void ee_i_mult1(struct ee_state* ee) {
    uint64_t r = SE6432(EE_RS32) * SE6432(EE_RT32);

    EE_LO1 = SE6432(r & 0xffffffff);
    EE_HI1 = SE6432(r >> 32);

    EE_RD = EE_LO1;
}
static inline void ee_i_multu(struct ee_state* ee) { printf("ee: multu unimplemented\n"); exit(1); }
static inline void ee_i_multu1(struct ee_state* ee) { printf("ee: multu1 unimplemented\n"); exit(1); }
static inline void ee_i_negs(struct ee_state* ee) { printf("ee: negs unimplemented\n"); exit(1); }
static inline void ee_i_nor(struct ee_state* ee) {
    EE_RD = ~(EE_RS | EE_RT);
}
static inline void ee_i_or(struct ee_state* ee) {
    EE_RD = EE_RS | EE_RT;
}
static inline void ee_i_ori(struct ee_state* ee) {
    EE_RT = EE_RS | EE_D_I16;
}
static inline void ee_i_pabsh(struct ee_state* ee) { printf("ee: pabsh unimplemented\n"); exit(1); }
static inline void ee_i_pabsw(struct ee_state* ee) { printf("ee: pabsw unimplemented\n"); exit(1); }
static inline void ee_i_paddb(struct ee_state* ee) { printf("ee: paddb unimplemented\n"); exit(1); }
static inline void ee_i_paddh(struct ee_state* ee) { printf("ee: paddh unimplemented\n"); exit(1); }
static inline void ee_i_paddsb(struct ee_state* ee) { printf("ee: paddsb unimplemented\n"); exit(1); }
static inline void ee_i_paddsh(struct ee_state* ee) { printf("ee: paddsh unimplemented\n"); exit(1); }
static inline void ee_i_paddsw(struct ee_state* ee) { printf("ee: paddsw unimplemented\n"); exit(1); }
static inline void ee_i_paddub(struct ee_state* ee) { printf("ee: paddub unimplemented\n"); exit(1); }
static inline void ee_i_padduh(struct ee_state* ee) { printf("ee: padduh unimplemented\n"); exit(1); }
static inline void ee_i_padduw(struct ee_state* ee) {
    uint64_t s0 = ee->r[EE_D_RS].u32[0];
    uint64_t s1 = ee->r[EE_D_RS].u32[1];
    uint64_t s2 = ee->r[EE_D_RS].u32[2];
    uint64_t s3 = ee->r[EE_D_RS].u32[3];
    uint64_t t0 = ee->r[EE_D_RT].u32[0];
    uint64_t t1 = ee->r[EE_D_RT].u32[1];
    uint64_t t2 = ee->r[EE_D_RT].u32[2];
    uint64_t t3 = ee->r[EE_D_RT].u32[3];
    uint64_t r0 = s0 + t0;
    uint64_t r1 = s1 + t1;
    uint64_t r2 = s2 + t2;
    uint64_t r3 = s3 + t3;

    ee->r[EE_D_RD].u32[0] = (r0 > 0xffffffff) ? 0xffffffff : r0;
    ee->r[EE_D_RD].u32[1] = (r1 > 0xffffffff) ? 0xffffffff : r1;
    ee->r[EE_D_RD].u32[2] = (r2 > 0xffffffff) ? 0xffffffff : r2;
    ee->r[EE_D_RD].u32[3] = (r3 > 0xffffffff) ? 0xffffffff : r3;
}
static inline void ee_i_paddw(struct ee_state* ee) { printf("ee: paddw unimplemented\n"); exit(1); }
static inline void ee_i_padsbh(struct ee_state* ee) { printf("ee: padsbh unimplemented\n"); exit(1); }
static inline void ee_i_pand(struct ee_state* ee) { printf("ee: pand unimplemented\n"); exit(1); }
static inline void ee_i_pceqb(struct ee_state* ee) { printf("ee: pceqb unimplemented\n"); exit(1); }
static inline void ee_i_pceqh(struct ee_state* ee) { printf("ee: pceqh unimplemented\n"); exit(1); }
static inline void ee_i_pceqw(struct ee_state* ee) { printf("ee: pceqw unimplemented\n"); exit(1); }
static inline void ee_i_pcgtb(struct ee_state* ee) { printf("ee: pcgtb unimplemented\n"); exit(1); }
static inline void ee_i_pcgth(struct ee_state* ee) { printf("ee: pcgth unimplemented\n"); exit(1); }
static inline void ee_i_pcgtw(struct ee_state* ee) { printf("ee: pcgtw unimplemented\n"); exit(1); }
static inline void ee_i_pcpyh(struct ee_state* ee) { printf("ee: pcpyh unimplemented\n"); exit(1); }
static inline void ee_i_pcpyld(struct ee_state* ee) {
    ee->r[EE_D_RD].u64[0] = EE_RT;
    ee->r[EE_D_RD].u64[1] = EE_RS;
}
static inline void ee_i_pcpyud(struct ee_state* ee) {
    ee->r[EE_D_RD].u64[0] = ee->r[EE_D_RT].u64[1];
    ee->r[EE_D_RD].u64[1] = ee->r[EE_D_RS].u64[1];
}
static inline void ee_i_pdivbw(struct ee_state* ee) { printf("ee: pdivbw unimplemented\n"); exit(1); }
static inline void ee_i_pdivuw(struct ee_state* ee) { printf("ee: pdivuw unimplemented\n"); exit(1); }
static inline void ee_i_pdivw(struct ee_state* ee) { printf("ee: pdivw unimplemented\n"); exit(1); }
static inline void ee_i_pexch(struct ee_state* ee) { printf("ee: pexch unimplemented\n"); exit(1); }
static inline void ee_i_pexcw(struct ee_state* ee) { printf("ee: pexcw unimplemented\n"); exit(1); }
static inline void ee_i_pexeh(struct ee_state* ee) { printf("ee: pexeh unimplemented\n"); exit(1); }
static inline void ee_i_pexew(struct ee_state* ee) { printf("ee: pexew unimplemented\n"); exit(1); }
static inline void ee_i_pext5(struct ee_state* ee) { printf("ee: pext5 unimplemented\n"); exit(1); }
static inline void ee_i_pextlb(struct ee_state* ee) { printf("ee: pextlb unimplemented\n"); exit(1); }
static inline void ee_i_pextlh(struct ee_state* ee) { printf("ee: pextlh unimplemented\n"); exit(1); }
static inline void ee_i_pextlw(struct ee_state* ee) {
    uint64_t s0 = ee->r[EE_D_RS].u32[0];
    uint64_t s1 = ee->r[EE_D_RS].u32[1];
    uint64_t t0 = ee->r[EE_D_RT].u32[0];
    uint64_t t1 = ee->r[EE_D_RT].u32[1];

    ee->r[EE_D_RD].u64[0] = (s0 << 32) | t0;
    ee->r[EE_D_RD].u64[1] = (s1 << 32) | t1;
}
static inline void ee_i_pextub(struct ee_state* ee) { printf("ee: pextub unimplemented\n"); exit(1); }
static inline void ee_i_pextuh(struct ee_state* ee) { printf("ee: pextuh unimplemented\n"); exit(1); }
static inline void ee_i_pextuw(struct ee_state* ee) { printf("ee: pextuw unimplemented\n"); exit(1); }
static inline void ee_i_phmadh(struct ee_state* ee) { printf("ee: phmadh unimplemented\n"); exit(1); }
static inline void ee_i_phmsbh(struct ee_state* ee) { printf("ee: phmsbh unimplemented\n"); exit(1); }
static inline void ee_i_pinteh(struct ee_state* ee) { printf("ee: pinteh unimplemented\n"); exit(1); }
static inline void ee_i_pinth(struct ee_state* ee) { printf("ee: pinth unimplemented\n"); exit(1); }
static inline void ee_i_plzcw(struct ee_state* ee) { 
    for (int i = 0; i < 2; i++) {
		uint32_t word = ee->r[EE_D_RS].u32[i];

		int msb = word & 0x80000000;

		word = (msb ? ~word : word);

		ee->r[EE_D_RD].u32[i] = (word ? (__builtin_clz(word) - 1) : 0x1f);
	}
}
static inline void ee_i_pmaddh(struct ee_state* ee) { printf("ee: pmaddh unimplemented\n"); exit(1); }
static inline void ee_i_pmadduw(struct ee_state* ee) { printf("ee: pmadduw unimplemented\n"); exit(1); }
static inline void ee_i_pmaddw(struct ee_state* ee) { printf("ee: pmaddw unimplemented\n"); exit(1); }
static inline void ee_i_pmaxh(struct ee_state* ee) { printf("ee: pmaxh unimplemented\n"); exit(1); }
static inline void ee_i_pmaxw(struct ee_state* ee) { printf("ee: pmaxw unimplemented\n"); exit(1); }
static inline void ee_i_pmfhi(struct ee_state* ee) {
    ee->r[EE_D_RD] = ee->hi;
}
static inline void ee_i_pmfhl(struct ee_state* ee) { printf("ee: pmfhl unimplemented\n"); exit(1); }
static inline void ee_i_pmflo(struct ee_state* ee) {
    ee->r[EE_D_RD] = ee->lo;
}
static inline void ee_i_pminh(struct ee_state* ee) { printf("ee: pminh unimplemented\n"); exit(1); }
static inline void ee_i_pminw(struct ee_state* ee) { printf("ee: pminw unimplemented\n"); exit(1); }
static inline void ee_i_pmsubh(struct ee_state* ee) { printf("ee: pmsubh unimplemented\n"); exit(1); }
static inline void ee_i_pmsubw(struct ee_state* ee) { printf("ee: pmsubw unimplemented\n"); exit(1); }
static inline void ee_i_pmthi(struct ee_state* ee) { printf("ee: pmthi unimplemented\n"); exit(1); }
static inline void ee_i_pmthl(struct ee_state* ee) { printf("ee: pmthl unimplemented\n"); exit(1); }
static inline void ee_i_pmtlo(struct ee_state* ee) { printf("ee: pmtlo unimplemented\n"); exit(1); }
static inline void ee_i_pmulth(struct ee_state* ee) { printf("ee: pmulth unimplemented\n"); exit(1); }
static inline void ee_i_pmultuw(struct ee_state* ee) { printf("ee: pmultuw unimplemented\n"); exit(1); }
static inline void ee_i_pmultw(struct ee_state* ee) { printf("ee: pmultw unimplemented\n"); exit(1); }
static inline void ee_i_pnor(struct ee_state* ee) { printf("ee: pnor unimplemented\n"); exit(1); }
static inline void ee_i_por(struct ee_state* ee) {
    ee->r[EE_D_RD].u128 = ee->r[EE_D_RS].u128 | ee->r[EE_D_RT].u128;
}
static inline void ee_i_ppac5(struct ee_state* ee) { printf("ee: ppac5 unimplemented\n"); exit(1); }
static inline void ee_i_ppacb(struct ee_state* ee) { printf("ee: ppacb unimplemented\n"); exit(1); }
static inline void ee_i_ppach(struct ee_state* ee) { printf("ee: ppach unimplemented\n"); exit(1); }
static inline void ee_i_ppacw(struct ee_state* ee) { printf("ee: ppacw unimplemented\n"); exit(1); }
static inline void ee_i_pref(struct ee_state* ee) { printf("ee: pref unimplemented\n"); exit(1); }
static inline void ee_i_prevh(struct ee_state* ee) { printf("ee: prevh unimplemented\n"); exit(1); }
static inline void ee_i_prot3w(struct ee_state* ee) { printf("ee: prot3w unimplemented\n"); exit(1); }
static inline void ee_i_psllh(struct ee_state* ee) { printf("ee: psllh unimplemented\n"); exit(1); }
static inline void ee_i_psllvw(struct ee_state* ee) { printf("ee: psllvw unimplemented\n"); exit(1); }
static inline void ee_i_psllw(struct ee_state* ee) { printf("ee: psllw unimplemented\n"); exit(1); }
static inline void ee_i_psrah(struct ee_state* ee) { printf("ee: psrah unimplemented\n"); exit(1); }
static inline void ee_i_psravw(struct ee_state* ee) { printf("ee: psravw unimplemented\n"); exit(1); }
static inline void ee_i_psraw(struct ee_state* ee) { printf("ee: psraw unimplemented\n"); exit(1); }
static inline void ee_i_psrlh(struct ee_state* ee) { printf("ee: psrlh unimplemented\n"); exit(1); }
static inline void ee_i_psrlvw(struct ee_state* ee) { printf("ee: psrlvw unimplemented\n"); exit(1); }
static inline void ee_i_psrlw(struct ee_state* ee) { printf("ee: psrlw unimplemented\n"); exit(1); }
static inline void ee_i_psubb(struct ee_state* ee) { printf("ee: psubb unimplemented\n"); exit(1); }
static inline void ee_i_psubh(struct ee_state* ee) { printf("ee: psubh unimplemented\n"); exit(1); }
static inline void ee_i_psubsb(struct ee_state* ee) { printf("ee: psubsb unimplemented\n"); exit(1); }
static inline void ee_i_psubsh(struct ee_state* ee) { printf("ee: psubsh unimplemented\n"); exit(1); }
static inline void ee_i_psubsw(struct ee_state* ee) { printf("ee: psubsw unimplemented\n"); exit(1); }
static inline void ee_i_psubub(struct ee_state* ee) { printf("ee: psubub unimplemented\n"); exit(1); }
static inline void ee_i_psubuh(struct ee_state* ee) { printf("ee: psubuh unimplemented\n"); exit(1); }
static inline void ee_i_psubuw(struct ee_state* ee) { printf("ee: psubuw unimplemented\n"); exit(1); }
static inline void ee_i_psubw(struct ee_state* ee) { printf("ee: psubw unimplemented\n"); exit(1); }
static inline void ee_i_pxor(struct ee_state* ee) { printf("ee: pxor unimplemented\n"); exit(1); }
static inline void ee_i_qfsrv(struct ee_state* ee) { printf("ee: qfsrv unimplemented\n"); exit(1); }
static inline void ee_i_qmfc2(struct ee_state* ee) {
    /* To-do: VU1 */
}
static inline void ee_i_qmtc2(struct ee_state* ee) {
    /* To-do: VU1 */
}
static inline void ee_i_rsqrts(struct ee_state* ee) { printf("ee: rsqrts unimplemented\n"); exit(1); }
static inline void ee_i_sb(struct ee_state* ee) {
    bus_write8(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT);
}
static inline void ee_i_sd(struct ee_state* ee) {
    bus_write64(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT);
}
static inline void ee_i_sdl(struct ee_state* ee) {
    static const uint8_t sdl_shift[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
    static const uint64_t sdl_mask[8] = {
        0xffffffffffffff00ULL, 0xffffffffffff0000ULL, 0xffffffffff000000ULL, 0xffffffff00000000ULL,
        0xffffff0000000000ULL, 0xffff000000000000ULL, 0xff00000000000000ULL, 0x0000000000000000ULL
    };

    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t shift = addr & 7;
    uint64_t data = bus_read64(ee, addr & ~7);

    bus_write64(ee, addr & ~7, (EE_RT >> sdl_shift[shift]) | (data & sdl_mask[shift]));
}
static inline void ee_i_sdr(struct ee_state* ee) {
    static const uint8_t sdr_shift[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
    static const uint64_t sdr_mask[8] = {
        0x0000000000000000ULL, 0x00000000000000ffULL, 0x000000000000ffffULL, 0x0000000000ffffffULL,
        0x00000000ffffffffULL, 0x000000ffffffffffULL, 0x0000ffffffffffffULL, 0x00ffffffffffffffULL
    };

    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t shift = addr & 7;
    uint64_t data = bus_read64(ee, addr & ~7);

    bus_write64(ee, addr & ~7, (EE_RT << sdr_shift[shift]) | (data & sdr_mask[shift]));
}
static inline void ee_i_sh(struct ee_state* ee) {
    bus_write16(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT);
}
static inline void ee_i_sll(struct ee_state* ee) {
    EE_RD = SE6432(EE_RT << EE_D_SA);
}
static inline void ee_i_sllv(struct ee_state* ee) {
    EE_RD = SE6432(EE_RT << (EE_RS & 0x1f));
}
static inline void ee_i_slt(struct ee_state* ee) {
    EE_RD = (int64_t)EE_RS < (int64_t)EE_RT;
}
static inline void ee_i_slti(struct ee_state* ee) {
    EE_RT = ((int64_t)EE_RS) < SE6416(EE_D_I16);
}
static inline void ee_i_sltiu(struct ee_state* ee) {
    EE_RT = EE_RS < (uint64_t)(SE6416(EE_D_I16));
}
static inline void ee_i_sltu(struct ee_state* ee) {
    EE_RD = EE_RS < EE_RT;
}
static inline void ee_i_sq(struct ee_state* ee) {
    bus_write128(ee, EE_RS32 + SE3216(EE_D_I16), ee->r[EE_D_RT]);
}
static inline void ee_i_sqc2(struct ee_state* ee) { printf("ee: sqc2 unimplemented\n"); exit(1); }
static inline void ee_i_sqrts(struct ee_state* ee) { printf("ee: sqrts unimplemented\n"); exit(1); }
static inline void ee_i_sra(struct ee_state* ee) {
    EE_RD = SE6432(((int32_t)EE_RT) >> EE_D_SA);
}
static inline void ee_i_srav(struct ee_state* ee) {
    EE_RD = SE6432((int64_t)EE_RT >> (EE_RS & 0x1f));
}
static inline void ee_i_srl(struct ee_state* ee) { 
    EE_RD = SE6432(EE_RT >> EE_D_SA);
}
static inline void ee_i_srlv(struct ee_state* ee) {
    EE_RD = SE6432(EE_RT >> (EE_RS & 0x1f));
}
static inline void ee_i_sub(struct ee_state* ee) { printf("ee: sub unimplemented\n"); exit(1); }
static inline void ee_i_subas(struct ee_state* ee) { printf("ee: subas unimplemented\n"); exit(1); }
static inline void ee_i_subs(struct ee_state* ee) { printf("ee: subs unimplemented\n"); exit(1); }
static inline void ee_i_subu(struct ee_state* ee) {
    EE_RD = SE6432(EE_RS - EE_RT);
}
static inline void ee_i_sw(struct ee_state* ee) {
    bus_write32(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT32);
}
static inline void ee_i_swc1(struct ee_state* ee) {
    /* To-do: SWC1 */
    bus_write32(ee, EE_RS32 + SE3216(EE_D_I16), 0);
}
static inline void ee_i_swl(struct ee_state* ee) { printf("ee: swl unimplemented\n"); exit(1); }
static inline void ee_i_swr(struct ee_state* ee) { printf("ee: swr unimplemented\n"); exit(1); }
static inline void ee_i_sync(struct ee_state* ee) {
    /* Do nothing */
}

#include "syscall.h"

static inline void ee_i_syscall(struct ee_state* ee) {
    printf("ee: syscall %08x: %s\n", ee->r[3].ul32, ee_get_syscall(ee->r[3].ul64));

    char buf[128];
    struct ee_dis_state ds;

    ds.pc = ee->pc;
    ds.print_address = 1;
    ds.print_opcode = 1;

    // printf("---------> %s\n", ee_disassemble(buf, bus_read32(ee, ee->pc), &ds));

    ee_exception_level1(ee, CAUSE_EXC1_SYS);
}
static inline void ee_i_teq(struct ee_state* ee) { printf("ee: teq unimplemented\n"); exit(1); }
static inline void ee_i_teqi(struct ee_state* ee) { printf("ee: teqi unimplemented\n"); exit(1); }
static inline void ee_i_tge(struct ee_state* ee) { printf("ee: tge unimplemented\n"); exit(1); }
static inline void ee_i_tgei(struct ee_state* ee) { printf("ee: tgei unimplemented\n"); exit(1); }
static inline void ee_i_tgeiu(struct ee_state* ee) { printf("ee: tgeiu unimplemented\n"); exit(1); }
static inline void ee_i_tgeu(struct ee_state* ee) { printf("ee: tgeu unimplemented\n"); exit(1); }
static inline void ee_i_tlbp(struct ee_state* ee) { printf("ee: tlbp unimplemented\n"); exit(1); }
static inline void ee_i_tlbr(struct ee_state* ee) { printf("ee: tlbr unimplemented\n"); exit(1); }
static inline void ee_i_tlbwi(struct ee_state* ee) {
    /* To-do: MMU */
}
static inline void ee_i_tlbwr(struct ee_state* ee) { printf("ee: tlbwr unimplemented\n"); exit(1); }
static inline void ee_i_tlt(struct ee_state* ee) { printf("ee: tlt unimplemented\n"); exit(1); }
static inline void ee_i_tlti(struct ee_state* ee) { printf("ee: tlti unimplemented\n"); exit(1); }
static inline void ee_i_tltiu(struct ee_state* ee) { printf("ee: tltiu unimplemented\n"); exit(1); }
static inline void ee_i_tltu(struct ee_state* ee) { printf("ee: tltu unimplemented\n"); exit(1); }
static inline void ee_i_tne(struct ee_state* ee) { printf("ee: tne unimplemented\n"); exit(1); }
static inline void ee_i_tnei(struct ee_state* ee) { printf("ee: tnei unimplemented\n"); exit(1); }
static inline void ee_i_vabs(struct ee_state* ee) { printf("ee: vabs unimplemented\n"); exit(1); }
static inline void ee_i_vadd(struct ee_state* ee) { printf("ee: vadd unimplemented\n"); exit(1); }
static inline void ee_i_vadda(struct ee_state* ee) { printf("ee: vadda unimplemented\n"); exit(1); }
static inline void ee_i_vaddai(struct ee_state* ee) { printf("ee: vaddai unimplemented\n"); exit(1); }
static inline void ee_i_vaddaq(struct ee_state* ee) { printf("ee: vaddaq unimplemented\n"); exit(1); }
static inline void ee_i_vaddaw(struct ee_state* ee) { printf("ee: vaddaw unimplemented\n"); exit(1); }
static inline void ee_i_vaddax(struct ee_state* ee) { printf("ee: vaddax unimplemented\n"); exit(1); }
static inline void ee_i_vadday(struct ee_state* ee) { printf("ee: vadday unimplemented\n"); exit(1); }
static inline void ee_i_vaddaz(struct ee_state* ee) { printf("ee: vaddaz unimplemented\n"); exit(1); }
static inline void ee_i_vaddi(struct ee_state* ee) { printf("ee: vaddi unimplemented\n"); exit(1); }
static inline void ee_i_vaddq(struct ee_state* ee) { printf("ee: vaddq unimplemented\n"); exit(1); }
static inline void ee_i_vaddw(struct ee_state* ee) { printf("ee: vaddw unimplemented\n"); exit(1); }
static inline void ee_i_vaddx(struct ee_state* ee) { printf("ee: vaddx unimplemented\n"); exit(1); }
static inline void ee_i_vaddy(struct ee_state* ee) { printf("ee: vaddy unimplemented\n"); exit(1); }
static inline void ee_i_vaddz(struct ee_state* ee) { printf("ee: vaddz unimplemented\n"); exit(1); }
static inline void ee_i_vcallms(struct ee_state* ee) { printf("ee: vcallms unimplemented\n"); exit(1); }
static inline void ee_i_vclipw(struct ee_state* ee) { printf("ee: vclipw unimplemented\n"); exit(1); }
static inline void ee_i_vdiv(struct ee_state* ee) { printf("ee: vdiv unimplemented\n"); exit(1); }
static inline void ee_i_vftoi0(struct ee_state* ee) { printf("ee: vftoi0 unimplemented\n"); exit(1); }
static inline void ee_i_vftoi12(struct ee_state* ee) { printf("ee: vftoi12 unimplemented\n"); exit(1); }
static inline void ee_i_vftoi15(struct ee_state* ee) { printf("ee: vftoi15 unimplemented\n"); exit(1); }
static inline void ee_i_vftoi4(struct ee_state* ee) { printf("ee: vftoi4 unimplemented\n"); exit(1); }
static inline void ee_i_viadd(struct ee_state* ee) {
    /* To-do: VU1 */
}
static inline void ee_i_viaddi(struct ee_state* ee) { printf("ee: viaddi unimplemented\n"); exit(1); }
static inline void ee_i_viand(struct ee_state* ee) { printf("ee: viand unimplemented\n"); exit(1); }
static inline void ee_i_vilwr(struct ee_state* ee) { printf("ee: vilwr unimplemented\n"); exit(1); }
static inline void ee_i_vior(struct ee_state* ee) { printf("ee: vior unimplemented\n"); exit(1); }
static inline void ee_i_visub(struct ee_state* ee) { printf("ee: visub unimplemented\n"); exit(1); }
static inline void ee_i_viswr(struct ee_state* ee) {
    /* To-do: VU1 */
}
static inline void ee_i_vitof0(struct ee_state* ee) { printf("ee: vitof0 unimplemented\n"); exit(1); }
static inline void ee_i_vitof12(struct ee_state* ee) { printf("ee: vitof12 unimplemented\n"); exit(1); }
static inline void ee_i_vitof15(struct ee_state* ee) { printf("ee: vitof15 unimplemented\n"); exit(1); }
static inline void ee_i_vitof4(struct ee_state* ee) { printf("ee: vitof4 unimplemented\n"); exit(1); }
static inline void ee_i_vlqd(struct ee_state* ee) { printf("ee: vlqd unimplemented\n"); exit(1); }
static inline void ee_i_vlqi(struct ee_state* ee) { printf("ee: vlqi unimplemented\n"); exit(1); }
static inline void ee_i_vmadd(struct ee_state* ee) { printf("ee: vmadd unimplemented\n"); exit(1); }
static inline void ee_i_vmadda(struct ee_state* ee) { printf("ee: vmadda unimplemented\n"); exit(1); }
static inline void ee_i_vmaddai(struct ee_state* ee) { printf("ee: vmaddai unimplemented\n"); exit(1); }
static inline void ee_i_vmaddaq(struct ee_state* ee) { printf("ee: vmaddaq unimplemented\n"); exit(1); }
static inline void ee_i_vmaddaw(struct ee_state* ee) { printf("ee: vmaddaw unimplemented\n"); exit(1); }
static inline void ee_i_vmaddax(struct ee_state* ee) { printf("ee: vmaddax unimplemented\n"); exit(1); }
static inline void ee_i_vmadday(struct ee_state* ee) { printf("ee: vmadday unimplemented\n"); exit(1); }
static inline void ee_i_vmaddaz(struct ee_state* ee) { printf("ee: vmaddaz unimplemented\n"); exit(1); }
static inline void ee_i_vmaddi(struct ee_state* ee) { printf("ee: vmaddi unimplemented\n"); exit(1); }
static inline void ee_i_vmaddq(struct ee_state* ee) { printf("ee: vmaddq unimplemented\n"); exit(1); }
static inline void ee_i_vmaddw(struct ee_state* ee) { printf("ee: vmaddw unimplemented\n"); exit(1); }
static inline void ee_i_vmaddx(struct ee_state* ee) { printf("ee: vmaddx unimplemented\n"); exit(1); }
static inline void ee_i_vmaddy(struct ee_state* ee) { printf("ee: vmaddy unimplemented\n"); exit(1); }
static inline void ee_i_vmaddz(struct ee_state* ee) { printf("ee: vmaddz unimplemented\n"); exit(1); }
static inline void ee_i_vmax(struct ee_state* ee) { printf("ee: vmax unimplemented\n"); exit(1); }
static inline void ee_i_vmaxi(struct ee_state* ee) { printf("ee: vmaxi unimplemented\n"); exit(1); }
static inline void ee_i_vmaxw(struct ee_state* ee) { printf("ee: vmaxw unimplemented\n"); exit(1); }
static inline void ee_i_vmaxx(struct ee_state* ee) { printf("ee: vmaxx unimplemented\n"); exit(1); }
static inline void ee_i_vmaxy(struct ee_state* ee) { printf("ee: vmaxy unimplemented\n"); exit(1); }
static inline void ee_i_vmaxz(struct ee_state* ee) { printf("ee: vmaxz unimplemented\n"); exit(1); }
static inline void ee_i_vmfir(struct ee_state* ee) { printf("ee: vmfir unimplemented\n"); exit(1); }
static inline void ee_i_vmini(struct ee_state* ee) { printf("ee: vmini unimplemented\n"); exit(1); }
static inline void ee_i_vminii(struct ee_state* ee) { printf("ee: vminii unimplemented\n"); exit(1); }
static inline void ee_i_vminiw(struct ee_state* ee) { printf("ee: vminiw unimplemented\n"); exit(1); }
static inline void ee_i_vminix(struct ee_state* ee) { printf("ee: vminix unimplemented\n"); exit(1); }
static inline void ee_i_vminiy(struct ee_state* ee) { printf("ee: vminiy unimplemented\n"); exit(1); }
static inline void ee_i_vminiz(struct ee_state* ee) { printf("ee: vminiz unimplemented\n"); exit(1); }
static inline void ee_i_vmove(struct ee_state* ee) { printf("ee: vmove unimplemented\n"); exit(1); }
static inline void ee_i_vmr32(struct ee_state* ee) { printf("ee: vmr32 unimplemented\n"); exit(1); }
static inline void ee_i_vmsub(struct ee_state* ee) { printf("ee: vmsub unimplemented\n"); exit(1); }
static inline void ee_i_vmsuba(struct ee_state* ee) { printf("ee: vmsuba unimplemented\n"); exit(1); }
static inline void ee_i_vmsubai(struct ee_state* ee) { printf("ee: vmsubai unimplemented\n"); exit(1); }
static inline void ee_i_vmsubaq(struct ee_state* ee) { printf("ee: vmsubaq unimplemented\n"); exit(1); }
static inline void ee_i_vmsubaw(struct ee_state* ee) { printf("ee: vmsubaw unimplemented\n"); exit(1); }
static inline void ee_i_vmsubax(struct ee_state* ee) { printf("ee: vmsubax unimplemented\n"); exit(1); }
static inline void ee_i_vmsubay(struct ee_state* ee) { printf("ee: vmsubay unimplemented\n"); exit(1); }
static inline void ee_i_vmsubaz(struct ee_state* ee) { printf("ee: vmsubaz unimplemented\n"); exit(1); }
static inline void ee_i_vmsubi(struct ee_state* ee) { printf("ee: vmsubi unimplemented\n"); exit(1); }
static inline void ee_i_vmsubq(struct ee_state* ee) { printf("ee: vmsubq unimplemented\n"); exit(1); }
static inline void ee_i_vmsubw(struct ee_state* ee) { printf("ee: vmsubw unimplemented\n"); exit(1); }
static inline void ee_i_vmsubx(struct ee_state* ee) { printf("ee: vmsubx unimplemented\n"); exit(1); }
static inline void ee_i_vmsuby(struct ee_state* ee) { printf("ee: vmsuby unimplemented\n"); exit(1); }
static inline void ee_i_vmsubz(struct ee_state* ee) { printf("ee: vmsubz unimplemented\n"); exit(1); }
static inline void ee_i_vmtir(struct ee_state* ee) { printf("ee: vmtir unimplemented\n"); exit(1); }
static inline void ee_i_vmul(struct ee_state* ee) { printf("ee: vmul unimplemented\n"); exit(1); }
static inline void ee_i_vmula(struct ee_state* ee) { printf("ee: vmula unimplemented\n"); exit(1); }
static inline void ee_i_vmulai(struct ee_state* ee) { printf("ee: vmulai unimplemented\n"); exit(1); }
static inline void ee_i_vmulaq(struct ee_state* ee) { printf("ee: vmulaq unimplemented\n"); exit(1); }
static inline void ee_i_vmulaw(struct ee_state* ee) { printf("ee: vmulaw unimplemented\n"); exit(1); }
static inline void ee_i_vmulax(struct ee_state* ee) { printf("ee: vmulax unimplemented\n"); exit(1); }
static inline void ee_i_vmulay(struct ee_state* ee) { printf("ee: vmulay unimplemented\n"); exit(1); }
static inline void ee_i_vmulaz(struct ee_state* ee) { printf("ee: vmulaz unimplemented\n"); exit(1); }
static inline void ee_i_vmuli(struct ee_state* ee) { printf("ee: vmuli unimplemented\n"); exit(1); }
static inline void ee_i_vmulq(struct ee_state* ee) { printf("ee: vmulq unimplemented\n"); exit(1); }
static inline void ee_i_vmulw(struct ee_state* ee) { printf("ee: vmulw unimplemented\n"); exit(1); }
static inline void ee_i_vmulx(struct ee_state* ee) { printf("ee: vmulx unimplemented\n"); exit(1); }
static inline void ee_i_vmuly(struct ee_state* ee) { printf("ee: vmuly unimplemented\n"); exit(1); }
static inline void ee_i_vmulz(struct ee_state* ee) { printf("ee: vmulz unimplemented\n"); exit(1); }
static inline void ee_i_vnop(struct ee_state* ee) { printf("ee: vnop unimplemented\n"); exit(1); }
static inline void ee_i_vopmsub(struct ee_state* ee) { printf("ee: vopmsub unimplemented\n"); exit(1); }
static inline void ee_i_vopmula(struct ee_state* ee) { printf("ee: vopmula unimplemented\n"); exit(1); }
static inline void ee_i_vrget(struct ee_state* ee) { printf("ee: vrget unimplemented\n"); exit(1); }
static inline void ee_i_vrinit(struct ee_state* ee) { printf("ee: vrinit unimplemented\n"); exit(1); }
static inline void ee_i_vrnext(struct ee_state* ee) { printf("ee: vrnext unimplemented\n"); exit(1); }
static inline void ee_i_vrsqrt(struct ee_state* ee) { printf("ee: vrsqrt unimplemented\n"); exit(1); }
static inline void ee_i_vrxor(struct ee_state* ee) { printf("ee: vrxor unimplemented\n"); exit(1); }
static inline void ee_i_vsqd(struct ee_state* ee) { printf("ee: vsqd unimplemented\n"); exit(1); }
static inline void ee_i_vsqi(struct ee_state* ee) {
    /* To-do: VU1 */
}
static inline void ee_i_vsqrt(struct ee_state* ee) { printf("ee: vsqrt unimplemented\n"); exit(1); }
static inline void ee_i_vsub(struct ee_state* ee) {
    /* To-do: VU1 */
}
static inline void ee_i_vsuba(struct ee_state* ee) { printf("ee: vsuba unimplemented\n"); exit(1); }
static inline void ee_i_vsubai(struct ee_state* ee) { printf("ee: vsubai unimplemented\n"); exit(1); }
static inline void ee_i_vsubaq(struct ee_state* ee) { printf("ee: vsubaq unimplemented\n"); exit(1); }
static inline void ee_i_vsubaw(struct ee_state* ee) { printf("ee: vsubaw unimplemented\n"); exit(1); }
static inline void ee_i_vsubax(struct ee_state* ee) { printf("ee: vsubax unimplemented\n"); exit(1); }
static inline void ee_i_vsubay(struct ee_state* ee) { printf("ee: vsubay unimplemented\n"); exit(1); }
static inline void ee_i_vsubaz(struct ee_state* ee) { printf("ee: vsubaz unimplemented\n"); exit(1); }
static inline void ee_i_vsubi(struct ee_state* ee) { printf("ee: vsubi unimplemented\n"); exit(1); }
static inline void ee_i_vsubq(struct ee_state* ee) { printf("ee: vsubq unimplemented\n"); exit(1); }
static inline void ee_i_vsubw(struct ee_state* ee) { printf("ee: vsubw unimplemented\n"); exit(1); }
static inline void ee_i_vsubx(struct ee_state* ee) { printf("ee: vsubx unimplemented\n"); exit(1); }
static inline void ee_i_vsuby(struct ee_state* ee) { printf("ee: vsuby unimplemented\n"); exit(1); }
static inline void ee_i_vsubz(struct ee_state* ee) { printf("ee: vsubz unimplemented\n"); exit(1); }
static inline void ee_i_vwaitq(struct ee_state* ee) { printf("ee: vwaitq unimplemented\n"); exit(1); }
static inline void ee_i_xor(struct ee_state* ee) { printf("ee: xor unimplemented\n"); exit(1); }
static inline void ee_i_xori(struct ee_state* ee) {
    EE_RT = EE_RS ^ EE_D_I16;
}

struct ee_state* ee_create(void) {
    return malloc(sizeof(struct ee_state));
}

void ee_init(struct ee_state* ee, struct ee_bus_s bus) {
    memset(ee, 0, sizeof(struct ee_state));

    ee->prid = 0x2e20;
    ee->pc = EE_VEC_RESET;
    ee->next_pc = ee->pc + 4;
    ee->bus = bus;

    // To-do: Set SR

    ee->scratchpad = ps2_ram_create();
    ps2_ram_init(ee->scratchpad, 0x4000);
}

static inline void ee_execute(struct ee_state* ee) {
    switch (ee->opcode & 0xFC000000) {
        case 0x00000000: { // special
            switch (ee->opcode & 0x0000003F) {
                case 0x00000000: ee_i_sll(ee); return;
                case 0x00000002: ee_i_srl(ee); return;
                case 0x00000003: ee_i_sra(ee); return;
                case 0x00000004: ee_i_sllv(ee); return;
                case 0x00000006: ee_i_srlv(ee); return;
                case 0x00000007: ee_i_srav(ee); return;
                case 0x00000008: ee_i_jr(ee); return;
                case 0x00000009: ee_i_jalr(ee); return;
                case 0x0000000A: ee_i_movz(ee); return;
                case 0x0000000B: ee_i_movn(ee); return;
                case 0x0000000C: ee_i_syscall(ee); return;
                case 0x0000000D: ee_i_break(ee); return;
                case 0x0000000F: ee_i_sync(ee); return;
                case 0x00000010: ee_i_mfhi(ee); return;
                case 0x00000011: ee_i_mthi(ee); return;
                case 0x00000012: ee_i_mflo(ee); return;
                case 0x00000013: ee_i_mtlo(ee); return;
                case 0x00000014: ee_i_dsllv(ee); return;
                case 0x00000016: ee_i_dsrlv(ee); return;
                case 0x00000017: ee_i_dsrav(ee); return;
                case 0x00000018: ee_i_mult(ee); return;
                case 0x00000019: ee_i_multu(ee); return;
                case 0x0000001A: ee_i_div(ee); return;
                case 0x0000001B: ee_i_divu(ee); return;
                case 0x00000020: ee_i_add(ee); return;
                case 0x00000021: ee_i_addu(ee); return;
                case 0x00000022: ee_i_sub(ee); return;
                case 0x00000023: ee_i_subu(ee); return;
                case 0x00000024: ee_i_and(ee); return;
                case 0x00000025: ee_i_or(ee); return;
                case 0x00000026: ee_i_xor(ee); return;
                case 0x00000027: ee_i_nor(ee); return;
                case 0x00000028: ee_i_mfsa(ee); return;
                case 0x00000029: ee_i_mtsa(ee); return;
                case 0x0000002A: ee_i_slt(ee); return;
                case 0x0000002B: ee_i_sltu(ee); return;
                case 0x0000002C: ee_i_dadd(ee); return;
                case 0x0000002D: ee_i_daddu(ee); return;
                case 0x0000002E: ee_i_dsub(ee); return;
                case 0x0000002F: ee_i_dsubu(ee); return;
                case 0x00000030: ee_i_tge(ee); return;
                case 0x00000031: ee_i_tgeu(ee); return;
                case 0x00000032: ee_i_tlt(ee); return;
                case 0x00000033: ee_i_tltu(ee); return;
                case 0x00000034: ee_i_teq(ee); return;
                case 0x00000036: ee_i_tne(ee); return;
                case 0x00000038: ee_i_dsll(ee); return;
                case 0x0000003A: ee_i_dsrl(ee); return;
                case 0x0000003B: ee_i_dsra(ee); return;
                case 0x0000003C: ee_i_dsll32(ee); return;
                case 0x0000003E: ee_i_dsrl32(ee); return;
                case 0x0000003F: ee_i_dsra32(ee); return;
            }
        } break;
        case 0x04000000: { // regimm
            switch (ee->opcode & 0x001F0000) {
                case 0x00000000: ee_i_bltz(ee); return;
                case 0x00010000: ee_i_bgez(ee); return;
                case 0x00020000: ee_i_bltzl(ee); return;
                case 0x00030000: ee_i_bgezl(ee); return;
                case 0x00080000: ee_i_tgei(ee); return;
                case 0x00090000: ee_i_tgeiu(ee); return;
                case 0x000A0000: ee_i_tlti(ee); return;
                case 0x000B0000: ee_i_tltiu(ee); return;
                case 0x000C0000: ee_i_teqi(ee); return;
                case 0x000E0000: ee_i_tnei(ee); return;
                case 0x00100000: ee_i_bltzal(ee); return;
                case 0x00110000: ee_i_bgezal(ee); return;
                case 0x00120000: ee_i_bltzall(ee); return;
                case 0x00130000: ee_i_bgezall(ee); return;
                case 0x00180000: ee_i_mtsab(ee); return;
                case 0x00190000: ee_i_mtsah(ee); return;
            }
        } break;
        case 0x08000000: ee_i_j(ee); return;
        case 0x0C000000: ee_i_jal(ee); return;
        case 0x10000000: ee_i_beq(ee); return;
        case 0x14000000: ee_i_bne(ee); return;
        case 0x18000000: ee_i_blez(ee); return;
        case 0x1C000000: ee_i_bgtz(ee); return;
        case 0x20000000: ee_i_addi(ee); return;
        case 0x24000000: ee_i_addiu(ee); return;
        case 0x28000000: ee_i_slti(ee); return;
        case 0x2C000000: ee_i_sltiu(ee); return;
        case 0x30000000: ee_i_andi(ee); return;
        case 0x34000000: ee_i_ori(ee); return;
        case 0x38000000: ee_i_xori(ee); return;
        case 0x3C000000: ee_i_lui(ee); return;
        case 0x40000000: { // cop0
            switch (ee->opcode & 0x03E00000) {
                case 0x00000000: ee_i_mfc0(ee); return;
                case 0x00800000: ee_i_mtc0(ee); return;
                case 0x01000000: {
                    switch (ee->opcode & 0x001F0000) {
                        case 0x00000000: ee_i_bc0f(ee); return;
                        case 0x00010000: ee_i_bc0t(ee); return;
                        case 0x00020000: ee_i_bc0fl(ee); return;
                        case 0x00030000: ee_i_bc0tl(ee); return;
                    }
                } break;
                case 0x02000000: {
                    switch (ee->opcode & 0x0000003F) {
                        case 0x00000001: ee_i_tlbr(ee); return;
                        case 0x00000002: ee_i_tlbwi(ee); return;
                        case 0x00000006: ee_i_tlbwr(ee); return;
                        case 0x00000008: ee_i_tlbp(ee); return;
                        case 0x00000018: ee_i_eret(ee); return;
                        case 0x00000038: ee_i_ei(ee); return;
                        case 0x00000039: ee_i_di(ee); return;
                    }
                } break;
            }
        } break;
        case 0x44000000: { // cop1
            switch (ee->opcode & 0x03E00000) {
                case 0x00000000: ee_i_mfc1(ee); return;
                case 0x00400000: ee_i_cfc1(ee); return;
                case 0x00800000: ee_i_mtc1(ee); return;
                case 0x00C00000: ee_i_ctc1(ee); return;
                case 0x01000000: {
                    switch (ee->opcode & 0x001F0000) {
                        case 0x00000000: ee_i_bc1f(ee); return;
                        case 0x00010000: ee_i_bc1t(ee); return;
                        case 0x00020000: ee_i_bc1fl(ee); return;
                        case 0x00030000: ee_i_bc1tl(ee); return;
                    }
                } break;
                case 0x02000000: {
                    switch (ee->opcode & 0x0000003F) {
                        case 0x00000000: ee_i_adds(ee); return;
                        case 0x00000001: ee_i_subs(ee); return;
                        case 0x00000002: ee_i_muls(ee); return;
                        case 0x00000003: ee_i_divs(ee); return;
                        case 0x00000004: ee_i_sqrts(ee); return;
                        case 0x00000005: ee_i_abss(ee); return;
                        case 0x00000006: ee_i_movs(ee); return;
                        case 0x00000007: ee_i_negs(ee); return;
                        case 0x00000016: ee_i_rsqrts(ee); return;
                        case 0x00000018: ee_i_addas(ee); return;
                        case 0x00000019: ee_i_subas(ee); return;
                        case 0x0000001A: ee_i_mulas(ee); return;
                        case 0x0000001C: ee_i_madds(ee); return;
                        case 0x0000001D: ee_i_msubs(ee); return;
                        case 0x0000001E: ee_i_maddas(ee); return;
                        case 0x0000001F: ee_i_msubas(ee); return;
                        case 0x00000024: ee_i_cvtw(ee); return;
                        case 0x00000028: ee_i_maxs(ee); return;
                        case 0x00000029: ee_i_mins(ee); return;
                        case 0x00000030: ee_i_cf(ee); return;
                        case 0x00000032: ee_i_ceq(ee); return;
                        case 0x00000034: ee_i_clt(ee); return;
                        case 0x00000036: ee_i_cle(ee); return;
                    }
                } break;
                case 0x02800000: {
                    switch (ee->opcode & 0x0000003F) {
                        case 0x00000020: ee_i_cvts(ee); return;
                    }
                } break;
            }
        } break;
        case 0x48000000: { // cop2
            switch (ee->opcode & 0x03E00000) {
                case 0x00200000: ee_i_qmfc2(ee); return;
                case 0x00400000: ee_i_cfc2(ee); return;
                case 0x00A00000: ee_i_qmtc2(ee); return;
                case 0x00C00000: ee_i_ctc2(ee); return;
                case 0x01000000: {
                    switch (ee->opcode & 0x001F0000) {
                        case 0x00000000: ee_i_bc2f(ee); return;
                        case 0x00010000: ee_i_bc2t(ee); return;
                        case 0x00020000: ee_i_bc2fl(ee); return;
                        case 0x00030000: ee_i_bc2tl(ee); return;
                    }
                } break;
                case 0x02000000:
                case 0x02200000:
                case 0x02400000:
                case 0x02600000:
                case 0x02800000:
                case 0x02A00000:
                case 0x02C00000:
                case 0x02E00000:
                case 0x03000000:
                case 0x03200000:
                case 0x03400000:
                case 0x03600000:
                case 0x03800000:
                case 0x03A00000:
                case 0x03C00000:
                case 0x03E00000: {
                    switch (ee->opcode & 0x0000003F) {
                        case 0x00000000: ee_i_vaddx(ee); return;
                        case 0x00000001: ee_i_vaddy(ee); return;
                        case 0x00000002: ee_i_vaddz(ee); return;
                        case 0x00000003: ee_i_vaddw(ee); return;
                        case 0x00000004: ee_i_vsubx(ee); return;
                        case 0x00000005: ee_i_vsuby(ee); return;
                        case 0x00000006: ee_i_vsubz(ee); return;
                        case 0x00000007: ee_i_vsubw(ee); return;
                        case 0x00000008: ee_i_vmaddx(ee); return;
                        case 0x00000009: ee_i_vmaddy(ee); return;
                        case 0x0000000A: ee_i_vmaddz(ee); return;
                        case 0x0000000B: ee_i_vmaddw(ee); return;
                        case 0x0000000C: ee_i_vmsubx(ee); return;
                        case 0x0000000D: ee_i_vmsuby(ee); return;
                        case 0x0000000E: ee_i_vmsubz(ee); return;
                        case 0x0000000F: ee_i_vmsubw(ee); return;
                        case 0x00000010: ee_i_vmaxx(ee); return;
                        case 0x00000011: ee_i_vmaxy(ee); return;
                        case 0x00000012: ee_i_vmaxz(ee); return;
                        case 0x00000013: ee_i_vmaxw(ee); return;
                        case 0x00000014: ee_i_vminix(ee); return;
                        case 0x00000015: ee_i_vminiy(ee); return;
                        case 0x00000016: ee_i_vminiz(ee); return;
                        case 0x00000017: ee_i_vminiw(ee); return;
                        case 0x00000018: ee_i_vmulx(ee); return;
                        case 0x00000019: ee_i_vmuly(ee); return;
                        case 0x0000001A: ee_i_vmulz(ee); return;
                        case 0x0000001B: ee_i_vmulw(ee); return;
                        case 0x0000001C: ee_i_vmulq(ee); return;
                        case 0x0000001D: ee_i_vmaxi(ee); return;
                        case 0x0000001E: ee_i_vmuli(ee); return;
                        case 0x0000001F: ee_i_vminii(ee); return;
                        case 0x00000020: ee_i_vaddq(ee); return;
                        case 0x00000021: ee_i_vmaddq(ee); return;
                        case 0x00000022: ee_i_vaddi(ee); return;
                        case 0x00000023: ee_i_vmaddi(ee); return;
                        case 0x00000024: ee_i_vsubq(ee); return;
                        case 0x00000025: ee_i_vmsubq(ee); return;
                        case 0x00000026: ee_i_vsubi(ee); return;
                        case 0x00000027: ee_i_vmsubi(ee); return;
                        case 0x00000028: ee_i_vadd(ee); return;
                        case 0x00000029: ee_i_vmadd(ee); return;
                        case 0x0000002A: ee_i_vmul(ee); return;
                        case 0x0000002B: ee_i_vmax(ee); return;
                        case 0x0000002C: ee_i_vsub(ee); return;
                        case 0x0000002D: ee_i_vmsub(ee); return;
                        case 0x0000002E: ee_i_vopmsub(ee); return;
                        case 0x0000002F: ee_i_vmini(ee); return;
                        case 0x00000030: ee_i_viadd(ee); return;
                        case 0x00000031: ee_i_visub(ee); return;
                        case 0x00000032: ee_i_viaddi(ee); return;
                        case 0x00000034: ee_i_viand(ee); return;
                        case 0x00000035: ee_i_vior(ee); return;
                        case 0x00000038: ee_i_vcallms(ee); return;
                        case 0x00000039: ee_i_callmsr(ee); return;
                        case 0x0000003C:
                        case 0x0000003D:
                        case 0x0000003E:
                        case 0x0000003F: {
                            uint32_t func = (ee->opcode & 3) | ((ee->opcode & 0x7c0) >> 4);

                            switch (func) {
                                case 0x00000000: ee_i_vaddax(ee); return;
                                case 0x00000001: ee_i_vadday(ee); return;
                                case 0x00000002: ee_i_vaddaz(ee); return;
                                case 0x00000003: ee_i_vaddaw(ee); return;
                                case 0x00000004: ee_i_vsubax(ee); return;
                                case 0x00000005: ee_i_vsubay(ee); return;
                                case 0x00000006: ee_i_vsubaz(ee); return;
                                case 0x00000007: ee_i_vsubaw(ee); return;
                                case 0x00000008: ee_i_vmaddax(ee); return;
                                case 0x00000009: ee_i_vmadday(ee); return;
                                case 0x0000000A: ee_i_vmaddaz(ee); return;
                                case 0x0000000B: ee_i_vmaddaw(ee); return;
                                case 0x0000000C: ee_i_vmsubax(ee); return;
                                case 0x0000000D: ee_i_vmsubay(ee); return;
                                case 0x0000000E: ee_i_vmsubaz(ee); return;
                                case 0x0000000F: ee_i_vmsubaw(ee); return;
                                case 0x00000010: ee_i_vitof0(ee); return;
                                case 0x00000011: ee_i_vitof4(ee); return;
                                case 0x00000012: ee_i_vitof12(ee); return;
                                case 0x00000013: ee_i_vitof15(ee); return;
                                case 0x00000014: ee_i_vftoi0(ee); return;
                                case 0x00000015: ee_i_vftoi4(ee); return;
                                case 0x00000016: ee_i_vftoi12(ee); return;
                                case 0x00000017: ee_i_vftoi15(ee); return;
                                case 0x00000018: ee_i_vmulax(ee); return;
                                case 0x00000019: ee_i_vmulay(ee); return;
                                case 0x0000001A: ee_i_vmulaz(ee); return;
                                case 0x0000001B: ee_i_vmulaw(ee); return;
                                case 0x0000001C: ee_i_vmulaq(ee); return;
                                case 0x0000001D: ee_i_vabs(ee); return;
                                case 0x0000001E: ee_i_vmulai(ee); return;
                                case 0x0000001F: ee_i_vclipw(ee); return;
                                case 0x00000020: ee_i_vaddaq(ee); return;
                                case 0x00000021: ee_i_vmaddaq(ee); return;
                                case 0x00000022: ee_i_vaddai(ee); return;
                                case 0x00000023: ee_i_vmaddai(ee); return;
                                case 0x00000024: ee_i_vsubaq(ee); return;
                                case 0x00000025: ee_i_vmsubaq(ee); return;
                                case 0x00000026: ee_i_vsubai(ee); return;
                                case 0x00000027: ee_i_vmsubai(ee); return;
                                case 0x00000028: ee_i_vadda(ee); return;
                                case 0x00000029: ee_i_vmadda(ee); return;
                                case 0x0000002A: ee_i_vmula(ee); return;
                                case 0x0000002C: ee_i_vsuba(ee); return;
                                case 0x0000002D: ee_i_vmsuba(ee); return;
                                case 0x0000002E: ee_i_vopmula(ee); return;
                                case 0x0000002F: ee_i_vnop(ee); return;
                                case 0x00000030: ee_i_vmove(ee); return;
                                case 0x00000031: ee_i_vmr32(ee); return;
                                case 0x00000034: ee_i_vlqi(ee); return;
                                case 0x00000035: ee_i_vsqi(ee); return;
                                case 0x00000036: ee_i_vlqd(ee); return;
                                case 0x00000037: ee_i_vsqd(ee); return;
                                case 0x00000038: ee_i_vdiv(ee); return;
                                case 0x00000039: ee_i_vsqrt(ee); return;
                                case 0x0000003A: ee_i_vrsqrt(ee); return;
                                case 0x0000003B: ee_i_vwaitq(ee); return;
                                case 0x0000003C: ee_i_vmtir(ee); return;
                                case 0x0000003D: ee_i_vmfir(ee); return;
                                case 0x0000003E: ee_i_vilwr(ee); return;
                                case 0x0000003F: ee_i_viswr(ee); return;
                                case 0x00000040: ee_i_vrnext(ee); return;
                                case 0x00000041: ee_i_vrget(ee); return;
                                case 0x00000042: ee_i_vrinit(ee); return;
                                case 0x00000043: ee_i_vrxor(ee); return;
                            }
                        } break;
                    }
                } break;
            }
        } break;
        case 0x50000000: ee_i_beql(ee); return;
        case 0x54000000: ee_i_bnel(ee); return;
        case 0x58000000: ee_i_blezl(ee); return;
        case 0x5C000000: ee_i_bgtzl(ee); return;
        case 0x60000000: ee_i_daddi(ee); return;
        case 0x64000000: ee_i_daddiu(ee); return;
        case 0x68000000: ee_i_ldl(ee); return;
        case 0x6C000000: ee_i_ldr(ee); return;
        case 0x70000000: { // mmi
            switch (ee->opcode & 0x0000003F) {
                case 0x00000000: ee_i_madd(ee); return;
                case 0x00000001: ee_i_maddu(ee); return;
                case 0x00000004: ee_i_plzcw(ee); return;
                case 0x00000008: {
                    switch (ee->opcode & 0x000007C0) {
                        case 0x00000000: ee_i_paddw(ee); return;
                        case 0x00000040: ee_i_psubw(ee); return;
                        case 0x00000080: ee_i_pcgtw(ee); return;
                        case 0x000000C0: ee_i_pmaxw(ee); return;
                        case 0x00000100: ee_i_paddh(ee); return;
                        case 0x00000140: ee_i_psubh(ee); return;
                        case 0x00000180: ee_i_pcgth(ee); return;
                        case 0x000001C0: ee_i_pmaxh(ee); return;
                        case 0x00000200: ee_i_paddb(ee); return;
                        case 0x00000240: ee_i_psubb(ee); return;
                        case 0x00000280: ee_i_pcgtb(ee); return;
                        case 0x00000400: ee_i_paddsw(ee); return;
                        case 0x00000440: ee_i_psubsw(ee); return;
                        case 0x00000480: ee_i_pextlw(ee); return;
                        case 0x000004C0: ee_i_ppacw(ee); return;
                        case 0x00000500: ee_i_paddsh(ee); return;
                        case 0x00000540: ee_i_psubsh(ee); return;
                        case 0x00000580: ee_i_pextlh(ee); return;
                        case 0x000005C0: ee_i_ppach(ee); return;
                        case 0x00000600: ee_i_paddsb(ee); return;
                        case 0x00000640: ee_i_psubsb(ee); return;
                        case 0x00000680: ee_i_pextlb(ee); return;
                        case 0x000006C0: ee_i_ppacb(ee); return;
                        case 0x00000780: ee_i_pext5(ee); return;
                        case 0x000007C0: ee_i_ppac5(ee); return;
                    }
                } break;
                case 0x00000009: {
                    switch (ee->opcode & 0x000007C0) {
                        case 0x00000000: ee_i_pmaddw(ee); return;
                        case 0x00000080: ee_i_psllvw(ee); return;
                        case 0x000000C0: ee_i_psrlvw(ee); return;
                        case 0x00000100: ee_i_pmsubw(ee); return;
                        case 0x00000200: ee_i_pmfhi(ee); return;
                        case 0x00000240: ee_i_pmflo(ee); return;
                        case 0x00000280: ee_i_pinth(ee); return;
                        case 0x00000300: ee_i_pmultw(ee); return;
                        case 0x00000340: ee_i_pdivw(ee); return;
                        case 0x00000380: ee_i_pcpyld(ee); return;
                        case 0x00000400: ee_i_pmaddh(ee); return;
                        case 0x00000440: ee_i_phmadh(ee); return;
                        case 0x00000480: ee_i_pand(ee); return;
                        case 0x000004C0: ee_i_pxor(ee); return;
                        case 0x00000500: ee_i_pmsubh(ee); return;
                        case 0x00000540: ee_i_phmsbh(ee); return;
                        case 0x00000680: ee_i_pexeh(ee); return;
                        case 0x000006C0: ee_i_prevh(ee); return;
                        case 0x00000700: ee_i_pmulth(ee); return;
                        case 0x00000740: ee_i_pdivbw(ee); return;
                        case 0x00000780: ee_i_pexew(ee); return;
                        case 0x000007C0: ee_i_prot3w(ee); return;
                    }
                } break;
                case 0x00000010: ee_i_mfhi1(ee); return;
                case 0x00000011: ee_i_mthi1(ee); return;
                case 0x00000012: ee_i_mflo1(ee); return;
                case 0x00000013: ee_i_mtlo1(ee); return;
                case 0x00000018: ee_i_mult1(ee); return;
                case 0x00000019: ee_i_multu1(ee); return;
                case 0x0000001A: ee_i_div1(ee); return;
                case 0x0000001B: ee_i_divu1(ee); return;
                case 0x00000020: ee_i_madd1(ee); return;
                case 0x00000021: ee_i_maddu1(ee); return;
                case 0x00000028: {
                    switch (ee->opcode & 0x000007C0) {
                        case 0x00000040: ee_i_pabsw(ee); return;
                        case 0x00000080: ee_i_pceqw(ee); return;
                        case 0x000000C0: ee_i_pminw(ee); return;
                        case 0x00000100: ee_i_padsbh(ee); return;
                        case 0x00000140: ee_i_pabsh(ee); return;
                        case 0x00000180: ee_i_pceqh(ee); return;
                        case 0x000001C0: ee_i_pminh(ee); return;
                        case 0x00000280: ee_i_pceqb(ee); return;
                        case 0x00000400: ee_i_padduw(ee); return;
                        case 0x00000440: ee_i_psubuw(ee); return;
                        case 0x00000480: ee_i_pextuw(ee); return;
                        case 0x00000500: ee_i_padduh(ee); return;
                        case 0x00000540: ee_i_psubuh(ee); return;
                        case 0x00000580: ee_i_pextuh(ee); return;
                        case 0x00000600: ee_i_paddub(ee); return;
                        case 0x00000640: ee_i_psubub(ee); return;
                        case 0x00000680: ee_i_pextub(ee); return;
                        case 0x000006C0: ee_i_qfsrv(ee); return;
                    }
                } break;
                case 0x00000029: {
                    switch (ee->opcode & 0x000007C0) {
                        case 0x00000000: ee_i_pmadduw(ee); return;
                        case 0x000000C0: ee_i_psravw(ee); return;
                        case 0x00000200: ee_i_pmthi(ee); return;
                        case 0x00000240: ee_i_pmtlo(ee); return;
                        case 0x00000280: ee_i_pinteh(ee); return;
                        case 0x00000300: ee_i_pmultuw(ee); return;
                        case 0x00000340: ee_i_pdivuw(ee); return;
                        case 0x00000380: ee_i_pcpyud(ee); return;
                        case 0x00000480: ee_i_por(ee); return;
                        case 0x000004C0: ee_i_pnor(ee); return;
                        case 0x00000680: ee_i_pexch(ee); return;
                        case 0x000006C0: ee_i_pcpyh(ee); return;
                        case 0x00000780: ee_i_pexcw(ee); return;
                    }
                } break;
                case 0x00000030: ee_i_pmfhl(ee); return;
                case 0x00000031: ee_i_pmthl(ee); return;
                case 0x00000034: ee_i_psllh(ee); return;
                case 0x00000036: ee_i_psrlh(ee); return;
                case 0x00000037: ee_i_psrah(ee); return;
                case 0x0000003C: ee_i_psllw(ee); return;
                case 0x0000003E: ee_i_psrlw(ee); return;
                case 0x0000003F: ee_i_psraw(ee); return;
            }
        } break;
        case 0x78000000: ee_i_lq(ee); return;
        case 0x7C000000: ee_i_sq(ee); return;
        case 0x80000000: ee_i_lb(ee); return;
        case 0x84000000: ee_i_lh(ee); return;
        case 0x88000000: ee_i_lwl(ee); return;
        case 0x8C000000: ee_i_lw(ee); return;
        case 0x90000000: ee_i_lbu(ee); return;
        case 0x94000000: ee_i_lhu(ee); return;
        case 0x98000000: ee_i_lwr(ee); return;
        case 0x9C000000: ee_i_lwu(ee); return;
        case 0xA0000000: ee_i_sb(ee); return;
        case 0xA4000000: ee_i_sh(ee); return;
        case 0xA8000000: ee_i_swl(ee); return;
        case 0xAC000000: ee_i_sw(ee); return;
        case 0xB0000000: ee_i_sdl(ee); return;
        case 0xB4000000: ee_i_sdr(ee); return;
        case 0xB8000000: ee_i_swr(ee); return;
        case 0xBC000000: ee_i_cache(ee); return;
        case 0xC4000000: ee_i_lwc1(ee); return;
        case 0xCC000000: ee_i_pref(ee); return;
        case 0xD8000000: ee_i_lqc2(ee); return;
        case 0xDC000000: ee_i_ld(ee); return;
        case 0xE4000000: ee_i_swc1(ee); return;
        case 0xF8000000: ee_i_sqc2(ee); return;
        case 0xFC000000: ee_i_sd(ee); return;
    }

    printf("ee: Invalid instruction %08x @ pc=%08x\n", ee->opcode, ee->pc);
}

static inline void ee_print_disassembly(struct ee_state* ee) {
    char buf[128];
    struct ee_dis_state ds;

    ds.print_address = 1;
    ds.print_opcode = 1;
    ds.pc = ee->pc;

    puts(ee_disassemble(buf, ee->opcode, &ds));
}

static int p = 0;

void ee_cycle(struct ee_state* ee) {
    ee->r[0].ul64 = 0;

    int irq_enabled = (ee->status & EE_SR_IE) && (ee->status & EE_SR_EIE) &&
                      (!(ee->status & EE_SR_EXL)) && (!(ee->status & EE_SR_ERL));
    int int0_pending = (ee->status & EE_SR_IM2) && (ee->cause & EE_CAUSE_IP2);
    int int1_pending = (ee->status & EE_SR_IM3) && (ee->cause & EE_CAUSE_IP3);

    if (irq_enabled && (int0_pending || int1_pending)) {
        ee->pc += 4;

        ee_exception_level1(ee, CAUSE_EXC1_INT);
    }

    ee->opcode = bus_read32(ee, ee->pc);

    // if (p) {
    //     ee_print_disassembly(ee);
    //     --p;
    // }

    ee->pc = ee->next_pc;
    ee->next_pc += 4;

    ee_execute(ee);

    // if (ee->pc == 0x00082000)
    //     p = 1;

    ++ee->count;

    ee->r[0].ul64 = 0;
}

void ee_destroy(struct ee_state* ee) {
    free(ee);
}