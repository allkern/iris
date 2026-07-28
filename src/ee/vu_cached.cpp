#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fenv.h>
#include <utility>

#if defined(__SSE2__) || defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
#include <immintrin.h>
#define VU_FMAC_SIMD 1
#endif

#include "vu.h"
#include "vu_def.hpp"
#include "vu_dis.h"

// #define printf(fmt, ...)(0)

#define VU_LD_DI(i) (ins->ld_di[i])
#define VU_LD_D (ins->ld_d)
#define VU_LD_S (ins->ld_s)
#define VU_LD_T (ins->ld_t)
#define VU_LD_SF (ins->ld_sf)
#define VU_LD_TF (ins->ld_tf)
#define VU_LD_IMM5 (ins->ld_imm5)
#define VU_LD_IMM11 (ins->ld_imm11)
#define VU_LD_IMM12 (ins->ld_imm12)
#define VU_LD_IMM15 (ins->ld_imm15)
#define VU_LD_IMM24 (ins->ld_imm24)
#define VU_ID vu->vi[VU_LD_D]
#define VU_IS vu->vi[VU_LD_S]
#define VU_IT vu->vi[VU_LD_T]
#define VU_UD_DI(i) (ins->ud_di[i])
#define VU_UD_D (ins->ud_d)
#define VU_UD_S (ins->ud_s)
#define VU_UD_T (ins->ud_t)
#define VU_D_FLD (0x01e00000)
#define VU_D_X (0x01000000)
#define VU_D_Y (0x00800000)
#define VU_D_Z (0x00400000)
#define VU_D_W (0x00200000)

[[noreturn]] inline void unreachable() {
    // Uses compiler specific extensions if possible.
    // Even if no extension is used, undefined behavior is still raised by
    // an empty function body and the noreturn attribute.
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
    __assume(false);
#else // GCC, Clang
    __builtin_unreachable();
#endif
}

struct vu_state* vu_create(void) {
    return new struct vu_state;
}

void vu_init(struct vu_state* vu, int id, struct ps2_gif* gif, struct ps2_vif* vif, struct vu_state* vu1) {
    vu->id = id;
    vu->vu1 = vu1;
    vu->vif = vif;
    vu->gif = gif;

    if (!id) {
        vu->micro_mem_size = 0x1ff;
        vu->vu_mem_size = 0xff;
    } else {
        vu->micro_mem_size = 0x7ff;
        vu->vu_mem_size = 0x3ff;
    }

    vu->vf[0].x = 0.0;
    vu->vf[0].y = 0.0;
    vu->vf[0].z = 0.0;
    vu->vf[0].w = 1.0;

    ps2_vu_reset(vu);

    // VU uses round to zero by default
    fesetround(FE_TOWARDZERO);

    vu->block_cache_size = 0;
    vu->block_cache.clear();
    vu->block_cache.resize(vu->micro_mem_size+1);
}

void vu_destroy(struct vu_state* vu) {
    delete vu;
}

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

static inline uint32_t vu_max(int32_t a, int32_t b) {
    return (a < 0 && b < 0) ? min(a, b) : max(a, b);
}

static inline uint32_t vu_min(int32_t a, int32_t b) {
    return (a < 0 && b < 0) ? max(a, b) : min(a, b);
}

static inline float vu_atan(float t) {
    //In reality, VU1 uses an approximation to derive the result. This is shown here.
    const static float atan_const[] = {
        0.999999344348907f, -0.333298563957214f,
        0.199465364217758f, -0.139085337519646f,
        0.096420042216778f, -0.055909886956215f,
        0.021861229091883f, -0.004054057877511f
    };

    float result = 0.785398185253143f; // pi/4

    for (int i = 0; i < 8; i++) {
        result += atan_const[i] * powf(t, (i * 2) + 1);
    }

    return result;
}

static inline void vu_update_status(struct vu_state* vu) {
    vu->status &= ~0x3f;

    vu->status |= (vu->mac_pipeline[3] & 0x000f) ? 1 : 0;
    vu->status |= (vu->mac_pipeline[3] & 0x00f0) ? 2 : 0;
    vu->status |= (vu->mac_pipeline[3] & 0x0f00) ? 4 : 0;
    vu->status |= (vu->mac_pipeline[3] & 0xf000) ? 8 : 0;

    vu->status |= (vu->status & 0x3f) << 6;
}

static inline void vu_set_q(struct vu_state* vu, float value, int delay) {
    if (vu->q_delay == 0) {
        vu->prev_q.f = vu->q.f;
    }

    vu->q.f = value;
    vu->q_delay = delay;
}

static inline struct vu_reg32 vu_get_q(struct vu_state* vu) {
    if (!vu->q_delay) {
        return vu->q;
    }

    return vu->prev_q;
}

static inline float vu_update_flags(struct vu_state* vu, float value, int index) {
    uint32_t value_u = *(uint32_t*)&value;

    int flag_id = 3 - index;

    // Sign flag
    if (value_u & 0x80000000)
        vu->mac |= 0x10 << flag_id;
    else
        vu->mac &= ~(0x10 << flag_id);

    // Zero flag, clear under/overflow
    if ((value_u & 0x7FFFFFFF) == 0) {
        vu->mac |= 1 << flag_id;
        vu->mac &= ~(0x1100 << flag_id);

        return value;
    }

    switch ((value_u >> 23) & 0xFF) {
        //Underflow, set zero
        case 0:
            vu->mac |= 0x101 << flag_id;
            vu->mac &= ~(0x1000 << flag_id);
            value_u = value_u & 0x80000000;
            break;
        //Overflow
        case 255:
            vu->mac |= 0x1000 << flag_id;
            vu->mac &= ~(0x101 << flag_id);
            value_u = (value_u & 0x80000000) | 0x7F7FFFFF;
            break;
        //Clear all but sign
        default:
            vu->mac &= ~(0x1101 << flag_id);
            break;
    }

    return *(float*)&value_u;
}

static inline void vu_clear_flags(struct vu_state* vu, int index) {
    vu->mac &= ~(0x1111 << (3 - index));
}

static inline float vu_cvtf(uint32_t value) {
    switch (value & 0x7f800000) {
        case 0x0: {
            value &= 0x80000000;

            return *(float*)&value;
        } break;

        case 0x7f800000: {
            uint32_t result = (value & 0x80000000) | 0x7f7fffff;

            return *(float*)&result;
        }
    }

    return *(float*)&value;
}

int32_t vu_cvti(float value) {
    if (value >= 2147483647.0)
        return 2147483647LL;

    if (value <= -2147483648.0)
        return -2147483648LL;

    return (int32_t)value;
}

static inline double ps2_to_double(uint32_t bits) {
    int e = (bits >> 23) & 0xff;

    if (e == 0) {
        return (bits & 0x80000000) ? -0.0 : 0.0;
    }

    double m = 1.0 + (double)(bits & 0x7fffff) / 8388608.0;
    double v = ldexp(m, e - 127);

    return (bits & 0x80000000) ? -v : v;
}

static inline uint32_t ps2_pack_double(double v) {
    uint32_t sign = signbit(v) ? 0x80000000u : 0u;

    double a = fabs(v);

    if (a == 0.0) {
        return sign;
    }

    int e;
    double m = frexp(a, &e);
    int biased = e - 1 + 127;

    if (biased > 255) {
        return sign | 0x7fffffff;
    }

    if (biased < 1) {
        return sign;
    }

    uint32_t mantissa = (uint32_t)((m * 2.0 - 1.0) * 8388608.0 + 0.5);

    if (mantissa > 0x7fffff) {
        mantissa = 0;
        if (++biased > 255) {
            return sign | 0x7fffffff;
        }
    }

    return sign | ((uint32_t)biased << 23) | mantissa;
}

static inline void vu_set_q_u32(struct vu_state* vu, uint32_t bits, int delay) {
    if (vu->q_delay == 0)
        vu->prev_q = vu->q;

    vu->q.u32 = bits;
    vu->q_delay = delay;
}

#define VU_STATUS_I 0x0410
#define VU_STATUS_D 0x0820

static inline void vu_set_vf(struct vu_state* vu, int r, int f, float v) {
    if (r) vu->vf[r].f[f] = v;
}

static inline void vu_set_vfu(struct vu_state* vu, int r, int f, int32_t v) {
    if (r) vu->vf[r].s32[f] = v;
}

static inline void vu_set_vf_x(struct vu_state* vu, int r, float v) {
    if (r) vu->vf[r].x = v;
}

static inline void vu_set_vf_y(struct vu_state* vu, int r, float v) {
    if (r) vu->vf[r].y = v;
}

static inline void vu_set_vf_z(struct vu_state* vu, int r, float v) {
    if (r) vu->vf[r].z = v;
}

static inline void vu_set_vf_w(struct vu_state* vu, int r, float v) {
    if (r) vu->vf[r].w = v;
}

static inline void vu_set_vi(struct vu_state* vu, int r, uint16_t v) {
    r &= 0xf;

    if (r) vu->vi[r] = v;
}

static inline float vu_vf_i(struct vu_state* vu, int r, int i) {
    return vu_cvtf(vu->vf[r].u32[i]);
}

static inline float vu_vf_x(struct vu_state* vu, int r) {
    return vu_cvtf(vu->vf[r].u32[0]);
}

static inline float vu_vf_y(struct vu_state* vu, int r) {
    return vu_cvtf(vu->vf[r].u32[1]);
}

static inline float vu_vf_z(struct vu_state* vu, int r) {
    return vu_cvtf(vu->vf[r].u32[2]);
}

static inline float vu_vf_w(struct vu_state* vu, int r) {
    return vu_cvtf(vu->vf[r].u32[3]);
}

static inline float vu_acc_i(struct vu_state* vu, int i) {
    return vu_cvtf(vu->acc.u32[i]);
}

static inline void vu_mem_write(struct vu_state* vu, uint16_t addr, uint32_t data, int i) {
    if (!vu->id) {
        if (addr <= 0x3ff) {
            vu->vu_mem[addr & 0xff].u32[i] = data;
        } else {
            if ((addr >= 0x400) && (addr <= 0x41f)) {
                vu->vu1->vf[addr & 0x1f].u32[i] = data;
            } else if ((addr >= 0x420) && (addr <= 0x42f)) {
                vu->vu1->vi[addr & 0xf] = data;
            } else if (addr == 0x430) {
                vu->vu1->status = data;
            } else if (addr == 0x431) {
                vu->vu1->mac = data;
            } else if (addr == 0x432) {
                vu->vu1->clip = data;
            } else if (addr == 0x434) {
                vu->vu1->r.u32 = data;
            } else if (addr == 0x435) {
                vu->vu1->i.u32 = data;
            } else if (addr == 0x436) {
                vu->vu1->q.u32 = data;
            } else if (addr == 0x437) {
                vu->vu1->p.u32 = data;
            } else if (addr == 0x43a) {
                vu->vu1->tpc = data;
            } else {
                // printf("vu: oob write\n");

                // exit(1);
            }
        }
    } else {
        // if (addr == 0x000001d3) *(int*)0 = 0;

        vu->vu_mem[addr & 0x3ff].u32[i] = data;
    }
}

static inline uint128_t vu_mem_read(struct vu_state* vu, uint32_t addr) {
    if (!vu->id) {
        if (addr <= 0x3ff) {
            return vu->vu_mem[addr & 0xff];
        } else {
            if ((addr >= 0x400) && (addr <= 0x41f)) {
                return vu->vu1->vf[addr & 0x1f].u128;
            } else if ((addr >= 0x420) && (addr <= 0x42f)) {
                uint128_t result; result.u32[0] = vu->vu1->vi[addr & 0xf];
                return result;
            } else if (addr == 0x430) {
                uint128_t result; result.u32[0] = vu->vu1->status;
                return result;
            } else if (addr == 0x431) {
                uint128_t result; result.u32[0] = vu->vu1->mac;
                return result;
            } else if (addr == 0x432) {
                uint128_t result; result.u32[0] = vu->vu1->clip;
                return result;
            } else if (addr == 0x434) {
                uint128_t result; result.u32[0] = vu->vu1->r.u32;
                return result;
            } else if (addr == 0x435) {
                uint128_t result; result.u32[0] = vu->vu1->i.u32;
                return result;
            } else if (addr == 0x436) {
                uint128_t result; result.u32[0] = vu->vu1->q.u32;
                return result;
            } else if (addr == 0x437) {
                uint128_t result; result.u32[0] = vu->vu1->p.u32;
                return result;
            } else if (addr == 0x43a) {
                uint128_t result; result.u32[0] = vu->vu1->tpc;
                return result;
            }
        }
    }

    return vu->vu_mem[addr & 0x3ff];
}

// Note: Branches take effect one instruction late, so a branch sitting in the delay
//       slot of another branch behaves normally, it computes its target from its own
//       PC. Except that its delay slot is whichever instruction the first branch
//       causes to be fetched next.
//
//       Let B1 be a branch at address a, B2 a branch at a+1 (B1's delay slot), and X
//       the address B1 resolves to (TGT1 if B1 is taken, a+2 otherwise). Execution is
//       always:
//
//           B1, B2, instruction at X, then TGT2 if B2 is taken, else X+1
//
//       which subsumes all four taken/not-taken combinations. A branch at X chains the
//       same way. Note TGT2 is relative to B2, it is never re-based onto TGT1.
//
//       Consequences: BAL/JALR in a delay slot link to X+1 rather than a+2, and JR/JALR
//       in a delay slot are taken like any other branch.
//
//       VU branches in delay slots are fairly common. Crazy Taxi and 18 Wheeler need
//       this, otherwise you get flickering geometry and the VU might eventually hang.
static inline void vu_branch(struct vu_state* vu, uint32_t target) {
    target &= 0x7ff;

    if (vu->branch_delay) {
        vu->delay_branch = true;
        vu->delay_branch_pc = target;

        return;
    }

    vu->branch_delay = 2;
    vu->branch_pc = target;
}

