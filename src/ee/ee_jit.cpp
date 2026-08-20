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

#include "ee.hpp"
#include "bus.hpp"
#include "vu.hpp"
#include "ee_dis.hpp"
#include "ee_def.hpp"
#include "ee_mmi.hpp"
#include "ee_fpu.hpp"
#include "ee_lsw.hpp"

#include "../jit_invoke.hpp"

namespace iris::ee {

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
#define VU_LOWER(ins) { vu::decode_lower(ee->vu0, i.opcode); vu::i_ ## ins(ee->vu0, &ee->vu0->lower); }
#define VU_UPPER(ins) { vu::decode_upper(ee->vu0, i.opcode); vu::i_ ## ins(ee->vu0, &ee->vu0->upper); }
#define VU_LOWER_TEMPLATE(ins) { \
    vu::decode_lower(ee->vu0, i.opcode); \
    switch ((i.opcode >> 21) & 0xf) { \
        case 0: vu::i_ ## ins <0>(ee->vu0, &ee->vu0->lower); break; \
        case 1: vu::i_ ## ins <vu::D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 2: vu::i_ ## ins <vu::D_Z>(ee->vu0, &ee->vu0->lower); break; \
        case 3: vu::i_ ## ins <vu::D_Z | vu::D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 4: vu::i_ ## ins <vu::D_Y>(ee->vu0, &ee->vu0->lower); break; \
        case 5: vu::i_ ## ins <vu::D_Y | vu::D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 6: vu::i_ ## ins <vu::D_Y | vu::D_Z>(ee->vu0, &ee->vu0->lower); break; \
        case 7: vu::i_ ## ins <vu::D_Y | vu::D_Z | vu::D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 8: vu::i_ ## ins <vu::D_X>(ee->vu0, &ee->vu0->lower); break; \
        case 9: vu::i_ ## ins <vu::D_X | vu::D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 10: vu::i_ ## ins <vu::D_X | vu::D_Z>(ee->vu0, &ee->vu0->lower); break; \
        case 11: vu::i_ ## ins <vu::D_X | vu::D_Z | vu::D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 12: vu::i_ ## ins <vu::D_X | vu::D_Y>(ee->vu0, &ee->vu0->lower); break; \
        case 13: vu::i_ ## ins <vu::D_X | vu::D_Y | vu::D_W>(ee->vu0, &ee->vu0->lower); break; \
        case 14: vu::i_ ## ins <vu::D_X | vu::D_Y | vu::D_Z>(ee->vu0, &ee->vu0->lower); break; \
        case 15: vu::i_ ## ins <vu::D_X | vu::D_Y | vu::D_Z | vu::D_W>(ee->vu0, &ee->vu0->lower); break; \
    } }
