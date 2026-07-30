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
#include "bus.h"
#include "vu.h"
#include "ee_dis.h"
#include "ee_def.hpp"
#include "ee_mmi.hpp"
#include "ee_fpu.hpp"
#include "ee_lsw.hpp"

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

#define VU_D_FLD (0x01e00000)
#define VU_D_X (0x01000000)
#define VU_D_Y (0x00800000)
#define VU_D_Z (0x00400000)
#define VU_D_W (0x00200000)

// file = fopen("vu.dump", "a"); fprintf(file, #ins "\n"); fclose(file);
#define VU_LOWER(ins) { ps2_vu_decode_lower(ee->vu0, i.opcode); vu_i_ ## ins(ee->vu0, &ee->vu0->lower); }
#define VU_UPPER(ins) { ps2_vu_decode_upper(ee->vu0, i.opcode); vu_i_ ## ins(ee->vu0, &ee->vu0->upper); }
#define VU_LOWER_TEMPLATE(ins) { \
    ps2_vu_decode_lower(ee->vu0, i.opcode); \
    switch ((i.opcode >> 21) & 0xf) { \
        case 0: vu_i_ ## ins <0>(ee->vu0, &ee->vu0->lower); break; \
        case 1: vu_i_ ## ins <VU_D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 2: vu_i_ ## ins <VU_D_Z>(ee->vu0, &ee->vu0->lower); break; \
        case 3: vu_i_ ## ins <VU_D_Z | VU_D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 4: vu_i_ ## ins <VU_D_Y>(ee->vu0, &ee->vu0->lower); break; \
        case 5: vu_i_ ## ins <VU_D_Y | VU_D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 6: vu_i_ ## ins <VU_D_Y | VU_D_Z>(ee->vu0, &ee->vu0->lower); break; \
        case 7: vu_i_ ## ins <VU_D_Y | VU_D_Z | VU_D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 8: vu_i_ ## ins <VU_D_X>(ee->vu0, &ee->vu0->lower); break; \
        case 9: vu_i_ ## ins <VU_D_X | VU_D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 10: vu_i_ ## ins <VU_D_X | VU_D_Z>(ee->vu0, &ee->vu0->lower); break; \
        case 11: vu_i_ ## ins <VU_D_X | VU_D_Z | VU_D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 12: vu_i_ ## ins <VU_D_X | VU_D_Y>(ee->vu0, &ee->vu0->lower); break; \
        case 13: vu_i_ ## ins <VU_D_X | VU_D_Y | VU_D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 14: vu_i_ ## ins <VU_D_X | VU_D_Y | VU_D_Z>(ee->vu0, &ee->vu0->lower); break; \
        case 15: vu_i_ ## ins <VU_D_X | VU_D_Y | VU_D_Z | VU_D_W>(ee->vu0, &ee->vu0->lower); break; \
    } }
#define VU_UPPER_TEMPLATE(ins) { \
    ps2_vu_decode_upper(ee->vu0, i.opcode); \
    switch ((i.opcode >> 21) & 0xf) { \
        case 0: vu_i_ ## ins <0>(ee->vu0, &ee->vu0->upper); break; \
        case 1: vu_i_ ## ins <VU_D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 2: vu_i_ ## ins <VU_D_Z>(ee->vu0, &ee->vu0->upper); break; \
        case 3: vu_i_ ## ins <VU_D_Z | VU_D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 4: vu_i_ ## ins <VU_D_Y>(ee->vu0, &ee->vu0->upper); break; \
        case 5: vu_i_ ## ins <VU_D_Y | VU_D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 6: vu_i_ ## ins <VU_D_Y | VU_D_Z>(ee->vu0, &ee->vu0->upper); break; \
        case 7: vu_i_ ## ins <VU_D_Y | VU_D_Z | VU_D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 8: vu_i_ ## ins <VU_D_X>(ee->vu0, &ee->vu0->upper); break; \
        case 9: vu_i_ ## ins <VU_D_X | VU_D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 10: vu_i_ ## ins <VU_D_X | VU_D_Z>(ee->vu0, &ee->vu0->upper); break; \
        case 11: vu_i_ ## ins <VU_D_X | VU_D_Z | VU_D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 12: vu_i_ ## ins <VU_D_X | VU_D_Y>(ee->vu0, &ee->vu0->upper); break; \
        case 13: vu_i_ ## ins <VU_D_X | VU_D_Y | VU_D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 14: vu_i_ ## ins <VU_D_X | VU_D_Y | VU_D_Z>(ee->vu0, &ee->vu0->upper); break; \
        case 15: vu_i_ ## ins <VU_D_X | VU_D_Y | VU_D_Z | VU_D_W>(ee->vu0, &ee->vu0->upper); break; \
    } }

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
    } else if (word < (int32_t)0xFFFF8000) {
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
#define EE_D_RS (i.rs.r)
#define EE_D_FS (i.rd.r)
#define EE_D_RT (i.rt.r)
#define EE_D_RD (i.rd.r)
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

#define BRANCH(cond, offset) \
    if (cond) { ee->next_pc = ee->pc + (offset); }

#define BRANCH_LIKELY(cond, offset) \
    BRANCH(cond, offset) else { ee->exception = 1; }

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

void ee_exception_level1(struct ee_state* ee, uint32_t cause);

#ifdef _EE_USE_MMU
static inline struct ee_vtlb_entry* ee_search_vtlb(struct ee_state* ee, uint32_t virt) {
    for (int i = 0; i < 48; i++) {
        struct ee_vtlb_entry* e = &ee->vtlb[i];

        if (e->s) {
            uint32_t mask = 0xffffc000;

            if ((virt & mask) == (e->vpn2 & mask)) {
                return e;
            }
        }

        uint32_t mask = (~e->mask) & 0xffffe000;

        // printf("ee: TLB search index=%d virt=%08x vpn2=%08x mask=%08x\n",
        //     i,
        //     virt,
        //     e->vpn2,
        //     mask
        // );

        if ((virt & mask) == (e->vpn2 & mask)) {
            return e;
        }
    }

    return nullptr;
}

static inline int ee_translate_virt(struct ee_state* ee, uint32_t virt, uint32_t* phys, int load) {
    int seg = ee_get_segment(virt);

    // Assume we're in kernel mode
    if (seg == EE_KSEG0 || seg == EE_KSEG1) {
        *phys = virt & 0x1fffffff;

        return 0;
    }

    struct ee_vtlb_entry* entry = ee_search_vtlb(ee, virt);

    if (!entry) {
        ee_exception_level1(ee, load ? CAUSE_EXC1_TLBL : CAUSE_EXC1_TLBS);

        ee->context &= 0x7ffff0;
        ee->context |= (virt & 0xFFFFE000) >> 9;

        printf("ee: TLB miss on %s at virt=%08x\n", load ? "load" : "store", virt);

        return -1;
    }

    if (entry->s) {
        *phys = virt & 0x00003fff;

        return 1;
    }

    // printf("ee: virt=%08x vpn2=%08x even={pfn=%08x v=%d d=%d} odd={pfn=%08x v=%d d=%d} mask=%08x s=%d g=%d\n",
    //     virt,
    //     entry->vpn2,
    //     entry->pfn0,
    //     entry->v0,
    //     entry->d0,
    //     entry->pfn1,
    //     entry->v1,
    //     entry->d1,
    //     entry->mask,
    //     entry->s,
    //     entry->g
    // );

    uint32_t nmask = 0xfffff000 & ~(entry->mask >> 1);
    uint32_t pfn = entry->pfn0;

    int odd = (virt & ((entry->mask >> 1) + 0x1000)) ? 1 : 0;

    if (odd) {
        pfn = entry->pfn1;
    }

    *phys = pfn | (virt & ~nmask);

    // printf("ee: Translated virt=%08x to phys=%08x\n", virt, *phys);

    // if (odd) exit(1);
    return 0;
}

#define BUS_READ_FUNC(b)                                                        \
    static inline uint64_t bus_read ## b(struct ee_state* ee, uint32_t addr) {  \
        uint32_t phys;                                                          \
        if (ee_translate_virt(ee, addr, &phys, 1) == 1)                         \
            return ps2_ram_read ## b(ee->spr, phys);                            \
        if (phys == 0x1000f000) ee->intc_reads++;                               \
        if (phys == 0x12001000) ee->csr_reads++;                                \
        return ee->bus.read ## b(ee->bus.udata, phys);                          \
    }

#define BUS_WRITE_FUNC(b)                                                                   \
    static inline void bus_write ## b(struct ee_state* ee, uint32_t addr, uint64_t data) {  \
        uint32_t phys;                                                                      \
        if (ee_translate_virt(ee, addr, &phys, 0) == 1)                                     \
            { ps2_ram_write ## b(ee->spr, phys, data); return; }                            \
        ee->bus.write ## b(ee->bus.udata, phys, data);                                      \
    }

BUS_READ_FUNC(8)
BUS_READ_FUNC(16)
BUS_READ_FUNC(32)
BUS_READ_FUNC(64)

static inline uint128_t bus_read128(struct ee_state* ee, uint32_t addr) {
    uint32_t phys;

    if (ee_translate_virt(ee, addr, &phys, 1) == 1)
        return ps2_ram_read128(ee->spr, phys);

    return ee->bus.read128(ee->bus.udata, phys);
}

BUS_WRITE_FUNC(8)
BUS_WRITE_FUNC(16)
BUS_WRITE_FUNC(32)
BUS_WRITE_FUNC(64)

static inline void bus_write128(struct ee_state* ee, uint32_t addr, uint128_t data) {
    uint32_t phys;

    if (ee_translate_virt(ee, addr, &phys, 0) == 1)
        { ps2_ram_write128(ee->spr, phys, data); return; }

    ee->bus.write128(ee->bus.udata, phys, data);
}
#else
static inline int ee_translate_virt(struct ee_state* ee, uint32_t virt, uint32_t* phys) {
    int seg = ee_get_segment(virt);

    // Assume we're in kernel mode
    if (seg == EE_KSEG0 || seg == EE_KSEG1) {
        *phys = virt & 0x1fffffff;

        return 0;
    }

    ee_page* page = &ee->pagetable[virt / EE_MIN_PAGESIZE];

    if (!page->valid) {
        printf("ee: Segmentation fault at 0x%08x\n", virt);

        return -1;
    }

    *phys = (page->pfn * EE_MIN_PAGESIZE) | (virt & (EE_MIN_PAGESIZE - 1));

    // printf("ee: Translated virt=%08x to phys=%08x\n", virt, *phys);

    return 0;
}

#define EE_CACHE_PAGECOUNT (0x20000000 / EE_MIN_PAGESIZE)

void ee_vfast_clear(struct ee_state* ee);

void ee_purge_cache(struct ee_state* ee) {
    ee_vfast_clear(ee);

    for (int i = 0; i < EE_CACHE_PAGECOUNT; i++) {
        ee->block_cache[i].dirty = true;
        ee->block_cache[i].valid = false;

        if (ee->block_cache[i].blocks) {
            delete[] ee->block_cache[i].blocks;

            ee->block_cache[i].blocks = nullptr;
        }
    }

    ee->last_block_lookup_pc = ~0u;
    ee->last_block_ptr = nullptr;

    ee->block_lut_gen++;
}

static inline bool ee_is_executable_region(uint32_t addr) {
    // EE should only ever execute from RAM (and its mirrors), and the BIOS
    return addr < 0x8000000 || (addr >= 0x1fc00000 && addr < 0x20000000);
}

static inline void ee_invalidate_page(struct ee_state* ee, uint32_t addr) {
    if (!ee_is_executable_region(addr))
        return;

    uint32_t page = addr / EE_MIN_PAGESIZE;

    if (ee->block_cache[page].dirty || !ee->block_cache[page].valid)
        return;

    if (addr < ee->block_cache[page].min_code_addr || addr >= ee->block_cache[page].max_code_addr)
        return;

    // printf("ee: Invalidating page at addr=%08x page=%u\n", addr, page);

    ee->block_cache[page].dirty = true;
    ee->block_lut_gen++;
}

#define INVALIDATE_CACHE_PAGE(addr) { \
    if (ee_is_executable_region(addr)) { \
        uint32_t page = (addr) / EE_MIN_PAGESIZE; \
        if (ee->block_cache[page]) { \
            ee->block_cache_dirty[page] = true; \
        } \
    } \
}

// #define INVALIDATE_CACHE_PAGE(addr) { \
//     uint32_t page = (addr) / _EE_CACHE_PAGESIZE; \
//     if (ee->block_cache[page]) { \
//         ee->block_cache[page][((addr) & (_EE_CACHE_PAGESIZE - 1)) >> 2].cycles = 0; \
//     } \
// }

#define BUS_READ_FUNC(b)                                                       \
    static inline uint64_t bus_read ## b(struct ee_state* ee, uint32_t addr) { \
        if ((addr & 0xf0000000) == 0x70000000)                                 \
            return ps2_ram_read ## b(ee->spr, addr & 0x3fff);                  \
        uint32_t phys;                                                         \
        ee_translate_virt(ee, addr, &phys);                                    \
        if (phys == 0x1000f000) ee->intc_reads++;                              \
        if (phys == 0x12001000) ee->csr_reads++;                               \
        return ee->bus.read ## b(ee->bus.udata, phys);                         \
    }

#define BUS_WRITE_FUNC(b)                                                                  \
    static inline void bus_write ## b(struct ee_state* ee, uint32_t addr, uint64_t data) { \
        if ((addr & 0xf0000000) == 0x70000000)                                             \
        { ps2_ram_write ## b(ee->spr, addr & 0x3fff, data); return; }                      \
        uint32_t phys;                                                                     \
        ee_translate_virt(ee, addr, &phys);                                                \
        ee_invalidate_page(ee, phys);                                                      \
        ee->bus.write ## b(ee->bus.udata, phys, data);                                     \
    }

BUS_READ_FUNC(8)
BUS_READ_FUNC(16)
BUS_READ_FUNC(32)
BUS_READ_FUNC(64)

static inline uint128_t bus_read128(struct ee_state* ee, uint32_t addr) {
    if ((addr & 0xf0000000) == 0x70000000)
        return ps2_ram_read128(ee->spr, addr & 0x3ff0);

    uint32_t phys;

    ee_translate_virt(ee, addr, &phys);

    return ee->bus.read128(ee->bus.udata, phys);
}

// static inline __m128i bus_read128_sse(struct ee_state* ee, uint32_t addr) {
//     uint128_t result = bus_read128(ee, addr);

//     return _mm_load_si128((__m128i*)&result);
// }

BUS_WRITE_FUNC(8)
BUS_WRITE_FUNC(16)
BUS_WRITE_FUNC(32)
BUS_WRITE_FUNC(64)

void bus_write128(struct ee_state* ee, uint32_t addr, uint128_t data) {
    if ((addr & 0xf0000000) == 0x70000000) {
        ps2_ram_write128(ee->spr, addr & 0x3ff0, data);

        return;
    }

    uint32_t phys;

    ee_translate_virt(ee, addr, &phys);
    ee_invalidate_page(ee, phys);

    ee->bus.write128(ee->bus.udata, phys, data);
}
#endif

#undef BUS_READ_FUNC
#undef BUS_WRITE_FUNC

static void ee_jit_read128(struct ee_state* ee, uint32_t addr, uint128_t* out) {
    *out = bus_read128(ee, addr);
}

static void ee_jit_write128(struct ee_state* ee, uint32_t addr, const uint128_t* in) {
    bus_write128(ee, addr, *in);
}

#define EE_FOLD_NO_PHYS 0xffffffffu
#define EE_VFAST_SPR_PAGE 0xffffffffu

static inline void* ee_vfast_page_base(struct ee_state* ee, uint32_t vaddr, int write, uint32_t* out_phys_page) {
    if ((vaddr & 0xf0000000) == 0x70000000) {
        *out_phys_page = EE_VFAST_SPR_PAGE;

        return ee->spr->buf + ((vaddr & 0x3fff) & ~0xfff);
    }

    uint32_t phys;
    int seg = ee_get_segment(vaddr);

    if (seg == EE_KSEG0 || seg == EE_KSEG1) {
        phys = vaddr & 0x1fffffff;
    } else {
        ee_page* page = &ee->pagetable[vaddr / EE_MIN_PAGESIZE];

        if (!page->valid)
            return nullptr;

        phys = (page->pfn * EE_MIN_PAGESIZE) | (vaddr & (EE_MIN_PAGESIZE - 1));
    }

    if (phys >= 0x20000000)
        return nullptr;

    struct ee_bus* bus = (struct ee_bus*)ee->bus.udata;

    if (!bus)
        return nullptr;

    void* ptr = write ? bus->fastmem_w_table[phys >> 13] : bus->fastmem_r_table[phys >> 13];

    if (!ptr)
        return nullptr;

    *out_phys_page = phys >> 12;

    return (uint8_t*)ptr + (phys & 0x1000);
}

static void* ee_fold_host_ptr(struct ee_state* ee, uint32_t vaddr, int bytes, bool write, uint32_t* out_phys) {
    if (((vaddr & 0xfff) + bytes) > 0x1000)
        return nullptr;

    uint32_t pp;

    void* base = ee_vfast_page_base(ee, vaddr, write, &pp);

    if (!base)
        return nullptr;

    *out_phys = (pp == EE_VFAST_SPR_PAGE) ? EE_FOLD_NO_PHYS : ((pp << 12) | (vaddr & 0xfff));

    return (uint8_t*)base + (vaddr & 0xfff);
}

#define EE_VFAST_ENTRIES 0x100000

void ee_vfast_clear(struct ee_state* ee) {
    if (ee->vfast_r) {
        memset(ee->vfast_r, 0, EE_VFAST_ENTRIES * sizeof(void*));
    }
}

#define EE_VFAST_READ_FUNC(b)                                                     \
    static uint64_t ee_vfast_read ## b(struct ee_state* ee, uint32_t addr) {      \
        uint32_t pp;                                                              \
        void* base = ee_vfast_page_base(ee, addr, 0, &pp);                        \
        if (base) ee->vfast_r[addr >> 12] = base;                                 \
        return bus_read ## b(ee, addr);                                           \
    }

EE_VFAST_READ_FUNC(8)
EE_VFAST_READ_FUNC(16)
EE_VFAST_READ_FUNC(32)
EE_VFAST_READ_FUNC(64)

#undef EE_VFAST_READ_FUNC

// Materializes a baked host pointer into a register. Kept separate so every
// folded access constructs its address the same way.
static inline asmjit::ujit::Gp ee_fold_base(asmjit::ujit::UniCompiler& uc, void* host) {
    asmjit::ujit::Gp base = uc.new_gp_ptr();

    uc.mov(base, asmjit::Imm((uint64_t)(uintptr_t)host));

    return base;
}

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

    ee->next_pc = addr;
}

void ee_exception_level1(struct ee_state* ee, uint32_t cause) {
    uint32_t vec = EE_VEC_COMMON;

    // Get a running region off a back edge promptly. Not load-bearing today --
    // everything reaching here terminates its sub-block anyway -- but it keeps
    // a region from looping on into a vector that has already been set.
    ee->exit_req = 1;

    switch (cause) {
        case CAUSE_EXC1_TLBL:
        case CAUSE_EXC1_TLBS:
            vec = EE_VEC_TLB;
            break;
        case CAUSE_EXC1_TLBIL:
        case CAUSE_EXC1_TLBIS:
            cause <<= 2;
            break;
        case CAUSE_EXC1_INT:
            vec = EE_VEC_IRQ;
            break;
    }

    ee->cause &= ~EE_CAUSE_EXC;
    ee->cause |= cause;

    if (!(ee->status & EE_SR_EXL)) {
        ee->epc = ee->pc;
        ee->cause &= ~EE_CAUSE_BD;
    }

    ee->status |= EE_SR_EXL;

    uint32_t addr = ((ee->status & EE_SR_BEV) ? 0xbfc00200 : 0x80000000) + vec;

    ee_set_pc(ee, addr);

    ee->pc = addr;

    // printf("ee: Exception level 1, cause=%d, vec=%08x, pc=%08x next_pc=%08x\n", cause, addr, ee->pc, ee->next_pc);
}

static inline void ee_exception_level2(struct ee_state* ee, uint32_t cause) {
    ee->exit_req = 1;   // see ee_exception_level1

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
        ee->exit_req = 1;   // let a running region's back edge bail out
    } else {
        ee->cause &= ~EE_CAUSE_IP2;
    }
}

