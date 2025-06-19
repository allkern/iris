#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>

#ifdef _EE_USE_INTRINSICS
#include <immintrin.h>
#include <tmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#endif

#include "ee.h"
#include "ee_dis.h"

#ifdef _WIN32
#define SSUBOVF64 __builtin_ssubll_overflow
#define SADDOVF64 __builtin_saddll_overflow
#else
#define SSUBOVF64 __builtin_ssubl_overflow
#define SADDOVF64 __builtin_saddl_overflow
#endif

// file = fopen("vu.dump", "a"); fprintf(file, #ins "\n"); fclose(file);
#define VU_LOWER(ins) { ee->vu0->lower = ee->opcode; vu_i_ ## ins(ee->vu0); }
#define VU_UPPER(ins) { ee->vu0->upper = ee->opcode; vu_i_ ## ins(ee->vu0); }

static FILE* file;
static int p = 0;

static inline int fast_abs32(int a) {
    uint32_t m = a >> 31;

    return (a ^ m) + (m & 1);
}

static inline int16_t fast_abs16(int16_t a) {
    uint16_t m = a >> 15;

    return (a ^ m) + (m & 1);
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

#define EE_D_RS ((ee->opcode >> 21) & 0x1f)
#define EE_D_FS ((ee->opcode >> 11) & 0x1f)
#define EE_D_RT ((ee->opcode >> 16) & 0x1f)
#define EE_D_RD ((ee->opcode >> 11) & 0x1f)
#define EE_D_FD ((ee->opcode >> 6) & 0x1f)
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
    BRANCH(cond, offset) else { ee->pc += 4; ee->next_pc += 4; }

#define SE6432(v) ((int64_t)((int32_t)(v)))
#define SE6416(v) ((int64_t)((int16_t)(v)))
#define SE648(v) ((int64_t)((int8_t)(v)))
#define SE3216(v) ((int32_t)((int16_t)(v)))

static inline void ee_print_disassembly(struct ee_state* ee) {
    char buf[128];
    struct ee_dis_state ds;

    ds.print_address = 1;
    ds.print_opcode = 1;
    ds.pc = ee->pc;

    puts(ee_disassemble(buf, ee->opcode, &ds));
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

    printf("ee: Unhandled virtual address %08x @ cyc=%ld\n", virt, ee->total_cycles);

    *(int*)0 = 0;

    exit(1);

    // To-do: MMU mapping
    *phys = virt & 0x1fffffff;

    return 0;
}

#define BUS_READ_FUNC(b)                                                        \
    static inline uint64_t bus_read ## b(struct ee_state* ee, uint32_t addr) {  \
        if ((addr & 0xf0000000) == 0x70000000)                                  \
            return ps2_ram_read ## b(ee->scratchpad, addr & 0x3fff);            \
        uint32_t phys;                                                          \
        if (ee_translate_virt(ee, addr, &phys)) {                               \
            printf("ee: TLB mapping error\n");                                  \
            exit(1);                                                            \
            return 0;                                                           \
        }                                                                       \
        return ee->bus.read ## b(ee->bus.udata, phys);                          \
    }

#define BUS_WRITE_FUNC(b)                                                                   \
    static inline void bus_write ## b(struct ee_state* ee, uint32_t addr, uint64_t data) {  \
        if ((addr & 0xf0000000) == 0x70000000)                                              \
            { ps2_ram_write ## b(ee->scratchpad, addr & 0x3fff, data); return; }            \
        uint32_t phys;                                                                      \
        if (ee_translate_virt(ee, addr, &phys)) {                                           \
            printf("ee: TLB mapping error\n");                                              \
            exit(1);                                                                        \
            return;                                                                         \
        }                                                                                   \
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

    if (ee_translate_virt(ee, addr, &phys)) {
        printf("ee: TLB mapping error\n");

        exit(1);

        return (uint128_t){ .u64[0] = 0, .u64[1] = 0 };
    }

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

    if (ee_translate_virt(ee, addr, &phys)) {
        printf("ee: TLB mapping error\n");

        exit(1);
    }

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
    if (ee_skip_fmv(ee, addr))
        return;

    ee->pc = addr;
    ee->next_pc = addr + 4;
}

static inline void ee_set_pc_delayed(struct ee_state* ee, uint32_t addr) {
    if (ee_skip_fmv(ee, addr))
        return;

    ee->next_pc = addr;
    ee->branch = 1;
}