#define VU_UPPER_TEMPLATE(ins) { \
    vu::decode_upper(ee->vu0, i.opcode); \
    switch ((i.opcode >> 21) & 0xf) { \
        case 0: vu::i_ ## ins <0>(ee->vu0, &ee->vu0->upper); break; \
        case 1: vu::i_ ## ins <vu::D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 2: vu::i_ ## ins <vu::D_Z>(ee->vu0, &ee->vu0->upper); break; \
        case 3: vu::i_ ## ins <vu::D_Z | vu::D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 4: vu::i_ ## ins <vu::D_Y>(ee->vu0, &ee->vu0->upper); break; \
        case 5: vu::i_ ## ins <vu::D_Y | vu::D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 6: vu::i_ ## ins <vu::D_Y | vu::D_Z>(ee->vu0, &ee->vu0->upper); break; \
        case 7: vu::i_ ## ins <vu::D_Y | vu::D_Z | vu::D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 8: vu::i_ ## ins <vu::D_X>(ee->vu0, &ee->vu0->upper); break; \
        case 9: vu::i_ ## ins <vu::D_X | vu::D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 10: vu::i_ ## ins <vu::D_X | vu::D_Z>(ee->vu0, &ee->vu0->upper); break; \
        case 11: vu::i_ ## ins <vu::D_X | vu::D_Z | vu::D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 12: vu::i_ ## ins <vu::D_X | vu::D_Y>(ee->vu0, &ee->vu0->upper); break; \
        case 13: vu::i_ ## ins <vu::D_X | vu::D_Y | vu::D_W>(ee->vu0, &ee->vu0->upper); break; \
        case 14: vu::i_ ## ins <vu::D_X | vu::D_Y | vu::D_Z>(ee->vu0, &ee->vu0->upper); break; \
        case 15: vu::i_ ## ins <vu::D_X | vu::D_Y | vu::D_Z | vu::D_W>(ee->vu0, &ee->vu0->upper); break; \
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

#define KUSEG 0
#define KSEG0 1
#define KSEG1 2
#define KSSEG 3
#define KSEG3 4

/*
    i.rs = (opcode >> 21) & 0x1f;
    i.rt = (opcode >> 16) & 0x1f;
    i.rd = (opcode >> 11) & 0x1f;
    i.fd = (opcode >> 6) & 0x1f;
    i.i15 = (opcode >> 6) & 0x7fff;
    i.i16 = opcode & 0xffff;
    i.i26 = opcode & 0x3ffffff;
*/
#define D_RS (i.rs.r)
#define D_FS (i.rd.r)
#define D_RT (i.rt.r)
#define D_RD (i.rd.r)
#define D_FD (i.sa)
#define D_SA (i.sa)
#define D_I15 (i.i15)
#define D_I16 (i.i16)
#define D_I26 (i.i26)
#define D_SI26 ((int32_t)(D_I26 << 6) >> 4)
#define D_SI16 ((int32_t)(D_I16 << 16) >> 14)

#define RT ee->r[D_RT].ul64
#define RD ee->r[D_RD].ul64
#define RS ee->r[D_RS].ul64
#define RT32 ee->r[D_RT].ul32
#define RD32 ee->r[D_RD].ul32
#define RS32 ee->r[D_RS].ul32
#define FD ee->f[D_FD].f
#define FT (fpu_cvtf(ee->f[D_RT].f))
#define FS (fpu_cvtf(ee->f[D_FS].f))
#define FT32 ee->f[D_RT].u32
#define FD32 ee->f[D_FD].u32
#define FS32 ee->f[D_FS].u32

#define HI0 ee->hi.u64[0]
#define LO0 ee->lo.u64[0]
#define HI1 ee->hi.u64[1]
#define LO1 ee->lo.u64[1]

#define BRANCH(cond, offset) \
    if (cond) { ee->next_pc = ee->pc + (offset); }

#define BRANCH_LIKELY(cond, offset) \
    BRANCH(cond, offset) else { ee->exception = 1; }

#define SE6432(v) ((int64_t)((int32_t)(v)))
#define SE6416(v) ((int64_t)((int16_t)(v)))
#define SE648(v) ((int64_t)((int8_t)(v)))
#define SE3216(v) ((int32_t)((int16_t)(v)))

static inline void print_disassembly(Ee* ee, const Instruction& i) {
    char buf[128];
    dis::Dis ds;

    ds.print_address = 1;
    ds.print_opcode = 1;
    ds.pc = ee->pc;

    puts(dis::disassemble(buf, i.opcode, &ds));
}

static inline int get_segment(uint32_t virt) {
    switch (virt & 0xe0000000) {
        case 0x00000000: return KUSEG;
        case 0x20000000: return KUSEG;
        case 0x40000000: return KUSEG;
        case 0x60000000: return KUSEG;
        case 0x80000000: return KSEG0;
        case 0xa0000000: return KSEG1;
        case 0xc0000000: return KSSEG;
        case 0xe0000000: return KSEG3;
    }

    return KUSEG;
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

static inline float fpu_cvtsw(union FpuReg* reg) {
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

static inline void fpu_cvtws(union FpuReg* d, union FpuReg* s) {
    if ((s->u32 & 0x7F800000) <= 0x4E800000)
        d->s32 = (int32_t)fpu_cvtf(s->f);
    else if ((s->u32 & 0x80000000) == 0)
        d->u32 = 0x7FFFFFFF;
    else
        d->u32 = 0x80000000;
}

static inline int fpu_check_overflow(Ee* ee, union FpuReg* reg) {
    if ((reg->u32 & ~0x80000000) == 0x7f800000) {
        reg->u32 = (reg->u32 & 0x80000000) | 0x7f7fffff;
        ee->fcr |= FPU_FLG_O | FPU_FLG_SO;

        return 1;
    }

    ee->fcr &= ~FPU_FLG_O;

    return 0;
}

static inline int fpu_check_underflow(Ee* ee, union FpuReg* reg) {
    if (((reg->u32 & 0x7F800000) == 0) && ((reg->u32 & 0x007FFFFF) != 0)) {
        reg->u32 &= 0x80000000;
        ee->fcr |= FPU_FLG_U | FPU_FLG_SU;

        return 1;
    }

    ee->fcr &= ~FPU_FLG_U;

    return 0;
}

static inline int fpu_check_overflow_no_flags(Ee* ee, union FpuReg* reg) {
    if ((reg->u32 & ~0x80000000) == 0x7f800000) {
        reg->u32 = (reg->u32 & 0x80000000) | 0x7f7fffff;

        return 1;
    }

    return 0;
}

static inline int fpu_check_underflow_no_flags(Ee* ee, union FpuReg* reg) {
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

void exception_level1(Ee* ee, uint32_t cause);

#ifdef _EE_USE_MMU
static inline VtlbEntry* search_vtlb(Ee* ee, uint32_t virt) {
    for (int i = 0; i < 48; i++) {
        VtlbEntry* e = &ee->vtlb[i];

        if (e->s) {
            uint32_t mask = 0xffffc000;

            if ((virt & mask) == (e->vpn2 & mask)) {
                return e;
            }
        }

        uint32_t mask = (~e->mask) & 0xffffe000;

        // iris_debug(ee, "TLB search index={} virt={:08x} vpn2={:08x} mask={:08x}", //     i,
        //     virt,
        //     e->vpn2,
        //     mask
        //);

        if ((virt & mask) == (e->vpn2 & mask)) {
            return e;
        }
    }

    return nullptr;
}

static inline int translate_virt(Ee* ee, uint32_t virt, uint32_t* phys, int load) {
    int seg = get_segment(virt);

    // Assume we're in kernel mode
    if (seg == KSEG0 || seg == KSEG1) {
        *phys = virt & 0x1fffffff;

        return 0;
    }

    VtlbEntry* entry = search_vtlb(ee, virt);

    if (!entry) {
        exception_level1(ee, load ? CAUSE_EXC1_TLBL : CAUSE_EXC1_TLBS);

        ee->context &= 0x7ffff0;
        ee->context |= (virt & 0xFFFFE000) >> 9;

        iris_debug(ee, "TLB miss on {} at virt={:08x}", load ? "load" : "store", virt);

        *phys = 0;

        return -1;
    }

    if (entry->s) {
        *phys = virt & 0x00003fff;

        return 1;
    }

    // iris_debug(ee, "virt={:08x} vpn2={:08x} even={pfn={:08x} v={} d={}} odd={pfn={:08x} v={} d={}} mask={:08x} s={} g={}", //     virt,
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
    //);

    uint32_t nmask = 0xfffff000 & ~(entry->mask >> 1);
    uint32_t pfn = entry->pfn0;

    int odd = (virt & ((entry->mask >> 1) + 0x1000)) ? 1 : 0;

    if (odd) {
        pfn = entry->pfn1;
    }

    *phys = pfn | (virt & ~nmask);

    // iris_debug(ee, "Translated virt={:08x} to phys={:08x}", virt, *phys);

    // if (odd) exit(1);
    return 0;
}

#define BUS_READ_FUNC(b)                                                        \
    static inline uint64_t bus_read ## b(Ee* ee, uint32_t addr) {  \
        uint32_t phys;                                                          \
        if (translate_virt(ee, addr, &phys, 1) == 1)                         \
            return ram::read ## b(ee->spr, phys);                            \
        if (phys == 0x1000f000) ee->intc_reads++;                               \
        if (phys == 0x12001000) ee->csr_reads++;                                \
        return ee->bus.read ## b(ee->bus.udata, phys);                          \
    }

#define BUS_WRITE_FUNC(b)                                                                   \
    static inline void bus_write ## b(Ee* ee, uint32_t addr, uint64_t data) {  \
        uint32_t phys;                                                                      \
        if (translate_virt(ee, addr, &phys, 0) == 1)                                     \
            { ram::write ## b(ee->spr, phys, data); return; }                            \
        ee->bus.write ## b(ee->bus.udata, phys, data);                                      \
    }

BUS_READ_FUNC(8)
BUS_READ_FUNC(16)
BUS_READ_FUNC(32)
BUS_READ_FUNC(64)

static inline uint128_t bus_read128(Ee* ee, uint32_t addr) {
    uint32_t phys;

    if (translate_virt(ee, addr, &phys, 1) == 1)
        return ram::read128(ee->spr, phys);

    return ee->bus.read128(ee->bus.udata, phys);
}

BUS_WRITE_FUNC(8)
BUS_WRITE_FUNC(16)
BUS_WRITE_FUNC(32)
BUS_WRITE_FUNC(64)

static inline void bus_write128(Ee* ee, uint32_t addr, uint128_t data) {
    uint32_t phys;

    if (translate_virt(ee, addr, &phys, 0) == 1)
        { ram::write128(ee->spr, phys, data); return; }

    ee->bus.write128(ee->bus.udata, phys, data);
}
#else
static inline int translate_virt(Ee* ee, uint32_t virt, uint32_t* phys) {
    int seg = get_segment(virt);

    // Assume we're in kernel mode
    if (seg == KSEG0 || seg == KSEG1) {
        *phys = virt & 0x1fffffff;

        return 0;
    }

    Page* page = &ee->pagetable[virt / MIN_PAGESIZE];

    if (!page->valid) {
        iris_debug(ee, "Segmentation fault at 0x{:08x}", virt);

        *phys = 0;

        return -1;
    }

    *phys = (page->pfn * MIN_PAGESIZE) | (virt & (MIN_PAGESIZE - 1));

    // iris_debug(ee, "Translated virt={:08x} to phys={:08x}", virt, *phys);

    return 0;
}

#define CACHE_PAGECOUNT (0x20000000 / MIN_PAGESIZE)

void vfast_clear(Ee* ee);

void purge_cache(Ee* ee) {
    vfast_clear(ee);

    for (int i = 0; i < CACHE_PAGECOUNT; i++) {
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

static inline bool is_executable_region(uint32_t addr) {
    // EE should only ever execute from RAM (and its mirrors), and the BIOS
    return addr < 0x8000000 || (addr >= 0x1fc00000 && addr < 0x20000000);
}

static inline void invalidate_page(Ee* ee, uint32_t addr) {
    if (!is_executable_region(addr))
        return;

    uint32_t page = addr / MIN_PAGESIZE;

    if (ee->block_cache[page].dirty || !ee->block_cache[page].valid)
        return;

    if (addr < ee->block_cache[page].min_code_addr || addr >= ee->block_cache[page].max_code_addr)
        return;

    // iris_debug(ee, "Invalidating page at addr={:08x} page={}", addr, page);

    ee->block_cache[page].dirty = true;
    ee->block_lut_gen++;
}

#define INVALIDATE_CACHE_PAGE(addr) { \
    if (is_executable_region(addr)) { \
        uint32_t page = (addr) / MIN_PAGESIZE; \
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
    static inline uint64_t bus_read ## b(Ee* ee, uint32_t addr) { \
        if ((addr & 0xf0000000) == 0x70000000)                                 \
            return ram::read ## b(ee->spr, addr & 0x3fff);                  \
        uint32_t phys;                                                         \
        translate_virt(ee, addr, &phys);                                    \
        if (phys == 0x1000f000) ee->intc_reads++;                              \
        if (phys == 0x12001000) ee->csr_reads++;                               \
        return ee->bus.read ## b(ee->bus.udata, phys);                         \
    }

#define BUS_WRITE_FUNC(b)                                                                  \
    static inline void bus_write ## b(Ee* ee, uint32_t addr, uint64_t data) { \
        if ((addr & 0xf0000000) == 0x70000000)                                             \
        { ram::write ## b(ee->spr, addr & 0x3fff, data); return; }                      \
        uint32_t phys;                                                                     \
        translate_virt(ee, addr, &phys);                                                \
        invalidate_page(ee, phys);                                                      \
        ee->bus.write ## b(ee->bus.udata, phys, data);                                     \
    }

BUS_READ_FUNC(8)
BUS_READ_FUNC(16)
BUS_READ_FUNC(32)
BUS_READ_FUNC(64)

static inline uint128_t bus_read128(Ee* ee, uint32_t addr) {
    if ((addr & 0xf0000000) == 0x70000000)
        return ram::read128(ee->spr, addr & 0x3ff0);

    uint32_t phys;

    translate_virt(ee, addr, &phys);

    return ee->bus.read128(ee->bus.udata, phys);
}

// static inline __m128i bus_read128_sse(Ee* ee, uint32_t addr) {
//     uint128_t result = bus_read128(ee, addr);

//     return _mm_load_si128((__m128i*)&result);
// }

BUS_WRITE_FUNC(8)
BUS_WRITE_FUNC(16)
BUS_WRITE_FUNC(32)
BUS_WRITE_FUNC(64)

void bus_write128(Ee* ee, uint32_t addr, uint128_t data) {
    if ((addr & 0xf0000000) == 0x70000000) {
        ram::write128(ee->spr, addr & 0x3ff0, data);

        return;
    }

    uint32_t phys;

    translate_virt(ee, addr, &phys);
    invalidate_page(ee, phys);

    ee->bus.write128(ee->bus.udata, phys, data);
}
#endif

#undef BUS_READ_FUNC
#undef BUS_WRITE_FUNC

static void jit_read128(Ee* ee, uint32_t addr, uint128_t* out) {
    *out = bus_read128(ee, addr);
}

static void jit_write128(Ee* ee, uint32_t addr, const uint128_t* in) {
    bus_write128(ee, addr, *in);
}

#define FOLD_NO_PHYS 0xffffffffu
#define VFAST_SPR_PAGE 0xffffffffu

static inline void* vfast_page_base(Ee* ee, uint32_t vaddr, int write, uint32_t* out_phys_page) {
    if ((vaddr & 0xf0000000) == 0x70000000) {
        *out_phys_page = VFAST_SPR_PAGE;

        return ee->spr->buf + ((vaddr & 0x3fff) & ~0xfff);
    }

    uint32_t phys;
    int seg = get_segment(vaddr);

    if (seg == KSEG0 || seg == KSEG1) {
        phys = vaddr & 0x1fffffff;
    } else {
        Page* page = &ee->pagetable[vaddr / MIN_PAGESIZE];

        if (!page->valid)
            return nullptr;

        phys = (page->pfn * MIN_PAGESIZE) | (vaddr & (MIN_PAGESIZE - 1));
    }

    if (phys >= 0x20000000)
        return nullptr;

    ee::bus::Bus* bus = (ee::bus::Bus*)ee->bus.udata;

    if (!bus)
        return nullptr;

    void* ptr = write ? bus->fastmem_w_table[phys >> 13] : bus->fastmem_r_table[phys >> 13];

    if (!ptr)
        return nullptr;

    *out_phys_page = phys >> 12;

    return (uint8_t*)ptr + (phys & 0x1000);
}

static void* fold_host_ptr(Ee* ee, uint32_t vaddr, int bytes, bool write, uint32_t* out_phys) {
    if (((vaddr & 0xfff) + bytes) > 0x1000)
        return nullptr;

    uint32_t pp;

    void* base = vfast_page_base(ee, vaddr, write, &pp);

    if (!base)
        return nullptr;

    *out_phys = (pp == VFAST_SPR_PAGE) ? FOLD_NO_PHYS : ((pp << 12) | (vaddr & 0xfff));

    return (uint8_t*)base + (vaddr & 0xfff);
}

#define VFAST_ENTRIES 0x100000

void vfast_clear(Ee* ee) {
    if (ee->vfast_r) {
        memset(ee->vfast_r, 0, VFAST_ENTRIES * sizeof(void*));
    }
}

#define VFAST_READ_FUNC(b)                                                     \
    static uint64_t vfast_read ## b(Ee* ee, uint32_t addr) {      \
        uint32_t pp;                                                              \
        void* base = vfast_page_base(ee, addr, 0, &pp);                        \
        if (base) ee->vfast_r[addr >> 12] = base;                                 \
        return bus_read ## b(ee, addr);                                           \
    }

VFAST_READ_FUNC(8)
VFAST_READ_FUNC(16)
VFAST_READ_FUNC(32)
VFAST_READ_FUNC(64)

#undef VFAST_READ_FUNC

static inline asmjit::ujit::Gp fold_base(asmjit::ujit::UniCompiler& uc, void* host) {
    asmjit::ujit::Gp base = uc.new_gp_ptr();

    uc.mov(base, asmjit::Imm((uint64_t)(uintptr_t)host));

    return base;
}

static inline int skip_fmv(Ee* ee, uint32_t addr) {
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

    iris_debug(ee, "Skipping FMV");

    return 1;
}

static inline void set_pc(Ee* ee, uint32_t addr) {
    if (ee->fmv_skip) {
        if (skip_fmv(ee, addr)) return;
    }

    ee->next_pc = addr;
}

void exception_level1(Ee* ee, uint32_t cause) {
    uint32_t vec = VEC_COMMON;

    ee->exit_req = 1;

    switch (cause) {
        case CAUSE_EXC1_TLBL:
        case CAUSE_EXC1_TLBS:
            vec = VEC_TLB;
            break;
        case CAUSE_EXC1_TLBIL:
        case CAUSE_EXC1_TLBIS:
            cause <<= 2;
            break;
        case CAUSE_EXC1_INT:
            vec = VEC_IRQ;
            break;
    }

    ee->cause &= ~CAUSE_EXC;
    ee->cause |= cause;

    if (!(ee->status & SR_EXL)) {
        ee->epc = ee->pc;
        ee->cause &= ~CAUSE_BD;
    }

    ee->status |= SR_EXL;

    uint32_t addr = ((ee->status & SR_BEV) ? 0xbfc00200 : 0x80000000) + vec;

    set_pc(ee, addr);

    ee->pc = addr;

    // iris_debug(ee, "Exception level 1, cause={}, vec={:08x}, pc={:08x} next_pc={:08x}", cause, addr, ee->pc, ee->next_pc);
}

static inline void exception_level2(Ee* ee, uint32_t cause) {
    ee->exit_req = 1;   // see exception_level1

    uint32_t vec;

    ee->cause &= ~CAUSE_EXC2;
    ee->cause |= cause;

    ee->errorepc = ee->pc - 4;

    if (ee->delay_slot) {
        ee->errorepc -= 4;
        ee->cause |= CAUSE_BD2;
    } else {
        ee->cause &= ~CAUSE_BD2;
    }

    ee->status |= SR_ERL;

    if ((cause == CAUSE_EXC2_RES) | (cause == CAUSE_EXC2_NMI)) {
        set_pc(ee, VEC_RESET);

        return;
    }

    if (cause == CAUSE_EXC2_PERFC) {
        vec = VEC_COUNTER;
    } else {
        vec = VEC_DEBUG;
    }

    set_pc(ee, ((ee->status & SR_DEV) ? 0xbfc00200 : 0x80000000) + vec);
}

static inline int check_irq(Ee* ee) {
    int irq_enabled = (ee->status & SR_IE) && (ee->status & SR_EIE) &&
        (!(ee->status & SR_EXL)) && (!(ee->status & SR_ERL));
    int int0_pending = (ee->status & SR_IM2) && (ee->cause & CAUSE_IP2);
    int int1_pending = (ee->status & SR_IM3) && (ee->cause & CAUSE_IP3);

    if (irq_enabled && (int0_pending || int1_pending)) {
        // iris_debug(ee, "Handling irq at pc={:08x} (int0={} ({}) int1={} ({})) sr={:08x} delay_slot={}", //     ee->pc,
        //     int0_pending, !!(ee->status & SR_IM2),
        //     int1_pending, !!(ee->status & SR_IM3),
        //     ee->status,
        //     ee->delay_slot
        //);

        ee->intc_reads = 0;

        exception_level1(ee, CAUSE_EXC1_INT);

        return 1;
    }

    return 0;
}

void set_int0(Ee* ee, int v) {
    if (v) {
        ee->cause |= CAUSE_IP2;
        ee->exit_req = 1;   // let a running region's back edge bail out
    } else {
        ee->cause &= ~CAUSE_IP2;
    }
}

void set_int1(Ee* ee, int v) {
    if (v) {
        ee->cause |= CAUSE_IP3;
        ee->exit_req = 1;
    } else {
        ee->cause &= ~CAUSE_IP3;
    }
}

void set_cpcond0(Ee* ee, int v) {
    ee->cpcond0 = v;
}

static inline void i_abss(Ee* ee, const Instruction& i) {
    ee->f[D_FD].u32 = ee->f[D_FS].u32 & 0x7fffffff;
    // FD = fabsf(FS);
}
static inline void i_add(Ee* ee, const Instruction& i) {
    int32_t s = RS;
    int32_t t = RT;

    int32_t r = s + t;
    uint32_t o = (s ^ r) & (t ^ r);

    if (o & 0x80000000) {
        exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        RD = SE6432(r);
    }
}
static inline void i_addas(Ee* ee, const Instruction& i) {
    ee->a.f = FS + FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void i_addi(Ee* ee, const Instruction& i) {
    int32_t s = RS;
    int32_t t = SE3216(D_I16);
    int32_t r;

    if (__builtin_sadd_overflow(s, t, &r)) {
        exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        RT = SE6432(r);
    }
}
static inline void i_addiu(Ee* ee, const Instruction& i) {
    RT = SE6432(RS32 + SE3216(D_I16));
}
static inline void i_adds(Ee* ee, const Instruction& i) {
    int d = D_FD;

    ee->f[d].f = FS + FT;

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void i_addu(Ee* ee, const Instruction& i) {
    RD = SE6432(RS + RT);
}
static inline void i_and(Ee* ee, const Instruction& i) {
    RD = RS & RT;
}
static inline void i_andi(Ee* ee, const Instruction& i) {
    RT = RS & D_I16;
}
static inline void i_bc0f(Ee* ee, const Instruction& i) {
    BRANCH(!ee->cpcond0, D_SI16);
}
static inline void i_bc0fl(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY(!ee->cpcond0, D_SI16);
}
static inline void i_bc0t(Ee* ee, const Instruction& i) {
    BRANCH(ee->cpcond0, D_SI16);
}
static inline void i_bc0tl(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY(ee->cpcond0, D_SI16);
}
static inline void i_bc1f(Ee* ee, const Instruction& i) {
    BRANCH((ee->fcr & FPU_FLG_C) == 0, D_SI16);
}
static inline void i_bc1fl(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY((ee->fcr & FPU_FLG_C) == 0, D_SI16);
}
static inline void i_bc1t(Ee* ee, const Instruction& i) {
    BRANCH((ee->fcr & FPU_FLG_C) != 0, D_SI16);
}
static inline void i_bc1tl(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY((ee->fcr & FPU_FLG_C) != 0, D_SI16);
}
static inline void i_bc2f(Ee* ee, const Instruction& i) { BRANCH(1, D_SI16); }
static inline void i_bc2fl(Ee* ee, const Instruction& i) { BRANCH_LIKELY(1, D_SI16); }
static inline void i_bc2t(Ee* ee, const Instruction& i) { BRANCH(0, D_SI16); }
static inline void i_bc2tl(Ee* ee, const Instruction& i) { BRANCH_LIKELY(0, D_SI16); }
static inline void i_beq(Ee* ee, const Instruction& i) {
    BRANCH(RS == RT, D_SI16);
}
static inline void i_beql(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY(RS == RT, D_SI16);
}
static inline void i_bgez(Ee* ee, const Instruction& i) {
    BRANCH((int64_t)RS >= (int64_t)0, D_SI16);
}
static inline void i_bgezal(Ee* ee, const Instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH((int64_t)RS >= (int64_t)0, D_SI16);
}
static inline void i_bgezall(Ee* ee, const Instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH_LIKELY((int64_t)RS >= (int64_t)0, D_SI16);
}
static inline void i_bgezl(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY((int64_t)RS >= (int64_t)0, D_SI16);
}
static inline void i_bgtz(Ee* ee, const Instruction& i) {
    BRANCH((int64_t)RS > (int64_t)0, D_SI16);
}
static inline void i_bgtzl(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY((int64_t)RS > (int64_t)0, D_SI16);
}
static inline void i_blez(Ee* ee, const Instruction& i) {
    BRANCH((int64_t)RS <= (int64_t)0, D_SI16);
}
static inline void i_blezl(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY((int64_t)RS <= (int64_t)0, D_SI16);
}
static inline void i_bltz(Ee* ee, const Instruction& i) {
    BRANCH((int64_t)RS < (int64_t)0, D_SI16);
}
static inline void i_bltzal(Ee* ee, const Instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH((int64_t)RS < (int64_t)0, D_SI16);
}
static inline void i_bltzall(Ee* ee, const Instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    BRANCH_LIKELY((int64_t)RS < (int64_t)0, D_SI16);
}
static inline void i_bltzl(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY((int64_t)RS < (int64_t)0, D_SI16);
}
static inline void i_bne(Ee* ee, const Instruction& i) {
    BRANCH(RS != RT, D_SI16);
}
static inline void i_bnel(Ee* ee, const Instruction& i) {
    BRANCH_LIKELY(RS != RT, D_SI16);
}
static inline void i_break(Ee* ee, const Instruction& i) {
    exception_level1(ee, CAUSE_EXC1_BP);
}
static inline void i_cache(Ee* ee, const Instruction& i) {
    /* To-do: Cache emulation */
} 
static inline void i_ceq(Ee* ee, const Instruction& i) {
    if (FS == FT) {
        ee->fcr |= FPU_FLG_C;
    } else {
        ee->fcr &= ~FPU_FLG_C;
    }
}
static inline void i_cf(Ee* ee, const Instruction& i) {
    ee->fcr &= ~FPU_FLG_C; 
}
static inline void i_cfc1(Ee* ee, const Instruction& i) {
    RT = SE6432((D_FS >= 16) ? ee->fcr : 0x2e30);
}
static inline void i_cfc2(Ee* ee, const Instruction& i) {
    RT = SE6432(vu::read_vi(ee->vu0, D_RD));
}
static inline void i_cle(Ee* ee, const Instruction& i) {
    if (FS <= FT) {
        ee->fcr |= FPU_FLG_C;
    } else {
        ee->fcr &= ~FPU_FLG_C;
    }
}
static inline void i_clt(Ee* ee, const Instruction& i) {
    if (FS < FT) {
        ee->fcr |= FPU_FLG_C;
    } else {
        ee->fcr &= ~FPU_FLG_C;
    }
}
static inline void i_ctc1(Ee* ee, const Instruction& i) {
    if (D_FS < 16)
        return;

    ee->fcr = RT32; // (ee->fcr & ~(0x83c078)) | (RT & 0x83c078);
}
static inline void i_ctc2(Ee* ee, const Instruction& i) {
    // To-do: Handle FBRST, VPU_STAT, CMSAR1
    int d = D_RD;

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

    vu::write_vi(ee->vu0, d, RT32);

    if ((i.opcode & 1) && vu::is_interlocked(ee->vu0)) {
        vu::execute_program_tpc(ee->vu0);
    }
}
static inline void i_cvts(Ee* ee, const Instruction& i) {
    FD = (float)ee->f[D_FS].s32;
    FD = fpu_cvtsw(&ee->f[D_FD]);
}
static inline void i_cvtw(Ee* ee, const Instruction& i) {
    fpu_cvtws(&ee->f[D_FD], &ee->f[D_FS]);
}
static inline void i_dadd(Ee* ee, const Instruction& i) {
    long long r;

    if (SADDOVF64((int64_t)RS, (int64_t)RT, &r)) {
        exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        RD = r;
    }
}
static inline void i_daddi(Ee* ee, const Instruction& i) {
    long long r;

    if (SADDOVF64((int64_t)RS, SE6416(D_I16), &r)) {
        exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        RT = r;
    }
}
static inline void i_daddiu(Ee* ee, const Instruction& i) {
    RT = RS + SE6416(D_I16);
}
static inline void i_daddu(Ee* ee, const Instruction& i) {
    RD = RS + RT;
}
static inline void i_di(Ee* ee, const Instruction& i) {
    int edi = ee->status & SR_EDI;
    int exl = ee->status & SR_EXL;
    int erl = ee->status & SR_ERL;
    int ksu = ee->status & SR_KSU;
    
    if (edi || exl || erl || !ksu)
        ee->status &= ~SR_EIE;
}
static inline void i_div(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int s = D_RS;

    if (ee->r[s].ul32 == 0x80000000 && ee->r[t].ul32 == 0xffffffff) {
        LO0 = (int32_t)0x80000000;
        HI0 = 0;
    } else if (ee->r[t].ul32 != 0) {
        HI0 = SE6432(ee->r[s].sl32 % ee->r[t].sl32);
        LO0 = SE6432(ee->r[s].sl32 / ee->r[t].sl32);
    } else {
        HI0 = SE6432(ee->r[s].ul32);
        LO0 = ((int32_t)ee->r[s].ul32 < 0) ? 1 : -1;
    }
}
static inline void i_div1(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int s = D_RS;

    if (ee->r[s].ul32 == 0x80000000 && ee->r[t].ul32 == 0xffffffff) {
        LO1 = (int32_t)0x80000000;
        HI1 = 0;
    } else if (ee->r[t].ul32 != 0) {
        HI1 = SE6432(ee->r[s].sl32 % ee->r[t].sl32);
        LO1 = SE6432(ee->r[s].sl32 / ee->r[t].sl32);
    } else {
        HI1 = SE6432(ee->r[s].ul32);
        LO1 = ((int32_t)ee->r[s].ul32 < 0) ? 1 : -1;
    }
}
static inline void i_divs(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int d = D_FD;
    int s = D_FS;

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

    ee->f[d].f = FS / FT;

    if (fpu_check_overflow_no_flags(ee, &ee->f[d]))
        return;

    fpu_check_underflow_no_flags(ee, &ee->f[d]);
}
static inline void i_divu(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int s = D_RS;

    if (!ee->r[t].ul32) {
        LO0 = -1;
        HI0 = SE6432(ee->r[s].ul32);

        return;
    }

    HI0 = SE6432(ee->r[s].ul32 % ee->r[t].ul32);
    LO0 = SE6432(ee->r[s].ul32 / ee->r[t].ul32);
}
static inline void i_divu1(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int s = D_RS;

    if (!ee->r[t].ul32) {
        LO1 = -1;
        HI1 = SE6432(ee->r[s].ul32);

        return;
    }

    HI1 = SE6432(ee->r[s].ul32 % ee->r[t].ul32);
    LO1 = SE6432(ee->r[s].ul32 / ee->r[t].ul32);
}
static inline void i_dsll(Ee* ee, const Instruction& i) {
    RD = RT << D_SA;
}
static inline void i_dsll32(Ee* ee, const Instruction& i) {
    RD = RT << (D_SA + 32);
}
static inline void i_dsllv(Ee* ee, const Instruction& i) {
    RD = RT << (RS & 0x3f);
}
static inline void i_dsra(Ee* ee, const Instruction& i) {
    RD = ((int64_t)RT) >> D_SA;
}
static inline void i_dsra32(Ee* ee, const Instruction& i) {
    RD = ((int64_t)RT) >> (D_SA + 32);
}
static inline void i_dsrav(Ee* ee, const Instruction& i) {
    RD = ((int64_t)RT) >> (RS & 0x3f);
}
static inline void i_dsrl(Ee* ee, const Instruction& i) {
    RD = RT >> D_SA;
}
static inline void i_dsrl32(Ee* ee, const Instruction& i) {
    RD = RT >> (D_SA + 32);
}
static inline void i_dsrlv(Ee* ee, const Instruction& i) {
    RD = RT >> (RS & 0x3f);
}
static inline void i_dsub(Ee* ee, const Instruction& i) {
    long long r;

    if (SSUBOVF64((int64_t)RS, (int64_t)RT, &r)) {
        exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        RD = r;
    }
}
static inline void i_dsubu(Ee* ee, const Instruction& i) {
    RD = RS - RT;
}
static inline void i_ei(Ee* ee, const Instruction& i) {
    int edi = ee->status & SR_EDI;
    int exl = ee->status & SR_EXL;
    int erl = ee->status & SR_ERL;
    int ksu = ee->status & SR_KSU;
    
    if (edi || exl || erl || !ksu)
        ee->status |= SR_EIE;
}
static inline void i_eret(Ee* ee, const Instruction& i) {
    if (ee->status & SR_ERL) {
        set_pc(ee, ee->errorepc);

        ee->status &= ~SR_ERL;
    } else {
        set_pc(ee, ee->epc);

        ee->status &= ~SR_EXL;
    }

    // iris_debug(ee, "ERET at pc={:08x} next_pc={:08x}", ee->pc, ee->next_pc);
}
static inline void i_j(Ee* ee, const Instruction& i) {
    set_pc(ee, (ee->next_pc & 0xf0000000) | (D_I26 << 2));
}
static inline void i_jal(Ee* ee, const Instruction& i) {
    ee->r[31].ul64 = ee->next_pc;

    set_pc(ee, (ee->next_pc & 0xf0000000) | (D_I26 << 2));
}
static inline void i_jalr(Ee* ee, const Instruction& i) {
    uint32_t next_pc = ee->next_pc;

    set_pc(ee, RS32);

    RD = next_pc;
}
static inline void i_jr(Ee* ee, const Instruction& i) {
    set_pc(ee, RS32);
}
static inline void i_lb(Ee* ee, const Instruction& i) {
    RT = SE648(bus_read8(ee, RS32 + SE3216(D_I16)));
}
static inline void i_lbu(Ee* ee, const Instruction& i) {
    RT = bus_read8(ee, RS32 + SE3216(D_I16));
}
static inline void i_ld(Ee* ee, const Instruction& i) {
    RT = bus_read64(ee, RS32 + SE3216(D_I16));
}
static inline void i_ldl(Ee* ee, const Instruction& i) {
    static const uint8_t ldl_shift[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
    static const uint64_t ldl_mask[8] = {
        0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
        0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
    };

    uint32_t addr = RS32 + SE3216(D_I16);
    uint32_t shift = addr & 7;
    uint64_t data = bus_read64(ee, addr & ~7);

    RT = (RT & ldl_mask[shift]) | (data << ldl_shift[shift]);
}
static inline void i_ldr(Ee* ee, const Instruction& i) {
    static const uint8_t ldr_shift[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
    static const uint64_t ldr_mask[8] = {
        0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL,
        0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL
    };

    uint32_t addr = RS32 + SE3216(D_I16);
    uint32_t shift = addr & 7;
    uint64_t data = bus_read64(ee, addr & ~7);

    RT = (RT & ldr_mask[shift]) | (data >> ldr_shift[shift]);
}
static inline void i_lh(Ee* ee, const Instruction& i) {
    RT = SE6416(bus_read16(ee, RS32 + SE3216(D_I16)));
}
static inline void i_lhu(Ee* ee, const Instruction& i) {
    RT = bus_read16(ee, RS32 + SE3216(D_I16));
}
static inline void i_lq(Ee* ee, const Instruction& i) {
    ee->r[D_RT] = bus_read128(ee, (RS32 + SE3216(D_I16)) & ~0xf);
}
static inline void i_lqc2(Ee* ee, const Instruction& i) {
    int d = D_RT;

    if (!d) return;

    ee->vu0->vf[D_RT].u128 = bus_read128(ee, (RS32 + SE3216(D_I16)) & ~0xf);
}
static inline void i_lui(Ee* ee, const Instruction& i) {
    RT = SE6432(D_I16 << 16);
}
static inline void i_lw(Ee* ee, const Instruction& i) {
    RT = SE6432(bus_read32(ee, RS32 + SE3216(D_I16)));
}
static inline void i_lwc1(Ee* ee, const Instruction& i) {
    FT32 = bus_read32(ee, RS32 + SE3216(D_I16));
}

static const uint32_t LWl_MASK[4] = { 0x00ffffff, 0x0000ffff, 0x000000ff, 0x00000000 };
static const uint32_t LWR_MASK[4] = { 0x00000000, 0xff000000, 0xffff0000, 0xffffff00 };
static const int LWl_SHIFT[4] = { 24, 16, 8, 0 };
static const int LWR_SHIFT[4] = { 0, 8, 16, 24 };

static inline void i_lwl(Ee* ee, const Instruction& i) {
    uint32_t addr = RS32 + SE3216(D_I16);
    uint32_t shift = addr & 3;
    uint32_t mem = bus_read32(ee, addr & ~3);

    // ensure the compiler does correct sign extension into 64 bits by using s32
    RT = (int32_t)((RT32 & LWl_MASK[shift]) | (mem << LWl_SHIFT[shift]));
}

static inline void i_lwr(Ee* ee, const Instruction& i) {
    uint32_t addr = RS32 + SE3216(D_I16);
    uint32_t shift = addr & 3;
    uint32_t data = bus_read32(ee, addr & ~3);

    // Use unsigned math here, and conditionally sign extend below, when needed.
    data = (RT32 & LWR_MASK[shift]) | (data >> LWR_SHIFT[shift]);

    if (!shift) {
        // This special case requires sign extension into the full 64 bit dest.
        RT = (int32_t)data;
    } else {
        // This case sets the lower 32 bits of the target register. Upper
        // 32 bits are always preserved.
        RT32 = data;
    }

    // iris_debug(ee, "lwr mem={:08x} reg={:016x} addr={:08x} shift={}", data, ee->r[D_RT].u64[0], addr, shift);
}
static inline void i_lwu(Ee* ee, const Instruction& i) {
    RT = bus_read32(ee, RS32 + SE3216(D_I16));
}
static inline void i_madd(Ee* ee, const Instruction& i) {
    uint64_t r = SE6432(RS32) * SE6432(RT32);
    uint64_t d = (uint64_t)ee->lo.u32[0] | (ee->hi.u64[0] << 32);

    d += r;

    LO0 = SE6432(d & 0xffffffff);
    HI0 = SE6432(d >> 32);

    RD = LO0;
}
static inline void i_madd1(Ee* ee, const Instruction& i) {
    uint64_t r = SE6432(RS32) * SE6432(RT32);
    uint64_t d = (LO1 & 0xffffffff) | (HI1 << 32);

    d += r;

    LO1 = SE6432(d & 0xffffffff);
    HI1 = SE6432(d >> 32);

    RD = LO1;
}
static inline void i_maddas(Ee* ee, const Instruction& i) {
    ee->a.f += FS * FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void i_madds(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int d = D_FD;
    int s = D_FS;

    float temp = fpu_cvtf(ee->f[s].f) * fpu_cvtf(ee->f[t].f);

    ee->f[d].f = fpu_cvtf(ee->a.f) + fpu_cvtf(temp);

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void i_maddu(Ee* ee, const Instruction& i) {
    uint64_t r = (uint64_t)RS32 * (uint64_t)RT32;
    uint64_t d = (uint64_t)ee->lo.u32[0] | (ee->hi.u64[0] << 32);

    d += r;

    LO0 = SE6432(d & 0xffffffff);
    HI0 = SE6432(d >> 32);

    RD = LO0;
}
static inline void i_maddu1(Ee* ee, const Instruction& i) {
    uint64_t r = (uint64_t)RS32 * (uint64_t)RT32;
    uint64_t d = (uint64_t)ee->lo.u32[2] | (ee->hi.u64[1] << 32);

    d += r;

    LO1 = SE6432(d & 0xffffffff);
    HI1 = SE6432(d >> 32);

    RD = LO1;
}
static inline void i_maxs(Ee* ee, const Instruction& i) {
    ee->f[D_FD].u32 = fpu_max(ee->f[D_FS].u32, ee->f[D_RT].u32);

    ee->fcr &= ~(FPU_FLG_O | FPU_FLG_U);
}
static inline void i_mfc0(Ee* ee, const Instruction& i) {
    RT = SE6432(ee->cop0_r[D_RD]);
}
static inline void i_mfc1(Ee* ee, const Instruction& i) {
    RT = SE6432(FS32);
}
static inline void i_mfhi(Ee* ee, const Instruction& i) {
    RD = HI0;
}
static inline void i_mfhi1(Ee* ee, const Instruction& i) {
    RD = HI1;
}
static inline void i_mflo(Ee* ee, const Instruction& i) {
    RD = LO0;
}
static inline void i_mflo1(Ee* ee, const Instruction& i) {
    RD = LO1;
}
static inline void i_mfsa(Ee* ee, const Instruction& i) {
    RD = ee->sa & 0xf;
}
static inline void i_mins(Ee* ee, const Instruction& i) {
    ee->f[D_FD].u32 = fpu_min(ee->f[D_FS].u32, ee->f[D_RT].u32);

    ee->fcr &= ~(FPU_FLG_O | FPU_FLG_U);
}
static inline void i_movn(Ee* ee, const Instruction& i) {
    if (RT) RD = RS;
}
static inline void i_movs(Ee* ee, const Instruction& i) {
    FD32 = FS32;
}
static inline void i_movz(Ee* ee, const Instruction& i) {
    if (!RT) RD = RS;
}
static inline void i_msubas(Ee* ee, const Instruction& i) {
    ee->a.f -= FS * FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void i_msubs(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int d = D_FD;
    int s = D_FS;

    float temp = fpu_cvtf(ee->f[s].f) * fpu_cvtf(ee->f[t].f);

    ee->f[d].f = fpu_cvtf(ee->a.f) - fpu_cvtf(temp);

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void i_mtc0(Ee* ee, const Instruction& i) {
    int d = D_RD;

    if (d == 4) {
        ee->cop0_r[4] &= 0x7ffff0;
        ee->cop0_r[4] |= (RT32 & 0xff800000);
    } else {
        ee->cop0_r[D_RD] = RT32;
    }
}
static inline void i_mtc1(Ee* ee, const Instruction& i) {
    FS32 = RT32;
}
static inline void i_mthi(Ee* ee, const Instruction& i) {
    HI0 = RS;
}
static inline void i_mthi1(Ee* ee, const Instruction& i) {
    HI1 = RS;
}
static inline void i_mtlo(Ee* ee, const Instruction& i) {
    LO0 = RS;
}
static inline void i_mtlo1(Ee* ee, const Instruction& i) {
    LO1 = RS;
}
static inline void i_mtsa(Ee* ee, const Instruction& i) {
    ee->sa = ((uint32_t)RS) & 0xf;
}
static inline void i_mtsab(Ee* ee, const Instruction& i) {
    ee->sa = (RS ^ D_I16) & 15;
}
static inline void i_mtsah(Ee* ee, const Instruction& i) {
    ee->sa = ((RS ^ D_I16) & 7) << 1;
}
static inline void i_mulas(Ee* ee, const Instruction& i) {
    ee->a.f = FS * FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void i_muls(Ee* ee, const Instruction& i) {
    int d = D_FD;

    ee->f[d].f = FS * FT;

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void i_mult(Ee* ee, const Instruction& i) {
    uint64_t r = SE6432(RS32) * SE6432(RT32);

    LO0 = SE6432(r & 0xffffffff);
    HI0 = SE6432(r >> 32);

    RD = LO0;
}
static inline void i_mult1(Ee* ee, const Instruction& i) {
    uint64_t r = SE6432(RS32) * SE6432(RT32);

    LO1 = SE6432(r & 0xffffffff);
    HI1 = SE6432(r >> 32);

    RD = LO1;
}
static inline void i_multu(Ee* ee, const Instruction& i) {
    uint64_t r = (uint64_t)RS32 * (uint64_t)RT32;

    LO0 = SE6432(r & 0xffffffff);
    HI0 = SE6432(r >> 32);

    RD = LO0;
}
static inline void i_multu1(Ee* ee, const Instruction& i) {
    uint64_t r = (uint64_t)RS32 * (uint64_t)RT32;

    LO1 = SE6432(r & 0xffffffff);
    HI1 = SE6432(r >> 32);

    RD = LO1;
}
static inline void i_negs(Ee* ee, const Instruction& i) {
    ee->f[D_FD].u32 = ee->f[D_FS].u32 ^ 0x80000000;

    ee->fcr &= ~(FPU_FLG_O | FPU_FLG_U);
}
static inline void i_nor(Ee* ee, const Instruction& i) {
    RD = ~(RS | RT);
}
static inline void i_or(Ee* ee, const Instruction& i) {
    RD = RS | RT;
}
static inline void i_ori(Ee* ee, const Instruction& i) {
    RT = RS | D_I16;
}
static inline void i_pabsh(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pabsw(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_paddb(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_paddh(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_paddsb(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_paddsh(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_paddsw(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_paddub(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_padduh(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_padduw(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_paddw(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_padsbh(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pand(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pceqb(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pceqh(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pceqw(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pcgtb(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pcgth(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pcgtw(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* s = &ee->r[D_RS];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pcpyh(Ee* ee, const Instruction& i) {
    uint128_t* d = &ee->r[D_RD];
    uint128_t* t = &ee->r[D_RT];

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
static inline void i_pcpyld(Ee* ee, const Instruction& i) {
#ifndef _EE_USE_INTRINSICS
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

    ee->r[d].u64[0] = rt.u64[0];
    ee->r[d].u64[1] = rs.u64[0];
#else
    __m128i a = _mm_load_si128((__m128i*)&ee->r[D_RT]);
    __m128i b = _mm_load_si128((__m128i*)&ee->r[D_RS]);
    __m128i r = _mm_unpacklo_epi64(a, b);

    _mm_store_si128((__m128i*)&ee->r[D_RD], r);
#endif
}
static inline void i_pcpyud(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

    ee->r[d].u64[0] = rs.u64[1];
    ee->r[d].u64[1] = rt.u64[1];
}
static inline void i_pdivbw(Ee* ee, const Instruction& i) {
    int s = D_RS;
    int t = D_RT;

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
static inline void i_pdivuw(Ee* ee, const Instruction& i) {
    int s = D_RS;
    int t = D_RT;

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
static inline void i_pdivw(Ee* ee, const Instruction& i) {
    int s = D_RS;
    int t = D_RT;

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
static inline void i_pexch(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rt.u16[2];
    ee->r[d].u16[2] = rt.u16[1];
    ee->r[d].u16[3] = rt.u16[3];
    ee->r[d].u16[4] = rt.u16[4];
    ee->r[d].u16[5] = rt.u16[6];
    ee->r[d].u16[6] = rt.u16[5];
    ee->r[d].u16[7] = rt.u16[7];
}
static inline void i_pexcw(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rt.u32[1];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void i_pexeh(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u16[0] = rt.u16[2];
    ee->r[d].u16[1] = rt.u16[1];
    ee->r[d].u16[2] = rt.u16[0];
    ee->r[d].u16[3] = rt.u16[3];
    ee->r[d].u16[4] = rt.u16[6];
    ee->r[d].u16[5] = rt.u16[5];
    ee->r[d].u16[6] = rt.u16[4];
    ee->r[d].u16[7] = rt.u16[7];
}
static inline void i_pexew(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u32[0] = rt.u32[2];
    ee->r[d].u32[1] = rt.u32[1];
    ee->r[d].u32[2] = rt.u32[0];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void i_pext5(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u32[0] = unpack_5551_8888(rt.u32[0]);
    ee->r[d].u32[1] = unpack_5551_8888(rt.u32[1]);
    ee->r[d].u32[2] = unpack_5551_8888(rt.u32[2]);
    ee->r[d].u32[3] = unpack_5551_8888(rt.u32[3]);
}
static inline void i_pextlb(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

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
static inline void i_pextlh(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rs.u16[0];
    ee->r[d].u16[2] = rt.u16[1];
    ee->r[d].u16[3] = rs.u16[1];
    ee->r[d].u16[4] = rt.u16[2];
    ee->r[d].u16[5] = rs.u16[2];
    ee->r[d].u16[6] = rt.u16[3];
    ee->r[d].u16[7] = rs.u16[3];
}
static inline void i_pextlw(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rs.u32[0];
    ee->r[d].u32[2] = rt.u32[1];
    ee->r[d].u32[3] = rs.u32[1];
}
static inline void i_pextub(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

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
static inline void i_pextuh(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

    ee->r[d].u16[0] = rt.u16[4];
    ee->r[d].u16[1] = rs.u16[4];
    ee->r[d].u16[2] = rt.u16[5];
    ee->r[d].u16[3] = rs.u16[5];
    ee->r[d].u16[4] = rt.u16[6];
    ee->r[d].u16[5] = rs.u16[6];
    ee->r[d].u16[6] = rt.u16[7];
    ee->r[d].u16[7] = rs.u16[7];
}
static inline void i_pextuw(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

    ee->r[d].u32[0] = rt.u32[2];
    ee->r[d].u32[1] = rs.u32[2];
    ee->r[d].u32[2] = rt.u32[3];
    ee->r[d].u32[3] = rs.u32[3];
}
static inline void i_phmadh(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

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
static inline void i_phmsbh(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

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
static inline void i_pinteh(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rs.u16[0];
    ee->r[d].u16[2] = rt.u16[2];
    ee->r[d].u16[3] = rs.u16[2];
    ee->r[d].u16[4] = rt.u16[4];
    ee->r[d].u16[5] = rs.u16[4];
    ee->r[d].u16[6] = rt.u16[6];
    ee->r[d].u16[7] = rs.u16[6];
}
static inline void i_pinth(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    uint128_t rs = ee->r[D_RS];
    int d = D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rs.u16[4];
    ee->r[d].u16[2] = rt.u16[1];
    ee->r[d].u16[3] = rs.u16[5];
    ee->r[d].u16[4] = rt.u16[2];
    ee->r[d].u16[5] = rs.u16[6];
    ee->r[d].u16[6] = rt.u16[3];
    ee->r[d].u16[7] = rs.u16[7];
}
static inline void i_plzcw(Ee* ee, const Instruction& i) {
    for (int j = 0; j < 2; j++) {
        uint32_t word = ee->r[D_RS].u32[j];

        int msb = word & 0x80000000;

        word = (msb ? ~word : word);

        ee->r[D_RD].u32[j] = (word ? (__builtin_clz(word) - 1) : 0x1f);
    }

    ee->r[D_RD].u32[2] = 0;
    ee->r[D_RD].u32[3] = 0;
}
static inline void i_pmaddh(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

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
static inline void i_pmadduw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    uint64_t r0 = (uint64_t)ee->r[s].u32[0] * (uint64_t)ee->r[t].u32[0];
    uint64_t r1 = (uint64_t)ee->r[s].u32[2] * (uint64_t)ee->r[t].u32[2];

    ee->r[d].u64[0] = r0 + ((ee->hi.u64[0] << 32) | (uint64_t)ee->lo.u32[0]);
    ee->r[d].u64[1] = r1 + ((ee->hi.u64[1] << 32) | (uint64_t)ee->lo.u32[2]);
    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void i_pmaddw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    uint64_t r0 = (int64_t)ee->r[s].s32[0] * (int64_t)ee->r[t].s32[0];
    uint64_t r1 = (int64_t)ee->r[s].s32[2] * (int64_t)ee->r[t].s32[2];

    ee->r[d].u64[0] = r0 + ((((uint64_t)ee->hi.u32[0]) << 32) | (uint64_t)ee->lo.u32[0]);
    ee->r[d].u64[1] = r1 + ((((uint64_t)ee->hi.u32[2]) << 32) | (uint64_t)ee->lo.u32[2]);
    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void i_pmaxh(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    ee->r[d].u16[0] = ((int16_t)ee->r[s].u16[0] > (int16_t)ee->r[t].u16[0]) ? ee->r[s].u16[0] : ee->r[t].u16[0];
    ee->r[d].u16[1] = ((int16_t)ee->r[s].u16[1] > (int16_t)ee->r[t].u16[1]) ? ee->r[s].u16[1] : ee->r[t].u16[1];
    ee->r[d].u16[2] = ((int16_t)ee->r[s].u16[2] > (int16_t)ee->r[t].u16[2]) ? ee->r[s].u16[2] : ee->r[t].u16[2];
    ee->r[d].u16[3] = ((int16_t)ee->r[s].u16[3] > (int16_t)ee->r[t].u16[3]) ? ee->r[s].u16[3] : ee->r[t].u16[3];
    ee->r[d].u16[4] = ((int16_t)ee->r[s].u16[4] > (int16_t)ee->r[t].u16[4]) ? ee->r[s].u16[4] : ee->r[t].u16[4];
    ee->r[d].u16[5] = ((int16_t)ee->r[s].u16[5] > (int16_t)ee->r[t].u16[5]) ? ee->r[s].u16[5] : ee->r[t].u16[5];
    ee->r[d].u16[6] = ((int16_t)ee->r[s].u16[6] > (int16_t)ee->r[t].u16[6]) ? ee->r[s].u16[6] : ee->r[t].u16[6];
    ee->r[d].u16[7] = ((int16_t)ee->r[s].u16[7] > (int16_t)ee->r[t].u16[7]) ? ee->r[s].u16[7] : ee->r[t].u16[7];
}
static inline void i_pmaxw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    ee->r[d].u32[0] = ((int32_t)ee->r[s].u32[0] > (int32_t)ee->r[t].u32[0]) ? ee->r[s].u32[0] : ee->r[t].u32[0];
    ee->r[d].u32[1] = ((int32_t)ee->r[s].u32[1] > (int32_t)ee->r[t].u32[1]) ? ee->r[s].u32[1] : ee->r[t].u32[1];
    ee->r[d].u32[2] = ((int32_t)ee->r[s].u32[2] > (int32_t)ee->r[t].u32[2]) ? ee->r[s].u32[2] : ee->r[t].u32[2];
    ee->r[d].u32[3] = ((int32_t)ee->r[s].u32[3] > (int32_t)ee->r[t].u32[3]) ? ee->r[s].u32[3] : ee->r[t].u32[3];
}
static inline void i_pmfhi(Ee* ee, const Instruction& i) {
    ee->r[D_RD] = ee->hi;
}
static inline void i_pmfhllw(Ee* ee, const Instruction& i) {
    int d = D_RD;

    ee->r[d].u32[0] = ee->lo.u32[0];
    ee->r[d].u32[1] = ee->hi.u32[0];
    ee->r[d].u32[2] = ee->lo.u32[2];
    ee->r[d].u32[3] = ee->hi.u32[2];
}
static inline void i_pmfhluw(Ee* ee, const Instruction& i) {
    int d = D_RD;

    ee->r[d].u32[0] = ee->lo.u32[1];
    ee->r[d].u32[1] = ee->hi.u32[1];
    ee->r[d].u32[2] = ee->lo.u32[3];
    ee->r[d].u32[3] = ee->hi.u32[3];
}
static inline void i_pmfhlslw(Ee* ee, const Instruction& i) {
    int d = D_RD;

    ee->r[d].u64[0] = SE6432(saturate32(((uint64_t)ee->lo.u32[0]) | (ee->hi.u64[0] << 32)));
    ee->r[d].u64[1] = SE6432(saturate32(((uint64_t)ee->lo.u32[2]) | (ee->hi.u64[1] << 32)));
}
static inline void i_pmfhllh(Ee* ee, const Instruction& i) {
    int d = D_RD;

    ee->r[d].u16[0] = ee->lo.u16[0];
    ee->r[d].u16[1] = ee->lo.u16[2];
    ee->r[d].u16[2] = ee->hi.u16[0];
    ee->r[d].u16[3] = ee->hi.u16[2];
    ee->r[d].u16[4] = ee->lo.u16[4];
    ee->r[d].u16[5] = ee->lo.u16[6];
    ee->r[d].u16[6] = ee->hi.u16[4];
    ee->r[d].u16[7] = ee->hi.u16[6];
    
}
static inline void i_pmfhlsh(Ee* ee, const Instruction& i) {
    int d = D_RD;

    ee->r[d].u16[0] = saturate16(ee->lo.u32[0]);
    ee->r[d].u16[1] = saturate16(ee->lo.u32[1]);
    ee->r[d].u16[2] = saturate16(ee->hi.u32[0]);
    ee->r[d].u16[3] = saturate16(ee->hi.u32[1]);
    ee->r[d].u16[4] = saturate16(ee->lo.u32[2]);
    ee->r[d].u16[5] = saturate16(ee->lo.u32[3]);
    ee->r[d].u16[6] = saturate16(ee->hi.u32[2]);
    ee->r[d].u16[7] = saturate16(ee->hi.u32[3]);
}
static inline void i_pmflo(Ee* ee, const Instruction& i) {
    ee->r[D_RD] = ee->lo;
}
static inline void i_pminh(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    ee->r[d].u16[0] = ((int16_t)ee->r[s].u16[0] < (int16_t)ee->r[t].u16[0]) ? ee->r[s].u16[0] : ee->r[t].u16[0];
    ee->r[d].u16[1] = ((int16_t)ee->r[s].u16[1] < (int16_t)ee->r[t].u16[1]) ? ee->r[s].u16[1] : ee->r[t].u16[1];
    ee->r[d].u16[2] = ((int16_t)ee->r[s].u16[2] < (int16_t)ee->r[t].u16[2]) ? ee->r[s].u16[2] : ee->r[t].u16[2];
    ee->r[d].u16[3] = ((int16_t)ee->r[s].u16[3] < (int16_t)ee->r[t].u16[3]) ? ee->r[s].u16[3] : ee->r[t].u16[3];
    ee->r[d].u16[4] = ((int16_t)ee->r[s].u16[4] < (int16_t)ee->r[t].u16[4]) ? ee->r[s].u16[4] : ee->r[t].u16[4];
    ee->r[d].u16[5] = ((int16_t)ee->r[s].u16[5] < (int16_t)ee->r[t].u16[5]) ? ee->r[s].u16[5] : ee->r[t].u16[5];
    ee->r[d].u16[6] = ((int16_t)ee->r[s].u16[6] < (int16_t)ee->r[t].u16[6]) ? ee->r[s].u16[6] : ee->r[t].u16[6];
    ee->r[d].u16[7] = ((int16_t)ee->r[s].u16[7] < (int16_t)ee->r[t].u16[7]) ? ee->r[s].u16[7] : ee->r[t].u16[7];
}
static inline void i_pminw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    ee->r[d].u32[0] = (ee->r[s].s32[0] < ee->r[t].s32[0]) ? ee->r[s].u32[0] : ee->r[t].u32[0];
    ee->r[d].u32[1] = (ee->r[s].s32[1] < ee->r[t].s32[1]) ? ee->r[s].u32[1] : ee->r[t].u32[1];
    ee->r[d].u32[2] = (ee->r[s].s32[2] < ee->r[t].s32[2]) ? ee->r[s].u32[2] : ee->r[t].u32[2];
    ee->r[d].u32[3] = (ee->r[s].s32[3] < ee->r[t].s32[3]) ? ee->r[s].u32[3] : ee->r[t].u32[3];
}
static inline void i_pmsubh(Ee* ee, const Instruction& i) {
    int s = D_RS;
    int t = D_RT;
    int d = D_RD;

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
static inline void i_pmsubw(Ee* ee, const Instruction& i) {
    int s = D_RS;
    int t = D_RT;
    int d = D_RD;

    uint64_t r0 = (int64_t)ee->r[s].s32[0] * (int64_t)ee->r[t].s32[0];
    uint64_t r1 = (int64_t)ee->r[s].s32[2] * (int64_t)ee->r[t].s32[2];

    ee->r[d].u64[0] = ((((uint64_t)ee->hi.u32[0]) << 32) | (uint64_t)ee->lo.u32[0]) - r0;
    ee->r[d].u64[1] = ((((uint64_t)ee->hi.u32[2]) << 32) | (uint64_t)ee->lo.u32[2]) - r1;
    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void i_pmthi(Ee* ee, const Instruction& i) {
    ee->hi = ee->r[D_RS];
}
static inline void i_pmthl(Ee* ee, const Instruction& i) {
    int s = D_RS;

    ee->lo.u32[0] = ee->r[s].u32[0];
    ee->lo.u32[2] = ee->r[s].u32[2];
    ee->hi.u32[0] = ee->r[s].u32[1];
    ee->hi.u32[2] = ee->r[s].u32[3];
}
static inline void i_pmtlo(Ee* ee, const Instruction& i) {
    ee->lo = ee->r[D_RS];
}
static inline void i_pmulth(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

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
static inline void i_pmultuw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    ee->r[d].u64[0] = (uint64_t)ee->r[s].u32[0] * (uint64_t)ee->r[t].u32[0];
    ee->r[d].u64[1] = (uint64_t)ee->r[s].u32[2] * (uint64_t)ee->r[t].u32[2];

    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void i_pmultw(Ee* ee, const Instruction& i) {
    int s = D_RS;
    int t = D_RT;
    int d = D_RD;

    ee->r[d].u64[0] = SE6432(ee->r[s].u32[0]) * SE6432(ee->r[t].u32[0]);
    ee->r[d].u64[1] = SE6432(ee->r[s].u32[2]) * SE6432(ee->r[t].u32[2]);

    ee->lo.u64[0] = SE6432(ee->r[d].u32[0]);
    ee->lo.u64[1] = SE6432(ee->r[d].u32[2]);
    ee->hi.u64[0] = SE6432(ee->r[d].u32[1]);
    ee->hi.u64[1] = SE6432(ee->r[d].u32[3]);
}
static inline void i_pnor(Ee* ee, const Instruction& i) {
    uint128_t rs = ee->r[D_RS];
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u64[0] = ~(rs.u64[0] | rt.u64[0]);
    ee->r[d].u64[1] = ~(rs.u64[1] | rt.u64[1]);
}
static inline void i_por(Ee* ee, const Instruction& i) {
    uint128_t rs = ee->r[D_RS];
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u64[0] = rs.u64[0] | rt.u64[0];
    ee->r[d].u64[1] = rs.u64[1] | rt.u64[1];
}
static inline void i_ppac5(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

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
static inline void i_ppacb(Ee* ee, const Instruction& i) {
    uint128_t rs = ee->r[D_RS];
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

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
static inline void i_ppach(Ee* ee, const Instruction& i) {
    uint128_t rs = ee->r[D_RS];
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u16[0] = rt.u16[0];
    ee->r[d].u16[1] = rt.u16[2];
    ee->r[d].u16[2] = rt.u16[4];
    ee->r[d].u16[3] = rt.u16[6];
    ee->r[d].u16[4] = rs.u16[0];
    ee->r[d].u16[5] = rs.u16[2];
    ee->r[d].u16[6] = rs.u16[4];
    ee->r[d].u16[7] = rs.u16[6];
}
static inline void i_ppacw(Ee* ee, const Instruction& i) {
    uint128_t rs = ee->r[D_RS];
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u32[0] = rt.u32[0];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rs.u32[0];
    ee->r[d].u32[3] = rs.u32[2];
}
static inline void i_pref(Ee* ee, const Instruction& i) {
    // Does nothing
}
static inline void i_prevh(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u16[0] = rt.u16[3];
    ee->r[d].u16[1] = rt.u16[2];
    ee->r[d].u16[2] = rt.u16[1];
    ee->r[d].u16[3] = rt.u16[0];
    ee->r[d].u16[4] = rt.u16[7];
    ee->r[d].u16[5] = rt.u16[6];
    ee->r[d].u16[6] = rt.u16[5];
    ee->r[d].u16[7] = rt.u16[4];
}
static inline void i_prot3w(Ee* ee, const Instruction& i) {
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u32[0] = rt.u32[1];
    ee->r[d].u32[1] = rt.u32[2];
    ee->r[d].u32[2] = rt.u32[0];
    ee->r[d].u32[3] = rt.u32[3];
}
static inline void i_psllh(Ee* ee, const Instruction& i) {
    int sa = D_SA & 0xf;
    int t = D_RT;
    int d = D_RD;

    ee->r[d].u16[0] = ee->r[t].u16[0] << sa;
    ee->r[d].u16[1] = ee->r[t].u16[1] << sa;
    ee->r[d].u16[2] = ee->r[t].u16[2] << sa;
    ee->r[d].u16[3] = ee->r[t].u16[3] << sa;
    ee->r[d].u16[4] = ee->r[t].u16[4] << sa;
    ee->r[d].u16[5] = ee->r[t].u16[5] << sa;
    ee->r[d].u16[6] = ee->r[t].u16[6] << sa;
    ee->r[d].u16[7] = ee->r[t].u16[7] << sa;
}
static inline void i_psllvw(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int d = D_RD;
    int s = D_RS;

    ee->r[d].u64[0] = SE6432(ee->r[t].u32[0] << (ee->r[s].u32[0] & 31));
    ee->r[d].u64[1] = SE6432(ee->r[t].u32[2] << (ee->r[s].u32[2] & 31));
}
static inline void i_psllw(Ee* ee, const Instruction& i) {
    int sa = D_SA;
    int t = D_RT;
    int d = D_RD;

    ee->r[d].u32[0] = ee->r[t].u32[0] << sa;
    ee->r[d].u32[1] = ee->r[t].u32[1] << sa;
    ee->r[d].u32[2] = ee->r[t].u32[2] << sa;
    ee->r[d].u32[3] = ee->r[t].u32[3] << sa;
}
static inline void i_psrah(Ee* ee, const Instruction& i) {
    int sa = D_SA & 0xf;
    int t = D_RT;
    int d = D_RD;

    ee->r[d].u16[0] = ((int16_t)ee->r[t].u16[0]) >> sa;
    ee->r[d].u16[1] = ((int16_t)ee->r[t].u16[1]) >> sa;
    ee->r[d].u16[2] = ((int16_t)ee->r[t].u16[2]) >> sa;
    ee->r[d].u16[3] = ((int16_t)ee->r[t].u16[3]) >> sa;
    ee->r[d].u16[4] = ((int16_t)ee->r[t].u16[4]) >> sa;
    ee->r[d].u16[5] = ((int16_t)ee->r[t].u16[5]) >> sa;
    ee->r[d].u16[6] = ((int16_t)ee->r[t].u16[6]) >> sa;
    ee->r[d].u16[7] = ((int16_t)ee->r[t].u16[7]) >> sa;
}
static inline void i_psravw(Ee* ee, const Instruction& i) {
    int s = D_RS;
    int t = D_RT;
    int d = D_RD;

    ee->r[d].u64[0] = SE6432((int32_t)ee->r[t].u32[0] >> (ee->r[s].u32[0] & 31));
    ee->r[d].u64[1] = SE6432((int32_t)ee->r[t].u32[2] >> (ee->r[s].u32[2] & 31));
}
static inline void i_psraw(Ee* ee, const Instruction& i) {
    int sa = D_SA;
    int t = D_RT;
    int d = D_RD;

    ee->r[d].u32[0] = ((int32_t)ee->r[t].u32[0]) >> sa;
    ee->r[d].u32[1] = ((int32_t)ee->r[t].u32[1]) >> sa;
    ee->r[d].u32[2] = ((int32_t)ee->r[t].u32[2]) >> sa;
    ee->r[d].u32[3] = ((int32_t)ee->r[t].u32[3]) >> sa;
}
static inline void i_psrlh(Ee* ee, const Instruction& i) {
    int sa = D_SA & 0xf;
    int t = D_RT;
    int d = D_RD;

    ee->r[d].u16[0] = ee->r[t].u16[0] >> sa;
    ee->r[d].u16[1] = ee->r[t].u16[1] >> sa;
    ee->r[d].u16[2] = ee->r[t].u16[2] >> sa;
    ee->r[d].u16[3] = ee->r[t].u16[3] >> sa;
    ee->r[d].u16[4] = ee->r[t].u16[4] >> sa;
    ee->r[d].u16[5] = ee->r[t].u16[5] >> sa;
    ee->r[d].u16[6] = ee->r[t].u16[6] >> sa;
    ee->r[d].u16[7] = ee->r[t].u16[7] >> sa;
}
static inline void i_psrlvw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    ee->r[d].u64[0] = SE6432(ee->r[t].u32[0] >> (ee->r[s].u32[0] & 31));
    ee->r[d].u64[1] = SE6432(ee->r[t].u32[2] >> (ee->r[s].u32[2] & 31));
}
static inline void i_psrlw(Ee* ee, const Instruction& i) {
    int sa = D_SA;
    int t = D_RT;
    int d = D_RD;

    ee->r[d].u32[0] = ee->r[t].u32[0] >> sa;
    ee->r[d].u32[1] = ee->r[t].u32[1] >> sa;
    ee->r[d].u32[2] = ee->r[t].u32[2] >> sa;
    ee->r[d].u32[3] = ee->r[t].u32[3] >> sa;
}
static inline void i_psubb(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int s = D_RS;
    int d = D_RD;

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
static inline void i_psubh(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int s = D_RS;
    int d = D_RD;

    ee->r[d].u16[0] = ee->r[s].u16[0] - ee->r[t].u16[0];
    ee->r[d].u16[1] = ee->r[s].u16[1] - ee->r[t].u16[1];
    ee->r[d].u16[2] = ee->r[s].u16[2] - ee->r[t].u16[2];
    ee->r[d].u16[3] = ee->r[s].u16[3] - ee->r[t].u16[3];
    ee->r[d].u16[4] = ee->r[s].u16[4] - ee->r[t].u16[4];
    ee->r[d].u16[5] = ee->r[s].u16[5] - ee->r[t].u16[5];
    ee->r[d].u16[6] = ee->r[s].u16[6] - ee->r[t].u16[6];
    ee->r[d].u16[7] = ee->r[s].u16[7] - ee->r[t].u16[7];
}
static inline void i_psubsb(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

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
static inline void i_psubsh(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

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
static inline void i_psubsw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    int64_t r0 = SE6432(ee->r[s].u32[0]) - SE6432(ee->r[t].u32[0]);
    int64_t r1 = SE6432(ee->r[s].u32[1]) - SE6432(ee->r[t].u32[1]);
    int64_t r2 = SE6432(ee->r[s].u32[2]) - SE6432(ee->r[t].u32[2]);
    int64_t r3 = SE6432(ee->r[s].u32[3]) - SE6432(ee->r[t].u32[3]);

    ee->r[d].u32[0] = (r0 >= 0x7fffffff) ? 0x7fffffff : ((r0 < (int32_t)0x80000000) ? 0x80000000 : r0);
    ee->r[d].u32[1] = (r1 >= 0x7fffffff) ? 0x7fffffff : ((r1 < (int32_t)0x80000000) ? 0x80000000 : r1);
    ee->r[d].u32[2] = (r2 >= 0x7fffffff) ? 0x7fffffff : ((r2 < (int32_t)0x80000000) ? 0x80000000 : r2);
    ee->r[d].u32[3] = (r3 >= 0x7fffffff) ? 0x7fffffff : ((r3 < (int32_t)0x80000000) ? 0x80000000 : r3);
}
static inline void i_psubub(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

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
static inline void i_psubuh(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

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
static inline void i_psubuw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    int64_t r0 = (int64_t)ee->r[s].u32[0] - (int64_t)ee->r[t].u32[0];
    int64_t r1 = (int64_t)ee->r[s].u32[1] - (int64_t)ee->r[t].u32[1];
    int64_t r2 = (int64_t)ee->r[s].u32[2] - (int64_t)ee->r[t].u32[2];
    int64_t r3 = (int64_t)ee->r[s].u32[3] - (int64_t)ee->r[t].u32[3];

    ee->r[d].u32[0] = (r0 < 0) ? 0 : r0;
    ee->r[d].u32[1] = (r1 < 0) ? 0 : r1;
    ee->r[d].u32[2] = (r2 < 0) ? 0 : r2;
    ee->r[d].u32[3] = (r3 < 0) ? 0 : r3;
}
static inline void i_psubw(Ee* ee, const Instruction& i) {
    int d = D_RD;
    int s = D_RS;
    int t = D_RT;

    ee->r[d].u32[0] = ee->r[s].u32[0] - ee->r[t].u32[0];
    ee->r[d].u32[1] = ee->r[s].u32[1] - ee->r[t].u32[1];
    ee->r[d].u32[2] = ee->r[s].u32[2] - ee->r[t].u32[2];
    ee->r[d].u32[3] = ee->r[s].u32[3] - ee->r[t].u32[3];
}
static inline void i_pxor(Ee* ee, const Instruction& i) {
    uint128_t rs = ee->r[D_RS];
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

    ee->r[d].u64[0] = rs.u64[0] ^ rt.u64[0];
    ee->r[d].u64[1] = rs.u64[1] ^ rt.u64[1];
}
static inline void i_qfsrv(Ee* ee, const Instruction& i) {
    uint128_t rs = ee->r[D_RS];
    uint128_t rt = ee->r[D_RT];
    int d = D_RD;

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
static inline void i_qmfc2(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int d = D_RD;

    ee->r[t].u64[0] = ee->vu0->vf[d].u64[0];
    ee->r[t].u64[1] = ee->vu0->vf[d].u64[1];
}
static inline void i_qmtc2(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int d = D_RD;

    if (!d) return;

    ee->vu0->vf[d].u128 = ee->r[t];
}
static inline void i_rsqrts(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int d = D_FD;

    ee->fcr &= ~(FPU_FLG_I | FPU_FLG_D);

    if ((ee->f[t].u32 & 0x7f800000) == 0) {
        ee->fcr |= FPU_FLG_D | FPU_FLG_SD;
        ee->f[d].u32 = (ee->f[t].u32 & 0x80000000) | 0x7f7fffff;
        
        return;
    } else if (ee->f[t].u32 & 0x80000000) {
        ee->fcr |= FPU_FLG_I | FPU_FLG_SI;

        ee->f[d].f = FS / sqrtf(fabsf(fpu_cvtf(ee->f[t].f)));
    } else {
        ee->f[d].f = FS / sqrtf(fpu_cvtf(ee->f[t].f));
    }

    if (fpu_check_overflow_no_flags(ee, &ee->f[d]))
        return;

    fpu_check_underflow_no_flags(ee, &ee->f[d]);
}
static inline void i_sb(Ee* ee, const Instruction& i) {
    bus_write8(ee, RS32 + SE3216(D_I16), RT);
}
static inline void i_sd(Ee* ee, const Instruction& i) {
    bus_write64(ee, RS32 + SE3216(D_I16), RT);
}
static inline void i_sdl(Ee* ee, const Instruction& i) {
    static const uint8_t sdl_shift[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
    static const uint64_t sdl_mask[8] = {
        0xffffffffffffff00ULL, 0xffffffffffff0000ULL, 0xffffffffff000000ULL, 0xffffffff00000000ULL,
        0xffffff0000000000ULL, 0xffff000000000000ULL, 0xff00000000000000ULL, 0x0000000000000000ULL
    };

    uint32_t addr = RS32 + SE3216(D_I16);
    uint32_t shift = addr & 7;
    uint64_t data = bus_read64(ee, addr & ~7);

    bus_write64(ee, addr & ~7, (RT >> sdl_shift[shift]) | (data & sdl_mask[shift]));
}
static inline void i_sdr(Ee* ee, const Instruction& i) {
    static const uint8_t sdr_shift[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
    static const uint64_t sdr_mask[8] = {
        0x0000000000000000ULL, 0x00000000000000ffULL, 0x000000000000ffffULL, 0x0000000000ffffffULL,
        0x00000000ffffffffULL, 0x000000ffffffffffULL, 0x0000ffffffffffffULL, 0x00ffffffffffffffULL
    };

    uint32_t addr = RS32 + SE3216(D_I16);
    uint32_t shift = addr & 7;
    uint64_t data = bus_read64(ee, addr & ~7);

    bus_write64(ee, addr & ~7, (RT << sdr_shift[shift]) | (data & sdr_mask[shift]));
}
static inline void i_sh(Ee* ee, const Instruction& i) {
    bus_write16(ee, RS32 + SE3216(D_I16), RT);
}
static inline void i_sll(Ee* ee, const Instruction& i) {
    RD = SE6432(RT32 << D_SA);
}
static inline void i_sllv(Ee* ee, const Instruction& i) {
    RD = SE6432(RT32 << (RS & 0x1f));
}
static inline void i_slt(Ee* ee, const Instruction& i) {
    RD = (int64_t)RS < (int64_t)RT;
}
static inline void i_slti(Ee* ee, const Instruction& i) {
    RT = ((int64_t)RS) < SE6416(D_I16);
}
static inline void i_sltiu(Ee* ee, const Instruction& i) {
    RT = RS < (uint64_t)(SE6416(D_I16));
}
static inline void i_sltu(Ee* ee, const Instruction& i) {
    RD = RS < RT;
}
static inline void i_sq(Ee* ee, const Instruction& i) {
    bus_write128(ee, (RS32 + SE3216(D_I16)) & ~0xf, ee->r[D_RT]);
}
static inline void i_sqc2(Ee* ee, const Instruction& i) {
    bus_write128(ee, (RS32 + SE3216(D_I16)) & ~0xf, ee->vu0->vf[D_RT].u128);
}
static inline void i_sqrts(Ee* ee, const Instruction& i) {
    int t = D_RT;
    int d = D_FD;

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
static inline void i_sra(Ee* ee, const Instruction& i) {
    RD = SE6432(((int32_t)RT32) >> D_SA);
}
static inline void i_srav(Ee* ee, const Instruction& i) {
    RD = SE6432(((int32_t)RT32) >> (RS & 0x1f));
}
static inline void i_srl(Ee* ee, const Instruction& i) {
    RD = SE6432(RT32 >> D_SA);
}
static inline void i_srlv(Ee* ee, const Instruction& i) {
    RD = SE6432(RT32 >> (RS & 0x1f));
}
static inline void i_sub(Ee* ee, const Instruction& i) {
    int32_t r;

    int o = __builtin_ssub_overflow(RS32, RT32, &r);

    if (o) {
        exception_level1(ee, CAUSE_EXC1_OV);
    } else {
        RD = SE6432(r);
    }
}
static inline void i_subas(Ee* ee, const Instruction& i) {
    ee->a.f = FS - FT;

    if (fpu_check_overflow(ee, &ee->a))
        return;

    fpu_check_underflow(ee, &ee->a);
}
static inline void i_subs(Ee* ee, const Instruction& i) {
    int d = D_FD;

    ee->f[d].f = FS - FT;

    if (fpu_check_overflow(ee, &ee->f[d]))
        return;

    fpu_check_underflow(ee, &ee->f[d]);
}
static inline void i_subu(Ee* ee, const Instruction& i) {
    RD = SE6432(RS - RT);
}
static inline void i_sw(Ee* ee, const Instruction& i) {
    bus_write32(ee, RS32 + SE3216(D_I16), RT32);
}
static inline void i_swc1(Ee* ee, const Instruction& i) {
    bus_write32(ee, RS32 + SE3216(D_I16), FT32);
}
static inline void i_swl(Ee* ee, const Instruction& i) {
    static const uint32_t swl_mask[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0x00000000 };
    static const uint8_t swl_shift[4] = { 24, 16, 8, 0 };

    uint32_t addr = RS32 + SE3216(D_I16);
    uint32_t mem = bus_read32(ee, addr & ~3);

    int shift = addr & 3;

    bus_write32(ee, addr & ~3, (RT32 >> swl_shift[shift] | (mem & swl_mask[shift])));

    // iris_debug(ee, "swl mem={:08x} reg={:016x} addr={:08x} shift={} rs={:08x} i16={:04x}", mem, ee->r[D_RT].u64[0], addr, shift, RS32, D_I16);
}
static inline void i_swr(Ee* ee, const Instruction& i) {
    static const uint32_t swr_mask[4] = { 0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };
    static const uint8_t swr_shift[4] = { 0, 8, 16, 24 };

    uint32_t addr = RS32 + SE3216(D_I16);
    uint32_t mem = bus_read32(ee, addr & ~3);

    int shift = addr & 3;

    bus_write32(ee, addr & ~3, (RT32 << swr_shift[shift]) | (mem & swr_mask[shift]));

    // iris_debug(ee, "swl mem={:08x} reg={:016x} addr={:08x} shift={} rs={:08x} i16={:04x}", mem, ee->r[D_RT].u64[0], addr, shift, RS32, D_I16);
}
static inline void i_sync(Ee* ee, const Instruction& i) {
    /* Do nothing */
}

// #include "syscall.hpp"

static inline void get_thread_list(Ee* ee) {
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

            iris_debug(ee, "Found Thread list base at {:08x}", ee->thread_list_base);

            break;
        }

        offset += 4;
    }
}

static inline void i_syscall(Ee* ee, const Instruction& i) {
    uint32_t id = ee->r[3].ul64;

    if (id & 0x80000000) {
        id = (~id) + 1;
    }

    switch (id) {
        // ChangeThreadPriority
        case 0x29: {
            get_thread_list(ee);
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
            if (ee->boot_args_pending && id == 0x07 && ee->r[6].u32[0] == 1) {
                uint32_t argv = ee->r[7].u32[0];
                uint32_t str = bus_read32(ee, argv);

                uint32_t room = 0;

                while (bus_read8(ee, str + room))
                    room++;

                uint32_t needed = 0;

                for (int k = 0; k < ee->boot_argc; k++)
                    needed += (uint32_t)strlen(ee->boot_args[k]) + 1;

                if (needed <= room) {
                    uint32_t at = str;

                    for (int k = 0; k < ee->boot_argc; k++) {
                        bus_write32(ee, argv + k * 4, at);

                        for (size_t m = 0; m <= strlen(ee->boot_args[k]); m++)
                            bus_write8(ee, at + (uint32_t)m, ee->boot_args[k][m]);

                        at += (uint32_t)strlen(ee->boot_args[k]) + 1;
                    }

                    ee->r[6].u32[0] = ee->boot_argc;
                }

                ee->boot_args_pending = 0;
            }

            // Note: This prevents keeping stale cache blocks
            //       stored in memory when switching games/software.
            ee->pending_purge = true;
        } break;

        // FlushCache
        case 0x64: {
            // iris_debug(ee, "Flushed {} blocks", ee->block_cache.size());
        } break;
    }

    exception_level1(ee, CAUSE_EXC1_SYS);
}
static inline void i_teq(Ee* ee, const Instruction& i) {
    if (RS == RT) exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void i_teqi(Ee* ee, const Instruction& i) {
    if (RS == SE6416(D_I16)) exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void i_tge(Ee* ee, const Instruction& i) {
    if (RS >= RT) exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void i_tgei(Ee* ee, const Instruction& i) { iris_fatal_error(ee, "tgei unimplemented"); }
static inline void i_tgeiu(Ee* ee, const Instruction& i) { iris_fatal_error(ee, "tgeiu unimplemented"); }
static inline void i_tgeu(Ee* ee, const Instruction& i) { iris_fatal_error(ee, "tgeu unimplemented"); }
static inline void i_tlbp(Ee* ee, const Instruction& i) {
    int index = ee->index & 0x3f;

    VtlbEntry* entry = &ee->vtlb[index];

    if ((ee->entryhi & 0xffffe000) == entry->vpn2 && (ee->entryhi & 0xff) == entry->asid) {
        ee->index |= 0x80000000;
    } else {
        ee->index &= ~0x80000000;
    }
}
static inline void i_tlbr(Ee* ee, const Instruction& i) {
    int index = ee->index & 0x3f;

    VtlbEntry* entry = &ee->vtlb[index];

    ee->entryhi = entry->vpn2 | entry->asid;
    ee->entrylo0 = (entry->pfn0 >> 6) | (entry->v0 << 1) | (entry->d0 << 2) | (entry->c0 << 3) | (entry->s << 31) | (entry->g);
    ee->entrylo1 = (entry->pfn1 >> 6) | (entry->v1 << 1) | (entry->d1 << 2) | (entry->c1 << 3) | (entry->s << 31) | (entry->g);
    ee->pagemask = entry->mask;
}
static inline void write_pagetable(Ee* ee, const VtlbEntry* entry) {
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

    uint32_t vpn0 = entry->vpn2 / MIN_PAGESIZE;
    uint32_t vpn1 = vpn0 + pagecount;
    uint32_t pfn0 = entry->pfn0 / MIN_PAGESIZE;
    uint32_t pfn1 = entry->pfn1 / MIN_PAGESIZE;

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

    flush_cache(ee);
}
static inline void i_tlbwi(Ee* ee, const Instruction& i) {
    VtlbEntry* entry = &ee->vtlb[ee->index & 0x3f];

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

    write_pagetable(ee, entry);

    // iris_debug(ee, "Index={} vpn2={:08x} even={pfn={:08x} v={} d={}} odd={pfn={:08x} v={} d={}} mask={:08x} s={} g={}", //     ee->index,
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
    //);
}
static inline void i_tlbwr(Ee* ee, const Instruction& i) {
    int index = (ee->count % (48 - ee->wired)) + ee->wired;

    VtlbEntry* entry = &ee->vtlb[index];

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

    write_pagetable(ee, entry);

    // iris_debug(ee, "tlbwr Index={} vpn2={:08x} even={pfn={:08x} v={} d={}} odd={pfn={:08x} v={} d={}} mask={:08x} s={} g={}", //     index,
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
    //);
}
static inline void i_tlt(Ee* ee, const Instruction& i) { iris_fatal_error(ee, "tlt unimplemented"); }
static inline void i_tlti(Ee* ee, const Instruction& i) { iris_fatal_error(ee, "tlti unimplemented"); }
static inline void i_tltiu(Ee* ee, const Instruction& i) { iris_fatal_error(ee, "tltiu unimplemented"); }
static inline void i_tltu(Ee* ee, const Instruction& i) { iris_fatal_error(ee, "tltu unimplemented"); }
static inline void i_tne(Ee* ee, const Instruction& i) {
    if (RS != RT) exception_level1(ee, CAUSE_EXC1_TR);
}
static inline void i_tnei(Ee* ee, const Instruction& i) { iris_fatal_error(ee, "tnei unimplemented"); }
static inline void i_vabs(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(abs) }
static inline void i_vadd(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(add) }
static inline void i_vadda(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(adda) }
static inline void i_vaddai(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addai) }
static inline void i_vaddaq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addaq) }
static inline void i_vaddaw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addaw) }
static inline void i_vaddax(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addax) }
static inline void i_vadday(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(adday) }
static inline void i_vaddaz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addaz) }
static inline void i_vaddi(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addi) }
static inline void i_vaddq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addq) }
static inline void i_vaddw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addw) }
static inline void i_vaddx(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addx) }
static inline void i_vaddy(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addy) }
static inline void i_vaddz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(addz) }
static inline void i_vcallms(Ee* ee, const Instruction& i) {
    vu::execute_program(ee->vu0, D_I15);
}
static inline void i_vcallmsr(Ee* ee, const Instruction& i) {
    vu::execute_program(ee->vu0, ee->vu0->cmsar0);
}
static inline void i_vclipw(Ee* ee, const Instruction& i) { VU_UPPER(clip) }
static inline void i_vdiv(Ee* ee, const Instruction& i) { VU_LOWER(div) ee->vu0->q_delay = 0; }
static inline void i_vftoi0(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(ftoi0) }
static inline void i_vftoi12(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(ftoi12) }
static inline void i_vftoi15(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(ftoi15) }
static inline void i_vftoi4(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(ftoi4) }
static inline void i_viadd(Ee* ee, const Instruction& i) { VU_LOWER(iadd) }
static inline void i_viaddi(Ee* ee, const Instruction& i) { VU_LOWER(iaddi) }
static inline void i_viand(Ee* ee, const Instruction& i) { VU_LOWER(iand) }
static inline void i_vilwr(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(ilwr) }
static inline void i_vior(Ee* ee, const Instruction& i) { VU_LOWER(ior) }
static inline void i_visub(Ee* ee, const Instruction& i) { VU_LOWER(isub) }
static inline void i_viswr(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(iswr) }
static inline void i_vitof0(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(itof0) }
static inline void i_vitof12(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(itof12) }
static inline void i_vitof15(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(itof15) }
static inline void i_vitof4(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(itof4) }
static inline void i_vlqd(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(lqd) }
static inline void i_vlqi(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(lqi) }
static inline void i_vmadd(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(madd) }
static inline void i_vmadda(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(madda) }
static inline void i_vmaddai(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddai) }
static inline void i_vmaddaq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddaq) }
static inline void i_vmaddaw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddaw) }
static inline void i_vmaddax(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddax) }
static inline void i_vmadday(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(madday) }
static inline void i_vmaddaz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddaz) }
static inline void i_vmaddi(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddi) }
static inline void i_vmaddq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddq) }
static inline void i_vmaddw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddw) }
static inline void i_vmaddx(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddx) }
static inline void i_vmaddy(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddy) }
static inline void i_vmaddz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maddz) }
static inline void i_vmax(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(max) }
static inline void i_vmaxi(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maxi) }
static inline void i_vmaxw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maxw) }
static inline void i_vmaxx(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maxx) }
static inline void i_vmaxy(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maxy) }
static inline void i_vmaxz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(maxz) }
static inline void i_vmfir(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mfir) }
static inline void i_vmini(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mini) }
static inline void i_vminii(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(minii) }
static inline void i_vminiw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(miniw) }
static inline void i_vminix(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(minix) }
static inline void i_vminiy(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(miniy) }
static inline void i_vminiz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(miniz) }
static inline void i_vmove(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(move) }
static inline void i_vmr32(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(mr32) }
static inline void i_vmsub(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msub) }
static inline void i_vmsuba(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msuba) }
static inline void i_vmsubai(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubai) }
static inline void i_vmsubaq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubaq) }
static inline void i_vmsubaw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubaw) }
static inline void i_vmsubax(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubax) }
static inline void i_vmsubay(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubay) }
static inline void i_vmsubaz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubaz) }
static inline void i_vmsubi(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubi) }
static inline void i_vmsubq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubq) }
static inline void i_vmsubw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubw) }
static inline void i_vmsubx(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubx) }
static inline void i_vmsuby(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msuby) }
static inline void i_vmsubz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(msubz) }
static inline void i_vmtir(Ee* ee, const Instruction& i) { VU_LOWER(mtir) }
static inline void i_vmul(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mul) }
static inline void i_vmula(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mula) }
static inline void i_vmulai(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulai) }
static inline void i_vmulaq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulaq) }
static inline void i_vmulaw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulaw) }
static inline void i_vmulax(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulax) }
static inline void i_vmulay(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulay) }
static inline void i_vmulaz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulaz) }
static inline void i_vmuli(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(muli) }
static inline void i_vmulq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulq) }
static inline void i_vmulw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulw) }
static inline void i_vmulx(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulx) }
static inline void i_vmuly(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(muly) }
static inline void i_vmulz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(mulz) }
static inline void i_vnop(Ee* ee, const Instruction& i) { VU_UPPER(nop) }
static inline void i_vopmsub(Ee* ee, const Instruction& i) { VU_UPPER(opmsub) }
static inline void i_vopmula(Ee* ee, const Instruction& i) { VU_UPPER(opmula) }
static inline void i_vrget(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(rget) }
static inline void i_vrinit(Ee* ee, const Instruction& i) { VU_LOWER(rinit) }
static inline void i_vrnext(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(rnext) }
static inline void i_vrsqrt(Ee* ee, const Instruction& i) { VU_LOWER(rsqrt) ee->vu0->q_delay = 0; }
static inline void i_vrxor(Ee* ee, const Instruction& i) { VU_LOWER(rxor) }
static inline void i_vsqd(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(sqd) }
static inline void i_vsqi(Ee* ee, const Instruction& i) { VU_LOWER_TEMPLATE(sqi) }
static inline void i_vsqrt(Ee* ee, const Instruction& i) { VU_LOWER(sqrt) ee->vu0->q_delay = 0; }
static inline void i_vsub(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(sub) }
static inline void i_vsuba(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(suba) }
static inline void i_vsubai(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subai) }
static inline void i_vsubaq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subaq) }
static inline void i_vsubaw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subaw) }
static inline void i_vsubax(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subax) }
static inline void i_vsubay(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subay) }
static inline void i_vsubaz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subaz) }
static inline void i_vsubi(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subi) }
static inline void i_vsubq(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subq) }
static inline void i_vsubw(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subw) }
static inline void i_vsubx(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subx) }
static inline void i_vsuby(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(suby) }
static inline void i_vsubz(Ee* ee, const Instruction& i) { VU_UPPER_TEMPLATE(subz) }
static inline void i_vwaitq(Ee* ee, const Instruction& i) { VU_LOWER(waitq) }
static inline void i_xor(Ee* ee, const Instruction& i) {
    RD = RS ^ RT;
}
static inline void i_xori(Ee* ee, const Instruction& i) {
    RT = RS ^ D_I16;
}
static inline void i_invalid(Ee* ee, const Instruction& i) {
    iris_fatal_error(ee, "Invalid instruction {:08x} at PC={:08x}", i.opcode, ee->pc);
}

