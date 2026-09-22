#include <cmath>
#include <math.h>
#include <fenv.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <utility>

#if !defined(IRIS_VU_NO_SIMD)
#if defined(__SSE2__) || defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
#include <immintrin.h>
#define FMAC_SIMD 1
#endif
#endif

#include <cstring>

#include "vu.hpp"
#include "vu_def.hpp"
#include "vu_jit.hpp"
#include "vu_dis.hpp"
#include <cstdint>
#include "gif.hpp"
#include "vif.hpp"
#include "mtvu.hpp"

#include "profile_tag.hpp"
#include "profile_counters.hpp"

namespace iris::vu {

#define LD_DI(i) (ins->ld_di[i])
#define LD_D (ins->ld_d)
#define LD_S (ins->ld_s)
#define LD_T (ins->ld_t)
#define LD_SF (ins->ld_sf)
#define LD_TF (ins->ld_tf)
#define LD_IMM5 (ins->ld_imm5)
#define LD_IMM11 (ins->ld_imm11)
#define LD_IMM12 (ins->ld_imm12)
#define LD_IMM15 (ins->ld_imm15)
#define LD_IMM24 (ins->ld_imm24)
#define ID vu->vi[LD_D & 0xf]
#define IS vu->vi[LD_S & 0xf]
#define IT vu->vi[LD_T & 0xf]
#define UD_DI(i) (ins->ud_di[i])
#define UD_D (ins->ud_d)
#define UD_S (ins->ud_s)
#define UD_T (ins->ud_t)
#define D_FLD (0x01e00000)
#define D_X (0x01000000)
#define D_Y (0x00800000)
#define D_Z (0x00400000)
#define D_W (0x00200000)

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

Vu* create(logger::Logger* logger, int id) {
    Vu* vu = new Vu();

    vu->logger = logger;
    vu->logger_id = logger::register_source(logger, id ? "vu1" : "vu0");
    vu->id = id;

    vu->engine = VU_ENGINE_INTERP;
    vu->jit_threshold = 1000;
    vu->max_cycles = 1 << 28;

    vu->region_limit = 1;
    vu->region_epoch = 0;

    vu->jit = jit::create();

    if (!id) {
        vu->micro_mem_size = 0x1ff;
        vu->vu_mem_size = 0xff;
    } else {
        vu->micro_mem_size = 0x7ff;
        vu->vu_mem_size = 0x3ff;
    }

    reset(vu);

    return vu;
}

void connect(Vu* vu, gif::Gif* gif, vif::Vif* vif, Vu* vu1) {
    vu->gif = gif;
    vu->vif = vif;
    vu->vu1 = vu1;
}

void destroy(Vu* vu) {
    jit::destroy(vu->jit);

    delete vu;
}

static inline uint32_t max(int32_t a, int32_t b) {
    return (a < 0 && b < 0) ? std::min(a, b) : std::max(a, b);
}

static inline uint32_t min(int32_t a, int32_t b) {
    return (a < 0 && b < 0) ? std::max(a, b) : std::min(a, b);
}

static inline float atan(float t) {
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

static inline void fold_status(Vu* vu, uint32_t mac) {
    vu->status &= ~0x3f;

    vu->status |= (mac & 0x000f) ? 1 : 0;
    vu->status |= (mac & 0x00f0) ? 2 : 0;
    vu->status |= (mac & 0x0f00) ? 4 : 0;
    vu->status |= (mac & 0xf000) ? 8 : 0;

    vu->status |= (vu->status & 0x3f) << 6;
}

static inline void update_status(Vu* vu) {
    fold_status(vu, vu->mac_pipeline[3]);
}

static inline void set_q(Vu* vu, float value, int delay) {
    if (vu->q_delay == 0) {
        vu->prev_q.f = vu->q.f;
    }

    vu->q.f = value;
    vu->q_delay = delay;
}

static inline Reg32 get_q(Vu* vu) {
    if (!vu->q_delay) {
        return vu->q;
    }

    return vu->prev_q;
}

static inline float update_flags(Vu* vu, float value, int index) {
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

static inline void clear_flags(Vu* vu, int index) {
    vu->mac &= ~(0x1111 << (3 - index));
}

static inline float cvtf(uint32_t value) {
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

int32_t cvti(float value) {
    if (value >= 2147483647.0)
        return 2147483647LL;

    if (value <= -2147483648.0)
        return -2147483648LL;

    return (int32_t)value;
}

static inline double ps2_to_double(uint32_t bits) {
    uint32_t e = (bits >> 23) & 0xff;

    if (!e) {
        return (bits & 0x80000000) ? -0.0 : 0.0;
    }

    uint64_t d = ((uint64_t)(bits >> 31) << 63)
               | ((uint64_t)(e + (1023 - 127)) << 52)
               | ((uint64_t)(bits & 0x7fffff) << 29);

    double out;

    memcpy(&out, &d, sizeof(out));

    return out;
}

static inline uint32_t ps2_pack_double(double v) {
    uint64_t d;

    memcpy(&d, &v, sizeof(d));

    uint32_t sign = (uint32_t)(d >> 63) << 31;

    uint64_t a = d & 0x7fffffffffffffffull;

    if (!a) {
        return sign;
    }

    int biased = (int)((a >> 52) - 1023 + 127);

    if (biased > 254) {
        return sign | 0x7f7fffff;
    }

    if (biased < 1) {
        return sign;
    }

    uint32_t mantissa = (uint32_t)(((a & 0xfffffffffffffull) + (1ull << 28)) >> 29);

    if (mantissa > 0x7fffff) {
        mantissa = 0;

        if (++biased > 254) {
            return sign | 0x7f7fffff;
        }
    }

    return sign | ((uint32_t)biased << 23) | mantissa;
}

static inline void set_q_u32(Vu* vu, uint32_t bits, int delay) {
    if (vu->q_delay == 0)
        vu->prev_q = vu->q;

    vu->q.u32 = bits;
    vu->q_delay = delay;
}

#define STATUS_I 0x0410
#define STATUS_D 0x0820

static inline void set_vf(Vu* vu, int r, int f, float v) {
    if (r) vu->vf[r].f[f] = v;
}

static inline void set_vfu(Vu* vu, int r, int f, int32_t v) {
    if (r) vu->vf[r].s32[f] = v;
}

static inline void set_vf_x(Vu* vu, int r, float v) {
    if (r) vu->vf[r].x = v;
}

static inline void set_vf_y(Vu* vu, int r, float v) {
    if (r) vu->vf[r].y = v;
}

static inline void set_vf_z(Vu* vu, int r, float v) {
    if (r) vu->vf[r].z = v;
}

static inline void set_vf_w(Vu* vu, int r, float v) {
    if (r) vu->vf[r].w = v;
}

static inline void set_vi(Vu* vu, int r, uint16_t v) {
    r &= 0xf;

    if (r) vu->vi[r] = v;
}

static inline uint16_t get_vi(Vu* vu, int r) {
    return vu->vi[r & 0xf];
}

static inline float vf_i(Vu* vu, int r, int i) {
    return cvtf(vu->vf[r].u32[i]);
}

static inline float vf_x(Vu* vu, int r) {
    return cvtf(vu->vf[r].u32[0]);
}

static inline float vf_y(Vu* vu, int r) {
    return cvtf(vu->vf[r].u32[1]);
}

static inline float vf_z(Vu* vu, int r) {
    return cvtf(vu->vf[r].u32[2]);
}

static inline float vf_w(Vu* vu, int r) {
    return cvtf(vu->vf[r].u32[3]);
}

static inline float acc_i(Vu* vu, int i) {
    return cvtf(vu->acc.u32[i]);
}

static inline void mem_write(Vu* vu, uint16_t addr, uint32_t data, int i) {
    if (!vu->id) {
        if (addr <= 0x3ff) {
            vu->vu_mem[addr & 0xff].u32[i] = data;
        } else {
            if (vu->mtvu) {
                mtvu::sync(vu->mtvu, mtvu::SYNC_VU1_REGISTERS);
            }

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
                // iris_debug(vu, "oob write");

                // exit(1);
            }
        }
    } else {
        // if (addr == 0x000001d3) *(int*)0 = 0;

        vu->vu_mem[addr & 0x3ff].u32[i] = data;
    }
}

static inline uint128_t mem_read(Vu* vu, uint32_t addr) {
    if (!vu->id) {
        if (addr <= 0x3ff) {
            return vu->vu_mem[addr & 0xff];
        } else {
            if (vu->mtvu) {
                mtvu::sync(vu->mtvu, mtvu::SYNC_VU1_REGISTERS);
            }

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
static inline void branch(Vu* vu, uint32_t target) {
    target &= vu->micro_mem_size;

    if (vu->branch_delay) {
        vu->delay_branch = true;
        vu->delay_branch_pc = target;

        return;
    }

    vu->branch_delay = 2;
    vu->branch_pc = target;
}

static inline uint32_t delay_slot_link(Vu* vu) {
    return ((vu->branch_delay ? vu->branch_pc : vu->tpc) + 1) & vu->micro_mem_size;
}

static inline void write_branch_pipeline(Vu* vu, int dst) {
    dst &= 0xf;

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

    // iris_debug(vu, "branch pipeline: dst={} prev={:04x} rw={}", //     vu->branch_pipeline_curr.reg, vu->branch_pipeline_curr.prev,
    //     vu->branch_pipeline_curr.rw
    //);
}

static inline uint16_t get_branch_register(Vu* vu, int reg) {
    reg &= 0xf;

    if (vu->vi_backup_cycles && (vu->vi_backup_reg == reg)) {
        return vu->vi_backup_value;
    }

    return vu->vi[reg];
}

void xgkick(Vu* vu) {
    uint16_t addr = vu->xgkick_addr;

    int eop = 1;

    do {
        uint128_t tag = mem_read(vu, addr++);

        if ((tag.u64[0] | tag.u64[1]) == 0)
            break;

        // addr &= 0x3ff;

        // if (addr == 0) break;

        // iris_debug(vu, "tag: addr={:08x} {:08x} {:08x} {:08x} {:08x}", addr - 1, tag.u32[3], tag.u32[2], tag.u32[1], tag.u32[0]);

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

            iris_fatal_error(vu, "Weird xgkick tag nloop={} nregs={} eop={} flg={} qwc={}", nloop,
                nregs,
                eop,
                flg,
                qwc);
        }

        gif::fifo_write(vu->gif, tag, gif::PATH1);

        for (int i = 0; i < qwc; i++) {
            // iris_debug(vu, "{:08x}: {:08x} {:08x} {:08x} {:08x}", //     addr,
            //     vu->vu_mem[addr].u32[3],
            //     vu->vu_mem[addr].u32[2],
            //     vu->vu_mem[addr].u32[1],
            //     vu->vu_mem[addr].u32[0]
            //);

            gif::fifo_write(vu->gif, mem_read(vu, addr++), gif::PATH1);

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
void i_abs(Vu* vu, const Instruction* ins) {
    int s = UD_S;
    int t = UD_T;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            set_vf(vu, t, i, fabsf(vf_i(vu, s, i)));
        }
    });
}

enum alu_op { OP_ADD, OP_SUB, OP_MUL, OP_MADD, OP_MSUB };
enum src_kind { SRC_VEC, SRC_BCX, SRC_BCY, SRC_BCZ, SRC_BCW, SRC_I, SRC_Q };

#ifdef FMAC_SIMD

static inline __m128i sel(__m128i mask, __m128i a, __m128i b) {
    return _mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b));
}

static inline __m128i cvtf4_i(__m128i v) {
    const __m128i em = _mm_set1_epi32(0x7f800000);
    const __m128i sm = _mm_set1_epi32((int)0x80000000);
    const __m128i maxn = _mm_set1_epi32(0x7f7fffff);
    const __m128i z = _mm_setzero_si128();

    __m128i exp = _mm_and_si128(v, em);
    __m128i is0 = _mm_cmpeq_epi32(exp, z);
    __m128i is255 = _mm_cmpeq_epi32(exp, em);
    __m128i sign = _mm_and_si128(v, sm);

    v = sel(is0, sign, v);
    v = sel(is255, _mm_or_si128(sign, maxn), v);

    return v;
}

static inline __m128 load_cvtf(const Reg128* r) {
    return _mm_castsi128_ps(cvtf4_i(_mm_loadu_si128((const __m128i*)r)));
}

template <uint32_t di>
static constexpr uint32_t nibblemask() {
    uint32_t nm = 0;
    for (int i = 0; i < 4; i++) if (di & (D_X >> i)) nm |= 0x1111u << (3 - i);
    return nm;
}

template <uint32_t di>
static inline __m128i flags4(Vu* vu, __m128i v) {
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

    __m128i r = sel(is0, sign, v);

    r = sel(is255, _mm_or_si128(sign, maxn), r);

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

    vu->mac = (vu->mac & ~0xFFFFu) | (newmac & nibblemask<di>());

    return r;
}

template <uint32_t di>
static inline void write_masked(void* dst, __m128i val) {
    constexpr uint32_t l0 = (di & D_X) ? 0xFFFFFFFFu : 0;
    constexpr uint32_t l1 = (di & D_Y) ? 0xFFFFFFFFu : 0;
    constexpr uint32_t l2 = (di & D_Z) ? 0xFFFFFFFFu : 0;
    constexpr uint32_t l3 = (di & D_W) ? 0xFFFFFFFFu : 0;

    if constexpr (l0 && l1 && l2 && l3) {
        _mm_storeu_si128((__m128i*)dst, val);
    } else if constexpr (l0 || l1 || l2 || l3) {
        const __m128i keep = _mm_set_epi32((int)l3, (int)l2, (int)l1, (int)l0);

        __m128i cur = _mm_loadu_si128((__m128i*)dst);

        _mm_storeu_si128((__m128i*)dst, sel(keep, val, cur));
    }
}

template <uint32_t di, alu_op OP, bool TO_ACC, src_kind TK>
static inline void fmac(Vu* vu, int d, int s, int t) {
    __m128 sv = load_cvtf(&vu->vf[s]);
    __m128 tv;

    if constexpr (TK == SRC_VEC) {
        tv = load_cvtf(&vu->vf[t]);
    } else if constexpr (TK == SRC_BCX) {
        __m128 tf = load_cvtf(&vu->vf[t]); tv = _mm_shuffle_ps(tf, tf, _MM_SHUFFLE(0, 0, 0, 0));
    } else if constexpr (TK == SRC_BCY) {
        __m128 tf = load_cvtf(&vu->vf[t]); tv = _mm_shuffle_ps(tf, tf, _MM_SHUFFLE(1, 1, 1, 1));
    } else if constexpr (TK == SRC_BCZ) {
        __m128 tf = load_cvtf(&vu->vf[t]); tv = _mm_shuffle_ps(tf, tf, _MM_SHUFFLE(2, 2, 2, 2));
    } else if constexpr (TK == SRC_BCW) {
        __m128 tf = load_cvtf(&vu->vf[t]); tv = _mm_shuffle_ps(tf, tf, _MM_SHUFFLE(3, 3, 3, 3));
    } else if constexpr (TK == SRC_I) {
        tv = _mm_set1_ps(vu->i.f);
    } else {
        tv = _mm_set1_ps(get_q(vu).f);
    }

    __m128 r;
    if constexpr (OP == OP_ADD) {
        r = _mm_add_ps(sv, tv);
    } else if constexpr (OP == OP_SUB) {
        r = _mm_sub_ps(sv, tv);
    } else if constexpr (OP == OP_MUL) {
        r = _mm_mul_ps(sv, tv);
    } else {
        __m128 acc = load_cvtf(&vu->acc);
        __m128 p = _mm_mul_ps(sv, tv);
        r = (OP == OP_MADD) ? _mm_add_ps(acc, p) : _mm_sub_ps(acc, p);
    }

    __m128i clamped = flags4<di>(vu, _mm_castps_si128(r));

    if constexpr (TO_ACC) {
        write_masked<di>(&vu->acc, clamped);
    } else {
        if (d) write_masked<di>(&vu->vf[d], clamped);
    }

    update_status(vu);
}

#else

template <uint32_t di, alu_op OP, bool TO_ACC, src_kind TK>
static inline void fmac(Vu* vu, int d, int s, int t) {
    float iq = 0.0f;
    if constexpr (TK == SRC_I) iq = vu->i.f;
    else if constexpr (TK == SRC_Q) iq = get_q(vu).f;

    float bc = 0.0f;
    if constexpr (TK == SRC_BCX) bc = vf_x(vu, t);
    else if constexpr (TK == SRC_BCY) bc = vf_y(vu, t);
    else if constexpr (TK == SRC_BCZ) bc = vf_z(vu, t);
    else if constexpr (TK == SRC_BCW) bc = vf_w(vu, t);

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            float sf = vf_i(vu, s, i);
            float tf;
            if constexpr (TK == SRC_VEC) tf = vf_i(vu, t, i);
            else if constexpr (TK == SRC_I || TK == SRC_Q) tf = iq;
            else tf = bc;

            float r;
            if constexpr (OP == OP_ADD) r = sf + tf;
            else if constexpr (OP == OP_SUB) r = sf - tf;
            else if constexpr (OP == OP_MUL) r = sf * tf;
            else if constexpr (OP == OP_MADD) r = acc_i(vu, i) + sf * tf;
            else r = acc_i(vu, i) - sf * tf;

            float out = update_flags(vu, r, i);
            if constexpr (TO_ACC) vu->acc.f[i] = out;
            else set_vf(vu, d, i, out);
        } else {
            clear_flags(vu, i);
        }
    });

