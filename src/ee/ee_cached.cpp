#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <math.h>
#include <fenv.h>

#ifdef _EE_USE_INTRINSICS
#include <immintrin.h>
#include <tmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#endif

#include "ee.h"
#include "ee_dis.h"
#include "ee_def.hpp"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#if defined(__has_builtin)
#if __has_builtin(__builtin_saddll_overflow) && __has_builtin(__builtin_saddll_overflow)
#define SADDOVF64 __builtin_saddll_overflow
#define SSUBOVF64 __builtin_ssubll_overflow
#else
#define SADDOVF64 __builtin_saddl_overflow
#define SSUBOVF64 __builtin_ssubl_overflow
#endif
#else
#define SADDOVF64 __builtin_saddll_overflow
#define SSUBOVF64 __builtin_ssubll_overflow
#endif

// file = fopen("vu.dump", "a"); fprintf(file, #ins "\n"); fclose(file);
#define VU_LOWER(ins) { ps2_vu_decode_lower(ee->vu0, i.opcode); vu_i_ ## ins(ee->vu0, &ee->vu0->lower); }
#define VU_UPPER(ins) { ps2_vu_decode_upper(ee->vu0, i.opcode); vu_i_ ## ins(ee->vu0, &ee->vu0->upper); }

static inline int fast_abs32(int a) {
    uint32_t m = a >> 31;

    return (a ^ m) + (m & 1);
}

static inline int16_t fast_abs16(int16_t a) {
    uint16_t m = a >> 15;

    return (a ^ m) + (m & 1);
}

static inline int16_t saturate16(int32_t word) {
    if (word > (int32_t)0x00007FFF) {
        return 0x7FFF;
    } else if (word < (int32_t)0xFFFF80000) {
        return 0x8000;
    } else {
        return (int16_t)word;
    }
}

static inline int32_t saturate32(int64_t word) {
    if (word > (int32_t)0x7FFFFFFF) {
        return 0x7FFFFFFF;
    } else if (word < (int32_t)0x80000000) {
        return 0x80000000;
    } else {
        return (int32_t)word;
    }
}

#ifdef _EE_USE_INTRINSICS
static inline __m128i _mm_adds_epi32(__m128i a, __m128i b) {
    const __m128i m = _mm_set1_epi32(0x7fffffff);
    __m128i r = _mm_add_epi32(a, b);
    __m128i sb = _mm_srli_epi32(a, 31);
    __m128i sat = _mm_add_epi32(m, sb);
    __m128i sx = _mm_xor_si128(a, b);
    __m128i o = _mm_andnot_si128(sx, _mm_xor_si128(a, r));

    // To-do: Use SSE3 version when SSE4.1 isn't available
    return _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(r),
                                          _mm_castsi128_ps(sat),
                                          _mm_castsi128_ps(o)));
}

static inline __m128i _mm_adds_epu32(__m128i a, __m128i b) {
    const __m128i m = _mm_set1_epi32(0xffffffff);

    __m128i x = _mm_xor_si128(a, m);
    __m128i c = _mm_min_epu32(b, x);

    return _mm_add_epi32(a, c);
}
#endif

static inline uint32_t unpack_5551_8888(uint32_t v) {
    return ((v & 0x001f) << 3) |
           ((v & 0x03e0) << 6) |
           ((v & 0x7c00) << 9) |
           ((v & 0x8000) << 16);
}

#define EE_KUSEG 0
#define EE_KSEG0 1
#define EE_KSEG1 2
#define EE_KSSEG 3
#define EE_KSEG3 4

/*
    i.rs = (opcode >> 21) & 0x1f;
    i.rt = (opcode >> 16) & 0x1f;
    i.rd = (opcode >> 11) & 0x1f;
    i.fd = (opcode >> 6) & 0x1f;
    i.i15 = (opcode >> 6) & 0x7fff;
    i.i16 = opcode & 0xffff;
    i.i26 = opcode & 0x3ffffff;
*/
#define EE_D_RS (i.rs)
#define EE_D_FS (i.rd)
#define EE_D_RT (i.rt)
#define EE_D_RD (i.rd)
#define EE_D_FD (i.sa)
#define EE_D_SA (i.sa)
#define EE_D_I15 (i.i15)
#define EE_D_I16 (i.i16)
#define EE_D_I26 (i.i26)
#define EE_D_SI26 ((int32_t)(EE_D_I26 << 6) >> 4)
#define EE_D_SI16 ((int32_t)(EE_D_I16 << 16) >> 14)

#define EE_RT ee->r[EE_D_RT].ul64
#define EE_RD ee->r[EE_D_RD].ul64
#define EE_RS ee->r[EE_D_RS].ul64
#define EE_RT32 ee->r[EE_D_RT].ul32
#define EE_RD32 ee->r[EE_D_RD].ul32
#define EE_RS32 ee->r[EE_D_RS].ul32
#define EE_FD ee->f[EE_D_FD].f
#define EE_FT (fpu_cvtf(ee->f[EE_D_RT].f))
#define EE_FS (fpu_cvtf(ee->f[EE_D_FS].f))
#define EE_FT32 ee->f[EE_D_RT].u32
#define EE_FD32 ee->f[EE_D_FD].u32
#define EE_FS32 ee->f[EE_D_FS].u32

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
    BRANCH(cond, offset) else { ee->exception = 1; ee->pc += 4; ee->next_pc += 4; }

#define SE6432(v) ((int64_t)((int32_t)(v)))
#define SE6416(v) ((int64_t)((int16_t)(v)))
#define SE648(v) ((int64_t)((int8_t)(v)))
#define SE3216(v) ((int32_t)((int16_t)(v)))

static inline void ee_print_disassembly(struct ee_state* ee, const ee_instruction& i) {
    char buf[128];
    struct ee_dis_state ds;

    ds.print_address = 1;
    ds.print_opcode = 1;
    ds.pc = ee->pc;

    puts(ee_disassemble(buf, i.opcode, &ds));
}

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

const uint32_t ee_bus_region_mask_table[] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0x7fffffff, 0x1fffffff, 0xffffffff, 0xffffffff
};

static inline float fpu_cvtf(float f) {
    uint32_t u32 = *(uint32_t*)&f;

    switch (u32 & 0x7f800000) {
        case 0x0: {
            u32 &= 0x80000000;

            return *(float*)&u32;
        } break;

        case 0x7f800000: {
            uint32_t result = (u32 & 0x80000000) | 0x7f7fffff;

            return *(float*)&result;
        }
    }

    return *(float*)&u32;
}

static inline float fpu_cvtsw(union ee_fpu_reg* reg) {
    switch (reg->u32 & 0x7F800000) {
        case 0x0: {
            reg->u32 &= 0x80000000;
        } break;

        case 0x7F800000: {
            reg->u32 = (reg->u32 & 0x80000000) | 0x7F7FFFFF;
        } break;
    }

    return reg->f;
}

static inline void fpu_cvtws(union ee_fpu_reg* d, union ee_fpu_reg* s) {
    if ((s->u32 & 0x7F800000) <= 0x4E800000)
        d->s32 = (int32_t)fpu_cvtf(s->f);
    else if ((s->u32 & 0x80000000) == 0)
        d->u32 = 0x7FFFFFFF;
    else
        d->u32 = 0x80000000;
}

static inline int fpu_check_overflow(struct ee_state* ee, union ee_fpu_reg* reg) {
    if ((reg->u32 & ~0x80000000) == 0x7f800000) {
        reg->u32 = (reg->u32 & 0x80000000) | 0x7f7fffff;
        ee->fcr |= FPU_FLG_O | FPU_FLG_SO;

        return 1;
    }

    ee->fcr &= ~FPU_FLG_O;

    return 0;
}

static inline int fpu_check_underflow(struct ee_state* ee, union ee_fpu_reg* reg) {
    if (((reg->u32 & 0x7F800000) == 0) && ((reg->u32 & 0x007FFFFF) != 0)) {
        reg->u32 &= 0x80000000;
        ee->fcr |= FPU_FLG_U | FPU_FLG_SU;

        return 1;
	}

    ee->fcr &= ~FPU_FLG_U;

    return 0;
}

static inline int fpu_check_overflow_no_flags(struct ee_state* ee, union ee_fpu_reg* reg) {
    if ((reg->u32 & ~0x80000000) == 0x7f800000) {
        reg->u32 = (reg->u32 & 0x80000000) | 0x7f7fffff;

        return 1;
    }

    return 0;
}

static inline int fpu_check_underflow_no_flags(struct ee_state* ee, union ee_fpu_reg* reg) {
    if (((reg->u32 & 0x7F800000) == 0) && ((reg->u32 & 0x007FFFFF) != 0)) {
        reg->u32 &= 0x80000000;

        return 1;
	}

    return 0;
}

static inline int fpu_max(int32_t a, int32_t b) {
    return (a < 0 && b < 0) ? min(a, b) : max(a, b);
}

static inline int fpu_min(int32_t a, int32_t b) {
    return (a < 0 && b < 0) ? max(a, b) : min(a, b);
}

static inline struct ee_vtlb_entry* ee_search_vtlb(struct ee_state* ee, uint32_t virt) {
    struct ee_vtlb_entry* entry = NULL;

    for (int i = 0; i < 48; i++) {
        ee->vtlb[i].mask;
    }
}

static inline int ee_translate_virt(struct ee_state* ee, uint32_t virt, uint32_t* phys) {
    int seg = ee_get_segment(virt);

    // Assume we're in kernel mode
    if (seg == EE_KSEG0 || seg == EE_KSEG1) {
        *phys = virt & 0x1fffffff;

        return 0;
    }

    if (virt >= 0x00000000 && virt <= 0x01FFFFFF) {
        *phys = virt & 0x1FFFFFF;

        return 0;
    }

    if (virt >= 0x10000000 && virt <= 0x1FFFFFFF) {
        *phys = virt & 0x1FFFFFFF;

        return 0;
    }

    if (virt >= 0x20000000 && virt <= 0x21FFFFFF) {
        *phys = virt & 0x1FFFFFF;

        return 0;
    }

    if (virt >= 0x30000000 && virt <= 0x31FFFFFF) {
        *phys = virt & 0x1FFFFFF;

        return 0;
    }

    // DECI2 area
    if (virt >= 0xFFFF8000) {
        *phys = (virt - 0xFFFF8000) + 0x78000;

        return 0;
    }

    *phys = virt & ee_bus_region_mask_table[virt >> 29];

    // printf("ee: Unhandled virtual address %08x @ cyc=%ld pc=%08x\n", virt, ee->total_cycles, ee->pc);

    // *(int*)0 = 0;

    // exit(1);

    // To-do: MMU mapping
    *phys = virt & 0x1fffffff;

    return 0;
}

#define BUS_READ_FUNC(b)                                                        \
    static inline uint64_t bus_read ## b(struct ee_state* ee, uint32_t addr) {  \
        if ((addr & 0xf0000000) == 0x70000000)                                  \
            return ps2_ram_read ## b(ee->scratchpad, addr & 0x3fff);            \
        uint32_t phys;                                                          \
        ee_translate_virt(ee, addr, &phys);                                     \
        if (phys == 0x1000f000) ee->intc_reads++;                               \
        if (phys == 0x12001000) ee->csr_reads++;                                \
        return ee->bus.read ## b(ee->bus.udata, phys);                          \
    }

#define BUS_WRITE_FUNC(b)                                                                   \
    static inline void bus_write ## b(struct ee_state* ee, uint32_t addr, uint64_t data) {  \
        if ((addr & 0xf0000000) == 0x70000000)                                              \
            { ps2_ram_write ## b(ee->scratchpad, addr & 0x3fff, data); return; }            \
        uint32_t phys;                                                                      \
        ee_translate_virt(ee, addr, &phys);                                                 \
        ee->bus.write ## b(ee->bus.udata, phys, data);                                      \
    }

BUS_READ_FUNC(8)
BUS_READ_FUNC(16)
BUS_READ_FUNC(32)
BUS_READ_FUNC(64)

static inline uint128_t bus_read128(struct ee_state* ee, uint32_t addr) {
    if ((addr & 0xf0000000) == 0x70000000)
        return ps2_ram_read128(ee->scratchpad, addr & 0x3ff0);

    uint32_t phys;

    ee_translate_virt(ee, addr, &phys);

    return ee->bus.read128(ee->bus.udata, phys);
}

BUS_WRITE_FUNC(8)
BUS_WRITE_FUNC(16)
BUS_WRITE_FUNC(32)
BUS_WRITE_FUNC(64)

static inline void bus_write128(struct ee_state* ee, uint32_t addr, uint128_t data) {
    if ((addr & 0xf0000000) == 0x70000000) {
        ps2_ram_write128(ee->scratchpad, addr & 0x3ff0, data);

        return;
    }

    uint32_t phys;

    ee_translate_virt(ee, addr, &phys);

    ee->bus.write128(ee->bus.udata, phys, data);
}

#undef BUS_READ_FUNC
#undef BUS_WRITE_FUNC

static inline int ee_skip_fmv(struct ee_state* ee, uint32_t addr) {
    if (bus_read32(ee, addr + 4) != 0x03E00008)
        return 0;

    uint32_t code = bus_read32(ee, addr);
    uint32_t p1 = 0x8c800040;
    uint32_t p2 = 0x8c020000 | (code & 0x1f0000) << 5;

    if ((code & 0xffe0ffff) != p1) {
        return 0;
    }

    if (bus_read32(ee, addr + 8) != p2) {
        return 0;
    }

    printf("ee: Skipping FMV\n");

    return 1;
}

static inline void ee_set_pc(struct ee_state* ee, uint32_t addr) {
    if (ee->fmv_skip) {
        if (ee_skip_fmv(ee, addr)) return;
    }

    ee->pc = addr;
    ee->next_pc = addr + 4;
}

static inline void ee_set_pc_delayed(struct ee_state* ee, uint32_t addr) {
    if (ee->fmv_skip) {
        if (ee_skip_fmv(ee, addr)) return;
    }

    ee->next_pc = addr;
    ee->branch = 1;
}