Ee* create(logger::Logger* logger, int ram_size) {
    Ee* ee = new Ee();

    ee->logger = logger;
    ee->logger_id = logger::register_source(logger, "ee");
    ee->ram_size = ram_size - 1;

    ee->vfast_r = (void**)calloc(VFAST_ENTRIES, sizeof(void*));

    ee->spr = ram::create(logger, 0x4000);
    ee->block_cache.resize(CACHE_PAGECOUNT);
    ee->jit_logger = new asmjit::FileLogger(stdout);

    ee->osd_config.screen_type = 1; // 4:3
    ee->osd_config.ps1drv_config = 0; // ???
    ee->osd_config.spdif_mode = 0; // Enabled
    ee->osd_config.timezone_offset = 0;
    ee->osd_config.video_output = 0; // RGB
    ee->osd_config.jap_language = 1; // Indicates not Japanese
    ee->osd_config.language = 1; // English
    ee->osd_config.version = 1; // Indicates normal kernel without extended language settings

    reset(ee);

    return ee;
}

void connect(Ee* ee, vu::Vu* vu0, vu::Vu* vu1, BusInterface bus) {
    ee->bus = bus;
    ee->vu0 = vu0;
    ee->vu1 = vu1;
}

void reset(Ee* ee) {
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
    ee->pc = VEC_RESET;
    ee->next_pc = ee->pc + 4;
    ee->intc_reads = 0;
    ee->csr_reads = 0;

    purge_cache(ee);

    ee->rt.reset(asmjit::ResetPolicy::kHard);

    fesetround(FE_TOWARDZERO);

    ram::reset(ee->spr);

    ee->fcr = 0x01000001;
}