static inline uint32_t vu_delay_slot_link(struct vu_state* vu) {
    return ((vu->branch_delay ? vu->branch_pc : vu->tpc) + 1) & 0x7ff;
}

static inline void vu_write_branch_pipeline(struct vu_state* vu, int dst) {
    if (!dst)
        return;

    // On repeat writes we need to remember the value from before the chain
    if (vu->vi_backup_cycles && dst == vu->vi_backup_reg) {
        vu->vi_backup_cycles = 2;

        return;
    }

    vu->vi_backup_cycles = 2;
    vu->vi_backup_reg = dst;
    vu->vi_backup_value = vu->vi[dst];

    // printf("branch pipeline: dst=%d prev=%04x rw=%d\n",
    //     vu->branch_pipeline_curr.reg, vu->branch_pipeline_curr.prev,
    //     vu->branch_pipeline_curr.rw
    // );
}

static inline uint16_t vu_get_branch_register(struct vu_state* vu, int reg) {
    if (vu->vi_backup_cycles && (vu->vi_backup_reg == reg)) {
        return vu->vi_backup_value;
    }

    return vu->vi[reg];
}

void vu_xgkick(struct vu_state* vu) {
    uint16_t addr = vu->xgkick_addr;

    int eop = 1;

    do {
        uint128_t tag = vu_mem_read(vu, addr++);

        if ((tag.u64[0] | tag.u64[1]) == 0)
            break;

        // addr &= 0x3ff;

        // if (addr == 0) break;

        // printf("tag: addr=%08x %08x %08x %08x %08x\n", addr - 1, tag.u32[3], tag.u32[2], tag.u32[1], tag.u32[0]);

        eop = (tag.u64[0] & 0x8000) != 0;

        int nloop = tag.u64[0] & 0x7fff;
        int flg = (tag.u64[0] >> 58) & 3;
        int nregs = (tag.u64[0] >> 60) & 0xf;

        if (!nloop)
            continue;

        if (!nregs)
            nregs = 16;

        int qwc = 0;

        switch (flg) {
            case 0: {
                qwc = nregs * nloop;
            } break;
            case 1: {
                qwc = (nregs * nloop + 1) / 2; // Round up for odd cases
            } break;
            case 2:
            case 3: {
                qwc = nloop;
            } break;
        }

        if (qwc >= 0x400) {
            return;

            fprintf(stderr, "vu: Weird xgkick tag nloop=%d nregs=%d eop=%d flg=%d qwc=%d\n",
                nloop,
                nregs,
                eop,
                flg,
                qwc
            ); 

            exit(1);
        }

        ps2_gif_fifo_write(vu->gif, tag, GIF_PATH1);


        for (int i = 0; i < qwc; i++) {
            // printf("vu: %08x: %08x %08x %08x %08x\n",
            //     addr,
            //     vu->vu_mem[addr].u32[3],
            //     vu->vu_mem[addr].u32[2],
            //     vu->vu_mem[addr].u32[1],
            //     vu->vu_mem[addr].u32[0]
            // );

            ps2_gif_fifo_write(vu->gif, vu_mem_read(vu, addr++), GIF_PATH1);

            addr &= 0x3ff;

            // if (addr == 0) {
            //     eop = 1;
            //     break;
            // }
        }
    } while (!eop);
}

template <typename F, std::size_t... Is>
void seq(F f, std::index_sequence<Is...>) {
    (f(std::integral_constant<std::size_t, Is>{}), ...);
}

template <size_t N, typename F>
void template_seq(F f) {
    seq(f, std::make_index_sequence<N>{});
}

// Upper pipeline
template <uint32_t di>
void vu_i_abs(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu_set_vf(vu, t, i, fabsf(vu_vf_i(vu, s, i)));
        }
    });
}

enum vu_alu_op { VU_OP_ADD, VU_OP_SUB, VU_OP_MUL, VU_OP_MADD, VU_OP_MSUB };
enum vu_src_kind { VU_SRC_VEC, VU_SRC_BCX, VU_SRC_BCY, VU_SRC_BCZ, VU_SRC_BCW, VU_SRC_I, VU_SRC_Q };

#ifdef VU_FMAC_SIMD

static inline __m128i vu_sel(__m128i mask, __m128i a, __m128i b) {
    return _mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b));
}

static inline __m128i vu_cvtf4_i(__m128i v) {
    const __m128i em = _mm_set1_epi32(0x7f800000);
    const __m128i sm = _mm_set1_epi32((int)0x80000000);
    const __m128i maxn = _mm_set1_epi32(0x7f7fffff);
    const __m128i z = _mm_setzero_si128();

    __m128i exp = _mm_and_si128(v, em);
    __m128i is0 = _mm_cmpeq_epi32(exp, z);
    __m128i is255 = _mm_cmpeq_epi32(exp, em);
    __m128i sign = _mm_and_si128(v, sm);

    v = vu_sel(is0, sign, v);
    v = vu_sel(is255, _mm_or_si128(sign, maxn), v);

    return v;
}

static inline __m128 vu_load_cvtf(const struct vu_reg128* r) {
    return _mm_castsi128_ps(vu_cvtf4_i(_mm_loadu_si128((const __m128i*)r)));
}

template <uint32_t di>
static constexpr uint32_t vu_nibblemask() {
    uint32_t nm = 0;
    for (int i = 0; i < 4; i++) if (di & (VU_D_X >> i)) nm |= 0x1111u << (3 - i);
    return nm;
}

template <uint32_t di>
static inline __m128i vu_flags4(struct vu_state* vu, __m128i v) {
    const __m128i em = _mm_set1_epi32(0x7f800000);
    const __m128i sm = _mm_set1_epi32((int)0x80000000);
    const __m128i mtm = _mm_set1_epi32(0x007fffff);
    const __m128i maxn = _mm_set1_epi32(0x7f7fffff);
    const __m128i z = _mm_setzero_si128();
    const __m128i ones = _mm_set1_epi32(-1);

    __m128i exp = _mm_and_si128(v, em);
    __m128i mant = _mm_and_si128(v, mtm);
    __m128i sign = _mm_and_si128(v, sm);

    __m128i is0 = _mm_cmpeq_epi32(exp, z);
    __m128i is255 = _mm_cmpeq_epi32(exp, em);
    __m128i mnz = _mm_xor_si128(_mm_cmpeq_epi32(mant, z), ones);
    __m128i isuf = _mm_and_si128(is0, mnz);

    __m128i r = vu_sel(is0, sign, v);

    r = vu_sel(is255, _mm_or_si128(sign, maxn), r);

    __m128i Z = _mm_srli_epi32(is0, 31);
    __m128i S = _mm_srli_epi32(v, 31);
    __m128i U = _mm_srli_epi32(isuf, 31);
    __m128i O = _mm_srli_epi32(is255, 31);
    __m128i lanebits = _mm_or_si128(_mm_or_si128(Z, _mm_slli_epi32(S, 4)),
                                    _mm_or_si128(_mm_slli_epi32(U, 8), _mm_slli_epi32(O, 12)));
    __m128i sh = _mm_mullo_epi16(lanebits, _mm_set_epi32(1, 2, 4, 8));
    __m128i h1 = _mm_or_si128(sh, _mm_shuffle_epi32(sh, _MM_SHUFFLE(2, 3, 0, 1)));
    __m128i h2 = _mm_or_si128(h1, _mm_shuffle_epi32(h1, _MM_SHUFFLE(1, 0, 3, 2)));

    uint32_t newmac = (uint32_t)_mm_cvtsi128_si32(h2) & 0xFFFFu;

    vu->mac = (vu->mac & ~0xFFFFu) | (newmac & vu_nibblemask<di>());

    return r;
}

template <uint32_t di>
static inline void vu_write_masked(void* dst, __m128i val) {
    constexpr uint32_t l0 = (di & VU_D_X) ? 0xFFFFFFFFu : 0;
    constexpr uint32_t l1 = (di & VU_D_Y) ? 0xFFFFFFFFu : 0;
    constexpr uint32_t l2 = (di & VU_D_Z) ? 0xFFFFFFFFu : 0;
    constexpr uint32_t l3 = (di & VU_D_W) ? 0xFFFFFFFFu : 0;

    if constexpr (l0 && l1 && l2 && l3) {
        _mm_storeu_si128((__m128i*)dst, val);
    } else if constexpr (l0 || l1 || l2 || l3) {
        const __m128i keep = _mm_set_epi32((int)l3, (int)l2, (int)l1, (int)l0);

        __m128i cur = _mm_loadu_si128((__m128i*)dst);

        _mm_storeu_si128((__m128i*)dst, vu_sel(keep, val, cur));
    }
}

template <uint32_t di, vu_alu_op OP, bool TO_ACC, vu_src_kind TK>
static inline void vu_fmac(struct vu_state* vu, int d, int s, int t) {
    __m128 sv = vu_load_cvtf(&vu->vf[s]);
    __m128 tv;

    if constexpr (TK == VU_SRC_VEC) {
        tv = vu_load_cvtf(&vu->vf[t]);
    } else if constexpr (TK == VU_SRC_BCX) {
        __m128 tf = vu_load_cvtf(&vu->vf[t]); tv = _mm_shuffle_ps(tf, tf, _MM_SHUFFLE(0, 0, 0, 0));
    } else if constexpr (TK == VU_SRC_BCY) {
        __m128 tf = vu_load_cvtf(&vu->vf[t]); tv = _mm_shuffle_ps(tf, tf, _MM_SHUFFLE(1, 1, 1, 1));
    } else if constexpr (TK == VU_SRC_BCZ) {
        __m128 tf = vu_load_cvtf(&vu->vf[t]); tv = _mm_shuffle_ps(tf, tf, _MM_SHUFFLE(2, 2, 2, 2));
    } else if constexpr (TK == VU_SRC_BCW) {
        __m128 tf = vu_load_cvtf(&vu->vf[t]); tv = _mm_shuffle_ps(tf, tf, _MM_SHUFFLE(3, 3, 3, 3));
    } else if constexpr (TK == VU_SRC_I) {
        tv = _mm_set1_ps(vu->i.f);
    } else {
        tv = _mm_set1_ps(vu_get_q(vu).f);
    }

    __m128 r;
    if constexpr (OP == VU_OP_ADD) {
        r = _mm_add_ps(sv, tv);
    } else if constexpr (OP == VU_OP_SUB) {
        r = _mm_sub_ps(sv, tv);
    } else if constexpr (OP == VU_OP_MUL) {
        r = _mm_mul_ps(sv, tv);
    } else {
        __m128 acc = vu_load_cvtf(&vu->acc);
        __m128 p = _mm_mul_ps(sv, tv);
        r = (OP == VU_OP_MADD) ? _mm_add_ps(acc, p) : _mm_sub_ps(acc, p);
    }

    __m128i clamped = vu_flags4<di>(vu, _mm_castps_si128(r));

    if constexpr (TO_ACC) {
        vu_write_masked<di>(&vu->acc, clamped);
    } else {
        if (d) vu_write_masked<di>(&vu->vf[d], clamped);
    }

    vu_update_status(vu);
}

#else

template <uint32_t di, vu_alu_op OP, bool TO_ACC, vu_src_kind TK>
static inline void vu_fmac(struct vu_state* vu, int d, int s, int t) {
    float iq = 0.0f;
    if constexpr (TK == VU_SRC_I) iq = vu->i.f;
    else if constexpr (TK == VU_SRC_Q) iq = vu_get_q(vu).f;

    float bc = 0.0f;
    if constexpr (TK == VU_SRC_BCX) bc = vu_vf_x(vu, t);
    else if constexpr (TK == VU_SRC_BCY) bc = vu_vf_y(vu, t);
    else if constexpr (TK == VU_SRC_BCZ) bc = vu_vf_z(vu, t);
    else if constexpr (TK == VU_SRC_BCW) bc = vu_vf_w(vu, t);

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            float sf = vu_vf_i(vu, s, i);
            float tf;
            if constexpr (TK == VU_SRC_VEC) tf = vu_vf_i(vu, t, i);
            else if constexpr (TK == VU_SRC_I || TK == VU_SRC_Q) tf = iq;
            else tf = bc;

            float r;
            if constexpr (OP == VU_OP_ADD) r = sf + tf;
            else if constexpr (OP == VU_OP_SUB) r = sf - tf;
            else if constexpr (OP == VU_OP_MUL) r = sf * tf;
            else if constexpr (OP == VU_OP_MADD) r = vu_acc_i(vu, i) + sf * tf;
            else r = vu_acc_i(vu, i) - sf * tf;

            float out = vu_update_flags(vu, r, i);
            if constexpr (TO_ACC) vu->acc.f[i] = out;
            else vu_set_vf(vu, d, i, out);
        } else {
            vu_clear_flags(vu, i);
        }
    });

    vu_update_status(vu);
}

#endif

#define VU_FMAC_D(name, OP, TK) \
    template <uint32_t di> \
    void name(struct vu_state* vu, const struct vu_instruction* ins) { \
        vu_fmac<di, OP, false, TK>(vu, VU_UD_D, VU_UD_S, VU_UD_T); \
    }

#define VU_FMAC_A(name, OP, TK) \
    template <uint32_t di> \
    void name(struct vu_state* vu, const struct vu_instruction* ins) { \
        vu_fmac<di, OP, true, TK>(vu, VU_UD_D, VU_UD_S, VU_UD_T); \
    }