void ee_set_int1(struct ee_state* ee, int v) {
    if (v) {
        ee->cause |= EE_CAUSE_IP3;
        ee->exit_req = 1;
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

    if ((i.opcode & 1) && vu_is_interlocked(ee->vu0)) {
        vu_execute_program_tpc(ee->vu0);
    }
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

    // printf("ee: ERET at pc=%08x next_pc=%08x\n", ee->pc, ee->next_pc);
}
static inline void ee_i_j(struct ee_state* ee, const ee_instruction& i) {
    ee_set_pc(ee, (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2));
}
static inline void ee_i_jal(struct ee_state* ee, const ee_instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    ee_set_pc(ee, (ee->next_pc & 0xf0000000) | (EE_D_I26 << 2));
}
static inline void ee_i_jalr(struct ee_state* ee, const ee_instruction& i) {
    uint32_t next_pc = ee->next_pc;

    ee_set_pc(ee, EE_RS32);

    EE_RD = next_pc;
}
static inline void ee_i_jr(struct ee_state* ee, const ee_instruction& i) {
    ee_set_pc(ee, EE_RS32);
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

static const uint32_t LWl_MASK[4] = { 0x00ffffff, 0x0000ffff, 0x000000ff, 0x00000000 };
static const uint32_t LWR_MASK[4] = { 0x00000000, 0xff000000, 0xffff0000, 0xffffff00 };
static const int LWl_SHIFT[4] = { 24, 16, 8, 0 };
static const int LWR_SHIFT[4] = { 0, 8, 16, 24 };

static inline void ee_i_lwl(struct ee_state* ee, const ee_instruction& i) {
    uint32_t addr = EE_RS32 + SE3216(EE_D_I16);
    uint32_t shift = addr & 3;
    uint32_t mem = bus_read32(ee, addr & ~3);

    // ensure the compiler does correct sign extension into 64 bits by using s32
    EE_RT = (int32_t)((EE_RT32 & LWl_MASK[shift]) | (mem << LWl_SHIFT[shift]));
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
    int d = EE_D_RD;

    if (d == 4) {
        ee->cop0_r[4] &= 0x7ffff0;
        ee->cop0_r[4] |= (EE_RT32 & 0xff800000);
    } else {
        ee->cop0_r[EE_D_RD] = EE_RT32;
    }
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
    __m128i a = _mm_load_si128((const __m128i*)t);
    __m128i f = _mm_cmpeq_epi16(a, b);
    __m128i r = _mm_add_epi16(_mm_abs_epi16(a), f);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((const __m128i*)t);
    __m128i f = _mm_cmpeq_epi32(a, b);
    __m128i r = _mm_add_epi32(_mm_abs_epi32(a), f);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_add_epi8(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_add_epi16(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_adds_epi8(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_adds_epi16(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_adds_epi32(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_adds_epu8(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_adds_epu16(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_adds_epu32(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_add_epi32(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i x = _mm_sub_epi16(a, b);
    __m128i y = _mm_add_epi16(a, b);
    __m128i r = _mm_blend_epi16(x, y, 15);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_and_si128(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_cmpeq_epi8(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_cmpeq_epi16(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_cmpeq_epi32(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_cmpgt_epi8(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_cmpgt_epi16(a, b);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)s);
    __m128i b = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_cmpgt_epi32(a, b);

    _mm_store_si128((__m128i*)d, r);
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

    __m128i m = _mm_load_si128((__m128i*)mask);
    __m128i a = _mm_load_si128((__m128i*)t);
    __m128i r = _mm_shuffle_epi8(a, m);

    _mm_store_si128((__m128i*)d, r);
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
    __m128i a = _mm_load_si128((__m128i*)&ee->r[EE_D_RT]);
    __m128i b = _mm_load_si128((__m128i*)&ee->r[EE_D_RS]);
    __m128i r = _mm_unpacklo_epi64(a, b);

    _mm_store_si128((__m128i*)&ee->r[EE_D_RD], r);
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

    // PLZCW only defines the low two words; normalize the upper half to zero so
    // the JIT (which writes the whole 128-bit register) matches.
    ee->r[EE_D_RD].u32[2] = 0;
    ee->r[EE_D_RD].u32[3] = 0;
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

    // Multiply-subtract counterpart of PMADDW, using PCSX2's recompiler
    // semantics: full 64-bit accumulator (HI<<32)|LO minus the signed product,
    // then split. Real hardware has an "off by one" multiplier quirk (PMSUBW
    // divides by 0xFFFFFFFF instead of >>32); PCSX2's JIT ignores it, and this
    // matches the JIT so the two stay in sync (see ee_mmi::pmsubw).
    uint64_t r0 = (int64_t)ee->r[s].s32[0] * (int64_t)ee->r[t].s32[0];
    uint64_t r1 = (int64_t)ee->r[s].s32[2] * (int64_t)ee->r[t].s32[2];

    ee->r[d].u64[0] = ((((uint64_t)ee->hi.u32[0]) << 32) | (uint64_t)ee->lo.u32[0]) - r0;
    ee->r[d].u64[1] = ((((uint64_t)ee->hi.u32[2]) << 32) | (uint64_t)ee->lo.u32[2]) - r1;
    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
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

static inline void ee_get_thread_list(struct ee_state* ee) {
    if (ee->thread_list_base) return;

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

static inline void ee_i_syscall(struct ee_state* ee, const ee_instruction& i) {
    uint32_t id = ee->r[3].ul64;

    if (id & 0x80000000) {
        id = (~id) + 1;
    }

    switch (id) {
        // ChangeThreadPriority
        case 0x29: {
            ee_get_thread_list(ee);
        } break;

        // SetOsdConfigParam
        case 0x4a: {
            uint32_t ptr = ee->r[4].u32[0];
            uint32_t value = bus_read32(ee, ptr);

            memcpy(&ee->osd_config, &value, 4);

            return;
        } break;

        // GetOsdConfigParam
        case 0x4b: {
            uint32_t ptr = ee->r[4].u32[0];
            uint32_t value;

            memcpy(&value, &ee->osd_config, 4);

            bus_write32(ee, ptr, value);

            return;
        } break;

        // RFU060
        case 0x3c: {
            if (ee->r[5].u32[0] == 0xffffffff) {
                ee->r[5].u32[0] = (ee->ram_size + 1) - ee->r[6].s32[0];
            }
        } break;

        // GetMemorySize
        case 0x7f: {
            ee->r[2].u32[0] = ee->ram_size + 1;

            return;
        } break;

        // LoadPS2
        // LoadExecPS2
        case 0x06:
        case 0x07: {
            // Note: This prevents keeping stale cache blocks
            //       stored in memory when switching games/software.
            ee->pending_purge = true;
        } break;

        // FlushCache
        case 0x64: {
            // printf("ee: Flushed %zd blocks\n", ee->block_cache.size());
        } break;
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
static inline void ee_i_tlbp(struct ee_state* ee, const ee_instruction& i) {
    int index = ee->index & 0x3f;

    struct ee_vtlb_entry* entry = &ee->vtlb[index];

    if ((ee->entryhi & 0xffffe000) == entry->vpn2 && (ee->entryhi & 0xff) == entry->asid) {
        ee->index |= 0x80000000;
    } else {
        ee->index &= ~0x80000000;
    }
}
static inline void ee_i_tlbr(struct ee_state* ee, const ee_instruction& i) {
    int index = ee->index & 0x3f;

    struct ee_vtlb_entry* entry = &ee->vtlb[index];

    ee->entryhi = entry->vpn2 | entry->asid;
    ee->entrylo0 = (entry->pfn0 >> 6) | (entry->v0 << 1) | (entry->d0 << 2) | (entry->c0 << 3) | (entry->s << 31) | (entry->g);
    ee->entrylo1 = (entry->pfn1 >> 6) | (entry->v1 << 1) | (entry->d1 << 2) | (entry->c1 << 3) | (entry->s << 31) | (entry->g);
    ee->pagemask = entry->mask;
}
static inline void ee_write_pagetable(struct ee_state* ee, const struct ee_vtlb_entry* entry) {
    uint32_t pagecount;

    switch ((ee->pagemask >> 13) & 0xfff) {
        case 0x000: { pagecount = 1 << 0; } break;
        case 0x003: { pagecount = 1 << 2; } break;
        case 0x00f: { pagecount = 1 << 4; } break;
        case 0x03f: { pagecount = 1 << 6; } break;
        case 0x0ff: { pagecount = 1 << 8; } break;
        case 0x3ff: { pagecount = 1 << 10; } break;
        case 0xfff: { pagecount = 1 << 12; } break;
    }

    uint32_t vpn0 = entry->vpn2 / EE_MIN_PAGESIZE;
    uint32_t vpn1 = vpn0 + pagecount;
    uint32_t pfn0 = entry->pfn0 / EE_MIN_PAGESIZE;
    uint32_t pfn1 = entry->pfn1 / EE_MIN_PAGESIZE;

    for (int i = 0; i < pagecount; i++) {
        ee->pagetable[vpn0+i].pfn = pfn0 + i;
        ee->pagetable[vpn0+i].dirty = entry->d0;
        ee->pagetable[vpn0+i].valid = entry->v0;
        ee->pagetable[vpn0+i].spr = entry->s;
        ee->pagetable[vpn0+i].global = entry->g;
        ee->pagetable[vpn1+i].pfn = pfn1 + i;
        ee->pagetable[vpn1+i].dirty = entry->d1;
        ee->pagetable[vpn1+i].valid = entry->v1;
        ee->pagetable[vpn1+i].spr = entry->s;
        ee->pagetable[vpn1+i].global = entry->g;
    }

    ee_flush_cache(ee);
}
static inline void ee_i_tlbwi(struct ee_state* ee, const ee_instruction& i) {
    struct ee_vtlb_entry* entry = &ee->vtlb[ee->index & 0x3f];

    entry->asid = ee->entryhi & 0xff;
    entry->pfn0 = (ee->entrylo0 & 0x3ffffc0) << 6;
    entry->pfn1 = (ee->entrylo1 & 0x3ffffc0) << 6;
    entry->mask = ee->pagemask & 0x1ffe000;
    entry->vpn2 = ee->entryhi & 0xffffe000;
    entry->v0 = (ee->entrylo0 >> 1) & 1;
    entry->d0 = (ee->entrylo0 >> 2) & 1;
    entry->c0 = (ee->entrylo0 >> 3) & 7;
    entry->v1 = (ee->entrylo1 >> 1) & 1;
    entry->d1 = (ee->entrylo1 >> 2) & 1;
    entry->c1 = (ee->entrylo1 >> 3) & 7;
    entry->s = (ee->entrylo0 >> 31) & 1;
    entry->g = (ee->entrylo0 & 1) && (ee->entrylo1 & 1);

    ee_write_pagetable(ee, entry);

    // printf("ee: Index=%d vpn2=%08x even={pfn=%08x v=%d d=%d} odd={pfn=%08x v=%d d=%d} mask=%08x s=%d g=%d\n",
    //     ee->index,
    //     entry->vpn2,
    //     entry->pfn0,
    //     entry->v0,
    //     entry->d0,
    //     entry->pfn1,
    //     entry->v1,
    //     entry->d1,
    //     entry->mask,
    //     entry->s,
    //     entry->g
    // );
}
static inline void ee_i_tlbwr(struct ee_state* ee, const ee_instruction& i) {
    int index = (ee->count % (48 - ee->wired)) + ee->wired;

    struct ee_vtlb_entry* entry = &ee->vtlb[index];

    entry->asid = ee->entryhi & 0xff;
    entry->pfn0 = (ee->entrylo0 & 0x3ffffc0) << 6;
    entry->pfn1 = (ee->entrylo1 & 0x3ffffc0) << 6;
    entry->mask = ee->pagemask & 0x1ffe000;
    entry->vpn2 = ee->entryhi & 0xffffe000;
    entry->v0 = (ee->entrylo0 >> 1) & 1;
    entry->d0 = (ee->entrylo0 >> 2) & 1;
    entry->c0 = (ee->entrylo0 >> 3) & 7;
    entry->v1 = (ee->entrylo1 >> 1) & 1;
    entry->d1 = (ee->entrylo1 >> 2) & 1;
    entry->c1 = (ee->entrylo1 >> 3) & 7;
    entry->s = (ee->entrylo0 >> 31) & 1;
    entry->g = (ee->entrylo0 & 1) && (ee->entrylo1 & 1);

    ee_write_pagetable(ee, entry);

    // printf("ee: tlbwr Index=%d vpn2=%08x even={pfn=%08x v=%d d=%d} odd={pfn=%08x v=%d d=%d} mask=%08x s=%d g=%d\n",
    //     index,
    //     entry->vpn2,
    //     entry->pfn0,
    //     entry->v0,
    //     entry->d0,
    //     entry->pfn1,
    //     entry->v1,
    //     entry->d1,
    //     entry->mask,
    //     entry->s,
    //     entry->g
    // );
}
static inline void ee_i_tlt(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tlt unimplemented\n"); exit(1); }
static inline void ee_i_tlti(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tlti unimplemented\n"); exit(1); }
static inline void ee_i_tltiu(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tltiu unimplemented\n"); exit(1); }
static inline void ee_i_tltu(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tltu unimplemented\n"); exit(1); }
static inline void ee_i_tne(struct ee_state* ee, const ee_instruction& i) {
    if (EE_RS != EE_RT) ee_exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void ee_i_tnei(struct ee_state* ee, const ee_instruction& i) { fprintf(stderr, "ee: tnei unimplemented\n"); exit(1); }
static inline void ee_i_vabs(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(abs) }
static inline void ee_i_vadd(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(add) }
static inline void ee_i_vadda(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(adda) }
static inline void ee_i_vaddai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addai) }
static inline void ee_i_vaddaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addaq) }
static inline void ee_i_vaddaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addaw) }
static inline void ee_i_vaddax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addax) }
static inline void ee_i_vadday(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(adday) }
static inline void ee_i_vaddaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addaz) }
static inline void ee_i_vaddi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addi) }
static inline void ee_i_vaddq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addq) }
static inline void ee_i_vaddw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addw) }
static inline void ee_i_vaddx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addx) }
static inline void ee_i_vaddy(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addy) }
static inline void ee_i_vaddz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(addz) }
static inline void ee_i_vcallms(struct ee_state* ee, const ee_instruction& i) {
    vu_execute_program(ee->vu0, EE_D_I15);
}
static inline void ee_i_vcallmsr(struct ee_state* ee, const ee_instruction& i) {
    vu_execute_program(ee->vu0, ee->vu0->cmsar0);
}
static inline void ee_i_vclipw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(clip) }
static inline void ee_i_vdiv(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(div) ee->vu0->q_delay = 0; }
static inline void ee_i_vftoi0(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(ftoi0) }
static inline void ee_i_vftoi12(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(ftoi12) }
static inline void ee_i_vftoi15(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(ftoi15) }
static inline void ee_i_vftoi4(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(ftoi4) }
static inline void ee_i_viadd(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(iadd) }
static inline void ee_i_viaddi(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(iaddi) }
static inline void ee_i_viand(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(iand) }
static inline void ee_i_vilwr(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(ilwr) }
static inline void ee_i_vior(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(ior) }
static inline void ee_i_visub(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(isub) }
static inline void ee_i_viswr(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(iswr) }
static inline void ee_i_vitof0(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(itof0) }
static inline void ee_i_vitof12(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(itof12) }
static inline void ee_i_vitof15(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(itof15) }
static inline void ee_i_vitof4(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(itof4) }
static inline void ee_i_vlqd(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(lqd) }
static inline void ee_i_vlqi(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(lqi) }
static inline void ee_i_vmadd(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(madd) }
static inline void ee_i_vmadda(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(madda) }
static inline void ee_i_vmaddai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddai) }
static inline void ee_i_vmaddaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddaq) }
static inline void ee_i_vmaddaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddaw) }
static inline void ee_i_vmaddax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddax) }
static inline void ee_i_vmadday(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(madday) }
static inline void ee_i_vmaddaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddaz) }
static inline void ee_i_vmaddi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddi) }
static inline void ee_i_vmaddq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddq) }
static inline void ee_i_vmaddw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddw) }
static inline void ee_i_vmaddx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddx) }
static inline void ee_i_vmaddy(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddy) }
static inline void ee_i_vmaddz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maddz) }
static inline void ee_i_vmax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(max) }
static inline void ee_i_vmaxi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maxi) }
static inline void ee_i_vmaxw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maxw) }
static inline void ee_i_vmaxx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maxx) }
static inline void ee_i_vmaxy(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maxy) }
static inline void ee_i_vmaxz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(maxz) }
static inline void ee_i_vmfir(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mfir) }
static inline void ee_i_vmini(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mini) }
static inline void ee_i_vminii(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(minii) }
static inline void ee_i_vminiw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(miniw) }
static inline void ee_i_vminix(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(minix) }
static inline void ee_i_vminiy(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(miniy) }
static inline void ee_i_vminiz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(miniz) }
static inline void ee_i_vmove(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(move) }
static inline void ee_i_vmr32(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(mr32) }
static inline void ee_i_vmsub(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msub) }
static inline void ee_i_vmsuba(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msuba) }
static inline void ee_i_vmsubai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubai) }
static inline void ee_i_vmsubaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubaq) }
static inline void ee_i_vmsubaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubaw) }
static inline void ee_i_vmsubax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubax) }
static inline void ee_i_vmsubay(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubay) }
static inline void ee_i_vmsubaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubaz) }
static inline void ee_i_vmsubi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubi) }
static inline void ee_i_vmsubq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubq) }
static inline void ee_i_vmsubw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubw) }
static inline void ee_i_vmsubx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubx) }
static inline void ee_i_vmsuby(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msuby) }
static inline void ee_i_vmsubz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(msubz) }
static inline void ee_i_vmtir(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(mtir) }
static inline void ee_i_vmul(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mul) }
static inline void ee_i_vmula(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mula) }
static inline void ee_i_vmulai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulai) }
static inline void ee_i_vmulaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulaq) }
static inline void ee_i_vmulaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulaw) }
static inline void ee_i_vmulax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulax) }
static inline void ee_i_vmulay(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulay) }
static inline void ee_i_vmulaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulaz) }
static inline void ee_i_vmuli(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(muli) }
static inline void ee_i_vmulq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulq) }
static inline void ee_i_vmulw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulw) }
static inline void ee_i_vmulx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulx) }
static inline void ee_i_vmuly(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(muly) }
static inline void ee_i_vmulz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(mulz) }
static inline void ee_i_vnop(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(nop) }
static inline void ee_i_vopmsub(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(opmsub) }
static inline void ee_i_vopmula(struct ee_state* ee, const ee_instruction& i) { VU_UPPER(opmula) }
static inline void ee_i_vrget(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(rget) }
static inline void ee_i_vrinit(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(rinit) }
static inline void ee_i_vrnext(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(rnext) }
static inline void ee_i_vrsqrt(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(rsqrt) ee->vu0->q_delay = 0; }
static inline void ee_i_vrxor(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(rxor) }
static inline void ee_i_vsqd(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(sqd) }
static inline void ee_i_vsqi(struct ee_state* ee, const ee_instruction& i) { VU_LOWER_TEMPLATE(sqi) }
static inline void ee_i_vsqrt(struct ee_state* ee, const ee_instruction& i) { VU_LOWER(sqrt) ee->vu0->q_delay = 0; }
static inline void ee_i_vsub(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(sub) }
static inline void ee_i_vsuba(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(suba) }
static inline void ee_i_vsubai(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subai) }
static inline void ee_i_vsubaq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subaq) }
static inline void ee_i_vsubaw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subaw) }
static inline void ee_i_vsubax(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subax) }
static inline void ee_i_vsubay(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subay) }
static inline void ee_i_vsubaz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subaz) }
static inline void ee_i_vsubi(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subi) }
static inline void ee_i_vsubq(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subq) }
static inline void ee_i_vsubw(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subw) }
static inline void ee_i_vsubx(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subx) }
static inline void ee_i_vsuby(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(suby) }
static inline void ee_i_vsubz(struct ee_state* ee, const ee_instruction& i) { VU_UPPER_TEMPLATE(subz) }
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

struct ee_state* ee_create(void) {
    return new ee_state();
}

void ee_init(struct ee_state* ee, struct vu_state* vu0, struct vu_state* vu1, int ram_size, struct ee_bus_s bus) {
    ee->prid = 0x2e20;
    ee->pc = EE_VEC_RESET;
    ee->next_pc = ee->pc + 4;
    ee->bus = bus;
    ee->vu0 = vu0;
    ee->vu1 = vu1;

    // Initialize block lookup cache (intentionally mismatches so first real lookup succeeds)
    ee->last_block_lookup_pc = ~0u;
    ee->last_block_ptr = nullptr;

    // gen starts at 1 so the zero-initialized LUT entries (gen 0) never match
    ee->block_lut_gen = 1;

    // Inline load fast path: virtual-page -> host-pointer table, filled lazily.
    // Null only if the allocation fails, in which case loads fall back to the bus.
    ee->vfast_r = (void**)calloc(EE_VFAST_ENTRIES, sizeof(void*));

    // To-do: Set SR

    ee->spr = ps2_ram_create();
    ps2_ram_init(ee->spr, 0x4000);

    // EE's FPU uses round to zero by default
    fesetround(FE_TOWARDZERO);

    ee->fcr = 0x01000001;
    ee->ram_size = ram_size - 1;

    ee->osd_config.screen_type = 1; // 4:3
    ee->osd_config.ps1drv_config = 0; // ???
    ee->osd_config.spdif_mode = 0; // Enabled
    ee->osd_config.timezone_offset = 0;
    ee->osd_config.video_output = 0; // RGB
    ee->osd_config.jap_language = 1; // Indicates not Japanese
    ee->osd_config.language = 1; // English
    ee->osd_config.version = 1; // Indicates normal kernel without extended language settings

    ee->block_cache.clear();
    ee->block_cache.resize(EE_CACHE_PAGECOUNT);

    ee->logger = new asmjit::FileLogger(stdout);

    for (int i = 0; i < 32; i++) {
        ee->reg_cache[i].valid = false;
    }
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

    ee_purge_cache(ee);

    ee->rt.reset(asmjit::ResetPolicy::kHard);

    fesetround(FE_TOWARDZERO);

    ps2_ram_reset(ee->spr);

    ee->fcr = 0x01000001;
}

void ee_destroy(struct ee_state* ee) {
    ps2_ram_destroy(ee->spr);

    free(ee->vfast_r);

    delete ee->logger;
    delete ee;
}

#define EE_BRANCH_NORMAL 1
#define EE_BRANCH_IMMEDIATE 2
#define EE_BRANCH_LIKELY 3
#define EE_BRANCH_COND 4