void destroy(Ee* ee) {
    ram::destroy(ee->spr);

    free(ee->vfast_r);

    delete ee->jit_logger;
    delete ee;
}

Instruction decode(uint32_t opcode) {
    Instruction i;

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
                case 0x00000000: i.cycles = CYC_DEFAULT; i.id = I_SLL; i.func = i_sll; return i;
                case 0x00000002: i.cycles = CYC_DEFAULT; i.id = I_SRL; i.func = i_srl; return i;
                case 0x00000003: i.cycles = CYC_DEFAULT; i.id = I_SRA; i.func = i_sra; return i;
                case 0x00000004: i.cycles = CYC_DEFAULT; i.id = I_SLLV; i.func = i_sllv; return i;
                case 0x00000006: i.cycles = CYC_DEFAULT; i.id = I_SRLV; i.func = i_srlv; return i;
                case 0x00000007: i.cycles = CYC_DEFAULT; i.id = I_SRAV; i.func = i_srav; return i;
                case 0x00000008: i.cycles = CYC_DEFAULT; i.branch = 1; i.id = I_JR; i.func = i_jr; return i;
                case 0x00000009: i.cycles = CYC_DEFAULT; i.branch = 1; i.id = I_JALR; i.func = i_jalr; return i;
                case 0x0000000A: i.cycles = CYC_DEFAULT; i.id = I_MOVZ; i.func = i_movz; return i;
                case 0x0000000B: i.cycles = CYC_DEFAULT; i.id = I_MOVN; i.func = i_movn; return i;
                case 0x0000000C: i.cycles = CYC_DEFAULT; i.branch = 2; i.id = I_SYSCALL; i.func = i_syscall; return i;
                case 0x0000000D: i.cycles = CYC_DEFAULT; i.branch = 2; i.id = I_BREAK; i.func = i_break; return i;
                case 0x0000000F: i.cycles = CYC_DEFAULT; i.id = I_SYNC; i.func = i_sync; return i;
                case 0x00000010: i.cycles = CYC_DEFAULT; i.id = I_MFHI; i.func = i_mfhi; return i;
                case 0x00000011: i.cycles = CYC_DEFAULT; i.id = I_MTHI; i.func = i_mthi; return i;
                case 0x00000012: i.cycles = CYC_DEFAULT; i.id = I_MFLO; i.func = i_mflo; return i;
                case 0x00000013: i.cycles = CYC_DEFAULT; i.id = I_MTLO; i.func = i_mtlo; return i;
                case 0x00000014: i.cycles = CYC_DEFAULT; i.id = I_DSLLV; i.func = i_dsllv; return i;
                case 0x00000016: i.cycles = CYC_DEFAULT; i.id = I_DSRLV; i.func = i_dsrlv; return i;
                case 0x00000017: i.cycles = CYC_DEFAULT; i.id = I_DSRAV; i.func = i_dsrav; return i;
                case 0x00000018: i.cycles = CYC_MULT; i.id = I_MULT; i.func = i_mult; return i;
                case 0x00000019: i.cycles = CYC_MULT; i.id = I_MULTU; i.func = i_multu; return i;
                case 0x0000001A: i.cycles = CYC_DIV; i.id = I_DIV; i.func = i_div; return i;
                case 0x0000001B: i.cycles = CYC_DIV; i.id = I_DIVU; i.func = i_divu; return i;
                case 0x00000020: i.cycles = CYC_DEFAULT; i.id = I_ADD; i.func = i_add; return i;
                case 0x00000021: i.cycles = CYC_DEFAULT; i.id = I_ADDU; i.func = i_addu; return i;
                case 0x00000022: i.cycles = CYC_DEFAULT; i.id = I_SUB; i.func = i_sub; return i;
                case 0x00000023: i.cycles = CYC_DEFAULT; i.id = I_SUBU; i.func = i_subu; return i;
                case 0x00000024: i.cycles = CYC_DEFAULT; i.id = I_AND; i.func = i_and; return i;
                case 0x00000025: i.cycles = CYC_DEFAULT; i.id = I_OR; i.func = i_or; return i;
                case 0x00000026: i.cycles = CYC_DEFAULT; i.id = I_XOR; i.func = i_xor; return i;
                case 0x00000027: i.cycles = CYC_DEFAULT; i.id = I_NOR; i.func = i_nor; return i;
                case 0x00000028: i.cycles = CYC_DEFAULT; i.id = I_MFSA; i.func = i_mfsa; return i;
                case 0x00000029: i.cycles = CYC_DEFAULT; i.id = I_MTSA; i.func = i_mtsa; return i;
                case 0x0000002A: i.cycles = CYC_DEFAULT; i.id = I_SLT; i.func = i_slt; return i;
                case 0x0000002B: i.cycles = CYC_DEFAULT; i.id = I_SLTU; i.func = i_sltu; return i;
                case 0x0000002C: i.cycles = CYC_DEFAULT; i.id = I_DADD; i.func = i_dadd; return i;
                case 0x0000002D: i.cycles = CYC_DEFAULT; i.id = I_DADDU; i.func = i_daddu; return i;
                case 0x0000002E: i.cycles = CYC_DEFAULT; i.id = I_DSUB; i.func = i_dsub; return i;
                case 0x0000002F: i.cycles = CYC_DEFAULT; i.id = I_DSUBU; i.func = i_dsubu; return i;
                case 0x00000030: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TGE; i.func = i_tge; return i;
                case 0x00000031: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TGEU; i.func = i_tgeu; return i;
                case 0x00000032: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TLT; i.func = i_tlt; return i;
                case 0x00000033: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TLTU; i.func = i_tltu; return i;
                case 0x00000034: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TEQ; i.func = i_teq; return i;
                case 0x00000036: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TNE; i.func = i_tne; return i;
                case 0x00000038: i.cycles = CYC_DEFAULT; i.id = I_DSLL; i.func = i_dsll; return i;
                case 0x0000003A: i.cycles = CYC_DEFAULT; i.id = I_DSRL; i.func = i_dsrl; return i;
                case 0x0000003B: i.cycles = CYC_DEFAULT; i.id = I_DSRA; i.func = i_dsra; return i;
                case 0x0000003C: i.cycles = CYC_DEFAULT; i.id = I_DSLL32; i.func = i_dsll32; return i;
                case 0x0000003E: i.cycles = CYC_DEFAULT; i.id = I_DSRL32; i.func = i_dsrl32; return i;
                case 0x0000003F: i.cycles = CYC_DEFAULT; i.id = I_DSRA32; i.func = i_dsra32; return i;
            }
        } break;
        case 0x04000000 >> 26: { // regimm
            switch ((opcode & 0x001F0000) >> 16) {
                case 0x00000000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BLTZ; i.func = i_bltz; return i;
                case 0x00010000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BGEZ; i.func = i_bgez; return i;
                case 0x00020000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BLTZL; i.func = i_bltzl; return i;
                case 0x00030000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BGEZL; i.func = i_bgezl; return i;
                case 0x00080000 >> 16: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TGEI; i.func = i_tgei; return i;
                case 0x00090000 >> 16: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TGEIU; i.func = i_tgeiu; return i;
                case 0x000A0000 >> 16: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TLTI; i.func = i_tlti; return i;
                case 0x000B0000 >> 16: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TLTIU; i.func = i_tltiu; return i;
                case 0x000C0000 >> 16: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TEQI; i.func = i_teqi; return i;
                case 0x000E0000 >> 16: i.cycles = CYC_BRANCH; i.branch = 4; i.id = I_TNEI; i.func = i_tnei; return i;
                case 0x00100000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BLTZAL; i.func = i_bltzal; return i;
                case 0x00110000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BGEZAL; i.func = i_bgezal; return i;
                case 0x00120000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BLTZALL; i.func = i_bltzall; return i;
                case 0x00130000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BGEZALL; i.func = i_bgezall; return i;
                case 0x00180000 >> 16: i.cycles = CYC_DEFAULT; i.id = I_MTSAB; i.func = i_mtsab; return i;
                case 0x00190000 >> 16: i.cycles = CYC_DEFAULT; i.id = I_MTSAH; i.func = i_mtsah; return i;
            }
        } break;
        case 0x08000000 >> 26: i.cycles = CYC_DEFAULT; i.branch = 1; i.id = I_J; i.func = i_j; return i;
        case 0x0C000000 >> 26: i.cycles = CYC_DEFAULT; i.branch = 1; i.id = I_JAL; i.func = i_jal; return i;
        case 0x10000000 >> 26: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BEQ; i.func = i_beq; return i;
        case 0x14000000 >> 26: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BNE; i.func = i_bne; return i;
        case 0x18000000 >> 26: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BLEZ; i.func = i_blez; return i;
        case 0x1C000000 >> 26: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BGTZ; i.func = i_bgtz; return i;
        case 0x20000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_ADDI; i.func = i_addi; return i;
        case 0x24000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_ADDIU; i.func = i_addiu; return i;
        case 0x28000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_SLTI; i.func = i_slti; return i;
        case 0x2C000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_SLTIU; i.func = i_sltiu; return i;
        case 0x30000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_ANDI; i.func = i_andi; return i;
        case 0x34000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_ORI; i.func = i_ori; return i;
        case 0x38000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_XORI; i.func = i_xori; return i;
        case 0x3C000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_LUI; i.func = i_lui; return i;
        case 0x40000000 >> 26: { // cop0
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00000000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_MFC0; i.func = i_mfc0; return i;
                case 0x00800000 >> 21: if (i.rd.r == 12) i.branch = 2; i.cycles = CYC_COP_DEFAULT; i.id = I_MTC0; i.func = i_mtc0; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BC0F; i.func = i_bc0f; return i;
                        case 0x00010000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BC0T; i.func = i_bc0t; return i;
                        case 0x00020000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BC0FL; i.func = i_bc0fl; return i;
                        case 0x00030000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BC0TL; i.func = i_bc0tl; return i;
                    }
                } break;
                case 0x02000000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000001: i.cycles = CYC_COP_DEFAULT; i.id = I_TLBR; i.func = i_tlbr; return i;
                        case 0x00000002: i.cycles = CYC_COP_DEFAULT; i.branch = 2; i.id = I_TLBWI; i.func = i_tlbwi; return i;
                        case 0x00000006: i.cycles = CYC_COP_DEFAULT; i.branch = 2; i.id = I_TLBWR; i.func = i_tlbwr; return i;
                        case 0x00000008: i.cycles = CYC_COP_DEFAULT; i.id = I_TLBP; i.func = i_tlbp; return i;
                        case 0x00000018: i.cycles = CYC_COP_DEFAULT; i.branch = 2; i.id = I_ERET; i.func = i_eret; return i;
                        case 0x00000038: i.cycles = CYC_COP_DEFAULT; i.branch = 2; i.id = I_EI; i.func = i_ei; return i;
                        case 0x00000039: i.cycles = CYC_COP_DEFAULT; i.branch = 2; i.id = I_DI; i.func = i_di; return i;
                    }
                } break;
            }
        } break;
        case 0x44000000 >> 26: { // cop1
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00000000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_MFC1; i.func = i_mfc1; return i;
                case 0x00400000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_CFC1; i.func = i_cfc1; return i;
                case 0x00800000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_MTC1; i.func = i_mtc1; return i;
                case 0x00C00000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_CTC1; i.func = i_ctc1; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BC1F; i.func = i_bc1f; return i;
                        case 0x00010000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BC1T; i.func = i_bc1t; return i;
                        case 0x00020000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BC1FL; i.func = i_bc1fl; return i;
                        case 0x00030000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BC1TL; i.func = i_bc1tl; return i;
                    }
                } break;
                case 0x02000000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000000: i.cycles = CYC_COP_DEFAULT; i.id = I_ADDS; i.func = i_adds; return i;
                        case 0x00000001: i.cycles = CYC_COP_DEFAULT; i.id = I_SUBS; i.func = i_subs; return i;
                        case 0x00000002: i.cycles = CYC_FPU_MULT; i.id = I_MULS; i.func = i_muls; return i;
                        case 0x00000003: i.cycles = CYC_FPU_DIV; i.id = I_DIVS; i.func = i_divs; return i;
                        case 0x00000004: i.cycles = CYC_FPU_DIV; i.id = I_SQRTS; i.func = i_sqrts; return i;
                        case 0x00000005: i.cycles = CYC_COP_DEFAULT; i.id = I_ABSS; i.func = i_abss; return i;
                        case 0x00000006: i.cycles = CYC_COP_DEFAULT; i.id = I_MOVS; i.func = i_movs; return i;
                        case 0x00000007: i.cycles = CYC_COP_DEFAULT; i.id = I_NEGS; i.func = i_negs; return i;
                        case 0x00000016: i.cycles = CYC_FPU_DIV; i.id = I_RSQRTS; i.func = i_rsqrts; return i;
                        case 0x00000018: i.cycles = CYC_COP_DEFAULT; i.id = I_ADDAS; i.func = i_addas; return i;
                        case 0x00000019: i.cycles = CYC_COP_DEFAULT; i.id = I_SUBAS; i.func = i_subas; return i;
                        case 0x0000001A: i.cycles = CYC_FPU_MULT; i.id = I_MULAS; i.func = i_mulas; return i;
                        case 0x0000001C: i.cycles = CYC_FPU_MULT; i.id = I_MADDS; i.func = i_madds; return i;
                        case 0x0000001D: i.cycles = CYC_FPU_MULT; i.id = I_MSUBS; i.func = i_msubs; return i;
                        case 0x0000001E: i.cycles = CYC_FPU_MULT; i.id = I_MADDAS; i.func = i_maddas; return i;
                        case 0x0000001F: i.cycles = CYC_FPU_MULT; i.id = I_MSUBAS; i.func = i_msubas; return i;
                        case 0x00000024: i.cycles = CYC_COP_DEFAULT; i.id = I_CVTW; i.func = i_cvtw; return i;
                        case 0x00000028: i.cycles = CYC_COP_DEFAULT; i.id = I_MAXS; i.func = i_maxs; return i;
                        case 0x00000029: i.cycles = CYC_COP_DEFAULT; i.id = I_MINS; i.func = i_mins; return i;
                        case 0x00000030: i.cycles = CYC_COP_DEFAULT; i.id = I_CF; i.func = i_cf; return i;
                        case 0x00000032: i.cycles = CYC_COP_DEFAULT; i.id = I_CEQ; i.func = i_ceq; return i;
                        case 0x00000034: i.cycles = CYC_COP_DEFAULT; i.id = I_CLT; i.func = i_clt; return i;
                        case 0x00000036: i.cycles = CYC_COP_DEFAULT; i.id = I_CLE; i.func = i_cle; return i;
                    }
                } break;
                case 0x02800000 >> 21: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000020: i.cycles = CYC_COP_DEFAULT; i.id = I_CVTS; i.func = i_cvts; return i;
                    }
                } break;
            }
        } break;
        case 0x48000000 >> 26: { // cop2
            switch ((opcode & 0x03E00000) >> 21) {
                case 0x00200000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_QMFC2; i.func = i_qmfc2; return i;
                case 0x00400000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_CFC2; i.func = i_cfc2; return i;
                case 0x00A00000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_QMTC2; i.func = i_qmtc2; return i;
                case 0x00C00000 >> 21: i.cycles = CYC_COP_DEFAULT; i.id = I_CTC2; i.func = i_ctc2; return i;
                case 0x01000000 >> 21: {
                    switch ((opcode & 0x001F0000) >> 16) {
                        case 0x00000000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BC2F; i.func = i_bc2f; return i;
                        case 0x00010000 >> 16: i.cycles = CYC_BRANCH; i.branch = 1; i.id = I_BC2T; i.func = i_bc2t; return i;
                        case 0x00020000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BC2FL; i.func = i_bc2fl; return i;
                        case 0x00030000 >> 16: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BC2TL; i.func = i_bc2tl; return i;
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
                        case 0x00000000: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDX; i.func = i_vaddx; return i;
                        case 0x00000001: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDY; i.func = i_vaddy; return i;
                        case 0x00000002: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDZ; i.func = i_vaddz; return i;
                        case 0x00000003: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDW; i.func = i_vaddw; return i;
                        case 0x00000004: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBX; i.func = i_vsubx; return i;
                        case 0x00000005: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBY; i.func = i_vsuby; return i;
                        case 0x00000006: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBZ; i.func = i_vsubz; return i;
                        case 0x00000007: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBW; i.func = i_vsubw; return i;
                        case 0x00000008: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDX; i.func = i_vmaddx; return i;
                        case 0x00000009: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDY; i.func = i_vmaddy; return i;
                        case 0x0000000A: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDZ; i.func = i_vmaddz; return i;
                        case 0x0000000B: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDW; i.func = i_vmaddw; return i;
                        case 0x0000000C: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBX; i.func = i_vmsubx; return i;
                        case 0x0000000D: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBY; i.func = i_vmsuby; return i;
                        case 0x0000000E: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBZ; i.func = i_vmsubz; return i;
                        case 0x0000000F: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBW; i.func = i_vmsubw; return i;
                        case 0x00000010: i.cycles = CYC_COP_DEFAULT; i.id = I_VMAXX; i.func = i_vmaxx; return i;
                        case 0x00000011: i.cycles = CYC_COP_DEFAULT; i.id = I_VMAXY; i.func = i_vmaxy; return i;
                        case 0x00000012: i.cycles = CYC_COP_DEFAULT; i.id = I_VMAXZ; i.func = i_vmaxz; return i;
                        case 0x00000013: i.cycles = CYC_COP_DEFAULT; i.id = I_VMAXW; i.func = i_vmaxw; return i;
                        case 0x00000014: i.cycles = CYC_COP_DEFAULT; i.id = I_VMINIX; i.func = i_vminix; return i;
                        case 0x00000015: i.cycles = CYC_COP_DEFAULT; i.id = I_VMINIY; i.func = i_vminiy; return i;
                        case 0x00000016: i.cycles = CYC_COP_DEFAULT; i.id = I_VMINIZ; i.func = i_vminiz; return i;
                        case 0x00000017: i.cycles = CYC_COP_DEFAULT; i.id = I_VMINIW; i.func = i_vminiw; return i;
                        case 0x00000018: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULX; i.func = i_vmulx; return i;
                        case 0x00000019: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULY; i.func = i_vmuly; return i;
                        case 0x0000001A: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULZ; i.func = i_vmulz; return i;
                        case 0x0000001B: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULW; i.func = i_vmulw; return i;
                        case 0x0000001C: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULQ; i.func = i_vmulq; return i;
                        case 0x0000001D: i.cycles = CYC_COP_DEFAULT; i.id = I_VMAXI; i.func = i_vmaxi; return i;
                        case 0x0000001E: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULI; i.func = i_vmuli; return i;
                        case 0x0000001F: i.cycles = CYC_COP_DEFAULT; i.id = I_VMINII; i.func = i_vminii; return i;
                        case 0x00000020: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDQ; i.func = i_vaddq; return i;
                        case 0x00000021: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDQ; i.func = i_vmaddq; return i;
                        case 0x00000022: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDI; i.func = i_vaddi; return i;
                        case 0x00000023: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDI; i.func = i_vmaddi; return i;
                        case 0x00000024: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBQ; i.func = i_vsubq; return i;
                        case 0x00000025: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBQ; i.func = i_vmsubq; return i;
                        case 0x00000026: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBI; i.func = i_vsubi; return i;
                        case 0x00000027: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBI; i.func = i_vmsubi; return i;
                        case 0x00000028: i.cycles = CYC_COP_DEFAULT; i.id = I_VADD; i.func = i_vadd; return i;
                        case 0x00000029: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADD; i.func = i_vmadd; return i;
                        case 0x0000002A: i.cycles = CYC_COP_DEFAULT; i.id = I_VMUL; i.func = i_vmul; return i;
                        case 0x0000002B: i.cycles = CYC_COP_DEFAULT; i.id = I_VMAX; i.func = i_vmax; return i;
                        case 0x0000002C: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUB; i.func = i_vsub; return i;
                        case 0x0000002D: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUB; i.func = i_vmsub; return i;
                        case 0x0000002E: i.cycles = CYC_COP_DEFAULT; i.id = I_VOPMSUB; i.func = i_vopmsub; return i;
                        case 0x0000002F: i.cycles = CYC_COP_DEFAULT; i.id = I_VMINI; i.func = i_vmini; return i;
                        case 0x00000030: i.cycles = CYC_COP_DEFAULT; i.id = I_VIADD; i.func = i_viadd; return i;
                        case 0x00000031: i.cycles = CYC_COP_DEFAULT; i.id = I_VISUB; i.func = i_visub; return i;
                        case 0x00000032: i.cycles = CYC_COP_DEFAULT; i.id = I_VIADDI; i.func = i_viaddi; return i;
                        case 0x00000034: i.cycles = CYC_COP_DEFAULT; i.id = I_VIAND; i.func = i_viand; return i;
                        case 0x00000035: i.cycles = CYC_COP_DEFAULT; i.id = I_VIOR; i.func = i_vior; return i;
                        case 0x00000038: i.cycles = CYC_COP_DEFAULT; i.id = I_VCALLMS; i.func = i_vcallms; return i;
                        case 0x00000039: i.cycles = CYC_COP_DEFAULT; i.id = I_VCALLMSR; i.func = i_vcallmsr; return i;
                        case 0x0000003C:
                        case 0x0000003D:
                        case 0x0000003E:
                        case 0x0000003F: {
                            uint32_t func = (opcode & 3) | ((opcode & 0x7c0) >> 4);

                            switch (func) {
                                case 0x00000000: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDAX; i.func = i_vaddax; return i;
                                case 0x00000001: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDAY; i.func = i_vadday; return i;
                                case 0x00000002: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDAZ; i.func = i_vaddaz; return i;
                                case 0x00000003: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDAW; i.func = i_vaddaw; return i;
                                case 0x00000004: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBAX; i.func = i_vsubax; return i;
                                case 0x00000005: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBAY; i.func = i_vsubay; return i;
                                case 0x00000006: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBAZ; i.func = i_vsubaz; return i;
                                case 0x00000007: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBAW; i.func = i_vsubaw; return i;
                                case 0x00000008: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDAX; i.func = i_vmaddax; return i;
                                case 0x00000009: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDAY; i.func = i_vmadday; return i;
                                case 0x0000000A: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDAZ; i.func = i_vmaddaz; return i;
                                case 0x0000000B: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDAW; i.func = i_vmaddaw; return i;
                                case 0x0000000C: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBAX; i.func = i_vmsubax; return i;
                                case 0x0000000D: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBAY; i.func = i_vmsubay; return i;
                                case 0x0000000E: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBAZ; i.func = i_vmsubaz; return i;
                                case 0x0000000F: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBAW; i.func = i_vmsubaw; return i;
                                case 0x00000010: i.cycles = CYC_COP_DEFAULT; i.id = I_VITOF0; i.func = i_vitof0; return i;
                                case 0x00000011: i.cycles = CYC_COP_DEFAULT; i.id = I_VITOF4; i.func = i_vitof4; return i;
                                case 0x00000012: i.cycles = CYC_COP_DEFAULT; i.id = I_VITOF12; i.func = i_vitof12; return i;
                                case 0x00000013: i.cycles = CYC_COP_DEFAULT; i.id = I_VITOF15; i.func = i_vitof15; return i;
                                case 0x00000014: i.cycles = CYC_COP_DEFAULT; i.id = I_VFTOI0; i.func = i_vftoi0; return i;
                                case 0x00000015: i.cycles = CYC_COP_DEFAULT; i.id = I_VFTOI4; i.func = i_vftoi4; return i;
                                case 0x00000016: i.cycles = CYC_COP_DEFAULT; i.id = I_VFTOI12; i.func = i_vftoi12; return i;
                                case 0x00000017: i.cycles = CYC_COP_DEFAULT; i.id = I_VFTOI15; i.func = i_vftoi15; return i;
                                case 0x00000018: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULAX; i.func = i_vmulax; return i;
                                case 0x00000019: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULAY; i.func = i_vmulay; return i;
                                case 0x0000001A: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULAZ; i.func = i_vmulaz; return i;
                                case 0x0000001B: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULAW; i.func = i_vmulaw; return i;
                                case 0x0000001C: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULAQ; i.func = i_vmulaq; return i;
                                case 0x0000001D: i.cycles = CYC_COP_DEFAULT; i.id = I_VABS; i.func = i_vabs; return i;
                                case 0x0000001E: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULAI; i.func = i_vmulai; return i;
                                case 0x0000001F: i.cycles = CYC_COP_DEFAULT; i.id = I_VCLIPW; i.func = i_vclipw; return i;
                                case 0x00000020: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDAQ; i.func = i_vaddaq; return i;
                                case 0x00000021: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDAQ; i.func = i_vmaddaq; return i;
                                case 0x00000022: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDAI; i.func = i_vaddai; return i;
                                case 0x00000023: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDAI; i.func = i_vmaddai; return i;
                                case 0x00000024: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBAQ; i.func = i_vsubaq; return i;
                                case 0x00000025: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBAQ; i.func = i_vmsubaq; return i;
                                case 0x00000026: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBAI; i.func = i_vsubai; return i;
                                case 0x00000027: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBAI; i.func = i_vmsubai; return i;
                                case 0x00000028: i.cycles = CYC_COP_DEFAULT; i.id = I_VADDA; i.func = i_vadda; return i;
                                case 0x00000029: i.cycles = CYC_COP_DEFAULT; i.id = I_VMADDA; i.func = i_vmadda; return i;
                                case 0x0000002A: i.cycles = CYC_COP_DEFAULT; i.id = I_VMULA; i.func = i_vmula; return i;
                                case 0x0000002C: i.cycles = CYC_COP_DEFAULT; i.id = I_VSUBA; i.func = i_vsuba; return i;
                                case 0x0000002D: i.cycles = CYC_COP_DEFAULT; i.id = I_VMSUBA; i.func = i_vmsuba; return i;
                                case 0x0000002E: i.cycles = CYC_COP_DEFAULT; i.id = I_VOPMULA; i.func = i_vopmula; return i;
                                case 0x0000002F: i.cycles = CYC_COP_DEFAULT; i.id = I_VNOP; i.func = i_vnop; return i;
                                case 0x00000030: i.cycles = CYC_COP_DEFAULT; i.id = I_VMOVE; i.func = i_vmove; return i;
                                case 0x00000031: i.cycles = CYC_COP_DEFAULT; i.id = I_VMR32; i.func = i_vmr32; return i;
                                case 0x00000034: i.cycles = CYC_COP_DEFAULT; i.id = I_VLQI; i.func = i_vlqi; return i;
                                case 0x00000035: i.cycles = CYC_COP_DEFAULT; i.id = I_VSQI; i.func = i_vsqi; return i;
                                case 0x00000036: i.cycles = CYC_COP_DEFAULT; i.id = I_VLQD; i.func = i_vlqd; return i;
                                case 0x00000037: i.cycles = CYC_COP_DEFAULT; i.id = I_VSQD; i.func = i_vsqd; return i;
                                case 0x00000038: i.cycles = CYC_COP_DEFAULT; i.id = I_VDIV; i.func = i_vdiv; return i;
                                case 0x00000039: i.cycles = CYC_COP_DEFAULT; i.id = I_VSQRT; i.func = i_vsqrt; return i;
                                case 0x0000003A: i.cycles = CYC_COP_DEFAULT; i.id = I_VRSQRT; i.func = i_vrsqrt; return i;
                                case 0x0000003B: i.cycles = CYC_COP_DEFAULT; i.id = I_VWAITQ; i.func = i_vwaitq; return i;
                                case 0x0000003C: i.cycles = CYC_COP_DEFAULT; i.id = I_VMTIR; i.func = i_vmtir; return i;
                                case 0x0000003D: i.cycles = CYC_COP_DEFAULT; i.id = I_VMFIR; i.func = i_vmfir; return i;
                                case 0x0000003E: i.cycles = CYC_COP_DEFAULT; i.id = I_VILWR; i.func = i_vilwr; return i;
                                case 0x0000003F: i.cycles = CYC_COP_DEFAULT; i.id = I_VISWR; i.func = i_viswr; return i;
                                case 0x00000040: i.cycles = CYC_COP_DEFAULT; i.id = I_VRNEXT; i.func = i_vrnext; return i;
                                case 0x00000041: i.cycles = CYC_COP_DEFAULT; i.id = I_VRGET; i.func = i_vrget; return i;
                                case 0x00000042: i.cycles = CYC_COP_DEFAULT; i.id = I_VRINIT; i.func = i_vrinit; return i;
                                case 0x00000043: i.cycles = CYC_COP_DEFAULT; i.id = I_VRXOR; i.func = i_vrxor; return i;
                            }
                        } break;
                    }
                } break;
            }
        } break;
        case 0x50000000 >> 26: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BEQL; i.func = i_beql; return i;
        case 0x54000000 >> 26: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BNEL; i.func = i_bnel; return i;
        case 0x58000000 >> 26: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BLEZL; i.func = i_blezl; return i;
        case 0x5C000000 >> 26: i.cycles = CYC_BRANCH; i.branch = 3; i.id = I_BGTZL; i.func = i_bgtzl; return i;
        case 0x60000000 >> 26: i.cycles = CYC_DEFAULT; i.branch = 4; i.id = I_DADDI; i.func = i_daddi; return i;
        case 0x64000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_DADDIU; i.func = i_daddiu; return i;
        case 0x68000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LDL; i.func = i_ldl; return i;
        case 0x6C000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LDR; i.func = i_ldr; return i;
        case 0x70000000 >> 26: { // mmi
            switch (opcode & 0x0000003F) {
                case 0x00000000: i.cycles = CYC_MULT; i.id = I_MADD; i.func = i_madd; return i;
                case 0x00000001: i.cycles = CYC_MULT; i.id = I_MADDU; i.func = i_maddu; return i;
                case 0x00000004: i.cycles = CYC_MMI_DEFAULT; i.id = I_PLZCW; i.func = i_plzcw; return i;
                case 0x00000008: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDW; i.func = i_paddw; return i;
                        case 0x00000040 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBW; i.func = i_psubw; return i;
                        case 0x00000080 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCGTW; i.func = i_pcgtw; return i;
                        case 0x000000C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMAXW; i.func = i_pmaxw; return i;
                        case 0x00000100 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDH; i.func = i_paddh; return i;
                        case 0x00000140 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBH; i.func = i_psubh; return i;
                        case 0x00000180 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCGTH; i.func = i_pcgth; return i;
                        case 0x000001C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMAXH; i.func = i_pmaxh; return i;
                        case 0x00000200 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDB; i.func = i_paddb; return i;
                        case 0x00000240 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBB; i.func = i_psubb; return i;
                        case 0x00000280 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCGTB; i.func = i_pcgtb; return i;
                        case 0x00000400 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDSW; i.func = i_paddsw; return i;
                        case 0x00000440 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBSW; i.func = i_psubsw; return i;
                        case 0x00000480 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXTLW; i.func = i_pextlw; return i;
                        case 0x000004C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PPACW; i.func = i_ppacw; return i;
                        case 0x00000500 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDSH; i.func = i_paddsh; return i;
                        case 0x00000540 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBSH; i.func = i_psubsh; return i;
                        case 0x00000580 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXTLH; i.func = i_pextlh; return i;
                        case 0x000005C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PPACH; i.func = i_ppach; return i;
                        case 0x00000600 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDSB; i.func = i_paddsb; return i;
                        case 0x00000640 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBSB; i.func = i_psubsb; return i;
                        case 0x00000680 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXTLB; i.func = i_pextlb; return i;
                        case 0x000006C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PPACB; i.func = i_ppacb; return i;
                        case 0x00000780 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXT5; i.func = i_pext5; return i;
                        case 0x000007C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PPAC5; i.func = i_ppac5; return i;
                    }
                } break;
                case 0x00000009: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PMADDW; i.func = i_pmaddw; return i;
                        case 0x00000080 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSLLVW; i.func = i_psllvw; return i;
                        case 0x000000C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSRLVW; i.func = i_psrlvw; return i;
                        case 0x00000100 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PMSUBW; i.func = i_pmsubw; return i;
                        case 0x00000200 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMFHI; i.func = i_pmfhi; return i;
                        case 0x00000240 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMFLO; i.func = i_pmflo; return i;
                        case 0x00000280 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PINTH; i.func = i_pinth; return i;
                        case 0x00000300 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PMULTW; i.func = i_pmultw; return i;
                        case 0x00000340 >> 6: i.cycles = CYC_MMI_DIV; i.id = I_PDIVW; i.func = i_pdivw; return i;
                        case 0x00000380 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCPYLD; i.func = i_pcpyld; return i;
                        case 0x00000400 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PMADDH; i.func = i_pmaddh; return i;
                        case 0x00000440 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PHMADH; i.func = i_phmadh; return i;
                        case 0x00000480 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PAND; i.func = i_pand; return i;
                        case 0x000004C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PXOR; i.func = i_pxor; return i;
                        case 0x00000500 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PMSUBH; i.func = i_pmsubh; return i;
                        case 0x00000540 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PHMSBH; i.func = i_phmsbh; return i;
                        case 0x00000680 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXEH; i.func = i_pexeh; return i;
                        case 0x000006C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PREVH; i.func = i_prevh; return i;
                        case 0x00000700 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PMULTH; i.func = i_pmulth; return i;
                        case 0x00000740 >> 6: i.cycles = CYC_MMI_DIV; i.id = I_PDIVBW; i.func = i_pdivbw; return i;
                        case 0x00000780 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXEW; i.func = i_pexew; return i;
                        case 0x000007C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PROT3W; i.func = i_prot3w; return i;
                    }
                } break;
                case 0x00000010: i.cycles = CYC_COP_DEFAULT; i.id = I_MFHI1; i.func = i_mfhi1; return i;
                case 0x00000011: i.cycles = CYC_COP_DEFAULT; i.id = I_MTHI1; i.func = i_mthi1; return i;
                case 0x00000012: i.cycles = CYC_COP_DEFAULT; i.id = I_MFLO1; i.func = i_mflo1; return i;
                case 0x00000013: i.cycles = CYC_COP_DEFAULT; i.id = I_MTLO1; i.func = i_mtlo1; return i;
                case 0x00000018: i.cycles = CYC_MULT; i.id = I_MULT1; i.func = i_mult1; return i;
                case 0x00000019: i.cycles = CYC_MULT; i.id = I_MULTU1; i.func = i_multu1; return i;
                case 0x0000001A: i.cycles = CYC_DIV; i.id = I_DIV1; i.func = i_div1; return i;
                case 0x0000001B: i.cycles = CYC_DIV; i.id = I_DIVU1; i.func = i_divu1; return i;
                case 0x00000020: i.cycles = CYC_MULT; i.id = I_MADD1; i.func = i_madd1; return i;
                case 0x00000021: i.cycles = CYC_MULT; i.id = I_MADDU1; i.func = i_maddu1; return i;
                case 0x00000028: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000040 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PABSW; i.func = i_pabsw; return i;
                        case 0x00000080 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCEQW; i.func = i_pceqw; return i;
                        case 0x000000C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMINW; i.func = i_pminw; return i;
                        case 0x00000100 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADSBH; i.func = i_padsbh; return i;
                        case 0x00000140 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PABSH; i.func = i_pabsh; return i;
                        case 0x00000180 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCEQH; i.func = i_pceqh; return i;
                        case 0x000001C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMINH; i.func = i_pminh; return i;
                        case 0x00000280 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCEQB; i.func = i_pceqb; return i;
                        case 0x00000400 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDUW; i.func = i_padduw; return i;
                        case 0x00000440 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBUW; i.func = i_psubuw; return i;
                        case 0x00000480 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXTUW; i.func = i_pextuw; return i;
                        case 0x00000500 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDUH; i.func = i_padduh; return i;
                        case 0x00000540 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBUH; i.func = i_psubuh; return i;
                        case 0x00000580 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXTUH; i.func = i_pextuh; return i;
                        case 0x00000600 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PADDUB; i.func = i_paddub; return i;
                        case 0x00000640 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSUBUB; i.func = i_psubub; return i;
                        case 0x00000680 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXTUB; i.func = i_pextub; return i;
                        case 0x000006C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_QFSRV; i.func = i_qfsrv; return i;
                    }
                } break;
                case 0x00000029: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PMADDUW; i.func = i_pmadduw; return i;
                        case 0x000000C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSRAVW; i.func = i_psravw; return i;
                        case 0x00000200 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMTHI; i.func = i_pmthi; return i;
                        case 0x00000240 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMTLO; i.func = i_pmtlo; return i;
                        case 0x00000280 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PINTEH; i.func = i_pinteh; return i;
                        case 0x00000300 >> 6: i.cycles = CYC_MMI_MULT; i.id = I_PMULTUW; i.func = i_pmultuw; return i;
                        case 0x00000340 >> 6: i.cycles = CYC_MMI_DIV; i.id = I_PDIVUW; i.func = i_pdivuw; return i;
                        case 0x00000380 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCPYUD; i.func = i_pcpyud; return i;
                        case 0x00000480 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_POR; i.func = i_por; return i;
                        case 0x000004C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PNOR; i.func = i_pnor; return i;
                        case 0x00000680 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXCH; i.func = i_pexch; return i;
                        case 0x000006C0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PCPYH; i.func = i_pcpyh; return i;
                        case 0x00000780 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PEXCW; i.func = i_pexcw; return i;
                    }
                } break;
                case 0x00000030: {
                    switch ((opcode & 0x000007C0) >> 6) {
                        case 0x00000000 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMFHLLW; i.func = i_pmfhllw; return i;
                        case 0x00000040 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMFHLUW; i.func = i_pmfhluw; return i;
                        case 0x00000080 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMFHLSLW; i.func = i_pmfhlslw; return i;
                        case 0x000000c0 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMFHLLH; i.func = i_pmfhllh; return i;
                        case 0x00000100 >> 6: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMFHLSH; i.func = i_pmfhlsh; return i;
                    }
                } break;
                case 0x00000031: i.cycles = CYC_MMI_DEFAULT; i.id = I_PMTHL; i.func = i_pmthl; return i;
                case 0x00000034: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSLLH; i.func = i_psllh; return i;
                case 0x00000036: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSRLH; i.func = i_psrlh; return i;
                case 0x00000037: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSRAH; i.func = i_psrah; return i;
                case 0x0000003C: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSLLW; i.func = i_psllw; return i;
                case 0x0000003E: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSRLW; i.func = i_psrlw; return i;
                case 0x0000003F: i.cycles = CYC_MMI_DEFAULT; i.id = I_PSRAW; i.func = i_psraw; return i;
            }
        } break;
        case 0x78000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LQ; i.func = i_lq; return i;
        case 0x7C000000 >> 26: i.cycles = CYC_LOAD; i.id = I_SQ; i.func = i_sq; return i;
        case 0x80000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LB; i.func = i_lb; return i;
        case 0x84000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LH; i.func = i_lh; return i;
        case 0x88000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LWL; i.func = i_lwl; return i;
        case 0x8C000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LW; i.func = i_lw; return i;
        case 0x90000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LBU; i.func = i_lbu; return i;
        case 0x94000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LHU; i.func = i_lhu; return i;
        case 0x98000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LWR; i.func = i_lwr; return i;
        case 0x9C000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LWU; i.func = i_lwu; return i;
        case 0xA0000000 >> 26: i.cycles = CYC_STORE; i.id = I_SB; i.func = i_sb; return i;
        case 0xA4000000 >> 26: i.cycles = CYC_STORE; i.id = I_SH; i.func = i_sh; return i;
        case 0xA8000000 >> 26: i.cycles = CYC_STORE; i.id = I_SWL; i.func = i_swl; return i;
        case 0xAC000000 >> 26: i.cycles = CYC_STORE; i.id = I_SW; i.func = i_sw; return i;
        case 0xB0000000 >> 26: i.cycles = CYC_STORE; i.id = I_SDL; i.func = i_sdl; return i;
        case 0xB4000000 >> 26: i.cycles = CYC_STORE; i.id = I_SDR; i.func = i_sdr; return i;
        case 0xB8000000 >> 26: i.cycles = CYC_STORE; i.id = I_SWR; i.func = i_swr; return i;
        case 0xBC000000 >> 26: i.cycles = CYC_DEFAULT; i.branch = 2; i.id = I_CACHE; i.func = i_cache; return i;
        case 0xC4000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LWC1; i.func = i_lwc1; return i;
        case 0xCC000000 >> 26: i.cycles = CYC_DEFAULT; i.id = I_PREF; i.func = i_pref; return i;
        case 0xD8000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LQC2; i.func = i_lqc2; return i;
        case 0xDC000000 >> 26: i.cycles = CYC_LOAD; i.id = I_LD; i.func = i_ld; return i;
        case 0xE4000000 >> 26: i.cycles = CYC_STORE; i.id = I_SWC1; i.func = i_swc1; return i;
        case 0xF8000000 >> 26: i.cycles = CYC_STORE; i.id = I_SQC2; i.func = i_sqc2; return i;
        case 0xFC000000 >> 26: i.cycles = CYC_STORE; i.id = I_SD; i.func = i_sd; return i;
    }

    i.id = I_INVALID; i.func = i_invalid;

    return i;
}