VU_FMAC_D(vu_i_add,   VU_OP_ADD, VU_SRC_VEC)
VU_FMAC_D(vu_i_addi,  VU_OP_ADD, VU_SRC_I)
VU_FMAC_D(vu_i_addq,  VU_OP_ADD, VU_SRC_Q)
VU_FMAC_D(vu_i_addx,  VU_OP_ADD, VU_SRC_BCX)
VU_FMAC_D(vu_i_addy,  VU_OP_ADD, VU_SRC_BCY)
VU_FMAC_D(vu_i_addz,  VU_OP_ADD, VU_SRC_BCZ)
VU_FMAC_D(vu_i_addw,  VU_OP_ADD, VU_SRC_BCW)
VU_FMAC_A(vu_i_adda,  VU_OP_ADD, VU_SRC_VEC)
VU_FMAC_A(vu_i_addai, VU_OP_ADD, VU_SRC_I)
VU_FMAC_A(vu_i_addaq, VU_OP_ADD, VU_SRC_Q)
VU_FMAC_A(vu_i_addax, VU_OP_ADD, VU_SRC_BCX)
VU_FMAC_A(vu_i_adday, VU_OP_ADD, VU_SRC_BCY)
VU_FMAC_A(vu_i_addaz, VU_OP_ADD, VU_SRC_BCZ)
VU_FMAC_A(vu_i_addaw, VU_OP_ADD, VU_SRC_BCW)

VU_FMAC_D(vu_i_sub,   VU_OP_SUB, VU_SRC_VEC)
VU_FMAC_D(vu_i_subi,  VU_OP_SUB, VU_SRC_I)
VU_FMAC_D(vu_i_subq,  VU_OP_SUB, VU_SRC_Q)
VU_FMAC_D(vu_i_subx,  VU_OP_SUB, VU_SRC_BCX)
VU_FMAC_D(vu_i_suby,  VU_OP_SUB, VU_SRC_BCY)
VU_FMAC_D(vu_i_subz,  VU_OP_SUB, VU_SRC_BCZ)
VU_FMAC_D(vu_i_subw,  VU_OP_SUB, VU_SRC_BCW)
VU_FMAC_A(vu_i_suba,  VU_OP_SUB, VU_SRC_VEC)
VU_FMAC_A(vu_i_subai, VU_OP_SUB, VU_SRC_I)
VU_FMAC_A(vu_i_subaq, VU_OP_SUB, VU_SRC_Q)
VU_FMAC_A(vu_i_subax, VU_OP_SUB, VU_SRC_BCX)
VU_FMAC_A(vu_i_subay, VU_OP_SUB, VU_SRC_BCY)
VU_FMAC_A(vu_i_subaz, VU_OP_SUB, VU_SRC_BCZ)
VU_FMAC_A(vu_i_subaw, VU_OP_SUB, VU_SRC_BCW)

VU_FMAC_D(vu_i_mul,   VU_OP_MUL, VU_SRC_VEC)
VU_FMAC_D(vu_i_muli,  VU_OP_MUL, VU_SRC_I)
VU_FMAC_D(vu_i_mulq,  VU_OP_MUL, VU_SRC_Q)
VU_FMAC_D(vu_i_mulx,  VU_OP_MUL, VU_SRC_BCX)
VU_FMAC_D(vu_i_muly,  VU_OP_MUL, VU_SRC_BCY)
VU_FMAC_D(vu_i_mulz,  VU_OP_MUL, VU_SRC_BCZ)
VU_FMAC_D(vu_i_mulw,  VU_OP_MUL, VU_SRC_BCW)
VU_FMAC_A(vu_i_mula,  VU_OP_MUL, VU_SRC_VEC)
VU_FMAC_A(vu_i_mulai, VU_OP_MUL, VU_SRC_I)
VU_FMAC_A(vu_i_mulaq, VU_OP_MUL, VU_SRC_Q)
VU_FMAC_A(vu_i_mulax, VU_OP_MUL, VU_SRC_BCX)
VU_FMAC_A(vu_i_mulay, VU_OP_MUL, VU_SRC_BCY)
VU_FMAC_A(vu_i_mulaz, VU_OP_MUL, VU_SRC_BCZ)
VU_FMAC_A(vu_i_mulaw, VU_OP_MUL, VU_SRC_BCW)

VU_FMAC_D(vu_i_madd,   VU_OP_MADD, VU_SRC_VEC)
VU_FMAC_D(vu_i_maddi,  VU_OP_MADD, VU_SRC_I)
VU_FMAC_D(vu_i_maddq,  VU_OP_MADD, VU_SRC_Q)
VU_FMAC_D(vu_i_maddx,  VU_OP_MADD, VU_SRC_BCX)
VU_FMAC_D(vu_i_maddy,  VU_OP_MADD, VU_SRC_BCY)
VU_FMAC_D(vu_i_maddz,  VU_OP_MADD, VU_SRC_BCZ)
VU_FMAC_D(vu_i_maddw,  VU_OP_MADD, VU_SRC_BCW)
VU_FMAC_A(vu_i_madda,  VU_OP_MADD, VU_SRC_VEC)
VU_FMAC_A(vu_i_maddai, VU_OP_MADD, VU_SRC_I)
VU_FMAC_A(vu_i_maddaq, VU_OP_MADD, VU_SRC_Q)
VU_FMAC_A(vu_i_maddax, VU_OP_MADD, VU_SRC_BCX)
VU_FMAC_A(vu_i_madday, VU_OP_MADD, VU_SRC_BCY)
VU_FMAC_A(vu_i_maddaz, VU_OP_MADD, VU_SRC_BCZ)
VU_FMAC_A(vu_i_maddaw, VU_OP_MADD, VU_SRC_BCW)

VU_FMAC_D(vu_i_msub,   VU_OP_MSUB, VU_SRC_VEC)
VU_FMAC_D(vu_i_msubi,  VU_OP_MSUB, VU_SRC_I)
VU_FMAC_D(vu_i_msubq,  VU_OP_MSUB, VU_SRC_Q)
VU_FMAC_D(vu_i_msubx,  VU_OP_MSUB, VU_SRC_BCX)
VU_FMAC_D(vu_i_msuby,  VU_OP_MSUB, VU_SRC_BCY)
VU_FMAC_D(vu_i_msubz,  VU_OP_MSUB, VU_SRC_BCZ)
VU_FMAC_D(vu_i_msubw,  VU_OP_MSUB, VU_SRC_BCW)
VU_FMAC_A(vu_i_msuba,  VU_OP_MSUB, VU_SRC_VEC)
VU_FMAC_A(vu_i_msubai, VU_OP_MSUB, VU_SRC_I)
VU_FMAC_A(vu_i_msubaq, VU_OP_MSUB, VU_SRC_Q)
VU_FMAC_A(vu_i_msubax, VU_OP_MSUB, VU_SRC_BCX)
VU_FMAC_A(vu_i_msubay, VU_OP_MSUB, VU_SRC_BCY)
VU_FMAC_A(vu_i_msubaz, VU_OP_MSUB, VU_SRC_BCZ)
VU_FMAC_A(vu_i_msubaw, VU_OP_MSUB, VU_SRC_BCW)

#ifdef VU_FMAC_SIMD
static inline __m128i vu_sm_key(__m128i x) {
    return _mm_xor_si128(x, _mm_and_si128(_mm_srai_epi32(x, 31), _mm_set1_epi32(0x7fffffff)));
}

template <uint32_t di, bool IS_MAX, vu_src_kind TK>
static inline void vu_minmax(struct vu_state* vu, int d, int s, int t) {
    if (!d) return;
    __m128i a = _mm_loadu_si128((const __m128i*)&vu->vf[s]);
    __m128i b;

    if constexpr (TK == VU_SRC_VEC) b = _mm_loadu_si128((const __m128i*)&vu->vf[t]);
    else if constexpr (TK == VU_SRC_I) b = _mm_set1_epi32(vu->i.s32);
    else if constexpr (TK == VU_SRC_BCX) b = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&vu->vf[t]), _MM_SHUFFLE(0, 0, 0, 0));
    else if constexpr (TK == VU_SRC_BCY) b = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&vu->vf[t]), _MM_SHUFFLE(1, 1, 1, 1));
    else if constexpr (TK == VU_SRC_BCZ) b = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&vu->vf[t]), _MM_SHUFFLE(2, 2, 2, 2));
    else b = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&vu->vf[t]), _MM_SHUFFLE(3, 3, 3, 3));

    __m128i agtb = _mm_cmpgt_epi32(vu_sm_key(a), vu_sm_key(b));
    __m128i res = IS_MAX ? vu_sel(agtb, a, b) : vu_sel(agtb, b, a);

    vu_write_masked<di>(&vu->vf[d], res);
}
#else
template <uint32_t di, bool IS_MAX, vu_src_kind TK>
static inline void vu_minmax(struct vu_state* vu, int d, int s, int t) {
    if (!d) return;

    int32_t bc = 0;

    if constexpr (TK == VU_SRC_I) bc = vu->i.s32;
    else if constexpr (TK == VU_SRC_BCX) bc = vu->vf[t].s32[0];
    else if constexpr (TK == VU_SRC_BCY) bc = vu->vf[t].s32[1];
    else if constexpr (TK == VU_SRC_BCZ) bc = vu->vf[t].s32[2];
    else if constexpr (TK == VU_SRC_BCW) bc = vu->vf[t].s32[3];

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            int32_t b;
            if constexpr (TK == VU_SRC_VEC) b = vu->vf[t].s32[i];
            else b = bc;
            vu->vf[d].u32[i] = IS_MAX ? vu_max(vu->vf[s].s32[i], b) : vu_min(vu->vf[s].s32[i], b);
        }
    });
}
#endif

#define VU_MINMAX(name, IS_MAX, TK) \
    template <uint32_t di> \
    void name(struct vu_state* vu, const struct vu_instruction* ins) { \
        vu_minmax<di, IS_MAX, TK>(vu, VU_UD_D, VU_UD_S, VU_UD_T); \
    }

VU_MINMAX(vu_i_max,   true,  VU_SRC_VEC)
VU_MINMAX(vu_i_maxi,  true,  VU_SRC_I)
VU_MINMAX(vu_i_maxx,  true,  VU_SRC_BCX)
VU_MINMAX(vu_i_maxy,  true,  VU_SRC_BCY)
VU_MINMAX(vu_i_maxz,  true,  VU_SRC_BCZ)
VU_MINMAX(vu_i_maxw,  true,  VU_SRC_BCW)
VU_MINMAX(vu_i_mini,  false, VU_SRC_VEC)
VU_MINMAX(vu_i_minii, false, VU_SRC_I)
VU_MINMAX(vu_i_minix, false, VU_SRC_BCX)
VU_MINMAX(vu_i_miniy, false, VU_SRC_BCY)
VU_MINMAX(vu_i_miniz, false, VU_SRC_BCZ)
VU_MINMAX(vu_i_miniw, false, VU_SRC_BCW)

#ifdef VU_FMAC_SIMD
void vu_i_opmula(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    __m128 sv = vu_load_cvtf(&vu->vf[s]);
    __m128 tv = vu_load_cvtf(&vu->vf[t]);
    __m128 syzx = _mm_shuffle_ps(sv, sv, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tzxy = _mm_shuffle_ps(tv, tv, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 prod = _mm_mul_ps(syzx, tzxy);

    __m128i pre = vu_cvtf4_i(_mm_castps_si128(prod));
    __m128i clamped = vu_flags4<VU_D_X | VU_D_Y | VU_D_Z>(vu, pre);

    vu_write_masked<VU_D_X | VU_D_Y | VU_D_Z>(&vu->acc, clamped);

    vu_update_status(vu);
}

void vu_i_opmsub(struct vu_state* vu, const struct vu_instruction* ins) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    __m128 sv = vu_load_cvtf(&vu->vf[s]);
    __m128 tv = vu_load_cvtf(&vu->vf[t]);
    __m128 accv = _mm_loadu_ps((const float*)&vu->acc);
    __m128 syzx = _mm_shuffle_ps(sv, sv, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tzxy = _mm_shuffle_ps(tv, tv, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 prod = _mm_mul_ps(syzx, tzxy);
    __m128 r = _mm_sub_ps(accv, prod);

    __m128i clamped = vu_flags4<VU_D_X | VU_D_Y | VU_D_Z>(vu, _mm_castps_si128(r));

    if (d) vu_write_masked<VU_D_X | VU_D_Y | VU_D_Z>(&vu->vf[d], clamped);

    vu_update_status(vu);
}
#else
void vu_i_opmula(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    vu->acc.x = vu_vf_y(vu, s) * vu_vf_z(vu, t);
    vu->acc.y = vu_vf_z(vu, s) * vu_vf_x(vu, t);
    vu->acc.z = vu_vf_x(vu, s) * vu_vf_y(vu, t);

    vu->acc.x = vu_cvtf(vu->acc.u32[0]);
    vu->acc.y = vu_cvtf(vu->acc.u32[1]);
    vu->acc.z = vu_cvtf(vu->acc.u32[2]);

    vu->acc.x = vu_update_flags(vu, vu->acc.x, 0);
    vu->acc.y = vu_update_flags(vu, vu->acc.y, 1);
    vu->acc.z = vu_update_flags(vu, vu->acc.z, 2);

    vu_clear_flags(vu, 3);
    vu_update_status(vu);
}

void vu_i_opmsub(struct vu_state* vu, const struct vu_instruction* ins) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    struct vu_reg128 tmp;

    tmp.f[0] = vu->acc.x - vu_vf_y(vu, s) * vu_vf_z(vu, t);
    tmp.f[1] = vu->acc.y - vu_vf_z(vu, s) * vu_vf_x(vu, t);
    tmp.f[2] = vu->acc.z - vu_vf_x(vu, s) * vu_vf_y(vu, t);

    vu_set_vf_x(vu, d, vu_update_flags(vu, tmp.f[0], 0));
    vu_set_vf_y(vu, d, vu_update_flags(vu, tmp.f[1], 1));
    vu_set_vf_z(vu, d, vu_update_flags(vu, tmp.f[2], 2));

    vu_clear_flags(vu, 3);
    vu_update_status(vu);
}
#endif
void vu_i_nop(struct vu_state* vu, const struct vu_instruction* ins) {
    // No operation
}

#ifdef VU_FMAC_SIMD
template <uint32_t di>
static inline void vu_ftoi(struct vu_state* vu, int t, int s, float scale) {
    __m128 f = _mm_mul_ps(vu_load_cvtf(&vu->vf[s]), _mm_set1_ps(scale));
    __m128i iv = _mm_cvttps_epi32(f);
    __m128i povf = _mm_castps_si128(_mm_cmpge_ps(f, _mm_set1_ps(2147483648.0f)));

    iv = vu_sel(povf, _mm_set1_epi32(0x7fffffff), iv);

    if (t) vu_write_masked<di>(&vu->vf[t], iv);
}

template <uint32_t di>
static inline void vu_itof(struct vu_state* vu, int t, int s, float scale) {
    __m128 f = _mm_mul_ps(_mm_cvtepi32_ps(_mm_loadu_si128((const __m128i*)&vu->vf[s])), _mm_set1_ps(scale));

    if (t) vu_write_masked<di>(&vu->vf[t], _mm_castps_si128(f));
}
#else
template <uint32_t di>
static inline void vu_ftoi(struct vu_state* vu, int t, int s, float scale) {
    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu_set_vfu(vu, t, i, vu_cvti(vu_vf_i(vu, s, i) * scale));
        }
    });
}