    update_status(vu);
}

#endif

#define FMAC_D(name, OP, TK) \
    template <uint32_t di> \
    void name(Vu* vu, const Instruction* ins) { \
        fmac<di, OP, false, TK>(vu, UD_D, UD_S, UD_T); \
    }

#define FMAC_A(name, OP, TK) \
    template <uint32_t di> \
    void name(Vu* vu, const Instruction* ins) { \
        fmac<di, OP, true, TK>(vu, UD_D, UD_S, UD_T); \
    }

FMAC_D(i_add,   OP_ADD, SRC_VEC)
FMAC_D(i_addi,  OP_ADD, SRC_I)
FMAC_D(i_addq,  OP_ADD, SRC_Q)
FMAC_D(i_addx,  OP_ADD, SRC_BCX)
FMAC_D(i_addy,  OP_ADD, SRC_BCY)
FMAC_D(i_addz,  OP_ADD, SRC_BCZ)
FMAC_D(i_addw,  OP_ADD, SRC_BCW)
FMAC_A(i_adda,  OP_ADD, SRC_VEC)
FMAC_A(i_addai, OP_ADD, SRC_I)
FMAC_A(i_addaq, OP_ADD, SRC_Q)
FMAC_A(i_addax, OP_ADD, SRC_BCX)
FMAC_A(i_adday, OP_ADD, SRC_BCY)
FMAC_A(i_addaz, OP_ADD, SRC_BCZ)
FMAC_A(i_addaw, OP_ADD, SRC_BCW)

FMAC_D(i_sub,   OP_SUB, SRC_VEC)
FMAC_D(i_subi,  OP_SUB, SRC_I)
FMAC_D(i_subq,  OP_SUB, SRC_Q)
FMAC_D(i_subx,  OP_SUB, SRC_BCX)
FMAC_D(i_suby,  OP_SUB, SRC_BCY)
FMAC_D(i_subz,  OP_SUB, SRC_BCZ)
FMAC_D(i_subw,  OP_SUB, SRC_BCW)
FMAC_A(i_suba,  OP_SUB, SRC_VEC)
FMAC_A(i_subai, OP_SUB, SRC_I)
FMAC_A(i_subaq, OP_SUB, SRC_Q)
FMAC_A(i_subax, OP_SUB, SRC_BCX)
FMAC_A(i_subay, OP_SUB, SRC_BCY)
FMAC_A(i_subaz, OP_SUB, SRC_BCZ)
FMAC_A(i_subaw, OP_SUB, SRC_BCW)

FMAC_D(i_mul,   OP_MUL, SRC_VEC)
FMAC_D(i_muli,  OP_MUL, SRC_I)
FMAC_D(i_mulq,  OP_MUL, SRC_Q)
FMAC_D(i_mulx,  OP_MUL, SRC_BCX)
FMAC_D(i_muly,  OP_MUL, SRC_BCY)
FMAC_D(i_mulz,  OP_MUL, SRC_BCZ)
FMAC_D(i_mulw,  OP_MUL, SRC_BCW)
FMAC_A(i_mula,  OP_MUL, SRC_VEC)
FMAC_A(i_mulai, OP_MUL, SRC_I)
FMAC_A(i_mulaq, OP_MUL, SRC_Q)
FMAC_A(i_mulax, OP_MUL, SRC_BCX)
FMAC_A(i_mulay, OP_MUL, SRC_BCY)
FMAC_A(i_mulaz, OP_MUL, SRC_BCZ)
FMAC_A(i_mulaw, OP_MUL, SRC_BCW)

FMAC_D(i_madd,   OP_MADD, SRC_VEC)
FMAC_D(i_maddi,  OP_MADD, SRC_I)
FMAC_D(i_maddq,  OP_MADD, SRC_Q)
FMAC_D(i_maddx,  OP_MADD, SRC_BCX)
FMAC_D(i_maddy,  OP_MADD, SRC_BCY)
FMAC_D(i_maddz,  OP_MADD, SRC_BCZ)
FMAC_D(i_maddw,  OP_MADD, SRC_BCW)
FMAC_A(i_madda,  OP_MADD, SRC_VEC)
FMAC_A(i_maddai, OP_MADD, SRC_I)
FMAC_A(i_maddaq, OP_MADD, SRC_Q)
FMAC_A(i_maddax, OP_MADD, SRC_BCX)
FMAC_A(i_madday, OP_MADD, SRC_BCY)
FMAC_A(i_maddaz, OP_MADD, SRC_BCZ)
FMAC_A(i_maddaw, OP_MADD, SRC_BCW)

FMAC_D(i_msub,   OP_MSUB, SRC_VEC)
FMAC_D(i_msubi,  OP_MSUB, SRC_I)
FMAC_D(i_msubq,  OP_MSUB, SRC_Q)
FMAC_D(i_msubx,  OP_MSUB, SRC_BCX)
FMAC_D(i_msuby,  OP_MSUB, SRC_BCY)
FMAC_D(i_msubz,  OP_MSUB, SRC_BCZ)
FMAC_D(i_msubw,  OP_MSUB, SRC_BCW)
FMAC_A(i_msuba,  OP_MSUB, SRC_VEC)
FMAC_A(i_msubai, OP_MSUB, SRC_I)
FMAC_A(i_msubaq, OP_MSUB, SRC_Q)
FMAC_A(i_msubax, OP_MSUB, SRC_BCX)
FMAC_A(i_msubay, OP_MSUB, SRC_BCY)
FMAC_A(i_msubaz, OP_MSUB, SRC_BCZ)
FMAC_A(i_msubaw, OP_MSUB, SRC_BCW)

#ifdef FMAC_SIMD
static inline __m128i sm_key(__m128i x) {
    return _mm_xor_si128(x, _mm_and_si128(_mm_srai_epi32(x, 31), _mm_set1_epi32(0x7fffffff)));
}

template <uint32_t di, bool IS_MAX, src_kind TK>
static inline void minmax(Vu* vu, int d, int s, int t) {
    if (!d) return;
    __m128i a = _mm_loadu_si128((const __m128i*)&vu->vf[s]);
    __m128i b;

    if constexpr (TK == SRC_VEC) b = _mm_loadu_si128((const __m128i*)&vu->vf[t]);
    else if constexpr (TK == SRC_I) b = _mm_set1_epi32(vu->i.s32);
    else if constexpr (TK == SRC_BCX) b = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&vu->vf[t]), _MM_SHUFFLE(0, 0, 0, 0));
    else if constexpr (TK == SRC_BCY) b = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&vu->vf[t]), _MM_SHUFFLE(1, 1, 1, 1));
    else if constexpr (TK == SRC_BCZ) b = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&vu->vf[t]), _MM_SHUFFLE(2, 2, 2, 2));
    else b = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&vu->vf[t]), _MM_SHUFFLE(3, 3, 3, 3));

    __m128i agtb = _mm_cmpgt_epi32(sm_key(a), sm_key(b));
    __m128i res = IS_MAX ? sel(agtb, a, b) : sel(agtb, b, a);

    if (d) write_masked<di>(&vu->vf[d], res);
}
#else
template <uint32_t di, bool IS_MAX, src_kind TK>
static inline void minmax(Vu* vu, int d, int s, int t) {
    if (!d) return;

    int32_t bc = 0;

    if constexpr (TK == SRC_I) bc = vu->i.s32;
    else if constexpr (TK == SRC_BCX) bc = vu->vf[t].s32[0];
    else if constexpr (TK == SRC_BCY) bc = vu->vf[t].s32[1];
    else if constexpr (TK == SRC_BCZ) bc = vu->vf[t].s32[2];
    else if constexpr (TK == SRC_BCW) bc = vu->vf[t].s32[3];

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            int32_t b;
            if constexpr (TK == SRC_VEC) b = vu->vf[t].s32[i];
            else b = bc;
            vu->vf[d].u32[i] = IS_MAX ? max(vu->vf[s].s32[i], b) : min(vu->vf[s].s32[i], b);
        }
    });
}
#endif

#define MINMAX(name, IS_MAX, TK) \
    template <uint32_t di> \
    void name(Vu* vu, const Instruction* ins) { \
        minmax<di, IS_MAX, TK>(vu, UD_D, UD_S, UD_T); \
    }