void ee_exception_level1(struct ee_state* ee, uint32_t cause) {
    ee->exception = 1;

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

static inline int ee_check_irq(struct ee_state* ee) {
    int irq_enabled = (ee->status & EE_SR_IE) && (ee->status & EE_SR_EIE) &&
        (!(ee->status & EE_SR_EXL)) && (!(ee->status & EE_SR_ERL));
    int int0_pending = (ee->status & EE_SR_IM2) && (ee->cause & EE_CAUSE_IP2);
    int int1_pending = (ee->status & EE_SR_IM3) && (ee->cause & EE_CAUSE_IP3);

    if (irq_enabled && (int0_pending || int1_pending)) {
        ee->pc += 4;

        // printf("ee: Handling irq at pc=%08x (int0=%d (%d) int1=%d (%d)) sr=%08x delay_slot=%d\n",
        //     ee->pc,
        //     int0_pending, !!(ee->status & EE_SR_IM2),
        //     int1_pending, !!(ee->status & EE_SR_IM3),
        //     ee->status,
        //     ee->delay_slot
        // );

        ee->intc_reads = 0;

        ee_exception_level1(ee, CAUSE_EXC1_INT);

        return 1;
    }

    return 0;
}

void ee_set_int0(struct ee_state* ee, int v) {
    if (v) {
        ee->cause |= EE_CAUSE_IP2;
    } else {
        ee->cause &= ~EE_CAUSE_IP2;
    }
}

void ee_set_int1(struct ee_state* ee, int v) {
    if (v) {
        ee->cause |= EE_CAUSE_IP3;
    } else {
        ee->cause &= ~EE_CAUSE_IP3;
    }
}

void ee_set_cpcond0(struct ee_state* ee, int v) {
    ee->cpcond0 = v;
}

static inline void ee_i_abss(struct ee_state* ee, const ee_instruction& i) {
    ee->f[EE_D_FD].u32 = ee->f[EE_D_FS].u32 & 0x7fffffff;
    // EE_FD = fabsf(EE_FS);
}
static inline void ee_i_add(struct ee_state* ee, const ee_instruction& i) {
    int32_t s = EE_RS;
    int32_t t = EE_RT;

    int32_t r = s + t;
    uint32_t o = (s ^ r) & (t ^ r);

    if (o & 0x80000000) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RD = SE6432(r);
    }
}
static inline void ee_i_addas(struct ee_state* ee, const ee_instruction& i) {
    ee->a.f = EE_FS + EE_FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void ee_i_addi(struct ee_state* ee, const ee_instruction& i) {
    int32_t s = EE_RS;
    int32_t t = SE3216(EE_D_I16);
    int32_t r;

    if (__builtin_sadd_overflow(s, t, &r)) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RT = SE6432(r);
    }
}
static inline void ee_i_addiu(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE6432(EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_adds(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_FD;

    ee->f[d].f = EE_FS + EE_FT;

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void ee_i_addu(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = SE6432(EE_RS + EE_RT);
}
static inline void ee_i_and(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RS & EE_RT;
}
static inline void ee_i_andi(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = EE_RS & EE_D_I16;
}
static inline void ee_i_bc0f(struct ee_state* ee, const ee_instruction& i) {
    BRANCH(!ee->cpcond0, EE_D_SI16);
}
static inline void ee_i_bc0fl(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY(!ee->cpcond0, EE_D_SI16);
}
static inline void ee_i_bc0t(struct ee_state* ee, const ee_instruction& i) {
    BRANCH(ee->cpcond0, EE_D_SI16);
}
static inline void ee_i_bc0tl(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY(ee->cpcond0, EE_D_SI16);
}
static inline void ee_i_bc1f(struct ee_state* ee, const ee_instruction& i) {
    BRANCH((ee->fcr & FPU_FLG_C) == 0, EE_D_SI16);
}
static inline void ee_i_bc1fl(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY((ee->fcr & FPU_FLG_C) == 0, EE_D_SI16);
}
static inline void ee_i_bc1t(struct ee_state* ee, const ee_instruction& i) {
    BRANCH((ee->fcr & FPU_FLG_C) != 0, EE_D_SI16);
}
static inline void ee_i_bc1tl(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY((ee->fcr & FPU_FLG_C) != 0, EE_D_SI16);
}
static inline void ee_i_bc2f(struct ee_state* ee, const ee_instruction& i) { BRANCH(1, EE_D_SI16); }
static inline void ee_i_bc2fl(struct ee_state* ee, const ee_instruction& i) { BRANCH_LIKELY(1, EE_D_SI16); }
static inline void ee_i_bc2t(struct ee_state* ee, const ee_instruction& i) { BRANCH(0, EE_D_SI16); }
static inline void ee_i_bc2tl(struct ee_state* ee, const ee_instruction& i) { BRANCH_LIKELY(0, EE_D_SI16); }
static inline void ee_i_beq(struct ee_state* ee, const ee_instruction& i) {
    BRANCH(EE_RS == EE_RT, EE_D_SI16);
}
static inline void ee_i_beql(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY(EE_RS == EE_RT, EE_D_SI16);
}
static inline void ee_i_bgez(struct ee_state* ee, const ee_instruction& i) {
    BRANCH((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgezal(struct ee_state* ee, const ee_instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgezall(struct ee_state* ee, const ee_instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH_LIKELY((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgezl(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgtz(struct ee_state* ee, const ee_instruction& i) {
    BRANCH((int64_t)EE_RS > (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgtzl(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY((int64_t)EE_RS > (int64_t)0, EE_D_SI16);
}
static inline void ee_i_blez(struct ee_state* ee, const ee_instruction& i) {
    BRANCH((int64_t)EE_RS <= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_blezl(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY((int64_t)EE_RS <= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltz(struct ee_state* ee, const ee_instruction& i) {
    BRANCH((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltzal(struct ee_state* ee, const ee_instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltzall(struct ee_state* ee, const ee_instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH_LIKELY((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltzl(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bne(struct ee_state* ee, const ee_instruction& i) {
    BRANCH(EE_RS != EE_RT, EE_D_SI16);
}
static inline void ee_i_bnel(struct ee_state* ee, const ee_instruction& i) {
    BRANCH_LIKELY(EE_RS != EE_RT, EE_D_SI16);
}
static inline void ee_i_break(struct ee_state* ee, const ee_instruction& i) {
    ee_exception_level1(ee, CAUSE_EXC1_BP);
}
static inline void ee_i_cache(struct ee_state* ee, const ee_instruction& i) {
    /* To-do: Cache emulation */
} 
static inline void ee_i_ceq(struct ee_state* ee, const ee_instruction& i) {
    if (EE_FS == EE_FT) {
        ee->fcr |= FPU_FLG_C;
    } else {
        ee->fcr &= ~FPU_FLG_C;
    }
}
static inline void ee_i_cf(struct ee_state* ee, const ee_instruction& i) {
    ee->fcr &= ~FPU_FLG_C; 
}
static inline void ee_i_cfc1(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE6432((EE_D_FS >= 16) ? ee->fcr : 0x2e30);
}
static inline void ee_i_cfc2(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE6432(ps2_vu_read_vi(ee->vu0, EE_D_RD));
}
static inline void ee_i_cle(struct ee_state* ee, const ee_instruction& i) {
    if (EE_FS <= EE_FT) {
        ee->fcr |= FPU_FLG_C;
    } else {
        ee->fcr &= ~FPU_FLG_C;
    }
}
static inline void ee_i_clt(struct ee_state* ee, const ee_instruction& i) {
    if (EE_FS < EE_FT) {
        ee->fcr |= FPU_FLG_C;
    } else {
        ee->fcr &= ~FPU_FLG_C;
    }
}
static inline void ee_i_ctc1(struct ee_state* ee, const ee_instruction& i) {
    if (EE_D_FS < 16)
        return;

    ee->fcr = EE_RT32; // (ee->fcr & ~(0x83c078)) | (EE_RT & 0x83c078);
}
static inline void ee_i_ctc2(struct ee_state* ee, const ee_instruction& i) {
    // To-do: Handle FBRST, VPU_STAT, CMSAR1
    int d = EE_D_RD;

    static const char* regs[] = {
        "Status flag",
        "MAC flag",
        "clipping flag",
        "reserved",
        "R",
        "I",
        "Q",
        "reserved",
        "reserved",
        "reserved",
        "TPC",
        "CMSAR0",
        "FBRST",
        "VPU-STAT",
        "reserved",
        "CMSAR1",
    };

    ps2_vu_write_vi(ee->vu0, d, EE_RT32);
}
static inline void ee_i_cvts(struct ee_state* ee, const ee_instruction& i) {
    EE_FD = (float)ee->f[EE_D_FS].s32;
    EE_FD = fpu_cvtsw(&ee->f[EE_D_FD]);
}
static inline void ee_i_cvtw(struct ee_state* ee, const ee_instruction& i) {
    fpu_cvtws(&ee->f[EE_D_FD], &ee->f[EE_D_FS]);
}
static inline void ee_i_dadd(struct ee_state* ee, const ee_instruction& i) {
    long long r;

    if (SADDOVF64((int64_t)EE_RS, (int64_t)EE_RT, &r)) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RD = r;
    }
}
static inline void ee_i_daddi(struct ee_state* ee, const ee_instruction& i) {
    long long r;

    if (SADDOVF64((int64_t)EE_RS, SE6416(EE_D_I16), &r)) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RT = r;
    }
}
static inline void ee_i_daddiu(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = EE_RS + SE6416(EE_D_I16);
}
static inline void ee_i_daddu(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RS + EE_RT;
}
static inline void ee_i_di(struct ee_state* ee, const ee_instruction& i) {
    int edi = ee->status & EE_SR_EDI;
    int exl = ee->status & EE_SR_EXL;
    int erl = ee->status & EE_SR_ERL;
    int ksu = ee->status & EE_SR_KSU;
    
    if (edi || exl || erl || !ksu)
        ee->status &= ~EE_SR_EIE;
}
static inline void ee_i_div(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int s = EE_D_RS;

	if (ee->r[s].ul32 == 0x80000000 && ee->r[t].ul32 == 0xffffffff) {
        EE_LO0 = (int32_t)0x80000000;
        EE_HI0 = 0;
	} else if (ee->r[t].ul32 != 0) {
        EE_HI0 = SE6432(ee->r[s].sl32 % ee->r[t].sl32);
        EE_LO0 = SE6432(ee->r[s].sl32 / ee->r[t].sl32);
    } else {
        EE_HI0 = SE6432(ee->r[s].ul32);
        EE_LO0 = ((int32_t)ee->r[s].ul32 < 0) ? 1 : -1;
	}
}
static inline void ee_i_div1(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int s = EE_D_RS;

	if (ee->r[s].ul32 == 0x80000000 && ee->r[t].ul32 == 0xffffffff) {
        EE_LO1 = (int32_t)0x80000000;
        EE_HI1 = 0;
	} else if (ee->r[t].ul32 != 0) {
        EE_HI1 = SE6432(ee->r[s].sl32 % ee->r[t].sl32);
        EE_LO1 = SE6432(ee->r[s].sl32 / ee->r[t].sl32);
    } else {
        EE_HI1 = SE6432(ee->r[s].ul32);
        EE_LO1 = ((int32_t)ee->r[s].ul32 < 0) ? 1 : -1;
	}
}
static inline void ee_i_divs(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int d = EE_D_FD;
    int s = EE_D_FS;

    ee->fcr &= ~(FPU_FLG_I | FPU_FLG_D);

    // If both the dividend and divisor are zero, set I/SI,
    // else set D/SD
	if ((ee->f[t].u32 & 0x7F800000) == 0) {
        if ((ee->f[s].u32 & 0x7F800000) == 0) {
            ee->fcr |= FPU_FLG_I | FPU_FLG_SI;
        } else {
            ee->fcr |= FPU_FLG_D | FPU_FLG_SD;
        }

		ee->f[d].u32 = ((ee->f[t].u32 ^ ee->f[s].u32) & 0x80000000) | 0x7f7fffff;

        return;
	}

    ee->f[d].f = EE_FS / EE_FT;

    if (fpu_check_overflow_no_flags(ee, &ee->f[d]))
        return;

    fpu_check_underflow_no_flags(ee, &ee->f[d]);
}
static inline void ee_i_divu(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int s = EE_D_RS;

    if (!ee->r[t].ul32) {
        EE_LO0 = -1;
        EE_HI0 = SE6432(ee->r[s].ul32);

        return;
    }

    EE_HI0 = SE6432(ee->r[s].ul32 % ee->r[t].ul32);
    EE_LO0 = SE6432(ee->r[s].ul32 / ee->r[t].ul32);
}
static inline void ee_i_divu1(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int s = EE_D_RS;

    if (!ee->r[t].ul32) {
        EE_LO1 = -1;
        EE_HI1 = SE6432(ee->r[s].ul32);

        return;
    }

    EE_HI1 = SE6432(ee->r[s].ul32 % ee->r[t].ul32);
    EE_LO1 = SE6432(ee->r[s].ul32 / ee->r[t].ul32);
}
static inline void ee_i_dsll(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RT << EE_D_SA;
}
static inline void ee_i_dsll32(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RT << (EE_D_SA + 32);
}
static inline void ee_i_dsllv(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RT << (EE_RS & 0x3f);
}
static inline void ee_i_dsra(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = ((int64_t)EE_RT) >> EE_D_SA;
}
static inline void ee_i_dsra32(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = ((int64_t)EE_RT) >> (EE_D_SA + 32);
}
static inline void ee_i_dsrav(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = ((int64_t)EE_RT) >> (EE_RS & 0x3f);
}
static inline void ee_i_dsrl(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RT >> EE_D_SA;
}
static inline void ee_i_dsrl32(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RT >> (EE_D_SA + 32);
}
static inline void ee_i_dsrlv(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RT >> (EE_RS & 0x3f);
}
static inline void ee_i_dsub(struct ee_state* ee, const ee_instruction& i) {
    long long r;

    if (SSUBOVF64((int64_t)EE_RS, (int64_t)EE_RT, &r)) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RD = r;
    }
}
static inline void ee_i_dsubu(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RS - EE_RT;
}
static inline void ee_i_ei(struct ee_state* ee, const ee_instruction& i) {
    int edi = ee->status & EE_SR_EDI;
    int exl = ee->status & EE_SR_EXL;
    int erl = ee->status & EE_SR_ERL;
    int ksu = ee->status & EE_SR_KSU;
    
    if (edi || exl || erl || !ksu)
        ee->status |= EE_SR_EIE;
}
static inline void ee_i_eret(struct ee_state* ee, const ee_instruction& i) {
    if (ee->status & EE_SR_ERL) {
        ee_set_pc(ee, ee->errorepc);

        ee->status &= ~EE_SR_ERL;
    } else {
        ee_set_pc(ee, ee->epc);

        ee->status &= ~EE_SR_EXL;
    }
}
static inline void ee_i_j(struct ee_state* ee, const ee_instruction& i) {
    ee_set_pc_delayed(ee, (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2));
}
static inline void ee_i_jal(struct ee_state* ee, const ee_instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    ee_set_pc_delayed(ee, (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2));
}
static inline void ee_i_jalr(struct ee_state* ee, const ee_instruction& i) {
    uint32_t next_pc = ee->next_pc;

    ee_set_pc_delayed(ee, EE_RS32);

    EE_RD = next_pc;
}
static inline void ee_i_jr(struct ee_state* ee, const ee_instruction& i) {
    ee_set_pc_delayed(ee, EE_RS32);
}
static inline void ee_i_lb(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE648(bus_read8(ee, EE_RS32 + SE3216(EE_D_I16)));
}
static inline void ee_i_lbu(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = bus_read8(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_ld(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = bus_read64(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_ldl(struct ee_state* ee, const ee_instruction& i) {
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
static inline void ee_i_ldr(struct ee_state* ee, const ee_instruction& i) {
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
static inline void ee_i_lh(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE6416(bus_read16(ee, EE_RS32 + SE3216(EE_D_I16)));
}
static inline void ee_i_lhu(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = bus_read16(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_lq(struct ee_state* ee, const ee_instruction& i) {
    ee->r[EE_D_RT] = bus_read128(ee, (EE_RS32 + SE3216(EE_D_I16)) & ~0xf);
}
static inline void ee_i_lqc2(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RT;

    if (!d) return;

    ee->vu0->vf[EE_D_RT].u128 = bus_read128(ee, (EE_RS32 + SE3216(EE_D_I16)) & ~0xf);
}
static inline void ee_i_lui(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE6432(EE_D_I16 << 16);
}
static inline void ee_i_lw(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE6432(bus_read32(ee, EE_RS32 + SE3216(EE_D_I16)));
}
static inline void ee_i_lwc1(struct ee_state* ee, const ee_instruction& i) {
    EE_FT32 = bus_read32(ee, EE_RS32 + SE3216(EE_D_I16));
}

static const uint32_t LWL_MASK[4] = { 0x00ffffff, 0x0000ffff, 0x000000ff, 0x00000000 };
static const uint32_t LWR_MASK[4] = { 0x00000000, 0xff000000, 0xffff0000, 0xffffff00 };
static const int LWL_SHIFT[4] = { 24, 16, 8, 0 };
static const int LWR_SHIFT[4] = { 0, 8, 16, 24 };

static inline void ee_i_lwl(struct ee_state* ee, const ee_instruction& i) {
    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t shift = addr & 3;
    uint32_t mem = bus_read32(ee, addr & ~3);

    // ensure the compiler does correct sign extension into 64 bits by using s32
    EE_RT = (int32_t)((EE_RT32 & LWL_MASK[shift]) | (mem << LWL_SHIFT[shift]));
}

static inline void ee_i_lwr(struct ee_state* ee, const ee_instruction& i) {
    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t shift = addr & 3;
    uint32_t data = bus_read32(ee, addr & ~3);

    // Use unsigned math here, and conditionally sign extend below, when needed.
    data = (EE_RT32 & LWR_MASK[shift]) | (data >> LWR_SHIFT[shift]);

    if (!shift) {
        // This special case requires sign extension into the full 64 bit dest.
        EE_RT = (int32_t)data;
    } else {
        // This case sets the lower 32 bits of the target register.  Upper
        // 32 bits are always preserved.
        EE_RT32 = data;
    }

    // printf("lwr mem=%08x reg=%016lx addr=%08x shift=%d\n", data, ee->r[EE_D_RT].u64[0], addr, shift);
}
static inline void ee_i_lwu(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = bus_read32(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_madd(struct ee_state* ee, const ee_instruction& i) {
    uint64_t r = SE6432(EE_RS32) * SE6432(EE_RT32);
    uint64_t d = (uint64_t)ee->lo.u32[0] | (ee->hi.u64[0] << 32);

    d += r;

    EE_LO0 = SE6432(d & 0xffffffff);
    EE_HI0 = SE6432(d >> 32);

    EE_RD = EE_LO0;
}
static inline void ee_i_madd1(struct ee_state* ee, const ee_instruction& i) {
    uint64_t r = SE6432(EE_RS32) * SE6432(EE_RT32);
    uint64_t d = (EE_LO1 & 0xffffffff) | (EE_HI1 << 32);

    d += r;

    EE_LO1 = SE6432(d & 0xffffffff);
    EE_HI1 = SE6432(d >> 32);

    EE_RD = EE_LO1;
}
static inline void ee_i_maddas(struct ee_state* ee, const ee_instruction& i) {
    ee->a.f += EE_FS * EE_FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void ee_i_madds(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
	int d = EE_D_FD;
    int s = EE_D_FS;

    float temp = fpu_cvtf(ee->f[s].f) * fpu_cvtf(ee->f[t].f);

	ee->f[d].f = fpu_cvtf(ee->a.f) + fpu_cvtf(temp);

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void ee_i_maddu(struct ee_state* ee, const ee_instruction& i) {
    uint64_t r = (uint64_t)EE_RS32 * (uint64_t)EE_RT32;
    uint64_t d = (uint64_t)ee->lo.u32[0] | (ee->hi.u64[0] << 32);

    d += r;

    EE_LO0 = SE6432(d & 0xffffffff);
    EE_HI0 = SE6432(d >> 32);

    EE_RD = EE_LO0;
}
static inline void ee_i_maddu1(struct ee_state* ee, const ee_instruction& i) {
    uint64_t r = (uint64_t)EE_RS32 * (uint64_t)EE_RT32;
    uint64_t d = (uint64_t)ee->lo.u32[2] | (ee->hi.u64[1] << 32);

    d += r;

    EE_LO1 = SE6432(d & 0xffffffff);
    EE_HI1 = SE6432(d >> 32);

    EE_RD = EE_LO1;
}
static inline void ee_i_maxs(struct ee_state* ee, const ee_instruction& i) {
    ee->f[EE_D_FD].u32 = fpu_max(ee->f[EE_D_FS].u32, ee->f[EE_D_RT].u32);

    ee->fcr &= ~(FPU_FLG_O | FPU_FLG_U);
}
static inline void ee_i_mfc0(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE6432(ee->cop0_r[EE_D_RD]);
}
static inline void ee_i_mfc1(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = SE6432(EE_FS32);
}
static inline void ee_i_mfhi(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_HI0;
}
static inline void ee_i_mfhi1(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_HI1;
}
static inline void ee_i_mflo(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_LO0;
}
static inline void ee_i_mflo1(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_LO1;
}
static inline void ee_i_mfsa(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = ee->sa & 0xf;
}
static inline void ee_i_mins(struct ee_state* ee, const ee_instruction& i) {
    ee->f[EE_D_FD].u32 = fpu_min(ee->f[EE_D_FS].u32, ee->f[EE_D_RT].u32);

    ee->fcr &= ~(FPU_FLG_O | FPU_FLG_U);
}
static inline void ee_i_movn(struct ee_state* ee, const ee_instruction& i) {
    if (EE_RT) EE_RD = EE_RS;
}
static inline void ee_i_movs(struct ee_state* ee, const ee_instruction& i) {
    EE_FD32 = EE_FS32;
}
static inline void ee_i_movz(struct ee_state* ee, const ee_instruction& i) {
    if (!EE_RT) EE_RD = EE_RS;
}
static inline void ee_i_msubas(struct ee_state* ee, const ee_instruction& i) {
    ee->a.f -= EE_FS * EE_FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void ee_i_msubs(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
	int d = EE_D_FD;
    int s = EE_D_FS;

    float temp = fpu_cvtf(ee->f[s].f) * fpu_cvtf(ee->f[t].f);

	ee->f[d].f = fpu_cvtf(ee->a.f) - fpu_cvtf(temp);

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void ee_i_mtc0(struct ee_state* ee, const ee_instruction& i) {
    ee->cop0_r[EE_D_RD] = EE_RT32;
}
static inline void ee_i_mtc1(struct ee_state* ee, const ee_instruction& i) {
    EE_FS32 = EE_RT32;
}
static inline void ee_i_mthi(struct ee_state* ee, const ee_instruction& i) {
    EE_HI0 = EE_RS;
}
static inline void ee_i_mthi1(struct ee_state* ee, const ee_instruction& i) {
    EE_HI1 = EE_RS;
}
static inline void ee_i_mtlo(struct ee_state* ee, const ee_instruction& i) {
    EE_LO0 = EE_RS;
}
static inline void ee_i_mtlo1(struct ee_state* ee, const ee_instruction& i) {
    EE_LO1 = EE_RS;
}
static inline void ee_i_mtsa(struct ee_state* ee, const ee_instruction& i) {
    ee->sa = ((uint32_t)EE_RS) & 0xf;
}
static inline void ee_i_mtsab(struct ee_state* ee, const ee_instruction& i) {
    ee->sa = (EE_RS ^ EE_D_I16) & 15;
}
static inline void ee_i_mtsah(struct ee_state* ee, const ee_instruction& i) {
    ee->sa = ((EE_RS ^ EE_D_I16) & 7) << 1;
}
static inline void ee_i_mulas(struct ee_state* ee, const ee_instruction& i) {
    ee->a.f = EE_FS * EE_FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void ee_i_muls(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_FD;

    ee->f[d].f = EE_FS * EE_FT;

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void ee_i_mult(struct ee_state* ee, const ee_instruction& i) {
    uint64_t r = SE6432(EE_RS32) * SE6432(EE_RT32);

    EE_LO0 = SE6432(r & 0xffffffff);
    EE_HI0 = SE6432(r >> 32);

    EE_RD = EE_LO0;
}
static inline void ee_i_mult1(struct ee_state* ee, const ee_instruction& i) {
    uint64_t r = SE6432(EE_RS32) * SE6432(EE_RT32);

    EE_LO1 = SE6432(r & 0xffffffff);
    EE_HI1 = SE6432(r >> 32);

    EE_RD = EE_LO1;
}
static inline void ee_i_multu(struct ee_state* ee, const ee_instruction& i) {
    uint64_t r = (uint64_t)EE_RS32 * (uint64_t)EE_RT32;

    EE_LO0 = SE6432(r & 0xffffffff);
    EE_HI0 = SE6432(r >> 32);

    EE_RD = EE_LO0;
}
static inline void ee_i_multu1(struct ee_state* ee, const ee_instruction& i) {
    uint64_t r = (uint64_t)EE_RS32 * (uint64_t)EE_RT32;

    EE_LO1 = SE6432(r & 0xffffffff);
    EE_HI1 = SE6432(r >> 32);

    EE_RD = EE_LO1;
}
static inline void ee_i_negs(struct ee_state* ee, const ee_instruction& i) {
    ee->f[EE_D_FD].u32 = ee->f[EE_D_FS].u32 ^ 0x80000000;

    ee->fcr &= ~(FPU_FLG_O | FPU_FLG_U);
}
static inline void ee_i_nor(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = ~(EE_RS | EE_RT);
}
static inline void ee_i_or(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RS | EE_RT;
}
static inline void ee_i_ori(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = EE_RS | EE_D_I16;
}
static inline void ee_i_pabsh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 8; i++) {
        d->u16[i] = (t->u16[i] == 0x8000) ? 0x7fff : fast_abs16(t->u16[i]);
    }
#else
    __m128i b = _mm_set1_epi16((unsigned short)0x8000);
    __m128i a = _mm_load_si128((void*)t);
    __m128i f = _mm_cmpeq_epi16(a, b);
    __m128i r = _mm_add_epi16(_mm_abs_epi16(a), f);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pabsw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 4; i++) {
        d->u32[i] = (t->u32[i] == 0x80000000) ? 0x7fffffff : fast_abs32(t->u32[i]);
    }
#else
    __m128i b = _mm_set1_epi32(0x80000000);
    __m128i a = _mm_load_si128((void*)t);
    __m128i f = _mm_cmpeq_epi32(a, b);
    __m128i r = _mm_add_epi32(_mm_abs_epi32(a), f);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_paddb(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 16; i++) {
        d->u8[i] = s->u8[i] + t->u8[i];
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_add_epi8(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_paddh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 8; i++) {
        d->u16[i] = s->u16[i] + t->u16[i];
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_add_epi16(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_paddsb(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 16; i++) {
        int32_t r = ((int32_t)(int8_t)s->u8[i]) + ((int32_t)(int8_t)t->u8[i]);
        d->u8[i] = (r > 0x7f) ? 0x7f : ((r < -128) ? 0x80 : r);
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_adds_epi8(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_paddsh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 8; i++) {
        int32_t r = (SE3216(s->u16[i])) + (SE3216(t->u16[i]));
        d->u16[i] = (r > 0x7fff) ? 0x7fff : ((r < -0x8000) ? 0x8000 : r);
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_adds_epi16(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_paddsw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 4; i++) {
        int64_t r = (SE6432(s->u32[i])) + (SE6432(t->u32[i]));
        d->u32[i] = (r >= 0x7fffffff) ? 0x7fffffff : ((r < (int32_t)0x80000000) ? 0x80000000 : r);
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_adds_epi32(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_paddub(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 16; i++) {
        uint32_t r = (uint32_t)s->u8[i] + (uint32_t)t->u8[i];
        d->u8[i] = (r > 0xff) ? 0xff : r;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_adds_epu8(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_padduh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 8; i++) {
        uint32_t r = (uint32_t)s->u16[i] + (uint32_t)t->u16[i];
        d->u16[i] = (r > 0xffff) ? 0xffff : r;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_adds_epu16(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_padduw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 4; i++) {
        uint64_t r = (uint64_t)s->u32[i] + (uint64_t)t->u32[i];
        d->u32[i] = (r > 0xffffffff) ? 0xffffffff : r;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_adds_epu32(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_paddw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 4; i++) {
        d->u32[i] = s->u32[i] + t->u32[i];
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_add_epi32(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_padsbh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    d->u16[0] = s->u16[0] - t->u16[0];
    d->u16[1] = s->u16[1] - t->u16[1];
    d->u16[2] = s->u16[2] - t->u16[2];
    d->u16[3] = s->u16[3] - t->u16[3];
    d->u16[4] = s->u16[4] + t->u16[4];
    d->u16[5] = s->u16[5] + t->u16[5];
    d->u16[6] = s->u16[6] + t->u16[6];
    d->u16[7] = s->u16[7] + t->u16[7];
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i x = _mm_sub_epi16(a, b);
    __m128i y = _mm_add_epi16(a, b);
    __m128i r = _mm_blend_epi16(x, y, 15);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pand(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    d->u64[0] = s->u64[0] & t->u64[0];
    d->u64[1] = s->u64[1] & t->u64[1];
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_and_si128(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pceqb(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 16; i++) {
        d->u8[i] = (s->u8[i] == t->u8[i]) ? 0xff : 0;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_cmpeq_epi8(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pceqh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 8; i++) {
        d->u16[i] = (s->u16[i] == t->u16[i]) ? 0xffff : 0;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_cmpeq_epi16(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pceqw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 4; i++) {
        d->u32[i] = (s->u32[i] == t->u32[i]) ? 0xffffffff : 0;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_cmpeq_epi32(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pcgtb(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 16; i++) {
        d->u8[i] = ((int8_t)s->u8[i] > (int8_t)t->u8[i]) ? 0xff : 0;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_cmpgt_epi8(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pcgth(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 8; i++) {
        d->u16[i] = ((int16_t)s->u16[i] > (int16_t)t->u16[i]) ? 0xffff : 0;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_cmpgt_epi16(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pcgtw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* s = &ee->r[EE_D_RS];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    for (int i = 0; i < 4; i++) {
        d->u32[i] = ((int32_t)s->u32[i] > (int32_t)t->u32[i]) ? 0xffffffff : 0;
    }
#else
    __m128i a = _mm_load_si128((void*)s);
    __m128i b = _mm_load_si128((void*)t);
    __m128i r = _mm_cmpgt_epi32(a, b);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pcpyh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t* d = &ee->r[EE_D_RD];
    uint128_t* t = &ee->r[EE_D_RT];

#ifndef _EE_USE_INTRINSICS
    uint128_t tc = *t;

    d->u16[0] = tc.u16[0];
    d->u16[1] = tc.u16[0];
    d->u16[2] = tc.u16[0];
    d->u16[3] = tc.u16[0];
    d->u16[4] = tc.u16[4];
    d->u16[5] = tc.u16[4];
    d->u16[6] = tc.u16[4];
    d->u16[7] = tc.u16[4];
#else
    static const uint64_t mask[] = {
        0x0100010001000100,
        0x0908090809080908
    };

    __m128i m = _mm_load_si128((void*)mask);
    __m128i a = _mm_load_si128((void*)t);
    __m128i r = _mm_shuffle_epi8(a, m);

    _mm_store_si128((void*)d, r);
#endif
}
static inline void ee_i_pcpyld(struct ee_state* ee, const ee_instruction& i) {
#ifndef _EE_USE_INTRINSICS
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u64[0] = rt.u64[0];
    ee->r[d].u64[1] = rs.u64[0];
#else
    __m128i a = _mm_load_si128((void*)&ee->r[EE_D_RT]);
    __m128i b = _mm_load_si128((void*)&ee->r[EE_D_RS]);
    __m128i r = _mm_unpacklo_epi64(a, b);

    _mm_store_si128((void*)&ee->r[EE_D_RD], r);
#endif
}
static inline void ee_i_pcpyud(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u64[0] = rs.u64[1];
    ee->r[d].u64[1] = rt.u64[1];
}
static inline void ee_i_pdivbw(struct ee_state* ee, const ee_instruction& i) {
    int s = EE_D_RS;
    int t = EE_D_RT;

    for (int i = 0; i < 4; i++) {
        if (ee->r[s].u32[i] == 0x80000000 && ee->r[t].u16[0] == 0xffff) {
            ee->lo.u32[i] = 0x80000000;
            ee->hi.u32[i] = 0;
        } else if (ee->r[t].u16[0] != 0) {
            ee->lo.u32[i] = ee->r[s].s32[i] / ee->r[t].s16[0];
            ee->hi.u32[i] = ee->r[s].s32[i] % ee->r[t].s16[0];
        } else {
            if (ee->r[s].s32[i] < 0) {
                ee->lo.u32[i] = 1;
            } else {
                ee->lo.u32[i] = -1;
            }

            ee->hi.u32[i] = ee->r[s].s32[i];
        }
    }

    // ee->hi.u32[0] = SE3216(ee->r[s].s32[0] % ee->r[t].s16[0]);
    // ee->hi.u32[1] = SE3216(ee->r[s].s32[1] % ee->r[t].s16[0]);
    // ee->hi.u32[2] = SE3216(ee->r[s].s32[2] % ee->r[t].s16[0]);
    // ee->hi.u32[3] = SE3216(ee->r[s].s32[3] % ee->r[t].s16[0]);
    // ee->lo.u32[0] = ee->r[s].s32[0] / ee->r[t].s16[0];
    // ee->lo.u32[1] = ee->r[s].s32[1] / ee->r[t].s16[0];
    // ee->lo.u32[2] = ee->r[s].s32[2] / ee->r[t].s16[0];
    // ee->lo.u32[3] = ee->r[s].s32[3] / ee->r[t].s16[0];
}
static inline void ee_i_pdivuw(struct ee_state* ee, const ee_instruction& i) {
    int s = EE_D_RS;
    int t = EE_D_RT;

    for (int i = 0; i < 4; i += 2) {
        if (ee->r[t].u32[i] != 0) {
            ee->lo.u64[i/2] = SE6432(ee->r[s].u32[i] / ee->r[t].u32[i]);
            ee->hi.u64[i/2] = SE6432(ee->r[s].u32[i] % ee->r[t].u32[i]);
        } else {
            ee->lo.u64[i/2] = (int64_t)-1;
            ee->hi.u64[i/2] = (int64_t)ee->r[s].s32[i];
        }
    }
}
static inline void ee_i_pdivw(struct ee_state* ee, const ee_instruction& i) {
    int s = EE_D_RS;
    int t = EE_D_RT;

    for (int i = 0; i < 4; i += 2) {
        if (ee->r[s].u32[i] == 0x80000000 && ee->r[t].u32[i] == 0xffffffff) {
            ee->lo.u64[i/2] = (int64_t)(int32_t)0x80000000;
            ee->hi.u64[i/2] = 0;
        } else if (ee->r[t].u32[i] != 0) {
            ee->lo.u64[i/2] = SE6432(ee->r[s].s32[i] / ee->r[t].s32[i]);
            ee->hi.u64[i/2] = SE6432(ee->r[s].s32[i] % ee->r[t].s32[i]);
        } else {
            if (ee->r[s].s32[i] < 0) {
                ee->lo.u64[i/2] = 1;
            } else {
                ee->lo.u64[i/2] = (int64_t)-1;
            }

            ee->hi.u64[i/2] = (int64_t)ee->r[s].s32[i];
        }
    }

    // ee->hi.u64[0] = SE6432(ee->r[s].s32[0] % ee->r[t].s32[0]);
    // ee->hi.u64[1] = SE6432(ee->r[s].s32[2] % ee->r[t].s32[2]);
    // ee->lo.u64[0] = SE6432(ee->r[s].s32[0] / ee->r[t].s32[0]);
    // ee->lo.u64[1] = SE6432(ee->r[s].s32[2] / ee->r[t].s32[2]);
}
static inline void ee_i_pexch(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rt.u16[2];
    ee->r[d].u16[2] = rt.u16[1];
    ee->r[d].u16[3] = rt.u16[3];
    ee->r[d].u16[4] = rt.u16[4];
    ee->r[d].u16[5] = rt.u16[6];
    ee->r[d].u16[6] = rt.u16[5];
    ee->r[d].u16[7] = rt.u16[7];
}
static inline void ee_i_pexcw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rt.u32[1];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void ee_i_pexeh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u16[0] = rt.u16[2];
    ee->r[d].u16[1] = rt.u16[1];
    ee->r[d].u16[2] = rt.u16[0];
    ee->r[d].u16[3] = rt.u16[3];
    ee->r[d].u16[4] = rt.u16[6];
    ee->r[d].u16[5] = rt.u16[5];
    ee->r[d].u16[6] = rt.u16[4];
    ee->r[d].u16[7] = rt.u16[7];
}
static inline void ee_i_pexew(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[2];
    ee->r[d].u32[1] = rt.u32[1];
    ee->r[d].u32[2] = rt.u32[0];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void ee_i_pext5(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = unpack_5551_8888(rt.u32[0]);
    ee->r[d].u32[1] = unpack_5551_8888(rt.u32[1]);
    ee->r[d].u32[2] = unpack_5551_8888(rt.u32[2]);
    ee->r[d].u32[3] = unpack_5551_8888(rt.u32[3]);
}
static inline void ee_i_pextlb(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u8[ 0] = rt.u8[0];
    ee->r[d].u8[ 1] = rs.u8[0];
    ee->r[d].u8[ 2] = rt.u8[1];
    ee->r[d].u8[ 3] = rs.u8[1];
    ee->r[d].u8[ 4] = rt.u8[2];
    ee->r[d].u8[ 5] = rs.u8[2];
    ee->r[d].u8[ 6] = rt.u8[3];
    ee->r[d].u8[ 7] = rs.u8[3];
    ee->r[d].u8[ 8] = rt.u8[4];
    ee->r[d].u8[ 9] = rs.u8[4];
    ee->r[d].u8[10] = rt.u8[5];
    ee->r[d].u8[11] = rs.u8[5];
    ee->r[d].u8[12] = rt.u8[6];
    ee->r[d].u8[13] = rs.u8[6];
    ee->r[d].u8[14] = rt.u8[7];
    ee->r[d].u8[15] = rs.u8[7];
}
static inline void ee_i_pextlh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rs.u16[0];
    ee->r[d].u16[2] = rt.u16[1];
    ee->r[d].u16[3] = rs.u16[1];
    ee->r[d].u16[4] = rt.u16[2];
    ee->r[d].u16[5] = rs.u16[2];
    ee->r[d].u16[6] = rt.u16[3];
    ee->r[d].u16[7] = rs.u16[3];
}
static inline void ee_i_pextlw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rs.u32[0];
    ee->r[d].u32[2] = rt.u32[1];
    ee->r[d].u32[3] = rs.u32[1];
}
static inline void ee_i_pextub(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u8[ 0] = rt.u8[ 8];
    ee->r[d].u8[ 1] = rs.u8[ 8];
    ee->r[d].u8[ 2] = rt.u8[ 9];
    ee->r[d].u8[ 3] = rs.u8[ 9];
    ee->r[d].u8[ 4] = rt.u8[10];
    ee->r[d].u8[ 5] = rs.u8[10];
    ee->r[d].u8[ 6] = rt.u8[11];
    ee->r[d].u8[ 7] = rs.u8[11];
    ee->r[d].u8[ 8] = rt.u8[12];
    ee->r[d].u8[ 9] = rs.u8[12];
    ee->r[d].u8[10] = rt.u8[13];
    ee->r[d].u8[11] = rs.u8[13];
    ee->r[d].u8[12] = rt.u8[14];
    ee->r[d].u8[13] = rs.u8[14];
    ee->r[d].u8[14] = rt.u8[15];
    ee->r[d].u8[15] = rs.u8[15];
}
static inline void ee_i_pextuh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u16[0] = rt.u16[4];
    ee->r[d].u16[1] = rs.u16[4];
    ee->r[d].u16[2] = rt.u16[5];
    ee->r[d].u16[3] = rs.u16[5];
    ee->r[d].u16[4] = rt.u16[6];
    ee->r[d].u16[5] = rs.u16[6];
    ee->r[d].u16[6] = rt.u16[7];
    ee->r[d].u16[7] = rs.u16[7];
}
static inline void ee_i_pextuw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[2];
    ee->r[d].u32[1] = rs.u32[2];
    ee->r[d].u32[2] = rt.u32[3];
    ee->r[d].u32[3] = rs.u32[3];
}
static inline void ee_i_phmadh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    int32_t r0 = (int32_t)(rs.s16[1] * rt.s16[1]);
    int32_t r1 = (int32_t)(rs.s16[3] * rt.s16[3]);
    int32_t r2 = (int32_t)(rs.s16[5] * rt.s16[5]);
    int32_t r3 = (int32_t)(rs.s16[7] * rt.s16[7]);

    ee->r[d].u32[0] = r0 + ((int16_t)rs.u16[0] * (int16_t)rt.u16[0]);
    ee->r[d].u32[1] = r1 + ((int16_t)rs.u16[2] * (int16_t)rt.u16[2]);
    ee->r[d].u32[2] = r2 + ((int16_t)rs.u16[4] * (int16_t)rt.u16[4]);
    ee->r[d].u32[3] = r3 + ((int16_t)rs.u16[6] * (int16_t)rt.u16[6]);
    ee->lo.u32[0] = ee->r[d].u32[0];
    ee->lo.u32[1] = r0;
    ee->hi.u32[0] = ee->r[d].u32[1];
    ee->hi.u32[1] = r1;
    ee->lo.u32[2] = ee->r[d].u32[2];
    ee->lo.u32[3] = r2;
    ee->hi.u32[2] = ee->r[d].u32[3];
    ee->hi.u32[3] = r3;
}
static inline void ee_i_phmsbh(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    int32_t r1 = ee->r[s].s16[1] * ee->r[t].s16[1];
    int32_t r3 = ee->r[s].s16[3] * ee->r[t].s16[3];
    int32_t r5 = ee->r[s].s16[5] * ee->r[t].s16[5];
    int32_t r7 = ee->r[s].s16[7] * ee->r[t].s16[7];

    ee->r[d].u32[0] = (int32_t)(r1 - (ee->r[s].s16[0] * ee->r[t].s16[0]));
    ee->r[d].u32[1] = (int32_t)(r3 - (ee->r[s].s16[2] * ee->r[t].s16[2]));
    ee->r[d].u32[2] = (int32_t)(r5 - (ee->r[s].s16[4] * ee->r[t].s16[4]));
    ee->r[d].u32[3] = (int32_t)(r7 - (ee->r[s].s16[6] * ee->r[t].s16[6]));
    ee->lo.u32[0] = ee->r[d].u32[0];
    ee->lo.u32[1] = ~r1;
    ee->hi.u32[0] = ee->r[d].u32[1];
    ee->hi.u32[1] = ~r3;
    ee->lo.u32[2] = ee->r[d].u32[2];
    ee->lo.u32[3] = ~r5;
    ee->hi.u32[2] = ee->r[d].u32[3];
    ee->hi.u32[3] = ~r7;
}
static inline void ee_i_pinteh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rs.u16[0];
    ee->r[d].u16[2] = rt.u16[2];
    ee->r[d].u16[3] = rs.u16[2];
    ee->r[d].u16[4] = rt.u16[4];
    ee->r[d].u16[5] = rs.u16[4];
    ee->r[d].u16[6] = rt.u16[6];
    ee->r[d].u16[7] = rs.u16[6];
}
static inline void ee_i_pinth(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rs.u16[4];
    ee->r[d].u16[2] = rt.u16[1];
    ee->r[d].u16[3] = rs.u16[5];
    ee->r[d].u16[4] = rt.u16[2];
    ee->r[d].u16[5] = rs.u16[6];
    ee->r[d].u16[6] = rt.u16[3];
    ee->r[d].u16[7] = rs.u16[7];
}
static inline void ee_i_plzcw(struct ee_state* ee, const ee_instruction& i) { 
    for (int j = 0; j < 2; j++) {
        uint32_t word = ee->r[EE_D_RS].u32[j];

        int msb = word & 0x80000000;

        word = (msb ? ~word : word);

        ee->r[EE_D_RD].u32[j] = (word ? (__builtin_clz(word) - 1) : 0x1f);
    }
}
static inline void ee_i_pmaddh(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    uint32_t r0 = SE3216(ee->r[s].u16[0]) * SE3216(ee->r[t].u16[0]);
    uint32_t r1 = SE3216(ee->r[s].u16[1]) * SE3216(ee->r[t].u16[1]);
    uint32_t r2 = SE3216(ee->r[s].u16[2]) * SE3216(ee->r[t].u16[2]);
    uint32_t r3 = SE3216(ee->r[s].u16[3]) * SE3216(ee->r[t].u16[3]);
    uint32_t r4 = SE3216(ee->r[s].u16[4]) * SE3216(ee->r[t].u16[4]);
    uint32_t r5 = SE3216(ee->r[s].u16[5]) * SE3216(ee->r[t].u16[5]);
    uint32_t r6 = SE3216(ee->r[s].u16[6]) * SE3216(ee->r[t].u16[6]);
    uint32_t r7 = SE3216(ee->r[s].u16[7]) * SE3216(ee->r[t].u16[7]);
    uint32_t c0 = ee->lo.u32[0];
    uint32_t c1 = ee->lo.u32[1];
    uint32_t c2 = ee->hi.u32[0];
    uint32_t c3 = ee->hi.u32[1];
    uint32_t c4 = ee->lo.u32[2];
    uint32_t c5 = ee->lo.u32[3];
    uint32_t c6 = ee->hi.u32[2];
    uint32_t c7 = ee->hi.u32[3];

    ee->r[d].u32[0] = r0 + c0;
    ee->lo.u32[1] = r1 + c1;
    ee->r[d].u32[1] = r2 + c2;
    ee->hi.u32[1] = r3 + c3;
    ee->r[d].u32[2] = r4 + c4;
    ee->lo.u32[3] = r5 + c5;
    ee->r[d].u32[3] = r6 + c6;
    ee->hi.u32[3] = r7 + c7;

    ee->lo.u32[0] = ee->r[d].u32[0];
    ee->hi.u32[0] = ee->r[d].u32[1];
    ee->lo.u32[2] = ee->r[d].u32[2];
    ee->hi.u32[2] = ee->r[d].u32[3];
}
static inline void ee_i_pmadduw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    uint64_t r0 = (uint64_t)ee->r[s].u32[0] * (uint64_t)ee->r[t].u32[0];
    uint64_t r1 = (uint64_t)ee->r[s].u32[2] * (uint64_t)ee->r[t].u32[2];

    ee->r[d].u64[0] = r0 + ((ee->hi.u64[0] << 32) | (uint64_t)ee->lo.u32[0]);
    ee->r[d].u64[1] = r1 + ((ee->hi.u64[1] << 32) | (uint64_t)ee->lo.u32[2]);
    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void ee_i_pmaddw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    uint64_t r0 = (int64_t)ee->r[s].s32[0] * (int64_t)ee->r[t].s32[0];
    uint64_t r1 = (int64_t)ee->r[s].s32[2] * (int64_t)ee->r[t].s32[2];

    ee->r[d].u64[0] = r0 + ((((uint64_t)ee->hi.u32[0]) << 32) | (uint64_t)ee->lo.u32[0]);
    ee->r[d].u64[1] = r1 + ((((uint64_t)ee->hi.u32[2]) << 32) | (uint64_t)ee->lo.u32[2]);
    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void ee_i_pmaxh(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u16[0] = ((int16_t)ee->r[s].u16[0] > (int16_t)ee->r[t].u16[0]) ? ee->r[s].u16[0] : ee->r[t].u16[0];
    ee->r[d].u16[1] = ((int16_t)ee->r[s].u16[1] > (int16_t)ee->r[t].u16[1]) ? ee->r[s].u16[1] : ee->r[t].u16[1];
    ee->r[d].u16[2] = ((int16_t)ee->r[s].u16[2] > (int16_t)ee->r[t].u16[2]) ? ee->r[s].u16[2] : ee->r[t].u16[2];
    ee->r[d].u16[3] = ((int16_t)ee->r[s].u16[3] > (int16_t)ee->r[t].u16[3]) ? ee->r[s].u16[3] : ee->r[t].u16[3];
    ee->r[d].u16[4] = ((int16_t)ee->r[s].u16[4] > (int16_t)ee->r[t].u16[4]) ? ee->r[s].u16[4] : ee->r[t].u16[4];
    ee->r[d].u16[5] = ((int16_t)ee->r[s].u16[5] > (int16_t)ee->r[t].u16[5]) ? ee->r[s].u16[5] : ee->r[t].u16[5];
    ee->r[d].u16[6] = ((int16_t)ee->r[s].u16[6] > (int16_t)ee->r[t].u16[6]) ? ee->r[s].u16[6] : ee->r[t].u16[6];
    ee->r[d].u16[7] = ((int16_t)ee->r[s].u16[7] > (int16_t)ee->r[t].u16[7]) ? ee->r[s].u16[7] : ee->r[t].u16[7];
}
static inline void ee_i_pmaxw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u32[0] = ((int32_t)ee->r[s].u32[0] > (int32_t)ee->r[t].u32[0]) ? ee->r[s].u32[0] : ee->r[t].u32[0];
    ee->r[d].u32[1] = ((int32_t)ee->r[s].u32[1] > (int32_t)ee->r[t].u32[1]) ? ee->r[s].u32[1] : ee->r[t].u32[1];
    ee->r[d].u32[2] = ((int32_t)ee->r[s].u32[2] > (int32_t)ee->r[t].u32[2]) ? ee->r[s].u32[2] : ee->r[t].u32[2];
    ee->r[d].u32[3] = ((int32_t)ee->r[s].u32[3] > (int32_t)ee->r[t].u32[3]) ? ee->r[s].u32[3] : ee->r[t].u32[3];
}
static inline void ee_i_pmfhi(struct ee_state* ee, const ee_instruction& i) {
    ee->r[EE_D_RD] = ee->hi;
}
static inline void ee_i_pmfhllw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;

    ee->r[d].u32[0] = ee->lo.u32[0];
    ee->r[d].u32[1] = ee->hi.u32[0];
    ee->r[d].u32[2] = ee->lo.u32[2];
    ee->r[d].u32[3] = ee->hi.u32[2];
}
static inline void ee_i_pmfhluw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;

    ee->r[d].u32[0] = ee->lo.u32[1];
    ee->r[d].u32[1] = ee->hi.u32[1];
    ee->r[d].u32[2] = ee->lo.u32[3];
    ee->r[d].u32[3] = ee->hi.u32[3];
}
static inline void ee_i_pmfhlslw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;

    ee->r[d].u64[0] = SE6432(saturate32(((uint64_t)ee->lo.u32[0]) | (ee->hi.u64[0] << 32)));
    ee->r[d].u64[1] = SE6432(saturate32(((uint64_t)ee->lo.u32[2]) | (ee->hi.u64[1] << 32)));
}
static inline void ee_i_pmfhllh(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;

    ee->r[d].u16[0] = ee->lo.u16[0];
    ee->r[d].u16[1] = ee->lo.u16[2];
    ee->r[d].u16[2] = ee->hi.u16[0];
    ee->r[d].u16[3] = ee->hi.u16[2];
    ee->r[d].u16[4] = ee->lo.u16[4];
    ee->r[d].u16[5] = ee->lo.u16[6];
    ee->r[d].u16[6] = ee->hi.u16[4];
    ee->r[d].u16[7] = ee->hi.u16[6];
    
}
static inline void ee_i_pmfhlsh(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;

    ee->r[d].u16[0] = saturate16(ee->lo.u32[0]);
    ee->r[d].u16[1] = saturate16(ee->lo.u32[1]);
    ee->r[d].u16[2] = saturate16(ee->hi.u32[0]);
    ee->r[d].u16[3] = saturate16(ee->hi.u32[1]);
    ee->r[d].u16[4] = saturate16(ee->lo.u32[2]);
    ee->r[d].u16[5] = saturate16(ee->lo.u32[3]);
    ee->r[d].u16[6] = saturate16(ee->hi.u32[2]);
    ee->r[d].u16[7] = saturate16(ee->hi.u32[3]);
}
static inline void ee_i_pmflo(struct ee_state* ee, const ee_instruction& i) {
    ee->r[EE_D_RD] = ee->lo;
}
static inline void ee_i_pminh(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u16[0] = ((int16_t)ee->r[s].u16[0] < (int16_t)ee->r[t].u16[0]) ? ee->r[s].u16[0] : ee->r[t].u16[0];
    ee->r[d].u16[1] = ((int16_t)ee->r[s].u16[1] < (int16_t)ee->r[t].u16[1]) ? ee->r[s].u16[1] : ee->r[t].u16[1];
    ee->r[d].u16[2] = ((int16_t)ee->r[s].u16[2] < (int16_t)ee->r[t].u16[2]) ? ee->r[s].u16[2] : ee->r[t].u16[2];
    ee->r[d].u16[3] = ((int16_t)ee->r[s].u16[3] < (int16_t)ee->r[t].u16[3]) ? ee->r[s].u16[3] : ee->r[t].u16[3];
    ee->r[d].u16[4] = ((int16_t)ee->r[s].u16[4] < (int16_t)ee->r[t].u16[4]) ? ee->r[s].u16[4] : ee->r[t].u16[4];
    ee->r[d].u16[5] = ((int16_t)ee->r[s].u16[5] < (int16_t)ee->r[t].u16[5]) ? ee->r[s].u16[5] : ee->r[t].u16[5];
    ee->r[d].u16[6] = ((int16_t)ee->r[s].u16[6] < (int16_t)ee->r[t].u16[6]) ? ee->r[s].u16[6] : ee->r[t].u16[6];
    ee->r[d].u16[7] = ((int16_t)ee->r[s].u16[7] < (int16_t)ee->r[t].u16[7]) ? ee->r[s].u16[7] : ee->r[t].u16[7];
}
static inline void ee_i_pminw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u32[0] = (ee->r[s].s32[0] < ee->r[t].s32[0]) ? ee->r[s].u32[0] : ee->r[t].u32[0];
    ee->r[d].u32[1] = (ee->r[s].s32[1] < ee->r[t].s32[1]) ? ee->r[s].u32[1] : ee->r[t].u32[1];
    ee->r[d].u32[2] = (ee->r[s].s32[2] < ee->r[t].s32[2]) ? ee->r[s].u32[2] : ee->r[t].u32[2];
    ee->r[d].u32[3] = (ee->r[s].s32[3] < ee->r[t].s32[3]) ? ee->r[s].u32[3] : ee->r[t].u32[3];
}
static inline void ee_i_pmsubh(struct ee_state* ee, const ee_instruction& i) {
    int s = EE_D_RS;
    int t = EE_D_RT;
    int d = EE_D_RD;

    int32_t r0 = (int32_t)ee->r[s].s16[0] * (int32_t)ee->r[t].s16[0];
    int32_t r1 = (int32_t)ee->r[s].s16[1] * (int32_t)ee->r[t].s16[1];
    int32_t r2 = (int32_t)ee->r[s].s16[2] * (int32_t)ee->r[t].s16[2];
    int32_t r3 = (int32_t)ee->r[s].s16[3] * (int32_t)ee->r[t].s16[3];
    int32_t r4 = (int32_t)ee->r[s].s16[4] * (int32_t)ee->r[t].s16[4];
    int32_t r5 = (int32_t)ee->r[s].s16[5] * (int32_t)ee->r[t].s16[5];
    int32_t r6 = (int32_t)ee->r[s].s16[6] * (int32_t)ee->r[t].s16[6];
    int32_t r7 = (int32_t)ee->r[s].s16[7] * (int32_t)ee->r[t].s16[7];

    ee->r[d].u32[0] = ee->lo.u32[0] - r0;
    ee->r[d].u32[1] = ee->hi.u32[0] - r2;
    ee->r[d].u32[2] = ee->lo.u32[2] - r4;
    ee->r[d].u32[3] = ee->hi.u32[2] - r6;
    ee->lo.u32[0] = ee->r[d].u32[0];
    ee->hi.u32[0] = ee->r[d].u32[1];
    ee->lo.u32[2] = ee->r[d].u32[2];
    ee->hi.u32[2] = ee->r[d].u32[3];

    ee->lo.u32[1] = ee->lo.u32[1] - r1;
    ee->lo.u32[3] = ee->lo.u32[3] - r5;
    ee->hi.u32[1] = ee->hi.u32[1] - r3;
    ee->hi.u32[3] = ee->hi.u32[3] - r7;
}
static inline void ee_i_pmsubw(struct ee_state* ee, const ee_instruction& i) {
    int s = EE_D_RS;
    int t = EE_D_RT;
    int d = EE_D_RD;

    // int64_t r0 = ee->r[s].s32[0] * ee->r[t].s32[0];
    // int64_t r1 = ee->r[s].s32[2] * ee->r[t].s32[2];

    // ee->r[d].u64[0] = ((ee->hi.u64[0] << 32) | (uint64_t)ee->lo.u32[0]) - r0;
    // ee->r[d].u64[1] = ((ee->hi.u64[1] << 32) | (uint64_t)ee->lo.u32[2]) - r0;

    // ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    // ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    // ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    // ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);

    int64_t op1 = (int64_t)ee->r[s].s32[0];
    int64_t op2 = (int64_t)ee->r[t].s32[0];
    int64_t result = op1 * op2;
    int64_t result2 = ((int64_t)ee->hi.s32[0] << 32) - result;

    result2 = (int32_t)(result2 / 4294967295);

    ee->lo.s64[0] = ee->lo.s32[0] - (int32_t)(result & 0xffffffff);
    ee->hi.s64[0] = (int32_t)result2;

    op1 = (int64_t)ee->r[s].s32[2];
    op2 = (int64_t)ee->r[t].s32[2];
    result = op1 * op2;

    result2 = ((int64_t)ee->hi.s32[2] << 32) - result;
    result2 = (int32_t)(result2 / 4294967295);

    ee->lo.s64[1] = ee->lo.s32[2] - (int32_t)(result & 0xffffffff);
    ee->hi.s64[1] = (int32_t)result2;

    ee->r[d].u32[0] = ee->lo.u32[0];
    ee->r[d].u32[1] = ee->hi.u32[0];
    ee->r[d].u32[2] = ee->lo.u32[2];
    ee->r[d].u32[3] = ee->hi.u32[2];
}
static inline void ee_i_pmthi(struct ee_state* ee, const ee_instruction& i) {
    ee->hi = ee->r[EE_D_RS];
}
static inline void ee_i_pmthl(struct ee_state* ee, const ee_instruction& i) {
    int s = EE_D_RS;

    ee->lo.u32[0] = ee->r[s].u32[0];
    ee->lo.u32[2] = ee->r[s].u32[2];
    ee->hi.u32[0] = ee->r[s].u32[1];
    ee->hi.u32[2] = ee->r[s].u32[3];
}
static inline void ee_i_pmtlo(struct ee_state* ee, const ee_instruction& i) {
    ee->lo = ee->r[EE_D_RS];
}
static inline void ee_i_pmulth(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->lo.u32[0] = (int32_t)(int16_t)ee->r[s].u16[0] * (int32_t)(int16_t)ee->r[t].u16[0];
    ee->lo.u32[1] = (int32_t)(int16_t)ee->r[s].u16[1] * (int32_t)(int16_t)ee->r[t].u16[1];
    ee->hi.u32[0] = (int32_t)(int16_t)ee->r[s].u16[2] * (int32_t)(int16_t)ee->r[t].u16[2];
    ee->hi.u32[1] = (int32_t)(int16_t)ee->r[s].u16[3] * (int32_t)(int16_t)ee->r[t].u16[3];
    ee->lo.u32[2] = (int32_t)(int16_t)ee->r[s].u16[4] * (int32_t)(int16_t)ee->r[t].u16[4];
    ee->lo.u32[3] = (int32_t)(int16_t)ee->r[s].u16[5] * (int32_t)(int16_t)ee->r[t].u16[5];
    ee->hi.u32[2] = (int32_t)(int16_t)ee->r[s].u16[6] * (int32_t)(int16_t)ee->r[t].u16[6];
    ee->hi.u32[3] = (int32_t)(int16_t)ee->r[s].u16[7] * (int32_t)(int16_t)ee->r[t].u16[7];
    ee->r[d].u32[0] = ee->lo.u32[0];
    ee->r[d].u32[1] = ee->hi.u32[0];
    ee->r[d].u32[2] = ee->lo.u32[2];
    ee->r[d].u32[3] = ee->hi.u32[2];
}
static inline void ee_i_pmultuw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u64[0] = (uint64_t)ee->r[s].u32[0] * (uint64_t)ee->r[t].u32[0];
    ee->r[d].u64[1] = (uint64_t)ee->r[s].u32[2] * (uint64_t)ee->r[t].u32[2];

    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void ee_i_pmultw(struct ee_state* ee, const ee_instruction& i) {
    int s = EE_D_RS;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u64[0] = SE6432(ee->r[s].u32[0]) * SE6432(ee->r[t].u32[0]);
    ee->r[d].u64[1] = SE6432(ee->r[s].u32[2]) * SE6432(ee->r[t].u32[2]);

    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void ee_i_pnor(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u64[0] = ~(rs.u64[0] | rt.u64[0]);
    ee->r[d].u64[1] = ~(rs.u64[1] | rt.u64[1]);
}
static inline void ee_i_por(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u64[0] = rs.u64[0] | rt.u64[0];
    ee->r[d].u64[1] = rs.u64[1] | rt.u64[1];
}
static inline void ee_i_ppac5(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = ((rt.u32[0] & 0x000000f8) >> 3) |
                      ((rt.u32[0] & 0x0000f800) >> 6) |
                      ((rt.u32[0] & 0x00f80000) >> 9) |
                      ((rt.u32[0] & 0x80000000) >> 16);
    ee->r[d].u32[1] = ((rt.u32[1] & 0x000000f8) >> 3) |
                      ((rt.u32[1] & 0x0000f800) >> 6) |
                      ((rt.u32[1] & 0x00f80000) >> 9) |
                      ((rt.u32[1] & 0x80000000) >> 16);
    ee->r[d].u32[2] = ((rt.u32[2] & 0x000000f8) >> 3) |
                      ((rt.u32[2] & 0x0000f800) >> 6) |
                      ((rt.u32[2] & 0x00f80000) >> 9) |
                      ((rt.u32[2] & 0x80000000) >> 16);
    ee->r[d].u32[3] = ((rt.u32[3] & 0x000000f8) >> 3) |
                      ((rt.u32[3] & 0x0000f800) >> 6) |
                      ((rt.u32[3] & 0x00f80000) >> 9) |
                      ((rt.u32[3] & 0x80000000) >> 16);
}
static inline void ee_i_ppacb(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u8[0 ] = rt.u8[ 0];
    ee->r[d].u8[1 ] = rt.u8[ 2];
    ee->r[d].u8[2 ] = rt.u8[ 4];
    ee->r[d].u8[3 ] = rt.u8[ 6];
    ee->r[d].u8[4 ] = rt.u8[ 8];
    ee->r[d].u8[5 ] = rt.u8[10];
    ee->r[d].u8[6 ] = rt.u8[12];
    ee->r[d].u8[7 ] = rt.u8[14];
    ee->r[d].u8[8 ] = rs.u8[ 0];
    ee->r[d].u8[9 ] = rs.u8[ 2];
    ee->r[d].u8[10] = rs.u8[ 4];
    ee->r[d].u8[11] = rs.u8[ 6];
    ee->r[d].u8[12] = rs.u8[ 8];
    ee->r[d].u8[13] = rs.u8[10];
    ee->r[d].u8[14] = rs.u8[12];
    ee->r[d].u8[15] = rs.u8[14];
}
static inline void ee_i_ppach(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rt.u16[2];
    ee->r[d].u16[2] = rt.u16[4];
    ee->r[d].u16[3] = rt.u16[6];
    ee->r[d].u16[4] = rs.u16[0];
    ee->r[d].u16[5] = rs.u16[2];
    ee->r[d].u16[6] = rs.u16[4];
    ee->r[d].u16[7] = rs.u16[6];
}
static inline void ee_i_ppacw(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rs.u32[0];
    ee->r[d].u32[3] = rs.u32[2];
}
static inline void ee_i_pref(struct ee_state* ee, const ee_instruction& i) {
    // Does nothing
}
static inline void ee_i_prevh(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u16[0] = rt.u16[3];
    ee->r[d].u16[1] = rt.u16[2];
    ee->r[d].u16[2] = rt.u16[1];
    ee->r[d].u16[3] = rt.u16[0];
    ee->r[d].u16[4] = rt.u16[7];
    ee->r[d].u16[5] = rt.u16[6];
    ee->r[d].u16[6] = rt.u16[5];
    ee->r[d].u16[7] = rt.u16[4];
}
static inline void ee_i_prot3w(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[1];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rt.u32[0];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void ee_i_psllh(struct ee_state* ee, const ee_instruction& i) {
    int sa = EE_D_SA & 0xf;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u16[0] = ee->r[t].u16[0] << sa;
    ee->r[d].u16[1] = ee->r[t].u16[1] << sa;
    ee->r[d].u16[2] = ee->r[t].u16[2] << sa;
    ee->r[d].u16[3] = ee->r[t].u16[3] << sa;
    ee->r[d].u16[4] = ee->r[t].u16[4] << sa;
    ee->r[d].u16[5] = ee->r[t].u16[5] << sa;
    ee->r[d].u16[6] = ee->r[t].u16[6] << sa;
    ee->r[d].u16[7] = ee->r[t].u16[7] << sa;
}
static inline void ee_i_psllvw(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int d = EE_D_RD;
    int s = EE_D_RS;

    ee->r[d].u64[0] = SE6432(ee->r[t].u32[0] << (ee->r[s].u32[0] & 31));
    ee->r[d].u64[1] = SE6432(ee->r[t].u32[2] << (ee->r[s].u32[2] & 31));
}
static inline void ee_i_psllw(struct ee_state* ee, const ee_instruction& i) {
    int sa = EE_D_SA;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u32[0] = ee->r[t].u32[0] << sa;
    ee->r[d].u32[1] = ee->r[t].u32[1] << sa;
    ee->r[d].u32[2] = ee->r[t].u32[2] << sa;
    ee->r[d].u32[3] = ee->r[t].u32[3] << sa;
}
static inline void ee_i_psrah(struct ee_state* ee, const ee_instruction& i) {
    int sa = EE_D_SA & 0xf;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u16[0] = ((int16_t)ee->r[t].u16[0]) >> sa;
    ee->r[d].u16[1] = ((int16_t)ee->r[t].u16[1]) >> sa;
    ee->r[d].u16[2] = ((int16_t)ee->r[t].u16[2]) >> sa;
    ee->r[d].u16[3] = ((int16_t)ee->r[t].u16[3]) >> sa;
    ee->r[d].u16[4] = ((int16_t)ee->r[t].u16[4]) >> sa;
    ee->r[d].u16[5] = ((int16_t)ee->r[t].u16[5]) >> sa;
    ee->r[d].u16[6] = ((int16_t)ee->r[t].u16[6]) >> sa;
    ee->r[d].u16[7] = ((int16_t)ee->r[t].u16[7]) >> sa;
}
static inline void ee_i_psravw(struct ee_state* ee, const ee_instruction& i) {
    int s = EE_D_RS;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u64[0] = SE6432((int32_t)ee->r[t].u32[0] >> (ee->r[s].u32[0] & 31));
    ee->r[d].u64[1] = SE6432((int32_t)ee->r[t].u32[2] >> (ee->r[s].u32[2] & 31));
}
static inline void ee_i_psraw(struct ee_state* ee, const ee_instruction& i) {
    int sa = EE_D_SA;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u32[0] = ((int32_t)ee->r[t].u32[0]) >> sa;
    ee->r[d].u32[1] = ((int32_t)ee->r[t].u32[1]) >> sa;
    ee->r[d].u32[2] = ((int32_t)ee->r[t].u32[2]) >> sa;
    ee->r[d].u32[3] = ((int32_t)ee->r[t].u32[3]) >> sa;
}
static inline void ee_i_psrlh(struct ee_state* ee, const ee_instruction& i) {
    int sa = EE_D_SA & 0xf;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u16[0] = ee->r[t].u16[0] >> sa;
    ee->r[d].u16[1] = ee->r[t].u16[1] >> sa;
    ee->r[d].u16[2] = ee->r[t].u16[2] >> sa;
    ee->r[d].u16[3] = ee->r[t].u16[3] >> sa;
    ee->r[d].u16[4] = ee->r[t].u16[4] >> sa;
    ee->r[d].u16[5] = ee->r[t].u16[5] >> sa;
    ee->r[d].u16[6] = ee->r[t].u16[6] >> sa;
    ee->r[d].u16[7] = ee->r[t].u16[7] >> sa;
}
static inline void ee_i_psrlvw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u64[0] = SE6432(ee->r[t].u32[0] >> (ee->r[s].u32[0] & 31));
    ee->r[d].u64[1] = SE6432(ee->r[t].u32[2] >> (ee->r[s].u32[2] & 31));
}
static inline void ee_i_psrlw(struct ee_state* ee, const ee_instruction& i) {
    int sa = EE_D_SA;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u32[0] = ee->r[t].u32[0] >> sa;
    ee->r[d].u32[1] = ee->r[t].u32[1] >> sa;
    ee->r[d].u32[2] = ee->r[t].u32[2] >> sa;
    ee->r[d].u32[3] = ee->r[t].u32[3] >> sa;
}
static inline void ee_i_psubb(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int s = EE_D_RS;
    int d = EE_D_RD;

    ee->r[d].u8[0 ] = ee->r[s].u8[0 ] - ee->r[t].u8[0 ];
    ee->r[d].u8[1 ] = ee->r[s].u8[1 ] - ee->r[t].u8[1 ];
    ee->r[d].u8[2 ] = ee->r[s].u8[2 ] - ee->r[t].u8[2 ];
    ee->r[d].u8[3 ] = ee->r[s].u8[3 ] - ee->r[t].u8[3 ];
    ee->r[d].u8[4 ] = ee->r[s].u8[4 ] - ee->r[t].u8[4 ];
    ee->r[d].u8[5 ] = ee->r[s].u8[5 ] - ee->r[t].u8[5 ];
    ee->r[d].u8[6 ] = ee->r[s].u8[6 ] - ee->r[t].u8[6 ];
    ee->r[d].u8[7 ] = ee->r[s].u8[7 ] - ee->r[t].u8[7 ];
    ee->r[d].u8[8 ] = ee->r[s].u8[8 ] - ee->r[t].u8[8 ];
    ee->r[d].u8[9 ] = ee->r[s].u8[9 ] - ee->r[t].u8[9 ];
    ee->r[d].u8[10] = ee->r[s].u8[10] - ee->r[t].u8[10];
    ee->r[d].u8[11] = ee->r[s].u8[11] - ee->r[t].u8[11];
    ee->r[d].u8[12] = ee->r[s].u8[12] - ee->r[t].u8[12];
    ee->r[d].u8[13] = ee->r[s].u8[13] - ee->r[t].u8[13];
    ee->r[d].u8[14] = ee->r[s].u8[14] - ee->r[t].u8[14];
    ee->r[d].u8[15] = ee->r[s].u8[15] - ee->r[t].u8[15];
}
static inline void ee_i_psubh(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int s = EE_D_RS;
    int d = EE_D_RD;

    ee->r[d].u16[0] = ee->r[s].u16[0] - ee->r[t].u16[0];
    ee->r[d].u16[1] = ee->r[s].u16[1] - ee->r[t].u16[1];
    ee->r[d].u16[2] = ee->r[s].u16[2] - ee->r[t].u16[2];
    ee->r[d].u16[3] = ee->r[s].u16[3] - ee->r[t].u16[3];
    ee->r[d].u16[4] = ee->r[s].u16[4] - ee->r[t].u16[4];
    ee->r[d].u16[5] = ee->r[s].u16[5] - ee->r[t].u16[5];
    ee->r[d].u16[6] = ee->r[s].u16[6] - ee->r[t].u16[6];
    ee->r[d].u16[7] = ee->r[s].u16[7] - ee->r[t].u16[7];
}
static inline void ee_i_psubsb(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    int32_t r0  = ((int32_t)(int8_t)ee->r[s].u8[0 ]) - ((int32_t)(int8_t)ee->r[t].u8[0 ]);
    int32_t r1  = ((int32_t)(int8_t)ee->r[s].u8[1 ]) - ((int32_t)(int8_t)ee->r[t].u8[1 ]);
    int32_t r2  = ((int32_t)(int8_t)ee->r[s].u8[2 ]) - ((int32_t)(int8_t)ee->r[t].u8[2 ]);
    int32_t r3  = ((int32_t)(int8_t)ee->r[s].u8[3 ]) - ((int32_t)(int8_t)ee->r[t].u8[3 ]);
    int32_t r4  = ((int32_t)(int8_t)ee->r[s].u8[4 ]) - ((int32_t)(int8_t)ee->r[t].u8[4 ]);
    int32_t r5  = ((int32_t)(int8_t)ee->r[s].u8[5 ]) - ((int32_t)(int8_t)ee->r[t].u8[5 ]);
    int32_t r6  = ((int32_t)(int8_t)ee->r[s].u8[6 ]) - ((int32_t)(int8_t)ee->r[t].u8[6 ]);
    int32_t r7  = ((int32_t)(int8_t)ee->r[s].u8[7 ]) - ((int32_t)(int8_t)ee->r[t].u8[7 ]);
    int32_t r8  = ((int32_t)(int8_t)ee->r[s].u8[8 ]) - ((int32_t)(int8_t)ee->r[t].u8[8 ]);
    int32_t r9  = ((int32_t)(int8_t)ee->r[s].u8[9 ]) - ((int32_t)(int8_t)ee->r[t].u8[9 ]);
    int32_t r10 = ((int32_t)(int8_t)ee->r[s].u8[10]) - ((int32_t)(int8_t)ee->r[t].u8[10]);
    int32_t r11 = ((int32_t)(int8_t)ee->r[s].u8[11]) - ((int32_t)(int8_t)ee->r[t].u8[11]);
    int32_t r12 = ((int32_t)(int8_t)ee->r[s].u8[12]) - ((int32_t)(int8_t)ee->r[t].u8[12]);
    int32_t r13 = ((int32_t)(int8_t)ee->r[s].u8[13]) - ((int32_t)(int8_t)ee->r[t].u8[13]);
    int32_t r14 = ((int32_t)(int8_t)ee->r[s].u8[14]) - ((int32_t)(int8_t)ee->r[t].u8[14]);
    int32_t r15 = ((int32_t)(int8_t)ee->r[s].u8[15]) - ((int32_t)(int8_t)ee->r[t].u8[15]);

    ee->r[d].u8[0 ] = (r0 >= 0x7f) ? 0x7f : ((r0 < -0x80) ? 0x80 : r0);
    ee->r[d].u8[1 ] = (r1 >= 0x7f) ? 0x7f : ((r1 < -0x80) ? 0x80 : r1);
    ee->r[d].u8[2 ] = (r2 >= 0x7f) ? 0x7f : ((r2 < -0x80) ? 0x80 : r2);
    ee->r[d].u8[3 ] = (r3 >= 0x7f) ? 0x7f : ((r3 < -0x80) ? 0x80 : r3);
    ee->r[d].u8[4 ] = (r4 >= 0x7f) ? 0x7f : ((r4 < -0x80) ? 0x80 : r4);
    ee->r[d].u8[5 ] = (r5 >= 0x7f) ? 0x7f : ((r5 < -0x80) ? 0x80 : r5);
    ee->r[d].u8[6 ] = (r6 >= 0x7f) ? 0x7f : ((r6 < -0x80) ? 0x80 : r6);
    ee->r[d].u8[7 ] = (r7 >= 0x7f) ? 0x7f : ((r7 < -0x80) ? 0x80 : r7);
    ee->r[d].u8[8 ] = (r8 >= 0x7f) ? 0x7f : ((r8 < -0x80) ? 0x80 : r8);
    ee->r[d].u8[9 ] = (r9 >= 0x7f) ? 0x7f : ((r9 < -0x80) ? 0x80 : r9);
    ee->r[d].u8[10] = (r10 >= 0x7f) ? 0x7f : ((r10 < -0x80) ? 0x80 : r10);
    ee->r[d].u8[11] = (r11 >= 0x7f) ? 0x7f : ((r11 < -0x80) ? 0x80 : r11);
    ee->r[d].u8[12] = (r12 >= 0x7f) ? 0x7f : ((r12 < -0x80) ? 0x80 : r12);
    ee->r[d].u8[13] = (r13 >= 0x7f) ? 0x7f : ((r13 < -0x80) ? 0x80 : r13);
    ee->r[d].u8[14] = (r14 >= 0x7f) ? 0x7f : ((r14 < -0x80) ? 0x80 : r14);
    ee->r[d].u8[15] = (r15 >= 0x7f) ? 0x7f : ((r15 < -0x80) ? 0x80 : r15);
}
static inline void ee_i_psubsh(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    int32_t r0 = SE3216(ee->r[s].u16[0]) - SE3216(ee->r[t].u16[0]);
    int32_t r1 = SE3216(ee->r[s].u16[1]) - SE3216(ee->r[t].u16[1]);
    int32_t r2 = SE3216(ee->r[s].u16[2]) - SE3216(ee->r[t].u16[2]);
    int32_t r3 = SE3216(ee->r[s].u16[3]) - SE3216(ee->r[t].u16[3]);
    int32_t r4 = SE3216(ee->r[s].u16[4]) - SE3216(ee->r[t].u16[4]);
    int32_t r5 = SE3216(ee->r[s].u16[5]) - SE3216(ee->r[t].u16[5]);
    int32_t r6 = SE3216(ee->r[s].u16[6]) - SE3216(ee->r[t].u16[6]);
    int32_t r7 = SE3216(ee->r[s].u16[7]) - SE3216(ee->r[t].u16[7]);

    ee->r[d].u16[0] = (r0 >= 0x7fff) ? 0x7fff : ((r0 < -0x8000) ? 0x8000 : r0);
    ee->r[d].u16[1] = (r1 >= 0x7fff) ? 0x7fff : ((r1 < -0x8000) ? 0x8000 : r1);
    ee->r[d].u16[2] = (r2 >= 0x7fff) ? 0x7fff : ((r2 < -0x8000) ? 0x8000 : r2);
    ee->r[d].u16[3] = (r3 >= 0x7fff) ? 0x7fff : ((r3 < -0x8000) ? 0x8000 : r3);
    ee->r[d].u16[4] = (r4 >= 0x7fff) ? 0x7fff : ((r4 < -0x8000) ? 0x8000 : r4);
    ee->r[d].u16[5] = (r5 >= 0x7fff) ? 0x7fff : ((r5 < -0x8000) ? 0x8000 : r5);
    ee->r[d].u16[6] = (r6 >= 0x7fff) ? 0x7fff : ((r6 < -0x8000) ? 0x8000 : r6);
    ee->r[d].u16[7] = (r7 >= 0x7fff) ? 0x7fff : ((r7 < -0x8000) ? 0x8000 : r7);
}
static inline void ee_i_psubsw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    int64_t r0 = SE6432(ee->r[s].u32[0]) - SE6432(ee->r[t].u32[0]);
    int64_t r1 = SE6432(ee->r[s].u32[1]) - SE6432(ee->r[t].u32[1]);
    int64_t r2 = SE6432(ee->r[s].u32[2]) - SE6432(ee->r[t].u32[2]);
    int64_t r3 = SE6432(ee->r[s].u32[3]) - SE6432(ee->r[t].u32[3]);

    ee->r[d].u32[0] = (r0 >= 0x7fffffff) ? 0x7fffffff : ((r0 < (int32_t)0x80000000) ? 0x80000000 : r0);
    ee->r[d].u32[1] = (r1 >= 0x7fffffff) ? 0x7fffffff : ((r1 < (int32_t)0x80000000) ? 0x80000000 : r1);
    ee->r[d].u32[2] = (r2 >= 0x7fffffff) ? 0x7fffffff : ((r2 < (int32_t)0x80000000) ? 0x80000000 : r2);
    ee->r[d].u32[3] = (r3 >= 0x7fffffff) ? 0x7fffffff : ((r3 < (int32_t)0x80000000) ? 0x80000000 : r3);
}
static inline void ee_i_psubub(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    int32_t r0  = ((int32_t)ee->r[s].u8[0 ]) - ((int32_t)ee->r[t].u8[0 ]);
    int32_t r1  = ((int32_t)ee->r[s].u8[1 ]) - ((int32_t)ee->r[t].u8[1 ]);
    int32_t r2  = ((int32_t)ee->r[s].u8[2 ]) - ((int32_t)ee->r[t].u8[2 ]);
    int32_t r3  = ((int32_t)ee->r[s].u8[3 ]) - ((int32_t)ee->r[t].u8[3 ]);
    int32_t r4  = ((int32_t)ee->r[s].u8[4 ]) - ((int32_t)ee->r[t].u8[4 ]);
    int32_t r5  = ((int32_t)ee->r[s].u8[5 ]) - ((int32_t)ee->r[t].u8[5 ]);
    int32_t r6  = ((int32_t)ee->r[s].u8[6 ]) - ((int32_t)ee->r[t].u8[6 ]);
    int32_t r7  = ((int32_t)ee->r[s].u8[7 ]) - ((int32_t)ee->r[t].u8[7 ]);
    int32_t r8  = ((int32_t)ee->r[s].u8[8 ]) - ((int32_t)ee->r[t].u8[8 ]);
    int32_t r9  = ((int32_t)ee->r[s].u8[9 ]) - ((int32_t)ee->r[t].u8[9 ]);
    int32_t r10 = ((int32_t)ee->r[s].u8[10]) - ((int32_t)ee->r[t].u8[10]);
    int32_t r11 = ((int32_t)ee->r[s].u8[11]) - ((int32_t)ee->r[t].u8[11]);
    int32_t r12 = ((int32_t)ee->r[s].u8[12]) - ((int32_t)ee->r[t].u8[12]);
    int32_t r13 = ((int32_t)ee->r[s].u8[13]) - ((int32_t)ee->r[t].u8[13]);
    int32_t r14 = ((int32_t)ee->r[s].u8[14]) - ((int32_t)ee->r[t].u8[14]);
    int32_t r15 = ((int32_t)ee->r[s].u8[15]) - ((int32_t)ee->r[t].u8[15]);

    ee->r[d].u8[0 ] = (r0 < 0) ? 0 : r0;
    ee->r[d].u8[1 ] = (r1 < 0) ? 0 : r1;
    ee->r[d].u8[2 ] = (r2 < 0) ? 0 : r2;
    ee->r[d].u8[3 ] = (r3 < 0) ? 0 : r3;
    ee->r[d].u8[4 ] = (r4 < 0) ? 0 : r4;
    ee->r[d].u8[5 ] = (r5 < 0) ? 0 : r5;
    ee->r[d].u8[6 ] = (r6 < 0) ? 0 : r6;
    ee->r[d].u8[7 ] = (r7 < 0) ? 0 : r7;
    ee->r[d].u8[8 ] = (r8 < 0) ? 0 : r8;
    ee->r[d].u8[9 ] = (r9 < 0) ? 0 : r9;
    ee->r[d].u8[10] = (r10 < 0) ? 0 : r10;
    ee->r[d].u8[11] = (r11 < 0) ? 0 : r11;
    ee->r[d].u8[12] = (r12 < 0) ? 0 : r12;
    ee->r[d].u8[13] = (r13 < 0) ? 0 : r13;
    ee->r[d].u8[14] = (r14 < 0) ? 0 : r14;
    ee->r[d].u8[15] = (r15 < 0) ? 0 : r15;
}
static inline void ee_i_psubuh(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    int32_t r0 = (int32_t)(ee->r[s].u16[0]) - (int32_t)(ee->r[t].u16[0]);
    int32_t r1 = (int32_t)(ee->r[s].u16[1]) - (int32_t)(ee->r[t].u16[1]);
    int32_t r2 = (int32_t)(ee->r[s].u16[2]) - (int32_t)(ee->r[t].u16[2]);
    int32_t r3 = (int32_t)(ee->r[s].u16[3]) - (int32_t)(ee->r[t].u16[3]);
    int32_t r4 = (int32_t)(ee->r[s].u16[4]) - (int32_t)(ee->r[t].u16[4]);
    int32_t r5 = (int32_t)(ee->r[s].u16[5]) - (int32_t)(ee->r[t].u16[5]);
    int32_t r6 = (int32_t)(ee->r[s].u16[6]) - (int32_t)(ee->r[t].u16[6]);
    int32_t r7 = (int32_t)(ee->r[s].u16[7]) - (int32_t)(ee->r[t].u16[7]);

    ee->r[d].u16[0] = (r0 < 0) ? 0 : r0;
    ee->r[d].u16[1] = (r1 < 0) ? 0 : r1;
    ee->r[d].u16[2] = (r2 < 0) ? 0 : r2;
    ee->r[d].u16[3] = (r3 < 0) ? 0 : r3;
    ee->r[d].u16[4] = (r4 < 0) ? 0 : r4;
    ee->r[d].u16[5] = (r5 < 0) ? 0 : r5;
    ee->r[d].u16[6] = (r6 < 0) ? 0 : r6;
    ee->r[d].u16[7] = (r7 < 0) ? 0 : r7;
}
static inline void ee_i_psubuw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    int64_t r0 = (int64_t)ee->r[s].u32[0] - (int64_t)ee->r[t].u32[0];
    int64_t r1 = (int64_t)ee->r[s].u32[1] - (int64_t)ee->r[t].u32[1];
    int64_t r2 = (int64_t)ee->r[s].u32[2] - (int64_t)ee->r[t].u32[2];
    int64_t r3 = (int64_t)ee->r[s].u32[3] - (int64_t)ee->r[t].u32[3];

    ee->r[d].u32[0] = (r0 < 0) ? 0 : r0;
    ee->r[d].u32[1] = (r1 < 0) ? 0 : r1;
    ee->r[d].u32[2] = (r2 < 0) ? 0 : r2;
    ee->r[d].u32[3] = (r3 < 0) ? 0 : r3;
}
static inline void ee_i_psubw(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u32[0] = ee->r[s].u32[0] - ee->r[t].u32[0];
    ee->r[d].u32[1] = ee->r[s].u32[1] - ee->r[t].u32[1];
    ee->r[d].u32[2] = ee->r[s].u32[2] - ee->r[t].u32[2];
    ee->r[d].u32[3] = ee->r[s].u32[3] - ee->r[t].u32[3];
}
static inline void ee_i_pxor(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u64[0] = rs.u64[0] ^ rt.u64[0];
    ee->r[d].u64[1] = rs.u64[1] ^ rt.u64[1];
}
static inline void ee_i_qfsrv(struct ee_state* ee, const ee_instruction& i) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    int shift = ee->sa * 8;

    uint128_t v;

    if (!shift) {
        v = rt;
    } else {
        if (shift < 64) {
            v.u64[0] = rt.u64[0] >> shift;
            v.u64[1] = rt.u64[1] >> shift;
            v.u64[0] |= rt.u64[1] << (64 - shift);
            v.u64[1] |= rs.u64[0] << (64 - shift);
        } else {
            v.u64[0] = rt.u64[1] >> (shift - 64);
            v.u64[1] = rs.u64[0] >> (shift - 64);

            if (shift != 64) {
                v.u64[0] |= rs.u64[0] << (128u - shift);
                v.u64[1] |= rs.u64[1] << (128u - shift);
            }
        }
    }

    ee->r[d] = v;
}
static inline void ee_i_qmfc2(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[t].u64[0] = ee->vu0->vf[d].u64[0];
    ee->r[t].u64[1] = ee->vu0->vf[d].u64[1];
}
static inline void ee_i_qmtc2(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int d = EE_D_RD;

    if (!d) return;

    ee->vu0->vf[d].u128 = ee->r[t];
}
static inline void ee_i_rsqrts(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int d = EE_D_FD;

    ee->fcr &= ~(FPU_FLG_I | FPU_FLG_D);

    if ((ee->f[t].u32 & 0x7f800000) == 0) {
        ee->fcr |= FPU_FLG_D | FPU_FLG_SD;
        ee->f[d].u32 = (ee->f[t].u32 & 0x80000000) | 0x7f7fffff;
        
        return;
    } else if (ee->f[t].u32 & 0x80000000) {
        ee->fcr |= FPU_FLG_I | FPU_FLG_SI;

        ee->f[d].f = EE_FS / sqrtf(fabsf(fpu_cvtf(ee->f[t].f)));
    } else {
        ee->f[d].f = EE_FS / sqrtf(fpu_cvtf(ee->f[t].f));
    }

    if (fpu_check_overflow_no_flags(ee, &ee->f[d]))
        return;

    fpu_check_underflow_no_flags(ee, &ee->f[d]);
}
static inline void ee_i_sb(struct ee_state* ee, const ee_instruction& i) {
    bus_write8(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT);
}
static inline void ee_i_sd(struct ee_state* ee, const ee_instruction& i) {
    bus_write64(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT);
}
static inline void ee_i_sdl(struct ee_state* ee, const ee_instruction& i) {
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
static inline void ee_i_sdr(struct ee_state* ee, const ee_instruction& i) {
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
static inline void ee_i_sh(struct ee_state* ee, const ee_instruction& i) {
    bus_write16(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT);
}
static inline void ee_i_sll(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = SE6432(EE_RT32 << EE_D_SA);
}
static inline void ee_i_sllv(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = SE6432(EE_RT32 << (EE_RS & 0x1f));
}
static inline void ee_i_slt(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = (int64_t)EE_RS < (int64_t)EE_RT;
}
static inline void ee_i_slti(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = ((int64_t)EE_RS) < SE6416(EE_D_I16);
}
static inline void ee_i_sltiu(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = EE_RS < (uint64_t)(SE6416(EE_D_I16));
}
static inline void ee_i_sltu(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RS < EE_RT;
}
static inline void ee_i_sq(struct ee_state* ee, const ee_instruction& i) {
    bus_write128(ee, (EE_RS32 + SE3216(EE_D_I16)) & ~0xf, ee->r[EE_D_RT]);
}
static inline void ee_i_sqc2(struct ee_state* ee, const ee_instruction& i) {
    bus_write128(ee, (EE_RS32 + SE3216(EE_D_I16)) & ~0xf, ee->vu0->vf[EE_D_RT].u128);
}
static inline void ee_i_sqrts(struct ee_state* ee, const ee_instruction& i) {
    int t = EE_D_RT;
    int d = EE_D_FD;

    ee->fcr &= ~(FPU_FLG_I | FPU_FLG_D);

    if ((ee->f[t].u32 & 0x7f800000) == 0) {
        ee->f[d].u32 = ee->f[t].u32 & 0x80000000;
    } else if (ee->f[t].u32 & 0x80000000) {
        ee->fcr |= FPU_FLG_I | FPU_FLG_SI;

        ee->f[d].f = sqrtf(fabsf(fpu_cvtf(ee->f[t].f)));
    } else {
        ee->f[d].f = sqrtf(fpu_cvtf(ee->f[t].f));
    }
}
static inline void ee_i_sra(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = SE6432(((int32_t)EE_RT32) >> EE_D_SA);
}
static inline void ee_i_srav(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = SE6432(((int32_t)EE_RT32) >> (EE_RS & 0x1f));
}
static inline void ee_i_srl(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = SE6432(EE_RT32 >> EE_D_SA);
}
static inline void ee_i_srlv(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = SE6432(EE_RT32 >> (EE_RS & 0x1f));
}
static inline void ee_i_sub(struct ee_state* ee, const ee_instruction& i) {
    int32_t r;

    int o = __builtin_ssub_overflow(EE_RS32, EE_RT32, &r);

    if (o) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RD = SE6432(r);
    }
}
static inline void ee_i_subas(struct ee_state* ee, const ee_instruction& i) {
    ee->a.f = EE_FS - EE_FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void ee_i_subs(struct ee_state* ee, const ee_instruction& i) {
    int d = EE_D_FD;

    ee->f[d].f = EE_FS - EE_FT;

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void ee_i_subu(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = SE6432(EE_RS - EE_RT);
}
static inline void ee_i_sw(struct ee_state* ee, const ee_instruction& i) {
    bus_write32(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT32);
}
static inline void ee_i_swc1(struct ee_state* ee, const ee_instruction& i) {
    bus_write32(ee, EE_RS32 + SE3216(EE_D_I16), EE_FT32);
}
static inline void ee_i_swl(struct ee_state* ee, const ee_instruction& i) {
    static const uint32_t swl_mask[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0x00000000 };
    static const uint8_t swl_shift[4] = { 24, 16, 8, 0 };

    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t mem = bus_read32(ee, addr & ~3);

    int shift = addr & 3;

    bus_write32(ee, addr & ~3, (EE_RT32 >> swl_shift[shift] | (mem & swl_mask[shift])));

    // printf("swl mem=%08x reg=%016lx addr=%08x shift=%d rs=%08x i16=%04x\n", mem, ee->r[EE_D_RT].u64[0], addr, shift, EE_RS32, EE_D_I16);
}
static inline void ee_i_swr(struct ee_state* ee, const ee_instruction& i) {
    static const uint32_t swr_mask[4] = { 0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };
    static const uint8_t swr_shift[4] = { 0, 8, 16, 24 };

    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t mem = bus_read32(ee, addr & ~3);

    int shift = addr & 3;

    bus_write32(ee, addr & ~3, (EE_RT32 << swr_shift[shift]) | (mem & swr_mask[shift]));

    // printf("swl mem=%08x reg=%016lx addr=%08x shift=%d rs=%08x i16=%04x\n", mem, ee->r[EE_D_RT].u64[0], addr, shift, EE_RS32, EE_D_I16);
}
static inline void ee_i_sync(struct ee_state* ee, const ee_instruction& i) {
    /* Do nothing */
}

// #include "syscall.h"

static inline void ee_i_syscall(struct ee_state* ee, const ee_instruction& i) {
    uint32_t id = ee->r[3].ul64;

    if (id & 0x80000000) {
        id = (~id) + 1;
    }

    // printf("ee: %s (%d, %08x) a0=%08x (%d)\n", ee_get_syscall(n), ee->r[3].ul32, ee->r[3].ul32, ee->r[4].ul32, ee->r[4].ul32);

    // ChangeThreadPriority
    if (id == 0x29 && !ee->thread_list_base) {
        uint32_t offset = 0;

        while (offset < 0x5000) {
            uint32_t inst[3];
            uint32_t addr = 0x80000000 + offset;

            inst[0] = bus_read32(ee, addr);
            inst[1] = bus_read32(ee, addr + 4);
            inst[2] = bus_read32(ee, addr + 8);

            if (inst[0] == 0xac420000 && inst[1] == 0 && inst[2] == 0) {
                uint32_t op = bus_read32(ee, 0x80000000 + offset + (4 * 6));

                ee->thread_list_base = 0x80010000 + (op & 0xffff) - 8;

                printf("ee: Found thread list base at %08x\n", ee->thread_list_base);

                break;
            }

            offset += 4;
        }
    }

    // FlushCache
    if (id == 0x64) {
        // printf("ee: Flushed %zd blocks\n", ee->block_cache.size());

        ee->block_cache.clear();
    }

    ee_exception_level1(ee, CAUSE_EXC1_SYS);
}
static inline void ee_i_teq(struct ee_state* ee, const ee_instruction& i) {
    if (EE_RS == EE_RT) ee_exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void ee_i_teqi(struct ee_state* ee, const ee_instruction& i) {
    if (EE_RS == SE6416(EE_D_I16)) ee_exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void ee_i_tge(struct ee_state* ee, const ee_instruction& i) {
    if (EE_RS >= EE_RT) ee_exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void ee_i_tgei(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tgei unimplemented\n"); exit(1); }
static inline void ee_i_tgeiu(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tgeiu unimplemented\n"); exit(1); }
static inline void ee_i_tgeu(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tgeu unimplemented\n"); exit(1); }
static inline void ee_i_tlbp(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tlbp unimplemented\n"); return; exit(1); }
static inline void ee_i_tlbr(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tlbr unimplemented\n"); return; exit(1); }
static inline void ee_i_tlbwi(struct ee_state* ee, const ee_instruction& i) {
    printf("ee: Index=%d EntryLo0=%08x EntryLo1=%08x EntryHi=%08x PageMask=%08x\n",
        ee->index,
        ee->entrylo0,
        ee->entrylo1,
        ee->entryhi,
        ee->pagemask
    );
    /* To-do: MMU */
}
static inline void ee_i_tlbwr(struct ee_state* ee, const ee_instruction& i) { return; fprintf(stderr, "ee: tlbwr unimplemented\n"); exit(1); }
static inline void ee_i_tlt(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tlt unimplemented\n"); exit(1); }
static inline void ee_i_tlti(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tlti unimplemented\n"); exit(1); }
static inline void ee_i_tltiu(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tltiu unimplemented\n"); exit(1); }
static inline void ee_i_tltu(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tltu unimplemented\n"); exit(1); }
static inline void ee_i_tne(struct ee_state* ee, const ee_instruction& i) {
    if (EE_RS != EE_RT) ee_exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void ee_i_tnei(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tnei unimplemented\n"); exit(1); }
static inline void ee_i_vabs(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(abs) }
static inline void ee_i_vadd(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(add) }
static inline void ee_i_vadda(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(adda) }
static inline void ee_i_vaddai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addai) }
static inline void ee_i_vaddaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addaq) }
static inline void ee_i_vaddaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addaw) }
static inline void ee_i_vaddax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addax) }
static inline void ee_i_vadday(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(adday) }
static inline void ee_i_vaddaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addaz) }
static inline void ee_i_vaddi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addi) }
static inline void ee_i_vaddq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addq) }
static inline void ee_i_vaddw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addw) }
static inline void ee_i_vaddx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addx) }
static inline void ee_i_vaddy(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addy) }
static inline void ee_i_vaddz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(addz) }
static inline void ee_i_vcallms(struct ee_state* ee, const ee_instruction& i) {
    vu_execute_program(ee->vu0, EE_D_I15);
}
static inline void ee_i_vcallmsr(struct ee_state* ee, const ee_instruction& i) {
    vu_execute_program(ee->vu0, ee->vu0->cmsar0);
}
static inline void ee_i_vclipw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(clip) }
static inline void ee_i_vdiv(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(div) }
static inline void ee_i_vftoi0(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(ftoi0) }
static inline void ee_i_vftoi12(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(ftoi12) }
static inline void ee_i_vftoi15(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(ftoi15) }
static inline void ee_i_vftoi4(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(ftoi4) }
static inline void ee_i_viadd(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(iadd) }
static inline void ee_i_viaddi(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(iaddi) }
static inline void ee_i_viand(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(iand) }
static inline void ee_i_vilwr(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(ilwr) }
static inline void ee_i_vior(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(ior) }
static inline void ee_i_visub(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(isub) }
static inline void ee_i_viswr(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(iswr) }
static inline void ee_i_vitof0(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(itof0) }
static inline void ee_i_vitof12(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(itof12) }
static inline void ee_i_vitof15(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(itof15) }
static inline void ee_i_vitof4(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(itof4) }
static inline void ee_i_vlqd(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(lqd) }
static inline void ee_i_vlqi(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(lqi) }
static inline void ee_i_vmadd(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(madd) }
static inline void ee_i_vmadda(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(madda) }
static inline void ee_i_vmaddai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddai) }
static inline void ee_i_vmaddaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddaq) }
static inline void ee_i_vmaddaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddaw) }
static inline void ee_i_vmaddax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddax) }
static inline void ee_i_vmadday(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(madday) }
static inline void ee_i_vmaddaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddaz) }
static inline void ee_i_vmaddi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddi) }
static inline void ee_i_vmaddq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddq) }
static inline void ee_i_vmaddw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddw) }
static inline void ee_i_vmaddx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddx) }
static inline void ee_i_vmaddy(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddy) }
static inline void ee_i_vmaddz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maddz) }
static inline void ee_i_vmax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(max) }
static inline void ee_i_vmaxi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maxi) }
static inline void ee_i_vmaxw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maxw) }
static inline void ee_i_vmaxx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maxx) }
static inline void ee_i_vmaxy(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maxy) }
static inline void ee_i_vmaxz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(maxz) }
static inline void ee_i_vmfir(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mfir) }
static inline void ee_i_vmini(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mini) }
static inline void ee_i_vminii(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(minii) }
static inline void ee_i_vminiw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(miniw) }
static inline void ee_i_vminix(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(minix) }
static inline void ee_i_vminiy(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(miniy) }
static inline void ee_i_vminiz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(miniz) }
static inline void ee_i_vmove(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(move) }
static inline void ee_i_vmr32(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(mr32) }
static inline void ee_i_vmsub(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msub) }
static inline void ee_i_vmsuba(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msuba) }
static inline void ee_i_vmsubai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubai) }
static inline void ee_i_vmsubaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubaq) }
static inline void ee_i_vmsubaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubaw) }
static inline void ee_i_vmsubax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubax) }
static inline void ee_i_vmsubay(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubay) }
static inline void ee_i_vmsubaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubaz) }
static inline void ee_i_vmsubi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubi) }
static inline void ee_i_vmsubq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubq) }
static inline void ee_i_vmsubw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubw) }
static inline void ee_i_vmsubx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubx) }
static inline void ee_i_vmsuby(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msuby) }
static inline void ee_i_vmsubz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(msubz) }
static inline void ee_i_vmtir(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(mtir) }
static inline void ee_i_vmul(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mul) }
static inline void ee_i_vmula(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mula) }
static inline void ee_i_vmulai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulai) }
static inline void ee_i_vmulaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulaq) }
static inline void ee_i_vmulaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulaw) }
static inline void ee_i_vmulax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulax) }
static inline void ee_i_vmulay(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulay) }
static inline void ee_i_vmulaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulaz) }
static inline void ee_i_vmuli(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(muli) }
static inline void ee_i_vmulq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulq) }
static inline void ee_i_vmulw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulw) }
static inline void ee_i_vmulx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulx) }
static inline void ee_i_vmuly(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(muly) }
static inline void ee_i_vmulz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(mulz) }
static inline void ee_i_vnop(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(nop) }
static inline void ee_i_vopmsub(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(opmsub) }
static inline void ee_i_vopmula(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(opmula) }
static inline void ee_i_vrget(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(rget) }
static inline void ee_i_vrinit(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(rinit) }
static inline void ee_i_vrnext(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(rnext) }
static inline void ee_i_vrsqrt(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(rsqrt) }
static inline void ee_i_vrxor(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(rxor) }
static inline void ee_i_vsqd(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(sqd) }
static inline void ee_i_vsqi(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(sqi) }
static inline void ee_i_vsqrt(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(sqrt) }
static inline void ee_i_vsub(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(sub) }
static inline void ee_i_vsuba(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(suba) }
static inline void ee_i_vsubai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subai) }
static inline void ee_i_vsubaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subaq) }
static inline void ee_i_vsubaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subaw) }
static inline void ee_i_vsubax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subax) }
static inline void ee_i_vsubay(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subay) }
static inline void ee_i_vsubaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subaz) }
static inline void ee_i_vsubi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subi) }
static inline void ee_i_vsubq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subq) }
static inline void ee_i_vsubw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subw) }
static inline void ee_i_vsubx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subx) }
static inline void ee_i_vsuby(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(suby) }
static inline void ee_i_vsubz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(subz) }
static inline void ee_i_vwaitq(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(waitq) }
static inline void ee_i_xor(struct ee_state* ee, const ee_instruction& i) {
    EE_RD = EE_RS ^ EE_RT;
}
static inline void ee_i_xori(struct ee_state* ee, const ee_instruction& i) {
    EE_RT = EE_RS ^ EE_D_I16;
}
static inline void ee_i_invalid(struct ee_state* ee, const ee_instruction& i) {
    fprintf(stderr, "ee: Invalid instruction %08x at PC=%08x\n", i.opcode, ee->pc);

    exit(1);
}
static inline void ee_i_nop(struct ee_state* ee, const ee_instruction& i) {
}