template <uint32_t di>
static inline void vu_itof(struct vu_state* vu, int t, int s, float scale) {
    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu_set_vf(vu, t, i, (float)((float)(vu->vf[s].s32[i]) * scale));
        }
    });
}
#endif

#define VU_CONV(name, fn, scale) \
    template <uint32_t di> \
    void name(struct vu_state* vu, const struct vu_instruction* ins) { \
        fn<di>(vu, VU_UD_T, VU_UD_S, scale); \
    }

VU_CONV(vu_i_ftoi0,  vu_ftoi, 1.0f)
VU_CONV(vu_i_ftoi4,  vu_ftoi, (1.0f / 0.0625f))
VU_CONV(vu_i_ftoi12, vu_ftoi, (1.0f / 0.000244140625f))
VU_CONV(vu_i_ftoi15, vu_ftoi, (1.0f / 0.000030517578125f))
VU_CONV(vu_i_itof0,  vu_itof, 1.0f)
VU_CONV(vu_i_itof4,  vu_itof, 0.0625f)
VU_CONV(vu_i_itof12, vu_itof, 0.000244140625f)
VU_CONV(vu_i_itof15, vu_itof, 0.000030517578125f)

void vu_i_clip(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_UD_T;
    int s = VU_UD_S;

    vu->clip <<= 6;

    double w = fabs(ps2_to_double(vu->vf[t].u32[3]));
    double x = ps2_to_double(vu->vf[s].u32[0]);
    double y = ps2_to_double(vu->vf[s].u32[1]);
    double z = ps2_to_double(vu->vf[s].u32[2]);

    vu->clip |= (x > +w);
    vu->clip |= (x < -w) << 1;
    vu->clip |= (y > +w) << 2;
    vu->clip |= (y < -w) << 3;
    vu->clip |= (z > +w) << 4;
    vu->clip |= (z < -w) << 5;
    vu->clip &= 0xFFFFFF;
}

// Lower pipeline
void vu_i_b(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_branch(vu, vu->tpc + VU_LD_IMM11);
}
void vu_i_bal(struct vu_state* vu, const struct vu_instruction* ins) {
    // Instruction next to the delay slot
    VU_IT = vu_delay_slot_link(vu);

    vu_branch(vu, vu->tpc + VU_LD_IMM11);
}
void vu_i_div(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_LD_T;
    int s = VU_LD_S;
    int tf = VU_LD_TF;
    int sf = VU_LD_SF;

    uint32_t nb = vu->vf[s].u32[sf];
    uint32_t db = vu->vf[t].u32[tf];
    uint32_t sign = (nb ^ db) & 0x80000000;

    vu->status &= ~0x30u;

    double num = ps2_to_double(nb);
    double den = ps2_to_double(db);

    uint32_t result;

    if (den == 0.0) {
        if (num == 0.0) {
            vu->status |= VU_STATUS_I;
        } else {
            vu->status |= VU_STATUS_D;
        }

        result = sign | 0x7fffffff;
    } else {
        result = ps2_pack_double(num / den);
    }

    vu_set_q_u32(vu, result, 7);
}
void vu_i_eatan(struct vu_state* vu, const struct vu_instruction* ins) {
    float x = vu_vf_i(vu, VU_LD_S, VU_LD_SF);

    if (x == -1.0f) {
        vu->p.u32 = 0xFF7FFFFF;
    } else {
        x = (x - 1.0f) / (x + 1.0f);

        vu->p.f = vu_atan(x);
    }
}
void vu_i_eatanxy(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    float x = vu_vf_x(vu, s);
    float y = vu_vf_y(vu, s);

    if (y + x == 0.0f) {
        vu->p.u32 = 0x7F7FFFFF | (vu->vf[s].u32[1] & 0x80000000);
    } else {
        x = (y - 1.0f) / (y + x);

        vu->p.f = vu_atan(x);
    }
}
void vu_i_eatanxz(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    float x = vu_vf_x(vu, s);
    float z = vu_vf_z(vu, s);

    //P = atan(z/x)
    if (z + x == 0.0f) {
        vu->p.u32 = 0x7F7FFFFF | (vu->vf[s].u32[2] & 0x80000000);
    } else {
        x = (z - x) / (z + x);

        vu->p.f = vu_atan(x);
    }
}
void vu_i_eexp(struct vu_state* vu, const struct vu_instruction* ins) {
    const static float coeffs[] = {
        0.249998688697815f, 0.031257584691048f,
        0.002591371303424f, 0.000171562001924f,
        0.000005430199963f, 0.000000690600018f
    };

    int s = VU_LD_S;
    int sf = VU_LD_SF;

    if (vu->vf[s].u32[sf] & 0x80000000) {
        vu->p.f = vu_vf_i(vu, s, sf);

        return;
    }

    float value = 1;
    float x = vu_vf_i(vu, s, sf);

    for (int exp = 1; exp <= 6; exp++)
        value += coeffs[exp - 1] * pow(x, exp);

    vu->p.f = 1.0 / value;
}
void vu_i_eleng(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;

    float x2 = vu_vf_x(vu, s) * vu_vf_x(vu, s);
    float y2 = vu_vf_y(vu, s) * vu_vf_y(vu, s);
    float z2 = vu_vf_z(vu, s) * vu_vf_z(vu, s);

    vu->p.f = sqrtf(x2 + y2 + z2);
}
void vu_i_ercpr(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->p.f = 1.0f / vu_vf_i(vu, VU_LD_S, VU_LD_SF);
}
void vu_i_erleng(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;

    float x2 = vu_vf_x(vu, s) * vu_vf_x(vu, s);
    float y2 = vu_vf_y(vu, s) * vu_vf_y(vu, s);
    float z2 = vu_vf_z(vu, s) * vu_vf_z(vu, s);

    vu->p.f = 1.0f / sqrtf(x2 + y2 + z2);
}
void vu_i_ersadd(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;

    float x2 = vu_vf_x(vu, s) * vu_vf_x(vu, s);
    float y2 = vu_vf_y(vu, s) * vu_vf_y(vu, s);
    float z2 = vu_vf_z(vu, s) * vu_vf_z(vu, s);

    vu->p.f = 1.0f / (x2 + y2 + z2);
}
void vu_i_ersqrt(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->p.f = 1.0f / sqrtf(vu_vf_i(vu, VU_LD_S, VU_LD_SF));
}
void vu_i_esadd(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;

    float x2 = vu_vf_x(vu, s) * vu_vf_x(vu, s);
    float y2 = vu_vf_y(vu, s) * vu_vf_y(vu, s);
    float z2 = vu_vf_z(vu, s) * vu_vf_z(vu, s);

    vu->p.f = x2 + y2 + z2;
}
void vu_i_esin(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->p.f = sinf(vu_vf_i(vu, VU_LD_S, VU_LD_SF));
}
void vu_i_esqrt(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->p.f = sqrtf(vu_vf_i(vu, VU_LD_S, VU_LD_SF));
}
void vu_i_esum(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;

    vu->p.f = vu_vf_x(vu, s) + vu_vf_y(vu, s) + vu_vf_z(vu, s) + vu_vf_w(vu, s);
}

#define VU_CLIP_DELAY 3
#define VU_VF_LATENCY 4
#define VU_CLIP_FLAGS(vu) ((vu)->clip_pipeline[VU_CLIP_DELAY])