MINMAX(i_max,   true,  SRC_VEC)
MINMAX(i_maxi,  true,  SRC_I)
MINMAX(i_maxx,  true,  SRC_BCX)
MINMAX(i_maxy,  true,  SRC_BCY)
MINMAX(i_maxz,  true,  SRC_BCZ)
MINMAX(i_maxw,  true,  SRC_BCW)
MINMAX(i_mini,  false, SRC_VEC)
MINMAX(i_minii, false, SRC_I)
MINMAX(i_minix, false, SRC_BCX)
MINMAX(i_miniy, false, SRC_BCY)
MINMAX(i_miniz, false, SRC_BCZ)
MINMAX(i_miniw, false, SRC_BCW)

#ifdef FMAC_SIMD
void i_opmula(Vu* vu, const Instruction* ins) {
    int s = UD_S;
    int t = UD_T;

    __m128 sv = load_cvtf(&vu->vf[s]);
    __m128 tv = load_cvtf(&vu->vf[t]);
    __m128 syzx = _mm_shuffle_ps(sv, sv, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tzxy = _mm_shuffle_ps(tv, tv, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 prod = _mm_mul_ps(syzx, tzxy);

    __m128i pre = cvtf4_i(_mm_castps_si128(prod));
    __m128i clamped = flags4<D_X | D_Y | D_Z>(vu, pre);

    write_masked<D_X | D_Y | D_Z>(&vu->acc, clamped);

    update_status(vu);
}

void i_opmsub(Vu* vu, const Instruction* ins) {
    int d = UD_D;
    int s = UD_S;
    int t = UD_T;

    __m128 sv = load_cvtf(&vu->vf[s]);
    __m128 tv = load_cvtf(&vu->vf[t]);
    __m128 accv = _mm_loadu_ps((const float*)&vu->acc);
    __m128 syzx = _mm_shuffle_ps(sv, sv, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tzxy = _mm_shuffle_ps(tv, tv, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 prod = _mm_mul_ps(syzx, tzxy);
    __m128 r = _mm_sub_ps(accv, prod);

    __m128i clamped = flags4<D_X | D_Y | D_Z>(vu, _mm_castps_si128(r));

    if (d) write_masked<D_X | D_Y | D_Z>(&vu->vf[d], clamped);

    update_status(vu);
}
#else
void i_opmula(Vu* vu, const Instruction* ins) {
    int s = UD_S;
    int t = UD_T;

    vu->acc.x = vf_y(vu, s) * vf_z(vu, t);
    vu->acc.y = vf_z(vu, s) * vf_x(vu, t);
    vu->acc.z = vf_x(vu, s) * vf_y(vu, t);

    vu->acc.x = cvtf(vu->acc.u32[0]);
    vu->acc.y = cvtf(vu->acc.u32[1]);
    vu->acc.z = cvtf(vu->acc.u32[2]);

    vu->acc.x = update_flags(vu, vu->acc.x, 0);
    vu->acc.y = update_flags(vu, vu->acc.y, 1);
    vu->acc.z = update_flags(vu, vu->acc.z, 2);

    clear_flags(vu, 3);
    update_status(vu);
}

void i_opmsub(Vu* vu, const Instruction* ins) {
    int d = UD_D;
    int s = UD_S;
    int t = UD_T;

    Reg128 tmp;

    tmp.f[0] = vu->acc.x - vf_y(vu, s) * vf_z(vu, t);
    tmp.f[1] = vu->acc.y - vf_z(vu, s) * vf_x(vu, t);
    tmp.f[2] = vu->acc.z - vf_x(vu, s) * vf_y(vu, t);

    set_vf_x(vu, d, update_flags(vu, tmp.f[0], 0));
    set_vf_y(vu, d, update_flags(vu, tmp.f[1], 1));
    set_vf_z(vu, d, update_flags(vu, tmp.f[2], 2));

    clear_flags(vu, 3);
    update_status(vu);
}
#endif
void i_nop(Vu* vu, const Instruction* ins) {
    // No operation
}

#ifdef FMAC_SIMD
template <uint32_t di>
static inline void ftoi(Vu* vu, int t, int s, float scale) {
    __m128 f = _mm_mul_ps(load_cvtf(&vu->vf[s]), _mm_set1_ps(scale));
    __m128i iv = _mm_cvttps_epi32(f);
    __m128i povf = _mm_castps_si128(_mm_cmpge_ps(f, _mm_set1_ps(2147483648.0f)));

    iv = sel(povf, _mm_set1_epi32(0x7fffffff), iv);

    if (t) write_masked<di>(&vu->vf[t], iv);
}

template <uint32_t di>
static inline void itof(Vu* vu, int t, int s, float scale) {
    __m128 f = _mm_mul_ps(_mm_cvtepi32_ps(_mm_loadu_si128((const __m128i*)&vu->vf[s])), _mm_set1_ps(scale));

    if (t) write_masked<di>(&vu->vf[t], _mm_castps_si128(f));
}
#else
template <uint32_t di>
static inline void ftoi(Vu* vu, int t, int s, float scale) {
    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            set_vfu(vu, t, i, cvti(vf_i(vu, s, i) * scale));
        }
    });
}

template <uint32_t di>
static inline void itof(Vu* vu, int t, int s, float scale) {
    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            set_vf(vu, t, i, (float)((float)(vu->vf[s].s32[i]) * scale));
        }
    });
}
#endif

#define CONV(name, fn, scale) \
    template <uint32_t di> \
    void name(Vu* vu, const Instruction* ins) { \
        fn<di>(vu, UD_T, UD_S, scale); \
    }

CONV(i_ftoi0,  ftoi, 1.0f)
CONV(i_ftoi4,  ftoi, (1.0f / 0.0625f))
CONV(i_ftoi12, ftoi, (1.0f / 0.000244140625f))
CONV(i_ftoi15, ftoi, (1.0f / 0.000030517578125f))
CONV(i_itof0,  itof, 1.0f)
CONV(i_itof4,  itof, 0.0625f)
CONV(i_itof12, itof, 0.000244140625f)
CONV(i_itof15, itof, 0.000030517578125f)