struct ee_state* ee_create(void) {
    return new ee_state();
}

void ee_init(struct ee_state* ee, struct vu_state* vu0, struct vu_state* vu1, struct ee_bus_s bus) {
    ee->prid = 0x2e20;
    ee->pc = EE_VEC_RESET;
    ee->next_pc = ee->pc + 4;
    ee->bus = bus;
    ee->vu0 = vu0;
    ee->vu1 = vu1;

    // To-do: Set SR

    ee->scratchpad = ps2_ram_create();
    ps2_ram_init(ee->scratchpad, 0x4000);

    // EE's FPU uses round to zero by default
    fesetround(FE_TOWARDZERO);

    ee->fcr = 0x01000001;
}

void ee_reset(struct ee_state* ee) {
    for (int i = 0; i < 32; i++)
        ee->r[i] = { 0 };

    for (int i = 0; i < 32; i++)
        ee->f[i].u32 = 0;

    for (int i = 0; i < 32; i++)
        ee->cop0_r[i] = 0;

    ee->a.u32 = 0;

    ee->hi = { 0 };
    ee->lo = { 0 };
    ee->pc = 0xbfc00000;
    ee->next_pc = ee->pc + 4;
    ee->opcode = 0;
    ee->sa = 0;
    ee->branch = 0;
    ee->branch_taken = 0;
    ee->delay_slot = 0;
    ee->prid = 0x2e20;
    ee->pc = EE_VEC_RESET;
    ee->next_pc = ee->pc + 4;
    ee->intc_reads = 0;
    ee->csr_reads = 0;

    ee->block_cache.clear();

    fesetround(FE_TOWARDZERO);

    ps2_ram_reset(ee->scratchpad);

    ee->fcr = 0x01000001;
}