static inline bool has_breakpoint(Ee* ee, uint32_t addr) {
    for (int i = 0; i < ee->breakpoint_count; i++) {
        if (ee->breakpoints[i] == addr) {
            return true;
        }
    }

    return false;
}

static inline SubBlock decode_sub_block(Ee* ee, uint32_t pc, int max_cycles, std::vector<Instruction>& out) {
    SubBlock sb;

    sb.start_pc = pc;
    sb.end_pc = pc;
    sb.cycles = 0;
    sb.first = (uint32_t)out.size();
    sb.count = 0;
    sb.term = TERM_FALLTHROUGH;
    sb.branch_idx = -1;
    sb.succ_pc[0] = sb.succ_pc[1] = 0;
    sb.has_succ[0] = sb.has_succ[1] = false;
    sb.succ[0] = sb.succ[1] = -1;
    sb.back_edge_target = false;

    Instruction i;

    bool delay_slot = false;

    while (max_cycles) {
        if (ee->breakpoint_count && !delay_slot && sb.end_pc != sb.start_pc && has_breakpoint(ee, sb.end_pc))
            break;

        ee->opcode = bus_read32(ee, sb.end_pc);

        if (ee->opcode != 0) {
            i = decode(ee->opcode);

            out.push_back(i);

            sb.count++;
        } else {
            i.branch = 0;
        }

        if (i.branch == 1 && delay_slot) {
            if (sb.end_pc != ee->last_delay_slot_branch) {
                ee->last_delay_slot_branch = sb.end_pc;

                iris_error(ee, "Branch in delay slot at PC={:08x} (Unhandled edge case)", sb.end_pc);
            }

            sb.end_pc += 4;

            break;
        }

        sb.cycles++; // += i.cycles;

        if (i.branch == 1 || i.branch == 3) {
            delay_slot = true;

            sb.term = i.branch == 1 ? TERM_BRANCH : TERM_LIKELY;
            sb.branch_idx = (int32_t)sb.count - 1;

            max_cycles = 2;
        } else if (i.branch != 0) {
            sb.term = TERM_EXCEPT;
            sb.branch_idx = (int32_t)sb.count - 1;

            max_cycles = 1;
        }

        // Arbitrarily big number for MMI instructions, perf benefits from
        // long MMI sequences, keeping guest SIMD regs in host SIMD regs
        // longer is good
        // if (i.cycles == CYC_MMI_DEFAULT && !delay_slot) {
        //     max_cycles = 16;
        // }

        max_cycles--;

        sb.end_pc += 4;
    }

    return sb;
}

static inline bool branch_is_pcrel(int id) {
    switch (id) {
        case I_BEQ:  case I_BNE:  case I_BEQL: case I_BNEL:
        case I_BLTZ: case I_BGEZ: case I_BLEZ: case I_BGTZ:
        case I_BLTZL: case I_BGEZL: case I_BLEZL: case I_BGTZL:
        case I_BLTZAL: case I_BGEZAL: case I_BLTZALL: case I_BGEZALL:
        case I_BC0F: case I_BC0T: case I_BC0FL: case I_BC0TL:
        case I_BC1F: case I_BC1T: case I_BC1FL: case I_BC1TL:
            return true;
    }

    return false;
}

static inline void successors(const Block& block, SubBlock& sb) {
    if (sb.term == TERM_FALLTHROUGH) {
        sb.succ_pc[0] = sb.end_pc;
        sb.has_succ[0] = true;

        return;
    }

    if (sb.term != TERM_BRANCH && sb.term != TERM_LIKELY) return;
    if (sb.branch_idx < 0) return;

    const Instruction& br = block.instructions[sb.first + sb.branch_idx];

    if (!branch_is_pcrel(br.id)) return;

    int32_t off = (int32_t)(br.i16 << 16) >> 14;

    sb.succ_pc[0] = sb.end_pc;
    sb.succ_pc[1] = (sb.end_pc - 4) + (uint32_t)off;
    sb.has_succ[0] = true;
    sb.has_succ[1] = true;

    bool is_rr = br.id == I_BEQ || br.id == I_BNE ||
                 br.id == I_BEQL || br.id == I_BNEL;

    if (is_rr && br.rs.r == br.rt.r) {
        bool eq = br.id == I_BEQ || br.id == I_BEQL;

        sb.has_succ[0] = !eq;
        sb.has_succ[1] = eq;
    }
}

static inline Block* cache_block(Ee* ee, int max_cycles) {
    uint32_t phys;

    translate_virt(ee, ee->pc, &phys);

    uint32_t page = phys / MIN_PAGESIZE;
    uint32_t offset = (phys & (MIN_PAGESIZE - 1)) >> 2;

    if (!ee->block_cache[page].valid) {
        ee->block_cache[page].blocks = new Block[MIN_PAGESIZE >> 2];
        ee->block_cache[page].dirty = false;
        ee->block_cache[page].valid = true;
        ee->block_cache[page].min_code_addr = ee->pc;
        ee->block_cache[page].max_code_addr = ee->pc;
    }

    Block& block = ee->block_cache[page].blocks[offset];

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

    bool grow = max_cycles >= BLOCK_MAX_INSTRS;

    uint32_t page_base = ee->pc & ~(uint32_t)(MIN_PAGESIZE - 1);

    std::vector <uint32_t> pending;

    pending.push_back(ee->pc);

    while (!pending.empty()) {
        uint32_t pc = pending.back();

        pending.pop_back();

        bool seen = false;

        for (const SubBlock& s : ee->sub_blocks) {
            if (s.start_pc == pc) {
                seen = true;
                break;
            }
        }

        if (seen) continue;

        if (!ee->sub_blocks.empty()) {
            if (!grow)
                break;

            if (ee->sub_blocks.size() >= (size_t)REGION_MAX_BLOCKS)
                break;

            if (block.instructions.size() >= REGION_MAX_INSTRS)
                break;
        }

        ee->sub_blocks.push_back(decode_sub_block(ee, pc, max_cycles, block.instructions));

        if (!grow) break;

        successors(block, ee->sub_blocks.back());

        const SubBlock& sb = ee->sub_blocks.back();

        for (int e = 1; e >= 0; e--) {
            if (!sb.has_succ[e])
                continue;

            if ((sb.succ_pc[e] & ~(uint32_t)(MIN_PAGESIZE - 1)) != page_base)
                continue;

            if (ee->breakpoint_count && has_breakpoint(ee, sb.succ_pc[e]))
                continue;

            pending.push_back(sb.succ_pc[e]);
        }
    }

    for (size_t k = 0; k < ee->sub_blocks.size(); k++) {
        SubBlock& s = ee->sub_blocks[k];

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

    for (const SubBlock& s : ee->sub_blocks) {
        if (ee->block_cache[page].max_code_addr < s.end_pc) {
            ee->block_cache[page].max_code_addr = s.end_pc;
        }

        if (ee->block_cache[page].min_code_addr > s.start_pc) {
            ee->block_cache[page].min_code_addr = s.start_pc;
        }
    }

    return &block;
}

void compile_block(Ee* ee, Block* block);

static inline Block* find_block(Ee* ee, uint32_t pc) {
#ifdef _EE_DISABLE_CACHE
    return nullptr;
#endif

    BlockLutEntry& lut = ee->block_lut[(pc >> 2) & BLOCK_LUT_MASK];

    if (lut.pc == pc && lut.gen == ee->block_lut_gen) {
        return lut.block;
    }

    uint32_t phys;

    translate_virt(ee, ee->pc, &phys);

    uint32_t page = phys / MIN_PAGESIZE;

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

    uint32_t offset = (phys & (MIN_PAGESIZE - 1)) >> 2;

    Block& block = ee->block_cache[page].blocks[offset];

    if (!block.cycles) {
        return nullptr;
    }

    lut.pc = pc;
    lut.gen = ee->block_lut_gen;
    lut.block = &block;

    return &block;
}

#define EE(m) ujit::mem_ptr(ee->state_ptr, offsetof(Ee, m))

static inline void flush_vec(Ee* ee, asmjit::ujit::UniCompiler* uc, int i) {
    using namespace asmjit;

    if (!ee->vec_cache[i].valid) return;

    if (ee->vec_cache[i].dirty) {
        uc->v_storeu128(ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + i * sizeof(uint128_t)), ee->vec_cache[i].vec);
    }

    ee->vec_cache[i].valid = false;
    ee->vec_cache[i].dirty = false;
}

static inline void materialize_const(Ee* ee, asmjit::ujit::UniCompiler* uc, int i) {
    ee->reg_cache[i].reg = uc->new_gp64();
    uc->mov(ee->reg_cache[i].reg, asmjit::Imm((int64_t)ee->reg_cache[i].value));
    ee->reg_cache[i].valid = true;
}