void vu_i_fcand(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->vi[1] = ((VU_CLIP_FLAGS(vu) & 0xffffff) & VU_LD_IMM24) != 0;
}
void vu_i_fceq(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->vi[1] = (VU_CLIP_FLAGS(vu) & 0xffffff) == VU_LD_IMM24;
}
void vu_i_fcget(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_LD_T;

    if (!t) return;

    vu->vi[VU_LD_T] = VU_CLIP_FLAGS(vu) & 0xfff;
}
void vu_i_fcor(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->vi[1] = ((VU_CLIP_FLAGS(vu) & 0xffffff) | VU_LD_IMM24) == 0xffffff;
}
void vu_i_fcset(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->clip = VU_LD_IMM24;
}
void vu_i_fmand(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_set_vi(vu, VU_LD_T, vu->mac_pipeline[3] & VU_IS);
}
void vu_i_fmeq(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_set_vi(vu, VU_LD_T, (VU_IS & 0xffff) == (vu->mac_pipeline[3] & 0xffff));
}
void vu_i_fmor(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_set_vi(vu, VU_LD_T, (VU_IS & 0xffff) | (vu->mac_pipeline[3] & 0xffff));
}
void vu_i_fsand(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_set_vi(vu, VU_LD_T, vu->status & VU_LD_IMM12);
}
void vu_i_fseq(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_set_vi(vu, VU_LD_T, (vu->status & 0xfff) == VU_LD_IMM12);
}
void vu_i_fsor(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_set_vi(vu, VU_LD_T, (vu->status & 0xfff) | VU_LD_IMM12);
}
void vu_i_fsset(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->status &= 0x3f;
    vu->status |= VU_LD_IMM12 & 0xfc0;
}
void vu_i_iadd(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_write_branch_pipeline(vu, VU_LD_D);

    vu_set_vi(vu, VU_LD_D, VU_IS + VU_IT);
}
void vu_i_iaddi(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_write_branch_pipeline(vu, VU_LD_T);

    vu_set_vi(vu, VU_LD_T, VU_IS + VU_LD_IMM5);
}
void vu_i_iaddiu(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_write_branch_pipeline(vu, VU_LD_T);

    vu_set_vi(vu, VU_LD_T, VU_IS + VU_LD_IMM15);
}
void vu_i_iand(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_write_branch_pipeline(vu, VU_LD_T);

    vu_set_vi(vu, VU_LD_D, VU_IS & VU_IT);
}
void vu_i_ibeq(struct vu_state* vu, const struct vu_instruction* ins) {
    uint16_t t = vu_get_branch_register(vu, VU_LD_T);
    uint16_t s = vu_get_branch_register(vu, VU_LD_S);

    if (t == s) vu_branch(vu, vu->tpc + VU_LD_IMM11);
}
void vu_i_ibgez(struct vu_state* vu, const struct vu_instruction* ins) {
    int16_t s = vu_get_branch_register(vu, VU_LD_S);

    if (s >= 0) vu_branch(vu, vu->tpc + VU_LD_IMM11);
}
void vu_i_ibgtz(struct vu_state* vu, const struct vu_instruction* ins) {
    int16_t s = vu_get_branch_register(vu, VU_LD_S);

    if (s > 0) vu_branch(vu, vu->tpc + VU_LD_IMM11);
}
void vu_i_iblez(struct vu_state* vu, const struct vu_instruction* ins) {
    int16_t s = vu_get_branch_register(vu, VU_LD_S);

    if (s <= 0) vu_branch(vu, vu->tpc + VU_LD_IMM11);
}
void vu_i_ibltz(struct vu_state* vu, const struct vu_instruction* ins) {
    int16_t s = vu_get_branch_register(vu, VU_LD_S);

    if (s < 0) vu_branch(vu, vu->tpc + VU_LD_IMM11);
}
void vu_i_ibne(struct vu_state* vu, const struct vu_instruction* ins) {
    uint16_t t = vu_get_branch_register(vu, VU_LD_T);
    uint16_t s = vu_get_branch_register(vu, VU_LD_S);

    // printf("ibne vi%02u (%04x), vi%02u (%04x), 0x%08x\n", VU_LD_T, t, VU_LD_S, s, vu->tpc + VU_LD_IMM11);

    if (t != s) vu_branch(vu, vu->tpc + VU_LD_IMM11);
}
template <uint32_t di>
void vu_i_ilw(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_LD_T;

    if (!t) return;

    uint32_t addr = VU_IS + VU_LD_IMM11;
    uint128_t data = vu_mem_read(vu, addr);

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vi[t] = data.u32[i];
        }
    });
}
template <uint32_t di>
void vu_i_ilwr(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    if (!t) return;

    uint32_t addr = vu->vi[s];
    uint128_t data = vu_mem_read(vu, addr);

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vi[t] = data.u32[i];
        }
    });
}
void vu_i_ior(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_write_branch_pipeline(vu, VU_LD_D);

    vu_set_vi(vu, VU_LD_D, VU_IS | VU_IT);
}
void vu_i_isub(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_write_branch_pipeline(vu, VU_LD_D);

    vu_set_vi(vu, VU_LD_D, VU_IS - VU_IT);
}
void vu_i_isubiu(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_write_branch_pipeline(vu, VU_LD_T);

    vu_set_vi(vu, VU_LD_T, VU_IS - VU_LD_IMM15);
}
template <uint32_t di>
void vu_i_isw(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s] + VU_LD_IMM11;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu_mem_write(vu, addr, vu->vi[t], i);
        }
    });
}
template <uint32_t di>
void vu_i_iswr(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s];

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu_mem_write(vu, addr, vu->vi[t], i);
        }
    });
}
void vu_i_jalr(struct vu_state* vu, const struct vu_instruction* ins) {
    uint16_t s = VU_IS;

    VU_IT = vu_delay_slot_link(vu);

    vu_branch(vu, s);
}
void vu_i_jr(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_branch(vu, VU_IS);
}
template <uint32_t di>
void vu_i_lq(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s] + VU_LD_IMM11;
    uint128_t data = vu_mem_read(vu, addr);

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vf[t].u32[i] = data.u32[i];
        }
    });
}
template <uint32_t di>
void vu_i_lqd(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    vu_write_branch_pipeline(vu, s);

    vu_set_vi(vu, s, vu->vi[s] - 1);

    uint32_t addr = vu->vi[s];
    uint128_t data = vu_mem_read(vu, addr);

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vf[t].u32[i] = data.u32[i];
        }
    });
}
template <uint32_t di>
void vu_i_lqi(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    vu_write_branch_pipeline(vu, s);

    if (t) {
        uint32_t addr = vu->vi[s];
        uint128_t data = vu_mem_read(vu, addr);

        template_seq<4>([&](auto i) {
            if constexpr (di & (VU_D_X >> i)) {
                vu->vf[t].u32[i] = data.u32[i];
            }
        });
    }

    vu_set_vi(vu, s, vu->vi[s] + 1);
}
template <uint32_t di>
void vu_i_mfir(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_LD_T;

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vf[t].u32[i] = (int32_t)(int16_t)VU_IS;
        }
    });
}
template <uint32_t di>
void vu_i_mfp(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_LD_T;

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vf[t].u32[i] = vu->p.u32;
        }
    });
}
template <uint32_t di>
void vu_i_move(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vf[t].u32[i] = vu->vf[s].u32[i];
        }
    });
}
template <uint32_t di>
void vu_i_mr32(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_LD_T;

    if (!t) return;

    int s = VU_LD_S;

    uint32_t x = vu->vf[s].u32[0];

    // template_seq<4>([&](auto i) {
    //     if constexpr (di & (VU_D_X >> i)) {
    //         vu->vf[t].u32[i] = vu->vf[s].u32[(i + 1) & 3];
    //     }
    // });

    if constexpr (di & VU_D_X) {
        vu->vf[t].u32[0] = vu->vf[s].u32[1];
    }
    if constexpr (di & VU_D_Y) {
        vu->vf[t].u32[1] = vu->vf[s].u32[2];
    }
    if constexpr (di & VU_D_Z) {
        vu->vf[t].u32[2] = vu->vf[s].u32[3];
    }
    if constexpr (di & VU_D_W) {
        vu->vf[t].u32[3] = x;
    }
}
void vu_i_mtir(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_write_branch_pipeline(vu, VU_LD_T);

    vu_set_vi(vu, VU_LD_T, vu->vf[VU_LD_S].u32[VU_LD_SF] & 0xffff);
}
template <uint32_t di>
void vu_i_rget(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_LD_T;

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vf[t].u32[i] = vu->r.u32;
        }
    });
}
void vu_i_rinit(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;

    vu->r.u32 = 0x3f800000;

    if (!s) return;

    vu->r.u32 |= vu->vf[s].u32[VU_LD_SF] & 0x007fffff;
}
template <uint32_t di>
void vu_i_rnext(struct vu_state* vu, const struct vu_instruction* ins) {
    int t = VU_LD_T;

    if (!t) return;

    int x = (vu->r.u32 >> 4) & 1;
    int y = (vu->r.u32 >> 22) & 1;

    vu->r.u32 <<= 1;
    vu->r.u32 ^= x ^ y;
    vu->r.u32 = (vu->r.u32 & 0x7FFFFF) | 0x3F800000;

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu->vf[t].u32[i] = vu->r.u32;
        }
    });
}
void vu_i_rsqrt(struct vu_state* vu, const struct vu_instruction* ins) {
    uint32_t nb = vu->vf[VU_LD_S].u32[VU_LD_SF];
    uint32_t db = vu->vf[VU_LD_T].u32[VU_LD_TF];

    vu->status &= ~0x30u;

    if (db & 0x80000000) {
        vu->status |= VU_STATUS_I;
    }

    double num = ps2_to_double(nb);
    double den = ps2_to_double(db & 0x7fffffff);

    uint32_t result;

    if (den == 0.0) {
        if (num != 0.0) {
            vu->status |= VU_STATUS_D;
        }

        result = (nb & 0x80000000) | 0x7fffffff;
    } else {
        result = ps2_pack_double(num / sqrt(den));
    }

    vu_set_q_u32(vu, result, 13);
}
void vu_i_rxor(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->r.u32 = 0x3F800000 | ((vu->r.u32 ^ vu->vf[VU_LD_S].u32[VU_LD_SF]) & 0x007FFFFF);
}
template <uint32_t di>
void vu_i_sq(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[t] + VU_LD_IMM11;

    // printf("vu: sq addr=%08x vf%02d=%08x %08x %08x %08x\n", addr, s, vu->vf[s].u32[3], vu->vf[s].u32[2], vu->vf[s].u32[1], vu->vf[s].u32[0]);

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu_mem_write(vu, addr, vu->vf[s].u32[i], i);
        }
    });
}
template <uint32_t di>
void vu_i_sqd(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    vu_write_branch_pipeline(vu, t);

    vu_set_vi(vu, t, vu->vi[t] - 1);

    uint32_t addr = vu->vi[t];

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu_mem_write(vu, addr, vu->vf[s].u32[i], i);
        }
    });
}
template <uint32_t di>
void vu_i_sqi(struct vu_state* vu, const struct vu_instruction* ins) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    vu_write_branch_pipeline(vu, t);

    uint32_t addr = vu->vi[t];

    template_seq<4>([&](auto i) {
        if constexpr (di & (VU_D_X >> i)) {
            vu_mem_write(vu, addr, vu->vf[s].u32[i], i);
        }
    });

    vu_set_vi(vu, t, vu->vi[t] + 1);
}
void vu_i_sqrt(struct vu_state* vu, const struct vu_instruction* ins) {
    uint32_t tb = vu->vf[VU_LD_T].u32[VU_LD_TF];

    vu->status &= ~0x30u;

    if (tb & 0x80000000) {
        vu->status |= VU_STATUS_I;
    }

    double v = ps2_to_double(tb & 0x7fffffff);

    vu_set_q_u32(vu, ps2_pack_double(sqrt(v)), 7);
}
void vu_i_waitp(struct vu_state* vu, const struct vu_instruction* ins) {
    // No operation
}
void vu_i_waitq(struct vu_state* vu, const struct vu_instruction* ins) {
    vu->q_delay = 0;
}

void vu_i_xgkick(struct vu_state* vu, const struct vu_instruction* ins) {
    // vu_xgkick(vu);
    // vu->xgkick_pending = 3;
    // vu->xgkick_addr = VU_IS;

    // return;

    uint16_t addr = VU_IS;

    int eop = 1;

    do {
        uint128_t tag = vu_mem_read(vu, addr++);

        if ((tag.u64[0] | tag.u64[1]) == 0)
            break;

        // addr &= 0x3ff;

        // if (addr == 0) break;

        // printf("tag: addr=%08x %08x %08x %08x %08x\n", addr - 1, tag.u32[3], tag.u32[2], tag.u32[1], tag.u32[0]);

        eop = (tag.u64[0] & 0x8000) != 0;

        int nloop = tag.u64[0] & 0x7fff;
        int flg = (tag.u64[0] >> 58) & 3;
        int nregs = (tag.u64[0] >> 60) & 0xf;

        if (!nloop)
            continue;

        if (!nregs)
            nregs = 16;

        int qwc = 0;

        switch (flg) {
            case 0: {
                qwc = nregs * nloop;
            } break;
            case 1: {
                qwc = (nregs * nloop + 1) / 2; // Round up for odd cases
            } break;
            case 2:
            case 3: {
                qwc = nloop;
            } break;
        }

        if (qwc >= 0x400) {
            return;

            fprintf(stderr, "vu: Weird xgkick tag nloop=%d nregs=%d eop=%d flg=%d qwc=%d\n",
                nloop,
                nregs,
                eop,
                flg,
                qwc
            ); 

            exit(1);
        }

        ps2_gif_fifo_write(vu->gif, tag, GIF_PATH1);

        for (int i = 0; i < qwc; i++) {
            // printf("vu: %08x: %08x %08x %08x %08x\n",
            //     addr,
            //     vu->vu_mem[addr].u32[3],
            //     vu->vu_mem[addr].u32[2],
            //     vu->vu_mem[addr].u32[1],
            //     vu->vu_mem[addr].u32[0]
            // );

            ps2_gif_fifo_write(vu->gif, vu_mem_read(vu, addr++), GIF_PATH1);

            addr &= 0x3ff;

            // if (addr == 0) {
            //     eop = 1;
            //     break;
            // }
        }
    } while (!eop);
}
void vu_i_xitop(struct vu_state* vu, const struct vu_instruction* ins) {
    vu_set_vi(vu, VU_LD_T, vu->vif->itop);
}
void vu_i_xtop(struct vu_state* vu, const struct vu_instruction* ins) {
    if (vu->id == 0) {
        printf("vu: xtop used in VU0\n");

        // exit(1);
    }

    vu_set_vi(vu, VU_LD_T, vu->vif->top);
}