ee_instruction ee_decode(uint32_t opcode) {
    ee_instruction i;

    i.opcode = opcode;
    i.rs.r = (opcode >> 21) & 0x1f;
    i.rt.r = (opcode >> 16) & 0x1f;
    i.rd.r = (opcode >> 11) & 0x1f;
    i.sa = (opcode >> 6) & 0x1f;
    i.i15 = (opcode >> 6) & 0x7fff;
    i.i16 = opcode & 0xffff;
    i.i26 = opcode & 0x3ffffff;

    i.branch = 0;
    i.cycles = 0;

    switch ((opcode & 0xFC000000) >> 26) {
        case 0x00000000 >> 26: { // special
            switch (opcode & 0x0000003F) {
                case 0x00000000: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SLL; i.func = ee_i_sll; return i;
                case 0x00000002: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SRL; i.func = ee_i_srl; return i;
                case 0x00000003: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SRA; i.func = ee_i_sra; return i;
                case 0x00000004: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SLLV; i.func = ee_i_sllv; return i;
                case 0x00000006: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SRLV; i.func = ee_i_srlv; return i;
                case 0x00000007: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SRAV; i.func = ee_i_srav; return i;
                case 0x00000008: i.cycles = EE_CYC_DEFAULT; i.branch = 1; i.id = EE_I_JR; i.func = ee_i_jr; return i;
                case 0x00000009: i.cycles = EE_CYC_DEFAULT; i.branch = 1; i.id = EE_I_JALR; i.func = ee_i_jalr; return i;
                case 0x0000000A: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MOVZ; i.func = ee_i_movz; return i;
                case 0x0000000B: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MOVN; i.func = ee_i_movn; return i;
                case 0x0000000C: i.cycles = EE_CYC_DEFAULT; i.branch = 2; i.id = EE_I_SYSCALL; i.func = ee_i_syscall; return i;
                case 0x0000000D: i.cycles = EE_CYC_DEFAULT; i.branch = 2; i.id = EE_I_BREAK; i.func = ee_i_break; return i;
                case 0x0000000F: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SYNC; i.func = ee_i_sync; return i;
                case 0x00000010: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MFHI; i.func = ee_i_mfhi; return i;
                case 0x00000011: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MTHI; i.func = ee_i_mthi; return i;
                case 0x00000012: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MFLO; i.func = ee_i_mflo; return i;
                case 0x00000013: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MTLO; i.func = ee_i_mtlo; return i;
                case 0x00000014: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSLLV; i.func = ee_i_dsllv; return i;
                case 0x00000016: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSRLV; i.func = ee_i_dsrlv; return i;
                case 0x00000017: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSRAV; i.func = ee_i_dsrav; return i;
                case 0x00000018: i.cycles = EE_CYC_MULT; i.id = EE_I_MULT; i.func = ee_i_mult; return i;
                case 0x00000019: i.cycles = EE_CYC_MULT; i.id = EE_I_MULTU; i.func = ee_i_multu; return i;
                case 0x0000001A: i.cycles = EE_CYC_DIV; i.id = EE_I_DIV; i.func = ee_i_div; return i;
                case 0x0000001B: i.cycles = EE_CYC_DIV; i.id = EE_I_DIVU; i.func = ee_i_divu; return i;
                case 0x00000020: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_ADD; i.func = ee_i_add; return i;
                case 0x00000021: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_ADDU; i.func = ee_i_addu; return i;
                case 0x00000022: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SUB; i.func = ee_i_sub; return i;
                case 0x00000023: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SUBU; i.func = ee_i_subu; return i;
                case 0x00000024: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_AND; i.func = ee_i_and; return i;
                case 0x00000025: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_OR; i.func = ee_i_or; return i;
                case 0x00000026: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_XOR; i.func = ee_i_xor; return i;
                case 0x00000027: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_NOR; i.func = ee_i_nor; return i;
                case 0x00000028: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MFSA; i.func = ee_i_mfsa; return i;
                case 0x00000029: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MTSA; i.func = ee_i_mtsa; return i;
                case 0x0000002A: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SLT; i.func = ee_i_slt; return i;
                case 0x0000002B: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SLTU; i.func = ee_i_sltu; return i;
                case 0x0000002C: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DADD; i.func = ee_i_dadd; return i;
                case 0x0000002D: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DADDU; i.func = ee_i_daddu; return i;
                case 0x0000002E: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSUB; i.func = ee_i_dsub; return i;
                case 0x0000002F: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSUBU; i.func = ee_i_dsubu; return i;
                case 0x00000030: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TGE; i.func = ee_i_tge; return i;
                case 0x00000031: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TGEU; i.func = ee_i_tgeu; return i;
                case 0x00000032: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TLT; i.func = ee_i_tlt; return i;
                case 0x00000033: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TLTU; i.func = ee_i_tltu; return i;
                case 0x00000034: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TEQ; i.func = ee_i_teq; return i;
                case 0x00000036: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TNE; i.func = ee_i_tne; return i;
                case 0x00000038: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSLL; i.func = ee_i_dsll; return i;
                case 0x0000003A: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSRL; i.func = ee_i_dsrl; return i;
                case 0x0000003B: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSRA; i.func = ee_i_dsra; return i;
                case 0x0000003C: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSLL32; i.func = ee_i_dsll32; return i;
                case 0x0000003E: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSRL32; i.func = ee_i_dsrl32; return i;
                case 0x0000003F: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DSRA32; i.func = ee_i_dsra32; return i;
            }
        } break;
        case 0x04000000 >> 26: { // regimm
            switch ((opcode & 0x001F0000) >> 16) {
                case 0x00000000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BLTZ; i.func = ee_i_bltz; return i;
                case 0x00010000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BGEZ; i.func = ee_i_bgez; return i;
                case 0x00020000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BLTZL; i.func = ee_i_bltzl; return i;
                case 0x00030000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BGEZL; i.func = ee_i_bgezl; return i;
                case 0x00080000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TGEI; i.func = ee_i_tgei; return i;
                case 0x00090000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TGEIU; i.func = ee_i_tgeiu; return i;
                case 0x000A0000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TLTI; i.func = ee_i_tlti; return i;
                case 0x000B0000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TLTIU; i.func = ee_i_tltiu; return i;
                case 0x000C0000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TEQI; i.func = ee_i_teqi; return i;
                case 0x000E0000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 4; i.id = EE_I_TNEI; i.func = ee_i_tnei; return i;
                case 0x00100000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BLTZAL; i.func = ee_i_bltzal; return i;
                case 0x00110000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BGEZAL; i.func = ee_i_bgezal; return i;
                case 0x00120000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BLTZALL; i.func = ee_i_bltzall; return i;
                case 0x00130000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BGEZALL; i.func = ee_i_bgezall; return i;
                case 0x00180000 >> 16: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MTSAB; i.func = ee_i_mtsab; return i;
                case 0x00190000 >> 16: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_MTSAH; i.func = ee_i_mtsah; return i;
            }
        } break;
        case 0x08000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.branch = 1; i.id = EE_I_J; i.func = ee_i_j; return i;
        case 0x0C000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.branch = 1; i.id = EE_I_JAL; i.func = ee_i_jal; return i;
        case 0x10000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BEQ; i.func = ee_i_beq; return i;
        case 0x14000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BNE; i.func = ee_i_bne; return i;
        case 0x18000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BLEZ; i.func = ee_i_blez; return i;
        case 0x1C000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BGTZ; i.func = ee_i_bgtz; return i;
        case 0x20000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_ADDI; i.func = ee_i_addi; return i;
        case 0x24000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_ADDIU; i.func = ee_i_addiu; return i;
        case 0x28000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SLTI; i.func = ee_i_slti; return i;
        case 0x2C000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_SLTIU; i.func = ee_i_sltiu; return i;
        case 0x30000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_ANDI; i.func = ee_i_andi; return i;
        case 0x34000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_ORI; i.func = ee_i_ori; return i;
        case 0x38000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_XORI; i.func = ee_i_xori; return i;
        case 0x3C000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_LUI; i.func = ee_i_lui; return i;
        case 0x40000000 >> 26: { // cop0
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00000000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MFC0; i.func = ee_i_mfc0; return i;
                case 0x00800000 >> 21: if (i.rd.r == 12) i.branch = 2; i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MTC0; i.func = ee_i_mtc0; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BC0F; i.func = ee_i_bc0f; return i;
                        case 0x00010000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BC0T; i.func = ee_i_bc0t; return i;
                        case 0x00020000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BC0FL; i.func = ee_i_bc0fl; return i;
                        case 0x00030000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BC0TL; i.func = ee_i_bc0tl; return i;
                    }
                } break;
                case 0x02000000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000001: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_TLBR; i.func = ee_i_tlbr; return i;
                        case 0x00000002: i.cycles = EE_CYC_COP_DEFAULT; i.branch = 2; i.id = EE_I_TLBWI; i.func = ee_i_tlbwi; return i;
                        case 0x00000006: i.cycles = EE_CYC_COP_DEFAULT; i.branch = 2; i.id = EE_I_TLBWR; i.func = ee_i_tlbwr; return i;
                        case 0x00000008: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_TLBP; i.func = ee_i_tlbp; return i;
                        case 0x00000018: i.cycles = EE_CYC_COP_DEFAULT; i.branch = 2; i.id = EE_I_ERET; i.func = ee_i_eret; return i;
                        case 0x00000038: i.cycles = EE_CYC_COP_DEFAULT; i.branch = 2; i.id = EE_I_EI; i.func = ee_i_ei; return i;
                        case 0x00000039: i.cycles = EE_CYC_COP_DEFAULT; i.branch = 2; i.id = EE_I_DI; i.func = ee_i_di; return i;
                    }
                } break;
            }
        } break;
        case 0x44000000 >> 26: { // cop1
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00000000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MFC1; i.func = ee_i_mfc1; return i;
                case 0x00400000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CFC1; i.func = ee_i_cfc1; return i;
                case 0x00800000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MTC1; i.func = ee_i_mtc1; return i;
                case 0x00C00000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CTC1; i.func = ee_i_ctc1; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BC1F; i.func = ee_i_bc1f; return i;
                        case 0x00010000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BC1T; i.func = ee_i_bc1t; return i;
                        case 0x00020000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BC1FL; i.func = ee_i_bc1fl; return i;
                        case 0x00030000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BC1TL; i.func = ee_i_bc1tl; return i;
                    }
                } break;
                case 0x02000000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000000: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_ADDS; i.func = ee_i_adds; return i;
                        case 0x00000001: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_SUBS; i.func = ee_i_subs; return i;
                        case 0x00000002: i.cycles = EE_CYC_FPU_MULT; i.id = EE_I_MULS; i.func = ee_i_muls; return i;
                        case 0x00000003: i.cycles = EE_CYC_FPU_DIV; i.id = EE_I_DIVS; i.func = ee_i_divs; return i;
                        case 0x00000004: i.cycles = EE_CYC_FPU_DIV; i.id = EE_I_SQRTS; i.func = ee_i_sqrts; return i;
                        case 0x00000005: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_ABSS; i.func = ee_i_abss; return i;
                        case 0x00000006: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MOVS; i.func = ee_i_movs; return i;
                        case 0x00000007: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_NEGS; i.func = ee_i_negs; return i;
                        case 0x00000016: i.cycles = EE_CYC_FPU_DIV; i.id = EE_I_RSQRTS; i.func = ee_i_rsqrts; return i;
                        case 0x00000018: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_ADDAS; i.func = ee_i_addas; return i;
                        case 0x00000019: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_SUBAS; i.func = ee_i_subas; return i;
                        case 0x0000001A: i.cycles = EE_CYC_FPU_MULT; i.id = EE_I_MULAS; i.func = ee_i_mulas; return i;
                        case 0x0000001C: i.cycles = EE_CYC_FPU_MULT; i.id = EE_I_MADDS; i.func = ee_i_madds; return i;
                        case 0x0000001D: i.cycles = EE_CYC_FPU_MULT; i.id = EE_I_MSUBS; i.func = ee_i_msubs; return i;
                        case 0x0000001E: i.cycles = EE_CYC_FPU_MULT; i.id = EE_I_MADDAS; i.func = ee_i_maddas; return i;
                        case 0x0000001F: i.cycles = EE_CYC_FPU_MULT; i.id = EE_I_MSUBAS; i.func = ee_i_msubas; return i;
                        case 0x00000024: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CVTW; i.func = ee_i_cvtw; return i;
                        case 0x00000028: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MAXS; i.func = ee_i_maxs; return i;
                        case 0x00000029: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MINS; i.func = ee_i_mins; return i;
                        case 0x00000030: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CF; i.func = ee_i_cf; return i;
                        case 0x00000032: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CEQ; i.func = ee_i_ceq; return i;
                        case 0x00000034: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CLT; i.func = ee_i_clt; return i;
                        case 0x00000036: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CLE; i.func = ee_i_cle; return i;
                    }
                } break;
                case 0x02800000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000020: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CVTS; i.func = ee_i_cvts; return i;
                    }
                } break;
            }
        } break;
        case 0x48000000 >> 26: { // cop2
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00200000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_QMFC2; i.func = ee_i_qmfc2; return i;
                case 0x00400000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CFC2; i.func = ee_i_cfc2; return i;
                case 0x00A00000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_QMTC2; i.func = ee_i_qmtc2; return i;
                case 0x00C00000 >> 21: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_CTC2; i.func = ee_i_ctc2; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BC2F; i.func = ee_i_bc2f; return i;
                        case 0x00010000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 1; i.id = EE_I_BC2T; i.func = ee_i_bc2t; return i;
                        case 0x00020000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BC2FL; i.func = ee_i_bc2fl; return i;
                        case 0x00030000 >> 16: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BC2TL; i.func = ee_i_bc2tl; return i;
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
                        case 0x00000000: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDX; i.func = ee_i_vaddx; return i;
                        case 0x00000001: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDY; i.func = ee_i_vaddy; return i;
                        case 0x00000002: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDZ; i.func = ee_i_vaddz; return i;
                        case 0x00000003: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDW; i.func = ee_i_vaddw; return i;
                        case 0x00000004: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBX; i.func = ee_i_vsubx; return i;
                        case 0x00000005: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBY; i.func = ee_i_vsuby; return i;
                        case 0x00000006: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBZ; i.func = ee_i_vsubz; return i;
                        case 0x00000007: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBW; i.func = ee_i_vsubw; return i;
                        case 0x00000008: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDX; i.func = ee_i_vmaddx; return i;
                        case 0x00000009: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDY; i.func = ee_i_vmaddy; return i;
                        case 0x0000000A: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDZ; i.func = ee_i_vmaddz; return i;
                        case 0x0000000B: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDW; i.func = ee_i_vmaddw; return i;
                        case 0x0000000C: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBX; i.func = ee_i_vmsubx; return i;
                        case 0x0000000D: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBY; i.func = ee_i_vmsuby; return i;
                        case 0x0000000E: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBZ; i.func = ee_i_vmsubz; return i;
                        case 0x0000000F: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBW; i.func = ee_i_vmsubw; return i;
                        case 0x00000010: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMAXX; i.func = ee_i_vmaxx; return i;
                        case 0x00000011: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMAXY; i.func = ee_i_vmaxy; return i;
                        case 0x00000012: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMAXZ; i.func = ee_i_vmaxz; return i;
                        case 0x00000013: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMAXW; i.func = ee_i_vmaxw; return i;
                        case 0x00000014: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMINIX; i.func = ee_i_vminix; return i;
                        case 0x00000015: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMINIY; i.func = ee_i_vminiy; return i;
                        case 0x00000016: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMINIZ; i.func = ee_i_vminiz; return i;
                        case 0x00000017: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMINIW; i.func = ee_i_vminiw; return i;
                        case 0x00000018: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULX; i.func = ee_i_vmulx; return i;
                        case 0x00000019: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULY; i.func = ee_i_vmuly; return i;
                        case 0x0000001A: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULZ; i.func = ee_i_vmulz; return i;
                        case 0x0000001B: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULW; i.func = ee_i_vmulw; return i;
                        case 0x0000001C: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULQ; i.func = ee_i_vmulq; return i;
                        case 0x0000001D: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMAXI; i.func = ee_i_vmaxi; return i;
                        case 0x0000001E: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULI; i.func = ee_i_vmuli; return i;
                        case 0x0000001F: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMINII; i.func = ee_i_vminii; return i;
                        case 0x00000020: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDQ; i.func = ee_i_vaddq; return i;
                        case 0x00000021: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDQ; i.func = ee_i_vmaddq; return i;
                        case 0x00000022: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDI; i.func = ee_i_vaddi; return i;
                        case 0x00000023: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDI; i.func = ee_i_vmaddi; return i;
                        case 0x00000024: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBQ; i.func = ee_i_vsubq; return i;
                        case 0x00000025: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBQ; i.func = ee_i_vmsubq; return i;
                        case 0x00000026: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBI; i.func = ee_i_vsubi; return i;
                        case 0x00000027: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBI; i.func = ee_i_vmsubi; return i;
                        case 0x00000028: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADD; i.func = ee_i_vadd; return i;
                        case 0x00000029: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADD; i.func = ee_i_vmadd; return i;
                        case 0x0000002A: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMUL; i.func = ee_i_vmul; return i;
                        case 0x0000002B: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMAX; i.func = ee_i_vmax; return i;
                        case 0x0000002C: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUB; i.func = ee_i_vsub; return i;
                        case 0x0000002D: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUB; i.func = ee_i_vmsub; return i;
                        case 0x0000002E: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VOPMSUB; i.func = ee_i_vopmsub; return i;
                        case 0x0000002F: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMINI; i.func = ee_i_vmini; return i;
                        case 0x00000030: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VIADD; i.func = ee_i_viadd; return i;
                        case 0x00000031: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VISUB; i.func = ee_i_visub; return i;
                        case 0x00000032: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VIADDI; i.func = ee_i_viaddi; return i;
                        case 0x00000034: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VIAND; i.func = ee_i_viand; return i;
                        case 0x00000035: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VIOR; i.func = ee_i_vior; return i;
                        case 0x00000038: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VCALLMS; i.func = ee_i_vcallms; return i;
                        case 0x00000039: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VCALLMSR; i.func = ee_i_vcallmsr; return i;
                        case 0x0000003C:
                        case 0x0000003D:
                        case 0x0000003E:
                        case 0x0000003F: {
                            uint32_t func = (opcode & 3) | ((opcode & 0x7c0) >> 4);

                            switch (func) {
                                case 0x00000000: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDAX; i.func = ee_i_vaddax; return i;
                                case 0x00000001: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDAY; i.func = ee_i_vadday; return i;
                                case 0x00000002: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDAZ; i.func = ee_i_vaddaz; return i;
                                case 0x00000003: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDAW; i.func = ee_i_vaddaw; return i;
                                case 0x00000004: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBAX; i.func = ee_i_vsubax; return i;
                                case 0x00000005: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBAY; i.func = ee_i_vsubay; return i;
                                case 0x00000006: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBAZ; i.func = ee_i_vsubaz; return i;
                                case 0x00000007: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBAW; i.func = ee_i_vsubaw; return i;
                                case 0x00000008: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDAX; i.func = ee_i_vmaddax; return i;
                                case 0x00000009: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDAY; i.func = ee_i_vmadday; return i;
                                case 0x0000000A: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDAZ; i.func = ee_i_vmaddaz; return i;
                                case 0x0000000B: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDAW; i.func = ee_i_vmaddaw; return i;
                                case 0x0000000C: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBAX; i.func = ee_i_vmsubax; return i;
                                case 0x0000000D: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBAY; i.func = ee_i_vmsubay; return i;
                                case 0x0000000E: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBAZ; i.func = ee_i_vmsubaz; return i;
                                case 0x0000000F: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBAW; i.func = ee_i_vmsubaw; return i;
                                case 0x00000010: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VITOF0; i.func = ee_i_vitof0; return i;
                                case 0x00000011: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VITOF4; i.func = ee_i_vitof4; return i;
                                case 0x00000012: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VITOF12; i.func = ee_i_vitof12; return i;
                                case 0x00000013: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VITOF15; i.func = ee_i_vitof15; return i;
                                case 0x00000014: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VFTOI0; i.func = ee_i_vftoi0; return i;
                                case 0x00000015: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VFTOI4; i.func = ee_i_vftoi4; return i;
                                case 0x00000016: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VFTOI12; i.func = ee_i_vftoi12; return i;
                                case 0x00000017: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VFTOI15; i.func = ee_i_vftoi15; return i;
                                case 0x00000018: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULAX; i.func = ee_i_vmulax; return i;
                                case 0x00000019: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULAY; i.func = ee_i_vmulay; return i;
                                case 0x0000001A: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULAZ; i.func = ee_i_vmulaz; return i;
                                case 0x0000001B: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULAW; i.func = ee_i_vmulaw; return i;
                                case 0x0000001C: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULAQ; i.func = ee_i_vmulaq; return i;
                                case 0x0000001D: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VABS; i.func = ee_i_vabs; return i;
                                case 0x0000001E: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULAI; i.func = ee_i_vmulai; return i;
                                case 0x0000001F: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VCLIPW; i.func = ee_i_vclipw; return i;
                                case 0x00000020: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDAQ; i.func = ee_i_vaddaq; return i;
                                case 0x00000021: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDAQ; i.func = ee_i_vmaddaq; return i;
                                case 0x00000022: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDAI; i.func = ee_i_vaddai; return i;
                                case 0x00000023: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDAI; i.func = ee_i_vmaddai; return i;
                                case 0x00000024: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBAQ; i.func = ee_i_vsubaq; return i;
                                case 0x00000025: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBAQ; i.func = ee_i_vmsubaq; return i;
                                case 0x00000026: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBAI; i.func = ee_i_vsubai; return i;
                                case 0x00000027: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBAI; i.func = ee_i_vmsubai; return i;
                                case 0x00000028: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VADDA; i.func = ee_i_vadda; return i;
                                case 0x00000029: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMADDA; i.func = ee_i_vmadda; return i;
                                case 0x0000002A: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMULA; i.func = ee_i_vmula; return i;
                                case 0x0000002C: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSUBA; i.func = ee_i_vsuba; return i;
                                case 0x0000002D: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMSUBA; i.func = ee_i_vmsuba; return i;
                                case 0x0000002E: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VOPMULA; i.func = ee_i_vopmula; return i;
                                case 0x0000002F: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VNOP; i.func = ee_i_vnop; return i;
                                case 0x00000030: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMOVE; i.func = ee_i_vmove; return i;
                                case 0x00000031: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMR32; i.func = ee_i_vmr32; return i;
                                case 0x00000034: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VLQI; i.func = ee_i_vlqi; return i;
                                case 0x00000035: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSQI; i.func = ee_i_vsqi; return i;
                                case 0x00000036: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VLQD; i.func = ee_i_vlqd; return i;
                                case 0x00000037: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSQD; i.func = ee_i_vsqd; return i;
                                case 0x00000038: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VDIV; i.func = ee_i_vdiv; return i;
                                case 0x00000039: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VSQRT; i.func = ee_i_vsqrt; return i;
                                case 0x0000003A: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VRSQRT; i.func = ee_i_vrsqrt; return i;
                                case 0x0000003B: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VWAITQ; i.func = ee_i_vwaitq; return i;
                                case 0x0000003C: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMTIR; i.func = ee_i_vmtir; return i;
                                case 0x0000003D: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VMFIR; i.func = ee_i_vmfir; return i;
                                case 0x0000003E: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VILWR; i.func = ee_i_vilwr; return i;
                                case 0x0000003F: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VISWR; i.func = ee_i_viswr; return i;
                                case 0x00000040: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VRNEXT; i.func = ee_i_vrnext; return i;
                                case 0x00000041: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VRGET; i.func = ee_i_vrget; return i;
                                case 0x00000042: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VRINIT; i.func = ee_i_vrinit; return i;
                                case 0x00000043: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_VRXOR; i.func = ee_i_vrxor; return i;
                            }
                        } break;
                    }
                } break;
            }
        } break;
        case 0x50000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BEQL; i.func = ee_i_beql; return i;
        case 0x54000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BNEL; i.func = ee_i_bnel; return i;
        case 0x58000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BLEZL; i.func = ee_i_blezl; return i;
        case 0x5C000000 >> 26: i.cycles = EE_CYC_BRANCH; i.branch = 3; i.id = EE_I_BGTZL; i.func = ee_i_bgtzl; return i;
        case 0x60000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.branch = 4; i.id = EE_I_DADDI; i.func = ee_i_daddi; return i;
        case 0x64000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_DADDIU; i.func = ee_i_daddiu; return i;
        case 0x68000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LDL; i.func = ee_i_ldl; return i;
        case 0x6C000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LDR; i.func = ee_i_ldr; return i;
        case 0x70000000 >> 26: { // mmi
            switch (opcode & 0x0000003F) {
                case 0x00000000: i.cycles = EE_CYC_MULT; i.id = EE_I_MADD; i.func = ee_i_madd; return i;
                case 0x00000001: i.cycles = EE_CYC_MULT; i.id = EE_I_MADDU; i.func = ee_i_maddu; return i;
                case 0x00000004: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PLZCW; i.func = ee_i_plzcw; return i;
                case 0x00000008: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDW; i.func = ee_i_paddw; return i;
                        case 0x00000040 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBW; i.func = ee_i_psubw; return i;
                        case 0x00000080 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCGTW; i.func = ee_i_pcgtw; return i;
                        case 0x000000C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMAXW; i.func = ee_i_pmaxw; return i;
                        case 0x00000100 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDH; i.func = ee_i_paddh; return i;
                        case 0x00000140 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBH; i.func = ee_i_psubh; return i;
                        case 0x00000180 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCGTH; i.func = ee_i_pcgth; return i;
                        case 0x000001C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMAXH; i.func = ee_i_pmaxh; return i;
                        case 0x00000200 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDB; i.func = ee_i_paddb; return i;
                        case 0x00000240 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBB; i.func = ee_i_psubb; return i;
                        case 0x00000280 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCGTB; i.func = ee_i_pcgtb; return i;
                        case 0x00000400 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDSW; i.func = ee_i_paddsw; return i;
                        case 0x00000440 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBSW; i.func = ee_i_psubsw; return i;
                        case 0x00000480 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXTLW; i.func = ee_i_pextlw; return i;
                        case 0x000004C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PPACW; i.func = ee_i_ppacw; return i;
                        case 0x00000500 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDSH; i.func = ee_i_paddsh; return i;
                        case 0x00000540 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBSH; i.func = ee_i_psubsh; return i;
                        case 0x00000580 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXTLH; i.func = ee_i_pextlh; return i;
                        case 0x000005C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PPACH; i.func = ee_i_ppach; return i;
                        case 0x00000600 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDSB; i.func = ee_i_paddsb; return i;
                        case 0x00000640 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBSB; i.func = ee_i_psubsb; return i;
                        case 0x00000680 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXTLB; i.func = ee_i_pextlb; return i;
                        case 0x000006C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PPACB; i.func = ee_i_ppacb; return i;
                        case 0x00000780 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXT5; i.func = ee_i_pext5; return i;
                        case 0x000007C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PPAC5; i.func = ee_i_ppac5; return i;
                    }
                } break;
                case 0x00000009: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PMADDW; i.func = ee_i_pmaddw; return i;
                        case 0x00000080 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSLLVW; i.func = ee_i_psllvw; return i;
                        case 0x000000C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSRLVW; i.func = ee_i_psrlvw; return i;
                        case 0x00000100 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PMSUBW; i.func = ee_i_pmsubw; return i;
                        case 0x00000200 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMFHI; i.func = ee_i_pmfhi; return i;
                        case 0x00000240 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMFLO; i.func = ee_i_pmflo; return i;
                        case 0x00000280 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PINTH; i.func = ee_i_pinth; return i;
                        case 0x00000300 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PMULTW; i.func = ee_i_pmultw; return i;
                        case 0x00000340 >> 6: i.cycles = EE_CYC_MMI_DIV; i.id = EE_I_PDIVW; i.func = ee_i_pdivw; return i;
                        case 0x00000380 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCPYLD; i.func = ee_i_pcpyld; return i;
                        case 0x00000400 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PMADDH; i.func = ee_i_pmaddh; return i;
                        case 0x00000440 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PHMADH; i.func = ee_i_phmadh; return i;
                        case 0x00000480 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PAND; i.func = ee_i_pand; return i;
                        case 0x000004C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PXOR; i.func = ee_i_pxor; return i;
                        case 0x00000500 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PMSUBH; i.func = ee_i_pmsubh; return i;
                        case 0x00000540 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PHMSBH; i.func = ee_i_phmsbh; return i;
                        case 0x00000680 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXEH; i.func = ee_i_pexeh; return i;
                        case 0x000006C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PREVH; i.func = ee_i_prevh; return i;
                        case 0x00000700 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PMULTH; i.func = ee_i_pmulth; return i;
                        case 0x00000740 >> 6: i.cycles = EE_CYC_MMI_DIV; i.id = EE_I_PDIVBW; i.func = ee_i_pdivbw; return i;
                        case 0x00000780 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXEW; i.func = ee_i_pexew; return i;
                        case 0x000007C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PROT3W; i.func = ee_i_prot3w; return i;
                    }
                } break;
                case 0x00000010: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MFHI1; i.func = ee_i_mfhi1; return i;
                case 0x00000011: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MTHI1; i.func = ee_i_mthi1; return i;
                case 0x00000012: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MFLO1; i.func = ee_i_mflo1; return i;
                case 0x00000013: i.cycles = EE_CYC_COP_DEFAULT; i.id = EE_I_MTLO1; i.func = ee_i_mtlo1; return i;
                case 0x00000018: i.cycles = EE_CYC_MULT; i.id = EE_I_MULT1; i.func = ee_i_mult1; return i;
                case 0x00000019: i.cycles = EE_CYC_MULT; i.id = EE_I_MULTU1; i.func = ee_i_multu1; return i;
                case 0x0000001A: i.cycles = EE_CYC_DIV; i.id = EE_I_DIV1; i.func = ee_i_div1; return i;
                case 0x0000001B: i.cycles = EE_CYC_DIV; i.id = EE_I_DIVU1; i.func = ee_i_divu1; return i;
                case 0x00000020: i.cycles = EE_CYC_MULT; i.id = EE_I_MADD1; i.func = ee_i_madd1; return i;
                case 0x00000021: i.cycles = EE_CYC_MULT; i.id = EE_I_MADDU1; i.func = ee_i_maddu1; return i;
                case 0x00000028: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000040 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PABSW; i.func = ee_i_pabsw; return i;
                        case 0x00000080 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCEQW; i.func = ee_i_pceqw; return i;
                        case 0x000000C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMINW; i.func = ee_i_pminw; return i;
                        case 0x00000100 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADSBH; i.func = ee_i_padsbh; return i;
                        case 0x00000140 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PABSH; i.func = ee_i_pabsh; return i;
                        case 0x00000180 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCEQH; i.func = ee_i_pceqh; return i;
                        case 0x000001C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMINH; i.func = ee_i_pminh; return i;
                        case 0x00000280 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCEQB; i.func = ee_i_pceqb; return i;
                        case 0x00000400 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDUW; i.func = ee_i_padduw; return i;
                        case 0x00000440 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBUW; i.func = ee_i_psubuw; return i;
                        case 0x00000480 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXTUW; i.func = ee_i_pextuw; return i;
                        case 0x00000500 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDUH; i.func = ee_i_padduh; return i;
                        case 0x00000540 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBUH; i.func = ee_i_psubuh; return i;
                        case 0x00000580 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXTUH; i.func = ee_i_pextuh; return i;
                        case 0x00000600 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PADDUB; i.func = ee_i_paddub; return i;
                        case 0x00000640 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSUBUB; i.func = ee_i_psubub; return i;
                        case 0x00000680 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXTUB; i.func = ee_i_pextub; return i;
                        case 0x000006C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_QFSRV; i.func = ee_i_qfsrv; return i;
                    }
                } break;
                case 0x00000029: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PMADDUW; i.func = ee_i_pmadduw; return i;
                        case 0x000000C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSRAVW; i.func = ee_i_psravw; return i;
                        case 0x00000200 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMTHI; i.func = ee_i_pmthi; return i;
                        case 0x00000240 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMTLO; i.func = ee_i_pmtlo; return i;
                        case 0x00000280 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PINTEH; i.func = ee_i_pinteh; return i;
                        case 0x00000300 >> 6: i.cycles = EE_CYC_MMI_MULT; i.id = EE_I_PMULTUW; i.func = ee_i_pmultuw; return i;
                        case 0x00000340 >> 6: i.cycles = EE_CYC_MMI_DIV; i.id = EE_I_PDIVUW; i.func = ee_i_pdivuw; return i;
                        case 0x00000380 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCPYUD; i.func = ee_i_pcpyud; return i;
                        case 0x00000480 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_POR; i.func = ee_i_por; return i;
                        case 0x000004C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PNOR; i.func = ee_i_pnor; return i;
                        case 0x00000680 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXCH; i.func = ee_i_pexch; return i;
                        case 0x000006C0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PCPYH; i.func = ee_i_pcpyh; return i;
                        case 0x00000780 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PEXCW; i.func = ee_i_pexcw; return i;
                    }
                } break;
                case 0x00000030: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMFHLLW; i.func = ee_i_pmfhllw; return i;
                        case 0x00000040 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMFHLUW; i.func = ee_i_pmfhluw; return i;
                        case 0x00000080 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMFHLSLW; i.func = ee_i_pmfhlslw; return i;
                        case 0x000000c0 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMFHLLH; i.func = ee_i_pmfhllh; return i;
                        case 0x00000100 >> 6: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMFHLSH; i.func = ee_i_pmfhlsh; return i;
                    }
                } break;
                case 0x00000031: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PMTHL; i.func = ee_i_pmthl; return i;
                case 0x00000034: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSLLH; i.func = ee_i_psllh; return i;
                case 0x00000036: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSRLH; i.func = ee_i_psrlh; return i;
                case 0x00000037: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSRAH; i.func = ee_i_psrah; return i;
                case 0x0000003C: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSLLW; i.func = ee_i_psllw; return i;
                case 0x0000003E: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSRLW; i.func = ee_i_psrlw; return i;
                case 0x0000003F: i.cycles = EE_CYC_MMI_DEFAULT; i.id = EE_I_PSRAW; i.func = ee_i_psraw; return i;
            }
        } break;
        case 0x78000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LQ; i.func = ee_i_lq; return i;
        case 0x7C000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_SQ; i.func = ee_i_sq; return i;
        case 0x80000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LB; i.func = ee_i_lb; return i;
        case 0x84000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LH; i.func = ee_i_lh; return i;
        case 0x88000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LWL; i.func = ee_i_lwl; return i;
        case 0x8C000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LW; i.func = ee_i_lw; return i;
        case 0x90000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LBU; i.func = ee_i_lbu; return i;
        case 0x94000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LHU; i.func = ee_i_lhu; return i;
        case 0x98000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LWR; i.func = ee_i_lwr; return i;
        case 0x9C000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LWU; i.func = ee_i_lwu; return i;
        case 0xA0000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SB; i.func = ee_i_sb; return i;
        case 0xA4000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SH; i.func = ee_i_sh; return i;
        case 0xA8000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SWL; i.func = ee_i_swl; return i;
        case 0xAC000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SW; i.func = ee_i_sw; return i;
        case 0xB0000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SDL; i.func = ee_i_sdl; return i;
        case 0xB4000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SDR; i.func = ee_i_sdr; return i;
        case 0xB8000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SWR; i.func = ee_i_swr; return i;
        case 0xBC000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.branch = 2; i.id = EE_I_CACHE; i.func = ee_i_cache; return i;
        case 0xC4000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LWC1; i.func = ee_i_lwc1; return i;
        case 0xCC000000 >> 26: i.cycles = EE_CYC_DEFAULT; i.id = EE_I_PREF; i.func = ee_i_pref; return i;
        case 0xD8000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LQC2; i.func = ee_i_lqc2; return i;
        case 0xDC000000 >> 26: i.cycles = EE_CYC_LOAD; i.id = EE_I_LD; i.func = ee_i_ld; return i;
        case 0xE4000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SWC1; i.func = ee_i_swc1; return i;
        case 0xF8000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SQC2; i.func = ee_i_sqc2; return i;
        case 0xFC000000 >> 26: i.cycles = EE_CYC_STORE; i.id = EE_I_SD; i.func = ee_i_sd; return i;
    }

    i.id = EE_I_INVALID; i.func = ee_i_invalid;

    return i;
}