void i_clip(Vu* vu, const Instruction* ins) {
    int t = UD_T;
    int s = UD_S;

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
void i_b(Vu* vu, const Instruction* ins) {
    branch(vu, vu->tpc + LD_IMM11);
}
void i_bal(Vu* vu, const Instruction* ins) {
    // Instruction next to the delay slot
    IT = delay_slot_link(vu);

    branch(vu, vu->tpc + LD_IMM11);
}

uint64_t jit_div_math(uint32_t nb, uint32_t db) {
    uint32_t sign = (nb ^ db) & 0x80000000;

    double num = ps2_to_double(nb);
    double den = ps2_to_double(db);

    if (den == 0.0) {
        uint64_t flag = num == 0.0 ? (uint64_t)STATUS_I : (uint64_t)STATUS_D;

        return (flag << 32) | (sign | 0x7f7fffff);
    }

    return ps2_pack_double(num / den);
}
uint64_t jit_sqrt_math(uint32_t tb) {
    uint64_t flags = (tb & 0x80000000) ? (uint64_t)STATUS_I : 0;

    double v = ps2_to_double(tb & 0x7fffffff);

    return (flags << 32) | ps2_pack_double(sqrt(v));
}
uint64_t jit_rsqrt_math(uint32_t nb, uint32_t db) {
    uint64_t flags = (db & 0x80000000) ? (uint64_t)STATUS_I : 0;

    double num = ps2_to_double(nb);
    double den = ps2_to_double(db & 0x7fffffff);

    if (den == 0.0) {
        if (num != 0.0) {
            flags |= STATUS_D;
        }

        return (flags << 32) | ((nb & 0x80000000) | 0x7f7fffff);
    }

    return (flags << 32) | ps2_pack_double(num / sqrt(den));
}
void i_div(Vu* vu, const Instruction* ins) {
    uint64_t r = jit_div_math(vu->vf[LD_S].u32[LD_SF], vu->vf[LD_T].u32[LD_TF]);

    vu->status &= ~0x30u;
    vu->status |= (uint32_t)(r >> 32);

    set_q_u32(vu, (uint32_t)r, 7);
}

static inline uint32_t clamp_p(float v) {
    if (std::isnan(v)) {
        return 0x7f7fffff;
    }

    if (std::isinf(v)) {
        return 0x7f7fffff | (std::signbit(v) ? 0x80000000u : 0u);
    }

    Reg32 p;

    p.f = v;

    return p.u32;
}

static inline uint32_t raw_p(float v) {
    Reg32 p;

    p.f = v;

    return p.u32;
}

uint32_t jit_efu_scalar(uint32_t key, uint32_t sb) {
    float s = cvtf(sb);

    switch (key) {
        case 0x78: return clamp_p(sqrtf(fabsf(s)));
        case 0x79: return clamp_p(1.0f / sqrtf(fabsf(s)));
        case 0x7a: return clamp_p(1.0f / s);
        case 0x7c: return raw_p(sinf(s));

        case 0x7d: {
            if (s == -1.0f) {
                return 0xFF7FFFFF;
            }

            return raw_p(atan((s - 1.0f) / (s + 1.0f)));
        }

        case 0x7e: {
            const static float coeffs[] = {
                0.249998688697815f, 0.031257584691048f,
                0.002591371303424f, 0.000171562001924f,
                0.000005430199963f, 0.000000690600018f
            };

            if (sb & 0x80000000) {
                return raw_p(s);
            }

            float value = 1;

            for (int exp = 1; exp <= 6; exp++)
                value += coeffs[exp - 1] * pow(s, exp);

            return raw_p(1.0 / value);
        }
    }

    return 0;
}

uint32_t jit_efu_vector(uint32_t key, uint32_t xb, uint32_t yb, uint32_t zb, uint32_t wb) {
    float x = cvtf(xb);
    float y = cvtf(yb);
    float z = cvtf(zb);

    float x2 = x * x;
    float y2 = y * y;
    float z2 = z * z;

    switch (key) {
        case 0x70: return raw_p(x2 + y2 + z2);
        case 0x71: return raw_p(1.0f / (x2 + y2 + z2));
        case 0x72: return clamp_p(sqrtf(x2 + y2 + z2));
        case 0x73: return clamp_p(1.0f / sqrtf(x2 + y2 + z2));

        case 0x74: {
            if (y + x == 0.0f) {
                return 0x7F7FFFFF | (yb & 0x80000000);
            }

            return raw_p(atan((y - 1.0f) / (y + x)));
        }

        // P = atan(z/x)
        case 0x75: {
            if (z + x == 0.0f) {
                return 0x7F7FFFFF | (zb & 0x80000000);
            }

            return raw_p(atan((z - x) / (z + x)));
        }

        case 0x76: return clamp_p(x + y + z + cvtf(wb));
    }

    return 0;
}

static inline void efu_scalar(Vu* vu, uint32_t key, int s, int sf) {
    vu->p.u32 = jit_efu_scalar(key, vu->vf[s].u32[sf]);
}

static inline void efu_vector(Vu* vu, uint32_t key, int s) {
    vu->p.u32 = jit_efu_vector(key, vu->vf[s].u32[0], vu->vf[s].u32[1],
                                    vu->vf[s].u32[2], vu->vf[s].u32[3]);
}

void i_eatan(Vu* vu, const Instruction* ins) {
    efu_scalar(vu, 0x7d, LD_S, LD_SF);
}
void i_eatanxy(Vu* vu, const Instruction* ins) {
    efu_vector(vu, 0x74, LD_S);
}
void i_eatanxz(Vu* vu, const Instruction* ins) {
    efu_vector(vu, 0x75, LD_S);
}
void i_eexp(Vu* vu, const Instruction* ins) {
    efu_scalar(vu, 0x7e, LD_S, LD_SF);
}
void i_eleng(Vu* vu, const Instruction* ins) {
    efu_vector(vu, 0x72, LD_S);
}
void i_ercpr(Vu* vu, const Instruction* ins) {
    efu_scalar(vu, 0x7a, LD_S, LD_SF);
}
void i_erleng(Vu* vu, const Instruction* ins) {
    efu_vector(vu, 0x73, LD_S);
}
void i_ersadd(Vu* vu, const Instruction* ins) {
    efu_vector(vu, 0x71, LD_S);
}
void i_ersqrt(Vu* vu, const Instruction* ins) {
    efu_scalar(vu, 0x79, LD_S, LD_SF);
}
void i_esadd(Vu* vu, const Instruction* ins) {
    efu_vector(vu, 0x70, LD_S);
}
void i_esin(Vu* vu, const Instruction* ins) {
    efu_scalar(vu, 0x7c, LD_S, LD_SF);
}
void i_esqrt(Vu* vu, const Instruction* ins) {
    efu_scalar(vu, 0x78, LD_S, LD_SF);
}
void i_esum(Vu* vu, const Instruction* ins) {
    efu_vector(vu, 0x76, LD_S);
}

#define CLIP_DELAY 3
#define CLIP_FLAGS(vu) ((vu)->clip_pipeline[CLIP_DELAY])

void i_fcand(Vu* vu, const Instruction* ins) {
    vu->vi[1] = ((CLIP_FLAGS(vu) & 0xffffff) & LD_IMM24) != 0;
}
void i_fceq(Vu* vu, const Instruction* ins) {
    vu->vi[1] = (CLIP_FLAGS(vu) & 0xffffff) == LD_IMM24;
}
void i_fcget(Vu* vu, const Instruction* ins) {
    set_vi(vu, LD_T, CLIP_FLAGS(vu) & 0xfff);
}
void i_fcor(Vu* vu, const Instruction* ins) {
    vu->vi[1] = ((CLIP_FLAGS(vu) & 0xffffff) | LD_IMM24) == 0xffffff;
}
void i_fcset(Vu* vu, const Instruction* ins) {
    vu->clip = LD_IMM24;
}
void i_fmand(Vu* vu, const Instruction* ins) {
    set_vi(vu, LD_T, vu->mac_pipeline[3] & IS);
}
void i_fmeq(Vu* vu, const Instruction* ins) {
    set_vi(vu, LD_T, (IS & 0xffff) == (vu->mac_pipeline[3] & 0xffff));
}
void i_fmor(Vu* vu, const Instruction* ins) {
    set_vi(vu, LD_T, (IS & 0xffff) | (vu->mac_pipeline[3] & 0xffff));
}
void i_fsand(Vu* vu, const Instruction* ins) {
    set_vi(vu, LD_T, vu->status & LD_IMM12);
}
void i_fseq(Vu* vu, const Instruction* ins) {
    set_vi(vu, LD_T, (vu->status & 0xfff) == LD_IMM12);
}
void i_fsor(Vu* vu, const Instruction* ins) {
    set_vi(vu, LD_T, (vu->status & 0xfff) | LD_IMM12);
}
void i_fsset(Vu* vu, const Instruction* ins) {
    vu->status &= 0x3f;
    vu->status |= LD_IMM12 & 0xfc0;
}
void i_iadd(Vu* vu, const Instruction* ins) {
    write_branch_pipeline(vu, LD_D);

    set_vi(vu, LD_D, IS + IT);
}
void i_iaddi(Vu* vu, const Instruction* ins) {
    write_branch_pipeline(vu, LD_T);

    set_vi(vu, LD_T, IS + LD_IMM5);
}
void i_iaddiu(Vu* vu, const Instruction* ins) {
    write_branch_pipeline(vu, LD_T);

    set_vi(vu, LD_T, IS + LD_IMM15);
}
void i_iand(Vu* vu, const Instruction* ins) {
    write_branch_pipeline(vu, LD_D);

    set_vi(vu, LD_D, IS & IT);
}
void i_ibeq(Vu* vu, const Instruction* ins) {
    uint16_t t = get_branch_register(vu, LD_T);
    uint16_t s = get_branch_register(vu, LD_S);

    if (t == s) branch(vu, vu->tpc + LD_IMM11);
}
void i_ibgez(Vu* vu, const Instruction* ins) {
    int16_t s = get_branch_register(vu, LD_S);

    if (s >= 0) branch(vu, vu->tpc + LD_IMM11);
}
void i_ibgtz(Vu* vu, const Instruction* ins) {
    int16_t s = get_branch_register(vu, LD_S);

    if (s > 0) branch(vu, vu->tpc + LD_IMM11);
}
void i_iblez(Vu* vu, const Instruction* ins) {
    int16_t s = get_branch_register(vu, LD_S);

    if (s <= 0) branch(vu, vu->tpc + LD_IMM11);
}
void i_ibltz(Vu* vu, const Instruction* ins) {
    int16_t s = get_branch_register(vu, LD_S);

    if (s < 0) branch(vu, vu->tpc + LD_IMM11);
}
void i_ibne(Vu* vu, const Instruction* ins) {
    uint16_t t = get_branch_register(vu, LD_T);
    uint16_t s = get_branch_register(vu, LD_S);

    if (t != s) branch(vu, vu->tpc + LD_IMM11);
}
template <uint32_t di>
void i_ilw(Vu* vu, const Instruction* ins) {
    int t = LD_T;

    if (!t) return;

    uint32_t addr = IS + LD_IMM11;
    uint128_t data = mem_read(vu, addr);

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            set_vi(vu, t, data.u32[i]);
        }
    });
}
template <uint32_t di>
void i_ilwr(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    if (!t) return;

    uint32_t addr = get_vi(vu, s);
    uint128_t data = mem_read(vu, addr);

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            set_vi(vu, t, data.u32[i]);
        }
    });
}
void i_ior(Vu* vu, const Instruction* ins) {
    write_branch_pipeline(vu, LD_D);

    set_vi(vu, LD_D, IS | IT);
}
void i_isub(Vu* vu, const Instruction* ins) {
    write_branch_pipeline(vu, LD_D);

    set_vi(vu, LD_D, IS - IT);
}
void i_isubiu(Vu* vu, const Instruction* ins) {
    write_branch_pipeline(vu, LD_T);

    set_vi(vu, LD_T, IS - LD_IMM15);
}
template <uint32_t di>
void i_isw(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    uint32_t addr = get_vi(vu, s) + LD_IMM11;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            mem_write(vu, addr, get_vi(vu, t), i);
        }
    });
}
template <uint32_t di>
void i_iswr(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    uint32_t addr = get_vi(vu, s);

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            mem_write(vu, addr, get_vi(vu, t), i);
        }
    });
}
void i_jalr(Vu* vu, const Instruction* ins) {
    uint16_t s = IS;

    IT = delay_slot_link(vu);

    branch(vu, s);
}
void i_jr(Vu* vu, const Instruction* ins) {
    branch(vu, IS);
}
template <uint32_t di>
void i_lq(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    uint32_t addr = get_vi(vu, s) + LD_IMM11;
    uint128_t data = mem_read(vu, addr);

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            vu->vf[t].u32[i] = data.u32[i];
        }
    });
}
template <uint32_t di>
void i_lqd(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    write_branch_pipeline(vu, s);

    set_vi(vu, s, get_vi(vu, s) - 1);

    uint32_t addr = get_vi(vu, s);
    uint128_t data = mem_read(vu, addr);

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            vu->vf[t].u32[i] = data.u32[i];
        }
    });
}
template <uint32_t di>
void i_lqi(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    write_branch_pipeline(vu, s);

    if (t) {
        uint32_t addr = get_vi(vu, s);
        uint128_t data = mem_read(vu, addr);

        template_seq<4>([&](auto i) {
            if constexpr (di & (D_X >> i)) {
                vu->vf[t].u32[i] = data.u32[i];
            }
        });
    }

    set_vi(vu, s, get_vi(vu, s) + 1);
}
template <uint32_t di>
void i_mfir(Vu* vu, const Instruction* ins) {
    int t = LD_T;

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            vu->vf[t].u32[i] = (int32_t)(int16_t)IS;
        }
    });
}
template <uint32_t di>
void i_mfp(Vu* vu, const Instruction* ins) {
    int t = LD_T;

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            vu->vf[t].u32[i] = vu->p.u32;
        }
    });
}
template <uint32_t di>
void i_move(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            vu->vf[t].u32[i] = vu->vf[s].u32[i];
        }
    });
}
template <uint32_t di>
void i_mr32(Vu* vu, const Instruction* ins) {
    int t = LD_T;

    if (!t) return;

    int s = LD_S;

    uint32_t x = vu->vf[s].u32[0];

    if constexpr (di & D_X) {
        vu->vf[t].u32[0] = vu->vf[s].u32[1];
    }
    if constexpr (di & D_Y) {
        vu->vf[t].u32[1] = vu->vf[s].u32[2];
    }
    if constexpr (di & D_Z) {
        vu->vf[t].u32[2] = vu->vf[s].u32[3];
    }
    if constexpr (di & D_W) {
        vu->vf[t].u32[3] = x;
    }
}
void i_mtir(Vu* vu, const Instruction* ins) {
    write_branch_pipeline(vu, LD_T);

    set_vi(vu, LD_T, vu->vf[LD_S].u32[LD_SF] & 0xffff);
}
template <uint32_t di>
void i_rget(Vu* vu, const Instruction* ins) {
    int t = LD_T;

    if (!t) return;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            vu->vf[t].u32[i] = vu->r.u32;
        }
    });
}
void i_rinit(Vu* vu, const Instruction* ins) {
    int s = LD_S;

    vu->r.u32 = 0x3f800000;

    if (!s) return;

    vu->r.u32 |= vu->vf[s].u32[LD_SF] & 0x007fffff;
}
template <uint32_t di>
void i_rnext(Vu* vu, const Instruction* ins) {
    int t = LD_T;

    if (!t) return;

    int x = (vu->r.u32 >> 4) & 1;
    int y = (vu->r.u32 >> 22) & 1;

    vu->r.u32 <<= 1;
    vu->r.u32 ^= x ^ y;
    vu->r.u32 = (vu->r.u32 & 0x7FFFFF) | 0x3F800000;

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            vu->vf[t].u32[i] = vu->r.u32;
        }
    });
}
void i_rsqrt(Vu* vu, const Instruction* ins) {
    uint64_t r = jit_rsqrt_math(vu->vf[LD_S].u32[LD_SF], vu->vf[LD_T].u32[LD_TF]);

    vu->status &= ~0x30u;
    vu->status |= (uint32_t)(r >> 32);

    set_q_u32(vu, (uint32_t)r, 13);
}
void i_rxor(Vu* vu, const Instruction* ins) {
    vu->r.u32 = 0x3F800000 | ((vu->r.u32 ^ vu->vf[LD_S].u32[LD_SF]) & 0x007FFFFF);
}
template <uint32_t di>
void i_sq(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    uint32_t addr = get_vi(vu, t) + LD_IMM11;

    // iris_debug(vu, "sq addr={:08x} vf{}={:08x} {:08x} {:08x} {:08x}", addr, s, vu->vf[s].u32[3], vu->vf[s].u32[2], vu->vf[s].u32[1], vu->vf[s].u32[0]);

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            mem_write(vu, addr, vu->vf[s].u32[i], i);
        }
    });
}
template <uint32_t di>
void i_sqd(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    write_branch_pipeline(vu, t);

    set_vi(vu, t, get_vi(vu, t) - 1);

    uint32_t addr = get_vi(vu, t);

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            mem_write(vu, addr, vu->vf[s].u32[i], i);
        }
    });
}
template <uint32_t di>
void i_sqi(Vu* vu, const Instruction* ins) {
    int s = LD_S;
    int t = LD_T;

    write_branch_pipeline(vu, t);

    uint32_t addr = get_vi(vu, t);

    template_seq<4>([&](auto i) {
        if constexpr (di & (D_X >> i)) {
            mem_write(vu, addr, vu->vf[s].u32[i], i);
        }
    });

    set_vi(vu, t, get_vi(vu, t) + 1);
}
void i_sqrt(Vu* vu, const Instruction* ins) {
    uint64_t r = jit_sqrt_math(vu->vf[LD_T].u32[LD_TF]);

    vu->status &= ~0x30u;
    vu->status |= (uint32_t)(r >> 32);

    set_q_u32(vu, (uint32_t)r, 7);
}
void i_waitp(Vu* vu, const Instruction* ins) {
    // No operation
}
void i_waitq(Vu* vu, const Instruction* ins) {
    vu->q_delay = 0;
}

void jit_mem_load(Vu* vu, uint32_t addr) {
    vu->jit_quad = mem_read(vu, addr);
}

void jit_mem_store(Vu* vu, uint32_t addr, uint32_t field) {
    for (int i = 0; i < 4; i++) {
        if (field & (8u >> i)) {
            mem_write(vu, (uint16_t)addr, vu->jit_quad.u32[i], i);
        }
    }
}

static inline uint16_t xgkick_send_data(Vu* vu, uint16_t addr, int qwc) {
    if (qwc <= 0) {
        return addr;
    }

    uint32_t index = addr & 0x3ff;
    uint32_t remaining = (uint32_t)qwc;

    while (remaining) {
        uint32_t run = 0x400 - index;

        if (run > remaining) {
            run = remaining;
        }

        gif::fifo_write_qwords(vu->gif, (const uint8_t*)&vu->vu_mem[index], run, gif::PATH1);

        remaining -= run;
        index = (index + run) & 0x3ff;
    }

    return (uint16_t)index;
}