void ee_exception_level1(struct ee_state* ee, uint32_t cause) {
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

static inline void ee_check_irq(struct ee_state* ee) {
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

        ee_exception_level1(ee, CAUSE_EXC1_INT);
    }
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

static inline void ee_i_abss(struct ee_state* ee) {
    EE_FD = fabsf(EE_FS);
}
static inline void ee_i_add(struct ee_state* ee) {
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
static inline void ee_i_addas(struct ee_state* ee) {
    ee->a.f = EE_FS + EE_FT;
}
static inline void ee_i_addi(struct ee_state* ee) {
    int32_t s = EE_RS;
    int32_t t = SE3216(EE_D_I16);
    int32_t r;

    if (__builtin_sadd_overflow(s, t, &r)) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RT = SE6432(r);
    }
}
static inline void ee_i_addiu(struct ee_state* ee) {
    EE_RT = SE6432(EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_adds(struct ee_state* ee) {
    EE_FD = EE_FS + EE_FT;
}
static inline void ee_i_addu(struct ee_state* ee) {
    EE_RD = SE6432(EE_RS + EE_RT);
}
static inline void ee_i_and(struct ee_state* ee) {
    EE_RD = EE_RS & EE_RT;
}
static inline void ee_i_andi(struct ee_state* ee) {
    EE_RT = EE_RS & EE_D_I16;
}
static inline void ee_i_bc0f(struct ee_state* ee) {
    BRANCH(!ee->cpcond0, EE_D_SI16);
}
static inline void ee_i_bc0fl(struct ee_state* ee) {
    BRANCH_LIKELY(!ee->cpcond0, EE_D_SI16);
}
static inline void ee_i_bc0t(struct ee_state* ee) {
    BRANCH(ee->cpcond0, EE_D_SI16);
}
static inline void ee_i_bc0tl(struct ee_state* ee) {
    BRANCH_LIKELY(ee->cpcond0, EE_D_SI16);
}
static inline void ee_i_bc1f(struct ee_state* ee) {
    BRANCH((ee->fcr & (1 << 23)) == 0, EE_D_SI16);
}
static inline void ee_i_bc1fl(struct ee_state* ee) {
    BRANCH_LIKELY((ee->fcr & (1 << 23)) == 0, EE_D_SI16);
}
static inline void ee_i_bc1t(struct ee_state* ee) {
    BRANCH((ee->fcr & (1 << 23)) != 0, EE_D_SI16);
}
static inline void ee_i_bc1tl(struct ee_state* ee) {
    BRANCH_LIKELY((ee->fcr & (1 << 23)) != 0, EE_D_SI16);
}
static inline void ee_i_bc2f(struct ee_state* ee) { BRANCH(1, EE_D_SI16); }
static inline void ee_i_bc2fl(struct ee_state* ee) { BRANCH_LIKELY(1, EE_D_SI16); }
static inline void ee_i_bc2t(struct ee_state* ee) { BRANCH(0, EE_D_SI16); }
static inline void ee_i_bc2tl(struct ee_state* ee) { BRANCH_LIKELY(0, EE_D_SI16); }
static inline void ee_i_beq(struct ee_state* ee) {
    BRANCH(EE_RS == EE_RT, EE_D_SI16);
}
static inline void ee_i_beql(struct ee_state* ee) {
    BRANCH_LIKELY(EE_RS == EE_RT, EE_D_SI16);
}
static inline void ee_i_bgez(struct ee_state* ee) {
    BRANCH((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgezal(struct ee_state* ee) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgezall(struct ee_state* ee) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH_LIKELY((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgezl(struct ee_state* ee) {
    BRANCH_LIKELY((int64_t)EE_RS >= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgtz(struct ee_state* ee) {
    BRANCH((int64_t)EE_RS > (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bgtzl(struct ee_state* ee) {
    BRANCH_LIKELY((int64_t)EE_RS > (int64_t)0, EE_D_SI16);
}
static inline void ee_i_blez(struct ee_state* ee) {
    BRANCH((int64_t)EE_RS <= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_blezl(struct ee_state* ee) {
    BRANCH_LIKELY((int64_t)EE_RS <= (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltz(struct ee_state* ee) {
    BRANCH((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltzal(struct ee_state* ee) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltzall(struct ee_state* ee) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH_LIKELY((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bltzl(struct ee_state* ee) {
    BRANCH_LIKELY((int64_t)EE_RS < (int64_t)0, EE_D_SI16);
}
static inline void ee_i_bne(struct ee_state* ee) {
    BRANCH(EE_RS != EE_RT, EE_D_SI16);
}
static inline void ee_i_bnel(struct ee_state* ee) {
    BRANCH_LIKELY(EE_RS != EE_RT, EE_D_SI16);
}
static inline void ee_i_break(struct ee_state* ee) {
    ee_exception_level1(ee, CAUSE_EXC1_BP);
}
static inline void ee_i_cache(struct ee_state* ee) {
    /* To-do: Cache emulation */
} 
static inline void ee_i_ceq(struct ee_state* ee) {
    if (EE_FS == EE_FT) {
        ee->fcr |= 1 << 23;    
    } else {
        ee->fcr &= ~(1 << 23);
    }
}
static inline void ee_i_cf(struct ee_state* ee) { printf("ee: cf unimplemented\n"); exit(1); }
static inline void ee_i_cfc1(struct ee_state* ee) {
    EE_RT = EE_D_FS ? ee->fcr : 0x2e00;
}
static inline void ee_i_cfc2(struct ee_state* ee) {
    // To-do: Handle FBRST, VPU_STAT, CMSAR1
    int d = EE_D_RD;

    EE_RT = d < 16 ? ee->vu0->vi[d] : ee->vu0->cr[d - 16];

    if (d == 28) {
        EE_RT &= 0x0c0c;
    }

    // static const char* regs[] = {
    //     "Status flag",
    //     "MAC flag",
    //     "clipping flag",
    //     "reserved",
    //     "R",
    //     "I",
    //     "Q",
    //     "reserved",
    //     "reserved",
    //     "reserved",
    //     "TPC",
    //     "CMSAR0",
    //     "FBRST",
    //     "VPU-STAT",
    //     "reserved",
    //     "CMSAR1",
    // };

    // if (d >= 16)
    // printf("ee: cfc2 %d (%s) <- %08x\n", d-16, regs[d-16], ee->vu0->cr[d - 16]);

    // if (d >= 16) {
    //     file = fopen("vu.dump", "a");
    //     fprintf(file, "ee: cfc2 %d (%s) <- %08x\n", d-16, regs[d-16], ee->vu0->cr[d - 16]);
    //     fclose(file);
    // }
}
static inline void ee_i_cle(struct ee_state* ee) {
    if (EE_FS <= EE_FT) {
        ee->fcr |= 1 << 23;
    } else {
        ee->fcr &= ~(1 << 23);
    }
}
static inline void ee_i_clt(struct ee_state* ee) {
    if (EE_FS < EE_FT) {
        ee->fcr |= 1 << 23;
    } else {
        ee->fcr &= ~(1 << 23);
    }
}
static inline void ee_i_ctc1(struct ee_state* ee) {
    ee->fcr = EE_RT;
}
static inline void ee_i_ctc2(struct ee_state* ee) {
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

    if (d < 16) {
        ee->vu0->vi[d] = EE_RT32;
    } else {
        if ((d-16) == 0) {
            // uint32_t status = ee->vu0->cr[0];
            ee->vu0->cr[0] &= ~0xfc0;
            ee->vu0->cr[0] |= EE_RT32 & 0xfc0;

            // printf("prev=%08x curr=%08x val=%08x\n", status, ee->vu0->cr[0], EE_RT32);
        } else {
            ee->vu0->cr[d - 16] = EE_RT32;
        }

        // printf("ee: ctc2 %d (%s) -> %08x\n", d-16, regs[d-16], EE_RT32);
    }
}
static inline void ee_i_cvts(struct ee_state* ee) {
    EE_FD = (float)ee->f[EE_D_FS].s32;
    EE_FD = fpu_cvtsw(&ee->f[EE_D_FD]);
}
static inline void ee_i_cvtw(struct ee_state* ee) {
    fpu_cvtws(&ee->f[EE_D_FD], &ee->f[EE_D_FS]);
}
static inline void ee_i_dadd(struct ee_state* ee) {
    int64_t r;

    if (SADDOVF64((int64_t)EE_RS, (int64_t)EE_RT, &r)) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RD = r;
    }
}
static inline void ee_i_daddi(struct ee_state* ee) {
    int64_t r;

    if (SADDOVF64((int64_t)EE_RS, SE6416(EE_D_I16), &r)) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RT = r;
    }
}
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
static inline void ee_i_div1(struct ee_state* ee) {
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
static inline void ee_i_divs(struct ee_state* ee) {
    EE_FD = EE_FS / EE_FT;
}
static inline void ee_i_divu(struct ee_state* ee) {
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
static inline void ee_i_divu1(struct ee_state* ee) {
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
static inline void ee_i_dsrlv(struct ee_state* ee) {
    EE_RD = EE_RT >> (EE_RS & 0x3f);
}
static inline void ee_i_dsub(struct ee_state* ee) {// printf("ee: dsub unimplemented\n"); exit(1);
    int64_t r;

    if (SSUBOVF64((int64_t)EE_RS, (int64_t)EE_RT, &r)) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RD = r;
    }
}
static inline void ee_i_dsubu(struct ee_state* ee) {
    EE_RD = EE_RS - EE_RT;
}
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
    ee_set_pc_delayed(ee, (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2));
}
static inline void ee_i_jal(struct ee_state* ee) {
    ee->r[31].ul64 = ee->next_pc;

    ee_set_pc_delayed(ee, (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2));
}
static inline void ee_i_jalr(struct ee_state* ee) {
    uint32_t next_pc = ee->next_pc;

    ee_set_pc_delayed(ee, EE_RS32);

    EE_RD = next_pc;
}
static inline void ee_i_jr(struct ee_state* ee) {
    ee_set_pc_delayed(ee, EE_RS32);
}
static inline void ee_i_lb(struct ee_state* ee) {
    EE_RT = SE648(bus_read8(ee, EE_RS32 + SE3216(EE_D_I16)));
}
static inline void ee_i_lbu(struct ee_state* ee) {
    if (((EE_RS32 + SE3216(EE_D_I16)) & 0x01ffffff) == 0x02000000)
        printf("ee: lbu @ pc=%08x next=%08x prev=%08x\n", ee->pc, ee->next_pc, ee->prev_pc);
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
    ee->r[EE_D_RT] = bus_read128(ee, (EE_RS32 + SE3216(EE_D_I16)) & ~0xf);
}
static inline void ee_i_lqc2(struct ee_state* ee) {
    ee->vu0->vf[EE_D_RT].u128 = bus_read128(ee, (EE_RS32 + SE3216(EE_D_I16)) & ~0xf);
}
static inline void ee_i_lui(struct ee_state* ee) {
    EE_RT = SE6432(EE_D_I16 << 16);
}
static inline void ee_i_lw(struct ee_state* ee) {
    EE_RT = SE6432(bus_read32(ee, EE_RS32 + SE3216(EE_D_I16)));
}
static inline void ee_i_lwc1(struct ee_state* ee) {
    EE_FT32 = bus_read32(ee, EE_RS32 + SE3216(EE_D_I16));
}

static const uint32_t LWL_MASK[4] = { 0x00ffffff, 0x0000ffff, 0x000000ff, 0x00000000 };
static const uint32_t LWR_MASK[4] = { 0x00000000, 0xff000000, 0xffff0000, 0xffffff00 };
static const int LWL_SHIFT[4] = { 24, 16, 8, 0 };
static const int LWR_SHIFT[4] = { 0, 8, 16, 24 };

static inline void ee_i_lwl(struct ee_state* ee) { // printf("ee: lwl unimplemented\n"); exit(1);
    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t shift = addr & 3;
    uint32_t mem = bus_read32(ee, addr & ~3);

    // ensure the compiler does correct sign extension into 64 bits by using s32
    EE_RT = (int32_t)((EE_RT32 & LWL_MASK[shift]) | (mem << LWL_SHIFT[shift]));

    // printf("lwl mem=%08x reg=%016lx addr=%08x shift=%d rs=%08x i16=%04x\n", mem, ee->r[EE_D_RT].u64[0], addr, shift, EE_RS32, EE_D_I16);
}

static inline void ee_i_lwr(struct ee_state* ee) { // printf("ee: lwr unimplemented\n"); exit(1);
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
static inline void ee_i_lwu(struct ee_state* ee) {
    EE_RT = bus_read32(ee, EE_RS32 + SE3216(EE_D_I16));
}
static inline void ee_i_madd(struct ee_state* ee) {
    uint64_t r = SE6432(EE_RS32) * SE6432(EE_RT32);
    uint64_t d = (uint64_t)ee->lo.u32[0] | (ee->hi.u64[0] << 32);

    d += r;

    EE_LO0 = SE6432(d & 0xffffffff);
    EE_HI0 = SE6432(d >> 32);

    EE_RD = EE_LO0;
}
static inline void ee_i_madd1(struct ee_state* ee) {
    uint64_t r = SE6432(EE_RS32) * SE6432(EE_RT32);
    uint64_t d = (EE_LO1 & 0xffffffff) | (EE_HI1 << 32);

    d += r;

    EE_LO1 = SE6432(d & 0xffffffff);
    EE_HI1 = SE6432(d >> 32);

    EE_RD = EE_LO1;
}
static inline void ee_i_maddas(struct ee_state* ee) {
    ee->a.f += EE_FS * EE_FT;
}
static inline void ee_i_madds(struct ee_state* ee) {
    EE_FD = ee->a.f + EE_FS * EE_FT;
}
static inline void ee_i_maddu(struct ee_state* ee) {
    uint64_t r = (uint64_t)EE_RS32 * (uint64_t)EE_RT32;
    uint64_t d = (uint64_t)ee->lo.u32[0] | (ee->hi.u64[0] << 32);

    d += r;

    EE_LO0 = SE6432(d & 0xffffffff);
    EE_HI0 = SE6432(d >> 32);

    EE_RD = EE_LO0;
}
static inline void ee_i_maddu1(struct ee_state* ee) {
    uint64_t r = (uint64_t)EE_RS32 * (uint64_t)EE_RT32;
    uint64_t d = (uint64_t)ee->lo.u32[2] | (ee->hi.u64[1] << 32);

    d += r;

    EE_LO1 = SE6432(d & 0xffffffff);
    EE_HI1 = SE6432(d >> 32);

    EE_RD = EE_LO1;
}
static inline void ee_i_maxs(struct ee_state* ee) {
    EE_FD = fmaxf(EE_FS, EE_FT);
}
static inline void ee_i_mfc0(struct ee_state* ee) {
    EE_RT = SE6432(ee->cop0_r[EE_D_RD]);
}
static inline void ee_i_mfc1(struct ee_state* ee) {
    EE_RT = SE6432(EE_FS32);
}
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
    EE_RD = ee->sa & 0xf;
}
static inline void ee_i_mins(struct ee_state* ee) {
    EE_FD = fminf(EE_FS, EE_FT);
}
static inline void ee_i_movn(struct ee_state* ee) {
    if (EE_RT) EE_RD = EE_RS;
}
static inline void ee_i_movs(struct ee_state* ee) {
    EE_FD32 = EE_FS32;
}
static inline void ee_i_movz(struct ee_state* ee) {
    if (!EE_RT) EE_RD = EE_RS;
}
static inline void ee_i_msubas(struct ee_state* ee) {
    ee->a.f -= EE_FS * EE_FT;
}
static inline void ee_i_msubs(struct ee_state* ee) {
    EE_FD = ee->a.f - (EE_FS * EE_FT);
}
static inline void ee_i_mtc0(struct ee_state* ee) {
    ee->cop0_r[EE_D_RD] = EE_RT32;
}
static inline void ee_i_mtc1(struct ee_state* ee) {
    EE_FS32 = EE_RT32;
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
    ee->sa = ((uint32_t)EE_RS) & 0xf;
}
static inline void ee_i_mtsab(struct ee_state* ee) {
    ee->sa = (EE_RS ^ EE_D_I16) & 15;
}
static inline void ee_i_mtsah(struct ee_state* ee) {
    ee->sa = ((EE_RS ^ EE_D_I16) & 7) << 1;
}
static inline void ee_i_mulas(struct ee_state* ee) {
    ee->a.f = EE_FS * EE_FT;
}
static inline void ee_i_muls(struct ee_state* ee) {
    EE_FD = EE_FS * EE_FT;
}
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
static inline void ee_i_multu(struct ee_state* ee) {
    uint64_t r = (uint64_t)EE_RS32 * (uint64_t)EE_RT32;

    EE_LO0 = SE6432(r & 0xffffffff);
    EE_HI0 = SE6432(r >> 32);

    EE_RD = EE_LO0;
}
static inline void ee_i_multu1(struct ee_state* ee) {
    uint64_t r = (uint64_t)EE_RS32 * (uint64_t)EE_RT32;

    EE_LO1 = SE6432(r & 0xffffffff);
    EE_HI1 = SE6432(r >> 32);

    EE_RD = EE_LO1;
}
static inline void ee_i_negs(struct ee_state* ee) {
    EE_FD = -EE_FS;
}
static inline void ee_i_nor(struct ee_state* ee) {
    EE_RD = ~(EE_RS | EE_RT);
}
static inline void ee_i_or(struct ee_state* ee) {
    EE_RD = EE_RS | EE_RT;
}
static inline void ee_i_ori(struct ee_state* ee) {
    EE_RT = EE_RS | EE_D_I16;
}
static inline void ee_i_pabsh(struct ee_state* ee) {
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
static inline void ee_i_pabsw(struct ee_state* ee) {
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
static inline void ee_i_paddb(struct ee_state* ee) {
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
static inline void ee_i_paddh(struct ee_state* ee) {
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
static inline void ee_i_paddsb(struct ee_state* ee) {
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
static inline void ee_i_paddsh(struct ee_state* ee) {
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
static inline void ee_i_paddsw(struct ee_state* ee) {
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
static inline void ee_i_paddub(struct ee_state* ee) {
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
static inline void ee_i_padduh(struct ee_state* ee) {
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
static inline void ee_i_padduw(struct ee_state* ee) {
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
static inline void ee_i_paddw(struct ee_state* ee) {
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
static inline void ee_i_padsbh(struct ee_state* ee) {
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
static inline void ee_i_pand(struct ee_state* ee) {
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
static inline void ee_i_pceqb(struct ee_state* ee) {
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
static inline void ee_i_pceqh(struct ee_state* ee) {
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
static inline void ee_i_pceqw(struct ee_state* ee) {
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
static inline void ee_i_pcgtb(struct ee_state* ee) {
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
static inline void ee_i_pcgth(struct ee_state* ee) {
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
static inline void ee_i_pcgtw(struct ee_state* ee) {
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
static inline void ee_i_pcpyh(struct ee_state* ee) {
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
static inline void ee_i_pcpyld(struct ee_state* ee) {
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
static inline void ee_i_pcpyud(struct ee_state* ee) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u64[0] = rs.u64[1];
    ee->r[d].u64[1] = rt.u64[1];
}
static inline void ee_i_pdivbw(struct ee_state* ee) { printf("ee: pdivbw unimplemented\n"); exit(1); }
static inline void ee_i_pdivuw(struct ee_state* ee) { printf("ee: pdivuw unimplemented\n"); exit(1); }
static inline void ee_i_pdivw(struct ee_state* ee) { printf("ee: pdivw unimplemented\n"); exit(1); }
static inline void ee_i_pexch(struct ee_state* ee) {
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
static inline void ee_i_pexcw(struct ee_state* ee) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rt.u32[1];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void ee_i_pexeh(struct ee_state* ee) {
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
static inline void ee_i_pexew(struct ee_state* ee) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[2];
    ee->r[d].u32[1] = rt.u32[1];
    ee->r[d].u32[2] = rt.u32[0];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void ee_i_pext5(struct ee_state* ee) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = unpack_5551_8888(rt.u32[0]);
    ee->r[d].u32[1] = unpack_5551_8888(rt.u32[1]);
    ee->r[d].u32[2] = unpack_5551_8888(rt.u32[2]);
    ee->r[d].u32[3] = unpack_5551_8888(rt.u32[3]);
}
static inline void ee_i_pextlb(struct ee_state* ee) {
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
static inline void ee_i_pextlh(struct ee_state* ee) {
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
static inline void ee_i_pextlw(struct ee_state* ee) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rs.u32[0];
    ee->r[d].u32[2] = rt.u32[1];
    ee->r[d].u32[3] = rs.u32[1];
}
static inline void ee_i_pextub(struct ee_state* ee) {
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
static inline void ee_i_pextuh(struct ee_state* ee) {
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
static inline void ee_i_pextuw(struct ee_state* ee) {
    uint128_t rt = ee->r[EE_D_RT];
    uint128_t rs = ee->r[EE_D_RS];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[2];
    ee->r[d].u32[1] = rs.u32[2];
    ee->r[d].u32[2] = rt.u32[3];
    ee->r[d].u32[3] = rs.u32[3];
}
static inline void ee_i_phmadh(struct ee_state* ee) { printf("ee: phmadh unimplemented\n"); exit(1); }
static inline void ee_i_phmsbh(struct ee_state* ee) { printf("ee: phmsbh unimplemented\n"); exit(1); }
static inline void ee_i_pinteh(struct ee_state* ee) {
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
static inline void ee_i_pinth(struct ee_state* ee) {
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
static inline void ee_i_pmaxh(struct ee_state* ee) {
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
static inline void ee_i_pmaxw(struct ee_state* ee) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u32[0] = ((int32_t)ee->r[s].u32[0] > (int32_t)ee->r[t].u32[0]) ? ee->r[s].u32[0] : ee->r[t].u32[0];
    ee->r[d].u32[1] = ((int32_t)ee->r[s].u32[1] > (int32_t)ee->r[t].u32[1]) ? ee->r[s].u32[1] : ee->r[t].u32[1];
    ee->r[d].u32[2] = ((int32_t)ee->r[s].u32[2] > (int32_t)ee->r[t].u32[2]) ? ee->r[s].u32[2] : ee->r[t].u32[2];
    ee->r[d].u32[3] = ((int32_t)ee->r[s].u32[3] > (int32_t)ee->r[t].u32[3]) ? ee->r[s].u32[3] : ee->r[t].u32[3];
}
static inline void ee_i_pmfhi(struct ee_state* ee) {
    ee->r[EE_D_RD] = ee->hi;
}
static inline void ee_i_pmfhllw(struct ee_state* ee) {
    int d = EE_D_RD;

    ee->r[d].u32[0] = ee->lo.u32[0];
    ee->r[d].u32[1] = ee->hi.u32[0];
    ee->r[d].u32[2] = ee->lo.u32[2];
    ee->r[d].u32[3] = ee->hi.u32[2];
}
static inline void ee_i_pmfhluw(struct ee_state* ee) {
    int d = EE_D_RD;

    ee->r[d].u32[0] = ee->lo.u32[1];
    ee->r[d].u32[1] = ee->hi.u32[1];
    ee->r[d].u32[2] = ee->lo.u32[3];
    ee->r[d].u32[3] = ee->hi.u32[3];
}
static inline void ee_i_pmfhlslw(struct ee_state* ee) { printf("ee: pmfhlslw unimplemented\n"); exit(1); }
static inline void ee_i_pmfhllh(struct ee_state* ee) {
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
static inline void ee_i_pmfhlsh(struct ee_state* ee) { printf("ee: pmfhlsh unimplemented\n"); exit(1); }
static inline void ee_i_pmflo(struct ee_state* ee) {
    ee->r[EE_D_RD] = ee->lo;
}
static inline void ee_i_pminh(struct ee_state* ee) {
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
static inline void ee_i_pminw(struct ee_state* ee) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u32[0] = ((int32_t)ee->r[s].u32[0] < (int32_t)ee->r[t].u32[0]) ? ee->r[s].u32[0] : ee->r[t].u32[0];
    ee->r[d].u32[1] = ((int32_t)ee->r[s].u32[1] < (int32_t)ee->r[t].u32[1]) ? ee->r[s].u32[1] : ee->r[t].u32[1];
    ee->r[d].u32[2] = ((int32_t)ee->r[s].u32[2] < (int32_t)ee->r[t].u32[2]) ? ee->r[s].u32[2] : ee->r[t].u32[2];
    ee->r[d].u32[3] = ((int32_t)ee->r[s].u32[3] < (int32_t)ee->r[t].u32[3]) ? ee->r[s].u32[3] : ee->r[t].u32[3];
}
static inline void ee_i_pmsubh(struct ee_state* ee) { printf("ee: pmsubh unimplemented\n"); exit(1); }
static inline void ee_i_pmsubw(struct ee_state* ee) { printf("ee: pmsubw unimplemented\n"); exit(1); }
static inline void ee_i_pmthi(struct ee_state* ee) {
    ee->hi = ee->r[EE_D_RS];
}
static inline void ee_i_pmthl(struct ee_state* ee) { printf("ee: pmthl unimplemented\n"); exit(1); }
static inline void ee_i_pmtlo(struct ee_state* ee) {
    ee->lo = ee->r[EE_D_RS];
}
static inline void ee_i_pmulth(struct ee_state* ee) {
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
static inline void ee_i_pmultuw(struct ee_state* ee) { printf("ee: pmultuw unimplemented\n"); exit(1); }
static inline void ee_i_pmultw(struct ee_state* ee) { printf("ee: pmultw unimplemented\n"); exit(1); }
static inline void ee_i_pnor(struct ee_state* ee) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u64[0] = ~(rs.u64[0] | rt.u64[0]);
    ee->r[d].u64[1] = ~(rs.u64[1] | rt.u64[1]);
}
static inline void ee_i_por(struct ee_state* ee) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u64[0] = rs.u64[0] | rt.u64[0];
    ee->r[d].u64[1] = rs.u64[1] | rt.u64[1];
}
static inline void ee_i_ppac5(struct ee_state* ee) {
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
static inline void ee_i_ppacb(struct ee_state* ee) {
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
static inline void ee_i_ppach(struct ee_state* ee) {
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
static inline void ee_i_ppacw(struct ee_state* ee) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rs.u32[0];
    ee->r[d].u32[3] = rs.u32[2];
}
static inline void ee_i_pref(struct ee_state* ee) {
    // Does nothing
}
static inline void ee_i_prevh(struct ee_state* ee) {
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
static inline void ee_i_prot3w(struct ee_state* ee) {
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u32[0] = rt.u32[1];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rt.u32[0];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void ee_i_psllh(struct ee_state* ee) { printf("ee: psllh unimplemented\n"); exit(1); }
static inline void ee_i_psllvw(struct ee_state* ee) { printf("ee: psllvw unimplemented\n"); exit(1); }
static inline void ee_i_psllw(struct ee_state* ee) {
    int sa = EE_D_SA;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u32[0] = ee->r[t].u32[0] << sa;
    ee->r[d].u32[1] = ee->r[t].u32[1] << sa;
    ee->r[d].u32[2] = ee->r[t].u32[2] << sa;
    ee->r[d].u32[3] = ee->r[t].u32[3] << sa;
}
static inline void ee_i_psrah(struct ee_state* ee) { printf("ee: psrah unimplemented\n"); exit(1); }
static inline void ee_i_psravw(struct ee_state* ee) { printf("ee: psravw unimplemented\n"); exit(1); }
static inline void ee_i_psraw(struct ee_state* ee) {
    int sa = EE_D_SA;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u32[0] = ((int32_t)ee->r[t].u32[0]) >> sa;
    ee->r[d].u32[1] = ((int32_t)ee->r[t].u32[1]) >> sa;
    ee->r[d].u32[2] = ((int32_t)ee->r[t].u32[2]) >> sa;
    ee->r[d].u32[3] = ((int32_t)ee->r[t].u32[3]) >> sa;
}
static inline void ee_i_psrlh(struct ee_state* ee) { printf("ee: psrlh unimplemented\n"); exit(1); }
static inline void ee_i_psrlvw(struct ee_state* ee) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u64[0] = SE6432(ee->r[t].u32[0] >> (ee->r[s].u32[0] & 31));
    ee->r[d].u64[1] = SE6432(ee->r[t].u32[2] >> (ee->r[s].u32[2] & 31));
}
static inline void ee_i_psrlw(struct ee_state* ee) {
    int sa = EE_D_SA;
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[d].u32[0] = ee->r[t].u32[0] >> sa;
    ee->r[d].u32[1] = ee->r[t].u32[1] >> sa;
    ee->r[d].u32[2] = ee->r[t].u32[2] >> sa;
    ee->r[d].u32[3] = ee->r[t].u32[3] >> sa;
}
static inline void ee_i_psubb(struct ee_state* ee) {
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
static inline void ee_i_psubh(struct ee_state* ee) {
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
static inline void ee_i_psubsb(struct ee_state* ee) {
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
static inline void ee_i_psubsh(struct ee_state* ee) {
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
static inline void ee_i_psubsw(struct ee_state* ee) {
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
static inline void ee_i_psubub(struct ee_state* ee) {
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
static inline void ee_i_psubuh(struct ee_state* ee) {
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
static inline void ee_i_psubuw(struct ee_state* ee) {
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
static inline void ee_i_psubw(struct ee_state* ee) {
    int d = EE_D_RD;
    int s = EE_D_RS;
    int t = EE_D_RT;

    ee->r[d].u32[0] = ee->r[s].u32[0] - ee->r[t].u32[0];
    ee->r[d].u32[1] = ee->r[s].u32[1] - ee->r[t].u32[1];
    ee->r[d].u32[2] = ee->r[s].u32[2] - ee->r[t].u32[2];
    ee->r[d].u32[3] = ee->r[s].u32[3] - ee->r[t].u32[3];
}
static inline void ee_i_pxor(struct ee_state* ee) {
    uint128_t rs = ee->r[EE_D_RS];
    uint128_t rt = ee->r[EE_D_RT];
    int d = EE_D_RD;

    ee->r[d].u64[0] = rs.u64[0] ^ rt.u64[0];
    ee->r[d].u64[1] = rs.u64[1] ^ rt.u64[1];
}
static inline void ee_i_qfsrv(struct ee_state* ee) {
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
static inline void ee_i_qmfc2(struct ee_state* ee) {
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->r[t].u64[0] = ee->vu0->vf[d].u64[0];
    ee->r[t].u64[1] = ee->vu0->vf[d].u64[1];
}
static inline void ee_i_qmtc2(struct ee_state* ee) {
    int t = EE_D_RT;
    int d = EE_D_RD;

    ee->vu0->vf[d].u128 = ee->r[t];
}
static inline void ee_i_rsqrts(struct ee_state* ee) {
    EE_FD = EE_FS / sqrtf(EE_FT);
}
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
    EE_RD = SE6432(EE_RT32 << EE_D_SA);
}
static inline void ee_i_sllv(struct ee_state* ee) {
    EE_RD = SE6432(EE_RT32 << (EE_RS & 0x1f));
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
    bus_write128(ee, (EE_RS32 + SE3216(EE_D_I16)) & ~0xf, ee->r[EE_D_RT]);
}
static inline void ee_i_sqc2(struct ee_state* ee) {
    bus_write128(ee, (EE_RS32 + SE3216(EE_D_I16)) & ~0xf, ee->vu0->vf[EE_D_RT].u128);
}
static inline void ee_i_sqrts(struct ee_state* ee) {
    EE_FD = sqrtf(EE_FT);
}
static inline void ee_i_sra(struct ee_state* ee) {
    EE_RD = SE6432(((int32_t)EE_RT32) >> EE_D_SA);
}
static inline void ee_i_srav(struct ee_state* ee) {
    EE_RD = SE6432(((int32_t)EE_RT32) >> (EE_RS & 0x1f));
}
static inline void ee_i_srl(struct ee_state* ee) {
    EE_RD = SE6432(EE_RT32 >> EE_D_SA);
}
static inline void ee_i_srlv(struct ee_state* ee) {
    EE_RD = SE6432(EE_RT32 >> (EE_RS & 0x1f));
}
static inline void ee_i_sub(struct ee_state* ee) {
    int32_t r;

    int o = __builtin_ssub_overflow(EE_RS32, EE_RT32, &r);

    if (o) {
        ee_exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        EE_RD = SE6432(r);
    }
}
static inline void ee_i_subas(struct ee_state* ee) {
    ee->a.f = EE_FS - EE_FT;
}
static inline void ee_i_subs(struct ee_state* ee) {
    EE_FD = EE_FS - EE_FT;
}
static inline void ee_i_subu(struct ee_state* ee) {
    EE_RD = SE6432(EE_RS - EE_RT);
}
static inline void ee_i_sw(struct ee_state* ee) {
    bus_write32(ee, EE_RS32 + SE3216(EE_D_I16), EE_RT32);
}
static inline void ee_i_swc1(struct ee_state* ee) {
    bus_write32(ee, EE_RS32 + SE3216(EE_D_I16), EE_FT32);
}
static inline void ee_i_swl(struct ee_state* ee) {
    static const uint32_t swl_mask[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0x00000000 };
    static const uint8_t swl_shift[4] = { 24, 16, 8, 0 };

    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t mem = bus_read32(ee, addr & ~3);

    int shift = addr & 3;

    bus_write32(ee, addr & ~3, (EE_RT32 >> swl_shift[shift] | (mem & swl_mask[shift])));

    // printf("swl mem=%08x reg=%016lx addr=%08x shift=%d rs=%08x i16=%04x\n", mem, ee->r[EE_D_RT].u64[0], addr, shift, EE_RS32, EE_D_I16);
}
static inline void ee_i_swr(struct ee_state* ee) {
    static const uint32_t swr_mask[4] = { 0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };
    static const uint8_t swr_shift[4] = { 0, 8, 16, 24 };

    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t mem = bus_read32(ee, addr & ~3);

    int shift = addr & 3;

    bus_write32(ee, addr & ~3, (EE_RT32 << swr_shift[shift]) | (mem & swr_mask[shift]));

    // printf("swl mem=%08x reg=%016lx addr=%08x shift=%d rs=%08x i16=%04x\n", mem, ee->r[EE_D_RT].u64[0], addr, shift, EE_RS32, EE_D_I16);
}
static inline void ee_i_sync(struct ee_state* ee) {
    /* Do nothing */
}

// #include "syscall.h"

static inline void ee_i_syscall(struct ee_state* ee) {
    // uint32_t n = ee->r[3].ul64;

    // if (n & 0x80000000) {
    //     n = (~n) + 1;
    // }

    // printf("ee: %s (%d, %08x) a0=%08x (%d)\n", ee_get_syscall(n), ee->r[3].ul32, ee->r[3].ul32, ee->r[4].ul32, ee->r[4].ul32);

    ee_exception_level1(ee, CAUSE_EXC1_SYS);
}
static inline void ee_i_teq(struct ee_state* ee) {
    if (EE_RS == EE_RT) ee_exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void ee_i_teqi(struct ee_state* ee) {
    if (EE_RS == SE6416(EE_D_I16)) ee_exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void ee_i_tge(struct ee_state* ee) { printf("ee: tge unimplemented\n"); exit(1); }
static inline void ee_i_tgei(struct ee_state* ee) { printf("ee: tgei unimplemented\n"); exit(1); }
static inline void ee_i_tgeiu(struct ee_state* ee) { printf("ee: tgeiu unimplemented\n"); exit(1); }
static inline void ee_i_tgeu(struct ee_state* ee) { printf("ee: tgeu unimplemented\n"); exit(1); }
static inline void ee_i_tlbp(struct ee_state* ee) { printf("ee: tlbp unimplemented\n"); return; exit(1); }
static inline void ee_i_tlbr(struct ee_state* ee) { printf("ee: tlbr unimplemented\n"); return; exit(1); }
static inline void ee_i_tlbwi(struct ee_state* ee) {
    printf("ee: Index=%d EntryLo0=%08x EntryLo1=%08x EntryHi=%08x PageMask=%08x\n",
        ee->index,
        ee->entrylo0,
        ee->entrylo1,
        ee->entryhi,
        ee->pagemask
    );
    /* To-do: MMU */
}
static inline void ee_i_tlbwr(struct ee_state* ee) { return; printf("ee: tlbwr unimplemented\n"); exit(1); }
static inline void ee_i_tlt(struct ee_state* ee) { printf("ee: tlt unimplemented\n"); exit(1); }
static inline void ee_i_tlti(struct ee_state* ee) { printf("ee: tlti unimplemented\n"); exit(1); }
static inline void ee_i_tltiu(struct ee_state* ee) { printf("ee: tltiu unimplemented\n"); exit(1); }
static inline void ee_i_tltu(struct ee_state* ee) { printf("ee: tltu unimplemented\n"); exit(1); }
static inline void ee_i_tne(struct ee_state* ee) {
    if (EE_RS != EE_RT) ee_exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void ee_i_tnei(struct ee_state* ee) { printf("ee: tnei unimplemented\n"); exit(1); }
static inline void ee_i_vabs(struct ee_state* ee) { VU_UPPER(abs) }
static inline void ee_i_vadd(struct ee_state* ee) { VU_UPPER(add) }
static inline void ee_i_vadda(struct ee_state* ee) { VU_UPPER(adda) }
static inline void ee_i_vaddai(struct ee_state* ee) { VU_UPPER(addai) }
static inline void ee_i_vaddaq(struct ee_state* ee) { VU_UPPER(addaq) }
static inline void ee_i_vaddaw(struct ee_state* ee) { VU_UPPER(addaw) }
static inline void ee_i_vaddax(struct ee_state* ee) { VU_UPPER(addax) }
static inline void ee_i_vadday(struct ee_state* ee) { VU_UPPER(adday) }
static inline void ee_i_vaddaz(struct ee_state* ee) { VU_UPPER(addaz) }
static inline void ee_i_vaddi(struct ee_state* ee) { VU_UPPER(addi) }
static inline void ee_i_vaddq(struct ee_state* ee) { VU_UPPER(addq) }
static inline void ee_i_vaddw(struct ee_state* ee) { VU_UPPER(addw) }
static inline void ee_i_vaddx(struct ee_state* ee) { VU_UPPER(addx) }
static inline void ee_i_vaddy(struct ee_state* ee) { VU_UPPER(addy) }
static inline void ee_i_vaddz(struct ee_state* ee) { VU_UPPER(addz) }
static inline void ee_i_vcallms(struct ee_state* ee) { printf("ee: vcallms unimplemented\n"); }
static inline void ee_i_vcallmsr(struct ee_state* ee) { printf("ee: vcallmsr unimplemented\n"); }
static inline void ee_i_vclipw(struct ee_state* ee) { VU_UPPER(clip) }
static inline void ee_i_vdiv(struct ee_state* ee) { VU_LOWER(div) }
static inline void ee_i_vftoi0(struct ee_state* ee) { VU_UPPER(ftoi0) }
static inline void ee_i_vftoi12(struct ee_state* ee) { VU_UPPER(ftoi12) }
static inline void ee_i_vftoi15(struct ee_state* ee) { VU_UPPER(ftoi15) }
static inline void ee_i_vftoi4(struct ee_state* ee) { VU_UPPER(ftoi4) }
static inline void ee_i_viadd(struct ee_state* ee) { VU_LOWER(iadd) }
static inline void ee_i_viaddi(struct ee_state* ee) { VU_LOWER(iaddi) }
static inline void ee_i_viand(struct ee_state* ee) { VU_LOWER(iand) }
static inline void ee_i_vilwr(struct ee_state* ee) { VU_LOWER(ilwr) }
static inline void ee_i_vior(struct ee_state* ee) { VU_LOWER(ior) }
static inline void ee_i_visub(struct ee_state* ee) { VU_LOWER(isub) }
static inline void ee_i_viswr(struct ee_state* ee) { VU_LOWER(iswr) }
static inline void ee_i_vitof0(struct ee_state* ee) { VU_UPPER(itof0) }
static inline void ee_i_vitof12(struct ee_state* ee) { VU_UPPER(itof12) }
static inline void ee_i_vitof15(struct ee_state* ee) { VU_UPPER(itof15) }
static inline void ee_i_vitof4(struct ee_state* ee) { VU_UPPER(itof4) }
static inline void ee_i_vlqd(struct ee_state* ee) { VU_LOWER(lqd) }
static inline void ee_i_vlqi(struct ee_state* ee) { VU_LOWER(lqi) }
static inline void ee_i_vmadd(struct ee_state* ee) { VU_UPPER(madd) }
static inline void ee_i_vmadda(struct ee_state* ee) { VU_UPPER(madda) }
static inline void ee_i_vmaddai(struct ee_state* ee) { VU_UPPER(maddai) }
static inline void ee_i_vmaddaq(struct ee_state* ee) { VU_UPPER(maddaq) }
static inline void ee_i_vmaddaw(struct ee_state* ee) { VU_UPPER(maddaw) }
static inline void ee_i_vmaddax(struct ee_state* ee) { VU_UPPER(maddax) }
static inline void ee_i_vmadday(struct ee_state* ee) { VU_UPPER(madday) }
static inline void ee_i_vmaddaz(struct ee_state* ee) { VU_UPPER(maddaz) }
static inline void ee_i_vmaddi(struct ee_state* ee) { VU_UPPER(maddi) }
static inline void ee_i_vmaddq(struct ee_state* ee) { VU_UPPER(maddq) }
static inline void ee_i_vmaddw(struct ee_state* ee) { VU_UPPER(maddw) }
static inline void ee_i_vmaddx(struct ee_state* ee) { VU_UPPER(maddx) }
static inline void ee_i_vmaddy(struct ee_state* ee) { VU_UPPER(maddy) }
static inline void ee_i_vmaddz(struct ee_state* ee) { VU_UPPER(maddz) }
static inline void ee_i_vmax(struct ee_state* ee) { VU_UPPER(max) }
static inline void ee_i_vmaxi(struct ee_state* ee) { VU_UPPER(maxi) }
static inline void ee_i_vmaxw(struct ee_state* ee) { VU_UPPER(maxw) }
static inline void ee_i_vmaxx(struct ee_state* ee) { VU_UPPER(maxx) }
static inline void ee_i_vmaxy(struct ee_state* ee) { VU_UPPER(maxy) }
static inline void ee_i_vmaxz(struct ee_state* ee) { VU_UPPER(maxz) }
static inline void ee_i_vmfir(struct ee_state* ee) { VU_UPPER(mfir) }
static inline void ee_i_vmini(struct ee_state* ee) { VU_UPPER(mini) }
static inline void ee_i_vminii(struct ee_state* ee) { VU_UPPER(minii) }
static inline void ee_i_vminiw(struct ee_state* ee) { VU_UPPER(miniw) }
static inline void ee_i_vminix(struct ee_state* ee) { VU_UPPER(minix) }
static inline void ee_i_vminiy(struct ee_state* ee) { VU_UPPER(miniy) }
static inline void ee_i_vminiz(struct ee_state* ee) { VU_UPPER(miniz) }
static inline void ee_i_vmove(struct ee_state* ee) { VU_LOWER(move) }
static inline void ee_i_vmr32(struct ee_state* ee) { VU_LOWER(mr32) }
static inline void ee_i_vmsub(struct ee_state* ee) { VU_UPPER(msub) }
static inline void ee_i_vmsuba(struct ee_state* ee) { VU_UPPER(msuba) }
static inline void ee_i_vmsubai(struct ee_state* ee) { VU_UPPER(msubai) }
static inline void ee_i_vmsubaq(struct ee_state* ee) { VU_UPPER(msubaq) }
static inline void ee_i_vmsubaw(struct ee_state* ee) { VU_UPPER(msubaw) }
static inline void ee_i_vmsubax(struct ee_state* ee) { VU_UPPER(msubax) }
static inline void ee_i_vmsubay(struct ee_state* ee) { VU_UPPER(msubay) }
static inline void ee_i_vmsubaz(struct ee_state* ee) { VU_UPPER(msubaz) }
static inline void ee_i_vmsubi(struct ee_state* ee) { VU_UPPER(msubi) }
static inline void ee_i_vmsubq(struct ee_state* ee) { VU_UPPER(msubq) }
static inline void ee_i_vmsubw(struct ee_state* ee) { VU_UPPER(msubw) }
static inline void ee_i_vmsubx(struct ee_state* ee) { VU_UPPER(msubx) }
static inline void ee_i_vmsuby(struct ee_state* ee) { VU_UPPER(msuby) }
static inline void ee_i_vmsubz(struct ee_state* ee) { VU_UPPER(msubz) }
static inline void ee_i_vmtir(struct ee_state* ee) { VU_LOWER(mtir) }
static inline void ee_i_vmul(struct ee_state* ee) { VU_UPPER(mul) }
static inline void ee_i_vmula(struct ee_state* ee) { VU_UPPER(mula) }
static inline void ee_i_vmulai(struct ee_state* ee) { VU_UPPER(mulai) }
static inline void ee_i_vmulaq(struct ee_state* ee) { VU_UPPER(mulaq) }
static inline void ee_i_vmulaw(struct ee_state* ee) { VU_UPPER(mulaw) }
static inline void ee_i_vmulax(struct ee_state* ee) { VU_UPPER(mulax) }
static inline void ee_i_vmulay(struct ee_state* ee) { VU_UPPER(mulay) }
static inline void ee_i_vmulaz(struct ee_state* ee) { VU_UPPER(mulaz) }
static inline void ee_i_vmuli(struct ee_state* ee) { VU_UPPER(muli) }
static inline void ee_i_vmulq(struct ee_state* ee) { VU_UPPER(mulq) }
static inline void ee_i_vmulw(struct ee_state* ee) { VU_UPPER(mulw) }
static inline void ee_i_vmulx(struct ee_state* ee) { VU_UPPER(mulx) }
static inline void ee_i_vmuly(struct ee_state* ee) { VU_UPPER(muly) }
static inline void ee_i_vmulz(struct ee_state* ee) { VU_UPPER(mulz) }
static inline void ee_i_vnop(struct ee_state* ee) { VU_UPPER(nop) }
static inline void ee_i_vopmsub(struct ee_state* ee) { VU_UPPER(opmsub) }
static inline void ee_i_vopmula(struct ee_state* ee) { VU_UPPER(opmula) }
static inline void ee_i_vrget(struct ee_state* ee) { VU_LOWER(rget) }
static inline void ee_i_vrinit(struct ee_state* ee) { VU_LOWER(rinit) }
static inline void ee_i_vrnext(struct ee_state* ee) { VU_LOWER(rnext) }
static inline void ee_i_vrsqrt(struct ee_state* ee) { VU_LOWER(rsqrt) }
static inline void ee_i_vrxor(struct ee_state* ee) { VU_LOWER(rxor) }
static inline void ee_i_vsqd(struct ee_state* ee) { VU_LOWER(sqd) }
static inline void ee_i_vsqi(struct ee_state* ee) { VU_LOWER(sqi) }
static inline void ee_i_vsqrt(struct ee_state* ee) { VU_LOWER(sqrt) }
static inline void ee_i_vsub(struct ee_state* ee) { VU_UPPER(sub) }
static inline void ee_i_vsuba(struct ee_state* ee) { VU_UPPER(suba) }
static inline void ee_i_vsubai(struct ee_state* ee) { VU_UPPER(subai) }
static inline void ee_i_vsubaq(struct ee_state* ee) { VU_UPPER(subaq) }
static inline void ee_i_vsubaw(struct ee_state* ee) { VU_UPPER(subaw) }
static inline void ee_i_vsubax(struct ee_state* ee) { VU_UPPER(subax) }
static inline void ee_i_vsubay(struct ee_state* ee) { VU_UPPER(subay) }
static inline void ee_i_vsubaz(struct ee_state* ee) { VU_UPPER(subaz) }
static inline void ee_i_vsubi(struct ee_state* ee) { VU_UPPER(subi) }
static inline void ee_i_vsubq(struct ee_state* ee) { VU_UPPER(subq) }
static inline void ee_i_vsubw(struct ee_state* ee) { VU_UPPER(subw) }
static inline void ee_i_vsubx(struct ee_state* ee) { VU_UPPER(subx) }
static inline void ee_i_vsuby(struct ee_state* ee) { VU_UPPER(suby) }
static inline void ee_i_vsubz(struct ee_state* ee) { VU_UPPER(subz) }
static inline void ee_i_vwaitq(struct ee_state* ee) { VU_LOWER(waitq) }
static inline void ee_i_xor(struct ee_state* ee) {
    EE_RD = EE_RS ^ EE_RT;
}
static inline void ee_i_xori(struct ee_state* ee) {
    EE_RT = EE_RS ^ EE_D_I16;
}

struct ee_state* ee_create(void) {
    return malloc(sizeof(struct ee_state));
}

void ee_init(struct ee_state* ee, struct vu_state* vu0, struct vu_state* vu1, struct ee_bus_s bus) {
    memset(ee, 0, sizeof(struct ee_state));

    ee->prid = 0x2e20;
    ee->pc = EE_VEC_RESET;
    ee->next_pc = ee->pc + 4;
    ee->bus = bus;
    ee->vu0 = vu0;
    ee->vu1 = vu1;

    // To-do: Set SR

    ee->scratchpad = ps2_ram_create();
    ps2_ram_init(ee->scratchpad, 0x4000);
}

static inline void ee_execute(struct ee_state* ee) {
    switch ((ee->opcode & 0xFC000000) >> 26) {
        case 0x00000000 >> 26: { // special
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
        case 0x04000000 >> 26: { // regimm
            switch ((ee->opcode & 0x001F0000) >> 16) {
                case 0x00000000 >> 16: ee_i_bltz(ee); return;
                case 0x00010000 >> 16: ee_i_bgez(ee); return;
                case 0x00020000 >> 16: ee_i_bltzl(ee); return;
                case 0x00030000 >> 16: ee_i_bgezl(ee); return;
                case 0x00080000 >> 16: ee_i_tgei(ee); return;
                case 0x00090000 >> 16: ee_i_tgeiu(ee); return;
                case 0x000A0000 >> 16: ee_i_tlti(ee); return;
                case 0x000B0000 >> 16: ee_i_tltiu(ee); return;
                case 0x000C0000 >> 16: ee_i_teqi(ee); return;
                case 0x000E0000 >> 16: ee_i_tnei(ee); return;
                case 0x00100000 >> 16: ee_i_bltzal(ee); return;
                case 0x00110000 >> 16: ee_i_bgezal(ee); return;
                case 0x00120000 >> 16: ee_i_bltzall(ee); return;
                case 0x00130000 >> 16: ee_i_bgezall(ee); return;
                case 0x00180000 >> 16: ee_i_mtsab(ee); return;
                case 0x00190000 >> 16: ee_i_mtsah(ee); return;
            }
        } break;
        case 0x08000000 >> 26: ee_i_j(ee); return;
        case 0x0C000000 >> 26: ee_i_jal(ee); return;
        case 0x10000000 >> 26: ee_i_beq(ee); return;
        case 0x14000000 >> 26: ee_i_bne(ee); return;
        case 0x18000000 >> 26: ee_i_blez(ee); return;
        case 0x1C000000 >> 26: ee_i_bgtz(ee); return;
        case 0x20000000 >> 26: ee_i_addi(ee); return;
        case 0x24000000 >> 26: ee_i_addiu(ee); return;
        case 0x28000000 >> 26: ee_i_slti(ee); return;
        case 0x2C000000 >> 26: ee_i_sltiu(ee); return;
        case 0x30000000 >> 26: ee_i_andi(ee); return;
        case 0x34000000 >> 26: ee_i_ori(ee); return;
        case 0x38000000 >> 26: ee_i_xori(ee); return;
        case 0x3C000000 >> 26: ee_i_lui(ee); return;
        case 0x40000000 >> 26: { // cop0
            switch ((ee->opcode & 0x03E00000) >> 21) {
                case 0x00000000 >> 21: ee_i_mfc0(ee); return;
                case 0x00800000 >> 21: ee_i_mtc0(ee); return;
                case 0x01000000 >> 21: {
                    switch ((ee->opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: ee_i_bc0f(ee); return;
                        case 0x00010000 >> 16: ee_i_bc0t(ee); return;
                        case 0x00020000 >> 16: ee_i_bc0fl(ee); return;
                        case 0x00030000 >> 16: ee_i_bc0tl(ee); return;
                    }
                } break;
                case 0x02000000 >> 21: {
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
        case 0x44000000 >> 26: { // cop1
            switch ((ee->opcode & 0x03E00000) >> 21) {
                case 0x00000000 >> 21: ee_i_mfc1(ee); return;
                case 0x00400000 >> 21: ee_i_cfc1(ee); return;
                case 0x00800000 >> 21: ee_i_mtc1(ee); return;
                case 0x00C00000 >> 21: ee_i_ctc1(ee); return;
                case 0x01000000 >> 21: {
                    switch ((ee->opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: ee_i_bc1f(ee); return;
                        case 0x00010000 >> 16: ee_i_bc1t(ee); return;
                        case 0x00020000 >> 16: ee_i_bc1fl(ee); return;
                        case 0x00030000 >> 16: ee_i_bc1tl(ee); return;
                    }
                } break;
                case 0x02000000 >> 21: {
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
                case 0x02800000 >> 21: {
                    switch (ee->opcode & 0x0000003F) {
                        case 0x00000020: ee_i_cvts(ee); return;
                    }
                } break;
            }
        } break;
        case 0x48000000 >> 26: { // cop2
            switch ((ee->opcode & 0x03E00000) >> 21) {
                case 0x00200000 >> 21: ee_i_qmfc2(ee); return;
                case 0x00400000 >> 21: ee_i_cfc2(ee); return;
                case 0x00A00000 >> 21: ee_i_qmtc2(ee); return;
                case 0x00C00000 >> 21: ee_i_ctc2(ee); return;
                case 0x01000000 >> 21: {
                    switch ((ee->opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: ee_i_bc2f(ee); return;
                        case 0x00010000 >> 16: ee_i_bc2t(ee); return;
                        case 0x00020000 >> 16: ee_i_bc2fl(ee); return;
                        case 0x00030000 >> 16: ee_i_bc2tl(ee); return;
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
                        case 0x00000039: ee_i_vcallmsr(ee); return;
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
        case 0x50000000 >> 26: ee_i_beql(ee); return;
        case 0x54000000 >> 26: ee_i_bnel(ee); return;
        case 0x58000000 >> 26: ee_i_blezl(ee); return;
        case 0x5C000000 >> 26: ee_i_bgtzl(ee); return;
        case 0x60000000 >> 26: ee_i_daddi(ee); return;
        case 0x64000000 >> 26: ee_i_daddiu(ee); return;
        case 0x68000000 >> 26: ee_i_ldl(ee); return;
        case 0x6C000000 >> 26: ee_i_ldr(ee); return;
        case 0x70000000 >> 26: { // mmi
            switch (ee->opcode & 0x0000003F) {
                case 0x00000000: ee_i_madd(ee); return;
                case 0x00000001: ee_i_maddu(ee); return;
                case 0x00000004: ee_i_plzcw(ee); return;
                case 0x00000008: {
                    switch ((ee->opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: ee_i_paddw(ee); return;
                        case 0x00000040 >> 6: ee_i_psubw(ee); return;
                        case 0x00000080 >> 6: ee_i_pcgtw(ee); return;
                        case 0x000000C0 >> 6: ee_i_pmaxw(ee); return;
                        case 0x00000100 >> 6: ee_i_paddh(ee); return;
                        case 0x00000140 >> 6: ee_i_psubh(ee); return;
                        case 0x00000180 >> 6: ee_i_pcgth(ee); return;
                        case 0x000001C0 >> 6: ee_i_pmaxh(ee); return;
                        case 0x00000200 >> 6: ee_i_paddb(ee); return;
                        case 0x00000240 >> 6: ee_i_psubb(ee); return;
                        case 0x00000280 >> 6: ee_i_pcgtb(ee); return;
                        case 0x00000400 >> 6: ee_i_paddsw(ee); return;
                        case 0x00000440 >> 6: ee_i_psubsw(ee); return;
                        case 0x00000480 >> 6: ee_i_pextlw(ee); return;
                        case 0x000004C0 >> 6: ee_i_ppacw(ee); return;
                        case 0x00000500 >> 6: ee_i_paddsh(ee); return;
                        case 0x00000540 >> 6: ee_i_psubsh(ee); return;
                        case 0x00000580 >> 6: ee_i_pextlh(ee); return;
                        case 0x000005C0 >> 6: ee_i_ppach(ee); return;
                        case 0x00000600 >> 6: ee_i_paddsb(ee); return;
                        case 0x00000640 >> 6: ee_i_psubsb(ee); return;
                        case 0x00000680 >> 6: ee_i_pextlb(ee); return;
                        case 0x000006C0 >> 6: ee_i_ppacb(ee); return;
                        case 0x00000780 >> 6: ee_i_pext5(ee); return;
                        case 0x000007C0 >> 6: ee_i_ppac5(ee); return;
                    }
                } break;
                case 0x00000009: {
                    switch ((ee->opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: ee_i_pmaddw(ee); return;
                        case 0x00000080 >> 6: ee_i_psllvw(ee); return;
                        case 0x000000C0 >> 6: ee_i_psrlvw(ee); return;
                        case 0x00000100 >> 6: ee_i_pmsubw(ee); return;
                        case 0x00000200 >> 6: ee_i_pmfhi(ee); return;
                        case 0x00000240 >> 6: ee_i_pmflo(ee); return;
                        case 0x00000280 >> 6: ee_i_pinth(ee); return;
                        case 0x00000300 >> 6: ee_i_pmultw(ee); return;
                        case 0x00000340 >> 6: ee_i_pdivw(ee); return;
                        case 0x00000380 >> 6: ee_i_pcpyld(ee); return;
                        case 0x00000400 >> 6: ee_i_pmaddh(ee); return;
                        case 0x00000440 >> 6: ee_i_phmadh(ee); return;
                        case 0x00000480 >> 6: ee_i_pand(ee); return;
                        case 0x000004C0 >> 6: ee_i_pxor(ee); return;
                        case 0x00000500 >> 6: ee_i_pmsubh(ee); return;
                        case 0x00000540 >> 6: ee_i_phmsbh(ee); return;
                        case 0x00000680 >> 6: ee_i_pexeh(ee); return;
                        case 0x000006C0 >> 6: ee_i_prevh(ee); return;
                        case 0x00000700 >> 6: ee_i_pmulth(ee); return;
                        case 0x00000740 >> 6: ee_i_pdivbw(ee); return;
                        case 0x00000780 >> 6: ee_i_pexew(ee); return;
                        case 0x000007C0 >> 6: ee_i_prot3w(ee); return;
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
                    switch ((ee->opcode & 0x000007C0) >> 6) {
                        case 0x00000040 >> 6: ee_i_pabsw(ee); return;
                        case 0x00000080 >> 6: ee_i_pceqw(ee); return;
                        case 0x000000C0 >> 6: ee_i_pminw(ee); return;
                        case 0x00000100 >> 6: ee_i_padsbh(ee); return;
                        case 0x00000140 >> 6: ee_i_pabsh(ee); return;
                        case 0x00000180 >> 6: ee_i_pceqh(ee); return;
                        case 0x000001C0 >> 6: ee_i_pminh(ee); return;
                        case 0x00000280 >> 6: ee_i_pceqb(ee); return;
                        case 0x00000400 >> 6: ee_i_padduw(ee); return;
                        case 0x00000440 >> 6: ee_i_psubuw(ee); return;
                        case 0x00000480 >> 6: ee_i_pextuw(ee); return;
                        case 0x00000500 >> 6: ee_i_padduh(ee); return;
                        case 0x00000540 >> 6: ee_i_psubuh(ee); return;
                        case 0x00000580 >> 6: ee_i_pextuh(ee); return;
                        case 0x00000600 >> 6: ee_i_paddub(ee); return;
                        case 0x00000640 >> 6: ee_i_psubub(ee); return;
                        case 0x00000680 >> 6: ee_i_pextub(ee); return;
                        case 0x000006C0 >> 6: ee_i_qfsrv(ee); return;
                    }
                } break;
                case 0x00000029: {
                    switch ((ee->opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: ee_i_pmadduw(ee); return;
                        case 0x000000C0 >> 6: ee_i_psravw(ee); return;
                        case 0x00000200 >> 6: ee_i_pmthi(ee); return;
                        case 0x00000240 >> 6: ee_i_pmtlo(ee); return;
                        case 0x00000280 >> 6: ee_i_pinteh(ee); return;
                        case 0x00000300 >> 6: ee_i_pmultuw(ee); return;
                        case 0x00000340 >> 6: ee_i_pdivuw(ee); return;
                        case 0x00000380 >> 6: ee_i_pcpyud(ee); return;
                        case 0x00000480 >> 6: ee_i_por(ee); return;
                        case 0x000004C0 >> 6: ee_i_pnor(ee); return;
                        case 0x00000680 >> 6: ee_i_pexch(ee); return;
                        case 0x000006C0 >> 6: ee_i_pcpyh(ee); return;
                        case 0x00000780 >> 6: ee_i_pexcw(ee); return;
                    }
                } break;
                case 0x00000030: {
                    switch ((ee->opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: ee_i_pmfhllw(ee); return;
                        case 0x00000040 >> 6: ee_i_pmfhluw(ee); return;
                        case 0x00000080 >> 6: ee_i_pmfhlslw(ee); return;
                        case 0x000000c0 >> 6: ee_i_pmfhllh(ee); return;
                        case 0x00000100 >> 6: ee_i_pmfhlsh(ee); return;
                    }
                } break;
                case 0x00000031: ee_i_pmthl(ee); return;
                case 0x00000034: ee_i_psllh(ee); return;
                case 0x00000036: ee_i_psrlh(ee); return;
                case 0x00000037: ee_i_psrah(ee); return;
                case 0x0000003C: ee_i_psllw(ee); return;
                case 0x0000003E: ee_i_psrlw(ee); return;
                case 0x0000003F: ee_i_psraw(ee); return;
            }
        } break;
        case 0x78000000 >> 26: ee_i_lq(ee); return;
        case 0x7C000000 >> 26: ee_i_sq(ee); return;
        case 0x80000000 >> 26: ee_i_lb(ee); return;
        case 0x84000000 >> 26: ee_i_lh(ee); return;
        case 0x88000000 >> 26: ee_i_lwl(ee); return;
        case 0x8C000000 >> 26: ee_i_lw(ee); return;
        case 0x90000000 >> 26: ee_i_lbu(ee); return;
        case 0x94000000 >> 26: ee_i_lhu(ee); return;
        case 0x98000000 >> 26: ee_i_lwr(ee); return;
        case 0x9C000000 >> 26: ee_i_lwu(ee); return;
        case 0xA0000000 >> 26: ee_i_sb(ee); return;
        case 0xA4000000 >> 26: ee_i_sh(ee); return;
        case 0xA8000000 >> 26: ee_i_swl(ee); return;
        case 0xAC000000 >> 26: ee_i_sw(ee); return;
        case 0xB0000000 >> 26: ee_i_sdl(ee); return;
        case 0xB4000000 >> 26: ee_i_sdr(ee); return;
        case 0xB8000000 >> 26: ee_i_swr(ee); return;
        case 0xBC000000 >> 26: ee_i_cache(ee); return;
        case 0xC4000000 >> 26: ee_i_lwc1(ee); return;
        case 0xCC000000 >> 26: ee_i_pref(ee); return;
        case 0xD8000000 >> 26: ee_i_lqc2(ee); return;
        case 0xDC000000 >> 26: ee_i_ld(ee); return;
        case 0xE4000000 >> 26: ee_i_swc1(ee); return;
        case 0xF8000000 >> 26: ee_i_sqc2(ee); return;
        case 0xFC000000 >> 26: ee_i_sd(ee); return;
    }

    printf("ee: Invalid instruction %08x @ pc=%08x (cyc=%ld)\n", ee->opcode, ee->prev_pc, ee->total_cycles);

    exit(1);
}

int loop = 0;

void ee_cycle(struct ee_state* ee) {
    ee->delay_slot = ee->branch;
    ee->branch = 0;

    // Would check for interrupts here, but we do this outside of the core
    // to reduce overhead
    ee_check_irq(ee);

	// if (ee->pc == 0x160700) {
	// 	p = 5000;
	// 	file = fopen("eegs.dump", "w");
	// }

    // if (ee->pc == 0x1c14bc) {
    //     p = 100;
    //     printf("got here\n");
    // }

    // if (ee->pc == 0x1c1200) {
    //     file = fopen("eegs.dump", "wb");

    //     p = 100;
    // }
    // if (ee->pc == 0x17e240) {
    //     file = fopen("eegs.dump", "wb");

    //     p = 5000;
    // }

    // if (ee->pc == 0x17e2e4) {
    //     if (ee->r[17].ul32 == 0x10000) {
    //         file = fopen("eegs.dump", "w");

    //         p = 5000;
    //     }
    //     /// printf("s1=%08x s4=%08x\n", ee->r[17].ul32, ee->r[20].ul32);
    // }

	// if (ee->pc == 0x1000d8) {
	// 	p = 200000;

    //     file = fopen("eegs.dump", "wb");
    // }

    // // //if (ee->pc == 0x9fc411d8) {
    // if (ee->pc == 0x9fc41430) {
    //     printf("ee: out\n");

    //     fclose(file);

    //     exit(1);

    //     // p = 510000;
    // }

    // vxprintf
    // if (ee->pc == 0x00106a20) {
    //     if (loop == 1) {
    //         printf("logging\n");
    //         file = fopen("eegs.dump", "w");

    //         p = 5000;
    //     } else {
    //         ++loop;
    //     }
    // }

    // loop
    // if (ee->pc == 0x0010907c) {
    //     if (p) {
    //         if (file) fclose(file);

    //         exit(1);
    //     }
    // }

    ee->prev_pc = ee->pc;
    ee->opcode = bus_read32(ee, ee->pc);

    // if (p) {
    //     // fwrite(&ee->pc, 4, 1, file);
    //     // fwrite(&ee->opcode, 4, 1, file);
    //     fprintf(file, "%08x %08x ", ee->pc, ee->opcode);

    //     for (int i = 1; i < 32; i++) {
    //         // fwrite(&ee->r[i].ul32, 4, 1, file);
    //         fprintf(file, "%08x ", ee->r[i].ul32);
    //     }

    //     fputc('\n', file);
    //     --p;

    //     if (!p) {
    //         if (file) fclose(file);

    //         exit(1);
    //     }
    // }

    // if (ee->total_cycles >= 250576836) {
    //     ee_print_disassembly(ee);
    // }

    // if (p) {
    //     ee_print_disassembly(ee);

    //     --p;

    //     if (!p) {
    //         exit(1);
    //     }
    // }

    ee->pc = ee->next_pc;
    ee->next_pc += 4;

    ee_execute(ee);

    ++ee->total_cycles;
    ++ee->count;

    // if (ee->count == ee->compare)
    //     ee->cause |= EE_CAUSE_IP7;

    ee->r[0].u64[0] = 0;
    ee->r[0].u64[1] = 0;
}

void ee_reset(struct ee_state* ee) {
    for (int i = 0; i < 32; i++)
        ee->r[i] = (uint128_t){ .u64[0] = 0, .u64[1] = 0 };

    for (int i = 0; i < 32; i++)
        ee->f[i].u32 = 0;

    for (int i = 0; i < 32; i++)
        ee->cop0_r[i] = 0;

    ee->a.u32 = 0;
    ee->fcr = 0;

    ee->hi = (uint128_t){ .u64[0] = 0, .u64[1] = 0 };
    ee->lo = (uint128_t){ .u64[0] = 0, .u64[1] = 0 };
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
}

void ee_destroy(struct ee_state* ee) {
    ps2_ram_destroy(ee->scratchpad);

    free(ee);
}