static inline ee_sub_block ee_decode_sub_block(struct ee_state* ee, uint32_t pc, int max_cycles, std::vector<ee_instruction>& out) {
    ee_sub_block sb;

    sb.start_pc = pc;
    sb.end_pc = pc;
    sb.cycles = 0;
    sb.first = (uint32_t)out.size();
    sb.count = 0;
    sb.term = EE_TERM_FALLTHROUGH;
    sb.branch_idx = -1;
    sb.succ_pc[0] = sb.succ_pc[1] = 0;
    sb.has_succ[0] = sb.has_succ[1] = false;
    sb.succ[0] = sb.succ[1] = -1;
    sb.back_edge_target = false;

    ee_instruction i;

    bool delay_slot = false;

    while (max_cycles) {
        ee->opcode = bus_read32(ee, sb.end_pc);

        if (ee->opcode != 0) {
            i = ee_decode(ee->opcode);

            out.push_back(i);

            sb.count++;
        } else {
            i.branch = 0;
        }

        if (i.branch == 1 && delay_slot) {
            fprintf(stderr, "ee: Branch in delay slot at PC=%08x (Unhandled edge case)\n", sb.end_pc);

            exit(1);
        }

        sb.cycles++; // += i.cycles;

        if (i.branch == 1 || i.branch == 3) {
            delay_slot = true;

            sb.term = i.branch == 1 ? EE_TERM_BRANCH : EE_TERM_LIKELY;
            sb.branch_idx = (int32_t)sb.count - 1;

            max_cycles = 2;
        } else if (i.branch != 0) {
            sb.term = EE_TERM_EXCEPT;
            sb.branch_idx = (int32_t)sb.count - 1;

            max_cycles = 1;
        }

        // Arbitrarily big number for MMI instructions, perf benefits from
        // long MMI sequences, keeping guest SIMD regs in host SIMD regs
        // longer is good
        // if (i.cycles == EE_CYC_MMI_DEFAULT && !delay_slot) {
        //     max_cycles = 16;
        // }

        max_cycles--;

        sb.end_pc += 4;
    }

    return sb;
}

static inline bool ee_branch_is_pcrel(int id) {
    switch (id) {
        case EE_I_BEQ:  case EE_I_BNE:  case EE_I_BEQL: case EE_I_BNEL:
        case EE_I_BLTZ: case EE_I_BGEZ: case EE_I_BLEZ: case EE_I_BGTZ:
        case EE_I_BLTZL: case EE_I_BGEZL: case EE_I_BLEZL: case EE_I_BGTZL:
        case EE_I_BLTZAL: case EE_I_BGEZAL: case EE_I_BLTZALL: case EE_I_BGEZALL:
        case EE_I_BC0F: case EE_I_BC0T: case EE_I_BC0FL: case EE_I_BC0TL:
        case EE_I_BC1F: case EE_I_BC1T: case EE_I_BC1FL: case EE_I_BC1TL:
            return true;
    }

    return false;
}

static inline void ee_successors(const struct ee_block& block, ee_sub_block& sb) {
    if (sb.term == EE_TERM_FALLTHROUGH) {
        sb.succ_pc[0] = sb.end_pc;
        sb.has_succ[0] = true;

        return;
    }

    if (sb.term != EE_TERM_BRANCH && sb.term != EE_TERM_LIKELY) return;
    if (sb.branch_idx < 0) return;

    const ee_instruction& br = block.instructions[sb.first + sb.branch_idx];

    if (!ee_branch_is_pcrel(br.id)) return;

    int32_t off = (int32_t)(br.i16 << 16) >> 14;

    sb.succ_pc[0] = sb.end_pc;
    sb.succ_pc[1] = (sb.end_pc - 4) + (uint32_t)off;
    sb.has_succ[0] = true;
    sb.has_succ[1] = true;

    bool is_rr = br.id == EE_I_BEQ || br.id == EE_I_BNE ||
                 br.id == EE_I_BEQL || br.id == EE_I_BNEL;

    if (is_rr && br.rs.r == br.rt.r) {
        bool eq = br.id == EE_I_BEQ || br.id == EE_I_BEQL;

        sb.has_succ[0] = !eq;
        sb.has_succ[1] = eq;
    }
}

static inline struct ee_block* ee_cache_block(struct ee_state* ee, int max_cycles) {
    uint32_t phys;

    ee_translate_virt(ee, ee->pc, &phys);

    uint32_t page = phys / EE_MIN_PAGESIZE;
    uint32_t offset = (phys & (EE_MIN_PAGESIZE - 1)) >> 2;

    if (!ee->block_cache[page].valid) {
        ee->block_cache[page].blocks = new struct ee_block[EE_MIN_PAGESIZE >> 2];
        ee->block_cache[page].dirty = false;
        ee->block_cache[page].valid = true;
        ee->block_cache[page].min_code_addr = ee->pc;
        ee->block_cache[page].max_code_addr = ee->pc;
    }

    struct ee_block& block = ee->block_cache[page].blocks[offset];

#ifdef _EE_DISABLE_CACHE
    block.instructions.clear();
    block.func = nullptr;
#endif

    if (ee->pc < ee->block_cache[page].min_code_addr) {
        ee->block_cache[page].min_code_addr = ee->pc;
    }

    block.instructions.clear();
    block.instructions.reserve(max_cycles);

    ee->sub_blocks.clear();

    bool grow = max_cycles >= EE_BLOCK_MAX_INSTRS;

    uint32_t page_base = ee->pc & ~(uint32_t)(EE_MIN_PAGESIZE - 1);

    std::vector <uint32_t> pending;

    pending.push_back(ee->pc);

    while (!pending.empty()) {
        uint32_t pc = pending.back();

        pending.pop_back();

        bool seen = false;

        for (const ee_sub_block& s : ee->sub_blocks) {
            if (s.start_pc == pc) {
                seen = true;
                break;
            }
        }

        if (seen) continue;

        if (!ee->sub_blocks.empty()) {
            if (!grow)
                break;

            if (ee->sub_blocks.size() >= (size_t)EE_REGION_MAX_BLOCKS)
                break;

            if (block.instructions.size() >= EE_REGION_MAX_INSTRS)
                break;
        }

        ee->sub_blocks.push_back(ee_decode_sub_block(ee, pc, max_cycles, block.instructions));

        if (!grow) break;

        ee_successors(block, ee->sub_blocks.back());

        const ee_sub_block& sb = ee->sub_blocks.back();

        for (int e = 1; e >= 0; e--) {
            if (!sb.has_succ[e])
                continue;

            if ((sb.succ_pc[e] & ~(uint32_t)(EE_MIN_PAGESIZE - 1)) != page_base)
                continue;

            pending.push_back(sb.succ_pc[e]);
        }
    }

    for (size_t k = 0; k < ee->sub_blocks.size(); k++) {
        ee_sub_block& s = ee->sub_blocks[k];

        for (int e = 0; e < 2; e++) {
            if (!s.has_succ[e]) continue;

            for (size_t j = 0; j < ee->sub_blocks.size(); j++) {
                if (ee->sub_blocks[j].start_pc != s.succ_pc[e]) continue;

                if (j <= k) {
                    ee->sub_blocks[j].back_edge_target = true;
                }

                s.succ[e] = (int32_t)j;

                break;
            }
        }
    }

    block.start_pc = ee->pc;
    block.end_pc = ee->sub_blocks.front().end_pc;
    block.cycles = ee->sub_blocks.front().cycles;

    for (const ee_sub_block& s : ee->sub_blocks) {
        if (ee->block_cache[page].max_code_addr < s.end_pc) {
            ee->block_cache[page].max_code_addr = s.end_pc;
        }

        if (ee->block_cache[page].min_code_addr > s.start_pc) {
            ee->block_cache[page].min_code_addr = s.start_pc;
        }
    }

    return &block;
}

void ee_compile_block(struct ee_state* ee, struct ee_block* block);

static inline struct ee_block* ee_find_block(struct ee_state* ee, uint32_t pc) {
#ifdef _EE_DISABLE_CACHE
    return nullptr;
#endif

    ee_block_lut_entry& lut = ee->block_lut[(pc >> 2) & EE_BLOCK_LUT_MASK];

    if (lut.pc == pc && lut.gen == ee->block_lut_gen) {
        return lut.block;
    }

    uint32_t phys;

    ee_translate_virt(ee, ee->pc, &phys);

    uint32_t page = phys / EE_MIN_PAGESIZE;

    if (!ee->block_cache[page].valid) {
        return nullptr;
    }

    if (ee->block_cache[page].dirty) {
        // Invalidate entire page if it's dirty (code was modified)
        delete[] ee->block_cache[page].blocks;

        ee->block_cache[page].blocks = nullptr;
        ee->block_cache[page].dirty = false;
        ee->block_cache[page].valid = false;
        ee->block_cache[page].min_code_addr = 0;
        ee->block_cache[page].max_code_addr = 0;

        ee->block_lut_gen++;

        return nullptr;
    }

    uint32_t offset = (phys & (EE_MIN_PAGESIZE - 1)) >> 2;

    struct ee_block& block = ee->block_cache[page].blocks[offset];

    if (!block.cycles) {
        return nullptr;
    }

    lut.pc = pc;
    lut.gen = ee->block_lut_gen;
    lut.block = &block;

    return &block;
}

#define EE(m) ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, m))

static inline void ee_flush_vec(struct ee_state* ee, asmjit::ujit::UniCompiler* uc, int i) {
    using namespace asmjit;

    if (!ee->vec_cache[i].valid) return;

    if (ee->vec_cache[i].dirty) {
        uc->v_storeu128(ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + i * sizeof(uint128_t)), ee->vec_cache[i].vec);
    }

    ee->vec_cache[i].valid = false;
    ee->vec_cache[i].dirty = false;
}

static inline void ee_materialize_const(struct ee_state* ee, asmjit::ujit::UniCompiler* uc, int i) {
    ee->reg_cache[i].reg = uc->new_gp64();
    uc->mov(ee->reg_cache[i].reg, asmjit::Imm((int64_t)ee->reg_cache[i].value));
    ee->reg_cache[i].valid = true;
}

static inline void ee_set_const(struct ee_state* ee, asmjit::ujit::UniCompiler* uc, int i, uint64_t value) {
    if (!i) return;

    ee_flush_vec(ee, uc, i);

    ee->reg_cache[i].constant = true;
    ee->reg_cache[i].valid = false;
    ee->reg_cache[i].value = value;
}

static inline ee_cached_reg& ee_get_reg(struct ee_state* ee, asmjit::ujit::UniCompiler* uc, int i, bool sync = true, bool b64 = true) {
    using namespace asmjit;

    ee_flush_vec(ee, uc, i);

    if (!ee->reg_cache[i].valid) {
        ee->reg_cache[i].reg = b64 ? uc->new_gp64() : uc->new_gp32();

        if (sync) {
            if (ee->reg_cache[i].constant) {
                uc->mov(ee->reg_cache[i].reg, Imm((int64_t)ee->reg_cache[i].value));
            } else if (b64) {
                uc->load_u64(ee->reg_cache[i].reg, ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + i * sizeof(uint128_t)));
            } else {
                uc->load_u32(ee->reg_cache[i].reg, ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + i * sizeof(uint128_t)));
            }
        }

        ee->reg_cache[i].valid = true;
    }

    if (!sync) ee->reg_cache[i].constant = false;

    return ee->reg_cache[i];
}

static inline void ee_flush_reg_cache(struct ee_state* ee, asmjit::ujit::UniCompiler* uc) {
    using namespace asmjit;

    for (int i = 0; i < 32; i++) {
        if (ee->vec_cache[i].valid) {
            if (ee->vec_cache[i].dirty) {
                uc->v_storeu128(ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + i * sizeof(uint128_t)), ee->vec_cache[i].vec);
            }

            ee->vec_cache[i].valid = false;
            ee->vec_cache[i].dirty = false;
        }

        if (!ee->reg_cache[i].valid && ee->reg_cache[i].constant) {
            ee_materialize_const(ee, uc, i);
        }

        if (ee->reg_cache[i].valid) {
            uc->store_u64(ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + i * sizeof(uint128_t)), ee->reg_cache[i].reg);
        }

        ee->reg_cache[i].valid = false;
        ee->reg_cache[i].constant = false;
    }
}

static inline void ee_sext32(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Gp& dst, const asmjit::ujit::Gp& src) {
#if defined(ASMJIT_UJIT_AARCH64)
    uc.cc->sxtw(dst.r64(), src.r32());
#else
    uc.cc->movsxd(dst.r64(), src.r32());
#endif
}

static inline void ee_sext32(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Gp& reg) {
    ee_sext32(uc, reg, reg);
}

static inline void ee_store_imm32(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Mem& dst, uint32_t value) {
#if defined(ASMJIT_UJIT_AARCH64)
    asmjit::ujit::Gp tmp = uc.new_gp32();

    uc.mov(tmp, asmjit::Imm(value));
    uc.store_u32(dst, tmp);
#else
    asmjit::ujit::Mem m = dst;

    m.set_size(4);

    uc.cc->mov(m, asmjit::Imm(value));
#endif
}

static inline void ee_sub_imm32(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Mem& dst, uint32_t value) {
#if defined(ASMJIT_UJIT_AARCH64)
    asmjit::ujit::Gp tmp = uc.new_gp32();

    uc.load_u32(tmp, dst);
    uc.sub(tmp, tmp, asmjit::Imm(value));
    uc.store_u32(dst, tmp);
#else
    asmjit::ujit::Mem m = dst;

    m.set_size(4);

    uc.cc->sub(m, asmjit::Imm(value));
#endif
}

static inline void ee_sextn(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Gp& reg, uint32_t bits) {
    uint32_t shift = 64u - bits;

    uc.shl(reg.r64(), reg.r64(), asmjit::Imm(shift));
    uc.sar(reg.r64(), reg.r64(), asmjit::Imm(shift));
}

static inline void ee_load_reg_vec128(struct ee_state* ee, asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Vec& dst, int idx) {
    using namespace asmjit;

    uc.v_loadu128(dst, EE(r[idx]));

    if (ee->reg_cache[idx].valid) {
        uc.s_insert_u64(dst, ee->reg_cache[idx].reg, 0);
    }
}

static inline asmjit::ujit::Vec ee_get_vec(struct ee_state* ee, asmjit::ujit::UniCompiler& uc, int idx) {
    using namespace asmjit;

    if (ee->vec_cache[idx].valid) {
        return ee->vec_cache[idx].vec;
    }

    if (ee->reg_cache[idx].constant && !ee->reg_cache[idx].valid) {
        ee_materialize_const(ee, &uc, idx);
    }

    ujit::Vec v = uc.new_vec128();

    uc.v_loadu128(v, EE(r[idx]));

    bool folded = ee->reg_cache[idx].valid;

    if (folded) {
        uc.s_insert_u64(v, ee->reg_cache[idx].reg, 0);

        ee->reg_cache[idx].valid = false;
        ee->reg_cache[idx].constant = false;
    }

    ee->vec_cache[idx].vec = v;
    ee->vec_cache[idx].valid = true;
    ee->vec_cache[idx].dirty = folded;

    return v;
}

static inline void ee_set_vec(struct ee_state* ee, asmjit::ujit::UniCompiler& uc, int idx, const asmjit::ujit::Vec& v) {
    ee->vec_cache[idx].vec = v;
    ee->vec_cache[idx].valid = true;
    ee->vec_cache[idx].dirty = true;
    ee->reg_cache[idx].valid = false;
    ee->reg_cache[idx].constant = false;
}

static inline void ee_sync_reg_to_mem(struct ee_state* ee, asmjit::ujit::UniCompiler& uc, int idx) {
    using namespace asmjit;

    if (ee->vec_cache[idx].valid) {
        if (ee->vec_cache[idx].dirty) {
            uc.v_storeu128(ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + idx * sizeof(uint128_t)), ee->vec_cache[idx].vec);
            ee->vec_cache[idx].dirty = false;
        }
    } else if (ee->reg_cache[idx].valid) {
        uc.store_u64(ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + idx * sizeof(uint128_t)), ee->reg_cache[idx].reg);
    } else if (ee->reg_cache[idx].constant) {
        ee_materialize_const(ee, &uc, idx);
        uc.store_u64(ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + idx * sizeof(uint128_t)), ee->reg_cache[idx].reg);
    }
}

enum { EE_MMI_WR_RD = 1, EE_MMI_WR_HI = 2, EE_MMI_WR_LO = 4 };