void jit_xgkick(Vu* vu, uint32_t start) {
    uint16_t addr = start;

    int eop = 1;

    do {
        uint128_t tag = mem_read(vu, addr++);

        if ((tag.u64[0] | tag.u64[1]) == 0)
            break;

        // addr &= 0x3ff;

        // if (addr == 0) break;

        // iris_debug(vu, "tag: addr={:08x} {:08x} {:08x} {:08x} {:08x}", addr - 1, tag.u32[3], tag.u32[2], tag.u32[1], tag.u32[0]);

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

            iris_fatal_error(vu, "Weird xgkick tag nloop={} nregs={} eop={} flg={} qwc={}", nloop,
                nregs,
                eop,
                flg,
                qwc);
        }

        gif::fifo_write(vu->gif, tag, gif::PATH1);

        if (vu->id) {
            addr = xgkick_send_data(vu, addr, qwc);

            continue;
        }

        for (int i = 0; i < qwc; i++) {
            // iris_debug(vu, "{:08x}: {:08x} {:08x} {:08x} {:08x}", //     addr,
            //     vu->vu_mem[addr].u32[3],
            //     vu->vu_mem[addr].u32[2],
            //     vu->vu_mem[addr].u32[1],
            //     vu->vu_mem[addr].u32[0]
            //);

            gif::fifo_write(vu->gif, mem_read(vu, addr++), gif::PATH1);

            addr &= 0x3ff;

            // if (addr == 0) {
            //     eop = 1;
            //     break;
            // }
        }
    } while (!eop);
}
const uint32_t* jit_vif_top(Vu* vu) {
    return &vu->vif->top;
}

const uint32_t* jit_vif_itop(Vu* vu) {
    return &vu->vif->itop;
}

void i_xgkick(Vu* vu, const Instruction* ins) {
    jit_xgkick(vu, IS);
}

void i_xitop(Vu* vu, const Instruction* ins) {
    set_vi(vu, LD_T, vu->vif->itop);
}
void i_xtop(Vu* vu, const Instruction* ins) {
    if (vu->id == 0) {
        iris_debug(vu, "xtop used in VU0");

        // exit(1);
    }

    set_vi(vu, LD_T, vu->vif->top);
}

uint64_t read8(Vu* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint8_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint8_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
uint64_t read16(Vu* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint16_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint16_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
uint64_t read32(Vu* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint32_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint32_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
uint64_t read64(Vu* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint64_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint64_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
uint128_t read128(Vu* vu, uint32_t addr) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        return *(uint128_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]);
    }

    uint8_t* ptr = (uint8_t*)vu->vu_mem;

    return *(uint128_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]);
}
void write8(Vu* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        invalidate_range(vu, addr, 1);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint8_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint8_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void write16(Vu* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        invalidate_range(vu, addr, 2);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint16_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint16_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void write32(Vu* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        invalidate_range(vu, addr, 4);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint32_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint32_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void write64(Vu* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        invalidate_range(vu, addr, 8);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint64_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint64_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void write128(Vu* vu, uint32_t addr, uint128_t data) {
    if (addr <= 0x3FFF) {
        invalidate_range(vu, addr, 16);

        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint128_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint128_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}

#define FLD_X 1
#define FLD_Y 2
#define FLD_Z 4
#define FLD_W 8

#define DEC_UD_S_SRC_T_BROADCAST(bc, f) \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].mask = (opcode >> 21) & 0xf; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = bc; \
    vu->upper.src[1].mask = 8 >> (bc); \
    vu->upper.func = f;

#define DEC_UD_D_DST_S_SRC_T_BROADCAST(bc, f) \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[0].mask = vu->upper.dst.field; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = bc; \
    vu->upper.src[1].mask = 8 >> (bc); \
    vu->upper.func = f;

#define DEC_UD_D_DST_S_SRC_T_SRC(f) \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[0].mask = vu->upper.dst.field; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = vu->upper.dst.field; \
    vu->upper.src[1].mask = vu->upper.dst.field; \
    vu->upper.func = f;

#define DEC_UD_D_DST_S_SRC(f) \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[0].mask = vu->upper.dst.field; \
    vu->upper.func = f;

#define DEC_UD_D_DST_S_SRC_Q_SRC(f) \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[0].mask = vu->upper.dst.field; \
    vu->upper.src[1].reg = REG_Q; \
    vu->upper.func = f;

#define DEC_UD_T_DST_S_SRC(f) \
    vu->upper.dst.reg = vu->upper.ud_t; \
    vu->upper.dst.field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[0].mask = vu->upper.dst.field; \
    vu->upper.func = f;

#define DEC_UD_S_SRC_T_SRC(f) \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].mask = (opcode >> 21) & 0xf; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = vu->upper.src[0].field; \
    vu->upper.src[1].mask = vu->upper.src[0].field; \
    vu->upper.func = f;

#define DEC_UD_S_SRC(f) \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].mask = (opcode >> 21) & 0xf; \
    vu->upper.func = f;

#define DEC_UD_S_SRC_Q_SRC(f) \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = (opcode >> 21) & 0xf; \
    vu->upper.src[0].mask = (opcode >> 21) & 0xf; \
    vu->upper.src[1].reg = REG_Q; \
    vu->upper.func = f;

#define DEC_OPMULA() \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = FLD_X | FLD_Y | FLD_Z; \
    vu->upper.src[0].mask = FLD_X | FLD_Y | FLD_Z; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = vu->upper.src[0].field; \
    vu->upper.src[1].mask = vu->upper.src[0].field; \
    vu->upper.func = i_opmula;

#define DEC_OPMSUB() \
    vu->upper.dst.reg = vu->upper.ud_d; \
    vu->upper.dst.field = FLD_X | FLD_Y | FLD_Z; \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = vu->upper.dst.field; \
    vu->upper.src[0].mask = vu->upper.dst.field; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = vu->upper.dst.field; \
    vu->upper.src[1].mask = vu->upper.dst.field; \
    vu->upper.func = i_opmsub;

#define DEC_CLIP() \
    vu->upper.src[0].reg = vu->upper.ud_s; \
    vu->upper.src[0].field = FLD_X | FLD_Y | FLD_Z; \
    vu->upper.src[0].mask = FLD_X | FLD_Y | FLD_Z; \
    vu->upper.src[1].reg = vu->upper.ud_t; \
    vu->upper.src[1].field = FLD_W; \
    vu->upper.src[1].mask = FLD_W; \
    vu->upper.func = i_clip;

#define DEC_LD_NONE(f) \
    vu->lower.func = f;

#define DEC_UD_NONE(f) \
    vu->upper.func = f;

#define DEC_LD_T_DST_S_VISRC(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define DEC_LD_T_DST_S_VISRC_S_VIDST(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.vi_dst = vu->lower.ld_s; \
    vu->lower.vi_src[0] = vu->lower.vi_dst; \
    vu->lower.func = f;

#define DEC_LD_S_SRC_T_VISRC_T_VIDST(f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = (opcode >> 21) & 0xf; \
    vu->lower.src[0].mask = (opcode >> 21) & 0xf; \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.vi_src[0] = vu->lower.vi_dst; \
    vu->lower.func = f;

#define DEC_LD_S_SF_SRC_T_TF_SRC(f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.ld_sf; \
    vu->lower.src[0].mask = 8 >> (vu->lower.ld_sf); \
    vu->lower.src[1].reg = vu->lower.ld_t; \
    vu->lower.src[1].field = vu->lower.ld_tf; \
    vu->lower.src[1].mask = 8 >> (vu->lower.ld_tf); \
    vu->lower.func = f;

#define DEC_LD_Q_DST_S_SF_SRC_T_TF_SRC(f) \
    vu->lower.dst.reg = REG_Q; \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.ld_sf; \
    vu->lower.src[0].mask = 8 >> (vu->lower.ld_sf); \
    vu->lower.src[1].reg = vu->lower.ld_t; \
    vu->lower.src[1].field = vu->lower.ld_tf; \
    vu->lower.src[1].mask = 8 >> (vu->lower.ld_tf); \
    vu->lower.func = f;

#define DEC_LD_T_VIDST_S_SF_SRC(f) \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.ld_sf; \
    vu->lower.src[0].mask = 8 >> (vu->lower.ld_sf); \
    vu->lower.func = f;

#define DEC_LD_T_DST_S_VISRC(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define DEC_LD_T_TF_SRC(f) \
    vu->lower.src[0].reg = vu->lower.ld_t; \
    vu->lower.src[0].field = vu->lower.ld_tf; \
    vu->lower.src[0].mask = 8 >> (vu->lower.ld_tf); \
    vu->lower.func = f;

#define DEC_LD_Q_DST_T_TF_SRC(f) \
    vu->lower.dst.reg = REG_Q; \
    vu->lower.src[0].reg = vu->lower.ld_t; \
    vu->lower.src[0].field = vu->lower.ld_tf; \
    vu->lower.src[0].mask = 8 >> (vu->lower.ld_tf); \
    vu->lower.func = f;

#define DEC_LD_S_SRC_T_VISRC(f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = (opcode >> 21) & 0xf; \
    vu->lower.src[0].mask = (opcode >> 21) & 0xf; \
    vu->lower.vi_src[0] = vu->lower.ld_t; \
    vu->lower.func = f;

#define DEC_LD_T_VIDST_S_VISRC(f) \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define DEC_LD_S_VISRC_T_VISRC(f) \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.vi_src[1] = vu->lower.ld_t; \
    vu->lower.func = f;

#define DEC_LD_T_VIDST_S_VISRC(f) \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define DEC_LD_T_VISRC_S_VISRC(f) \
    vu->lower.vi_src[0] = vu->lower.ld_t; \
    vu->lower.vi_src[1] = vu->lower.ld_s; \
    vu->lower.func = f;

#define DEC_LD_VIDST(v, f) \
    vu->lower.vi_dst = v; \
    vu->lower.func = f;

#define DEC_LD_T_VIDST(f) \
    vu->lower.vi_dst = vu->lower.ld_t; \
    vu->lower.func = f;

#define DEC_LD_S_VISRC(f) \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.func = f;

#define DEC_LD_S_FLD_SRC(fld, f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = fld; \
    vu->lower.src[0].mask = fld; \
    vu->lower.func = f;

#define DEC_LD_D_VIDST_S_VISRC_T_VISRC(f) \
    vu->lower.vi_dst = vu->lower.ld_d; \
    vu->lower.vi_src[0] = vu->lower.ld_s; \
    vu->lower.vi_src[1] = vu->lower.ld_t; \
    vu->lower.func = f;

#define DEC_LD_T_DST_S_SRC(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.dst.field; \
    vu->lower.src[0].mask = vu->lower.dst.field; \
    vu->lower.func = f;

#define DEC_LD_T_DST(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.func = f;

#define DEC_LD_S_SF_SRC(f) \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = vu->lower.ld_sf; \
    vu->lower.src[0].mask = 8 >> (vu->lower.ld_sf); \
    vu->lower.func = f;

#define DEC_MR32(f) \
    vu->lower.dst.reg = vu->lower.ld_t; \
    vu->lower.dst.field = (opcode >> 21) & 0xf; \
    vu->lower.src[0].reg = vu->lower.ld_s; \
    vu->lower.src[0].field = (vu->lower.dst.field >> 1) | ((vu->lower.dst.field & 1) << 3); \
    vu->lower.src[0].mask = (vu->lower.dst.field >> 1) | ((vu->lower.dst.field & 1) << 3); \
    vu->lower.func = f;

#define GET_TEMPLATE_FN(i) \
    [&](uint32_t opcode) { \
        switch ((opcode >> 21) & 0xf) { \
            case 0: return &i<0>; \
            case 1: return &i<D_W>; \
            case 2: return &i<D_Z>; \
            case 3: return &i<D_Z | D_W>; \
            case 4: return &i<D_Y>; \
            case 5: return &i<D_Y | D_W>; \
            case 6: return &i<D_Y | D_Z>; \
            case 7: return &i<D_Y | D_Z | D_W>; \
            case 8: return &i<D_X>; \
            case 9: return &i<D_X | D_W>; \
            case 10: return &i<D_X | D_Z>; \
            case 11: return &i<D_X | D_Z | D_W>; \
            case 12: return &i<D_X | D_Y>; \
            case 13: return &i<D_X | D_Y | D_W>; \
            case 14: return &i<D_X | D_Y | D_Z>; \
            case 15: return &i<D_X | D_Y | D_Z | D_W>; \
            default: __builtin_unreachable(); \
        } \
    __builtin_unreachable(); }(opcode)

void decode_upper(Vu* vu, uint32_t opcode) {
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
    vu->upper.src[0].mask = 0;
    vu->upper.src[1].reg = 0;
    vu->upper.src[1].field = 0;
    vu->upper.src[1].mask = 0;

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
            case 0x00: DEC_UD_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_addax)); return;
            case 0x01: DEC_UD_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_adday)); return;
            case 0x02: DEC_UD_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_addaz)); return;
            case 0x03: DEC_UD_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_addaw)); return;
            case 0x04: DEC_UD_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_subax)); return;
            case 0x05: DEC_UD_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_subay)); return;
            case 0x06: DEC_UD_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_subaz)); return;
            case 0x07: DEC_UD_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_subaw)); return;
            case 0x08: DEC_UD_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_maddax)); return;
            case 0x09: DEC_UD_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_madday)); return;
            case 0x0A: DEC_UD_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_maddaz)); return;
            case 0x0B: DEC_UD_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_maddaw)); return;
            case 0x0C: DEC_UD_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_msubax)); return;
            case 0x0D: DEC_UD_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_msubay)); return;
            case 0x0E: DEC_UD_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_msubaz)); return;
            case 0x0F: DEC_UD_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_msubaw)); return;
            case 0x10: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_itof0)); return;
            case 0x11: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_itof4)); return;
            case 0x12: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_itof12)); return;
            case 0x13: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_itof15)); return;
            case 0x14: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_ftoi0)); return;
            case 0x15: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_ftoi4)); return;
            case 0x16: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_ftoi12)); return;
            case 0x17: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_ftoi15)); return;
            case 0x18: DEC_UD_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_mulax)); return;
            case 0x19: DEC_UD_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_mulay)); return;
            case 0x1A: DEC_UD_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_mulaz)); return;
            case 0x1B: DEC_UD_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_mulaw)); return;
            case 0x1C: DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_mulaq)); return;
            case 0x1D: DEC_UD_T_DST_S_SRC(GET_TEMPLATE_FN(i_abs)); return;
            case 0x1E: DEC_UD_S_SRC(GET_TEMPLATE_FN(i_mulai)); return;
            case 0x1F: DEC_CLIP(); return;
            case 0x20: DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_addaq)); return;
            case 0x21: DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_maddaq)); return;
            case 0x22: DEC_UD_S_SRC(GET_TEMPLATE_FN(i_addai)); return;
            case 0x23: DEC_UD_S_SRC(GET_TEMPLATE_FN(i_maddai)); return;
            case 0x24: DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_subaq)); return;
            case 0x25: DEC_UD_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_msubaq)); return;
            case 0x26: DEC_UD_S_SRC(GET_TEMPLATE_FN(i_subai)); return;
            case 0x27: DEC_UD_S_SRC(GET_TEMPLATE_FN(i_msubai)); return;
            case 0x28: DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(i_adda)); return;
            case 0x29: DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(i_madda)); return;
            case 0x2A: DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(i_mula)); return;
            case 0x2C: DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(i_suba)); return;
            case 0x2D: DEC_UD_S_SRC_T_SRC(GET_TEMPLATE_FN(i_msuba)); return;
            case 0x2E: DEC_OPMULA(); return;
            case 0x2F: DEC_UD_NONE(i_nop); return;
        }
    } else {
        // Decode 0000003F style instruction
        switch (opcode & 0x3f) {
            case 0x00: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_addx)); return;
            case 0x01: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_addy)); return;
            case 0x02: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_addz)); return;
            case 0x03: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_addw)); return;
            case 0x04: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_subx)); return;
            case 0x05: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_suby)); return;
            case 0x06: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_subz)); return;
            case 0x07: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_subw)); return;
            case 0x08: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_maddx)); return;
            case 0x09: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_maddy)); return;
            case 0x0A: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_maddz)); return;
            case 0x0B: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_maddw)); return;
            case 0x0C: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_msubx)); return;
            case 0x0D: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_msuby)); return;
            case 0x0E: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_msubz)); return;
            case 0x0F: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_msubw)); return;
            case 0x10: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_maxx)); return;
            case 0x11: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_maxy)); return;
            case 0x12: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_maxz)); return;
            case 0x13: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_maxw)); return;
            case 0x14: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_minix)); return;
            case 0x15: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_miniy)); return;
            case 0x16: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_miniz)); return;
            case 0x17: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_miniw)); return;
            case 0x18: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_X, GET_TEMPLATE_FN(i_mulx)); return;
            case 0x19: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Y, GET_TEMPLATE_FN(i_muly)); return;
            case 0x1A: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_Z, GET_TEMPLATE_FN(i_mulz)); return;
            case 0x1B: DEC_UD_D_DST_S_SRC_T_BROADCAST(FLD_W, GET_TEMPLATE_FN(i_mulw)); return;
            case 0x1C: DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_mulq)); return;
            case 0x1D: DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(i_maxi)); return;
            case 0x1E: DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(i_muli)); return;
            case 0x1F: DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(i_minii)); return;
            case 0x20: DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_addq)); return;
            case 0x21: DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_maddq)); return;
            case 0x22: DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(i_addi)); return;
            case 0x23: DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(i_maddi)); return;
            case 0x24: DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_subq)); return;
            case 0x25: DEC_UD_D_DST_S_SRC_Q_SRC(GET_TEMPLATE_FN(i_msubq)); return;
            case 0x26: DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(i_subi)); return;
            case 0x27: DEC_UD_D_DST_S_SRC(GET_TEMPLATE_FN(i_msubi)); return;
            case 0x28: DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(i_add)); return;
            case 0x29: DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(i_madd)); return;
            case 0x2A: DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(i_mul)); return;
            case 0x2B: DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(i_max)); return;
            case 0x2C: DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(i_sub)); return;
            case 0x2D: DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(i_msub)); return;
            case 0x2E: DEC_OPMSUB(); return;
            case 0x2F: DEC_UD_D_DST_S_SRC_T_SRC(GET_TEMPLATE_FN(i_mini)); return;
        }
    }
}