uint64_t ps2_vu_read8(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint8_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint8_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
uint64_t ps2_vu_read16(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint16_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint16_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
uint64_t ps2_vu_read32(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint32_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint32_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
uint64_t ps2_vu_read64(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint64_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint64_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
uint128_t ps2_vu_read128(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint128_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint128_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
void ps2_vu_write8(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        vu_invalidate_range(vu, addr, 1);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint8_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint8_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void ps2_vu_write16(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        vu_invalidate_range(vu, addr, 2);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint16_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint16_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void ps2_vu_write32(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        vu_invalidate_range(vu, addr, 4);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint32_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint32_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void ps2_vu_write64(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        vu_invalidate_range(vu, addr, 8);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint64_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint64_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void ps2_vu_write128(struct vu_state* vu, uint32_t addr, uint128_t data) {
    if (addr <= 0x3FFF) {
        vu_invalidate_range(vu, addr, 16);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint128_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint128_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}

#define VU_FLD_X 1
#define VU_FLD_Y 2
#define VU_FLD_Z 4
#define VU_FLD_W 8

#define VU_DEC_UD_S_SRC_T_BROADCAST(bc, f) \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = (opcode >> 21) & 0xf; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = bc; \
    vu->upper.func = f;

#define VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(bc, f) \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = bc; \
    vu->upper.func = f;

#define VU_DEC_UD_D_DST_S_SRC_T_SRC(f) \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = vu->upper.dst.field; \
    vu->upper.func = f;

#define VU_DEC_UD_D_DST_S_SRC(f) \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.func = f;

#define VU_DEC_UD_D_DST_S_SRC_Q_SRC(f) \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[1].reg = VU_REG_Q; \
    vu->upper.func = f;

#define VU_DEC_UD_T_DST_S_SRC(f) \
    vu->upper.dst.reg = vu->upper.ud_t; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.func = f;

#define VU_DEC_UD_S_SRC_T_SRC(f) \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = (opcode >> 21) & 0xf; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = vu->upper.src[0].field; \
    vu->upper.func = f;

#define VU_DEC_UD_S_SRC(f) \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = (opcode >> 21) & 0xf; \
    vu->upper.func = f;

#define VU_DEC_UD_S_SRC_Q_SRC(f) \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = (opcode >> 21) & 0xf; \
    vu->upper.src[1].reg = VU_REG_Q; \
    vu->upper.func = f;

#define VU_DEC_OPMULA() \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = VU_FLD_X | VU_FLD_Y | VU_FLD_Z; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = vu->upper.src[0].field; \
    vu->upper.func = vu_i_opmula;

#define VU_DEC_OPMSUB() \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = VU_FLD_X | VU_FLD_Y | VU_FLD_Z; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = vu->upper.dst.field; \
    vu->upper.func = vu_i_opmsub;

#define VU_DEC_CLIP() \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = VU_FLD_X | VU_FLD_Y | VU_FLD_Z; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = VU_FLD_W; \
    vu->upper.func = vu_i_clip;

#define VU_DEC_LD_NONE(f) \
    vu->lower.func = f;

#define VU_DEC_UD_NONE(f) \
    vu->upper.func = f;

#define VU_DEC_LD_T_DST_S_VISRC(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define VU_DEC_LD_T_DST_S_VISRC_S_VIDST(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.vi_dst = vu->lower.ld_s; \
    vu->lower.vi_src[0] = vu->lower.vi_dst; \
    vu->lower.func = f;

#define VU_DEC_LD_S_SRC_T_VISRC_T_VIDST(f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = (opcode >> 21) & 0xf; \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.vi_src[0] = vu->lower.vi_dst; \
    vu->lower.func = f;

#define VU_DEC_LD_S_SF_SRC_T_TF_SRC(f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.ld_sf; \
    vu->lower.src[1].reg = vu->lower.ld_t; \
    vu->lower.src[1].field = vu->lower.ld_tf; \
    vu->lower.func = f;

#define VU_DEC_LD_Q_DST_S_SF_SRC_T_TF_SRC(f) \
    vu->lower.dst.reg = VU_REG_Q; \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.ld_sf; \
    vu->lower.src[1].reg = vu->lower.ld_t; \
    vu->lower.src[1].field = vu->lower.ld_tf; \
    vu->lower.func = f;

#define VU_DEC_LD_T_VIDST_S_SF_SRC(f) \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.ld_sf; \
    vu->lower.func = f;

#define VU_DEC_LD_T_DST_S_VISRC(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define VU_DEC_LD_T_TF_SRC(f) \
    vu->lower.src[0].reg = vu->lower.ld_t; \
    vu->lower.src[0].field = vu->lower.ld_tf; \
    vu->lower.func = f;

#define VU_DEC_LD_Q_DST_T_TF_SRC(f) \
    vu->lower.dst.reg = VU_REG_Q; \
    vu->lower.src[0].reg = vu->lower.ld_t; \
    vu->lower.src[0].field = vu->lower.ld_tf; \
    vu->lower.func = f;

#define VU_DEC_LD_S_SRC_T_VISRC(f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = (opcode >> 21) & 0xf; \
    vu->lower.vi_src[0] = vu->lower.ld_t; \
    vu->lower.func = f;

#define VU_DEC_LD_T_VIDST_S_VISRC(f) \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define VU_DEC_LD_S_VISRC_T_VISRC(f) \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.vi_src[1] = vu->lower.ld_t; \
    vu->lower.func = f;

#define VU_DEC_LD_T_VIDST_S_VISRC(f) \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define VU_DEC_LD_T_VISRC_S_VISRC(f) \
    vu->lower.vi_src[0] = vu->lower.ld_t; \
    vu->lower.vi_src[1] = vu->lower.ld_s; \
    vu->lower.func = f;

#define VU_DEC_LD_VIDST(v, f) \
    vu->lower.vi_dst = v; \
    vu->lower.func = f;

#define VU_DEC_LD_T_VIDST(f) \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.func = f;

#define VU_DEC_LD_S_VISRC(f) \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define VU_DEC_LD_S_FLD_SRC(fld, f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = fld; \
    vu->lower.func = f;

#define VU_DEC_LD_D_VIDST_S_VISRC_T_VISRC(f) \
    vu->lower.vi_dst = vu->lower.ld_d; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.vi_src[1] = vu->lower.ld_t; \
    vu->lower.func = f;

#define VU_DEC_LD_T_DST_S_SRC(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.dst.field; \
    vu->lower.func = f;

#define VU_DEC_LD_T_DST(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.func = f;

#define VU_DEC_LD_S_SF_SRC(f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.ld_sf; \
    vu->lower.func = f;

#define VU_DEC_MR32(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = (vu->lower.dst.field >> 1) | ((vu->lower.dst.field & 1) << 3); \
    vu->lower.func = f;

#define GET_TEMPLATE_FN(i) \
    [&](uint32_t opcode) { \
        switch ((opcode >> 21) & 0xf) { \
            case 0: return &i<0>; \
            case 1: return &i<VU_D_W>; \
            case 2: return &i<VU_D_Z>; \
            case 3: return &i<VU_D_Z | VU_D_W>; \
            case 4: return &i<VU_D_Y>; \
            case 5: return &i<VU_D_Y | VU_D_W>; \
            case 6: return &i<VU_D_Y | VU_D_Z>; \
            case 7: return &i<VU_D_Y | VU_D_Z | VU_D_W>; \
            case 8: return &i<VU_D_X>; \
            case 9: return &i<VU_D_X | VU_D_W>; \
            case 10: return &i<VU_D_X | VU_D_Z>; \
            case 11: return &i<VU_D_X | VU_D_Z | VU_D_W>; \
            case 12: return &i<VU_D_X | VU_D_Y>; \
            case 13: return &i<VU_D_X | VU_D_Y | VU_D_W>; \
            case 14: return &i<VU_D_X | VU_D_Y | VU_D_Z>; \
            case 15: return &i<VU_D_X | VU_D_Y | VU_D_Z | VU_D_W>; \
            default: __builtin_unreachable(); \
        } \
    __builtin_unreachable(); }(opcode)

void vu_decode_upper(struct vu_state* vu, uint32_t opcode) {
    vu->upper.opcode = opcode;
    vu->upper.ud_d = (opcode >> 6) & 0x1f;
    vu->upper.ud_s = (opcode >> 11) & 0x1f;
    vu->upper.ud_t = (opcode >> 16) & 0x1f;

    for (int i = 0; i < 4; i++)
        vu->upper.ud_di[i] = opcode & (1 << (24 - i));

    vu->upper.func = NULL;
    vu->upper.dst.reg = 0;
    vu->upper.dst.field = 0;
    vu->upper.src[0].reg = 0;
    vu->upper.src[0].field = 0;
    vu->upper.src[1].reg = 0;
    vu->upper.src[1].field = 0;

    // Decode 000007FF style instruction
    if ((opcode & 0x3c) == 0x3c) {
        // 0EEEE 1111 EE
        // -0EE EE11 11EE
        // --------------
        // bit 10 is always 0
        // bits 2-5 are always 1
        // --------------
        // bits 0-1 and bits 6-9 (6 bits) are enough to decode
        // all of the following
        switch (((opcode & 0x3c0) >> 4) | (opcode & 3)) {
            case 0x00: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_addax)); return;
            case 0x01: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_adday)); return;
            case 0x02: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_addaz)); return;
            case 0x03: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_addaw)); return;
            case 0x04: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_subax)); return;
            case 0x05: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_subay)); return;
            case 0x06: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_subaz)); return;
            case 0x07: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_subaw)); return;
            case 0x08: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_maddax)); return;
            case 0x09: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_madday)); return;
            case 0x0A: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_maddaz)); return;
            case 0x0B: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_maddaw)); return;
            case 0x0C: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_msubax)); return;
            case 0x0D: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_msubay)); return;
            case 0x0E: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_msubaz)); return;
            case 0x0F: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_msubaw)); return;
            case 0x10: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_itof0)); return;
            case 0x11: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_itof4)); return;
            case 0x12: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_itof12)); return;
            case 0x13: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_itof15)); return;
            case 0x14: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_ftoi0)); return;
            case 0x15: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_ftoi4)); return;
            case 0x16: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_ftoi12)); return;
            case 0x17: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_ftoi15)); return;
            case 0x18: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_mulax)); return;
            case 0x19: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_mulay)); return;
            case 0x1A: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_mulaz)); return;
            case 0x1B: VU_DEC_UD_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_mulaw)); return;
            case 0x1C: VU_DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_mulaq)); return;
            case 0x1D: VU_DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_abs)); return;
            case 0x1E: VU_DEC_UD_S_SRC(GET_TEMPLATE_FN(vu_i_mulai)); return;
            case 0x1F: VU_DEC_CLIP(); return;
            case 0x20: VU_DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_addaq)); return;
            case 0x21: VU_DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_maddaq)); return;
            case 0x22: VU_DEC_UD_S_SRC(GET_TEMPLATE_FN(vu_i_addai)); return;
            case 0x23: VU_DEC_UD_S_SRC(GET_TEMPLATE_FN(vu_i_maddai)); return;
            case 0x24: VU_DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_subaq)); return;
            case 0x25: VU_DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_msubaq)); return;
            case 0x26: VU_DEC_UD_S_SRC(GET_TEMPLATE_FN(vu_i_subai)); return;
            case 0x27: VU_DEC_UD_S_SRC(GET_TEMPLATE_FN(vu_i_msubai)); return;
            case 0x28: VU_DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_adda)); return;
            case 0x29: VU_DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_madda)); return;
            case 0x2A: VU_DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_mula)); return;
            case 0x2C: VU_DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_suba)); return;
            case 0x2D: VU_DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_msuba)); return;
            case 0x2E: VU_DEC_OPMULA(); return;
            case 0x2F: VU_DEC_UD_NONE(vu_i_nop); return;
        }
    } else {
        // Decode 0000003F style instruction
        switch (opcode & 0x3f) {
            case 0x00: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_addx)); return;
            case 0x01: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_addy)); return;
            case 0x02: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_addz)); return;
            case 0x03: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_addw)); return;
            case 0x04: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_subx)); return;
            case 0x05: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_suby)); return;
            case 0x06: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_subz)); return;
            case 0x07: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_subw)); return;
            case 0x08: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_maddx)); return;
            case 0x09: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_maddy)); return;
            case 0x0A: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_maddz)); return;
            case 0x0B: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_maddw)); return;
            case 0x0C: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_msubx)); return;
            case 0x0D: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_msuby)); return;
            case 0x0E: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_msubz)); return;
            case 0x0F: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_msubw)); return;
            case 0x10: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_maxx)); return;
            case 0x11: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_maxy)); return;
            case 0x12: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_maxz)); return;
            case 0x13: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_maxw)); return;
            case 0x14: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_minix)); return;
            case 0x15: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_miniy)); return;
            case 0x16: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_miniz)); return;
            case 0x17: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_miniw)); return;
            case 0x18: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_X, GET_TEMPLATE_FN(vu_i_mulx)); return;
            case 0x19: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Y, GET_TEMPLATE_FN(vu_i_muly)); return;
            case 0x1A: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_Z, GET_TEMPLATE_FN(vu_i_mulz)); return;
            case 0x1B: VU_DEC_UD_D_DST_S_SRC_T_BROADCAST(VU_FLD_W, GET_TEMPLATE_FN(vu_i_mulw)); return;
            case 0x1C: VU_DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_mulq)); return;
            case 0x1D: VU_DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(vu_i_maxi)); return;
            case 0x1E: VU_DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(vu_i_muli)); return;
            case 0x1F: VU_DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(vu_i_minii)); return;
            case 0x20: VU_DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_addq)); return;
            case 0x21: VU_DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_maddq)); return;
            case 0x22: VU_DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(vu_i_addi)); return;
            case 0x23: VU_DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(vu_i_maddi)); return;
            case 0x24: VU_DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_subq)); return;
            case 0x25: VU_DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(vu_i_msubq)); return;
            case 0x26: VU_DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(vu_i_subi)); return;
            case 0x27: VU_DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(vu_i_msubi)); return;
            case 0x28: VU_DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_add)); return;
            case 0x29: VU_DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_madd)); return;
            case 0x2A: VU_DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_mul)); return;
            case 0x2B: VU_DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_max)); return;
            case 0x2C: VU_DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_sub)); return;
            case 0x2D: VU_DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_msub)); return;
            case 0x2E: VU_DEC_OPMSUB(); return;
            case 0x2F: VU_DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(vu_i_mini)); return;
        }
    }
}