typedef void (*ee_wide_emit_fn)(asmjit::ujit::UniCompiler&, const asmjit::ujit::Vec&, const asmjit::ujit::Vec&, const asmjit::ujit::Vec&, const asmjit::ujit::Vec&, const asmjit::ujit::Vec&);

static inline void ee_emit_mmi_wide(struct ee_state* ee, asmjit::ujit::UniCompiler& uc, const ee_instruction& i, ee_wide_emit_fn emit, unsigned wmask, bool reads_st) {
    using namespace asmjit;

    ujit::Vec vhi = uc.new_vec128();
    ujit::Vec vlo = uc.new_vec128();

    uc.v_loadu128(vhi, EE(hi));
    uc.v_loadu128(vlo, EE(lo));

    ujit::Vec vrs = vhi, vrt = vhi;

    if (reads_st) {
        vrs = ee_get_vec(ee, uc, i.rs.r);
        vrt = ee_get_vec(ee, uc, i.rt.r);
    }

    ujit::Vec vrd = (wmask & EE_MMI_WR_RD) ? uc.new_vec128() : vhi;

    emit(uc, vrd, vhi, vlo, vrs, vrt);

    if ((wmask & EE_MMI_WR_RD) && i.rd.r) {
        ee_set_vec(ee, uc, i.rd.r, vrd);
    }
    
    if (wmask & EE_MMI_WR_HI) {
        uc.v_storeu128(EE(hi), vhi);
    }

    if (wmask & EE_MMI_WR_LO) {
       uc.v_storeu128(EE(lo), vlo);
    }
}

static inline bool ee_reg_is_const(struct ee_state* ee, int r, uint64_t* v) {
    if (r == 0) {
        *v = 0;
        
        return true;
    }

    if (ee->reg_cache[r].constant) {
        *v = ee->reg_cache[r].value;
        
        return true;
    }

    return false;
}

static inline bool ee_one_const(struct ee_state* ee, int ra, int rb, uint64_t* c, int* other) {
    uint64_t va, vb;

    bool ca = ee_reg_is_const(ee, ra, &va);
    bool cb = ee_reg_is_const(ee, rb, &vb);

    if (ca == cb)
        return false;

    *c = ca ? va : vb;
    *other = ca ? rb : ra;

    return true;
}

static inline bool ee_fits_imm32(uint64_t c) {
    return c == (uint64_t)(int64_t)(int32_t)c;
}

static int n = 0;