void decode_lower(Vu* vu, uint32_t opcode) {
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
    vu->lower.src[0].mask = 0;
    vu->lower.src[1].reg = 0;
    vu->lower.src[1].field = 0;
    vu->lower.src[1].mask = 0;
    vu->lower.vi_src[0] = 0;
    vu->lower.vi_src[1] = 0;
    vu->lower.vi_dst = 0;
    vu->lower.branch = 0;

    switch ((opcode & 0xFE000000) >> 25) {
        case 0x00: DEC_LD_T_DST_S_VISRC(GET_TEMPLATE_FN(i_lq)); return;
        case 0x01: DEC_LD_S_SRC_T_VISRC(GET_TEMPLATE_FN(i_sq)); return;
        case 0x04: DEC_LD_T_VIDST_S_VISRC(GET_TEMPLATE_FN(i_ilw)); return;
        case 0x05: DEC_LD_S_VISRC_T_VISRC(GET_TEMPLATE_FN(i_isw)); return;
        case 0x08: DEC_LD_T_VIDST_S_VISRC(i_iaddiu); return;
        case 0x09: DEC_LD_T_VIDST_S_VISRC(i_isubiu); return;

        // Note: The flag check instructions clobber the destination register
        //       "immediately", this means we don't actually need to generate
        //       a dependency.
        case 0x10: DEC_LD_NONE(i_fceq); return;
        case 0x11: DEC_LD_NONE(i_fcset); return;
        case 0x12: DEC_LD_NONE(i_fcand); return;
        case 0x13: DEC_LD_NONE(i_fcor); return;
        case 0x14: DEC_LD_NONE(i_fseq); return;
        case 0x15: DEC_LD_NONE(i_fsset); return;
        case 0x16: DEC_LD_NONE(i_fsand); return;
        case 0x17: DEC_LD_NONE(i_fsor); return;
        case 0x18: DEC_LD_S_VISRC(i_fmeq); return;
        case 0x1A: DEC_LD_S_VISRC(i_fmand); return;
        case 0x1B: DEC_LD_S_VISRC(i_fmor); return;
        case 0x1C: DEC_LD_NONE(i_fcget); return;
        case 0x20: vu->lower.branch = 1; DEC_LD_NONE(i_b); return;
        case 0x21: vu->lower.branch = 1; DEC_LD_T_VIDST(i_bal); return;
        case 0x24: vu->lower.branch = 1; DEC_LD_S_VISRC(i_jr); return;
        case 0x25: vu->lower.branch = 1; DEC_LD_T_VIDST_S_VISRC(i_jalr); return;
        case 0x28: vu->lower.branch = 1; DEC_LD_S_VISRC_T_VISRC(i_ibeq); return;
        case 0x29: vu->lower.branch = 1; DEC_LD_S_VISRC_T_VISRC(i_ibne); return;
        case 0x2C: vu->lower.branch = 1; DEC_LD_S_VISRC(i_ibltz); return;
        case 0x2D: vu->lower.branch = 1; DEC_LD_S_VISRC(i_ibgtz); return;
        case 0x2E: vu->lower.branch = 1; DEC_LD_S_VISRC(i_iblez); return;
        case 0x2F: vu->lower.branch = 1; DEC_LD_S_VISRC(i_ibgez); return;
        case 0x40: {
            if ((opcode & 0x3C) == 0x3C) {
                switch (((opcode & 0x7C0) >> 4) | (opcode & 3)) {
                    case 0x30: DEC_LD_T_DST_S_SRC(GET_TEMPLATE_FN(i_move)); return;
                    case 0x31: DEC_MR32(GET_TEMPLATE_FN(i_mr32)); return;
                    case 0x34: DEC_LD_T_DST_S_VISRC_S_VIDST(GET_TEMPLATE_FN(i_lqi)); return;
                    case 0x35: DEC_LD_S_SRC_T_VISRC_T_VIDST(GET_TEMPLATE_FN(i_sqi)); return;
                    case 0x36: DEC_LD_T_DST_S_VISRC_S_VIDST(GET_TEMPLATE_FN(i_lqd)); return;
                    case 0x37: DEC_LD_S_SRC_T_VISRC_T_VIDST(GET_TEMPLATE_FN(i_sqd)); return;
                    case 0x38: DEC_LD_Q_DST_S_SF_SRC_T_TF_SRC(i_div); return;
                    case 0x39: DEC_LD_Q_DST_T_TF_SRC(i_sqrt); return;
                    case 0x3A: DEC_LD_Q_DST_S_SF_SRC_T_TF_SRC(i_rsqrt); return;
                    case 0x3B: DEC_LD_NONE(i_waitq); return;
                    case 0x3C: DEC_LD_T_VIDST_S_SF_SRC(i_mtir); return;
                    case 0x3D: DEC_LD_T_DST_S_VISRC(GET_TEMPLATE_FN(i_mfir)); return;
                    case 0x3E: DEC_LD_T_VIDST_S_VISRC(GET_TEMPLATE_FN(i_ilwr)); return;
                    case 0x3F: DEC_LD_T_VISRC_S_VISRC(GET_TEMPLATE_FN(i_iswr)); return;
                    case 0x40: DEC_LD_T_DST(GET_TEMPLATE_FN(i_rnext)); return;
                    case 0x41: DEC_LD_T_DST(GET_TEMPLATE_FN(i_rget)); return;
                    case 0x42: DEC_LD_S_SF_SRC(i_rinit); return;
                    case 0x43: DEC_LD_S_SF_SRC(i_rxor); return;
                    case 0x64: DEC_LD_T_DST(GET_TEMPLATE_FN(i_mfp)); return;
                    case 0x68: DEC_LD_T_VIDST(i_xtop); return;
                    case 0x69: DEC_LD_T_VIDST(i_xitop); return;
                    case 0x6C: DEC_LD_S_VISRC(i_xgkick); return;
                    case 0x70: DEC_LD_S_FLD_SRC(FLD_X | FLD_Y | FLD_Z, i_esadd); return;
                    case 0x71: DEC_LD_S_FLD_SRC(FLD_X | FLD_Y | FLD_Z, i_ersadd); return;
                    case 0x72: DEC_LD_S_FLD_SRC(FLD_X | FLD_Y | FLD_Z, i_eleng); return;
                    case 0x73: DEC_LD_S_FLD_SRC(FLD_X | FLD_Y | FLD_Z, i_erleng); return;
                    case 0x74: DEC_LD_S_FLD_SRC(FLD_X | FLD_Y, i_eatanxy); return;
                    case 0x75: DEC_LD_S_FLD_SRC(FLD_X | FLD_Z, i_eatanxz); return;
                    case 0x76: DEC_LD_S_FLD_SRC(FLD_X | FLD_Y | FLD_Z | FLD_W, i_esum); return;
                    case 0x78: DEC_LD_S_SF_SRC(i_esqrt); return;
                    case 0x79: DEC_LD_S_SF_SRC(i_ersqrt); return;
                    case 0x7A: DEC_LD_S_SF_SRC(i_ercpr); return;
                    case 0x7B: DEC_LD_NONE(i_waitp); return;
                    case 0x7C: DEC_LD_S_SF_SRC(i_esin); return;
                    case 0x7D: DEC_LD_S_SF_SRC(i_eatan); return;
                    case 0x7E: DEC_LD_S_SF_SRC(i_eexp); return;
                }
            } else {
                switch (opcode & 0x3F) {
                    case 0x30: DEC_LD_D_VIDST_S_VISRC_T_VISRC(i_iadd); return;
                    case 0x31: DEC_LD_D_VIDST_S_VISRC_T_VISRC(i_isub); return;
                    case 0x32: DEC_LD_T_VIDST_S_VISRC(i_iaddi); return;
                    case 0x34: DEC_LD_D_VIDST_S_VISRC_T_VISRC(i_iand); return;
                    case 0x35: DEC_LD_D_VIDST_S_VISRC_T_VISRC(i_ior); return;
                }
            }
        } break;
    }
}