static inline void set_const(Ee* ee, asmjit::ujit::UniCompiler* uc, int i, uint64_t value) {
    if (!i) return;

    flush_vec(ee, uc, i);

    ee->reg_cache[i].constant = true;
    ee->reg_cache[i].valid = false;
    ee->reg_cache[i].value = value;
}

static inline CachedReg& get_reg(Ee* ee, asmjit::ujit::UniCompiler* uc, int i, bool sync = true, bool b64 = true) {
    using namespace asmjit;

    flush_vec(ee, uc, i);

    if (!ee->reg_cache[i].valid) {
        ee->reg_cache[i].reg = b64 ? uc->new_gp64() : uc->new_gp32();

        if (sync) {
            if (ee->reg_cache[i].constant) {
                uc->mov(ee->reg_cache[i].reg, Imm((int64_t)ee->reg_cache[i].value));
            } else if (b64) {
                uc->load_u64(ee->reg_cache[i].reg, ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + i * sizeof(uint128_t)));
            } else {
                uc->load_u32(ee->reg_cache[i].reg, ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + i * sizeof(uint128_t)));
            }
        }

        ee->reg_cache[i].valid = true;
    }

    if (!sync) ee->reg_cache[i].constant = false;

    return ee->reg_cache[i];
}

static inline void flush_reg_cache(Ee* ee, asmjit::ujit::UniCompiler* uc) {
    using namespace asmjit;

    for (int i = 0; i < 32; i++) {
        if (ee->vec_cache[i].valid) {
            if (ee->vec_cache[i].dirty) {
                uc->v_storeu128(ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + i * sizeof(uint128_t)), ee->vec_cache[i].vec);
            }

            ee->vec_cache[i].valid = false;
            ee->vec_cache[i].dirty = false;
        }

        if (!ee->reg_cache[i].valid && ee->reg_cache[i].constant) {
            materialize_const(ee, uc, i);
        }

        if (ee->reg_cache[i].valid) {
            uc->store_u64(ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + i * sizeof(uint128_t)), ee->reg_cache[i].reg);
        }

        ee->reg_cache[i].valid = false;
        ee->reg_cache[i].constant = false;
    }
}

static inline void sext32(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Gp& dst, const asmjit::ujit::Gp& src) {
#if defined(ASMJIT_UJIT_AARCH64)
    uc.cc->sxtw(dst.r64(), src.r32());
#else
    uc.cc->movsxd(dst.r64(), src.r32());
#endif
}

static inline void sext32(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Gp& reg) {
    sext32(uc, reg, reg);
}