void ee_compile_block(struct ee_state* ee, struct ee_block* block) {
    using namespace asmjit;

    CodeHolder code;

    code.init(ee->rt.environment(), ee->rt.cpu_features());
    // code.set_logger(ee->logger);

    // if (code.logger()) {
    //     printf("---------------------------------- Block at PC=%08x, %zu sub-blocks, %zu instructions\n", block->start_pc, ee->sub_blocks.size(), block->instructions.size());
    // }

    ujit::BackendCompiler bc(&code);
    ujit::UniCompiler uc(&bc, ee->rt.cpu_features(), ee->rt.cpu_hints());

    FuncNode* func = uc.add_func(FuncSignature::build<void, ee_state*>());

    ee->ee_ptr = uc.new_gp_ptr();

    func->set_arg(0, ee->ee_ptr);

    asmjit::Label block_exit = uc.new_label();

    uint32_t sb_end_pc = block->end_pc;

    std::vector <Label> sb_label(ee->sub_blocks.size());

    for (Label& l : sb_label) l = uc.new_label();

    uint32_t region_phys;

    ee_translate_virt(ee, block->start_pc, &region_phys);

    const bool* region_dirty = &ee->block_cache[region_phys / EE_MIN_PAGESIZE].dirty;

    const ee_sub_block* cur_sb = &ee->sub_blocks[0];

    size_t cur_sb_i = 0;

    enum PendKind { PEND_NONE, PEND_COND, PEND_TAKEN, PEND_DONE };

    PendKind pending = PEND_NONE;
    int32_t pending_off = 0;

    auto sb_links = [&]() { return cur_sb->succ[0] >= 0 || cur_sb->succ[1] >= 0; };

    auto emit_branch_target = [&](int32_t off) {
        ee_store_imm32(uc, EE(next_pc), (sb_end_pc - 4) + off);
    };

    auto emit_edge = [&](int side, int32_t off) {
        int32_t s = cur_sb->succ[side];

        if (side == 1) emit_branch_target(off);

        ee_flush_reg_cache(ee, &uc);

        if (s < 0) {
            uc.j(block_exit);

            return;
        }

        ujit::Gp t = uc.new_gp32();

        uc.load_u32(t, EE(cycles_left));
        uc.j(block_exit, ujit::scmp_le(t, Imm(0)));

        uc.j(sb_label[s]);
    };

    auto emit_branch = [&](const ujit::UniCondition& not_taken, int32_t off) {
        Label skip = uc.new_label();

        uc.j(skip, not_taken);
        emit_branch_target(off);
        uc.bind(skip);

        if (sb_links()) {
            pending = PEND_COND;
            pending_off = off;
        }
    };

    auto emit_branch_likely = [&](const ujit::UniCondition& taken, int32_t off) {
        ee_flush_reg_cache(ee, &uc);

        Label l_taken = uc.new_label();

        uc.j(l_taken, taken);

        if (sb_links()) emit_edge(0, off);
        else uc.ret();

        uc.bind(l_taken);

        if (!sb_links()) emit_branch_target(off);
        else { pending = PEND_TAKEN; pending_off = off; }
    };

    auto emit_link = [&]() {
        ee_cached_reg& ra = ee_get_reg(ee, &uc, 31, false);

        uc.mov(ra.reg, Imm((uint64_t)sb_end_pc));
    };

    auto emit_branch_folded = [&](bool taken, bool likely, int32_t off) {
        if (taken) {
            if (sb_links()) { pending = PEND_TAKEN; pending_off = off; }
            else emit_branch_target(off);
        } else if (likely) {
            if (sb_links()) { emit_edge(0, off); pending = PEND_DONE; }
            else { ee_flush_reg_cache(ee, &uc); uc.ret(); }
        }
    };

    enum { EE_ZC_LTZ, EE_ZC_GEZ, EE_ZC_LEZ, EE_ZC_GTZ };

    auto emit_branch_z = [&](const ee_instruction& i, int op, bool likely, bool link) {
        if (link) emit_link();

        uint64_t cs;

        if (!sb_links() && ee_reg_is_const(ee, i.rs.r, &cs)) {
            int64_t v = (int64_t)cs;

            bool taken = op == EE_ZC_LTZ ? v <  0 :
                         op == EE_ZC_GEZ ? v >= 0 :
                         op == EE_ZC_LEZ ? v <= 0 :
                                           v >  0;

            emit_branch_folded(taken, likely, EE_D_SI16);

            return;
        }

        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

        if (likely) {
            emit_branch_likely(op == EE_ZC_LTZ ? ujit::scmp_lt(rs.reg, Imm(0)) :
                               op == EE_ZC_GEZ ? ujit::scmp_ge(rs.reg, Imm(0)) :
                               op == EE_ZC_LEZ ? ujit::scmp_le(rs.reg, Imm(0)) :
                                                 ujit::scmp_gt(rs.reg, Imm(0)), EE_D_SI16);
        } else {
            emit_branch(op == EE_ZC_LTZ ? ujit::scmp_ge(rs.reg, Imm(0)) :
                        op == EE_ZC_GEZ ? ujit::scmp_lt(rs.reg, Imm(0)) :
                        op == EE_ZC_LEZ ? ujit::scmp_gt(rs.reg, Imm(0)) :
                                          ujit::scmp_le(rs.reg, Imm(0)), EE_D_SI16);
        }
    };

    auto emit_branch_eq = [&](const ee_instruction& i, bool eq, bool likely) {
        uint64_t cs, ct;

        if (!sb_links() && ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
            emit_branch_folded(eq ? cs == ct : cs != ct, likely, EE_D_SI16);

            return;
        }

        if (i.rs.r == i.rt.r) {
            emit_branch_folded(eq, likely, EE_D_SI16);

            return;
        }

        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
        ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

        if (likely) {
            emit_branch_likely(eq ? ujit::cmp_eq(rs.reg, rt.reg) : ujit::cmp_ne(rs.reg, rt.reg), EE_D_SI16);
        } else {
            emit_branch(eq ? ujit::cmp_ne(rs.reg, rt.reg) : ujit::cmp_eq(rs.reg, rt.reg), EE_D_SI16);
        }
    };

    for (size_t sb_i = 0; sb_i < ee->sub_blocks.size(); sb_i++) {
        const ee_sub_block& sb = ee->sub_blocks[sb_i];

        sb_end_pc = sb.end_pc;
        cur_sb = &sb;
        cur_sb_i = sb_i;
        pending = PEND_NONE;

        ee_flush_reg_cache(ee, &uc);

        uc.bind(sb_label[sb_i]);

        if (sb.back_edge_target) {
            ee_store_imm32(uc, EE(next_pc), sb.start_pc);

            ujit::Gp t = uc.new_gp32();

            uc.load_u32(t, EE(cycles_left));
            uc.j(block_exit, ujit::scmp_le(t, Imm(0)));

            ujit::Gp gp = uc.new_gp_ptr();
            ujit::Gp g = uc.new_gp32();

            uc.mov(gp, Imm((uintptr_t)region_dirty));
            uc.load_u8(g, ujit::mem_ptr(gp, 0));
            uc.j(block_exit, ujit::test_nz(g));

            ujit::Gp x = uc.new_gp32();

            uc.load_u32(x, EE(exit_req));
            uc.j(block_exit, ujit::test_nz(x));
        }

        ee_store_imm32(uc, EE(pc), sb.end_pc - 4);
        ee_store_imm32(uc, EE(next_pc), sb.end_pc);

        ee_sub_imm32(uc, EE(cycles_left), sb.cycles);

        for (uint32_t sb_n = 0; sb_n < sb.count; sb_n++) {
            const ee_instruction& i = block->instructions[sb.first + sb_n];

            switch (i.id) {
                case EE_I_ADDI:
                case EE_I_ADDIU: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (ee_reg_is_const(ee, i.rs.r, &cs)) {
                        ee_set_const(ee, &uc, i.rt.r, (int64_t)(int32_t)((uint32_t)cs + (int32_t)(int16_t)i.i16));
                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, sync);

                    if (!i.rs.r) {
                        uc.mov(rt.reg, Imm((int64_t)(int16_t)i.i16));
                    } else {
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                        uc.add(rt.reg, rs.reg, Imm((int32_t)(int16_t)i.i16));

                        ee_sext32(uc, rt.reg);
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_DADDI:
                case EE_I_DADDIU: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (ee_reg_is_const(ee, i.rs.r, &cs)) {
                        ee_set_const(ee, &uc, i.rt.r, cs + (int64_t)(int16_t)i.i16);
                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, sync);

                    if (!i.rs.r) {
                        uc.mov(rt.reg, Imm((int64_t)(int16_t)i.i16));
                    } else {
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                        uc.add(rt.reg, rs.reg, Imm((int32_t)(int16_t)i.i16));
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_MFC0: {
                    if (!i.rt.r) continue;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, false);

                    uc.load_u32(rt.reg, EE(cop0_r[i.rd.r]));

                    ee_sext32(uc, rt.reg);
                } break;

                case EE_I_MTC0: {
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.store_u32(EE(cop0_r[i.rd.r]), rt.reg);
                } break;

                case EE_I_SUB:
                case EE_I_SUBU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)cs - (uint32_t)ct));
                        continue;
                    }

                    uint64_t kt;

                    if (!ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &kt)) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, i.rs.r == i.rd.r);
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                        uc.add(rd.reg, rs.reg, Imm((int32_t)(uint32_t)(0 - kt)));

                        ee_sext32(uc, rd.reg);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.sub(rd.reg, rs.reg, rt.reg);

                    ee_sext32(uc, rd.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_ANDI:
                case EE_I_XORI: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (ee_reg_is_const(ee, i.rs.r, &cs)) {
                        uint64_t imm = (uint16_t)i.i16;
                        ee_set_const(ee, &uc, i.rt.r, i.id == EE_I_ANDI ? (cs & imm) : (cs ^ imm));

                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    switch (i.id) {
                        case EE_I_ANDI: uc.and_(rt.reg, rs.reg, Imm(i.i16)); break;
                        case EE_I_XORI: uc.xor_(rt.reg, rs.reg, Imm(i.i16)); break;
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_ORI: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (ee_reg_is_const(ee, i.rs.r, &cs)) {
                        ee_set_const(ee, &uc, i.rt.r, cs | (uint64_t)(uint16_t)i.i16);

                        continue;
                    }

                    bool sync = i.rs.r == i.rt.r;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, sync);

                    if (!i.rs.r) {
                        uc.mov(rt.reg, Imm(i.i16));
                    } else {
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                        uc.or_(rt.reg, rs.reg, Imm(i.i16));
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_AND:
                case EE_I_OR:
                case EE_I_XOR: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
                        uint64_t v = i.id == EE_I_AND ? (cs & ct) : i.id == EE_I_OR ? (cs | ct) : (cs ^ ct);

                        ee_set_const(ee, &uc, i.rd.r, v);

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (ee_one_const(ee, i.rs.r, i.rt.r, &kc, &ko)) {
                        if (i.id == EE_I_AND && kc == 0) {
                            ee_set_const(ee, &uc, i.rd.r, 0);

                            continue;
                        }

                        if (ee_fits_imm32(kc)) {
                            ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                            ee_cached_reg& rs = ee_get_reg(ee, &uc, ko);

                            switch (i.id) {
                                case EE_I_AND: uc.and_(rd.reg, rs.reg, Imm((int64_t)kc)); break;
                                case EE_I_OR: uc.or_(rd.reg, rs.reg, Imm((int64_t)kc)); break;
                                case EE_I_XOR: uc.xor_(rd.reg, rs.reg, Imm((int64_t)kc)); break;
                            }

                            ee->reg_cache[i.rd.r].constant = false;

                            continue;
                        }
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp tmp = uc.new_gp64();

                    switch (i.id) {
                        case EE_I_AND: uc.and_(tmp, rs.reg, rt.reg); break;
                        case EE_I_OR: uc.or_(tmp, rs.reg, rt.reg); break;
                        case EE_I_XOR: uc.xor_(tmp, rs.reg, rt.reg); break;
                    }

                    uc.mov(rd.reg, tmp);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_NOR: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, ~(cs | ct));

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (ee_one_const(ee, i.rs.r, i.rt.r, &kc, &ko)) {
                        if (kc == ~0ull) {
                            ee_set_const(ee, &uc, i.rd.r, 0);

                            continue;
                        }

                        if (ee_fits_imm32(kc)) {
                            ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                            ee_cached_reg& rs = ee_get_reg(ee, &uc, ko);

                            uc.or_(rd.reg, rs.reg, Imm((int64_t)kc));
                            uc.not_(rd.reg, rd.reg);

                            ee->reg_cache[i.rd.r].constant = false;

                            continue;
                        }
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp tmp = uc.new_gp64();

                    uc.or_(tmp, rs.reg, rt.reg);
                    uc.not_(tmp, tmp);
                    uc.mov(rd.reg, tmp);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_SLT: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, (int64_t)cs < (int64_t)ct ? 1 : 0);

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (ee_one_const(ee, i.rs.r, i.rt.r, &kc, &ko) && ee_fits_imm32(kc)) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                        ee_cached_reg& rn = ee_get_reg(ee, &uc, ko);

                        ujit::UniCondition c = (ko == i.rs.r) ? ujit::scmp_lt(rn.reg, Imm((int64_t)kc)) : ujit::scmp_gt(rn.reg, Imm((int64_t)kc));

                        uc.select(rd.reg, Imm(1), Imm(0), c);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.select(rd.reg, Imm(1), Imm(0), ujit::scmp_lt(rs.reg, rt.reg));

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_SLTU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, cs < ct ? 1 : 0);

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (ee_one_const(ee, i.rs.r, i.rt.r, &kc, &ko) && ee_fits_imm32(kc)) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                        ee_cached_reg& rn = ee_get_reg(ee, &uc, ko);

                        ujit::UniCondition c = (ko == i.rs.r) ? ujit::ucmp_lt(rn.reg, Imm((int64_t)kc)) : ujit::ucmp_gt(rn.reg, Imm((int64_t)kc));

                        uc.select(rd.reg, Imm(1), Imm(0), c);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.select(rd.reg, Imm(1), Imm(0), ujit::ucmp_lt(rs.reg, rt.reg));

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_SLTI: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (ee_reg_is_const(ee, i.rs.r, &cs)) {
                        ee_set_const(ee, &uc, i.rt.r, (int64_t)cs < (int64_t)(int16_t)i.i16 ? 1 : 0);

                        continue;
                    }

                    bool sync = i.rs.r == i.rt.r;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    uc.select(rt.reg, Imm(1), Imm(0), ujit::scmp_lt(rs.reg, Imm((int64_t)(int16_t)i.i16)));

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_SLTIU: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (ee_reg_is_const(ee, i.rs.r, &cs)) {
                        ee_set_const(ee, &uc, i.rt.r, cs < (uint64_t)(int64_t)(int16_t)i.i16 ? 1 : 0);

                        continue;
                    }

                    bool sync = i.rs.r == i.rt.r;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    if (i.rs.r == 0) {
                        uc.mov(rt.reg, Imm(0ull < ((int64_t)(int16_t)i.i16)));

                        continue;
                    }

                    uc.select(rt.reg, Imm(1), Imm(0), ujit::ucmp_lt(rs.reg, Imm((int64_t)(int16_t)i.i16)));

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                // NOPs
                case EE_I_CACHE:
                case EE_I_PREF:
                case EE_I_SYNC: {
                    continue;
                } break;

                case EE_I_BEQ:  emit_branch_eq(i, true,  false); break;
                case EE_I_BNE:  emit_branch_eq(i, false, false); break;
                case EE_I_BEQL: emit_branch_eq(i, true,  true);  break;
                case EE_I_BNEL: emit_branch_eq(i, false, true);  break;

                case EE_I_BLTZ:    emit_branch_z(i, EE_ZC_LTZ, false, false); break;
                case EE_I_BGEZ:    emit_branch_z(i, EE_ZC_GEZ, false, false); break;
                case EE_I_BLEZ:    emit_branch_z(i, EE_ZC_LEZ, false, false); break;
                case EE_I_BGTZ:    emit_branch_z(i, EE_ZC_GTZ, false, false); break;
                case EE_I_BLTZAL:  emit_branch_z(i, EE_ZC_LTZ, false, true);  break;
                case EE_I_BGEZAL:  emit_branch_z(i, EE_ZC_GEZ, false, true);  break;
                case EE_I_BLTZL:   emit_branch_z(i, EE_ZC_LTZ, true,  false); break;
                case EE_I_BGEZL:   emit_branch_z(i, EE_ZC_GEZ, true,  false); break;
                case EE_I_BLEZL:   emit_branch_z(i, EE_ZC_LEZ, true,  false); break;
                case EE_I_BGTZL:   emit_branch_z(i, EE_ZC_GTZ, true,  false); break;
                case EE_I_BLTZALL: emit_branch_z(i, EE_ZC_LTZ, true,  true);  break;
                case EE_I_BGEZALL: emit_branch_z(i, EE_ZC_GEZ, true,  true);  break;

                case EE_I_BC0F: {
                    ujit::Gp cc = uc.new_gp32();
                    uc.load_u32(cc, EE(cpcond0));
                    emit_branch(ujit::test_nz(cc), EE_D_SI16);
                } break;

                case EE_I_BC0T: {
                    ujit::Gp cc = uc.new_gp32();
                    uc.load_u32(cc, EE(cpcond0));
                    emit_branch(ujit::test_z(cc), EE_D_SI16);
                } break;

                case EE_I_BC0FL: {
                    ujit::Gp cc = uc.new_gp32();
                    uc.load_u32(cc, EE(cpcond0));
                    emit_branch_likely(ujit::test_z(cc), EE_D_SI16);
                } break;

                case EE_I_BC0TL: {
                    ujit::Gp cc = uc.new_gp32();
                    uc.load_u32(cc, EE(cpcond0));
                    emit_branch_likely(ujit::test_nz(cc), EE_D_SI16);
                } break;

                case EE_I_BC1F: {
                    ujit::Gp f = uc.new_gp32();
                    uc.load_u32(f, EE(fcr));
                    emit_branch(ujit::test_nz(f, Imm(FPU_FLG_C)), EE_D_SI16);
                } break;

                case EE_I_BC1T: {
                    ujit::Gp f = uc.new_gp32();
                    uc.load_u32(f, EE(fcr));
                    emit_branch(ujit::test_z(f, Imm(FPU_FLG_C)), EE_D_SI16);
                } break;

                case EE_I_BC1FL: {
                    ujit::Gp f = uc.new_gp32();
                    uc.load_u32(f, EE(fcr));
                    emit_branch_likely(ujit::test_z(f, Imm(FPU_FLG_C)), EE_D_SI16);
                } break;

                case EE_I_BC1TL: {
                    ujit::Gp f = uc.new_gp32();
                    uc.load_u32(f, EE(fcr));
                    emit_branch_likely(ujit::test_nz(f, Imm(FPU_FLG_C)), EE_D_SI16);
                } break;

                case EE_I_BC2F:
                case EE_I_BC2FL: {
                    emit_branch_target(EE_D_SI16);
                } break;

                case EE_I_BC2T: {
                } break;

                case EE_I_BC2TL: {
                    ee_flush_reg_cache(ee, &uc);
                    uc.ret();
                } break;

                case EE_I_J: {
                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((i.i26 << 2) | (sb_end_pc & 0xF0000000)));

                    Label L0 = uc.new_label();
                    Label L1 = uc.new_label();

                    ujit::Gp skip_fmv = uc.new_gp32();

                    uc.load_u32(skip_fmv, EE(fmv_skip));
                    uc.j(L0, ujit::test_z(skip_fmv));

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        (uintptr_t)ee_skip_fmv,
                        FuncSignature::build<int, ee_state*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, tmp);
                    invoke_node->set_ret(0, skip_fmv);

                    uc.j(L1, ujit::test_nz(skip_fmv));

                    uc.bind(L0);
                    uc.store_u32(EE(next_pc), tmp);

                    uc.bind(L1);
                } break;

                case EE_I_JR: {
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    uc.store_u32(EE(next_pc), rs.reg);
                } break;

                case EE_I_JAL: {
                    ee_cached_reg& ra = ee_get_reg(ee, &uc, 31, false);

                    uc.mov(ra.reg, Imm((uint64_t)sb_end_pc));

                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((i.i26 << 2) | (sb_end_pc & 0xF0000000)));

                    Label L0 = uc.new_label();
                    Label L1 = uc.new_label();

                    ujit::Gp skip_fmv = uc.new_gp32();

                    uc.load_u32(skip_fmv, EE(fmv_skip));
                    uc.j(L0, ujit::test_z(skip_fmv));

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        (uintptr_t)ee_skip_fmv,
                        FuncSignature::build<int, ee_state*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, tmp);
                    invoke_node->set_ret(0, skip_fmv);

                    uc.j(L1, ujit::test_nz(skip_fmv));

                    uc.bind(L0);
                    uc.store_u32(EE(next_pc), tmp);

                    uc.bind(L1);
                } break;

                case EE_I_JALR: {
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    if (!i.rd.r) {
                        uc.store_u32(EE(next_pc), rs.reg);
                    } else {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, false);

                        uc.store_u32(EE(next_pc), rs.reg);
                        uc.mov(rd.reg, Imm((uint64_t)sb_end_pc));
                    }
                } break;

                case EE_I_SLL: {
                    if (!i.rd.r) continue;

                    uint64_t ct;

                    if (ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)ct << i.sa));

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.shl(rd.reg, rt.reg, Imm(i.sa));

                    ee_sext32(uc, rd.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_SRL: {
                    if (!i.rd.r) continue;

                    uint64_t ct;

                    if (ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, (uint64_t)((uint32_t)ct >> i.sa));

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp tmp = uc.new_gp64();

                    uc.and_(tmp, rt.reg, Imm(0xFFFFFFFF));
                    uc.shr(rd.reg, tmp, Imm(i.sa));

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_SRA: {
                    if (!i.rd.r) continue;

                    uint64_t ct;

                    if (ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((int32_t)ct >> i.sa));

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp tmp1 = rd.reg.r32();
                    ujit::Gp tmp2 = rt.reg.r32();

                    uc.sar(tmp1, tmp2, Imm(i.sa));

                    ee_sext32(uc, rd.reg, tmp1);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_SLLV: {
                    if (!i.rd.r) continue;

                    uint64_t camt;

                    if (ee_reg_is_const(ee, i.rs.r, &camt)) {
                        uint32_t sa = (uint32_t)camt & 0x1f;
                        uint64_t cval;

                        if (ee_reg_is_const(ee, i.rt.r, &cval)) {
                            ee_set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)cval << sa));

                            continue;
                        }

                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, i.rt.r == i.rd.r);
                        ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                        uc.shl(rd.reg, rt.reg, Imm(sa));

                        ee_sext32(uc, rd.reg);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r || i.rs.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp64();

                    uc.and_(tmp, rs.reg, Imm(0x1F));
                    uc.shl(rd.reg, rt.reg, tmp);

                    ee_sext32(uc, rd.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_SRLV: {
                    if (!i.rd.r) continue;

                    uint64_t camt;

                    if (ee_reg_is_const(ee, i.rs.r, &camt)) {
                        uint32_t sa = (uint32_t)camt & 0x1f;
                        uint64_t cval;

                        if (ee_reg_is_const(ee, i.rt.r, &cval)) {
                            ee_set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)cval >> sa));

                            continue;
                        }

                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, i.rt.r == i.rd.r);
                        ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                        ujit::Gp rd32 = rd.reg.r32();
                        ujit::Gp rt32 = rt.reg.r32();

                        uc.shr(rd32, rt32, Imm(sa));

                        ee_sext32(uc, rd.reg, rd32);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r || i.rs.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp32();
                    ujit::Gp rd32 = rd.reg.r32();
                    ujit::Gp rt32 = rt.reg.r32();
                    ujit::Gp rs32 = rs.reg.r32();

                    uc.and_(tmp, rs32, Imm(0x1F));
                    uc.shr(rd32, rt32, tmp);

                    ee_sext32(uc, rd.reg, rd32);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_SRAV: {
                    if (!i.rd.r) continue;

                    uint64_t camt;

                    if (ee_reg_is_const(ee, i.rs.r, &camt)) {
                        uint32_t sa = (uint32_t)camt & 0x1f;
                        uint64_t cval;

                        if (ee_reg_is_const(ee, i.rt.r, &cval)) {
                            ee_set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((int32_t)cval >> sa));

                            continue;
                        }

                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, i.rt.r == i.rd.r);
                        ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                        ujit::Gp rd32 = rd.reg.r32();
                        ujit::Gp rt32 = rt.reg.r32();

                        uc.sar(rd32, rt32, Imm(sa));

                        ee_sext32(uc, rd.reg, rd32);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r || i.rs.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp32();
                    ujit::Gp rd32 = rd.reg.r32();
                    ujit::Gp rt32 = rt.reg.r32();
                    ujit::Gp rs32 = rs.reg.r32();

                    uc.and_(tmp, rs32, Imm(0x1F));
                    uc.sar(rd32, rt32, tmp);

                    ee_sext32(uc, rd.reg, rd32);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_DSLL:
                case EE_I_DSRL:
                case EE_I_DSRA:
                case EE_I_DSLL32:
                case EE_I_DSRL32:
                case EE_I_DSRA32: {
                    if (!i.rd.r) continue;

                    uint64_t ct;
                    if (ee_reg_is_const(ee, i.rt.r, &ct)) {
                        uint32_t sa = i.sa + ((i.id == EE_I_DSLL32 || i.id == EE_I_DSRL32 || i.id == EE_I_DSRA32) ? 32 : 0);
                        uint64_t v;

                        switch (i.id) {
                            case EE_I_DSLL:
                            case EE_I_DSLL32: {
                                v = ct << sa;
                            } break;

                            case EE_I_DSRL: 
                            case EE_I_DSRL32: {
                                v = ct >> sa;
                            } break;

                            default: {
                                v = (uint64_t)((int64_t)ct >> sa);
                            } break;
                        }

                        ee_set_const(ee, &uc, i.rd.r, v);

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    switch (i.id) {
                        case EE_I_DSLL: uc.shl(rd.reg, rt.reg, Imm(i.sa)); break;
                        case EE_I_DSRL: uc.shr(rd.reg, rt.reg, Imm(i.sa)); break;
                        case EE_I_DSRA: uc.sar(rd.reg, rt.reg, Imm(i.sa)); break;
                        case EE_I_DSLL32: uc.shl(rd.reg, rt.reg, Imm(i.sa + 32)); break;
                        case EE_I_DSRL32: uc.shr(rd.reg, rt.reg, Imm(i.sa + 32)); break;
                        case EE_I_DSRA32: uc.sar(rd.reg, rt.reg, Imm(i.sa + 32)); break;
                    }

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_DSLLV:
                case EE_I_DSRLV:
                case EE_I_DSRAV: {
                    if (!i.rd.r) continue;

                    uint64_t camt;

                    if (ee_reg_is_const(ee, i.rs.r, &camt)) {
                        uint32_t sa = (uint32_t)camt & 0x3f;
                        uint64_t cval;

                        if (ee_reg_is_const(ee, i.rt.r, &cval)) {
                            uint64_t v;

                            switch (i.id) {
                                case EE_I_DSLLV: v = cval << sa; break;
                                case EE_I_DSRLV: v = cval >> sa; break;
                                case EE_I_DSRAV: v = (uint64_t)((int64_t)cval >> sa); break;
                            }

                            ee_set_const(ee, &uc, i.rd.r, v);

                            continue;
                        }

                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, i.rt.r == i.rd.r);
                        ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                        switch (i.id) {
                            case EE_I_DSLLV: uc.shl(rd.reg, rt.reg, Imm(sa)); break;
                            case EE_I_DSRLV: uc.shr(rd.reg, rt.reg, Imm(sa)); break;
                            case EE_I_DSRAV: uc.sar(rd.reg, rt.reg, Imm(sa)); break;
                        }

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r || i.rs.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp64();

                    uc.and_(tmp, rs.reg, Imm(0x3F));

                    switch (i.id) {
                        case EE_I_DSLLV: uc.shl(rd.reg, rt.reg, tmp); break;
                        case EE_I_DSRLV: uc.shr(rd.reg, rt.reg, tmp); break;
                        case EE_I_DSRAV: uc.sar(rd.reg, rt.reg, tmp); break;
                    }

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_LUI: {
                    if (!i.rt.r) continue;

                    ee_set_const(ee, &uc, i.rt.r, (int64_t)(int32_t)(i.i16 << 16));

                    continue;
                } break;

                case EE_I_LWC1: {
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp32();
                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        bus_read32,
                        FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_ret(0, tmp);

                    uc.store_u32(EE(f[i.rt.r]), tmp);
                } break;

                case EE_I_SWC1: {
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();
                    ujit::Gp val = uc.new_gp64();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.load_u32(val, EE(f[i.rt.r]));

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        (uintptr_t)bus_write32,
                        FuncSignature::build<void, ee_state*, uint32_t, uint64_t>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, val);
                } break;

                case EE_I_MTC1: {
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.store_u32(EE(f[i.rd.r]), rt.reg.r32());
                } break;

                case EE_I_MFC1: {
                    if (!i.rt.r) continue;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, false);

                    uc.load_u32(rt.reg, EE(f[i.rd.r]));
                    ee_sext32(uc, rt.reg);

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_MOVS:
                case EE_I_ABSS:
                case EE_I_NEGS: {
                    ujit::Gp tmp = uc.new_gp32();

                    uc.load_u32(tmp, EE(f[i.rd.r]));

                    switch (i.id) {
                        case EE_I_ABSS: uc.and_(tmp, tmp, Imm(0x7fffffff)); break;
                        case EE_I_NEGS: uc.xor_(tmp, tmp, Imm(0x80000000)); break;
                    }

                    uc.store_u32(EE(f[i.sa]), tmp);

                    if (i.id != EE_I_MOVS) {
                        ujit::Gp fcr = uc.new_gp32();

                        uc.load_u32(fcr, EE(fcr));
                        uc.and_(fcr, fcr, Imm(~(FPU_FLG_O | FPU_FLG_U)));
                        uc.store_u32(EE(fcr), fcr);
                    }
                } break;

                case EE_I_ADDS:
                case EE_I_SUBS:
                case EE_I_MULS:
                case EE_I_DIVS: {
                    ujit::Gp fs  = uc.new_gp32();
                    ujit::Gp ft  = uc.new_gp32();
                    ujit::Gp fcr = uc.new_gp32();
                    ujit::Gp out = uc.new_gp32();

                    uc.load_u32(fs, EE(f[i.rd.r]));
                    uc.load_u32(ft, EE(f[i.rt.r]));
                    uc.load_u32(fcr, EE(fcr));

                    switch (i.id) {
                        case EE_I_ADDS: ee_fpu::adds(uc, out, fcr, fs, ft); break;
                        case EE_I_SUBS: ee_fpu::subs(uc, out, fcr, fs, ft); break;
                        case EE_I_MULS: ee_fpu::muls(uc, out, fcr, fs, ft); break;
                        case EE_I_DIVS: ee_fpu::divs(uc, out, fcr, fs, ft); break;
                    }

                    uc.store_u32(EE(f[i.sa]), out);
                    uc.store_u32(EE(fcr), fcr);
                } break;

                case EE_I_MAXS:
                case EE_I_MINS: {
                    ujit::Gp fs = uc.new_gp32();
                    ujit::Gp ft = uc.new_gp32();
                    ujit::Gp out = uc.new_gp32();

                    uc.load_u32(fs, EE(f[i.rd.r]));
                    uc.load_u32(ft, EE(f[i.rt.r]));

                    if (i.id == EE_I_MAXS) {
                        ee_fpu::maxs(uc, out, fs, ft);
                    } else {
                        ee_fpu::mins(uc, out, fs, ft);   
                    }

                    uc.store_u32(EE(f[i.sa]), out);

                    ujit::Gp fcr = uc.new_gp32();

                    uc.load_u32(fcr, EE(fcr));
                    uc.and_(fcr, fcr, Imm(~(uint32_t)(FPU_FLG_O | FPU_FLG_U)));
                    uc.store_u32(EE(fcr), fcr);
                } break;

                case EE_I_ADDAS:
                case EE_I_SUBAS:
                case EE_I_MULAS:
                case EE_I_MADDAS:
                case EE_I_MSUBAS:
                case EE_I_MADDS:
                case EE_I_MSUBS:
                case EE_I_SQRTS:
                case EE_I_RSQRTS: {
                    ujit::Gp fs = uc.new_gp32();
                    ujit::Gp ft = uc.new_gp32();
                    ujit::Gp fcr = uc.new_gp32();
                    ujit::Gp out = uc.new_gp32();

                    uc.load_u32(fs, EE(f[i.rd.r]));
                    uc.load_u32(ft, EE(f[i.rt.r]));
                    uc.load_u32(fcr, EE(fcr));

                    bool to_acc = i.id == EE_I_ADDAS || i.id == EE_I_SUBAS || i.id == EE_I_MULAS ||
                                i.id == EE_I_MADDAS || i.id == EE_I_MSUBAS;

                    switch (i.id) {
                        case EE_I_ADDAS: ee_fpu::adds(uc, out, fcr, fs, ft); break;
                        case EE_I_SUBAS: ee_fpu::subs(uc, out, fcr, fs, ft); break;
                        case EE_I_MULAS: ee_fpu::muls(uc, out, fcr, fs, ft); break;
                        case EE_I_SQRTS: ee_fpu::sqrts(uc, out, fcr, ft); break;
                        case EE_I_RSQRTS: ee_fpu::rsqrts(uc, out, fcr, fs, ft); break;

                        default: {
                            ujit::Gp acc = uc.new_gp32(); uc.load_u32(acc, EE(a));

                            switch (i.id) {
                                case EE_I_MADDS: ee_fpu::madds(uc, out, fcr, acc, fs, ft); break;
                                case EE_I_MSUBS: ee_fpu::msubs(uc, out, fcr, acc, fs, ft); break;
                                case EE_I_MADDAS: ee_fpu::maddas(uc, out, fcr, acc, fs, ft); break;
                                case EE_I_MSUBAS: ee_fpu::msubas(uc, out, fcr, acc, fs, ft); break;
                            }
                        } break;
                    }

                    uc.store_u32(to_acc ? EE(a) : EE(f[i.sa]), out);
                    uc.store_u32(EE(fcr), fcr);
                } break;

                case EE_I_CF:
                case EE_I_CEQ:
                case EE_I_CLT:
                case EE_I_CLE: {
                    ujit::Gp fcr = uc.new_gp32();

                    uc.load_u32(fcr, EE(fcr));

                    if (i.id == EE_I_CF) {
                        ee_fpu::cf(uc, fcr);
                    } else {
                        ujit::Gp fs = uc.new_gp32();
                        ujit::Gp ft = uc.new_gp32();

                        uc.load_u32(fs, EE(f[i.rd.r]));
                        uc.load_u32(ft, EE(f[i.rt.r]));

                        switch (i.id) {
                            case EE_I_CEQ: ee_fpu::ceq(uc, fcr, fs, ft); break;
                            case EE_I_CLT: ee_fpu::clt(uc, fcr, fs, ft); break;
                            case EE_I_CLE: ee_fpu::cle(uc, fcr, fs, ft); break;
                        }
                    }

                    uc.store_u32(EE(fcr), fcr);
                } break;

                case EE_I_CVTW:
                case EE_I_CVTS: {
                    ujit::Gp fs = uc.new_gp32();
                    ujit::Gp out = uc.new_gp32();

                    uc.load_u32(fs, EE(f[i.rd.r]));

                    if (i.id == EE_I_CVTW) {
                        ee_fpu::cvtws(uc, out, fs);
                    } else {
                        ee_fpu::cvtsw(uc, out, fs);
                    }

                    uc.store_u32(EE(f[i.sa]), out);
                } break;

                case EE_I_LB:
                case EE_I_LH:
                case EE_I_LW:
                case EE_I_LD: {
                    uintptr_t func;
                    int bytes;

                    switch (i.id) {
                        case EE_I_LB: func = (uintptr_t)bus_read8;  bytes = 1; break;
                        case EE_I_LH: func = (uintptr_t)bus_read16; bytes = 2; break;
                        case EE_I_LW: func = (uintptr_t)bus_read32; bytes = 4; break;
                        case EE_I_LD: func = (uintptr_t)bus_read64; bytes = 8; break;
                    }

                    uint64_t cs;
                    uint32_t phys;

                    void* host = nullptr;

                    if (ee_reg_is_const(ee, i.rs.r, &cs))
                        host = ee_fold_host_ptr(ee, (uint32_t)cs + (int32_t)(int16_t)i.i16, bytes, false, &phys);

                    if (host) {
                        if (i.rt.r) {
                            ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, false);
                            ujit::Gp base = ee_fold_base(uc, host);

                            switch (i.id) {
                                case EE_I_LB: uc.load_i8(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case EE_I_LH: uc.load_i16(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case EE_I_LW: uc.load_i32(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case EE_I_LD: uc.load_u64(rt.reg, ujit::mem_ptr(base, 0)); break;
                            }

                            ee->reg_cache[i.rt.r].constant = false;
                        }

                        continue;
                    }

                    if (ee->vfast_r && i.rt.r) {
                        uintptr_t slow_func;

                        switch (i.id) {
                            case EE_I_LB: slow_func = (uintptr_t)ee_vfast_read8;  break;
                            case EE_I_LH: slow_func = (uintptr_t)ee_vfast_read16; break;
                            case EE_I_LW: slow_func = (uintptr_t)ee_vfast_read32; break;
                            case EE_I_LD: slow_func = (uintptr_t)ee_vfast_read64; break;
                        }

                        ee_cached_reg& frt = ee_get_reg(ee, &uc, i.rt.r, i.rt.r == i.rs.r);
                        ee_cached_reg& frs = ee_get_reg(ee, &uc, i.rs.r);

                        ujit::Gp addr = uc.new_gp32();
                        ujit::Gp pidx = uc.new_gp32();
                        ujit::Gp idx = uc.new_gp64();
                        ujit::Gp tbl = uc.new_gp_ptr();
                        ujit::Gp base = uc.new_gp_ptr();
                        ujit::Gp off = uc.new_gp64();

                        uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                        uc.add(addr, addr, frs.reg.r32());

                        asmjit::Label slow = uc.new_label();
                        asmjit::Label done = uc.new_label();

                        uc.shr(pidx, addr, Imm(12));
                        uc.mov(idx.r32(), pidx);
                        uc.mov(tbl, Imm((uint64_t)(uintptr_t)ee->vfast_r));

                        uc.load_u64(base, ujit::mem_ptr(tbl, idx, 3));
                        uc.j(slow, ujit::test_z(base));

                        uc.mov(off.r32(), addr);
                        uc.and_(off, off, Imm(0xfff));

                        switch (i.id) {
                            case EE_I_LB: uc.load_u8(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case EE_I_LH: uc.load_u16(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case EE_I_LW: uc.load_u32(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case EE_I_LD: uc.load_u64(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                        }

                        uc.j(done);
                        uc.bind(slow);

                        InvokeNode* slow_node;

                        bc.invoke(
                            Out(slow_node),
                            slow_func,
                            FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                        );

                        slow_node->set_arg(0, ee->ee_ptr);
                        slow_node->set_arg(1, addr);
                        slow_node->set_ret(0, frt.reg);

                        uc.bind(done);

                        switch (i.id) {
                            case EE_I_LB: ee_sextn(uc, frt.reg, 8); break;
                            case EE_I_LH: ee_sextn(uc, frt.reg, 16); break;
                            case EE_I_LW: ee_sext32(uc, frt.reg); break;
                        }

                        ee->reg_cache[i.rt.r].constant = false;

                        continue;
                    }

                    if (!i.rt.r) {
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                        ujit::Gp tmp = uc.new_gp32();

                        uc.mov(tmp, Imm((int32_t)(int16_t)i.i16));
                        uc.add(tmp, tmp, rs.reg.r32());

                        InvokeNode* invoke_node;

                        bc.invoke(
                            Out(invoke_node),
                            func,
                            FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                        );

                        invoke_node->set_arg(0, ee->ee_ptr);
                        invoke_node->set_arg(1, tmp);

                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((int32_t)(int16_t)i.i16));
                    uc.add(tmp, tmp, rs.reg.r32());

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        func,
                        FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, tmp);
                    invoke_node->set_ret(0, rt.reg);

                    if (i.id != EE_I_LD) {
                        switch (i.id) {
                            case EE_I_LB: ee_sextn(uc, rt.reg, 8); break;
                            case EE_I_LH: ee_sextn(uc, rt.reg, 16); break;
                            case EE_I_LW: ee_sext32(uc, rt.reg); break;
                        }
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_LDL:
                case EE_I_LDR: {
                    bool is_l = i.id == EE_I_LDL;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();
                    ujit::Gp aligned = uc.new_gp32();
                    ujit::Gp off = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(aligned, addr, Imm(~7));
                    uc.and_(off, addr, Imm(7));

                    ujit::Gp data = uc.new_gp64();

                    InvokeNode* rd_node;

                    bc.invoke(
                        Out(rd_node),
                        (uintptr_t)bus_read64,
                        FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                    );

                    rd_node->set_arg(0, ee->ee_ptr);
                    rd_node->set_arg(1, aligned);
                    rd_node->set_ret(0, data);

                    if (!i.rt.r) continue;

                    ujit::Gp sh = uc.new_gp32();

                    if (is_l) {
                        uc.mov(sh, Imm(7));
                        uc.sub(sh, sh, off);
                        uc.shl(sh, sh, Imm(3));
                    } else {
                        uc.shl(sh, off, Imm(3));
                    }

                    ujit::Gp shifted = uc.new_gp64();

                    if (is_l) {
                        uc.shl(shifted, data, sh);
                    } else {
                        uc.shr(shifted, data, sh);
                    }

                    ujit::Gp mask = uc.new_gp64();

                    if (is_l) {
                        uc.mov(mask, Imm(1));
                        uc.shl(mask, mask, sh);
                        uc.sub(mask, mask, Imm(1));
                    } else {
                        uc.mov(mask, Imm((int64_t)-1));
                        uc.shr(mask, mask, sh);
                        uc.not_(mask, mask);
                    }

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.and_(rt.reg, rt.reg, mask);
                    uc.or_(rt.reg, rt.reg, shifted);

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_LBU:
                case EE_I_LHU:
                case EE_I_LWU: {
                    uintptr_t func;
                    int bytes;

                    switch (i.id) {
                        case EE_I_LBU: func = (uintptr_t)bus_read8;  bytes = 1; break;
                        case EE_I_LHU: func = (uintptr_t)bus_read16; bytes = 2; break;
                        case EE_I_LWU: func = (uintptr_t)bus_read32; bytes = 4; break;
                    }

                    uint64_t cs;
                    uint32_t phys;

                    void* host = nullptr;

                    if (ee_reg_is_const(ee, i.rs.r, &cs)) {
                        host = ee_fold_host_ptr(ee, (uint32_t)cs + (int32_t)(int16_t)i.i16, bytes, false, &phys);
                    }

                    if (host) {
                        if (i.rt.r) {
                            ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, false);

                            ujit::Gp base = ee_fold_base(uc, host);

                            switch (i.id) {
                                case EE_I_LBU: uc.load_u8(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case EE_I_LHU: uc.load_u16(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case EE_I_LWU: uc.load_u32(rt.reg, ujit::mem_ptr(base, 0)); break;
                            }

                            ee->reg_cache[i.rt.r].constant = false;
                        }

                        continue;
                    }

                    if (ee->vfast_r && i.rt.r) {
                        uintptr_t slow_func;

                        switch (i.id) {
                            case EE_I_LBU: slow_func = (uintptr_t)ee_vfast_read8; break;
                            case EE_I_LHU: slow_func = (uintptr_t)ee_vfast_read16; break;
                            case EE_I_LWU: slow_func = (uintptr_t)ee_vfast_read32; break;
                        }

                        ee_cached_reg& frt = ee_get_reg(ee, &uc, i.rt.r, i.rt.r == i.rs.r);
                        ee_cached_reg& frs = ee_get_reg(ee, &uc, i.rs.r);

                        ujit::Gp addr = uc.new_gp32();
                        ujit::Gp pidx = uc.new_gp32();
                        ujit::Gp idx = uc.new_gp64();
                        ujit::Gp tbl = uc.new_gp_ptr();
                        ujit::Gp base = uc.new_gp_ptr();
                        ujit::Gp off = uc.new_gp64();

                        uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                        uc.add(addr, addr, frs.reg.r32());

                        asmjit::Label slow = uc.new_label();
                        asmjit::Label done = uc.new_label();

                        uc.shr(pidx, addr, Imm(12));
                        uc.mov(idx.r32(), pidx);
                        uc.mov(tbl, Imm((uint64_t)(uintptr_t)ee->vfast_r));

                        uc.load_u64(base, ujit::mem_ptr(tbl, idx, 3));
                        uc.j(slow, ujit::test_z(base));

                        uc.mov(off.r32(), addr);
                        uc.and_(off, off, Imm(0xfff));

                        switch (i.id) {
                            case EE_I_LBU: uc.load_u8(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case EE_I_LHU: uc.load_u16(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case EE_I_LWU: uc.load_u32(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                        }

                        uc.j(done);
                        uc.bind(slow);

                        InvokeNode* slow_node;

                        bc.invoke(
                            Out(slow_node),
                            slow_func,
                            FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                        );

                        slow_node->set_arg(0, ee->ee_ptr);
                        slow_node->set_arg(1, addr);
                        slow_node->set_ret(0, frt.reg);

                        uc.bind(done);

                        ee->reg_cache[i.rt.r].constant = false;

                        continue;
                    }

                    if (!i.rt.r) {
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                        ujit::Gp tmp = uc.new_gp32();

                        uc.mov(tmp, Imm((int32_t)(int16_t)i.i16));
                        uc.add(tmp, tmp, rs.reg.r32());

                        InvokeNode* invoke_node;

                        bc.invoke(
                            Out(invoke_node),
                            func,
                            FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                        );

                        invoke_node->set_arg(0, ee->ee_ptr);
                        invoke_node->set_arg(1, tmp);

                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((int32_t)(int16_t)i.i16));
                    uc.add(tmp, tmp, rs.reg.r32());

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        func,
                        FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, tmp);
                    invoke_node->set_ret(0, rt.reg);

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_LQ: {
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(addr, addr, Imm(~0xf));

                    int rt = i.rt.r;

                    if (rt) {
                        ee->vec_cache[rt].valid = false;
                        ee->vec_cache[rt].dirty = false;
                        ee->reg_cache[rt].valid = false;
                        ee->reg_cache[rt].constant = false;
                    }

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.lea(ptr, ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + rt * sizeof(uint128_t)));

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        (uintptr_t)ee_jit_read128,
                        FuncSignature::build<void, ee_state*, uint32_t, uint128_t*>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, ptr);

                    if (!rt) {
                        uc.store_zero_u64(EE(r[0].u64[0]));
                        uc.store_zero_u64(EE(r[0].u64[1]));
                    }
                } break;

                case EE_I_SB:
                case EE_I_SH:
                case EE_I_SW:
                case EE_I_SD: {
                    uintptr_t func;
                    int bytes;

                    switch (i.id) {
                        case EE_I_SB: func = (uintptr_t)bus_write8; bytes = 1; break;
                        case EE_I_SH: func = (uintptr_t)bus_write16; bytes = 2; break;
                        case EE_I_SW: func = (uintptr_t)bus_write32; bytes = 4; break;
                        case EE_I_SD: func = (uintptr_t)bus_write64; bytes = 8; break;
                    }

                    {
                        uint64_t cs;
                        uint32_t phys;
                        void* host = nullptr;

                        if (ee_reg_is_const(ee, i.rs.r, &cs))
                            host = ee_fold_host_ptr(ee, (uint32_t)cs + (int32_t)(int16_t)i.i16, bytes, true, &phys);

                        if (host) {
                            ee_cached_reg& frt = ee_get_reg(ee, &uc, i.rt.r);

                            if (phys != EE_FOLD_NO_PHYS) {
                                ee_cache_page* pg = &ee->block_cache[phys / EE_MIN_PAGESIZE];

                                asmjit::Label no_smc = uc.new_label();
                                ujit::Gp pgp = uc.new_gp_ptr();
                                ujit::Gp pgv = uc.new_gp32();

                                uc.mov(pgp, Imm((uint64_t)(uintptr_t)pg));
                                uc.load_u8(pgv, ujit::mem_ptr(pgp, offsetof(ee_cache_page, valid)));
                                uc.j(no_smc, ujit::test_z(pgv));

                                InvokeNode* inv_node;

                                bc.invoke(
                                    Out(inv_node),
                                    (uintptr_t)ee_invalidate_page,
                                    FuncSignature::build<void, ee_state*, uint32_t>()
                                );

                                inv_node->set_arg(0, ee->ee_ptr);
                                inv_node->set_arg(1, Imm(phys));

                                uc.bind(no_smc);
                            }

                            ujit::Gp base = ee_fold_base(uc, host);

                            switch (i.id) {
                                case EE_I_SB: uc.store_u8(ujit::mem_ptr(base, 0), frt.reg); break;
                                case EE_I_SH: uc.store_u16(ujit::mem_ptr(base, 0), frt.reg); break;
                                case EE_I_SW: uc.store_u32(ujit::mem_ptr(base, 0), frt.reg); break;
                                case EE_I_SD: uc.store_u64(ujit::mem_ptr(base, 0), frt.reg); break;
                            }

                            continue;
                        }
                    }

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp1 = uc.new_gp64();

                    uc.mov(tmp1, Imm((int32_t)(int16_t)i.i16));
                    uc.add(tmp1, tmp1, rs.reg);

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        func,
                        FuncSignature::build<void, ee_state*, uint32_t, uint64_t>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, tmp1);
                    invoke_node->set_arg(2, rt.reg);
                } break;

                case EE_I_SDL:
                case EE_I_SDR: {
                    bool is_l = i.id == EE_I_SDL;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp addr = uc.new_gp32();
                    ujit::Gp aligned = uc.new_gp32();
                    ujit::Gp off = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(aligned, addr, Imm(~7));
                    uc.and_(off, addr, Imm(7));

                    ujit::Gp data = uc.new_gp64();

                    InvokeNode* rd_node;

                    bc.invoke(
                        Out(rd_node),
                        (uintptr_t)bus_read64,
                        FuncSignature::build<uint64_t, ee_state*, uint32_t>()
                    );

                    rd_node->set_arg(0, ee->ee_ptr);
                    rd_node->set_arg(1, aligned);
                    rd_node->set_ret(0, data);

                    ujit::Gp sh = uc.new_gp32();

                    if (is_l) {
                        uc.mov(sh, Imm(7));
                        uc.sub(sh, sh, off);
                        uc.shl(sh, sh, Imm(3));
                    } else {
                        uc.shl(sh, off, Imm(3));
                    }

                    ujit::Gp rtsh = uc.new_gp64();

                    if (is_l) {
                        uc.shr(rtsh, rt.reg, sh);
                    } else {
                        uc.shl(rtsh, rt.reg, sh);
                    }

                    ujit::Gp mask = uc.new_gp64();

                    if (is_l) {
                        uc.mov(mask, Imm((int64_t)-1));
                        uc.shr(mask, mask, sh);
                        uc.not_(mask, mask);
                    } else {
                        uc.mov(mask, Imm(1));
                        uc.shl(mask, mask, sh);
                        uc.sub(mask, mask, Imm(1));
                    }

                    ujit::Gp store_val = uc.new_gp64();

                    uc.and_(store_val, data, mask);
                    uc.or_(store_val, store_val, rtsh);

                    InvokeNode* wr_node;

                    bc.invoke(
                        Out(wr_node),
                        (uintptr_t)bus_write64,
                        FuncSignature::build<void, ee_state*, uint32_t, uint64_t>()
                    );

                    wr_node->set_arg(0, ee->ee_ptr);
                    wr_node->set_arg(1, aligned);
                    wr_node->set_arg(2, store_val);
                } break;

                case EE_I_SQ: {
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(addr, addr, Imm(~0xf));

                    ee_sync_reg_to_mem(ee, uc, i.rt.r);

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.lea(ptr, ujit::mem_ptr(ee->ee_ptr, offsetof(ee_state, r) + i.rt.r * sizeof(uint128_t)));

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        (uintptr_t)ee_jit_write128,
                        FuncSignature::build<void, ee_state*, uint32_t, uint128_t*>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, ptr);
                } break;

                case EE_I_LQC2: {
                    int rt = i.rt.r;

                    if (!rt) continue;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(addr, addr, Imm(~0xf));

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.load_u64(ptr, EE(vu0));
                    uc.lea(ptr, ujit::mem_ptr(ptr, (int)(offsetof(vu_state, vf) + rt * sizeof(vu_reg128))));

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        (uintptr_t)ee_jit_read128,
                        FuncSignature::build<void, ee_state*, uint32_t, uint128_t*>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, ptr);
                } break;

                case EE_I_SQC2: {
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(addr, addr, Imm(~0xf));

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.load_u64(ptr, EE(vu0));
                    uc.lea(ptr, ujit::mem_ptr(ptr, (int)(offsetof(vu_state, vf) + i.rt.r * sizeof(vu_reg128))));

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        (uintptr_t)ee_jit_write128,
                        FuncSignature::build<void, ee_state*, uint32_t, uint128_t*>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, ptr);
                } break;

                case EE_I_QMFC2: {
                    if (!i.rt.r) continue;

                    ujit::Gp ptr = uc.new_gp_ptr();
                    uc.load_u64(ptr, EE(vu0));

                    ujit::Vec v = uc.new_vec128();
                    uc.v_loadu128(v, ujit::mem_ptr(ptr, (int)(offsetof(vu_state, vf) + i.rd.r * sizeof(vu_reg128))));

                    ee_set_vec(ee, uc, i.rt.r, v);
                } break;

                case EE_I_QMTC2: {
                    if (!i.rd.r) continue;

                    ujit::Vec v = ee_get_vec(ee, uc, i.rt.r);

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.load_u64(ptr, EE(vu0));
                    uc.v_storeu128(ujit::mem_ptr(ptr, (int)(offsetof(vu_state, vf) + i.rd.r * sizeof(vu_reg128))), v);
                } break;

                case EE_I_PADDB:  case EE_I_PADDH:  case EE_I_PADDW:
                case EE_I_PSUBB:  case EE_I_PSUBH:  case EE_I_PSUBW:
                case EE_I_PADDUB: case EE_I_PADDUH: case EE_I_PSUBUB: case EE_I_PSUBUH:
                case EE_I_PADDSB: case EE_I_PADDSH: case EE_I_PSUBSB: case EE_I_PSUBSH:
                case EE_I_PADDSW: case EE_I_PSUBSW: case EE_I_PADDUW: case EE_I_PSUBUW:
                case EE_I_PADSBH:
                case EE_I_PAND:   case EE_I_POR:    case EE_I_PXOR:   case EE_I_PNOR:
                case EE_I_PCEQB:  case EE_I_PCEQH:  case EE_I_PCEQW:
                case EE_I_PCGTB:  case EE_I_PCGTH:  case EE_I_PCGTW:
                case EE_I_PMAXH:  case EE_I_PMAXW:  case EE_I_PMINH:  case EE_I_PMINW:
                case EE_I_PEXTLB: case EE_I_PEXTLH: case EE_I_PEXTLW:
                case EE_I_PEXTUB: case EE_I_PEXTUH: case EE_I_PEXTUW:
                case EE_I_PCPYLD: case EE_I_PCPYUD: case EE_I_PINTH:  case EE_I_PINTEH:
                case EE_I_PPACB:  case EE_I_PPACH:  case EE_I_PPACW: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrs = ee_get_vec(ee, uc, i.rs.r);
                    ujit::Vec vrt = ee_get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrd = uc.new_vec128();

                    switch (i.id) {
                        case EE_I_PADDB:  ee_mmi::paddb(uc, vrd, vrs, vrt); break;
                        case EE_I_PADDH:  ee_mmi::paddh(uc, vrd, vrs, vrt); break;
                        case EE_I_PADDW:  ee_mmi::paddw(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBB:  ee_mmi::psubb(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBH:  ee_mmi::psubh(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBW:  ee_mmi::psubw(uc, vrd, vrs, vrt); break;
                        case EE_I_PADDUB: ee_mmi::paddub(uc, vrd, vrs, vrt); break;
                        case EE_I_PADDUH: ee_mmi::padduh(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBUB: ee_mmi::psubub(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBUH: ee_mmi::psubuh(uc, vrd, vrs, vrt); break;
                        case EE_I_PADDSB: ee_mmi::paddsb(uc, vrd, vrs, vrt); break;
                        case EE_I_PADDSH: ee_mmi::paddsh(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBSB: ee_mmi::psubsb(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBSH: ee_mmi::psubsh(uc, vrd, vrs, vrt); break;
                        case EE_I_PADDSW: ee_mmi::paddsw(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBSW: ee_mmi::psubsw(uc, vrd, vrs, vrt); break;
                        case EE_I_PADDUW: ee_mmi::padduw(uc, vrd, vrs, vrt); break;
                        case EE_I_PSUBUW: ee_mmi::psubuw(uc, vrd, vrs, vrt); break;
                        case EE_I_PADSBH: ee_mmi::padsbh(uc, vrd, vrs, vrt); break;
                        case EE_I_PAND:   ee_mmi::pand(uc, vrd, vrs, vrt); break;
                        case EE_I_POR:    ee_mmi::por(uc, vrd, vrs, vrt); break;
                        case EE_I_PXOR:   ee_mmi::pxor(uc, vrd, vrs, vrt); break;
                        case EE_I_PNOR:   ee_mmi::pnor(uc, vrd, vrs, vrt); break;
                        case EE_I_PCEQB:  ee_mmi::pceqb(uc, vrd, vrs, vrt); break;
                        case EE_I_PCEQH:  ee_mmi::pceqh(uc, vrd, vrs, vrt); break;
                        case EE_I_PCEQW:  ee_mmi::pceqw(uc, vrd, vrs, vrt); break;
                        case EE_I_PCGTB:  ee_mmi::pcgtb(uc, vrd, vrs, vrt); break;
                        case EE_I_PCGTH:  ee_mmi::pcgth(uc, vrd, vrs, vrt); break;
                        case EE_I_PCGTW:  ee_mmi::pcgtw(uc, vrd, vrs, vrt); break;
                        case EE_I_PMAXH:  ee_mmi::pmaxh(uc, vrd, vrs, vrt); break;
                        case EE_I_PMAXW:  ee_mmi::pmaxw(uc, vrd, vrs, vrt); break;
                        case EE_I_PMINH:  ee_mmi::pminh(uc, vrd, vrs, vrt); break;
                        case EE_I_PMINW:  ee_mmi::pminw(uc, vrd, vrs, vrt); break;
                        case EE_I_PEXTLB: ee_mmi::pextlb(uc, vrd, vrs, vrt); break;
                        case EE_I_PEXTLH: ee_mmi::pextlh(uc, vrd, vrs, vrt); break;
                        case EE_I_PEXTLW: ee_mmi::pextlw(uc, vrd, vrs, vrt); break;
                        case EE_I_PEXTUB: ee_mmi::pextub(uc, vrd, vrs, vrt); break;
                        case EE_I_PEXTUH: ee_mmi::pextuh(uc, vrd, vrs, vrt); break;
                        case EE_I_PEXTUW: ee_mmi::pextuw(uc, vrd, vrs, vrt); break;
                        case EE_I_PCPYLD: ee_mmi::pcpyld(uc, vrd, vrs, vrt); break;
                        case EE_I_PCPYUD: ee_mmi::pcpyud(uc, vrd, vrs, vrt); break;
                        case EE_I_PINTH:  ee_mmi::pinth(uc, vrd, vrs, vrt); break;
                        case EE_I_PINTEH: ee_mmi::pinteh(uc, vrd, vrs, vrt); break;
                        case EE_I_PPACB:  ee_mmi::ppacb(uc, vrd, vrs, vrt); break;
                        case EE_I_PPACH:  ee_mmi::ppach(uc, vrd, vrs, vrt); break;
                        case EE_I_PPACW:  ee_mmi::ppacw(uc, vrd, vrs, vrt); break;
                    }

                    ee_set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case EE_I_PCPYH: case EE_I_PEXEH: case EE_I_PREVH: case EE_I_PEXCH:
                case EE_I_PEXEW: case EE_I_PEXCW: case EE_I_PROT3W:
                case EE_I_PABSH: case EE_I_PABSW:
                case EE_I_PEXT5: case EE_I_PPAC5: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrt = ee_get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrd = uc.new_vec128();

                    switch (i.id) {
                        case EE_I_PCPYH:  ee_mmi::pcpyh(uc, vrd, vrt); break;
                        case EE_I_PEXEH:  ee_mmi::pexeh(uc, vrd, vrt); break;
                        case EE_I_PREVH:  ee_mmi::prevh(uc, vrd, vrt); break;
                        case EE_I_PEXCH:  ee_mmi::pexch(uc, vrd, vrt); break;
                        case EE_I_PEXEW:  ee_mmi::pexew(uc, vrd, vrt); break;
                        case EE_I_PEXCW:  ee_mmi::pexcw(uc, vrd, vrt); break;
                        case EE_I_PROT3W: ee_mmi::prot3w(uc, vrd, vrt); break;
                        case EE_I_PABSH:  ee_mmi::pabsh(uc, vrd, vrt); break;
                        case EE_I_PABSW:  ee_mmi::pabsw(uc, vrd, vrt); break;
                        case EE_I_PEXT5:  ee_mmi::pext5(uc, vrd, vrt); break;
                        case EE_I_PPAC5:  ee_mmi::ppac5(uc, vrd, vrt); break;
                    }

                    ee_set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case EE_I_PSLLH:
                case EE_I_PSLLW:
                case EE_I_PSRLH:
                case EE_I_PSRLW:
                case EE_I_PSRAH:
                case EE_I_PSRAW: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrt = ee_get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrd = uc.new_vec128();

                    switch (i.id) {
                        case EE_I_PSLLH: ee_mmi::psllh(uc, vrd, vrt, i.sa); break;
                        case EE_I_PSLLW: ee_mmi::psllw(uc, vrd, vrt, i.sa); break;
                        case EE_I_PSRLH: ee_mmi::psrlh(uc, vrd, vrt, i.sa); break;
                        case EE_I_PSRLW: ee_mmi::psrlw(uc, vrd, vrt, i.sa); break;
                        case EE_I_PSRAH: ee_mmi::psrah(uc, vrd, vrt, i.sa); break;
                        case EE_I_PSRAW: ee_mmi::psraw(uc, vrd, vrt, i.sa); break;
                    }

                    ee_set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case EE_I_PMFHI: ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmfhi, EE_MMI_WR_RD, false); break;
                case EE_I_PMFLO: ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmflo, EE_MMI_WR_RD, false); break;
                case EE_I_PMTHI: ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmthi, EE_MMI_WR_HI, true); break;
                case EE_I_PMTLO: ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmtlo, EE_MMI_WR_LO, true); break;
                case EE_I_PMTHL: ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmthl, EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PMULTW:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmultw,  EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PMULTUW: ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmultuw, EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PMADDW:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmaddw,  EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PMADDUW: ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmadduw, EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PMULTH:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmulth,  EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PMADDH:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmaddh,  EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PMSUBW:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmsubw,  EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PMSUBH:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmsubh,  EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PHMADH:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::phmadh,  EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;
                case EE_I_PHMSBH:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::phmsbh,  EE_MMI_WR_RD | EE_MMI_WR_HI | EE_MMI_WR_LO, true); break;

                case EE_I_MOVZ:
                case EE_I_MOVN: {
                    if (!i.rd.r) continue;

                    uint64_t crt;

                    if (ee_reg_is_const(ee, i.rt.r, &crt)) {
                        bool moves = (i.id == EE_I_MOVZ) ? (crt == 0) : (crt != 0);

                        if (!moves) continue;

                        uint64_t crs;

                        if (ee_reg_is_const(ee, i.rs.r, &crs)) {
                            ee_set_const(ee, &uc, i.rd.r, crs);

                            continue;
                        }

                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, i.rs.r == i.rd.r);
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                        uc.mov(rd.reg, rs.reg);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::CondCode cond_code = i.id == EE_I_MOVZ ? ujit::CondCode::kZero : ujit::CondCode::kNotZero;
                    ujit::UniCondition cond(ujit::UniOpCond::kTest, cond_code, rt.reg, rt.reg);

                    uc.cmov(rd.reg, rs.reg, cond);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_ADD:
                case EE_I_ADDU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)cs + (uint32_t)ct));

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (ee_one_const(ee, i.rs.r, i.rt.r, &kc, &ko)) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, ko);

                        uc.add(rd.reg, rs.reg, Imm((int32_t)(uint32_t)kc));

                        ee_sext32(uc, rd.reg);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);

                    if (!i.rs.r || !i.rt.r) {
                        ee_cached_reg& src = ee_get_reg(ee, &uc, i.rs.r ? i.rs.r : i.rt.r);

                        uc.mov(rd.reg, src.reg);
                    } else {
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                        ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                        uc.add(rd.reg, rs.reg, rt.reg);
                    }

                    ee_sext32(uc, rd.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_DADD:
                case EE_I_DADDU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, cs + ct);

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (ee_one_const(ee, i.rs.r, i.rt.r, &kc, &ko) && ee_fits_imm32(kc)) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, ko);

                        uc.add(rd.reg, rs.reg, Imm((int64_t)kc));

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);

                    if (!i.rs.r || !i.rt.r) {
                        ee_cached_reg& src = ee_get_reg(ee, &uc, i.rs.r ? i.rs.r : i.rt.r);

                        uc.mov(rd.reg, src.reg);
                    } else {
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                        ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                        uc.add(rd.reg, rs.reg, rt.reg);
                    }

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_DSUB:
                case EE_I_DSUBU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &ct)) {
                        ee_set_const(ee, &uc, i.rd.r, cs - ct);

                        continue;
                    }

                    uint64_t kt;

                    if (!ee_reg_is_const(ee, i.rs.r, &cs) && ee_reg_is_const(ee, i.rt.r, &kt) && ee_fits_imm32((uint64_t)(0 - kt))) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, i.rs.r == i.rd.r);
                        ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                        uc.add(rd.reg, rs.reg, Imm((int64_t)(uint64_t)(0 - kt)));

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, sync);
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.sub(rd.reg, rs.reg, rt.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case EE_I_MFHI:
                case EE_I_MFHI1:
                case EE_I_MFLO:
                case EE_I_MFLO1: {
                    if (!i.rd.r) continue;

                    bool is_hi = i.id == EE_I_MFHI || i.id == EE_I_MFHI1;
                    bool is_p1 = i.id == EE_I_MFHI1 || i.id == EE_I_MFLO1;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, false);

                    uc.load_u64(rd.reg, is_hi ? (is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0])) : (is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0])));
                } break;

                case EE_I_MTHI:
                case EE_I_MTHI1:
                case EE_I_MTLO:
                case EE_I_MTLO1: {
                    bool is_hi = i.id == EE_I_MTHI || i.id == EE_I_MTHI1;
                    bool is_p1 = i.id == EE_I_MTHI1 || i.id == EE_I_MTLO1;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);

                    uc.store_u64(is_hi ? (is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]))
                                    : (is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0])), rs.reg);
                } break;

                case EE_I_MFSA: {
                    if (!i.rd.r) continue;

                    ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, false);

                    uc.load_u32(rd.reg, EE(sa));
                    uc.and_(rd.reg, rd.reg, Imm(0xf));
                } break;

                case EE_I_MTSA:
                case EE_I_MTSAB:
                case EE_I_MTSAH: {
                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ujit::Gp t = uc.new_gp32();

                    if (i.id == EE_I_MTSA) {
                        uc.and_(t, rs.reg.r32(), Imm(0xf));
                    } else if (i.id == EE_I_MTSAB) {
                        uc.xor_(t, rs.reg.r32(), Imm((uint32_t)i.i16));
                        uc.and_(t, t, Imm(15));
                    } else {
                        uc.xor_(t, rs.reg.r32(), Imm((uint32_t)i.i16));
                        uc.and_(t, t, Imm(7));
                        uc.shl(t, t, Imm(1));
                    }

                    uc.store_u32(EE(sa), t);
                } break;

                case EE_I_MULT:
                case EE_I_MULT1: {
                    bool is_p1 = i.id == EE_I_MULT1;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp a = uc.new_gp64();
                    ujit::Gp b = uc.new_gp64();
                    ujit::Gp prod = uc.new_gp64();
                    ujit::Gp lo = uc.new_gp64();
                    ujit::Gp hi = uc.new_gp64();

                    ee_sext32(uc, a, rs.reg);
                    ee_sext32(uc, b, rt.reg);

                    uc.mul(prod, a, b);

                    ee_sext32(uc, lo, prod);
                    uc.sar(hi, prod, Imm(32));

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);

                    if (i.rd.r) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, false);

                        uc.mov(rd.reg, lo);
                    }
                } break;

                case EE_I_MULTU:
                case EE_I_MULTU1: {
                    bool is_p1 = i.id == EE_I_MULTU1;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp a = uc.new_gp64();
                    ujit::Gp b = uc.new_gp64();
                    ujit::Gp prod = uc.new_gp64();
                    ujit::Gp lo = uc.new_gp64();
                    ujit::Gp hi = uc.new_gp64();

                    uc.mov(a.r32(), rs.reg.r32());
                    uc.mov(b.r32(), rt.reg.r32());

                    uc.mul(prod, a, b);

                    ee_sext32(uc, lo, prod);
                    uc.sar(hi, prod, Imm(32));

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);

                    if (i.rd.r) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, false);

                        uc.mov(rd.reg, lo);
                    }
                } break;

                case EE_I_MADD:
                case EE_I_MADD1:
                case EE_I_MADDU:
                case EE_I_MADDU1: {
                    bool is_p1 = i.id == EE_I_MADD1 || i.id == EE_I_MADDU1;
                    bool is_u  = i.id == EE_I_MADDU || i.id == EE_I_MADDU1;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp a = uc.new_gp64();
                    ujit::Gp b = uc.new_gp64();

                    // Unsigned variants zero-extend the 32-bit operands; signed ones
                    // sign-extend. The accumulate and output are identical.
                    if (is_u) {
                        uc.mov(a.r32(), rs.reg.r32());
                        uc.mov(b.r32(), rt.reg.r32());
                    } else {
                        ee_sext32(uc, a, rs.reg);
                        ee_sext32(uc, b, rt.reg);
                    }

                    ujit::Gp prod = uc.new_gp64();
                    ujit::Gp acc = uc.new_gp64();
                    ujit::Gp hia = uc.new_gp64();
                    ujit::Gp lo = uc.new_gp64();
                    ujit::Gp hi = uc.new_gp64();

                    uc.mul(prod, a, b);

                    uc.load_u32(acc, is_p1 ? EE(lo.u32[2]) : EE(lo.u32[0]));
                    uc.load_u32(hia, is_p1 ? EE(hi.u32[2]) : EE(hi.u32[0]));
                    uc.shl(hia, hia, Imm(32));
                    uc.or_(acc, acc, hia);
                    uc.add(acc, acc, prod);

                    ee_sext32(uc, lo, acc);
                    uc.sar(hi, acc, Imm(32));

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);

                    if (i.rd.r) {
                        ee_cached_reg& rd = ee_get_reg(ee, &uc, i.rd.r, false);

                        uc.mov(rd.reg, lo);
                    }
                } break;

                case EE_I_DIV:
                case EE_I_DIV1: {
                    bool is_p1 = i.id == EE_I_DIV1;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp s = uc.new_gp32();
                    ujit::Gp t = uc.new_gp32();
                    ujit::Gp lo = uc.new_gp64();
                    ujit::Gp hi = uc.new_gp64();

                    uc.mov(s, rs.reg.r32());
                    uc.mov(t, rt.reg.r32());

                    Label l_zero = uc.new_label();
                    Label l_ovf = uc.new_label();
                    Label l_done = uc.new_label();

                    uc.j(l_zero, ujit::test_z(t));

                    // (s ^ INT_MIN) | (t + 1) == 0 iff s == INT_MIN && t == -1.
                    ujit::Gp chk = uc.new_gp32();
                    ujit::Gp c1 = uc.new_gp32();

                    uc.xor_(chk, s, Imm((int64_t)(int32_t)0x80000000));
                    uc.add(c1, t, Imm(1));
                    uc.or_(chk, chk, c1);
                    uc.j(l_ovf, ujit::test_z(chk));

                    ujit::Gp ms = uc.new_gp32();
                    ujit::Gp as = uc.new_gp32();
                    ujit::Gp mt = uc.new_gp32();
                    ujit::Gp at = uc.new_gp32();
                    ujit::Gp uq = uc.new_gp32();
                    ujit::Gp ur = uc.new_gp32();
                    ujit::Gp nq = uc.new_gp32();
                    ujit::Gp sq = uc.new_gp32();
                    ujit::Gp q = uc.new_gp32();
                    ujit::Gp nr = uc.new_gp32();
                    ujit::Gp r = uc.new_gp32();

                    uc.sar(ms, s, Imm(31));
                    uc.xor_(as, s, ms);
                    uc.sub(as, as, ms);
                    uc.sar(mt, t, Imm(31));
                    uc.xor_(at, t, mt);
                    uc.sub(at, at, mt);

                    uc.udiv(uq, as, at);
                    uc.umod(ur, as, at);

                    uc.neg(nq, uq);
                    uc.xor_(sq, s, t);
                    uc.select(q, nq, uq, ujit::scmp_lt(sq, Imm(0)));

                    uc.neg(nr, ur);
                    uc.select(r, nr, ur, ujit::scmp_lt(s, Imm(0)));

                    ee_sext32(uc, lo, q);
                    ee_sext32(uc, hi, r);

                    uc.j(l_done);

                    uc.bind(l_zero);

                    ujit::Gp zq = uc.new_gp32();
                    uc.select(zq, Imm(1), Imm(-1), ujit::scmp_lt(s, Imm(0)));

                    ee_sext32(uc, lo, zq);
                    ee_sext32(uc, hi, s);

                    uc.j(l_done);

                    uc.bind(l_ovf);
                    uc.mov(lo, Imm((int64_t)(int32_t)0x80000000));
                    uc.mov(hi, Imm(0));

                    uc.bind(l_done);

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);
                } break;

                case EE_I_DIVU:
                case EE_I_DIVU1: {
                    bool is_p1 = i.id == EE_I_DIVU1;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp s = uc.new_gp32();
                    ujit::Gp t = uc.new_gp32();

                    uc.mov(s, rs.reg.r32());
                    uc.mov(t, rt.reg.r32());

                    ujit::Gp lo = uc.new_gp64();
                    ujit::Gp hi = uc.new_gp64();

                    Label l_zero = uc.new_label();
                    Label l_done = uc.new_label();

                    uc.j(l_zero, ujit::test_z(t));

                    ujit::Gp q = uc.new_gp32();
                    ujit::Gp r = uc.new_gp32();

                    uc.udiv(q, s, t);
                    uc.umod(r, s, t);

                    ee_sext32(uc, lo, q);
                    ee_sext32(uc, hi, r);

                    uc.j(l_done);

                    uc.bind(l_zero);
                    uc.mov(lo, Imm((int64_t)-1));

                    ee_sext32(uc, hi, s);

                    uc.bind(l_done);

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);
                } break;

                case EE_I_PLZCW: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrs = ee_get_vec(ee, uc, i.rs.r);
                    ujit::Vec vrd = uc.new_vec128();

                    ee_mmi::plzcw(uc, vrd, vrs);

                    ee_set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case EE_I_PSLLVW:
                case EE_I_PSRLVW:
                case EE_I_PSRAVW: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrs = ee_get_vec(ee, uc, i.rs.r);
                    ujit::Vec vrt = ee_get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrd = uc.new_vec128();

                    if (i.id == EE_I_PSLLVW) {
                        ee_mmi::psllvw(uc, vrd, vrs, vrt);
                    } else if (i.id == EE_I_PSRLVW) {
                        ee_mmi::psrlvw(uc, vrd, vrs, vrt);
                    } else {
                        ee_mmi::psravw(uc, vrd, vrs, vrt);
                    }

                    ee_set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case EE_I_PMFHLLW:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmfhllw,  EE_MMI_WR_RD, false); break;
                case EE_I_PMFHLSH:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmfhlsh,  EE_MMI_WR_RD, false); break;
                case EE_I_PMFHLLH:  ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmfhllh,  EE_MMI_WR_RD, false); break;
                case EE_I_PMFHLSLW: ee_emit_mmi_wide(ee, uc, i, ee_mmi::pmfhlslw, EE_MMI_WR_RD, false); break;

                case EE_I_PMFHLUW: {
                    if (!i.rd.r) continue;

                    ujit::Gp w0 = uc.new_gp32();
                    ujit::Gp w1 = uc.new_gp32();
                    ujit::Gp w2 = uc.new_gp32();
                    ujit::Gp w3 = uc.new_gp32();

                    uc.load_u32(w0, EE(lo.u32[1]));
                    uc.load_u32(w1, EE(hi.u32[1]));
                    uc.load_u32(w2, EE(lo.u32[3]));
                    uc.load_u32(w3, EE(hi.u32[3]));

                    ujit::Vec v = uc.new_vec128();

                    uc.s_mov_u32(v, w0);
                    uc.s_insert_u32(v, w1, 1);
                    uc.s_insert_u32(v, w2, 2);
                    uc.s_insert_u32(v, w3, 3);

                    ee_set_vec(ee, uc, i.rd.r, v);
                } break;

                case EE_I_QFSRV: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrt = ee_get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrs = ee_get_vec(ee, uc, i.rs.r);

                    uc.v_storeu128(EE(qfsrv_buf[0]), vrt);
                    uc.v_storeu128(EE(qfsrv_buf[16]), vrs);

                    ujit::Gp off = uc.new_gp_ptr();

                    uc.load_u32(off.r32(), EE(sa));
                    uc.and_(off.r32(), off.r32(), Imm(15));

                    ujit::Gp base = uc.new_gp_ptr();

                    uc.lea(base, EE(qfsrv_buf[0]));

                    ujit::Vec out = uc.new_vec128();

                    uc.v_loadu128(out, ujit::mem_ptr(base, off, 0));

                    ee_set_vec(ee, uc, i.rd.r, out);
                } break;

                case EE_I_CFC1: {
                    if (!i.rt.r) continue;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, false);

                    if (EE_D_FS >= 16) {
                        uc.load_u32(rt.reg, EE(fcr));

                        ee_sext32(uc, rt.reg);
                    } else {
                        uc.mov(rt.reg, Imm((int64_t)0x2e30));
                    }
                } break;

                case EE_I_CTC1: {
                    if (EE_D_FS < 16) continue;

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    uc.store_u32(EE(fcr), rt.reg);
                } break;

                case EE_I_CFC2: {
                    if (!i.rt.r) continue;

                    ujit::Gp vu = uc.new_gp_ptr();

                    uc.load_u64(vu, EE(vu0));

                    InvokeNode* inv;

                    bc.invoke(
                        Out(inv),
                        (uintptr_t)ps2_vu_read_vi,
                        FuncSignature::build<uint32_t, void*, int32_t>()
                    );

                    inv->set_arg(0, vu);
                    inv->set_arg(1, Imm(i.rd.r));

                    ujit::Gp res = uc.new_gp32();
                    inv->set_ret(0, res);

                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, false);
                    ee_sext32(uc, rt.reg, res);

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case EE_I_CTC2: {
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r);

                    ujit::Gp val = uc.new_gp32();
                    ujit::Gp vu = uc.new_gp_ptr();

                    uc.mov(val, rt.reg.r32());
                    uc.load_u64(vu, EE(vu0));

                    InvokeNode* inv;

                    bc.invoke(
                        Out(inv),
                        (uintptr_t)ps2_vu_write_vi,
                        FuncSignature::build<void, void*, int32_t, uint32_t>()
                    );

                    inv->set_arg(0, vu);
                    inv->set_arg(1, Imm(i.rd.r));
                    inv->set_arg(2, val);

                    if (i.opcode & 1) {
                        InvokeNode* inv_lock;

                        bc.invoke(
                            Out(inv_lock),
                            (uintptr_t)vu_is_interlocked,
                            FuncSignature::build<int32_t, void*>()
                        );

                        inv_lock->set_arg(0, vu);

                        ujit::Gp locked = uc.new_gp32();

                        inv_lock->set_ret(0, locked);

                        Label l_skip = uc.new_label();

                        uc.j(l_skip, ujit::test_z(locked));

                        InvokeNode* inv_exec;

                        bc.invoke(
                            Out(inv_exec),
                            (uintptr_t)vu_execute_program_tpc,
                            FuncSignature::build<void, void*>()
                        );

                        inv_exec->set_arg(0, vu);

                        uc.bind(l_skip);
                    }
                } break;

                case EE_I_ERET: {
                    ujit::Gp st = uc.new_gp32();
                    ujit::Gp t = uc.new_gp32();

                    uc.load_u32(st, EE(status));
                    uc.and_(t, st, Imm(EE_SR_ERL));

                    ujit::Gp addr = uc.new_gp32();
                    ujit::Gp nst = uc.new_gp32();

                    Label l_erl = uc.new_label();
                    Label l_done = uc.new_label();

                    uc.j(l_erl, ujit::test_nz(t));

                    uc.load_u32(addr, EE(epc));
                    uc.and_(nst, st, Imm(~(uint32_t)EE_SR_EXL));
                    uc.j(l_done);

                    uc.bind(l_erl);
                    uc.load_u32(addr, EE(errorepc));
                    uc.and_(nst, st, Imm(~(uint32_t)EE_SR_ERL));

                    uc.bind(l_done);

                    Label l_store = uc.new_label();
                    Label l_skip  = uc.new_label();

                    ujit::Gp fs = uc.new_gp32();

                    uc.load_u32(fs, EE(fmv_skip));
                    uc.j(l_store, ujit::test_z(fs));

                    InvokeNode* eret_inv;

                    bc.invoke(
                        Out(eret_inv),
                        (uintptr_t)ee_skip_fmv,
                        FuncSignature::build<int, ee_state*, uint32_t>()
                    );

                    eret_inv->set_arg(0, ee->ee_ptr);
                    eret_inv->set_arg(1, addr);
                    eret_inv->set_ret(0, fs);

                    uc.j(l_skip, ujit::test_nz(fs));

                    uc.bind(l_store);
                    uc.store_u32(EE(next_pc), addr);
                    uc.bind(l_skip);

                    uc.store_u32(EE(status), nst);
                } break;

                case EE_I_EI:
                case EE_I_DI: {
                    ujit::Gp st = uc.new_gp32();
                    ujit::Gp t = uc.new_gp32();
                    ujit::Gp k = uc.new_gp32();

                    uc.load_u32(st, EE(status));
                    uc.and_(t, st, Imm(EE_SR_EDI | EE_SR_EXL | EE_SR_ERL));
                    uc.and_(k, st, Imm(EE_SR_KSU));

                    Label l_do = uc.new_label();
                    Label l_skip = uc.new_label();

                    uc.j(l_do, ujit::test_nz(t));
                    uc.j(l_skip, ujit::test_nz(k));

                    uc.bind(l_do);

                    ujit::Gp nv = uc.new_gp32();

                    if (i.id == EE_I_EI) {
                        uc.or_(nv, st, Imm(EE_SR_EIE));
                    } else {
                        uc.and_(nv, st, Imm(~(uint32_t)EE_SR_EIE));
                    }

                    uc.store_u32(EE(status), nv);
                    uc.bind(l_skip);
                } break;

                case EE_I_LWL:
                case EE_I_LWR:
                case EE_I_SWL:
                case EE_I_SWR: {
                    bool is_load = i.id == EE_I_LWL || i.id == EE_I_LWR;

                    if (is_load && !i.rt.r) continue;

                    ee_cached_reg& rs = ee_get_reg(ee, &uc, i.rs.r);
                    ee_cached_reg& rt = ee_get_reg(ee, &uc, i.rt.r, is_load ? (i.rt.r == i.rs.r) : true);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());

                    ujit::Gp aligned = uc.new_gp32();
                    ujit::Gp shift = uc.new_gp32();
                    ujit::Gp mem = uc.new_gp32();

                    uc.and_(aligned, addr, Imm(~3));
                    uc.and_(shift, addr, Imm(3));

                    InvokeNode* rd;

                    bc.invoke(Out(rd), (uintptr_t)bus_read32, FuncSignature::build<uint64_t, ee_state*, uint32_t>());
                    rd->set_arg(0, ee->ee_ptr);
                    rd->set_arg(1, aligned);
                    rd->set_ret(0, mem);

                    if (i.id == EE_I_LWL) {
                        uc.mov(rt.reg, ee_lsw::lwl(uc, rt.reg, mem, shift));

                        ee->reg_cache[i.rt.r].constant = false;
                    } else if (i.id == EE_I_LWR) {
                        uc.mov(rt.reg, ee_lsw::lwr(uc, rt.reg, mem, shift));

                        ee->reg_cache[i.rt.r].constant = false;
                    } else {
                        ujit::Gp val = (i.id == EE_I_SWL) ? ee_lsw::swl(uc, rt.reg, mem, shift) : ee_lsw::swr(uc, rt.reg, mem, shift);
                        ujit::Gp val64 = uc.new_gp64();
                        
                        uc.mov(val64.r32(), val);

                        InvokeNode* wr;

                        bc.invoke(Out(wr), (uintptr_t)bus_write32, FuncSignature::build<void, ee_state*, uint32_t, uint64_t>());
                        wr->set_arg(0, ee->ee_ptr);
                        wr->set_arg(1, aligned);
                        wr->set_arg(2, val64);
                    }
                } break;

                default: {
                    ee_flush_reg_cache(ee, &uc);

                    InvokeNode* invoke_node;

                    bc.invoke(
                        Out(invoke_node),
                        (uintptr_t)i.func,
                        FuncSignature::build<void, ee_state*, ee_instruction&>()
                    );

                    invoke_node->set_arg(0, ee->ee_ptr);
                    invoke_node->set_arg(1, Imm((uintptr_t)&i));

                    uc.store_zero_u64(EE(r[0].u64[0]));
                    uc.store_zero_u64(EE(r[0].u64[1]));
                } break;
            }
        }

        ee_flush_reg_cache(ee, &uc);

        switch (pending) {
            case PEND_DONE: break;

            case PEND_COND: {
                ujit::Gp c = uc.new_gp32();

                uc.load_u32(c, EE(cycles_left));
                uc.j(block_exit, ujit::scmp_le(c, Imm(0)));

                ujit::Gp t = uc.new_gp32();

                uc.load_u32(t, EE(next_pc));

                uint32_t target = (sb_end_pc - 4) + (uint32_t)pending_off;

                if (sb.succ[1] >= 0) {
                    uc.j(sb_label[sb.succ[1]], ujit::cmp_eq(t, Imm(target)));
                } else {
                    Label l_nt = uc.new_label();

                    uc.j(l_nt, ujit::cmp_ne(t, Imm(target)));
                    uc.j(block_exit);
                    uc.bind(l_nt);
                }

                uc.j(sb.succ[0] < 0 ? block_exit : sb_label[sb.succ[0]]);
            } break;

            case PEND_TAKEN: {
                emit_edge(1, pending_off);
            } break;

            case PEND_NONE: {
                if (sb.succ[0] >= 0) {
                    emit_edge(0, 0);
                } else {
                    uc.j(block_exit);
                }
            } break;
        }
    }

    uc.bind(block_exit);

    ee_flush_reg_cache(ee, &uc);

    uc.ret();

    uc.end_func();

    Error err = uc.finalize();

    if (err != Error::kOk) {
        printf("ee: Failed to finalize JIT compilation %d\n", err);

        exit(1);
    }

    Error err1 = ee->rt.add(&block->func, &code);

    if (err1 != Error::kOk) {
        printf("ee: Failed to add JIT code to runtime %d\n", err1);

        exit(1);
    }

    // if (code.logger()) {
    //     char buf[128];

    //     ee_dis_state ds;
    //     ds.print_opcode = true;
    //     ds.print_address = true;
    //     ds.pc = block->start_pc;

    //     printf("\n <------- Guest disassembly for block at PC=0x%08x:\n", block->start_pc);

    //     for (const auto& i : block->instructions) {
    //         puts(ee_disassemble(buf, i.opcode, &ds));

    //         ds.pc += 4;
    //     }
    // }
}