void vu_decode_lower(struct vu_state* vu, uint32_t opcode) {
    vu->lower.opcode = opcode;
    vu->lower.ld_d = (opcode >> 6) & 0x1f;
    vu->lower.ld_s = (opcode >> 11) & 0x1f;
    vu->lower.ld_t = (opcode >> 16) & 0x1f;
    vu->lower.ld_sf = (opcode >> 21) & 3;
    vu->lower.ld_tf = (opcode >> 23) & 3;
    vu->lower.ld_imm5 = ((int32_t)(((opcode >> 6) & 0x1f) << 27)) >> 27;
    vu->lower.ld_imm11 = ((int32_t)((opcode & 0x7ff) << 21)) >> 21;
    vu->lower.ld_imm12 = (((opcode >> 21) & 1) << 11) | (opcode & 0x7ff);
    vu->lower.ld_imm15 = (opcode & 0x7ff) | ((opcode & 0x1e00000) >> 10);
    vu->lower.ld_imm24 = opcode & 0xffffff;

    for (int i = 0; i < 4; i++)
        vu->lower.ld_di[i] = opcode & (1 << (24 - i));

    vu->lower.func = NULL;
    vu->lower.dst.reg = 0;
    vu->lower.dst.field = 0;
    vu->lower.src[0].reg = 0;
    vu->lower.src[0].field = 0;
    vu->lower.src[1].reg = 0;
    vu->lower.src[1].field = 0;
    vu->lower.vi_src[0] = 0;
    vu->lower.vi_src[1] = 0;
    vu->lower.vi_dst = 0;
    vu->lower.branch = 0;

    switch ((opcode & 0xFE000000) >> 25) {
        case 0x00: VU_DEC_LD_T_DST_S_VISRC(GET_TEMPLATE_FN(vu_i_lq)); return;
        case 0x01: VU_DEC_LD_S_SRC_T_VISRC(GET_TEMPLATE_FN(vu_i_sq)); return;
        case 0x04: VU_DEC_LD_T_VIDST_S_VISRC(GET_TEMPLATE_FN(vu_i_ilw)); return;
        case 0x05: VU_DEC_LD_S_VISRC_T_VISRC(GET_TEMPLATE_FN(vu_i_isw)); return;
        case 0x08: VU_DEC_LD_T_VIDST_S_VISRC(vu_i_iaddiu); return;
        case 0x09: VU_DEC_LD_T_VIDST_S_VISRC(vu_i_isubiu); return;

        // Note: The flag check instructions clobber the destination register
        //       "immediately", this means we don't actually need to generate
        //       a dependency.
        case 0x10: VU_DEC_LD_NONE(vu_i_fceq); return;
        case 0x11: VU_DEC_LD_NONE(vu_i_fcset); return;
        case 0x12: VU_DEC_LD_NONE(vu_i_fcand); return;
        case 0x13: VU_DEC_LD_NONE(vu_i_fcor); return;
        case 0x14: VU_DEC_LD_NONE(vu_i_fseq); return;
        case 0x15: VU_DEC_LD_NONE(vu_i_fsset); return;
        case 0x16: VU_DEC_LD_NONE(vu_i_fsand); return;
        case 0x17: VU_DEC_LD_NONE(vu_i_fsor); return;
        case 0x18: VU_DEC_LD_S_VISRC(vu_i_fmeq); return;
        case 0x1A: VU_DEC_LD_S_VISRC(vu_i_fmand); return;
        case 0x1B: VU_DEC_LD_S_VISRC(vu_i_fmor); return;
        case 0x1C: VU_DEC_LD_NONE(vu_i_fcget); return;
        case 0x20: vu->lower.branch = 1; VU_DEC_LD_NONE(vu_i_b); return;
        case 0x21: vu->lower.branch = 1; VU_DEC_LD_T_VIDST(vu_i_bal); return;
        case 0x24: vu->lower.branch = 1; VU_DEC_LD_S_VISRC(vu_i_jr); return;
        case 0x25: vu->lower.branch = 1; VU_DEC_LD_T_VIDST_S_VISRC(vu_i_jalr); return;
        case 0x28: vu->lower.branch = 1; VU_DEC_LD_S_VISRC_T_VISRC(vu_i_ibeq); return;
        case 0x29: vu->lower.branch = 1; VU_DEC_LD_S_VISRC_T_VISRC(vu_i_ibne); return;
        case 0x2C: vu->lower.branch = 1; VU_DEC_LD_S_VISRC(vu_i_ibltz); return;
        case 0x2D: vu->lower.branch = 1; VU_DEC_LD_S_VISRC(vu_i_ibgtz); return;
        case 0x2E: vu->lower.branch = 1; VU_DEC_LD_S_VISRC(vu_i_iblez); return;
        case 0x2F: vu->lower.branch = 1; VU_DEC_LD_S_VISRC(vu_i_ibgez); return;
        case 0x40: {
            if ((opcode & 0x3C) == 0x3C) {
                switch (((opcode & 0x7C0) >> 4) | (opcode & 3)) {
                    case 0x30: VU_DEC_LD_T_DST_S_SRC(GET_TEMPLATE_FN(vu_i_move)); return;
                    case 0x31: VU_DEC_MR32(GET_TEMPLATE_FN(vu_i_mr32)); return;
                    case 0x34: VU_DEC_LD_T_DST_S_VISRC_S_VIDST(GET_TEMPLATE_FN(vu_i_lqi)); return;
                    case 0x35: VU_DEC_LD_S_SRC_T_VISRC_T_VIDST(GET_TEMPLATE_FN(vu_i_sqi)); return;
                    case 0x36: VU_DEC_LD_T_DST_S_VISRC_S_VIDST(GET_TEMPLATE_FN(vu_i_lqd)); return;
                    case 0x37: VU_DEC_LD_S_SRC_T_VISRC_T_VIDST(GET_TEMPLATE_FN(vu_i_sqd)); return;
                    case 0x38: VU_DEC_LD_Q_DST_S_SF_SRC_T_TF_SRC(vu_i_div); return;
                    case 0x39: VU_DEC_LD_Q_DST_T_TF_SRC(vu_i_sqrt); return;
                    case 0x3A: VU_DEC_LD_Q_DST_S_SF_SRC_T_TF_SRC(vu_i_rsqrt); return;
                    case 0x3B: VU_DEC_LD_NONE(vu_i_waitq); return;
                    case 0x3C: VU_DEC_LD_T_VIDST_S_SF_SRC(vu_i_mtir); return;
                    case 0x3D: VU_DEC_LD_T_DST_S_VISRC(GET_TEMPLATE_FN(vu_i_mfir)); return;
                    case 0x3E: VU_DEC_LD_T_VIDST_S_VISRC(GET_TEMPLATE_FN(vu_i_ilwr)); return;
                    case 0x3F: VU_DEC_LD_T_VISRC_S_VISRC(GET_TEMPLATE_FN(vu_i_iswr)); return;
                    case 0x40: VU_DEC_LD_T_DST(GET_TEMPLATE_FN(vu_i_rnext)); return;
                    case 0x41: VU_DEC_LD_T_DST(GET_TEMPLATE_FN(vu_i_rget)); return;
                    case 0x42: VU_DEC_LD_S_SF_SRC(vu_i_rinit); return;
                    case 0x43: VU_DEC_LD_S_SF_SRC(vu_i_rxor); return;
                    case 0x64: VU_DEC_LD_T_DST(GET_TEMPLATE_FN(vu_i_mfp)); return;
                    case 0x68: VU_DEC_LD_T_VIDST(vu_i_xtop); return;
                    case 0x69: VU_DEC_LD_T_VIDST(vu_i_xitop); return;
                    case 0x6C: VU_DEC_LD_S_VISRC(vu_i_xgkick); return;
                    case 0x70: VU_DEC_LD_S_FLD_SRC(VU_FLD_X | VU_FLD_Y | VU_FLD_Z, vu_i_esadd); return;
                    case 0x71: VU_DEC_LD_S_FLD_SRC(VU_FLD_X | VU_FLD_Y | VU_FLD_Z, vu_i_ersadd); return;
                    case 0x72: VU_DEC_LD_S_FLD_SRC(VU_FLD_X | VU_FLD_Y | VU_FLD_Z, vu_i_eleng); return;
                    case 0x73: VU_DEC_LD_S_FLD_SRC(VU_FLD_X | VU_FLD_Y | VU_FLD_Z, vu_i_erleng); return;
                    case 0x74: VU_DEC_LD_S_FLD_SRC(VU_FLD_X | VU_FLD_Y, vu_i_eatanxy); return;
                    case 0x75: VU_DEC_LD_S_FLD_SRC(VU_FLD_X | VU_FLD_Z, vu_i_eatanxz); return;
                    case 0x76: VU_DEC_LD_S_FLD_SRC(VU_FLD_X | VU_FLD_Y | VU_FLD_Z | VU_FLD_W, vu_i_esum); return;
                    case 0x78: VU_DEC_LD_S_SF_SRC(vu_i_esqrt); return;
                    case 0x79: VU_DEC_LD_S_SF_SRC(vu_i_ersqrt); return;
                    case 0x7A: VU_DEC_LD_S_SF_SRC(vu_i_ercpr); return;
                    case 0x7B: VU_DEC_LD_NONE(vu_i_waitp); return;
                    case 0x7C: VU_DEC_LD_S_SF_SRC(vu_i_esin); return;
                    case 0x7D: VU_DEC_LD_S_SF_SRC(vu_i_eatan); return;
                    case 0x7E: VU_DEC_LD_S_SF_SRC(vu_i_eexp); return;
                }
            } else {
                switch (opcode & 0x3F) {
                    case 0x30: VU_DEC_LD_D_VIDST_S_VISRC_T_VISRC(vu_i_iadd); return;
                    case 0x31: VU_DEC_LD_D_VIDST_S_VISRC_T_VISRC(vu_i_isub); return;
                    case 0x32: VU_DEC_LD_T_VIDST_S_VISRC(vu_i_iaddi); return;
                    case 0x34: VU_DEC_LD_D_VIDST_S_VISRC_T_VISRC(vu_i_iand); return;
                    case 0x35: VU_DEC_LD_D_VIDST_S_VISRC_T_VISRC(vu_i_ior); return;
                }
            }
        } break;
    }
}

static inline void vu_advance_fmac_pipeline(struct vu_state* vu) {
    vu->upper_pipeline[3] = vu->upper_pipeline[2];
    vu->upper_pipeline[2] = vu->upper_pipeline[1];
    vu->upper_pipeline[1] = vu->upper_pipeline[0];
    vu->upper_pipeline[0].dst.reg = vu->upper.dst.reg;
    vu->upper_pipeline[0].dst.field = vu->upper.dst.field;
    vu->lower_pipeline[3] = vu->lower_pipeline[2];
    vu->lower_pipeline[2] = vu->lower_pipeline[1];
    vu->lower_pipeline[1] = vu->lower_pipeline[0];
    vu->lower_pipeline[0].dst.reg = vu->lower.dst.reg;
    vu->lower_pipeline[0].dst.field = vu->lower.dst.field;
}

static inline int vu_get_fmac_stall_cycles(struct vu_state* vu) {
    for (int i = 0; i < 0x4; i++) {
        for (int j = 0; j < 2; j++) {
            if (vu->upper.src[j].reg == vu->lower_pipeline[i].dst.reg) {
                if (vu->upper.src[j].field & vu->lower_pipeline[i].dst.field) {
                    return 4 - i;
                }
            }
        }
    }

    return 0;
}

vu_block* vu_find_block(struct vu_state* vu, uint32_t tpc) {
    if (tpc == vu->last_block_lookup_tpc) {
        return vu->last_block_ptr;
    }

    vu_block* block = &vu->block_cache[tpc & vu->micro_mem_size];

    if (!block->cycles) {
        return nullptr;
    }

    // Update cache for next lookup
    vu->last_block_lookup_tpc = tpc;
    vu->last_block_ptr = block;

    return block;
}

static int c = 0;

static inline int vu_vf_write_mask(const struct vu_instruction& ins);

vu_block* vu_cache_block(struct vu_state* vu, uint32_t tpc, int max_cycles) {
    vu_block* block = &vu->block_cache[tpc & vu->micro_mem_size];

    vu->block_cache_size++;

    block->tpc = tpc;
    block->cycles = 0;
    block->entries.clear();

    // printf("vu: caching block at %04x\n", tpc);

    bool delay_slot = false;

    for (int i = 0; i < max_cycles; i++) {
        vu_block_entry entry = { 0 };

        uint64_t liw = vu->micro_mem[tpc++ & 0x7ff];
        uint32_t upper = liw >> 32;
        uint32_t lower = liw & 0xffffffff;

        // LOI consumes the raw lower word when the i-bit is set.
        entry.lower.opcode = lower;

        entry.i_bit = (upper & 0x80000000) != 0;
        entry.e_bit = (upper & 0x40000000) != 0;
        entry.m_bit = (upper & 0x20000000) != 0;

        vu_decode_upper(vu, upper & 0x7ffffff);

        entry.upper = vu->upper;

        if (!entry.i_bit) {
            vu_decode_lower(vu, lower);

            entry.lower = vu->lower;
            entry.branch = vu->lower.branch;
            entry.hazard0 = vu->upper.dst.reg == vu->lower.src[0].reg;
            entry.hazard1 = vu->upper.dst.reg == vu->lower.src[1].reg;
            entry.hazard2 = vu->upper.dst.reg == vu->lower.dst.reg;
            entry.hazard3 = vu->lower.dst.reg == VU_REG_Q;

            entry.lw_reg = (entry.lower.dst.reg && entry.lower.dst.reg < 32) ? entry.lower.dst.reg : 0;
            entry.lw_mask = entry.lw_reg ? vu_vf_write_mask(entry.lower) : 0;
            entry.is_mtir = (entry.lower.func == vu_i_mtir) && (entry.lower.src[0].reg != 0);
            entry.mtir_reg = entry.lower.src[0].reg;
            entry.mtir_comp = entry.lower.src[0].field;
            entry.is_waitq = entry.lower.func == vu_i_waitq;
            entry.lower_is_nop = entry.lower.func == vu_i_nop;
        }

        entry.uw_reg = (entry.upper.dst.reg && entry.upper.dst.reg < 32) ? entry.upper.dst.reg : 0;
        entry.uw_mask = entry.uw_reg ? vu_vf_write_mask(entry.upper) : 0;

        // If this entry is a branch or has the E bit set, we end the block here
        if (entry.branch || entry.e_bit) {
            i = max_cycles - 2;
        }

        // if (entry.branch) {
        //     // if (delay_slot) {
        //     //     printf("vu%d: warning: branch in delay slot at %04x\n", vu->id, (tpc - 1) & 0x7ff);
        //     // }

        //     delay_slot = true;
        // } else {
        //     delay_slot = false;
        // }

        block->cycles++;

        block->entries.push_back(entry);
    }

    // vu_dis_state ds;

    // ds.addr = block->tpc;
    // ds.print_address = 0;
    // ds.print_opcode = 0;

    // for (const vu_block_entry& entry : block->entries) {
    //     char upper_buf[512];
    //     char lower_buf[512];

    //     printf(" %s %04x: %08x %08x %-40s %s\n",
    //         entry.i_bit ? "I" : entry.e_bit ? "E" : " ",
    //         ds.addr++,
    //         entry.upper.opcode,
    //         entry.lower.opcode,
    //         vu_disassemble_upper(upper_buf, entry.upper.opcode, &ds),
    //         vu_disassemble_lower(lower_buf, entry.lower.opcode, &ds)
    //     );
    // }

    // Prime fast lookup with a pointer known to be valid after this insertion.
    vu->last_block_lookup_tpc = block->tpc;
    vu->last_block_ptr = block;

    return block;
}