static inline void advance_fmac_pipeline(Vu* vu) {
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

static inline int get_fmac_stall_cycles(Vu* vu) {
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

Block* find_block(Vu* vu, uint32_t tpc) {
    if (tpc == vu->last_block_lookup_tpc) {
        return vu->last_block_ptr;
    }

    Block* block = &vu->block_cache[tpc & vu->micro_mem_size];

    if (!block->cycles || block->tpc != tpc) {
        return nullptr;
    }

    // Update cache for next lookup
    vu->last_block_lookup_tpc = tpc;
    vu->last_block_ptr = block;

    return block;
}

static int c = 0;

static inline int vf_write_mask(const Instruction& ins);

static inline void set_read_operand(BlockEntry& entry, int slot, int reg, int field) {
    if (reg <= 0 || reg >= 32 || !field) {
        entry.read_reg[slot] = 0;
        entry.read_mask[slot] = 0;

        return;
    }

    uint8_t mask = 0;

    for (int c = 0; c < 4; c++) {
        if (field & (0x8 >> c)) {
            mask |= (uint8_t)(1 << c);
        }
    }

    entry.read_reg[slot] = (uint8_t)reg;
    entry.read_mask[slot] = mask;
}

static inline int decode_stall(const BlockEntry& entry, const uint64_t ready[32][4], uint64_t cycle) {
    uint64_t wait = cycle;

    for (int i = 0; i < 4; i++) {
        int reg = entry.read_reg[i];

        if (!reg) {
            continue;
        }

        for (int c = 0; c < 4; c++) {
            if (!(entry.read_mask[i] & (1 << c))) {
                continue;
            }

            if (ready[reg][c] > wait) {
                wait = ready[reg][c];
            }
        }
    }

    return (int)(wait - cycle);
}

static inline void record_decode_writes(const BlockEntry& entry, uint64_t ready[32][4], uint64_t issue) {
    if (!entry.lw_reg) {
        return;
    }

    for (int c = 0; c < 4; c++) {
        if (entry.lw_mask & (1 << c)) {
            ready[entry.lw_reg][c] = issue + VF_LATENCY;
        }
    }
}

static inline uint64_t hash_micro_mem(Vu* vu, uint32_t tpc, uint32_t words) {
    uint64_t h = 0xcbf29ce484222325ull;

    for (uint32_t i = 0; i < words; i++) {
        h ^= vu->micro_mem[(tpc + i) & vu->micro_mem_size];
        h *= 0x100000001b3ull;
    }

    return h;
}

static inline void note_block_length(Vu* vu, const Block* block) {
    if ((uint32_t)block->cycles > vu->longest_block) {
        vu->longest_block = (uint32_t)block->cycles;
    }
}

static inline bool adopt_victim_without_decoding(Vu* vu, Block* block, int max_cycles) {
    uint32_t lengths[8];
    uint32_t count = jit::victim_lengths(vu, block->tpc, lengths, 8);

    for (uint32_t index = 0; index < count; index++) {
        if (lengths[index] > (uint32_t)max_cycles) {
            continue;
        }

        block->src_len = lengths[index];
        block->src_hash = hash_micro_mem(vu, block->tpc, lengths[index]);

        if (jit::adopt_block(vu, block)) {
            block->cycles = (int)block->src_len;

            note_block_length(vu, block);

            return true;
        }
    }

    block->src_len = 0;
    block->src_hash = 0;

    return false;
}

Block* cache_block(Vu* vu, uint32_t tpc, int max_cycles) {
    Block* block = &vu->block_cache[tpc & vu->micro_mem_size];

    if (block->src_len && block->tpc == tpc && block->src_hash == hash_micro_mem(vu, tpc, block->src_len)) {
        block->cycles = (int)block->src_len;

        note_block_length(vu, block);

        vu->block_cache_size++;

        vu->last_block_lookup_tpc = tpc;
        vu->last_block_ptr = block;

        return block;
    }

    jit::save_runs(vu, block);
    jit::stash_block(vu, block);

    vu->block_cache_size++;

    block->tpc = tpc;
    block->cycles = 0;
    block->runs = 0;
    block->entries.clear();

    if (adopt_victim_without_decoding(vu, block, max_cycles)) {
        vu->last_block_lookup_tpc = block->tpc;
        vu->last_block_ptr = block;

        return block;
    }

    // iris_debug(vu, "caching block at {:04x}", tpc);

    bool delay_slot = false;
    bool terminate = false;

    uint64_t ready[32][4] = {};
    uint64_t cycle = 0;

    for (int i = 0; i < max_cycles; i++) {
        BlockEntry entry = { 0 };

        uint64_t liw = vu->micro_mem[tpc++ & vu->micro_mem_size];
        uint32_t upper = liw >> 32;
        uint32_t lower = liw & 0xffffffff;

        // LOI consumes the raw lower word when the i-bit is set.
        entry.lower.opcode = lower;

        entry.i_bit = (upper & 0x80000000) != 0;
        entry.e_bit = (upper & 0x40000000) != 0;
        entry.m_bit = (upper & 0x20000000) != 0;

        decode_upper(vu, upper & 0x7ffffff);

        entry.upper = vu->upper;

        if (!entry.i_bit) {
            decode_lower(vu, lower);

            entry.lower = vu->lower;
            entry.branch = vu->lower.branch;
            entry.hazard0 = vu->upper.dst.reg == vu->lower.src[0].reg;
            entry.hazard1 = vu->upper.dst.reg == vu->lower.src[1].reg;
            entry.hazard2 = vu->upper.dst.reg == vu->lower.dst.reg;
            entry.hazard3 = vu->lower.dst.reg == REG_Q;

            const int lower_written = vu->lower.dst.reg;

            const bool lower_writes_vf = lower_written > 0
                && lower_written < 32
                && lower_written != vu->upper.dst.reg;

            const bool upper_reads_lower_dst = lower_written == vu->upper.src[0].reg
                || lower_written == vu->upper.src[1].reg;

            entry.swap_hazard = (entry.hazard0 || entry.hazard1)
                && lower_writes_vf
                && upper_reads_lower_dst;

            set_read_operand(entry, 0, vu->lower.src[0].reg, vu->lower.src[0].mask);
            set_read_operand(entry, 1, vu->lower.src[1].reg, vu->lower.src[1].mask);

            entry.lw_reg = (entry.lower.dst.reg && entry.lower.dst.reg < 32) ? entry.lower.dst.reg : 0;
            entry.lw_mask = entry.lw_reg ? vf_write_mask(entry.lower) : 0;
            entry.is_mtir = (entry.lower.func == i_mtir) && (entry.lower.src[0].reg != 0);
            entry.mtir_reg = entry.lower.src[0].reg;
            entry.mtir_comp = entry.lower.src[0].field;
            entry.is_waitq = entry.lower.func == i_waitq;
            entry.lower_is_nop = entry.lower.func == i_nop;
        }

        set_read_operand(entry, 2, vu->upper.src[0].reg, vu->upper.src[0].mask);
        set_read_operand(entry, 3, vu->upper.src[1].reg, vu->upper.src[1].mask);

        entry.uw_reg = (entry.upper.dst.reg && entry.upper.dst.reg < 32) ? entry.upper.dst.reg : 0;
        entry.uw_mask = entry.uw_reg ? vf_write_mask(entry.upper) : 0;

        int stall = decode_stall(entry, ready, cycle);

        entry.stall = (uint8_t)(stall > 255 ? 255 : stall);

        cycle += entry.stall;

        record_decode_writes(entry, ready, cycle);

        cycle++;

        if (terminate) {
            block->cycles++;
            block->entries.push_back(entry);

            break;
        }

        terminate = entry.branch || entry.e_bit;
        block->cycles++;

        block->entries.push_back(entry);
    }


    block->src_len = (uint32_t)block->cycles;
    block->src_hash = hash_micro_mem(vu, block->tpc, block->src_len);

    note_block_length(vu, block);

    jit::adopt_block(vu, block);
    jit::restore_runs(vu, block);

    // Prime fast lookup with a pointer known to be valid after this insertion.
    vu->last_block_lookup_tpc = block->tpc;
    vu->last_block_ptr = block;

    return block;
}

static inline void shift_flag_pipeline(Vu* vu) {
    vu->mac_pipeline[3] = vu->mac_pipeline[2];
    vu->mac_pipeline[2] = vu->mac_pipeline[1];
    vu->mac_pipeline[1] = vu->mac_pipeline[0];
    vu->mac_pipeline[0] = vu->mac;

    vu->clip_pipeline[3] = vu->clip_pipeline[2];
    vu->clip_pipeline[2] = vu->clip_pipeline[1];
    vu->clip_pipeline[1] = vu->clip_pipeline[0];
    vu->clip_pipeline[0] = vu->clip;
}

static inline int vf_write_mask(const Instruction& ins) {
    if (ins.func == i_opmsub)
        return 0x7;

    int mask = 0;

    for (int c = 0; c < 4; c++) {
        if (ins.dst.field & (0x8 >> c)) {
            mask |= 1 << c;
        }
    }

    return mask;
}

static inline int interlock_stall(Vu* vu, const BlockEntry& entry) {
    int stall = entry.stall;

    if (entry.is_mtir) {
        uint64_t ready = vu->vf_ready[entry.mtir_reg][entry.mtir_comp];

        if (ready > vu->vu_cycle + stall) {
            stall = (int)(ready - vu->vu_cycle);
        }
    }

    return stall;
}

static inline void record_vf_writes(Vu* vu, const BlockEntry& entry) {
    if (entry.uw_reg) {
        for (int c = 0; c < 4; c++) {
            if (entry.uw_mask & (1 << c)) {
                vu->vf_ready[entry.uw_reg][c] = vu->vu_cycle + VF_LATENCY;
            }
        }
    }

    if (entry.lw_reg) {
        for (int c = 0; c < 4; c++){
            if (entry.lw_mask & (1 << c)) {
                vu->vf_ready[entry.lw_reg][c] = vu->vu_cycle + VF_LATENCY;
            }
        }
    }
}

static inline void entry_prologue(Vu* vu, const BlockEntry& entry) {
    for (int stall = interlock_stall(vu, entry); stall--; ) {
        if (vu->q_delay)
            vu->q_delay--;

        shift_flag_pipeline(vu);

        vu->vu_cycle++;
    }

    if (vu->q_delay)
        vu->q_delay--;

    update_status(vu);
}

static inline void entry_epilogue(Vu* vu, const BlockEntry& entry) {
    shift_flag_pipeline(vu);

    if (vu->vi_backup_cycles) {
        vu->vi_backup_cycles--;

        if (!vu->vi_backup_cycles) {
            vu->vi_backup_reg = 0;
            vu->vi_backup_value = 0;
        }
    }

    record_vf_writes(vu, entry);

    vu->vu_cycle++;
}

void execute_block_entry(Vu* vu, const BlockEntry& entry) {
    entry_prologue(vu, entry);

    if (entry.i_bit) {
        entry.upper.func(vu, &entry.upper);

        // LOI
        vu->i.u32 = entry.lower.opcode;
    } else {
        if (entry.hazard3 && vu->q_delay) vu->q_delay = 0;

        if (!entry.upper.dst.reg) {
            entry.upper.func(vu, &entry.upper);

            if (!entry.lower_is_nop) entry.lower.func(vu, &entry.lower);
        } else if (entry.swap_hazard) {
            Reg128 kept = vu->vf[entry.lower.dst.reg];

            entry.lower.func(vu, &entry.lower);

            Reg128 written = vu->vf[entry.lower.dst.reg];

            vu->vf[entry.lower.dst.reg] = kept;

            entry.upper.func(vu, &entry.upper);

            vu->vf[entry.lower.dst.reg] = written;
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

            Reg128 tmp = vu->vf[entry.upper.dst.reg];

            entry.lower.func(vu, &entry.lower);

            vu->vf[entry.upper.dst.reg] = tmp;
        } else {
            entry.upper.func(vu, &entry.upper);

            if (!entry.lower_is_nop) entry.lower.func(vu, &entry.lower);
        }
    }

    entry_epilogue(vu, entry);
}

void jit_execute_entry(Vu* vu, const BlockEntry* entry) {
    execute_block_entry(vu, *entry);
}

void jit_entry_prologue(Vu* vu, const BlockEntry* entry) {
    entry_prologue(vu, *entry);
}

void jit_entry_epilogue(Vu* vu, const BlockEntry* entry) {
    entry_epilogue(vu, *entry);
}

void jit_entry_stall(Vu* vu, const BlockEntry* entry) {
    if (!entry->is_mtir) {
        return;
    }

    uint64_t ready = vu->vf_ready[entry->mtir_reg][entry->mtir_comp];

    int excess = ready > vu->vu_cycle ? (int)(ready - vu->vu_cycle) : 0;

    for (int stall = excess; stall--; ) {
        if (vu->q_delay)
            vu->q_delay--;

        shift_flag_pipeline(vu);

        vu->vu_cycle++;
    }
}

void jit_execute_upper(Vu* vu, const BlockEntry* entry) {
    // if (!entry->upper.func) {
    //     fprintf(stderr, "jit_execute_upper: null handler, tpc=%04x opcode=%08x" "\n",
    //         vu->tpc, entry->upper.opcode);
    //     fflush(stderr);
    //     abort();
    // }

    entry->upper.func(vu, &entry->upper);
}

void jit_execute_lower(Vu* vu, const BlockEntry* entry) {
    // if (!entry->lower.func) {
    //     fprintf(stderr, "jit_execute_lower: null handler, tpc=%04x opcode=%08x i_bit=%d" "\n",
    //         vu->tpc, entry->lower.opcode, entry->i_bit);
    //     fflush(stderr);
    //     abort();
    // }

    entry->lower.func(vu, &entry->lower);
}

bool execute_block(Vu* vu, Block* block) {
    // iris_debug(vu, "Input TPC {:04x}", vu->tpc);

    for (const BlockEntry& entry : block->entries) {
        if (entry.m_bit) {
            vu->m_bit_pending = 1;

            vu->e_bit = 2;
        }

        if (entry.e_bit)
            vu->e_bit = 2;

        vu->tpc = (vu->tpc + 1) & vu->micro_mem_size;

        execute_block_entry(vu, entry);

        bool taken = false;

        if (vu->branch_delay && !--vu->branch_delay) {
            vu->tpc = vu->branch_pc;

            if (vu->delay_branch) {
                vu->branch_delay = 1;
                vu->branch_pc = vu->delay_branch_pc;
                vu->delay_branch = false;
            }

            taken = true;
        }

        if (vu->e_bit && !--vu->e_bit) {
            if (vu->m_bit_pending) {
                vu->m_bit_pending = 0;
                vu->waiting_for_interlock = true;
            }

            return true;
        }

        if (taken)
            break;
    }

    // iris_debug(vu, "Output TPC {:04x}", vu->tpc);

    return false;
}

static inline void count_block_run(Vu* vu, Block* block, bool compiled) {
    uint64_t entries = block->entries.size();

    if (vu->id && compiled) {
        profile::count(profile::VU1_BLOCKS_COMPILED_RUN);
        profile::count(profile::VU1_ENTRIES_COMPILED_RUN, entries);
    } else if (vu->id) {
        profile::count(profile::VU1_BLOCKS_INTERPRETED_RUN);
        profile::count(profile::VU1_ENTRIES_INTERPRETED_RUN, entries);
    } else if (compiled) {
        profile::count(profile::VU0_BLOCKS_COMPILED_RUN);
        profile::count(profile::VU0_ENTRIES_COMPILED_RUN, entries);
    } else {
        profile::count(profile::VU0_BLOCKS_INTERPRETED_RUN);
        profile::count(profile::VU0_ENTRIES_INTERPRETED_RUN, entries);
    }
}

static bool run_block(Vu* vu, Block* block) {
    if (vu->engine != VU_ENGINE_JIT) {
        count_block_run(vu, block, false);

        return execute_block(vu, block);
    }

    if (block->func && block->region_blocks > 1 && block->region_epoch != vu->region_epoch) {
        bool intact = true;

        bool moved = false;
        bool content = false;

        for (const RegionDep& dep : block->region_deps) {
            const Block& member = vu->block_cache[dep.tpc & vu->micro_mem_size];

            if (member.tpc != dep.tpc
                || (const void*)member.entries.data() != dep.entries) {
                moved = true;
            }

            if (member.src_hash != dep.hash
                || hash_micro_mem(vu, dep.tpc, dep.len) != dep.hash) {
                content = true;
            }

            if (moved || content) {
                intact = false;

                break;
            }
        }

        if (intact) {
            block->region_epoch = vu->region_epoch;
        } else {
            if (content && block->region_churn < 0xffff) {
                block->region_churn++;
            }

            jit::release_block(vu->jit, block);

            block->jit_failed = false;
        }
    }

    if (!block->func && !block->jit_failed) {
        if ((int)block->runs < vu->jit_threshold) {
            block->runs++;

            count_block_run(vu, block, false);

            return execute_block(vu, block);
        }

        if (!block->compile_pending) {
            jit::compile_block(vu, block);
        }
    }

    if (!block->func) {
        count_block_run(vu, block, false);

        return execute_block(vu, block);
    }

    count_block_run(vu, block, true);

    block->func(vu);

    return vu->jit_exit != VU_JIT_CONTINUE;
}

static void run(Vu* vu) {
    if (vu->id) {
        profile::count(profile::VU1_PROGRAMS);
    } else {
        profile::count(profile::VU0_PROGRAMS);
    }

    int previous_jit = profile::get_active_jit();

    if (vu->id) {
        profile::set_active_jit(profile::JIT_VU1);
    } else {
        profile::set_active_jit(profile::JIT_VU0);
    }

    if (vu->engine == VU_ENGINE_JIT) {
        jit::flush_if_needed(vu);
        jit::drain_blocks(vu);
    }

    const uint64_t deadline = vu->vu_cycle + vu->max_cycles;

    vu->run_deadline = vu->max_cycles ? deadline : ~0ull;

    while (true) {
        Block* block = &vu->block_cache[vu->tpc & vu->micro_mem_size];

        if (!block->cycles || block->tpc != vu->tpc) {
            block = cache_block(vu, vu->tpc, 64);
        }

        if (run_block(vu, block)) {
            break;
        }

        if (vu->max_cycles && vu->vu_cycle >= deadline) {
            iris_fatal_error(vu,
                "Microprogram ran for {} cycles without reaching an E bit, tpc={:04x}",
                vu->max_cycles, vu->tpc
            );

            break;
        }
    }

    profile::set_active_jit(previous_jit);
}

void execute_program(Vu* vu, uint32_t addr) {
    if (vu->disable)
        return;

    // Clear VU0 interlock
    vu->waiting_for_interlock = false;
    vu->m_bit_pending = 0;

    vu->tpc = addr & vu->micro_mem_size;
    vu->i_bit = 0;
    vu->e_bit = 0;
    vu->branch_delay = 0;
    vu->delay_branch = false;

    vu->vu_cycle += VF_LATENCY + 1;

    run(vu);
}

void write_vi(Vu* vu, int index, uint32_t value) {
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
                profile::count(profile::VU0_RESETS);

                reset_registers(vu);
            }

            if (value & 0x200) {
                // Reset VU1
                profile::count(profile::VU1_RESETS);

                if (vu->mtvu) {
                    mtvu::push_vu1_reset(vu->mtvu);
                } else {
                    reset_registers(vu->vu1);
                }
            }
        } break;
        case 29: return; // VU VPU-STAT register, read-only
        case 30: return; // VU reserved register, read-only
        case 31: {
            vu->cmsar1 = value & 0xffff;

            if (vu->mtvu) {
                mtvu::push_vu1_execute(vu->mtvu, vu->cmsar1 >> 3);
            } else {
                execute_program(vu->vu1, vu->cmsar1 >> 3);
            }
        } break;
    }
}