static inline bool ee_is_irq_pending(struct ee_state* ee) {
    int irq_enabled = (ee->status & EE_SR_IE) && (ee->status & EE_SR_EIE) &&
        (!(ee->status & EE_SR_EXL)) && (!(ee->status & EE_SR_ERL));
    int int0_pending = (ee->status & EE_SR_IM2) && (ee->cause & EE_CAUSE_IP2);
    int int1_pending = (ee->status & EE_SR_IM3) && (ee->cause & EE_CAUSE_IP3);

    return irq_enabled && (int0_pending || int1_pending);
}

static inline int _ee_run_block(struct ee_state* ee, int budget, int compile_hint) {
    int total = 0;

    while (true) {
        struct ee_block* block = ee_find_block(ee, ee->pc);

        if (!block) {
            ee->cache_misses++;

            block = ee_cache_block(ee, compile_hint);

            ee_compile_block(ee, block);
        } else {
            ee->cache_hits++;
        }

        ee->block_pc = ee->pc;
        ee->pc = block->end_pc - 4;
        ee->next_pc = block->end_pc;

        int given = budget - total;

        if (given < 1) given = 1;

        ee->cycles_left = given;
        ee->exit_req = 0;

        block->func(ee);

        int cycles = given - ee->cycles_left;

        if (cycles < 1) cycles = 1;

        ee->count += cycles;
        ee->total_cycles += cycles;
        ee->pc = ee->next_pc;

        total += cycles;

        if (ee->pending_purge) {
            printf("ee: Purging cache\n");

            ee_purge_cache(ee);

            ee->pending_purge = false;

            break;
        }

        if (total >= budget)
            break;

        if (ee->pc == 0x81fc0 || ee->intc_reads >= 10000 || ee->csr_reads >= 10000)
            break;

        if (ee_is_irq_pending(ee))
            break;
    }

    return total;
}