void ee_destroy(struct ee_state* ee) {
    ps2_ram_destroy(ee->scratchpad);

    delete ee;
}

#define EE_BRANCH_NORMAL 1
#define EE_BRANCH_IMMEDIATE 2
#define EE_BRANCH_LIKELY 3
#define EE_BRANCH_COND 4

ee_instruction ee_decode(uint32_t opcode) {
    ee_instruction i;

    i.opcode = opcode;
    i.rs = (opcode >> 21) & 0x1f;
    i.rt = (opcode >> 16) & 0x1f;
    i.rd = (opcode >> 11) & 0x1f;
    i.sa = (opcode >> 6) & 0x1f;
    i.i15 = (opcode >> 6) & 0x7fff;
    i.i16 = opcode & 0xffff;
    i.i26 = opcode & 0x3ffffff;

    i.branch = 0;
    i.cycles = 0;

    switch ((opcode & 0xFC000000) >> 26) {
        case 0x00000000 >> 26: { // special
            switch (opcode & 0x0000003F) {
                case 0x00000000: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_sll; return i;
                case 0x00000002: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_srl; return i;
                case 0x00000003: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_sra; return i;
                case 0x00000004: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_sllv; return i;
                case 0x00000006: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_srlv; return i;
                case 0x00000007: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_srav; return i;
                case 0x00000008: i.cycles = EE_CYC_DEFAULT; i.branch = 1; i.func = ee_i_jr; return i;
                case 0x00000009: i.cycles = EE_CYC_DEFAULT; i.branch = 1; i.func = ee_i_jalr; return i;
                case 0x0000000A: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_movz; return i;
                case 0x0000000B: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_movn; return i;
                case 0x0000000C: i.cycles = EE_CYC_DEFAULT; i.branch = 2; i.func = ee_i_syscall; return i;
                case 0x0000000D: i.cycles = EE_CYC_DEFAULT; i.branch = 2; i.func = ee_i_break; return i;
                case 0x0000000F: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_sync; return i;
                case 0x00000010: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_mfhi; return i;
                case 0x00000011: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_mthi; return i;
                case 0x00000012: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_mflo; return i;
                case 0x00000013: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_mtlo; return i;
                case 0x00000014: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsllv; return i;
                case 0x00000016: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsrlv; return i;
                case 0x00000017: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsrav; return i;
                case 0x00000018: i.cycles = EE_CYC_MULT; i.func = ee_i_mult; return i;
                case 0x00000019: i.cycles = EE_CYC_MULT; i.func = ee_i_multu; return i;
                case 0x0000001A: i.cycles = EE_CYC_DIV; i.func = ee_i_div; return i;
                case 0x0000001B: i.cycles = EE_CYC_DIV; i.func = ee_i_divu; return i;
                case 0x00000020: i.cycles = EE_CYC_DEFAULT; i.branch = 4; i.func = ee_i_add; return i;
                case 0x00000021: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_addu; return i;
                case 0x00000022: i.cycles = EE_CYC_DEFAULT; i.branch = 4; i.func = ee_i_sub; return i;
                case 0x00000023: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_subu; return i;
                case 0x00000024: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_and; return i;
                case 0x00000025: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_or; return i;
                case 0x00000026: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_xor; return i;
                case 0x00000027: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_nor; return i;
                case 0x00000028: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_mfsa; return i;
                case 0x00000029: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_mtsa; return i;
                case 0x0000002A: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_slt; return i;
                case 0x0000002B: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_sltu; return i;
                case 0x0000002C: i.cycles = EE_CYC_DEFAULT; i.branch = 4; i.func = ee_i_dadd; return i;
                case 0x0000002D: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_daddu; return i;
                case 0x0000002E: i.cycles = EE_CYC_DEFAULT; i.branch = 4; i.func = ee_i_dsub; return i;
                case 0x0000002F: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsubu; return i;
                case 0x00000030: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tge; return i;
                case 0x00000031: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tgeu; return i;
                case 0x00000032: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tlt; return i;
                case 0x00000033: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tltu; return i;
                case 0x00000034: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_teq; return i;
                case 0x00000036: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tne; return i;
                case 0x00000038: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsll; return i;
                case 0x0000003A: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsrl; return i;
                case 0x0000003B: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsra; return i;
                case 0x0000003C: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsll32; return i;
                case 0x0000003E: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsrl32; return i;
                case 0x0000003F: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_dsra32; return i;
            }
        } break;
        case 0x04000000 >> 26: { // regimm
            switch ((opcode & 0x001F0000) >> 16) {
                case 0x00000000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bltz; return i;
                case 0x00010000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bgez; return i;
                case 0x00020000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bltzl; return i;
                case 0x00030000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bgezl; return i;
                case 0x00080000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tgei; return i;
                case 0x00090000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tgeiu; return i;
                case 0x000A0000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tlti; return i;
                case 0x000B0000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tltiu; return i;
                case 0x000C0000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_teqi; return i;
                case 0x000E0000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.func = ee_i_tnei; return i;
                case 0x00100000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bltzal; return i;
                case 0x00110000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bgezal; return i;
                case 0x00120000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bltzall; return i;
                case 0x00130000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bgezall; return i;
                case 0x00180000 >> 16: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_mtsab; return i;
                case 0x00190000 >> 16: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_mtsah; return i;
            }
        } break;
        case 0x08000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.branch = 1; i.func = ee_i_j; return i;
        case 0x0C000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.branch = 1; i.func = ee_i_jal; return i;
        case 0x10000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_beq; return i;
        case 0x14000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bne; return i;
        case 0x18000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_blez; return i;
        case 0x1C000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bgtz; return i;
        case 0x20000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.branch = 4; i.func = ee_i_addi; return i;
        case 0x24000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_addiu; return i;
        case 0x28000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_slti; return i;
        case 0x2C000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_sltiu; return i;
        case 0x30000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_andi; return i;
        case 0x34000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_ori; return i;
        case 0x38000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_xori; return i;
        case 0x3C000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_lui; return i;
        case 0x40000000 >> 26: { // cop0
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00000000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mfc0; return i;
                case 0x00800000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mtc0; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bc0f; return i;
                        case 0x00010000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bc0t; return i;
                        case 0x00020000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bc0fl; return i;
                        case 0x00030000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bc0tl; return i;
                    }
                } break;
                case 0x02000000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000001: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_tlbr; return i;
                        case 0x00000002: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_tlbwi; return i;
                        case 0x00000006: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_tlbwr; return i;
                        case 0x00000008: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_tlbp; return i;
                        case 0x00000018: i.cycles = EE_CYC_COP_DEFAULT; i.branch = 2; i.func = ee_i_eret; return i;
                        case 0x00000038: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_ei; return i;
                        case 0x00000039: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_di; return i;
                    }
                } break;
            }
        } break;
        case 0x44000000 >> 26: { // cop1
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00000000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mfc1; return i;
                case 0x00400000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_cfc1; return i;
                case 0x00800000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mtc1; return i;
                case 0x00C00000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_ctc1; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bc1f; return i;
                        case 0x00010000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bc1t; return i;
                        case 0x00020000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bc1fl; return i;
                        case 0x00030000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bc1tl; return i;
                    }
                } break;
                case 0x02000000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000000: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_adds; return i;
                        case 0x00000001: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_subs; return i;
                        case 0x00000002: i.cycles = EE_CYC_FPU_MULT; i.func = ee_i_muls; return i;
                        case 0x00000003: i.cycles = EE_CYC_FPU_DIV; i.func = ee_i_divs; return i;
                        case 0x00000004: i.cycles = EE_CYC_FPU_DIV; i.func = ee_i_sqrts; return i;
                        case 0x00000005: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_abss; return i;
                        case 0x00000006: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_movs; return i;
                        case 0x00000007: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_negs; return i;
                        case 0x00000016: i.cycles = EE_CYC_FPU_DIV; i.func = ee_i_rsqrts; return i;
                        case 0x00000018: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_addas; return i;
                        case 0x00000019: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_subas; return i;
                        case 0x0000001A: i.cycles = EE_CYC_FPU_MULT; i.func = ee_i_mulas; return i;
                        case 0x0000001C: i.cycles = EE_CYC_FPU_MULT; i.func = ee_i_madds; return i;
                        case 0x0000001D: i.cycles = EE_CYC_FPU_MULT; i.func = ee_i_msubs; return i;
                        case 0x0000001E: i.cycles = EE_CYC_FPU_MULT; i.func = ee_i_maddas; return i;
                        case 0x0000001F: i.cycles = EE_CYC_FPU_MULT; i.func = ee_i_msubas; return i;
                        case 0x00000024: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_cvtw; return i;
                        case 0x00000028: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_maxs; return i;
                        case 0x00000029: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mins; return i;
                        case 0x00000030: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_cf; return i;
                        case 0x00000032: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_ceq; return i;
                        case 0x00000034: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_clt; return i;
                        case 0x00000036: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_cle; return i;
                    }
                } break;
                case 0x02800000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000020: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_cvts; return i;
                    }
                } break;
            }
        } break;
        case 0x48000000 >> 26: { // cop2
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00200000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_qmfc2; return i;
                case 0x00400000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_cfc2; return i;
                case 0x00A00000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_qmtc2; return i;
                case 0x00C00000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_ctc2; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bc2f; return i;
                        case 0x00010000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.func = ee_i_bc2t; return i;
                        case 0x00020000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bc2fl; return i;
                        case 0x00030000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bc2tl; return i;
                    }
                } break;
                case 0x02000000 >> 21:
                case 0x02200000 >> 21:
                case 0x02400000 >> 21:
                case 0x02600000 >> 21:
                case 0x02800000 >> 21:
                case 0x02A00000 >> 21:
                case 0x02C00000 >> 21:
                case 0x02E00000 >> 21:
                case 0x03000000 >> 21:
                case 0x03200000 >> 21:
                case 0x03400000 >> 21:
                case 0x03600000 >> 21:
                case 0x03800000 >> 21:
                case 0x03A00000 >> 21:
                case 0x03C00000 >> 21:
                case 0x03E00000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000000: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddx; return i;
                        case 0x00000001: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddy; return i;
                        case 0x00000002: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddz; return i;
                        case 0x00000003: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddw; return i;
                        case 0x00000004: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubx; return i;
                        case 0x00000005: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsuby; return i;
                        case 0x00000006: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubz; return i;
                        case 0x00000007: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubw; return i;
                        case 0x00000008: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddx; return i;
                        case 0x00000009: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddy; return i;
                        case 0x0000000A: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddz; return i;
                        case 0x0000000B: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddw; return i;
                        case 0x0000000C: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubx; return i;
                        case 0x0000000D: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsuby; return i;
                        case 0x0000000E: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubz; return i;
                        case 0x0000000F: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubw; return i;
                        case 0x00000010: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaxx; return i;
                        case 0x00000011: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaxy; return i;
                        case 0x00000012: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaxz; return i;
                        case 0x00000013: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaxw; return i;
                        case 0x00000014: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vminix; return i;
                        case 0x00000015: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vminiy; return i;
                        case 0x00000016: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vminiz; return i;
                        case 0x00000017: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vminiw; return i;
                        case 0x00000018: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulx; return i;
                        case 0x00000019: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmuly; return i;
                        case 0x0000001A: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulz; return i;
                        case 0x0000001B: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulw; return i;
                        case 0x0000001C: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulq; return i;
                        case 0x0000001D: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaxi; return i;
                        case 0x0000001E: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmuli; return i;
                        case 0x0000001F: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vminii; return i;
                        case 0x00000020: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddq; return i;
                        case 0x00000021: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddq; return i;
                        case 0x00000022: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddi; return i;
                        case 0x00000023: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddi; return i;
                        case 0x00000024: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubq; return i;
                        case 0x00000025: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubq; return i;
                        case 0x00000026: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubi; return i;
                        case 0x00000027: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubi; return i;
                        case 0x00000028: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vadd; return i;
                        case 0x00000029: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmadd; return i;
                        case 0x0000002A: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmul; return i;
                        case 0x0000002B: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmax; return i;
                        case 0x0000002C: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsub; return i;
                        case 0x0000002D: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsub; return i;
                        case 0x0000002E: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vopmsub; return i;
                        case 0x0000002F: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmini; return i;
                        case 0x00000030: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_viadd; return i;
                        case 0x00000031: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_visub; return i;
                        case 0x00000032: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_viaddi; return i;
                        case 0x00000034: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_viand; return i;
                        case 0x00000035: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vior; return i;
                        case 0x00000038: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vcallms; return i;
                        case 0x00000039: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vcallmsr; return i;
                        case 0x0000003C:
                        case 0x0000003D:
                        case 0x0000003E:
                        case 0x0000003F: {
                            uint32_t func = (opcode & 3) | ((opcode & 0x7c0) >> 4);

                            switch (func) {
                                case 0x00000000: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddax; return i;
                                case 0x00000001: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vadday; return i;
                                case 0x00000002: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddaz; return i;
                                case 0x00000003: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddaw; return i;
                                case 0x00000004: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubax; return i;
                                case 0x00000005: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubay; return i;
                                case 0x00000006: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubaz; return i;
                                case 0x00000007: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubaw; return i;
                                case 0x00000008: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddax; return i;
                                case 0x00000009: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmadday; return i;
                                case 0x0000000A: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddaz; return i;
                                case 0x0000000B: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddaw; return i;
                                case 0x0000000C: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubax; return i;
                                case 0x0000000D: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubay; return i;
                                case 0x0000000E: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubaz; return i;
                                case 0x0000000F: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubaw; return i;
                                case 0x00000010: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vitof0; return i;
                                case 0x00000011: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vitof4; return i;
                                case 0x00000012: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vitof12; return i;
                                case 0x00000013: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vitof15; return i;
                                case 0x00000014: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vftoi0; return i;
                                case 0x00000015: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vftoi4; return i;
                                case 0x00000016: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vftoi12; return i;
                                case 0x00000017: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vftoi15; return i;
                                case 0x00000018: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulax; return i;
                                case 0x00000019: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulay; return i;
                                case 0x0000001A: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulaz; return i;
                                case 0x0000001B: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulaw; return i;
                                case 0x0000001C: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulaq; return i;
                                case 0x0000001D: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vabs; return i;
                                case 0x0000001E: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmulai; return i;
                                case 0x0000001F: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vclipw; return i;
                                case 0x00000020: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddaq; return i;
                                case 0x00000021: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddaq; return i;
                                case 0x00000022: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vaddai; return i;
                                case 0x00000023: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmaddai; return i;
                                case 0x00000024: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubaq; return i;
                                case 0x00000025: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubaq; return i;
                                case 0x00000026: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsubai; return i;
                                case 0x00000027: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsubai; return i;
                                case 0x00000028: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vadda; return i;
                                case 0x00000029: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmadda; return i;
                                case 0x0000002A: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmula; return i;
                                case 0x0000002C: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsuba; return i;
                                case 0x0000002D: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmsuba; return i;
                                case 0x0000002E: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vopmula; return i;
                                case 0x0000002F: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vnop; return i;
                                case 0x00000030: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmove; return i;
                                case 0x00000031: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmr32; return i;
                                case 0x00000034: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vlqi; return i;
                                case 0x00000035: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsqi; return i;
                                case 0x00000036: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vlqd; return i;
                                case 0x00000037: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsqd; return i;
                                case 0x00000038: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vdiv; return i;
                                case 0x00000039: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vsqrt; return i;
                                case 0x0000003A: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vrsqrt; return i;
                                case 0x0000003B: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vwaitq; return i;
                                case 0x0000003C: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmtir; return i;
                                case 0x0000003D: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vmfir; return i;
                                case 0x0000003E: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vilwr; return i;
                                case 0x0000003F: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_viswr; return i;
                                case 0x00000040: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vrnext; return i;
                                case 0x00000041: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vrget; return i;
                                case 0x00000042: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vrinit; return i;
                                case 0x00000043: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_vrxor; return i;
                            }
                        } break;
                    }
                } break;
            }
        } break;
        case 0x50000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_beql; return i;
        case 0x54000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bnel; return i;
        case 0x58000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_blezl; return i;
        case 0x5C000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.func = ee_i_bgtzl; return i;
        case 0x60000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.branch = 4; i.func = ee_i_daddi; return i;
        case 0x64000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_daddiu; return i;
        case 0x68000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_ldl; return i;
        case 0x6C000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_ldr; return i;
        case 0x70000000 >> 26: { // mmi
            switch (opcode & 0x0000003F) {
                case 0x00000000: i.cycles = EE_CYC_MULT; i.func = ee_i_madd; return i;
                case 0x00000001: i.cycles = EE_CYC_MULT; i.func = ee_i_maddu; return i;
                case 0x00000004: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_plzcw; return i;
                case 0x00000008: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_paddw; return i;
                        case 0x00000040 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubw; return i;
                        case 0x00000080 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pcgtw; return i;
                        case 0x000000C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmaxw; return i;
                        case 0x00000100 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_paddh; return i;
                        case 0x00000140 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubh; return i;
                        case 0x00000180 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pcgth; return i;
                        case 0x000001C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmaxh; return i;
                        case 0x00000200 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_paddb; return i;
                        case 0x00000240 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubb; return i;
                        case 0x00000280 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pcgtb; return i;
                        case 0x00000400 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_paddsw; return i;
                        case 0x00000440 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubsw; return i;
                        case 0x00000480 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pextlw; return i;
                        case 0x000004C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_ppacw; return i;
                        case 0x00000500 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_paddsh; return i;
                        case 0x00000540 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubsh; return i;
                        case 0x00000580 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pextlh; return i;
                        case 0x000005C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_ppach; return i;
                        case 0x00000600 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_paddsb; return i;
                        case 0x00000640 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubsb; return i;
                        case 0x00000680 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pextlb; return i;
                        case 0x000006C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_ppacb; return i;
                        case 0x00000780 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pext5; return i;
                        case 0x000007C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_ppac5; return i;
                    }
                } break;
                case 0x00000009: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_pmaddw; return i;
                        case 0x00000080 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psllvw; return i;
                        case 0x000000C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psrlvw; return i;
                        case 0x00000100 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_pmsubw; return i;
                        case 0x00000200 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmfhi; return i;
                        case 0x00000240 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmflo; return i;
                        case 0x00000280 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pinth; return i;
                        case 0x00000300 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_pmultw; return i;
                        case 0x00000340 >> 6: i.cycles = EE_CYC_MMI_DIV; i.func = ee_i_pdivw; return i;
                        case 0x00000380 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pcpyld; return i;
                        case 0x00000400 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_pmaddh; return i;
                        case 0x00000440 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_phmadh; return i;
                        case 0x00000480 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pand; return i;
                        case 0x000004C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pxor; return i;
                        case 0x00000500 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_pmsubh; return i;
                        case 0x00000540 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_phmsbh; return i;
                        case 0x00000680 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pexeh; return i;
                        case 0x000006C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_prevh; return i;
                        case 0x00000700 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_pmulth; return i;
                        case 0x00000740 >> 6: i.cycles = EE_CYC_MMI_DIV; i.func = ee_i_pdivbw; return i;
                        case 0x00000780 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pexew; return i;
                        case 0x000007C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_prot3w; return i;
                    }
                } break;
                case 0x00000010: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mfhi1; return i;
                case 0x00000011: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mthi1; return i;
                case 0x00000012: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mflo1; return i;
                case 0x00000013: i.cycles = EE_CYC_COP_DEFAULT; i.func = ee_i_mtlo1; return i;
                case 0x00000018: i.cycles = EE_CYC_MULT; i.func = ee_i_mult1; return i;
                case 0x00000019: i.cycles = EE_CYC_MULT; i.func = ee_i_multu1; return i;
                case 0x0000001A: i.cycles = EE_CYC_DIV; i.func = ee_i_div1; return i;
                case 0x0000001B: i.cycles = EE_CYC_DIV; i.func = ee_i_divu1; return i;
                case 0x00000020: i.cycles = EE_CYC_MULT; i.func = ee_i_madd1; return i;
                case 0x00000021: i.cycles = EE_CYC_MULT; i.func = ee_i_maddu1; return i;
                case 0x00000028: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000040 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pabsw; return i;
                        case 0x00000080 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pceqw; return i;
                        case 0x000000C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pminw; return i;
                        case 0x00000100 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_padsbh; return i;
                        case 0x00000140 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pabsh; return i;
                        case 0x00000180 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pceqh; return i;
                        case 0x000001C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pminh; return i;
                        case 0x00000280 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pceqb; return i;
                        case 0x00000400 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_padduw; return i;
                        case 0x00000440 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubuw; return i;
                        case 0x00000480 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pextuw; return i;
                        case 0x00000500 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_padduh; return i;
                        case 0x00000540 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubuh; return i;
                        case 0x00000580 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pextuh; return i;
                        case 0x00000600 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_paddub; return i;
                        case 0x00000640 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psubub; return i;
                        case 0x00000680 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pextub; return i;
                        case 0x000006C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_qfsrv; return i;
                    }
                } break;
                case 0x00000029: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_pmadduw; return i;
                        case 0x000000C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psravw; return i;
                        case 0x00000200 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmthi; return i;
                        case 0x00000240 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmtlo; return i;
                        case 0x00000280 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pinteh; return i;
                        case 0x00000300 >> 6: i.cycles = EE_CYC_MMI_MULT; i.func = ee_i_pmultuw; return i;
                        case 0x00000340 >> 6: i.cycles = EE_CYC_MMI_DIV; i.func = ee_i_pdivuw; return i;
                        case 0x00000380 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pcpyud; return i;
                        case 0x00000480 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_por; return i;
                        case 0x000004C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pnor; return i;
                        case 0x00000680 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pexch; return i;
                        case 0x000006C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pcpyh; return i;
                        case 0x00000780 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pexcw; return i;
                    }
                } break;
                case 0x00000030: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmfhllw; return i;
                        case 0x00000040 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmfhluw; return i;
                        case 0x00000080 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmfhlslw; return i;
                        case 0x000000c0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmfhllh; return i;
                        case 0x00000100 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmfhlsh; return i;
                    }
                } break;
                case 0x00000031: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_pmthl; return i;
                case 0x00000034: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psllh; return i;
                case 0x00000036: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psrlh; return i;
                case 0x00000037: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psrah; return i;
                case 0x0000003C: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psllw; return i;
                case 0x0000003E: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psrlw; return i;
                case 0x0000003F: i.cycles = EE_CYC_MMI_DEFAULT; i.func = ee_i_psraw; return i;
            }
        } break;
        case 0x78000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lq; return i;
        case 0x7C000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_sq; return i;
        case 0x80000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lb; return i;
        case 0x84000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lh; return i;
        case 0x88000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lwl; return i;
        case 0x8C000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lw; return i;
        case 0x90000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lbu; return i;
        case 0x94000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lhu; return i;
        case 0x98000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lwr; return i;
        case 0x9C000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lwu; return i;
        case 0xA0000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_sb; return i;
        case 0xA4000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_sh; return i;
        case 0xA8000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_swl; return i;
        case 0xAC000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_sw; return i;
        case 0xB0000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_sdl; return i;
        case 0xB4000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_sdr; return i;
        case 0xB8000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_swr; return i;
        case 0xBC000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_cache; return i;
        case 0xC4000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lwc1; return i;
        case 0xCC000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.func = ee_i_pref; return i;
        case 0xD8000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_lqc2; return i;
        case 0xDC000000 >> 26: i.cycles = EE_CYC_LOAD; i.func = ee_i_ld; return i;
        case 0xE4000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_swc1; return i;
        case 0xF8000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_sqc2; return i;
        case 0xFC000000 >> 26: i.cycles = EE_CYC_STORE; i.func = ee_i_sd; return i;
    }

    i.func = ee_i_invalid;

    return i;
}