static inline void vu_shift_flag_pipeline(struct vu_state* vu) {
    vu->mac_pipeline[3] = vu->mac_pipeline[2];
    vu->mac_pipeline[2] = vu->mac_pipeline[1];
    vu->mac_pipeline[1] = vu->mac_pipeline[0];
    vu->mac_pipeline[0] = vu->mac;

    vu->clip_pipeline[3] = vu->clip_pipeline[2];
    vu->clip_pipeline[2] = vu->clip_pipeline[1];
    vu->clip_pipeline[1] = vu->clip_pipeline[0];
    vu->clip_pipeline[0] = vu->clip;
}

static inline int vu_vf_write_mask(const struct vu_instruction& ins) {
    if (ins.func == vu_i_opmsub)
        return 0x7;

    int mask = 0;

    for (int c = 0; c < 4; c++) {
        if (ins.dst.field & (0x8 >> c)) {
            mask |= 1 << c;
        }
    }

    return mask;
}

static inline int vu_interlock_stall(struct vu_state* vu, const vu_block_entry& entry) {
    if (!entry.is_mtir)
        return 0;

    uint64_t ready = vu->vf_ready[entry.mtir_reg][entry.mtir_comp];

    if (ready <= vu->vu_cycle) {
        return 0;
    }

    return (int)(ready - vu->vu_cycle);
}

static inline void vu_record_vf_writes(struct vu_state* vu, const vu_block_entry& entry) {
    if (entry.uw_reg) {
        for (int c = 0; c < 4; c++) {
            if (entry.uw_mask & (1 << c)) {
                vu->vf_ready[entry.uw_reg][c] = vu->vu_cycle + VU_VF_LATENCY;
            }
        }
    }

    if (entry.lw_reg) {
        for (int c = 0; c < 4; c++){
            if (entry.lw_mask & (1 << c)) {
                vu->vf_ready[entry.lw_reg][c] = vu->vu_cycle + VU_VF_LATENCY;
            }
        }
    }
}

void vu_execute_block_entry(struct vu_state* vu, const vu_block_entry& entry) {
    for (int stall = vu_interlock_stall(vu, entry); stall--; ) {
        if (vu->q_delay)
            vu->q_delay--;

        vu_shift_flag_pipeline(vu);

        vu->vu_cycle++;
    }

    if (vu->q_delay)
        vu->q_delay--;

    vu_update_status(vu);

    if (entry.i_bit) {
        entry.upper.func(vu, &entry.upper);

        // LOI
        vu->i.u32 = entry.lower.opcode;
    } else {
        if (entry.hazard3 && vu->q_delay) vu->q_delay = 0;

        if (!entry.upper.dst.reg) {
            entry.upper.func(vu, &entry.upper);

            if (!entry.lower_is_nop) entry.lower.func(vu, &entry.lower);
        } else if (entry.hazard0 || entry.hazard1 || entry.is_waitq) {
            // Upper instruction writes to a register that the lower
            // instruction reads from. In this case the lower instruction
            // gets the previous value of the register, executing the lower
            // instruction first does the trick.

            // We also execute WAITQ first, since it will stall the pipeline
            // if the upper instruction reads Q

            entry.lower.func(vu, &entry.lower);
            entry.upper.func(vu, &entry.upper);
        } else if (entry.hazard2) {
            // Upper and lower instructions write to the same register.
            // In this case the upper instruction takes priority, so we
            // restore the value of the register after executing the lower
            // instruction.

            entry.upper.func(vu, &entry.upper);

            struct vu_reg128 tmp = vu->vf[entry.upper.dst.reg];

            entry.lower.func(vu, &entry.lower);

            vu->vf[entry.upper.dst.reg] = tmp;
        } else {
            entry.upper.func(vu, &entry.upper);

            if (!entry.lower_is_nop) entry.lower.func(vu, &entry.lower);
        }
    }

    vu_shift_flag_pipeline(vu);

    if (vu->vi_backup_cycles) {
        vu->vi_backup_cycles--;

        if (!vu->vi_backup_cycles) {
            vu->vi_backup_reg = 0;
            vu->vi_backup_value = 0;
        }
    }

    vu_record_vf_writes(vu, entry);

    vu->vu_cycle++;
}

bool vu_execute_block(struct vu_state* vu, vu_block* block) {
    // printf("vu: Input TPC %04x\n", vu->tpc);

    for (const vu_block_entry& entry : block->entries) {
        // Immediately end execution. TPC still points at this instruction, so the
        // interlock resume re-runs it with any pending branch intact.
        if (entry.m_bit) {
            vu->waiting_for_interlock = true;

            return true;
        }

        if (entry.e_bit)
            vu->e_bit = 2;

        vu->tpc = (vu->tpc + 1) & 0x7ff;

        vu_execute_block_entry(vu, entry);

        bool taken = false;

        if (vu->branch_delay && !--vu->branch_delay) {
            vu->tpc = vu->branch_pc;

            // A branch fired in this branch's delay slot. Its own delay slot is the
            // instruction we just jumped to, so arm it for exactly one more instruction.
            if (vu->delay_branch) {
                vu->branch_delay = 1;
                vu->branch_pc = vu->delay_branch_pc;
                vu->delay_branch = false;
            }

            taken = true;
        }

        if (vu->e_bit && !--vu->e_bit)
            return true;

        // The rest of this block is no longer the instruction stream we're executing.
        if (taken)
            break;
    }

    // printf("vu: Output TPC %04x\n", vu->tpc);

    return false;
}

static void vu_run(struct vu_state* vu) {
    while (true) {
        vu_block* block = vu_find_block(vu, vu->tpc);

        if (!block) {
            vu->cache_misses++;

            block = vu_cache_block(vu, vu->tpc, 64);
        } else {
            vu->cache_hits++;
        }

        if (vu_execute_block(vu, block))
            break;
    }
}

void vu_execute_program(struct vu_state* vu, uint32_t addr) {
    if (vu->disable)
        return;

    // Clear VU0 interlock
    vu->waiting_for_interlock = false;

    vu->tpc = addr & 0x7ff;
    vu->i_bit = 0;
    vu->e_bit = 0;
    vu->branch_delay = 0;
    vu->delay_branch = false;

    vu->vu_cycle = 0;

    for (int i = 0; i < 32; i++)
        for (int c = 0; c < 4; c++)
            vu->vf_ready[i][c] = 0;

    vu_run(vu);
}

void ps2_vu_write_vi(struct vu_state* vu, int index, uint32_t value) {
    switch (index) {
        case 0: return;
        case 1: case 2: case 3:
        case 4: case 5: case 6: case 7:
        case 8: case 9: case 10: case 11:
        case 12: case 13: case 14: case 15: {
            vu->vi[index] = value & 0xffff;
        } break;

        case 16: {
            vu->status &= ~0xfc0;
            vu->status |= value & 0xfc0;
        } break;

        case 17: return; // MAC flag register, read-only
        case 18: {
            vu->clip = value & 0xffffff;
        } break;
        
        case 19: return; // VU revision register? read-only

        case 20: {
            vu->r.u32 = value & 0x7fffff;
        } break;
        case 21: {
            vu->i.u32 = value;
        } break;
        case 22: {
            vu->q.u32 = value;
        } break;
        case 23: return;
        case 24: {
            vu->cr[8] = value & 0xc0c;
        } break;
        case 25: return;
        case 26: return; // VU TPC register, read-only
        case 27: {
            vu->cmsar0 = value & 0xffff;
        } break;
        case 28: {
            // To-do: Handle FBRST
            vu->fbrst = value & 0xc0c;

            if (value & 2) {
                // Reset VU0
                ps2_vu_reset(vu);
            }

            if (value & 0x200) {
                // Reset VU1
                ps2_vu_reset(vu->vu1);
            }
        } break;
        case 29: return; // VU VPU-STAT register, read-only
        case 30: return; // VU reserved register, read-only
        case 31: {
            vu->cmsar1 = value & 0xffff;

            vu_execute_program(vu->vu1, vu->cmsar1 >> 3);
        } break;
    }
}

uint32_t ps2_vu_read_vi(struct vu_state* vu, int index) {
    switch (index) {
        case 0: case 1: case 2: case 3:
        case 4: case 5: case 6: case 7:
        case 8: case 9: case 10: case 11:
        case 12: case 13: case 14: case 15: {
            return vu->vi[index];
        } break;

        case 19: { // VU revision register
            return 0x2e30;
        } break;

        default: {
            return vu->cr[index - 16];
        } break;
    }
}

void ps2_vu_reset(struct vu_state* vu) {
    vu->disable = false;

    for (int i = 0; i < 16; i++)
        vu->vi[i] = 0;

    for (int i = 0; i < 32; i++) {
        vu->vf[i].u32[0] = 0;
        vu->vf[i].u32[1] = 0;
        vu->vf[i].u32[2] = 0;
        vu->vf[i].u32[3] = 0;
    }

    vu->r.u32 = 0x3f800000;
    vu->i.u32 = 0;
    vu->q.u32 = 0;
    vu->clip = 0;
    vu->status = 0;
    vu->fbrst = 0;
    vu->cmsar0 = 0;
    vu->cmsar1 = 0;
    vu->mac = 0;
    vu->mac_pipeline[0] = 0;
    vu->mac_pipeline[1] = 0;
    vu->mac_pipeline[2] = 0;
    vu->mac_pipeline[3] = 0;
    vu->clip_pipeline[0] = 0;
    vu->clip_pipeline[1] = 0;
    vu->clip_pipeline[2] = 0;
    vu->clip_pipeline[3] = 0;
    vu->tpc = 0;
    vu->i_bit = 0;
    vu->e_bit = 0;
    vu->m_bit = 0;
    vu->d_bit = 0;
    vu->t_bit = 0;
    vu->q_delay = 0;
    vu->branch_delay = 0;
    vu->delay_branch = false;
    vu->prev_q.u32 = 0;

    vu->vu_cycle = 0;

    for (int i = 0; i < 32; i++) {
        for (int c = 0; c < 4; c++) {
            vu->vf_ready[i][c] = 0;
        }
    }

    vu->last_block_lookup_tpc = ~0u;
    vu->last_block_ptr = nullptr;

    vu->block_cache_size = 0;
    vu->block_cache.clear();
    vu->block_cache.resize(vu->micro_mem_size+1);

    vu->vf[0].w = 1.0;
}

void ps2_vu_decode_upper(struct vu_state* vu, uint32_t opcode) {
    vu_decode_upper(vu, opcode);
}

void ps2_vu_decode_lower(struct vu_state* vu, uint32_t opcode) {
    vu_decode_lower(vu, opcode);
}

void vu_execute_program_tpc(struct vu_state* vu) {
    if (vu->disable)
        return;

    // Clear VU0 interlock
    vu->waiting_for_interlock = false;

    vu_run(vu);
}

uint128_t* vu_get_vu_mem_ptr(struct vu_state* vu, uint32_t addr) {
    return &vu->vu_mem[addr & vu->vu_mem_size];
}

uint64_t* vu_get_micro_mem_ptr(struct vu_state* vu, uint32_t addr) {
    return &vu->micro_mem[addr & vu->micro_mem_size];
}

uint32_t vu_get_tpc(struct vu_state* vu) {
    return vu->tpc;
}

void ps2_vu_execute_lower(struct vu_state* vu, uint32_t opcode) {
    vu_decode_lower(vu, opcode);

    vu->lower.func(vu, &vu->lower);
}

void ps2_vu_execute_upper(struct vu_state* vu, uint32_t opcode) {
    vu_decode_upper(vu, opcode);

    vu->upper.func(vu, &vu->upper);
}

void vu_clear_block_cache(struct vu_state* vu) {
    vu->block_cache_size = 0;
    vu->block_cache.clear();
    vu->block_cache.resize(vu->micro_mem_size+1);

    vu->last_block_lookup_tpc = ~0u;
    vu->last_block_ptr = nullptr;
}

void vu_invalidate_range(struct vu_state* vu, uint32_t addr, uint32_t size) {
    if (!size || vu->block_cache_size == 0) {
        return;
    }

    const uint32_t word_mask = (uint32_t)vu->micro_mem_size;
    const uint32_t word_count = word_mask + 1;
    const uint32_t byte_mask = (word_mask << 3) | 7;
    const uint32_t byte_count = word_count << 3;

    if (size >= byte_count) {
        for (vu_block& block : vu->block_cache) {
            if (!block.cycles) {
                continue;
            }

            block.cycles = 0;
            block.entries.clear();
        }

        vu->block_cache_size = 0;
        vu->last_block_lookup_tpc = ~0u;
        vu->last_block_ptr = nullptr;

        return;
    }

    const uint32_t start_byte = addr & byte_mask;
    const uint32_t start_word = start_byte >> 3;
    const uint32_t offset_in_word = start_byte & 7;
    const uint32_t invalid_word_count = (offset_in_word + size + 7) >> 3;

    int invalidated = 0;

    for (vu_block& block : vu->block_cache) {
        if (!block.cycles) {
            continue;
        }

        bool intersects = false;

        for (int i = 0; i < block.cycles; i++) {
            const uint32_t block_word = (block.tpc + (uint32_t)i) & word_mask;
            const uint32_t rel = (block_word - start_word) & word_mask;

            if (rel < invalid_word_count) {
                intersects = true;
                break;
            }
        }

        if (!intersects) {
            continue;
        }

        block.cycles = 0;
        block.entries.clear();
        invalidated++;
    }

    if (!invalidated) {
        return;
    }

    vu->block_cache_size -= invalidated;

    if (vu->block_cache_size < 0) {
        vu->block_cache_size = 0;
    }

    vu->last_block_lookup_tpc = ~0u;
    vu->last_block_ptr = nullptr;
}

int vu_is_interlocked(struct vu_state* vu) {
    return vu->waiting_for_interlock;
}

// #undef printf