static inline void store_imm32(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Mem& dst, uint32_t value) {
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

static inline void sub_imm32(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Mem& dst, uint32_t value) {
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

static inline void sextn(asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Gp& reg, uint32_t bits) {
    uint32_t shift = 64u - bits;

    uc.shl(reg.r64(), reg.r64(), asmjit::Imm(shift));
    uc.sar(reg.r64(), reg.r64(), asmjit::Imm(shift));
}

static inline void load_reg_vec128(Ee* ee, asmjit::ujit::UniCompiler& uc, const asmjit::ujit::Vec& dst, int idx) {
    using namespace asmjit;

    uc.v_loadu128(dst, EE(r[idx]));

    if (ee->reg_cache[idx].valid) {
        uc.s_insert_u64(dst, ee->reg_cache[idx].reg, 0);
    }
}

static inline asmjit::ujit::Vec get_vec(Ee* ee, asmjit::ujit::UniCompiler& uc, int idx) {
    using namespace asmjit;

    if (ee->vec_cache[idx].valid) {
        return ee->vec_cache[idx].vec;
    }

    if (ee->reg_cache[idx].constant && !ee->reg_cache[idx].valid) {
        materialize_const(ee, &uc, idx);
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

static inline void set_vec(Ee* ee, asmjit::ujit::UniCompiler& uc, int idx, const asmjit::ujit::Vec& v) {
    ee->vec_cache[idx].vec = v;
    ee->vec_cache[idx].valid = true;
    ee->vec_cache[idx].dirty = true;
    ee->reg_cache[idx].valid = false;
    ee->reg_cache[idx].constant = false;
}

static inline void sync_reg_to_mem(Ee* ee, asmjit::ujit::UniCompiler& uc, int idx) {
    using namespace asmjit;

    if (ee->vec_cache[idx].valid) {
        if (ee->vec_cache[idx].dirty) {
            uc.v_storeu128(ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + idx * sizeof(uint128_t)), ee->vec_cache[idx].vec);
            ee->vec_cache[idx].dirty = false;
        }
    } else if (ee->reg_cache[idx].valid) {
        uc.store_u64(ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + idx * sizeof(uint128_t)), ee->reg_cache[idx].reg);
    } else if (ee->reg_cache[idx].constant) {
        materialize_const(ee, &uc, idx);
        uc.store_u64(ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + idx * sizeof(uint128_t)), ee->reg_cache[idx].reg);
    }
}

enum { MMI_WR_RD = 1, MMI_WR_HI = 2, MMI_WR_LO = 4 };

typedef void (*WideEmitFn)(asmjit::ujit::UniCompiler&, const asmjit::ujit::Vec&, const asmjit::ujit::Vec&, const asmjit::ujit::Vec&, const asmjit::ujit::Vec&, const asmjit::ujit::Vec&);

static inline void emit_mmi_wide(Ee* ee, asmjit::ujit::UniCompiler& uc, const Instruction& i, WideEmitFn emit, unsigned wmask, bool reads_st) {
    using namespace asmjit;

    ujit::Vec vhi = uc.new_vec128();
    ujit::Vec vlo = uc.new_vec128();

    uc.v_loadu128(vhi, EE(hi));
    uc.v_loadu128(vlo, EE(lo));

    ujit::Vec vrs = vhi, vrt = vhi;

    if (reads_st) {
        vrs = get_vec(ee, uc, i.rs.r);
        vrt = get_vec(ee, uc, i.rt.r);
    }

    ujit::Vec vrd = (wmask & MMI_WR_RD) ? uc.new_vec128() : vhi;

    emit(uc, vrd, vhi, vlo, vrs, vrt);

    if ((wmask & MMI_WR_RD) && i.rd.r) {
        set_vec(ee, uc, i.rd.r, vrd);
    }
    
    if (wmask & MMI_WR_HI) {
        uc.v_storeu128(EE(hi), vhi);
    }

    if (wmask & MMI_WR_LO) {
       uc.v_storeu128(EE(lo), vlo);
    }
}

static inline bool reg_is_const(Ee* ee, int r, uint64_t* v) {
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

static inline bool one_const(Ee* ee, int ra, int rb, uint64_t* c, int* other) {
    uint64_t va, vb;

    bool ca = reg_is_const(ee, ra, &va);
    bool cb = reg_is_const(ee, rb, &vb);

    if (ca == cb)
        return false;

    *c = ca ? va : vb;
    *other = ca ? rb : ra;

    return true;
}

static inline bool fits_imm32(uint64_t c) {
    return c == (uint64_t)(int64_t)(int32_t)c;
}

static int n = 0;

void compile_block(Ee* ee, Block* block) {
    using namespace asmjit;

    CodeHolder code;

    code.init(ee->rt.environment(), ee->rt.cpu_features());

    // if (logger::get_level(ee->logger) == logger::LEVEL_DEBUG)
    //     code.set_logger(ee->jit_logger);

    // if (code.logger()) {
    //     iris_debug(ee, "---------------------------------- Block at PC={:08x}, {} sub-blocks, {} instructions", block->start_pc, ee->sub_blocks.size(), block->instructions.size());
    // }

    ujit::BackendCompiler bc(&code);
    ujit::UniCompiler uc(&bc, ee->rt.cpu_features(), ee->rt.cpu_hints());

    FuncNode* func = uc.add_func(FuncSignature::build<void, Ee*>());

    ee->state_ptr = uc.new_gp_ptr();

    func->set_arg(0, ee->state_ptr);

    asmjit::Label block_exit = uc.new_label();

    uint32_t sb_end_pc = block->end_pc;

    std::vector <Label> sb_label(ee->sub_blocks.size());

    for (Label& l : sb_label) l = uc.new_label();

    uint32_t region_phys;

    translate_virt(ee, block->start_pc, &region_phys);

    const bool* region_dirty = &ee->block_cache[region_phys / MIN_PAGESIZE].dirty;

    const SubBlock* cur_sb = &ee->sub_blocks[0];

    size_t cur_sb_i = 0;

    enum PendKind { PEND_NONE, PEND_COND, PEND_TAKEN, PEND_DONE };

    PendKind pending = PEND_NONE;
    int32_t pending_off = 0;

    auto sb_links = [&]() { return cur_sb->succ[0] >= 0 || cur_sb->succ[1] >= 0; };

    auto emit_branch_target = [&](int32_t off) {
        store_imm32(uc, EE(next_pc), (sb_end_pc - 4) + off);
    };

    auto emit_edge = [&](int side, int32_t off) {
        int32_t s = cur_sb->succ[side];

        if (side == 1) emit_branch_target(off);

        flush_reg_cache(ee, &uc);

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
        flush_reg_cache(ee, &uc);

        Label l_taken = uc.new_label();

        uc.j(l_taken, taken);

        if (sb_links()) emit_edge(0, off);
        else uc.ret();

        uc.bind(l_taken);

        if (!sb_links()) emit_branch_target(off);
        else { pending = PEND_TAKEN; pending_off = off; }
    };

    auto emit_link = [&]() {
        CachedReg& ra = get_reg(ee, &uc, 31, false);

        uc.mov(ra.reg, Imm((uint64_t)sb_end_pc));
    };

    auto emit_branch_folded = [&](bool taken, bool likely, int32_t off) {
        if (taken) {
            if (sb_links()) { pending = PEND_TAKEN; pending_off = off; }
            else emit_branch_target(off);
        } else if (likely) {
            if (sb_links()) { emit_edge(0, off); pending = PEND_DONE; }
            else { flush_reg_cache(ee, &uc); uc.ret(); }
        }
    };

    enum { ZC_LTZ, ZC_GEZ, ZC_LEZ, ZC_GTZ };

    auto emit_branch_z = [&](const Instruction& i, int op, bool likely, bool link) {
        if (link) emit_link();

        uint64_t cs;

        if (!sb_links() && reg_is_const(ee, i.rs.r, &cs)) {
            int64_t v = (int64_t)cs;

            bool taken = op == ZC_LTZ ? v <  0 :
                         op == ZC_GEZ ? v >= 0 :
                         op == ZC_LEZ ? v <= 0 :
                                           v >  0;

            emit_branch_folded(taken, likely, D_SI16);

            return;
        }

        CachedReg& rs = get_reg(ee, &uc, i.rs.r);

        if (likely) {
            emit_branch_likely(op == ZC_LTZ ? ujit::scmp_lt(rs.reg, Imm(0)) :
                               op == ZC_GEZ ? ujit::scmp_ge(rs.reg, Imm(0)) :
                               op == ZC_LEZ ? ujit::scmp_le(rs.reg, Imm(0)) :
                                                 ujit::scmp_gt(rs.reg, Imm(0)), D_SI16);
        } else {
            emit_branch(op == ZC_LTZ ? ujit::scmp_ge(rs.reg, Imm(0)) :
                        op == ZC_GEZ ? ujit::scmp_lt(rs.reg, Imm(0)) :
                        op == ZC_LEZ ? ujit::scmp_gt(rs.reg, Imm(0)) :
                                          ujit::scmp_le(rs.reg, Imm(0)), D_SI16);
        }
    };

    auto emit_branch_eq = [&](const Instruction& i, bool eq, bool likely) {
        uint64_t cs, ct;

        if (!sb_links() && reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
            emit_branch_folded(eq ? cs == ct : cs != ct, likely, D_SI16);

            return;
        }

        if (i.rs.r == i.rt.r) {
            emit_branch_folded(eq, likely, D_SI16);

            return;
        }

        CachedReg& rs = get_reg(ee, &uc, i.rs.r);
        CachedReg& rt = get_reg(ee, &uc, i.rt.r);

        if (likely) {
            emit_branch_likely(eq ? ujit::cmp_eq(rs.reg, rt.reg) : ujit::cmp_ne(rs.reg, rt.reg), D_SI16);
        } else {
            emit_branch(eq ? ujit::cmp_ne(rs.reg, rt.reg) : ujit::cmp_eq(rs.reg, rt.reg), D_SI16);
        }
    };

    for (size_t sb_i = 0; sb_i < ee->sub_blocks.size(); sb_i++) {
        const SubBlock& sb = ee->sub_blocks[sb_i];

        sb_end_pc = sb.end_pc;
        cur_sb = &sb;
        cur_sb_i = sb_i;
        pending = PEND_NONE;

        flush_reg_cache(ee, &uc);

        uc.bind(sb_label[sb_i]);

        if (sb.back_edge_target) {
            store_imm32(uc, EE(next_pc), sb.start_pc);

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

        store_imm32(uc, EE(pc), sb.end_pc - 4);
        store_imm32(uc, EE(next_pc), sb.end_pc);

        sub_imm32(uc, EE(cycles_left), sb.cycles);

        for (uint32_t sb_n = 0; sb_n < sb.count; sb_n++) {
            const Instruction& i = block->instructions[sb.first + sb_n];

            switch (i.id) {
                case I_ADDI:
                case I_ADDIU: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (reg_is_const(ee, i.rs.r, &cs)) {
                        set_const(ee, &uc, i.rt.r, (int64_t)(int32_t)((uint32_t)cs + (int32_t)(int16_t)i.i16));
                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, sync);

                    if (!i.rs.r) {
                        uc.mov(rt.reg, Imm((int64_t)(int16_t)i.i16));
                    } else {
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                        uc.add(rt.reg, rs.reg, Imm((int32_t)(int16_t)i.i16));

                        sext32(uc, rt.reg);
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_DADDI:
                case I_DADDIU: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (reg_is_const(ee, i.rs.r, &cs)) {
                        set_const(ee, &uc, i.rt.r, cs + (int64_t)(int16_t)i.i16);
                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, sync);

                    if (!i.rs.r) {
                        uc.mov(rt.reg, Imm((int64_t)(int16_t)i.i16));
                    } else {
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                        uc.add(rt.reg, rs.reg, Imm((int32_t)(int16_t)i.i16));
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_MFC0: {
                    if (!i.rt.r) continue;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, false);

                    uc.load_u32(rt.reg, EE(cop0_r[i.rd.r]));

                    sext32(uc, rt.reg);
                } break;

                case I_MTC0: {
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.store_u32(EE(cop0_r[i.rd.r]), rt.reg);
                } break;

                case I_SUB:
                case I_SUBU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)cs - (uint32_t)ct));
                        continue;
                    }

                    uint64_t kt;

                    if (!reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &kt)) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, i.rs.r == i.rd.r);
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                        uc.add(rd.reg, rs.reg, Imm((int32_t)(uint32_t)(0 - kt)));

                        sext32(uc, rd.reg);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.sub(rd.reg, rs.reg, rt.reg);

                    sext32(uc, rd.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_ANDI:
                case I_XORI: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (reg_is_const(ee, i.rs.r, &cs)) {
                        uint64_t imm = (uint16_t)i.i16;
                        set_const(ee, &uc, i.rt.r, i.id == I_ANDI ? (cs & imm) : (cs ^ imm));

                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    switch (i.id) {
                        case I_ANDI: uc.and_(rt.reg, rs.reg, Imm(i.i16)); break;
                        case I_XORI: uc.xor_(rt.reg, rs.reg, Imm(i.i16)); break;
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_ORI: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (reg_is_const(ee, i.rs.r, &cs)) {
                        set_const(ee, &uc, i.rt.r, cs | (uint64_t)(uint16_t)i.i16);

                        continue;
                    }

                    bool sync = i.rs.r == i.rt.r;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, sync);

                    if (!i.rs.r) {
                        uc.mov(rt.reg, Imm(i.i16));
                    } else {
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                        uc.or_(rt.reg, rs.reg, Imm(i.i16));
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_AND:
                case I_OR:
                case I_XOR: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
                        uint64_t v = i.id == I_AND ? (cs & ct) : i.id == I_OR ? (cs | ct) : (cs ^ ct);

                        set_const(ee, &uc, i.rd.r, v);

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (one_const(ee, i.rs.r, i.rt.r, &kc, &ko)) {
                        if (i.id == I_AND && kc == 0) {
                            set_const(ee, &uc, i.rd.r, 0);

                            continue;
                        }

                        if (fits_imm32(kc)) {
                            CachedReg& rd = get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                            CachedReg& rs = get_reg(ee, &uc, ko);

                            switch (i.id) {
                                case I_AND: uc.and_(rd.reg, rs.reg, Imm((int64_t)kc)); break;
                                case I_OR: uc.or_(rd.reg, rs.reg, Imm((int64_t)kc)); break;
                                case I_XOR: uc.xor_(rd.reg, rs.reg, Imm((int64_t)kc)); break;
                            }

                            ee->reg_cache[i.rd.r].constant = false;

                            continue;
                        }
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp tmp = uc.new_gp64();

                    switch (i.id) {
                        case I_AND: uc.and_(tmp, rs.reg, rt.reg); break;
                        case I_OR: uc.or_(tmp, rs.reg, rt.reg); break;
                        case I_XOR: uc.xor_(tmp, rs.reg, rt.reg); break;
                    }

                    uc.mov(rd.reg, tmp);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_NOR: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, ~(cs | ct));

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (one_const(ee, i.rs.r, i.rt.r, &kc, &ko)) {
                        if (kc == ~0ull) {
                            set_const(ee, &uc, i.rd.r, 0);

                            continue;
                        }

                        if (fits_imm32(kc)) {
                            CachedReg& rd = get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                            CachedReg& rs = get_reg(ee, &uc, ko);

                            uc.or_(rd.reg, rs.reg, Imm((int64_t)kc));
                            uc.not_(rd.reg, rd.reg);

                            ee->reg_cache[i.rd.r].constant = false;

                            continue;
                        }
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp tmp = uc.new_gp64();

                    uc.or_(tmp, rs.reg, rt.reg);
                    uc.not_(tmp, tmp);
                    uc.mov(rd.reg, tmp);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_SLT: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, (int64_t)cs < (int64_t)ct ? 1 : 0);

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (one_const(ee, i.rs.r, i.rt.r, &kc, &ko) && fits_imm32(kc)) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                        CachedReg& rn = get_reg(ee, &uc, ko);

                        ujit::UniCondition c = (ko == i.rs.r) ? ujit::scmp_lt(rn.reg, Imm((int64_t)kc)) : ujit::scmp_gt(rn.reg, Imm((int64_t)kc));

                        uc.select(rd.reg, Imm(1), Imm(0), c);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.select(rd.reg, Imm(1), Imm(0), ujit::scmp_lt(rs.reg, rt.reg));

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_SLTU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, cs < ct ? 1 : 0);

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (one_const(ee, i.rs.r, i.rt.r, &kc, &ko) && fits_imm32(kc)) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                        CachedReg& rn = get_reg(ee, &uc, ko);

                        ujit::UniCondition c = (ko == i.rs.r) ? ujit::ucmp_lt(rn.reg, Imm((int64_t)kc)) : ujit::ucmp_gt(rn.reg, Imm((int64_t)kc));

                        uc.select(rd.reg, Imm(1), Imm(0), c);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.select(rd.reg, Imm(1), Imm(0), ujit::ucmp_lt(rs.reg, rt.reg));

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_SLTI: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (reg_is_const(ee, i.rs.r, &cs)) {
                        set_const(ee, &uc, i.rt.r, (int64_t)cs < (int64_t)(int16_t)i.i16 ? 1 : 0);

                        continue;
                    }

                    bool sync = i.rs.r == i.rt.r;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    uc.select(rt.reg, Imm(1), Imm(0), ujit::scmp_lt(rs.reg, Imm((int64_t)(int16_t)i.i16)));

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_SLTIU: {
                    if (!i.rt.r) continue;

                    uint64_t cs;

                    if (reg_is_const(ee, i.rs.r, &cs)) {
                        set_const(ee, &uc, i.rt.r, cs < (uint64_t)(int64_t)(int16_t)i.i16 ? 1 : 0);

                        continue;
                    }

                    bool sync = i.rs.r == i.rt.r;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    if (i.rs.r == 0) {
                        uc.mov(rt.reg, Imm(0ull < ((int64_t)(int16_t)i.i16)));

                        continue;
                    }

                    uc.select(rt.reg, Imm(1), Imm(0), ujit::ucmp_lt(rs.reg, Imm((int64_t)(int16_t)i.i16)));

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                // NOPs
                case I_CACHE:
                case I_PREF:
                case I_SYNC: {
                    continue;
                } break;

                case I_BEQ:  emit_branch_eq(i, true,  false); break;
                case I_BNE:  emit_branch_eq(i, false, false); break;
                case I_BEQL: emit_branch_eq(i, true,  true);  break;
                case I_BNEL: emit_branch_eq(i, false, true);  break;

                case I_BLTZ:    emit_branch_z(i, ZC_LTZ, false, false); break;
                case I_BGEZ:    emit_branch_z(i, ZC_GEZ, false, false); break;
                case I_BLEZ:    emit_branch_z(i, ZC_LEZ, false, false); break;
                case I_BGTZ:    emit_branch_z(i, ZC_GTZ, false, false); break;
                case I_BLTZAL:  emit_branch_z(i, ZC_LTZ, false, true);  break;
                case I_BGEZAL:  emit_branch_z(i, ZC_GEZ, false, true);  break;
                case I_BLTZL:   emit_branch_z(i, ZC_LTZ, true,  false); break;
                case I_BGEZL:   emit_branch_z(i, ZC_GEZ, true,  false); break;
                case I_BLEZL:   emit_branch_z(i, ZC_LEZ, true,  false); break;
                case I_BGTZL:   emit_branch_z(i, ZC_GTZ, true,  false); break;
                case I_BLTZALL: emit_branch_z(i, ZC_LTZ, true,  true);  break;
                case I_BGEZALL: emit_branch_z(i, ZC_GEZ, true,  true);  break;

                case I_BC0F: {
                    ujit::Gp cc = uc.new_gp32();
                    uc.load_u32(cc, EE(cpcond0));
                    emit_branch(ujit::test_nz(cc), D_SI16);
                } break;

                case I_BC0T: {
                    ujit::Gp cc = uc.new_gp32();
                    uc.load_u32(cc, EE(cpcond0));
                    emit_branch(ujit::test_z(cc), D_SI16);
                } break;

                case I_BC0FL: {
                    ujit::Gp cc = uc.new_gp32();
                    uc.load_u32(cc, EE(cpcond0));
                    emit_branch_likely(ujit::test_z(cc), D_SI16);
                } break;

                case I_BC0TL: {
                    ujit::Gp cc = uc.new_gp32();
                    uc.load_u32(cc, EE(cpcond0));
                    emit_branch_likely(ujit::test_nz(cc), D_SI16);
                } break;

                case I_BC1F: {
                    ujit::Gp f = uc.new_gp32();
                    uc.load_u32(f, EE(fcr));
                    emit_branch(ujit::test_nz(f, Imm(FPU_FLG_C)), D_SI16);
                } break;

                case I_BC1T: {
                    ujit::Gp f = uc.new_gp32();
                    uc.load_u32(f, EE(fcr));
                    emit_branch(ujit::test_z(f, Imm(FPU_FLG_C)), D_SI16);
                } break;

                case I_BC1FL: {
                    ujit::Gp f = uc.new_gp32();
                    uc.load_u32(f, EE(fcr));
                    emit_branch_likely(ujit::test_z(f, Imm(FPU_FLG_C)), D_SI16);
                } break;

                case I_BC1TL: {
                    ujit::Gp f = uc.new_gp32();
                    uc.load_u32(f, EE(fcr));
                    emit_branch_likely(ujit::test_nz(f, Imm(FPU_FLG_C)), D_SI16);
                } break;

                case I_BC2F:
                case I_BC2FL: {
                    emit_branch_target(D_SI16);
                } break;

                case I_BC2T: {
                } break;

                case I_BC2TL: {
                    flush_reg_cache(ee, &uc);
                    uc.ret();
                } break;

                case I_J: {
                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((i.i26 << 2) | (sb_end_pc & 0xF0000000)));

                    Label L0 = uc.new_label();
                    Label L1 = uc.new_label();

                    ujit::Gp skip_fmv_reg = uc.new_gp32();

                    uc.load_u32(skip_fmv_reg, EE(fmv_skip));
                    uc.j(L0, ujit::test_z(skip_fmv_reg));

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)skip_fmv,
                        FuncSignature::build<int, Ee*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, tmp);
                    invoke_node->set_ret(0, skip_fmv_reg);

                    uc.j(L1, ujit::test_nz(skip_fmv_reg));

                    uc.bind(L0);
                    uc.store_u32(EE(next_pc), tmp);

                    uc.bind(L1);
                } break;

                case I_JR: {
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    uc.store_u32(EE(next_pc), rs.reg);
                } break;

                case I_JAL: {
                    CachedReg& ra = get_reg(ee, &uc, 31, false);

                    uc.mov(ra.reg, Imm((uint64_t)sb_end_pc));

                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((i.i26 << 2) | (sb_end_pc & 0xF0000000)));

                    Label L0 = uc.new_label();
                    Label L1 = uc.new_label();

                    ujit::Gp skip_fmv_reg = uc.new_gp32();

                    uc.load_u32(skip_fmv_reg, EE(fmv_skip));
                    uc.j(L0, ujit::test_z(skip_fmv_reg));

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)skip_fmv,
                        FuncSignature::build<int, Ee*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, tmp);
                    invoke_node->set_ret(0, skip_fmv_reg);

                    uc.j(L1, ujit::test_nz(skip_fmv_reg));

                    uc.bind(L0);
                    uc.store_u32(EE(next_pc), tmp);

                    uc.bind(L1);
                } break;

                case I_JALR: {
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    if (!i.rd.r) {
                        uc.store_u32(EE(next_pc), rs.reg);
                    } else {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, false);

                        uc.store_u32(EE(next_pc), rs.reg);
                        uc.mov(rd.reg, Imm((uint64_t)sb_end_pc));
                    }
                } break;

                case I_SLL: {
                    if (!i.rd.r) continue;

                    uint64_t ct;

                    if (reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)ct << i.sa));

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.shl(rd.reg, rt.reg, Imm(i.sa));

                    sext32(uc, rd.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_SRL: {
                    if (!i.rd.r) continue;

                    uint64_t ct;

                    if (reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, (uint64_t)(int64_t)(int32_t)((uint32_t)ct >> i.sa));

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp tmp = uc.new_gp64();

                    uc.and_(tmp, rt.reg, Imm(0xFFFFFFFF));
                    uc.shr(rd.reg, tmp, Imm(i.sa));

                    sext32(uc, rd.reg, rd.reg.r32());

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_SRA: {
                    if (!i.rd.r) continue;

                    uint64_t ct;

                    if (reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((int32_t)ct >> i.sa));

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp tmp1 = rd.reg.r32();
                    ujit::Gp tmp2 = rt.reg.r32();

                    uc.sar(tmp1, tmp2, Imm(i.sa));

                    sext32(uc, rd.reg, tmp1);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_SLLV: {
                    if (!i.rd.r) continue;

                    uint64_t camt;

                    if (reg_is_const(ee, i.rs.r, &camt)) {
                        uint32_t sa = (uint32_t)camt & 0x1f;
                        uint64_t cval;

                        if (reg_is_const(ee, i.rt.r, &cval)) {
                            set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)cval << sa));

                            continue;
                        }

                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, i.rt.r == i.rd.r);
                        CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                        uc.shl(rd.reg, rt.reg, Imm(sa));

                        sext32(uc, rd.reg);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r || i.rs.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp64();

                    uc.and_(tmp, rs.reg, Imm(0x1F));
                    uc.shl(rd.reg, rt.reg, tmp);

                    sext32(uc, rd.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_SRLV: {
                    if (!i.rd.r) continue;

                    uint64_t camt;

                    if (reg_is_const(ee, i.rs.r, &camt)) {
                        uint32_t sa = (uint32_t)camt & 0x1f;
                        uint64_t cval;

                        if (reg_is_const(ee, i.rt.r, &cval)) {
                            set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)cval >> sa));

                            continue;
                        }

                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, i.rt.r == i.rd.r);
                        CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                        ujit::Gp rd32 = rd.reg.r32();
                        ujit::Gp rt32 = rt.reg.r32();

                        uc.shr(rd32, rt32, Imm(sa));

                        sext32(uc, rd.reg, rd32);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r || i.rs.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp32();
                    ujit::Gp rd32 = rd.reg.r32();
                    ujit::Gp rt32 = rt.reg.r32();
                    ujit::Gp rs32 = rs.reg.r32();

                    uc.and_(tmp, rs32, Imm(0x1F));
                    uc.shr(rd32, rt32, tmp);

                    sext32(uc, rd.reg, rd32);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_SRAV: {
                    if (!i.rd.r) continue;

                    uint64_t camt;

                    if (reg_is_const(ee, i.rs.r, &camt)) {
                        uint32_t sa = (uint32_t)camt & 0x1f;
                        uint64_t cval;

                        if (reg_is_const(ee, i.rt.r, &cval)) {
                            set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((int32_t)cval >> sa));

                            continue;
                        }

                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, i.rt.r == i.rd.r);
                        CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                        ujit::Gp rd32 = rd.reg.r32();
                        ujit::Gp rt32 = rt.reg.r32();

                        uc.sar(rd32, rt32, Imm(sa));

                        sext32(uc, rd.reg, rd32);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r || i.rs.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp32();
                    ujit::Gp rd32 = rd.reg.r32();
                    ujit::Gp rt32 = rt.reg.r32();
                    ujit::Gp rs32 = rs.reg.r32();

                    uc.and_(tmp, rs32, Imm(0x1F));
                    uc.sar(rd32, rt32, tmp);

                    sext32(uc, rd.reg, rd32);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_DSLL:
                case I_DSRL:
                case I_DSRA:
                case I_DSLL32:
                case I_DSRL32:
                case I_DSRA32: {
                    if (!i.rd.r) continue;

                    uint64_t ct;
                    if (reg_is_const(ee, i.rt.r, &ct)) {
                        uint32_t sa = i.sa + ((i.id == I_DSLL32 || i.id == I_DSRL32 || i.id == I_DSRA32) ? 32 : 0);
                        uint64_t v;

                        switch (i.id) {
                            case I_DSLL:
                            case I_DSLL32: {
                                v = ct << sa;
                            } break;

                            case I_DSRL: 
                            case I_DSRL32: {
                                v = ct >> sa;
                            } break;

                            default: {
                                v = (uint64_t)((int64_t)ct >> sa);
                            } break;
                        }

                        set_const(ee, &uc, i.rd.r, v);

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    switch (i.id) {
                        case I_DSLL: uc.shl(rd.reg, rt.reg, Imm(i.sa)); break;
                        case I_DSRL: uc.shr(rd.reg, rt.reg, Imm(i.sa)); break;
                        case I_DSRA: uc.sar(rd.reg, rt.reg, Imm(i.sa)); break;
                        case I_DSLL32: uc.shl(rd.reg, rt.reg, Imm(i.sa + 32)); break;
                        case I_DSRL32: uc.shr(rd.reg, rt.reg, Imm(i.sa + 32)); break;
                        case I_DSRA32: uc.sar(rd.reg, rt.reg, Imm(i.sa + 32)); break;
                    }

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_DSLLV:
                case I_DSRLV:
                case I_DSRAV: {
                    if (!i.rd.r) continue;

                    uint64_t camt;

                    if (reg_is_const(ee, i.rs.r, &camt)) {
                        uint32_t sa = (uint32_t)camt & 0x3f;
                        uint64_t cval;

                        if (reg_is_const(ee, i.rt.r, &cval)) {
                            uint64_t v;

                            switch (i.id) {
                                case I_DSLLV: v = cval << sa; break;
                                case I_DSRLV: v = cval >> sa; break;
                                case I_DSRAV: v = (uint64_t)((int64_t)cval >> sa); break;
                            }

                            set_const(ee, &uc, i.rd.r, v);

                            continue;
                        }

                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, i.rt.r == i.rd.r);
                        CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                        switch (i.id) {
                            case I_DSLLV: uc.shl(rd.reg, rt.reg, Imm(sa)); break;
                            case I_DSRLV: uc.shr(rd.reg, rt.reg, Imm(sa)); break;
                            case I_DSRAV: uc.sar(rd.reg, rt.reg, Imm(sa)); break;
                        }

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rt.r == i.rd.r || i.rs.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp64();

                    uc.and_(tmp, rs.reg, Imm(0x3F));

                    switch (i.id) {
                        case I_DSLLV: uc.shl(rd.reg, rt.reg, tmp); break;
                        case I_DSRLV: uc.shr(rd.reg, rt.reg, tmp); break;
                        case I_DSRAV: uc.sar(rd.reg, rt.reg, tmp); break;
                    }

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_LUI: {
                    if (!i.rt.r) continue;

                    set_const(ee, &uc, i.rt.r, (int64_t)(int32_t)(i.i16 << 16));

                    continue;
                } break;

                case I_LWC1: {
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp32();
                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)bus_read32,
                        FuncSignature::build<uint64_t, Ee*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_ret(0, tmp);

                    uc.store_u32(EE(f[i.rt.r]), tmp);
                } break;

                case I_SWC1: {
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();
                    ujit::Gp val = uc.new_gp64();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.load_u32(val, EE(f[i.rt.r]));

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)bus_write32,
                        FuncSignature::build<void, Ee*, uint32_t, uint64_t>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, val);
                } break;

                case I_MTC1: {
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.store_u32(EE(f[i.rd.r]), rt.reg.r32());
                } break;

                case I_MFC1: {
                    if (!i.rt.r) continue;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, false);

                    uc.load_u32(rt.reg, EE(f[i.rd.r]));
                    sext32(uc, rt.reg);

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_MOVS:
                case I_ABSS:
                case I_NEGS: {
                    ujit::Gp tmp = uc.new_gp32();

                    uc.load_u32(tmp, EE(f[i.rd.r]));

                    switch (i.id) {
                        case I_ABSS: uc.and_(tmp, tmp, Imm(0x7fffffff)); break;
                        case I_NEGS: uc.xor_(tmp, tmp, Imm(0x80000000)); break;
                    }

                    uc.store_u32(EE(f[i.sa]), tmp);

                    if (i.id != I_MOVS) {
                        ujit::Gp fcr = uc.new_gp32();

                        uc.load_u32(fcr, EE(fcr));
                        uc.and_(fcr, fcr, Imm(~(FPU_FLG_O | FPU_FLG_U)));
                        uc.store_u32(EE(fcr), fcr);
                    }
                } break;

                case I_ADDS:
                case I_SUBS:
                case I_MULS:
                case I_DIVS: {
                    ujit::Gp fs  = uc.new_gp32();
                    ujit::Gp ft  = uc.new_gp32();
                    ujit::Gp fcr = uc.new_gp32();
                    ujit::Gp out = uc.new_gp32();

                    uc.load_u32(fs, EE(f[i.rd.r]));
                    uc.load_u32(ft, EE(f[i.rt.r]));
                    uc.load_u32(fcr, EE(fcr));

                    switch (i.id) {
                        case I_ADDS: fpu::adds(uc, out, fcr, fs, ft); break;
                        case I_SUBS: fpu::subs(uc, out, fcr, fs, ft); break;
                        case I_MULS: fpu::muls(uc, out, fcr, fs, ft); break;
                        case I_DIVS: fpu::divs(uc, out, fcr, fs, ft); break;
                    }

                    uc.store_u32(EE(f[i.sa]), out);
                    uc.store_u32(EE(fcr), fcr);
                } break;

                case I_MAXS:
                case I_MINS: {
                    ujit::Gp fs = uc.new_gp32();
                    ujit::Gp ft = uc.new_gp32();
                    ujit::Gp out = uc.new_gp32();

                    uc.load_u32(fs, EE(f[i.rd.r]));
                    uc.load_u32(ft, EE(f[i.rt.r]));

                    if (i.id == I_MAXS) {
                        fpu::maxs(uc, out, fs, ft);
                    } else {
                        fpu::mins(uc, out, fs, ft);   
                    }

                    uc.store_u32(EE(f[i.sa]), out);

                    ujit::Gp fcr = uc.new_gp32();

                    uc.load_u32(fcr, EE(fcr));
                    uc.and_(fcr, fcr, Imm(~(uint32_t)(FPU_FLG_O | FPU_FLG_U)));
                    uc.store_u32(EE(fcr), fcr);
                } break;

                case I_ADDAS:
                case I_SUBAS:
                case I_MULAS:
                case I_MADDAS:
                case I_MSUBAS:
                case I_MADDS:
                case I_MSUBS:
                case I_SQRTS:
                case I_RSQRTS: {
                    ujit::Gp fs = uc.new_gp32();
                    ujit::Gp ft = uc.new_gp32();
                    ujit::Gp fcr = uc.new_gp32();
                    ujit::Gp out = uc.new_gp32();

                    uc.load_u32(fs, EE(f[i.rd.r]));
                    uc.load_u32(ft, EE(f[i.rt.r]));
                    uc.load_u32(fcr, EE(fcr));

                    bool to_acc = i.id == I_ADDAS || i.id == I_SUBAS || i.id == I_MULAS ||
                                i.id == I_MADDAS || i.id == I_MSUBAS;

                    switch (i.id) {
                        case I_ADDAS: fpu::adds(uc, out, fcr, fs, ft); break;
                        case I_SUBAS: fpu::subs(uc, out, fcr, fs, ft); break;
                        case I_MULAS: fpu::muls(uc, out, fcr, fs, ft); break;
                        case I_SQRTS: fpu::sqrts(uc, out, fcr, ft); break;
                        case I_RSQRTS: fpu::rsqrts(uc, out, fcr, fs, ft); break;

                        default: {
                            ujit::Gp acc = uc.new_gp32(); uc.load_u32(acc, EE(a));

                            switch (i.id) {
                                case I_MADDS: fpu::madds(uc, out, fcr, acc, fs, ft); break;
                                case I_MSUBS: fpu::msubs(uc, out, fcr, acc, fs, ft); break;
                                case I_MADDAS: fpu::maddas(uc, out, fcr, acc, fs, ft); break;
                                case I_MSUBAS: fpu::msubas(uc, out, fcr, acc, fs, ft); break;
                            }
                        } break;
                    }

                    uc.store_u32(to_acc ? EE(a) : EE(f[i.sa]), out);
                    uc.store_u32(EE(fcr), fcr);
                } break;

                case I_CF:
                case I_CEQ:
                case I_CLT:
                case I_CLE: {
                    ujit::Gp fcr = uc.new_gp32();

                    uc.load_u32(fcr, EE(fcr));

                    if (i.id == I_CF) {
                        fpu::cf(uc, fcr);
                    } else {
                        ujit::Gp fs = uc.new_gp32();
                        ujit::Gp ft = uc.new_gp32();

                        uc.load_u32(fs, EE(f[i.rd.r]));
                        uc.load_u32(ft, EE(f[i.rt.r]));

                        switch (i.id) {
                            case I_CEQ: fpu::ceq(uc, fcr, fs, ft); break;
                            case I_CLT: fpu::clt(uc, fcr, fs, ft); break;
                            case I_CLE: fpu::cle(uc, fcr, fs, ft); break;
                        }
                    }

                    uc.store_u32(EE(fcr), fcr);
                } break;

                case I_CVTW:
                case I_CVTS: {
                    ujit::Gp fs = uc.new_gp32();
                    ujit::Gp out = uc.new_gp32();

                    uc.load_u32(fs, EE(f[i.rd.r]));

                    if (i.id == I_CVTW) {
                        fpu::cvtws(uc, out, fs);
                    } else {
                        fpu::cvtsw(uc, out, fs);
                    }

                    uc.store_u32(EE(f[i.sa]), out);
                } break;

                case I_LB:
                case I_LH:
                case I_LW:
                case I_LD: {
                    uintptr_t func;
                    int bytes;

                    switch (i.id) {
                        case I_LB: func = (uintptr_t)bus_read8;  bytes = 1; break;
                        case I_LH: func = (uintptr_t)bus_read16; bytes = 2; break;
                        case I_LW: func = (uintptr_t)bus_read32; bytes = 4; break;
                        case I_LD: func = (uintptr_t)bus_read64; bytes = 8; break;
                    }

                    uint64_t cs;
                    uint32_t phys;

                    void* host = nullptr;

                    if (reg_is_const(ee, i.rs.r, &cs))
                        host = fold_host_ptr(ee, (uint32_t)cs + (int32_t)(int16_t)i.i16, bytes, false, &phys);

                    if (host) {
                        if (i.rt.r) {
                            CachedReg& rt = get_reg(ee, &uc, i.rt.r, false);
                            ujit::Gp base = fold_base(uc, host);

                            switch (i.id) {
                                case I_LB: uc.load_i8(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case I_LH: uc.load_i16(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case I_LW: uc.load_i32(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case I_LD: uc.load_u64(rt.reg, ujit::mem_ptr(base, 0)); break;
                            }

                            ee->reg_cache[i.rt.r].constant = false;
                        }

                        continue;
                    }

                    if (ee->vfast_r && i.rt.r) {
                        uintptr_t slow_func;

                        switch (i.id) {
                            case I_LB: slow_func = (uintptr_t)vfast_read8;  break;
                            case I_LH: slow_func = (uintptr_t)vfast_read16; break;
                            case I_LW: slow_func = (uintptr_t)vfast_read32; break;
                            case I_LD: slow_func = (uintptr_t)vfast_read64; break;
                        }

                        CachedReg& frt = get_reg(ee, &uc, i.rt.r, i.rt.r == i.rs.r);
                        CachedReg& frs = get_reg(ee, &uc, i.rs.r);

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
                            case I_LB: uc.load_u8(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case I_LH: uc.load_u16(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case I_LW: uc.load_u32(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case I_LD: uc.load_u64(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                        }

                        uc.j(done);
                        uc.bind(slow);

                        InvokeNode* slow_node = jit_invoke(
                            uc,
                            slow_func,
                            FuncSignature::build<uint64_t, Ee*, uint32_t>()
                        );

                        slow_node->set_arg(0, ee->state_ptr);
                        slow_node->set_arg(1, addr);
                        slow_node->set_ret(0, frt.reg);

                        uc.bind(done);

                        switch (i.id) {
                            case I_LB: sextn(uc, frt.reg, 8); break;
                            case I_LH: sextn(uc, frt.reg, 16); break;
                            case I_LW: sext32(uc, frt.reg); break;
                        }

                        ee->reg_cache[i.rt.r].constant = false;

                        continue;
                    }

                    if (!i.rt.r) {
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                        ujit::Gp tmp = uc.new_gp32();

                        uc.mov(tmp, Imm((int32_t)(int16_t)i.i16));
                        uc.add(tmp, tmp, rs.reg.r32());

                        InvokeNode* invoke_node = jit_invoke(
                            uc,
                            func,
                            FuncSignature::build<uint64_t, Ee*, uint32_t>()
                        );

                        invoke_node->set_arg(0, ee->state_ptr);
                        invoke_node->set_arg(1, tmp);

                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((int32_t)(int16_t)i.i16));
                    uc.add(tmp, tmp, rs.reg.r32());

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        func,
                        FuncSignature::build<uint64_t, Ee*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, tmp);
                    invoke_node->set_ret(0, rt.reg);

                    if (i.id != I_LD) {
                        switch (i.id) {
                            case I_LB: sextn(uc, rt.reg, 8); break;
                            case I_LH: sextn(uc, rt.reg, 16); break;
                            case I_LW: sext32(uc, rt.reg); break;
                        }
                    }

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_LDL:
                case I_LDR: {
                    bool is_l = i.id == I_LDL;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();
                    ujit::Gp aligned = uc.new_gp32();
                    ujit::Gp off = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(aligned, addr, Imm(~7));
                    uc.and_(off, addr, Imm(7));

                    ujit::Gp data = uc.new_gp64();

                    InvokeNode* rd_node = jit_invoke(
                        uc,
                        (uintptr_t)bus_read64,
                        FuncSignature::build<uint64_t, Ee*, uint32_t>()
                    );

                    rd_node->set_arg(0, ee->state_ptr);
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

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.and_(rt.reg, rt.reg, mask);
                    uc.or_(rt.reg, rt.reg, shifted);

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_LBU:
                case I_LHU:
                case I_LWU: {
                    uintptr_t func;
                    int bytes;

                    switch (i.id) {
                        case I_LBU: func = (uintptr_t)bus_read8;  bytes = 1; break;
                        case I_LHU: func = (uintptr_t)bus_read16; bytes = 2; break;
                        case I_LWU: func = (uintptr_t)bus_read32; bytes = 4; break;
                    }

                    uint64_t cs;
                    uint32_t phys;

                    void* host = nullptr;

                    if (reg_is_const(ee, i.rs.r, &cs)) {
                        host = fold_host_ptr(ee, (uint32_t)cs + (int32_t)(int16_t)i.i16, bytes, false, &phys);
                    }

                    if (host) {
                        if (i.rt.r) {
                            CachedReg& rt = get_reg(ee, &uc, i.rt.r, false);

                            ujit::Gp base = fold_base(uc, host);

                            switch (i.id) {
                                case I_LBU: uc.load_u8(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case I_LHU: uc.load_u16(rt.reg, ujit::mem_ptr(base, 0)); break;
                                case I_LWU: uc.load_u32(rt.reg, ujit::mem_ptr(base, 0)); break;
                            }

                            ee->reg_cache[i.rt.r].constant = false;
                        }

                        continue;
                    }

                    if (ee->vfast_r && i.rt.r) {
                        uintptr_t slow_func;

                        switch (i.id) {
                            case I_LBU: slow_func = (uintptr_t)vfast_read8; break;
                            case I_LHU: slow_func = (uintptr_t)vfast_read16; break;
                            case I_LWU: slow_func = (uintptr_t)vfast_read32; break;
                        }

                        CachedReg& frt = get_reg(ee, &uc, i.rt.r, i.rt.r == i.rs.r);
                        CachedReg& frs = get_reg(ee, &uc, i.rs.r);

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
                            case I_LBU: uc.load_u8(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case I_LHU: uc.load_u16(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                            case I_LWU: uc.load_u32(frt.reg, ujit::mem_ptr(base, off, 0)); break;
                        }

                        uc.j(done);
                        uc.bind(slow);

                        InvokeNode* slow_node = jit_invoke(
                            uc,
                            slow_func,
                            FuncSignature::build<uint64_t, Ee*, uint32_t>()
                        );

                        slow_node->set_arg(0, ee->state_ptr);
                        slow_node->set_arg(1, addr);
                        slow_node->set_ret(0, frt.reg);

                        uc.bind(done);

                        ee->reg_cache[i.rt.r].constant = false;

                        continue;
                    }

                    if (!i.rt.r) {
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                        ujit::Gp tmp = uc.new_gp32();

                        uc.mov(tmp, Imm((int32_t)(int16_t)i.i16));
                        uc.add(tmp, tmp, rs.reg.r32());

                        InvokeNode* invoke_node = jit_invoke(
                            uc,
                            func,
                            FuncSignature::build<uint64_t, Ee*, uint32_t>()
                        );

                        invoke_node->set_arg(0, ee->state_ptr);
                        invoke_node->set_arg(1, tmp);

                        continue;
                    }

                    bool sync = i.rt.r == i.rs.r;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp = uc.new_gp32();

                    uc.mov(tmp, Imm((int32_t)(int16_t)i.i16));
                    uc.add(tmp, tmp, rs.reg.r32());

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        func,
                        FuncSignature::build<uint64_t, Ee*, uint32_t>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, tmp);
                    invoke_node->set_ret(0, rt.reg);

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_LQ: {
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

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

                    uc.lea(ptr, ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + rt * sizeof(uint128_t)));

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)jit_read128,
                        FuncSignature::build<void, Ee*, uint32_t, uint128_t*>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, ptr);

                    if (!rt) {
                        uc.store_zero_u64(EE(r[0].u64[0]));
                        uc.store_zero_u64(EE(r[0].u64[1]));
                    }
                } break;

                case I_SB:
                case I_SH:
                case I_SW:
                case I_SD: {
                    uintptr_t func;
                    int bytes;

                    switch (i.id) {
                        case I_SB: func = (uintptr_t)bus_write8; bytes = 1; break;
                        case I_SH: func = (uintptr_t)bus_write16; bytes = 2; break;
                        case I_SW: func = (uintptr_t)bus_write32; bytes = 4; break;
                        case I_SD: func = (uintptr_t)bus_write64; bytes = 8; break;
                    }

                    {
                        uint64_t cs;
                        uint32_t phys;
                        void* host = nullptr;

                        if (reg_is_const(ee, i.rs.r, &cs))
                            host = fold_host_ptr(ee, (uint32_t)cs + (int32_t)(int16_t)i.i16, bytes, true, &phys);

                        if (host) {
                            CachedReg& frt = get_reg(ee, &uc, i.rt.r);

                            if (phys != FOLD_NO_PHYS) {
                                CachePage* pg = &ee->block_cache[phys / MIN_PAGESIZE];

                                asmjit::Label no_smc = uc.new_label();
                                ujit::Gp pgp = uc.new_gp_ptr();
                                ujit::Gp pgv = uc.new_gp32();

                                uc.mov(pgp, Imm((uint64_t)(uintptr_t)pg));
                                uc.load_u8(pgv, ujit::mem_ptr(pgp, offsetof(CachePage, valid)));
                                uc.j(no_smc, ujit::test_z(pgv));

                                InvokeNode* inv_node = jit_invoke(
                                    uc,
                                    (uintptr_t)invalidate_page,
                                    FuncSignature::build<void, Ee*, uint32_t>()
                                );

                                inv_node->set_arg(0, ee->state_ptr);
                                inv_node->set_arg(1, Imm(phys));

                                uc.bind(no_smc);
                            }

                            ujit::Gp base = fold_base(uc, host);

                            switch (i.id) {
                                case I_SB: uc.store_u8(ujit::mem_ptr(base, 0), frt.reg); break;
                                case I_SH: uc.store_u16(ujit::mem_ptr(base, 0), frt.reg); break;
                                case I_SW: uc.store_u32(ujit::mem_ptr(base, 0), frt.reg); break;
                                case I_SD: uc.store_u64(ujit::mem_ptr(base, 0), frt.reg); break;
                            }

                            continue;
                        }
                    }

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp tmp1 = uc.new_gp64();

                    uc.mov(tmp1, Imm((int32_t)(int16_t)i.i16));
                    uc.add(tmp1, tmp1, rs.reg);

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        func,
                        FuncSignature::build<void, Ee*, uint32_t, uint64_t>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, tmp1);
                    invoke_node->set_arg(2, rt.reg);
                } break;

                case I_SDL:
                case I_SDR: {
                    bool is_l = i.id == I_SDL;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp addr = uc.new_gp32();
                    ujit::Gp aligned = uc.new_gp32();
                    ujit::Gp off = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(aligned, addr, Imm(~7));
                    uc.and_(off, addr, Imm(7));

                    ujit::Gp data = uc.new_gp64();

                    InvokeNode* rd_node = jit_invoke(
                        uc,
                        (uintptr_t)bus_read64,
                        FuncSignature::build<uint64_t, Ee*, uint32_t>()
                    );

                    rd_node->set_arg(0, ee->state_ptr);
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

                    InvokeNode* wr_node = jit_invoke(
                        uc,
                        (uintptr_t)bus_write64,
                        FuncSignature::build<void, Ee*, uint32_t, uint64_t>()
                    );

                    wr_node->set_arg(0, ee->state_ptr);
                    wr_node->set_arg(1, aligned);
                    wr_node->set_arg(2, store_val);
                } break;

                case I_SQ: {
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(addr, addr, Imm(~0xf));

                    sync_reg_to_mem(ee, uc, i.rt.r);

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.lea(ptr, ujit::mem_ptr(ee->state_ptr, offsetof(Ee, r) + i.rt.r * sizeof(uint128_t)));

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)jit_write128,
                        FuncSignature::build<void, Ee*, uint32_t, uint128_t*>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, ptr);
                } break;

                case I_LQC2: {
                    int rt = i.rt.r;

                    if (!rt) continue;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(addr, addr, Imm(~0xf));

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.load_u64(ptr, EE(vu0));
                    uc.lea(ptr, ujit::mem_ptr(ptr, (int)(offsetof(vu::Vu, vf) + rt * sizeof(vu::Reg128))));

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)jit_read128,
                        FuncSignature::build<void, Ee*, uint32_t, uint128_t*>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, ptr);
                } break;

                case I_SQC2: {
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());
                    uc.and_(addr, addr, Imm(~0xf));

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.load_u64(ptr, EE(vu0));
                    uc.lea(ptr, ujit::mem_ptr(ptr, (int)(offsetof(vu::Vu, vf) + i.rt.r * sizeof(vu::Reg128))));

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)jit_write128,
                        FuncSignature::build<void, Ee*, uint32_t, uint128_t*>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, addr);
                    invoke_node->set_arg(2, ptr);
                } break;

                case I_QMFC2: {
                    if (!i.rt.r) continue;

                    ujit::Gp ptr = uc.new_gp_ptr();
                    uc.load_u64(ptr, EE(vu0));

                    ujit::Vec v = uc.new_vec128();
                    uc.v_loadu128(v, ujit::mem_ptr(ptr, (int)(offsetof(vu::Vu, vf) + i.rd.r * sizeof(vu::Reg128))));

                    set_vec(ee, uc, i.rt.r, v);
                } break;

                case I_QMTC2: {
                    if (!i.rd.r) continue;

                    ujit::Vec v = get_vec(ee, uc, i.rt.r);

                    ujit::Gp ptr = uc.new_gp_ptr();

                    uc.load_u64(ptr, EE(vu0));
                    uc.v_storeu128(ujit::mem_ptr(ptr, (int)(offsetof(vu::Vu, vf) + i.rd.r * sizeof(vu::Reg128))), v);
                } break;

                case I_PADDB:  case I_PADDH:  case I_PADDW:
                case I_PSUBB:  case I_PSUBH:  case I_PSUBW:
                case I_PADDUB: case I_PADDUH: case I_PSUBUB: case I_PSUBUH:
                case I_PADDSB: case I_PADDSH: case I_PSUBSB: case I_PSUBSH:
                case I_PADDSW: case I_PSUBSW: case I_PADDUW: case I_PSUBUW:
                case I_PADSBH:
                case I_PAND:   case I_POR:    case I_PXOR:   case I_PNOR:
                case I_PCEQB:  case I_PCEQH:  case I_PCEQW:
                case I_PCGTB:  case I_PCGTH:  case I_PCGTW:
                case I_PMAXH:  case I_PMAXW:  case I_PMINH:  case I_PMINW:
                case I_PEXTLB: case I_PEXTLH: case I_PEXTLW:
                case I_PEXTUB: case I_PEXTUH: case I_PEXTUW:
                case I_PCPYLD: case I_PCPYUD: case I_PINTH:  case I_PINTEH:
                case I_PPACB:  case I_PPACH:  case I_PPACW: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrs = get_vec(ee, uc, i.rs.r);
                    ujit::Vec vrt = get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrd = uc.new_vec128();

                    switch (i.id) {
                        case I_PADDB:  mmi::paddb(uc, vrd, vrs, vrt); break;
                        case I_PADDH:  mmi::paddh(uc, vrd, vrs, vrt); break;
                        case I_PADDW:  mmi::paddw(uc, vrd, vrs, vrt); break;
                        case I_PSUBB:  mmi::psubb(uc, vrd, vrs, vrt); break;
                        case I_PSUBH:  mmi::psubh(uc, vrd, vrs, vrt); break;
                        case I_PSUBW:  mmi::psubw(uc, vrd, vrs, vrt); break;
                        case I_PADDUB: mmi::paddub(uc, vrd, vrs, vrt); break;
                        case I_PADDUH: mmi::padduh(uc, vrd, vrs, vrt); break;
                        case I_PSUBUB: mmi::psubub(uc, vrd, vrs, vrt); break;
                        case I_PSUBUH: mmi::psubuh(uc, vrd, vrs, vrt); break;
                        case I_PADDSB: mmi::paddsb(uc, vrd, vrs, vrt); break;
                        case I_PADDSH: mmi::paddsh(uc, vrd, vrs, vrt); break;
                        case I_PSUBSB: mmi::psubsb(uc, vrd, vrs, vrt); break;
                        case I_PSUBSH: mmi::psubsh(uc, vrd, vrs, vrt); break;
                        case I_PADDSW: mmi::paddsw(uc, vrd, vrs, vrt); break;
                        case I_PSUBSW: mmi::psubsw(uc, vrd, vrs, vrt); break;
                        case I_PADDUW: mmi::padduw(uc, vrd, vrs, vrt); break;
                        case I_PSUBUW: mmi::psubuw(uc, vrd, vrs, vrt); break;
                        case I_PADSBH: mmi::padsbh(uc, vrd, vrs, vrt); break;
                        case I_PAND:   mmi::pand(uc, vrd, vrs, vrt); break;
                        case I_POR:    mmi::por(uc, vrd, vrs, vrt); break;
                        case I_PXOR:   mmi::pxor(uc, vrd, vrs, vrt); break;
                        case I_PNOR:   mmi::pnor(uc, vrd, vrs, vrt); break;
                        case I_PCEQB:  mmi::pceqb(uc, vrd, vrs, vrt); break;
                        case I_PCEQH:  mmi::pceqh(uc, vrd, vrs, vrt); break;
                        case I_PCEQW:  mmi::pceqw(uc, vrd, vrs, vrt); break;
                        case I_PCGTB:  mmi::pcgtb(uc, vrd, vrs, vrt); break;
                        case I_PCGTH:  mmi::pcgth(uc, vrd, vrs, vrt); break;
                        case I_PCGTW:  mmi::pcgtw(uc, vrd, vrs, vrt); break;
                        case I_PMAXH:  mmi::pmaxh(uc, vrd, vrs, vrt); break;
                        case I_PMAXW:  mmi::pmaxw(uc, vrd, vrs, vrt); break;
                        case I_PMINH:  mmi::pminh(uc, vrd, vrs, vrt); break;
                        case I_PMINW:  mmi::pminw(uc, vrd, vrs, vrt); break;
                        case I_PEXTLB: mmi::pextlb(uc, vrd, vrs, vrt); break;
                        case I_PEXTLH: mmi::pextlh(uc, vrd, vrs, vrt); break;
                        case I_PEXTLW: mmi::pextlw(uc, vrd, vrs, vrt); break;
                        case I_PEXTUB: mmi::pextub(uc, vrd, vrs, vrt); break;
                        case I_PEXTUH: mmi::pextuh(uc, vrd, vrs, vrt); break;
                        case I_PEXTUW: mmi::pextuw(uc, vrd, vrs, vrt); break;
                        case I_PCPYLD: mmi::pcpyld(uc, vrd, vrs, vrt); break;
                        case I_PCPYUD: mmi::pcpyud(uc, vrd, vrs, vrt); break;
                        case I_PINTH:  mmi::pinth(uc, vrd, vrs, vrt); break;
                        case I_PINTEH: mmi::pinteh(uc, vrd, vrs, vrt); break;
                        case I_PPACB:  mmi::ppacb(uc, vrd, vrs, vrt); break;
                        case I_PPACH:  mmi::ppach(uc, vrd, vrs, vrt); break;
                        case I_PPACW:  mmi::ppacw(uc, vrd, vrs, vrt); break;
                    }

                    set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case I_PCPYH: case I_PEXEH: case I_PREVH: case I_PEXCH:
                case I_PEXEW: case I_PEXCW: case I_PROT3W:
                case I_PABSH: case I_PABSW:
                case I_PEXT5: case I_PPAC5: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrt = get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrd = uc.new_vec128();

                    switch (i.id) {
                        case I_PCPYH:  mmi::pcpyh(uc, vrd, vrt); break;
                        case I_PEXEH:  mmi::pexeh(uc, vrd, vrt); break;
                        case I_PREVH:  mmi::prevh(uc, vrd, vrt); break;
                        case I_PEXCH:  mmi::pexch(uc, vrd, vrt); break;
                        case I_PEXEW:  mmi::pexew(uc, vrd, vrt); break;
                        case I_PEXCW:  mmi::pexcw(uc, vrd, vrt); break;
                        case I_PROT3W: mmi::prot3w(uc, vrd, vrt); break;
                        case I_PABSH:  mmi::pabsh(uc, vrd, vrt); break;
                        case I_PABSW:  mmi::pabsw(uc, vrd, vrt); break;
                        case I_PEXT5:  mmi::pext5(uc, vrd, vrt); break;
                        case I_PPAC5:  mmi::ppac5(uc, vrd, vrt); break;
                    }

                    set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case I_PSLLH:
                case I_PSLLW:
                case I_PSRLH:
                case I_PSRLW:
                case I_PSRAH:
                case I_PSRAW: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrt = get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrd = uc.new_vec128();

                    switch (i.id) {
                        case I_PSLLH: mmi::psllh(uc, vrd, vrt, i.sa); break;
                        case I_PSLLW: mmi::psllw(uc, vrd, vrt, i.sa); break;
                        case I_PSRLH: mmi::psrlh(uc, vrd, vrt, i.sa); break;
                        case I_PSRLW: mmi::psrlw(uc, vrd, vrt, i.sa); break;
                        case I_PSRAH: mmi::psrah(uc, vrd, vrt, i.sa); break;
                        case I_PSRAW: mmi::psraw(uc, vrd, vrt, i.sa); break;
                    }

                    set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case I_PMFHI: emit_mmi_wide(ee, uc, i, mmi::pmfhi, MMI_WR_RD, false); break;
                case I_PMFLO: emit_mmi_wide(ee, uc, i, mmi::pmflo, MMI_WR_RD, false); break;
                case I_PMTHI: emit_mmi_wide(ee, uc, i, mmi::pmthi, MMI_WR_HI, true); break;
                case I_PMTLO: emit_mmi_wide(ee, uc, i, mmi::pmtlo, MMI_WR_LO, true); break;
                case I_PMTHL: emit_mmi_wide(ee, uc, i, mmi::pmthl, MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PMULTW:  emit_mmi_wide(ee, uc, i, mmi::pmultw,  MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PMULTUW: emit_mmi_wide(ee, uc, i, mmi::pmultuw, MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PMADDW:  emit_mmi_wide(ee, uc, i, mmi::pmaddw,  MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PMADDUW: emit_mmi_wide(ee, uc, i, mmi::pmadduw, MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PMULTH:  emit_mmi_wide(ee, uc, i, mmi::pmulth,  MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PMADDH:  emit_mmi_wide(ee, uc, i, mmi::pmaddh,  MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PMSUBW:  emit_mmi_wide(ee, uc, i, mmi::pmsubw,  MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PMSUBH:  emit_mmi_wide(ee, uc, i, mmi::pmsubh,  MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PHMADH:  emit_mmi_wide(ee, uc, i, mmi::phmadh,  MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;
                case I_PHMSBH:  emit_mmi_wide(ee, uc, i, mmi::phmsbh,  MMI_WR_RD | MMI_WR_HI | MMI_WR_LO, true); break;

                case I_MOVZ:
                case I_MOVN: {
                    if (!i.rd.r) continue;

                    uint64_t crt;

                    if (reg_is_const(ee, i.rt.r, &crt)) {
                        bool moves = (i.id == I_MOVZ) ? (crt == 0) : (crt != 0);

                        if (!moves) continue;

                        uint64_t crs;

                        if (reg_is_const(ee, i.rs.r, &crs)) {
                            set_const(ee, &uc, i.rd.r, crs);

                            continue;
                        }

                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, i.rs.r == i.rd.r);
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                        uc.mov(rd.reg, rs.reg);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::CondCode cond_code = i.id == I_MOVZ ? ujit::CondCode::kZero : ujit::CondCode::kNotZero;
                    ujit::UniCondition cond(ujit::UniOpCond::kTest, cond_code, rt.reg, rt.reg);

                    uc.cmov(rd.reg, rs.reg, cond);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_ADD:
                case I_ADDU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, (int64_t)(int32_t)((uint32_t)cs + (uint32_t)ct));

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (one_const(ee, i.rs.r, i.rt.r, &kc, &ko)) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                        CachedReg& rs = get_reg(ee, &uc, ko);

                        uc.add(rd.reg, rs.reg, Imm((int32_t)(uint32_t)kc));

                        sext32(uc, rd.reg);

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);

                    if (!i.rs.r || !i.rt.r) {
                        CachedReg& src = get_reg(ee, &uc, i.rs.r ? i.rs.r : i.rt.r);

                        uc.mov(rd.reg, src.reg);
                    } else {
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                        CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                        uc.add(rd.reg, rs.reg, rt.reg);
                    }

                    sext32(uc, rd.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_DADD:
                case I_DADDU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, cs + ct);

                        continue;
                    }

                    uint64_t kc;
                    int ko;

                    if (one_const(ee, i.rs.r, i.rt.r, &kc, &ko) && fits_imm32(kc)) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, ko == i.rd.r);
                        CachedReg& rs = get_reg(ee, &uc, ko);

                        uc.add(rd.reg, rs.reg, Imm((int64_t)kc));

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);

                    if (!i.rs.r || !i.rt.r) {
                        CachedReg& src = get_reg(ee, &uc, i.rs.r ? i.rs.r : i.rt.r);

                        uc.mov(rd.reg, src.reg);
                    } else {
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                        CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                        uc.add(rd.reg, rs.reg, rt.reg);
                    }

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_DSUB:
                case I_DSUBU: {
                    if (!i.rd.r) continue;

                    uint64_t cs, ct;

                    if (reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &ct)) {
                        set_const(ee, &uc, i.rd.r, cs - ct);

                        continue;
                    }

                    uint64_t kt;

                    if (!reg_is_const(ee, i.rs.r, &cs) && reg_is_const(ee, i.rt.r, &kt) && fits_imm32((uint64_t)(0 - kt))) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, i.rs.r == i.rd.r);
                        CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                        uc.add(rd.reg, rs.reg, Imm((int64_t)(uint64_t)(0 - kt)));

                        ee->reg_cache[i.rd.r].constant = false;

                        continue;
                    }

                    bool sync = i.rs.r == i.rd.r || i.rt.r == i.rd.r;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, sync);
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.sub(rd.reg, rs.reg, rt.reg);

                    ee->reg_cache[i.rd.r].constant = false;
                } break;

                case I_MFHI:
                case I_MFHI1:
                case I_MFLO:
                case I_MFLO1: {
                    if (!i.rd.r) continue;

                    bool is_hi = i.id == I_MFHI || i.id == I_MFHI1;
                    bool is_p1 = i.id == I_MFHI1 || i.id == I_MFLO1;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, false);

                    uc.load_u64(rd.reg, is_hi ? (is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0])) : (is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0])));
                } break;

                case I_MTHI:
                case I_MTHI1:
                case I_MTLO:
                case I_MTLO1: {
                    bool is_hi = i.id == I_MTHI || i.id == I_MTHI1;
                    bool is_p1 = i.id == I_MTHI1 || i.id == I_MTLO1;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);

                    uc.store_u64(is_hi ? (is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]))
                                    : (is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0])), rs.reg);
                } break;

                case I_MFSA: {
                    if (!i.rd.r) continue;

                    CachedReg& rd = get_reg(ee, &uc, i.rd.r, false);

                    uc.load_u32(rd.reg, EE(sa));
                    uc.and_(rd.reg, rd.reg, Imm(0xf));
                } break;

                case I_MTSA:
                case I_MTSAB:
                case I_MTSAH: {
                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    ujit::Gp t = uc.new_gp32();

                    if (i.id == I_MTSA) {
                        uc.and_(t, rs.reg.r32(), Imm(0xf));
                    } else if (i.id == I_MTSAB) {
                        uc.xor_(t, rs.reg.r32(), Imm((uint32_t)i.i16));
                        uc.and_(t, t, Imm(15));
                    } else {
                        uc.xor_(t, rs.reg.r32(), Imm((uint32_t)i.i16));
                        uc.and_(t, t, Imm(7));
                        uc.shl(t, t, Imm(1));
                    }

                    uc.store_u32(EE(sa), t);
                } break;

                case I_MULT:
                case I_MULT1: {
                    bool is_p1 = i.id == I_MULT1;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp a = uc.new_gp64();
                    ujit::Gp b = uc.new_gp64();
                    ujit::Gp prod = uc.new_gp64();
                    ujit::Gp lo = uc.new_gp64();
                    ujit::Gp hi = uc.new_gp64();

                    sext32(uc, a, rs.reg);
                    sext32(uc, b, rt.reg);

                    uc.mul(prod, a, b);

                    sext32(uc, lo, prod);
                    uc.sar(hi, prod, Imm(32));

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);

                    if (i.rd.r) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, false);

                        uc.mov(rd.reg, lo);
                    }
                } break;

                case I_MULTU:
                case I_MULTU1: {
                    bool is_p1 = i.id == I_MULTU1;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp a = uc.new_gp64();
                    ujit::Gp b = uc.new_gp64();
                    ujit::Gp prod = uc.new_gp64();
                    ujit::Gp lo = uc.new_gp64();
                    ujit::Gp hi = uc.new_gp64();

                    uc.mov(a.r32(), rs.reg.r32());
                    uc.mov(b.r32(), rt.reg.r32());

                    uc.mul(prod, a, b);

                    sext32(uc, lo, prod);
                    uc.sar(hi, prod, Imm(32));

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);

                    if (i.rd.r) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, false);

                        uc.mov(rd.reg, lo);
                    }
                } break;

                case I_MADD:
                case I_MADD1:
                case I_MADDU:
                case I_MADDU1: {
                    bool is_p1 = i.id == I_MADD1 || i.id == I_MADDU1;
                    bool is_u  = i.id == I_MADDU || i.id == I_MADDU1;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp a = uc.new_gp64();
                    ujit::Gp b = uc.new_gp64();

                    // Unsigned variants zero-extend the 32-bit operands; signed ones
                    // sign-extend. The accumulate and output are identical.
                    if (is_u) {
                        uc.mov(a.r32(), rs.reg.r32());
                        uc.mov(b.r32(), rt.reg.r32());
                    } else {
                        sext32(uc, a, rs.reg);
                        sext32(uc, b, rt.reg);
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

                    sext32(uc, lo, acc);
                    uc.sar(hi, acc, Imm(32));

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);

                    if (i.rd.r) {
                        CachedReg& rd = get_reg(ee, &uc, i.rd.r, false);

                        uc.mov(rd.reg, lo);
                    }
                } break;

                case I_DIV:
                case I_DIV1: {
                    bool is_p1 = i.id == I_DIV1;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

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

                    sext32(uc, lo, q);
                    sext32(uc, hi, r);

                    uc.j(l_done);

                    uc.bind(l_zero);

                    ujit::Gp zq = uc.new_gp32();
                    uc.select(zq, Imm(1), Imm(-1), ujit::scmp_lt(s, Imm(0)));

                    sext32(uc, lo, zq);
                    sext32(uc, hi, s);

                    uc.j(l_done);

                    uc.bind(l_ovf);
                    uc.mov(lo, Imm((int64_t)(int32_t)0x80000000));
                    uc.mov(hi, Imm(0));

                    uc.bind(l_done);

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);
                } break;

                case I_DIVU:
                case I_DIVU1: {
                    bool is_p1 = i.id == I_DIVU1;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

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

                    sext32(uc, lo, q);
                    sext32(uc, hi, r);

                    uc.j(l_done);

                    uc.bind(l_zero);
                    uc.mov(lo, Imm((int64_t)-1));

                    sext32(uc, hi, s);

                    uc.bind(l_done);

                    uc.store_u64(is_p1 ? EE(lo.u64[1]) : EE(lo.u64[0]), lo);
                    uc.store_u64(is_p1 ? EE(hi.u64[1]) : EE(hi.u64[0]), hi);
                } break;

                case I_PLZCW: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrs = get_vec(ee, uc, i.rs.r);
                    ujit::Vec vrd = uc.new_vec128();

                    mmi::plzcw(uc, vrd, vrs);

                    set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case I_PSLLVW:
                case I_PSRLVW:
                case I_PSRAVW: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrs = get_vec(ee, uc, i.rs.r);
                    ujit::Vec vrt = get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrd = uc.new_vec128();

                    if (i.id == I_PSLLVW) {
                        mmi::psllvw(uc, vrd, vrs, vrt);
                    } else if (i.id == I_PSRLVW) {
                        mmi::psrlvw(uc, vrd, vrs, vrt);
                    } else {
                        mmi::psravw(uc, vrd, vrs, vrt);
                    }

                    set_vec(ee, uc, i.rd.r, vrd);
                } break;

                case I_PMFHLLW:  emit_mmi_wide(ee, uc, i, mmi::pmfhllw,  MMI_WR_RD, false); break;
                case I_PMFHLSH:  emit_mmi_wide(ee, uc, i, mmi::pmfhlsh,  MMI_WR_RD, false); break;
                case I_PMFHLLH:  emit_mmi_wide(ee, uc, i, mmi::pmfhllh,  MMI_WR_RD, false); break;
                case I_PMFHLSLW: emit_mmi_wide(ee, uc, i, mmi::pmfhlslw, MMI_WR_RD, false); break;

                case I_PMFHLUW: {
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

                    set_vec(ee, uc, i.rd.r, v);
                } break;

                case I_QFSRV: {
                    if (!i.rd.r) continue;

                    ujit::Vec vrt = get_vec(ee, uc, i.rt.r);
                    ujit::Vec vrs = get_vec(ee, uc, i.rs.r);

                    uc.v_storeu128(EE(qfsrv_buf[0]), vrt);
                    uc.v_storeu128(EE(qfsrv_buf[16]), vrs);

                    ujit::Gp off = uc.new_gp_ptr();

                    uc.load_u32(off.r32(), EE(sa));
                    uc.and_(off.r32(), off.r32(), Imm(15));

                    ujit::Gp base = uc.new_gp_ptr();

                    uc.lea(base, EE(qfsrv_buf[0]));

                    ujit::Vec out = uc.new_vec128();

                    uc.v_loadu128(out, ujit::mem_ptr(base, off, 0));

                    set_vec(ee, uc, i.rd.r, out);
                } break;

                case I_CFC1: {
                    if (!i.rt.r) continue;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, false);

                    if (D_FS >= 16) {
                        uc.load_u32(rt.reg, EE(fcr));

                        sext32(uc, rt.reg);
                    } else {
                        uc.mov(rt.reg, Imm((int64_t)0x2e30));
                    }
                } break;

                case I_CTC1: {
                    if (D_FS < 16) continue;

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    uc.store_u32(EE(fcr), rt.reg);
                } break;

                case I_CFC2: {
                    if (!i.rt.r) continue;

                    ujit::Gp vu = uc.new_gp_ptr();

                    uc.load_u64(vu, EE(vu0));

                    InvokeNode* inv = jit_invoke(
                        uc,
                        (uintptr_t)vu::read_vi,
                        FuncSignature::build<uint32_t, void*, int32_t>()
                    );

                    inv->set_arg(0, vu);
                    inv->set_arg(1, Imm(i.rd.r));

                    ujit::Gp res = uc.new_gp32();
                    inv->set_ret(0, res);

                    CachedReg& rt = get_reg(ee, &uc, i.rt.r, false);
                    sext32(uc, rt.reg, res);

                    ee->reg_cache[i.rt.r].constant = false;
                } break;

                case I_CTC2: {
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp val = uc.new_gp32();
                    ujit::Gp vu = uc.new_gp_ptr();

                    uc.mov(val, rt.reg.r32());
                    uc.load_u64(vu, EE(vu0));

                    InvokeNode* inv = jit_invoke(
                        uc,
                        (uintptr_t)vu::write_vi,
                        FuncSignature::build<void, void*, int32_t, uint32_t>()
                    );

                    inv->set_arg(0, vu);
                    inv->set_arg(1, Imm(i.rd.r));
                    inv->set_arg(2, val);

                    if (i.opcode & 1) {
                        InvokeNode* inv_lock = jit_invoke(
                            uc,
                            (uintptr_t)vu::is_interlocked,
                            FuncSignature::build<int32_t, void*>()
                        );

                        inv_lock->set_arg(0, vu);

                        ujit::Gp locked = uc.new_gp32();

                        inv_lock->set_ret(0, locked);

                        Label l_skip = uc.new_label();

                        uc.j(l_skip, ujit::test_z(locked));

                        InvokeNode* inv_exec = jit_invoke(
                            uc,
                            (uintptr_t)vu::execute_program_tpc,
                            FuncSignature::build<void, void*>()
                        );

                        inv_exec->set_arg(0, vu);

                        uc.bind(l_skip);
                    }
                } break;

                case I_ERET: {
                    ujit::Gp st = uc.new_gp32();
                    ujit::Gp t = uc.new_gp32();

                    uc.load_u32(st, EE(status));
                    uc.and_(t, st, Imm(SR_ERL));

                    ujit::Gp addr = uc.new_gp32();
                    ujit::Gp nst = uc.new_gp32();

                    Label l_erl = uc.new_label();
                    Label l_done = uc.new_label();

                    uc.j(l_erl, ujit::test_nz(t));

                    uc.load_u32(addr, EE(epc));
                    uc.and_(nst, st, Imm(~(uint32_t)SR_EXL));
                    uc.j(l_done);

                    uc.bind(l_erl);
                    uc.load_u32(addr, EE(errorepc));
                    uc.and_(nst, st, Imm(~(uint32_t)SR_ERL));

                    uc.bind(l_done);

                    Label l_store = uc.new_label();
                    Label l_skip  = uc.new_label();

                    ujit::Gp fs = uc.new_gp32();

                    uc.load_u32(fs, EE(fmv_skip));
                    uc.j(l_store, ujit::test_z(fs));

                    InvokeNode* eret_inv = jit_invoke(
                        uc,
                        (uintptr_t)skip_fmv,
                        FuncSignature::build<int, Ee*, uint32_t>()
                    );

                    eret_inv->set_arg(0, ee->state_ptr);
                    eret_inv->set_arg(1, addr);
                    eret_inv->set_ret(0, fs);

                    uc.j(l_skip, ujit::test_nz(fs));

                    uc.bind(l_store);
                    uc.store_u32(EE(next_pc), addr);
                    uc.bind(l_skip);

                    uc.store_u32(EE(status), nst);
                } break;

                case I_EI:
                case I_DI: {
                    ujit::Gp st = uc.new_gp32();
                    ujit::Gp t = uc.new_gp32();
                    ujit::Gp k = uc.new_gp32();

                    uc.load_u32(st, EE(status));
                    uc.and_(t, st, Imm(SR_EDI | SR_EXL | SR_ERL));
                    uc.and_(k, st, Imm(SR_KSU));

                    Label l_do = uc.new_label();
                    Label l_skip = uc.new_label();

                    uc.j(l_do, ujit::test_nz(t));
                    uc.j(l_skip, ujit::test_nz(k));

                    uc.bind(l_do);

                    ujit::Gp nv = uc.new_gp32();

                    if (i.id == I_EI) {
                        uc.or_(nv, st, Imm(SR_EIE));
                    } else {
                        uc.and_(nv, st, Imm(~(uint32_t)SR_EIE));
                    }

                    uc.store_u32(EE(status), nv);
                    uc.bind(l_skip);
                } break;

                case I_LWL:
                case I_LWR:
                case I_SWL:
                case I_SWR: {
                    bool is_load = i.id == I_LWL || i.id == I_LWR;

                    if (is_load && !i.rt.r) continue;

                    CachedReg& rs = get_reg(ee, &uc, i.rs.r);
                    CachedReg& rt = get_reg(ee, &uc, i.rt.r);

                    ujit::Gp addr = uc.new_gp32();

                    uc.mov(addr, Imm((int32_t)(int16_t)i.i16));
                    uc.add(addr, addr, rs.reg.r32());

                    ujit::Gp aligned = uc.new_gp32();
                    ujit::Gp shift = uc.new_gp32();
                    ujit::Gp mem = uc.new_gp32();

                    uc.and_(aligned, addr, Imm(~3));
                    uc.and_(shift, addr, Imm(3));

                    InvokeNode* rd = jit_invoke(uc, (uintptr_t)bus_read32, FuncSignature::build<uint64_t, Ee*, uint32_t>());
                    rd->set_arg(0, ee->state_ptr);
                    rd->set_arg(1, aligned);
                    rd->set_ret(0, mem);

                    if (i.id == I_LWL) {
                        uc.mov(rt.reg, lsw::lwl(uc, rt.reg, mem, shift));

                        ee->reg_cache[i.rt.r].constant = false;
                    } else if (i.id == I_LWR) {
                        uc.mov(rt.reg, lsw::lwr(uc, rt.reg, mem, shift));

                        ee->reg_cache[i.rt.r].constant = false;
                    } else {
                        ujit::Gp val = (i.id == I_SWL) ? lsw::swl(uc, rt.reg, mem, shift) : lsw::swr(uc, rt.reg, mem, shift);
                        ujit::Gp val64 = uc.new_gp64();
                        
                        uc.mov(val64.r32(), val);

                        InvokeNode* wr = jit_invoke(uc, (uintptr_t)bus_write32, FuncSignature::build<void, Ee*, uint32_t, uint64_t>());
                        wr->set_arg(0, ee->state_ptr);
                        wr->set_arg(1, aligned);
                        wr->set_arg(2, val64);
                    }
                } break;

                default: {
                    flush_reg_cache(ee, &uc);

                    InvokeNode* invoke_node = jit_invoke(
                        uc,
                        (uintptr_t)i.func,
                        FuncSignature::build<void, Ee*, Instruction&>()
                    );

                    invoke_node->set_arg(0, ee->state_ptr);
                    invoke_node->set_arg(1, Imm((uintptr_t)&i));

                    uc.store_zero_u64(EE(r[0].u64[0]));
                    uc.store_zero_u64(EE(r[0].u64[1]));
                } break;
            }
        }

        flush_reg_cache(ee, &uc);

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

    flush_reg_cache(ee, &uc);

    uc.ret();

    uc.end_func();

    Error err = uc.finalize();

    if (err != Error::kOk) {
        iris_error(ee, "JIT compilation error!");
        iris_error(ee, "Guest block at PC=0x{:08x}:", block->start_pc);

        char buf[128];

        dis::Dis ds;
        ds.print_opcode = true;
        ds.print_address = true;
        ds.pc = block->start_pc;

        for (const auto& i : block->instructions) {
            iris_error(ee, "  {}", dis::disassemble(buf, i.opcode, &ds));

            ds.pc += 4;
        }

        iris_fatal_error(ee, "Failed to finalize JIT compilation ({})", DebugUtils::error_as_string(err));

        block->func = nullptr;

        return;
    }

    Error err1 = ee->rt.add(&block->func, &code);

    if (err1 != Error::kOk) {
        iris_fatal_error(ee, "Failed to add JIT code to runtime ({})", DebugUtils::error_as_string(err1));

        block->func = nullptr;

        return;
    }

    // if (code.logger()) {
    //     char buf[128];

    //     dis::Dis ds;
    //     ds.print_opcode = true;
    //     ds.print_address = true;
    //     ds.pc = block->start_pc;

    //     iris_debug(ee, "\n <------- Guest disassembly for block at PC=0x{:08x}:", block->start_pc);

    //     for (const auto& i : block->instructions) {
    //         iris_debug(ee, "{}", dis::disassemble(buf, i.opcode, &ds));

    //         ds.pc += 4;
    //     }
    // }
}