int ee_run_block(struct ee_state* ee, int max_cycles) {
    // This is the entrypoint to the EENULL thread.
    // If we hit this address, the program is basically idling
    // so we "fast-forward" 1024 cycles
    if (ee->pc == 0x81fc0) {
        ee_check_irq(ee);

        ee->total_cycles += 8 * 128;
        ee->count += 8 * 128;
        // ee->eenull_counter += 8 * 64;

        return 8 * 128;
    }

    if (ee->intc_reads >= 10000) {
        ee_check_irq(ee);

        return 1024;
    }

    // if (ee->csr_reads >= 1000) {
    //     ee_check_irq(ee);

    //     return 1024;
    // }

    if (ee->block_cache.contains(ee->pc)) {
        const auto& block = ee->block_cache[ee->pc];

        ee->block_pc = ee->pc;

        int cycles = 0;

        for (const auto& i : block.instructions) {
            ee->delay_slot = ee->branch;
            ee->branch = 0;

            // If we need to handle an interrupt, break out of the loop
            if (ee_check_irq(ee))
                break;

            // if (ee->pc >= 0x81fc0 && ee->pc <= 0x81fdc) {
            //     ee->eenull_counter++;
            // }

            ee->pc = ee->next_pc;
            ee->next_pc += 4;

            i.func(ee, i);

            ee->count++;
            ee->r[0] = { 0 };

            cycles++;

            // An exception occurred or likely branch was taken
            // break immediately and clear the exception flag
            if (ee->exception) {
                ee->exception = 0;

                break;
            }
        }

        // printf("ee: Block executed with %d cycles pc=%08x\n", cycles, ee->pc);

        return cycles;
    }

    uint32_t pc = ee->pc;
    uint32_t block_pc = ee->pc;
    ee_instruction i;
    ee_block block;

    block.cycles = 0;
    block.instructions.reserve(max_cycles);

    while (max_cycles) {
        ee->opcode = bus_read32(ee, pc);

        if (ee->opcode != 0) {
            i = ee_decode(ee->opcode);

            block.instructions.push_back(i);
        } else {
            i.func = ee_i_nop;
            i.branch = 0;

            block.instructions.push_back(i);
        }

        block.cycles += i.cycles;

        if (i.branch == 1 || i.branch == 3) {
            max_cycles = 2;
        } else if (i.branch != 0) {
            max_cycles = 1;
        }

        max_cycles--;

        pc += 4;
    }

    ee->block_cache[block_pc] = block;

    return 0;
}

uint32_t ee_get_pc(struct ee_state* ee) {
    return ee->pc;
}

struct ps2_ram* ee_get_spr(struct ee_state* ee) {
    return ee->scratchpad;
}

void ee_set_fmv_skip(struct ee_state* ee, int v) {
    ee->fmv_skip = v;
}

void ee_reset_intc_reads(struct ee_state* ee) {
    ee->intc_reads = 0;
}

void ee_reset_csr_reads(struct ee_state* ee) {
    ee->csr_reads = 0;
}