void macro_step(Vu* vu, uint32_t status) {
    vu->status = status;

    fold_status(vu, vu->mac);
}

uint32_t read_vi(Vu* vu, int index) {
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

void reset_registers(Vu* vu) {
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

    vu->upload_lo = ~0u;
    vu->upload_hi = 0;

    vu->vf[0].w = 1.0;

    fesetround(FE_TOWARDZERO);
}

void reset(Vu* vu) {
    reset_registers(vu);

    jit::flush_blocks(vu);

    vu->block_cache_size = 0;
    vu->block_cache.clear();
    vu->block_cache.resize(vu->micro_mem_size+1);
}

void execute_program_tpc(Vu* vu) {
    if (vu->disable)
        return;

    // Clear VU0 interlock
    vu->waiting_for_interlock = false;

    run(vu);
}

uint128_t* get_vu_mem_ptr(Vu* vu, uint32_t addr) {
    return &vu->vu_mem[addr & vu->vu_mem_size];
}

uint32_t get_vu_mem_size(Vu* vu) {
    return (uint32_t)vu->vu_mem_size;
}

uint64_t* get_micro_mem_ptr(Vu* vu, uint32_t addr) {
    return &vu->micro_mem[addr & vu->micro_mem_size];
}

void begin_micro_upload(Vu* vu) {
    vu->upload_lo = ~0u;
    vu->upload_hi = 0;
}

void upload_micro_word(Vu* vu, uint32_t word_addr, uint64_t data) {
    word_addr &= vu->micro_mem_size;

    if (vu->micro_mem[word_addr] == data) {
        return;
    }

    vu->micro_mem[word_addr] = data;

    if (word_addr < vu->upload_lo) {
        vu->upload_lo = word_addr;
    }

    if (word_addr > vu->upload_hi) {
        vu->upload_hi = word_addr;
    }
}

void upload_micro_words(Vu* vu, uint32_t word_addr, const uint8_t* data, uint32_t count) {
    for (uint32_t index = 0; index < count; index++) {
        uint64_t word;

        memcpy(&word, data + (size_t)index * sizeof(word), sizeof(word));

        upload_micro_word(vu, word_addr + index, word);
    }
}

void end_micro_upload(Vu* vu) {
    if (vu->upload_lo > vu->upload_hi) {
        return;
    }

    profile::count(profile::VU_MICRO_UPLOADS);

    invalidate_range(vu, vu->upload_lo << 3, (vu->upload_hi - vu->upload_lo + 1) << 3);

    vu->upload_lo = ~0u;
    vu->upload_hi = 0;
}

void write_micro_mem(Vu* vu, uint32_t word_addr, uint64_t data) {
    profile::count(profile::VU_MICRO_WRITES);

    word_addr &= vu->micro_mem_size;

    vu->micro_mem[word_addr] = data;

    invalidate_range(vu, word_addr << 3, 8);
}

uint32_t get_tpc(Vu* vu) {
    return vu->tpc;
}

void execute_lower(Vu* vu, uint32_t opcode) {
    decode_lower(vu, opcode);

    vu->lower.func(vu, &vu->lower);
}

void execute_upper(Vu* vu, uint32_t opcode) {
    decode_upper(vu, opcode);

    vu->upper.func(vu, &vu->upper);
}

void clear_block_cache(Vu* vu) {
    jit::flush_blocks(vu);

    vu->block_cache_size = 0;
    vu->block_cache.clear();
    vu->block_cache.resize(vu->micro_mem_size+1);

    vu->last_block_lookup_tpc = ~0u;
    vu->last_block_ptr = nullptr;
}

static inline bool invalidate_intersecting_block(Block& block, uint32_t start_word, uint32_t invalid_word_count, uint32_t word_mask) {
    if (!block.cycles) {
        return false;
    }

    for (int i = 0; i < block.cycles; i++) {
        const uint32_t block_word = (block.tpc + (uint32_t)i) & word_mask;
        const uint32_t rel = (block_word - start_word) & word_mask;

        if (rel < invalid_word_count) {
            block.cycles = 0;

            return true;
        }
    }

    return false;
}

void invalidate_range(Vu* vu, uint32_t addr, uint32_t size) {
    if (!size || vu->block_cache_size == 0) {
        return;
    }

    const uint32_t word_mask = (uint32_t)vu->micro_mem_size;
    const uint32_t word_count = word_mask + 1;
    const uint32_t byte_mask = (word_mask << 3) | 7;
    const uint32_t byte_count = word_count << 3;

    if (size >= byte_count) {
        for (Block& block : vu->block_cache) {
            if (!block.cycles) {
                continue;
            }

            block.cycles = 0;
        }

        vu->block_cache_size = 0;
        vu->last_block_lookup_tpc = ~0u;
        vu->last_block_ptr = nullptr;
        vu->region_epoch++;

        return;
    }

    const uint32_t start_byte = addr & byte_mask;
    const uint32_t start_word = start_byte >> 3;
    const uint32_t offset_in_word = start_byte & 7;
    const uint32_t invalid_word_count = (offset_in_word + size + 7) >> 3;

    int invalidated = 0;

    const uint32_t longest = vu->longest_block;
    const uint32_t window = longest ? invalid_word_count + longest - 1 : word_count;

    if (window < word_count) {
        const uint32_t first_slot = (start_word - (longest - 1)) & word_mask;

        for (uint32_t index = 0; index < window; index++) {
            Block& block = vu->block_cache[(first_slot + index) & word_mask];

            if (invalidate_intersecting_block(block, start_word, invalid_word_count, word_mask)) {
                invalidated++;
            }
        }
    } else {
        for (Block& block : vu->block_cache) {
            if (invalidate_intersecting_block(block, start_word, invalid_word_count, word_mask)) {
                invalidated++;
            }
        }
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
    vu->region_epoch++;
}

int is_interlocked(Vu* vu) {
    return vu->waiting_for_interlock;
}

// #undef printf

}