static inline bool is_irq_pending(Ee* ee) {
    int irq_enabled = (ee->status & SR_IE) && (ee->status & SR_EIE) &&
        (!(ee->status & SR_EXL)) && (!(ee->status & SR_ERL));
    int int0_pending = (ee->status & SR_IM2) && (ee->cause & CAUSE_IP2);
    int int1_pending = (ee->status & SR_IM3) && (ee->cause & CAUSE_IP3);

    return irq_enabled && (int0_pending || int1_pending);
}

static inline int _ee_run_block(Ee* ee, int budget, int compile_hint) {
    int total = 0;

    while (true) {
        if (ee->breakpoint_count) {
            if (ee->pc == ee->bp_skip_pc) {
                ee->bp_skip_pc = 0xffffffff;
            } else if (has_breakpoint(ee, ee->pc)) {
                ee->bp_hit = true;
                ee->bp_hit_pc = ee->pc;

                break;
            }
        }

        Block* block = find_block(ee, ee->pc);

        if (!block) {
            ee->cache_misses++;

            block = cache_block(ee, compile_hint);

            compile_block(ee, block);
        } else {
            ee->cache_hits++;
        }

        if (!block->func) break;

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
            iris_debug(ee, "Purging cache");

            purge_cache(ee);

            ee->pending_purge = false;

            break;
        }

        if (total >= budget)
            break;

        if (ee->pc == 0x81fc0 || ee->intc_reads >= 10000 || ee->csr_reads >= 10000)
            break;

        if (is_irq_pending(ee))
            break;
    }

    return total;
}

bool breakpoint_hit(Ee* ee) {
    return ee->bp_hit;
}

void set_breakpoints(Ee* ee, const uint32_t* addrs, int count) {
    if (count > EE_MAX_BREAKPOINTS)
        count = EE_MAX_BREAKPOINTS;

    ee->breakpoint_count = count;

    for (int i = 0; i < count; i++)
        ee->breakpoints[i] = addrs[i];

    ee->bp_hit = false;
    ee->bp_skip_pc = 0xffffffff;

    purge_cache(ee);
}

int run_block(Ee* ee, int max_cycles) {
    if (ee->bp_hit)
        return 0;

    if (is_irq_pending(ee)) {
        int cycles = _ee_run_block(ee, 1, 4);

        exception_level1(ee, CAUSE_EXC1_INT);

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
    return _ee_run_block(ee, max_cycles, BLOCK_MAX_INSTRS);
}

int step(Ee* ee) {
    static Instruction i;

    ee->delay_slot = ee->branch;
    ee->branch = 0;

    // Would check for interrupts here, but we do this outside of the core
    // to reduce overhead
    check_irq(ee);

    ee->prev_pc = ee->pc;
    ee->opcode = bus_read32(ee, ee->pc);
    ee->pc = ee->next_pc;
    ee->next_pc += 4;

    i = decode(ee->opcode);

    i.func(ee, i);

    ++ee->total_cycles;
    ++ee->count;

    ee->r[0].u64[0] = 0;
    ee->r[0].u64[1] = 0;

    return 1;
}

void flush_cache(Ee* ee) {
    vfast_clear(ee);

    if (ee->block_cache.empty())
        return;

    for (int i = 0; i < CACHE_PAGECOUNT; i++) {
        ee->block_cache[i].dirty = true;
    }

    ee->last_block_lookup_pc = ~0u;
    ee->last_block_ptr = nullptr;
    ee->block_lut_gen++;
}

uint32_t get_pc(Ee* ee) {
    return ee->pc;
}

ram::Ram* get_spr(Ee* ee) {
    return ee->spr;
}

void set_fmv_skip(Ee* ee, int v) {
    ee->fmv_skip = v;
}

void set_boot_args(Ee* ee, const char* const* args, int count) {
    ee->boot_argc = 0;
    ee->boot_args_pending = 0;

    if (!args || count <= 0)
        return;

    if (count > (int)(sizeof(ee->boot_args) / sizeof(ee->boot_args[0])))
        return;

    for (int i = 0; i < count; i++) {
        strncpy(ee->boot_args[i], args[i], sizeof(ee->boot_args[0]) - 1);

        ee->boot_args[i][sizeof(ee->boot_args[0]) - 1] = '\0';
    }

    ee->boot_argc = count;
    ee->boot_args_pending = 1;
}

void reset_intc_reads(Ee* ee) {
    ee->intc_reads = 0;
}

void reset_csr_reads(Ee* ee) {
    ee->csr_reads = 0;
}

void set_ram_size(Ee* ee, int ram_size) {
    ee->ram_size = ram_size - 1;
}

void set_osd_config(Ee* ee, OsdConfig config) {
    ee->osd_config = config;
}

OsdConfig get_osd_config(Ee* ee) {
    return ee->osd_config;
}

void invalidate_block(Ee* ee, uint32_t addr) {
    uint32_t page = addr / MIN_PAGESIZE;

    // if (ee->block_cache[page].valid && !ee->block_cache[page].dirty) {
    //     iris_debug(ee, "Invalidating block at address 0x{:08x}", addr);
    // }

    ee->block_cache[page].dirty = true;
    ee->block_lut_gen++;
}

void invalidate_range(Ee* ee, uint32_t addr, uint32_t size) {
    for (uint32_t i = 0; i < size; i += MIN_PAGESIZE) {
        invalidate_block(ee, addr + i);
    }
}

}