int ee_run_block(struct ee_state* ee, int max_cycles) {
    if (ee_is_irq_pending(ee)) {
        int cycles = _ee_run_block(ee, 1, 4);

        ee_exception_level1(ee, CAUSE_EXC1_INT);

        return cycles;
    }

    if (ee->pc == 0x81fc0 || ee->intc_reads >= 10000 || ee->csr_reads >= 10000) {
        ee->total_cycles += 16*64;
        ee->count += 16*64;
        // ee->eenull_counter += 8 * 64;

        ee->idle_skips++;

        return 16*64;
    }

    // max_cycles is the remaining timeslice budget; chain blocks up to it.
    return _ee_run_block(ee, max_cycles, EE_BLOCK_MAX_INSTRS);
}

int ee_step(struct ee_state* ee) {
    static ee_instruction i;

    ee->delay_slot = ee->branch;
    ee->branch = 0;

    // Would check for interrupts here, but we do this outside of the core
    // to reduce overhead
    ee_check_irq(ee);

    ee->prev_pc = ee->pc;
    ee->opcode = bus_read32(ee, ee->pc);
    ee->pc = ee->next_pc;
    ee->next_pc += 4;

    i = ee_decode(ee->opcode);

    i.func(ee, i);

    ++ee->total_cycles;
    ++ee->count;

    ee->r[0].u64[0] = 0;
    ee->r[0].u64[1] = 0;

    return 1;
}

void ee_flush_cache(struct ee_state* ee) {
    ee_vfast_clear(ee);

    if (ee->block_cache.empty())
        return;

    for (int i = 0; i < EE_CACHE_PAGECOUNT; i++) {
        ee->block_cache[i].dirty = true;
    }

    ee->last_block_lookup_pc = ~0u;
    ee->last_block_ptr = nullptr;
    ee->block_lut_gen++;
}

uint32_t ee_get_pc(struct ee_state* ee) {
    return ee->pc;
}

struct ps2_ram* ee_get_spr(struct ee_state* ee) {
    return ee->spr;
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

void ee_set_ram_size(struct ee_state* ee, int ram_size) {
    ee->ram_size = ram_size - 1;
}

void ee_set_osd_config(struct ee_state* ee, struct ee_osd_config config) {
    ee->osd_config = config;
}

struct ee_osd_config ee_get_osd_config(struct ee_state* ee) {
    return ee->osd_config;
}

void ee_invalidate_block(struct ee_state* ee, uint32_t addr) {
    uint32_t page = addr / EE_MIN_PAGESIZE;

    // if (ee->block_cache[page].valid && !ee->block_cache[page].dirty) {
    //     printf("ee: Invalidating block at address 0x%08x\n", addr);
    // }

    ee->block_cache[page].dirty = true;
    ee->block_lut_gen++;
}

void ee_invalidate_range(struct ee_state* ee, uint32_t addr, uint32_t size) {
    for (uint32_t i = 0; i < size; i += EE_MIN_PAGESIZE) {
        ee_invalidate_block(ee, addr + i);
    }
}