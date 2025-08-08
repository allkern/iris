#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fenv.h>

#include "vu.h"
#include "vu_dis.h"

// #define printf(fmt, ...)(0)

#define VU_LD_DEST ((vu->lower >> 21) & 0xf)
#define VU_LD_DI(i) (vu->lower & (1 << (24 - i)))
#define VU_LD_DX ((vu->lower >> 24) & 1)
#define VU_LD_DY ((vu->lower >> 23) & 1)
#define VU_LD_DZ ((vu->lower >> 22) & 1)
#define VU_LD_DW ((vu->lower >> 21) & 1)
#define VU_LD_D ((vu->lower >> 6) & 0x1f)
#define VU_LD_S ((vu->lower >> 11) & 0x1f)
#define VU_LD_T ((vu->lower >> 16) & 0x1f)
#define VU_LD_SF ((vu->lower >> 21) & 3)
#define VU_LD_TF ((vu->lower >> 23) & 3)
#define VU_LD_IMM5 (((int32_t)(VU_LD_D << 27)) >> 27)
#define VU_LD_IMM11 (((int32_t)((vu->lower & 0x7ff) << 21)) >> 21)
#define VU_LD_IMM12 ((((vu->lower >> 21) & 0x1) << 11) | (vu->lower & 0x7ff))
#define VU_LD_IMM15 ((vu->lower & 0x7ff) | ((vu->lower & 0x1e00000) >> 10))
#define VU_LD_IMM24 (vu->lower & 0xffffff)
#define VU_ID vu->vi[VU_LD_D]
#define VU_IS vu->vi[VU_LD_S]
#define VU_IT vu->vi[VU_LD_T]
#define VU_UD_DEST ((vu->upper >> 21) & 0xf)
#define VU_UD_DI(i) (vu->upper & (1 << (24 - i)))
#define VU_UD_DX ((vu->upper >> 24) & 1)
#define VU_UD_DY ((vu->upper >> 23) & 1)
#define VU_UD_DZ ((vu->upper >> 22) & 1)
#define VU_UD_DW ((vu->upper >> 21) & 1)
#define VU_UD_D ((vu->upper >> 6) & 0x1f)
#define VU_UD_S ((vu->upper >> 11) & 0x1f)
#define VU_UD_T ((vu->upper >> 16) & 0x1f)

struct vu_state* vu_create(void) {
    return (struct vu_state*)malloc(sizeof(struct vu_state));
}

void vu_init(struct vu_state* vu, int id, struct ps2_gif* gif, struct ps2_vif* vif, struct vu_state* vu1) {
    memset(vu, 0, sizeof(struct vu_state));

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

    // VU uses round to zero by default
    fesetround(FE_TOWARDZERO);
}

void vu_destroy(struct vu_state* vu) {
    free(vu);
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

static inline void vu_mem_write(struct vu_state* vu, uint32_t addr, uint32_t data, int i) {
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
                printf("vu: oob write\n");

                exit(1);
            }
        }
    } else {
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

// Upper pipeline
void vu_i_abs(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, fabsf(vu_vf_i(vu, s, i)));
    }
}
void vu_i_add(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + vu_vf_i(vu, t, i);

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addi(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + vu->i.f;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addq(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + vu->q.f;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addx(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addy(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addz(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addw(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_adda(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + vu_vf_i(vu, t, i);

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addai(struct vu_state* vu) {
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + vu->i.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addaq(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + vu->q.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addax(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_adday(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addaz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_addaw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) + bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_sub(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - vu_vf_i(vu, t, i);

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subi(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - vu->i.f;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subq(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - vu->q.f;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subx(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_suby(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subz(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subw(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_suba(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - vu_vf_i(vu, t, i);

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subai(struct vu_state* vu) {
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - vu->i.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subaq(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - vu->q.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subax(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - bc;
            
            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subay(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - bc;
            
            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subaz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - bc;
            
            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_subaw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) - bc;
            
            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mul(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * vu_vf_i(vu, t, i);

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_muli(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * vu->i.f;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulq(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * vu->q.f;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulx(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_muly(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulz(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulw(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mula(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * vu_vf_i(vu, t, i);

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulai(struct vu_state* vu) {
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * vu->i.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulaq(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * vu->q.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulax(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulay(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulaz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_mulaw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_madd(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * vu_vf_i(vu, t, i);

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddi(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * vu->i.f;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddq(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * vu->q.f;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddx(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddy(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * bc;

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_madda(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * vu_vf_i(vu, t, i);

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddai(struct vu_state* vu) {
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * vu->i.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddaq(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu->acc.f[i] + vu_vf_i(vu, s, i) * vu->q.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddax(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_madday(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddaz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_maddaw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) + vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msub(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * vu_vf_i(vu, t, i);

            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubi(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * vu->i.f;
            
            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubq(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - (vu_vf_i(vu, s, i) * vu->q.f);
            
            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubx(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * bc;
            
            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msuby(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * bc;
            
            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * bc;
            
            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * bc;
            
            vu_set_vf(vu, d, i, vu_update_flags(vu, result, i));
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msuba(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * vu_vf_i(vu, t, i);

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubai(struct vu_state* vu) {
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * vu->i.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubaq(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu->acc.f[i] - vu_vf_i(vu, s, i) * vu->q.f;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubax(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubay(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubaz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_msubaw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float result = vu_acc_i(vu, i) - vu_vf_i(vu, s, i) * bc;

            vu->acc.f[i] = vu_update_flags(vu, result, i);
        } else {
            vu_clear_flags(vu, i);
        }
    }

    vu_update_status(vu);
}
void vu_i_max(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            vu->vf[d].u32[i] = vu_max(vu->vf[s].s32[i], vu->vf[t].s32[i]);
        }
    }
}
void vu_i_maxi(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;

    if (!d) return;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            vu->vf[d].u32[i] = vu_max(vu->vf[s].s32[i], vu->i.s32);
        }
    }
}
void vu_i_maxx(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    int32_t bc = vu->vf[t].s32[0];

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu->vf[d].u32[i] = vu_max(vu->vf[s].s32[i], bc);
        }
    }
}
void vu_i_maxy(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    int32_t bc = vu->vf[t].s32[1];

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu->vf[d].u32[i] = vu_max(vu->vf[s].s32[i], bc);
        }
    }
}
void vu_i_maxz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    int32_t bc = vu->vf[t].s32[2];

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu->vf[d].u32[i] = vu_max(vu->vf[s].s32[i], bc);
        }
    }
}
void vu_i_maxw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    int32_t bc = vu->vf[t].s32[3];

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu->vf[d].u32[i] = vu_max(vu->vf[s].s32[i], bc);
        }
    }
}
void vu_i_mini(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            vu->vf[d].u32[i] = vu_min(vu->vf[s].s32[i], vu->vf[t].s32[i]);
        }
    }
}
void vu_i_minii(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;

    if (!d) return;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            vu->vf[d].u32[i] = vu_min(vu->vf[s].s32[i], vu->i.s32);
        }
    }
}
void vu_i_minix(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    int32_t bc = vu->vf[t].s32[0];

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            vu->vf[d].u32[i] = vu_min(vu->vf[s].s32[i], bc);
        }
    }
}
void vu_i_miniy(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    int32_t bc = vu->vf[t].s32[1];

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            vu->vf[d].u32[i] = vu_min(vu->vf[s].s32[i], bc);
        }
    }
}
void vu_i_miniz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    int32_t bc = vu->vf[t].s32[2];

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            vu->vf[d].u32[i] = vu_min(vu->vf[s].s32[i], bc);
        }
    }
}
void vu_i_miniw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    if (!d) return;

    int32_t bc = vu->vf[t].s32[3];

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            vu->vf[d].u32[i] = vu_min(vu->vf[s].s32[i], bc);
        }
    }
}
void vu_i_opmula(struct vu_state* vu) {
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

    // printf("s(%d)=%08x %08x %08x t(%d)=%08x %08x %08x acc=%08x %08x %08x prev=%08x %08x %08x\n",
    //     s,
    //     vu->vf[s].u32[0],
    //     vu->vf[s].u32[1],
    //     vu->vf[s].u32[2],
    //     t,
    //     vu->vf[t].u32[0],
    //     vu->vf[t].u32[1],
    //     vu->vf[t].u32[2],
    //     vu->acc.u32[0],
    //     vu->acc.u32[1],
    //     vu->acc.u32[2],
    //     acc.u32[0],
    //     acc.u32[1],
    //     acc.u32[2]
    // );

    vu_clear_flags(vu, 3);
    vu_update_status(vu);
}
void vu_i_opmsub(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    struct vu_reg tmp;

    tmp.f[0] = vu->acc.x - vu_vf_y(vu, s) * vu_vf_z(vu, t);
    tmp.f[1] = vu->acc.y - vu_vf_z(vu, s) * vu_vf_x(vu, t);
    tmp.f[2] = vu->acc.z - vu_vf_x(vu, s) * vu_vf_y(vu, t);

    vu_set_vf_x(vu, d, vu_update_flags(vu, tmp.f[0], 0));
    vu_set_vf_y(vu, d, vu_update_flags(vu, tmp.f[1], 1));
    vu_set_vf_z(vu, d, vu_update_flags(vu, tmp.f[2], 2));

    // printf("s(%d)=%08x %08x %08x t(%d)=%08x %08x %08x d=%08x %08x %08x dp=%08x %08x %08x\n",
    //     s,
    //     vu->vf[s].u32[0],
    //     vu->vf[s].u32[1],
    //     vu->vf[s].u32[2],
    //     t,
    //     vu->vf[t].u32[0],
    //     vu->vf[t].u32[1],
    //     vu->vf[t].u32[2],
    //     vu->vf[d].u32[0],
    //     vu->vf[d].u32[1],
    //     vu->vf[d].u32[2],
    //     dv.u32[0],
    //     dv.u32[1],
    //     dv.u32[2]
    // );

    vu_clear_flags(vu, 3);
    vu_update_status(vu);
}
void vu_i_nop(struct vu_state* vu) {
    // No operation
}
void vu_i_ftoi0(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vfu(vu, t, i, vu_cvti(vu_vf_i(vu, s, i)));
    }
}
void vu_i_ftoi4(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vfu(vu, t, i, vu_cvti(vu_vf_i(vu, s, i) * (1.0f / 0.0625f)));
    }
}
void vu_i_ftoi12(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vfu(vu, t, i, vu_cvti(vu_vf_i(vu, s, i) * (1.0f / 0.000244140625f)));
    }
}
void vu_i_ftoi15(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vfu(vu, t, i, vu_cvti(vu_vf_i(vu, s, i) * (1.0f / 0.000030517578125f)));
    }
}
void vu_i_itof0(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, (float)vu->vf[s].s32[i]);
    }
}
void vu_i_itof4(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, (float)((float)(vu->vf[s].s32[i]) * 0.0625f));
    }
}
void vu_i_itof12(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, (float)((float)(vu->vf[s].s32[i]) * 0.000244140625f));
    }
}
void vu_i_itof15(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, (float)((float)(vu->vf[s].s32[i]) * 0.000030517578125f));
    }
}
void vu_i_clip(struct vu_state* vu) {
    int t = VU_UD_T;
    int s = VU_UD_S;

    vu->clip <<= 6;

    float w = fabsf(vu_vf_w(vu, t));
    float x = vu_vf_x(vu, s);
    float y = vu_vf_y(vu, s);
    float z = vu_vf_z(vu, s);

    vu->clip |= (x > +w);
    vu->clip |= (x < -w) << 1;
    vu->clip |= (y > +w) << 2;
    vu->clip |= (y < -w) << 3;
    vu->clip |= (z > +w) << 4;
    vu->clip |= (z < -w) << 5;
    vu->clip &= 0xFFFFFF;
}

// Lower pipeline
void vu_i_b(struct vu_state* vu) {
    vu->next_tpc = vu->tpc + VU_LD_IMM11;
}
void vu_i_bal(struct vu_state* vu) {
    // Instruction next to the delay slot
    VU_IT = vu->tpc + 1;

    vu->next_tpc = vu->tpc + VU_LD_IMM11;
}
void vu_i_div(struct vu_state* vu) {
    int t = VU_LD_T;
    int s = VU_LD_S;
    int tf = VU_LD_TF;
    int sf = VU_LD_SF;

    vu->q.f = vu_vf_i(vu, s, sf) / vu_vf_i(vu, t, tf);
    vu->q.f = vu_cvtf(vu->q.u32);
}
void vu_i_eatan(struct vu_state* vu) {
    float x = vu_vf_i(vu, VU_LD_S, VU_LD_SF);

    if (x == -1.0f) {
        vu->p.u32 = 0xFF7FFFFF;
    } else {
        x = (x - 1.0f) / (x + 1.0f);

        vu->p.f = vu_atan(x);
    }
}
void vu_i_eatanxy(struct vu_state* vu) {
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
void vu_i_eatanxz(struct vu_state* vu) {
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
void vu_i_eexp(struct vu_state* vu) {
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
void vu_i_eleng(struct vu_state* vu) {
    int s = VU_LD_S;

    float x2 = vu_vf_x(vu, s) * vu_vf_x(vu, s);
    float y2 = vu_vf_y(vu, s) * vu_vf_y(vu, s);
    float z2 = vu_vf_z(vu, s) * vu_vf_z(vu, s);

    vu->p.f = sqrtf(x2 + y2 + z2);
}
void vu_i_ercpr(struct vu_state* vu) {
    vu->p.f = 1.0f / vu_vf_i(vu, VU_LD_S, VU_LD_SF);
}
void vu_i_erleng(struct vu_state* vu) {
    int s = VU_LD_S;

    float x2 = vu_vf_x(vu, s) * vu_vf_x(vu, s);
    float y2 = vu_vf_y(vu, s) * vu_vf_y(vu, s);
    float z2 = vu_vf_z(vu, s) * vu_vf_z(vu, s);

    vu->p.f = 1.0f / sqrtf(x2 + y2 + z2);
}
void vu_i_ersadd(struct vu_state* vu) {
    int s = VU_LD_S;

    float x2 = vu_vf_x(vu, s) * vu_vf_x(vu, s);
    float y2 = vu_vf_y(vu, s) * vu_vf_y(vu, s);
    float z2 = vu_vf_z(vu, s) * vu_vf_z(vu, s);

    vu->p.f = 1.0f / (x2 + y2 + z2);
}
void vu_i_ersqrt(struct vu_state* vu) {
    vu->p.f = 1.0f / sqrtf(vu_vf_i(vu, VU_LD_S, VU_LD_SF));
}
void vu_i_esadd(struct vu_state* vu) {
    int s = VU_LD_S;

    float x2 = vu_vf_x(vu, s) * vu_vf_x(vu, s);
    float y2 = vu_vf_y(vu, s) * vu_vf_y(vu, s);
    float z2 = vu_vf_z(vu, s) * vu_vf_z(vu, s);

    vu->p.f = x2 + y2 + z2;
}
void vu_i_esin(struct vu_state* vu) {
    vu->p.f = sinf(vu_vf_i(vu, VU_LD_S, VU_LD_SF));
}
void vu_i_esqrt(struct vu_state* vu) {
    vu->p.f = sqrtf(vu_vf_i(vu, VU_LD_S, VU_LD_SF));
}
void vu_i_esum(struct vu_state* vu) {
    int s = VU_LD_S;

    vu->p.f = vu_vf_x(vu, s) + vu_vf_y(vu, s) + vu_vf_z(vu, s) + vu_vf_w(vu, s);
}
void vu_i_fcand(struct vu_state* vu) {
    vu->vi[1] = ((vu->clip & 0xffffff) & VU_LD_IMM24) != 0;
}
void vu_i_fceq(struct vu_state* vu) {
    vu->vi[1] = (vu->clip & 0xffffff) == VU_LD_IMM24;
}
void vu_i_fcget(struct vu_state* vu) {
    int t = VU_LD_T;

    if (!t) return;

    vu->vi[VU_LD_T] = vu->clip & 0xfff;
}
void vu_i_fcor(struct vu_state* vu) {
    vu->vi[1] = ((vu->clip & 0xffffff) | VU_LD_IMM24) == 0xffffff;
}
void vu_i_fcset(struct vu_state* vu) {
    vu->clip = VU_LD_IMM24;
}
void vu_i_fmand(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, vu->mac_pipeline[3] & VU_IS);
}
void vu_i_fmeq(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, (VU_IS & 0xffff) == (vu->mac_pipeline[3] & 0xffff));
}
void vu_i_fmor(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, (VU_IS & 0xffff) | (vu->mac_pipeline[3] & 0xffff));
}
void vu_i_fsand(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, vu->status & VU_LD_IMM12);
}
void vu_i_fseq(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, (vu->status & 0xfff) == VU_LD_IMM12);
}
void vu_i_fsor(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, (vu->status & 0xfff) | VU_LD_IMM12);
}
void vu_i_fsset(struct vu_state* vu) {
    vu->status &= 0x3f;
    vu->status |= VU_LD_IMM12 & 0xfc0;
}
void vu_i_iadd(struct vu_state* vu) {
    // printf("iadd vi%02u, vi%02u (%04x), vi%02u (%04x)\n", VU_LD_D, VU_LD_S, VU_IS, VU_LD_T, VU_IT);

    vu_set_vi(vu, VU_LD_D, VU_IS + VU_IT);
}
void vu_i_iaddi(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, VU_IS + VU_LD_IMM5);
}
void vu_i_iaddiu(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, VU_IS + VU_LD_IMM15);
}
void vu_i_iand(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_D, VU_IS & VU_IT);
}
void vu_i_ibeq(struct vu_state* vu) {
    if (VU_IT == VU_IS) {
        vu->next_tpc = vu->tpc + VU_LD_IMM11;
    }
}
void vu_i_ibgez(struct vu_state* vu) {
    if ((int16_t)VU_IS >= 0) {
        vu->next_tpc = vu->tpc + VU_LD_IMM11;
    }
}
void vu_i_ibgtz(struct vu_state* vu) {
    if ((int16_t)VU_IS > 0) {
        vu->next_tpc = vu->tpc + VU_LD_IMM11;
    }
}
void vu_i_iblez(struct vu_state* vu) {
    if ((int16_t)VU_IS <= 0) {
        vu->next_tpc = vu->tpc + VU_LD_IMM11;
    }
}
void vu_i_ibltz(struct vu_state* vu) {
    if ((int16_t)VU_IS < 0) {
        vu->next_tpc = vu->tpc + VU_LD_IMM11;
    }
}
void vu_i_ibne(struct vu_state* vu) {
    // printf("ibne vi%02u (%04x), vi%02u (%04x), 0x%08x\n", VU_LD_T, VU_IT, VU_LD_S, VU_IS, vu->tpc + VU_LD_IMM11);

    if (VU_IT != VU_IS) {
        vu->next_tpc = vu->tpc + VU_LD_IMM11;
    }
}
void vu_i_ilw(struct vu_state* vu) {
    int t = VU_LD_T;

    if (!t) return;

    uint32_t addr = VU_IS + VU_LD_IMM11;
    uint128_t data = vu_mem_read(vu, addr);

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vi[t] = data.u32[i];
    }
}
void vu_i_ilwr(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    if (!t) return;

    uint32_t addr = vu->vi[s];
    uint128_t data = vu_mem_read(vu, addr);

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vi[t] = data.u32[i];
    }
}
void vu_i_ior(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_D, VU_IS | VU_IT);
}
void vu_i_isub(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_D, VU_IS - VU_IT);
}
void vu_i_isubiu(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, VU_IS - VU_LD_IMM15);
}
void vu_i_isw(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s] + VU_LD_IMM11;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu_mem_write(vu, addr, vu->vi[t], i);
    }
}
void vu_i_iswr(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s];

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu_mem_write(vu, addr, vu->vi[t], i);
    }
}
void vu_i_jalr(struct vu_state* vu) {
    uint16_t s = VU_IS;

    VU_IT = vu->tpc + 1;

    vu->next_tpc = s;
}
void vu_i_jr(struct vu_state* vu) {
    vu->next_tpc = VU_IS;
}
void vu_i_lq(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s] + VU_LD_IMM11;
    uint128_t data = vu_mem_read(vu, addr);

    if (!t) return;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vf[t].u32[i] = data.u32[i];
    }
}
void vu_i_lqd(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    vu_set_vi(vu, s, vu->vi[s] - 1);

    uint32_t addr = vu->vi[s];
    uint128_t data = vu_mem_read(vu, addr);

    if (!t) return;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vf[t].u32[i] = data.u32[i];
    }
}
void vu_i_lqi(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s];
    uint128_t data = vu_mem_read(vu, addr);

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) if (t) vu->vf[t].u32[i] = data.u32[i];
    }

    // printf(" vf%02u, (vi%02u++) (%04x) (%f %f %f %f)\n",
    //     t,
    //     s,
    //     VU_IS,
    //     vu->vf[t].f[0],
    //     vu->vf[t].f[1],
    //     vu->vf[t].f[2],
    //     vu->vf[t].f[3]
    // );

    if (s) vu->vi[s]++;
}
void vu_i_mfir(struct vu_state* vu) {
    int t = VU_LD_T;

    if (!t) return;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vf[t].u32[i] = (int32_t)(*((int16_t*)&VU_IS));
    }
}
void vu_i_mfp(struct vu_state* vu) {
    int t = VU_LD_T;

    if (!t) return;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vf[t].u32[i] = vu->p.f;
    }
}
void vu_i_move(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    if (!t) return;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vf[t].u32[i] = vu->vf[s].u32[i];
    }
}
void vu_i_mr32(struct vu_state* vu) {
    int t = VU_LD_T;

    if (!t) return;

    int s = VU_LD_S;

    uint32_t x = vu->vf[s].u32[0];

    // for (int i = 0; i < 4; i++) {
    //     if (VU_LD_DI(i)) vu->vf[t].u32[i] = rs.u32[(i + 1) & 3];
    // }

    if (VU_LD_DI(0)) vu->vf[t].u32[0] = vu->vf[s].u32[1];
    if (VU_LD_DI(1)) vu->vf[t].u32[1] = vu->vf[s].u32[2];
    if (VU_LD_DI(2)) vu->vf[t].u32[2] = vu->vf[s].u32[3];
    if (VU_LD_DI(3)) vu->vf[t].u32[3] = x;
}
void vu_i_mtir(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, vu->vf[VU_LD_S].u32[VU_LD_SF] & 0xffff);
}
void vu_i_rget(struct vu_state* vu) {
    int t = VU_LD_T;

    if (!t) return;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vf[t].u32[i] = vu->r.u32;
    }
}
void vu_i_rinit(struct vu_state* vu) {
    int s = VU_LD_S;

    vu->r.u32 = 0x3f800000;

    if (!s) return;

    vu->r.u32 |= vu->vf[s].u32[VU_LD_SF] & 0x007fffff;
}
void vu_i_rnext(struct vu_state* vu) {
    int t = VU_LD_T;

    if (!t) return;

    int x = (vu->r.u32 >> 4) & 1;
    int y = (vu->r.u32 >> 22) & 1;

    vu->r.u32 <<= 1;
    vu->r.u32 ^= x ^ y;
    vu->r.u32 = (vu->r.u32 & 0x7FFFFF) | 0x3F800000;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vf[t].u32[i] = vu->r.u32;
    }
}
void vu_i_rsqrt(struct vu_state* vu) {
    vu->q.f = vu_vf_i(vu, VU_LD_S, VU_LD_SF) / sqrtf(vu_vf_i(vu, VU_LD_T, VU_LD_TF));
    vu->q.f = vu_cvtf(vu->q.u32);
}
void vu_i_rxor(struct vu_state* vu) {
    vu->r.u32 = 0x3F800000 | ((vu->r.u32 ^ vu->vf[VU_LD_S].u32[VU_LD_SF]) & 0x007FFFFF);
}
void vu_i_sq(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[t] + VU_LD_IMM11;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu_mem_write(vu, addr, vu->vf[s].u32[i], i);
    }
}
void vu_i_sqd(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    vu_set_vi(vu, t, vu->vi[t] - 1);

    uint32_t addr = vu->vi[t];

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu_mem_write(vu, addr, vu->vf[s].u32[i], i);
    }
}
void vu_i_sqi(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[t];

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu_mem_write(vu, addr, vu->vf[s].u32[i], i);
    }

    vu_set_vi(vu, t, vu->vi[t] + 1);
}
void vu_i_sqrt(struct vu_state* vu) {
    vu->q.f = sqrtf(vu_vf_i(vu, VU_LD_T, VU_LD_TF));
    vu->q.f = vu_cvtf(vu->q.u32);
}
void vu_i_waitp(struct vu_state* vu) {
    // No operation
}
void vu_i_waitq(struct vu_state* vu) {
    // No operation
    vu->q_delay = 0;
}

void vu_i_xgkick(struct vu_state* vu) {
    uint16_t addr = VU_IS;

    int eop = 1;

    do {
        uint128_t tag = vu_mem_read(vu, addr++);

        addr &= 0x7ff;

        // printf("tag: addr=%08x %08x %08x %08x %08x\n", addr - 1, tag.u32[3], tag.u32[2], tag.u32[1], tag.u32[0]);

        ps2_gif_write128(vu->gif, 0, tag);

        eop = (tag.u64[0] & 0x8000) != 0;

        int nloop = tag.u64[0] & 0x7fff;
        int flg = (tag.u64[0] >> 58) & 3;
        int nregs = (tag.u64[0] >> 60) & 0xf;

        if (!nloop)
            continue;

        // if (!nregs)
        //     nregs = 16;

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

        // printf("vu: nloop=%d nregs=%d eop=%d flg=%d qwc=%d\n",
        //     nloop,
        //     nregs,
        //     eop,
        //     flg,
        //     qwc
        // ); 

        for (int i = 0; i < qwc; i++) {
            // printf("vu: %08x: %08x %08x %08x %08x\n",
            //     addr,
            //     vu->vu_mem[addr].u32[3],
            //     vu->vu_mem[addr].u32[2],
            //     vu->vu_mem[addr].u32[1],
            //     vu->vu_mem[addr].u32[0]
            // );

            ps2_gif_write128(vu->gif, 0, vu_mem_read(vu, addr++));

            addr &= 0x7ff;
        }
    } while (!eop);
}
void vu_i_xitop(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_T, vu->vif->itop);
}
void vu_i_xtop(struct vu_state* vu) {
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
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint8_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint8_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void ps2_vu_write16(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint16_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint16_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void ps2_vu_write32(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint32_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint32_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void ps2_vu_write64(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint64_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint64_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}
void ps2_vu_write128(struct vu_state* vu, uint32_t addr, uint128_t data) {
    if (addr <= 0x3FFF) {
        uint8_t* ptr = (uint8_t*)vu->micro_mem;

        *(uint128_t*)(&ptr[addr & ((vu->micro_mem_size << 3) | 7)]) = data;
    } else {
        uint8_t* ptr = (uint8_t*)vu->vu_mem;

        *(uint128_t*)(&ptr[addr & ((vu->vu_mem_size << 4) | 0xf)]) = data;
    }
}

static inline void vu_execute_upper(struct vu_state* vu, uint32_t opcode) {
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
            case 0x00: vu_i_addax(vu); return;
            case 0x01: vu_i_adday(vu); return;
            case 0x02: vu_i_addaz(vu); return;
            case 0x03: vu_i_addaw(vu); return;
            case 0x04: vu_i_subax(vu); return;
            case 0x05: vu_i_subay(vu); return;
            case 0x06: vu_i_subaz(vu); return;
            case 0x07: vu_i_subaw(vu); return;
            case 0x08: vu_i_maddax(vu); return;
            case 0x09: vu_i_madday(vu); return;
            case 0x0A: vu_i_maddaz(vu); return;
            case 0x0B: vu_i_maddaw(vu); return;
            case 0x0C: vu_i_msubax(vu); return;
            case 0x0D: vu_i_msubay(vu); return;
            case 0x0E: vu_i_msubaz(vu); return;
            case 0x0F: vu_i_msubaw(vu); return;
            case 0x10: vu_i_itof0(vu); return;
            case 0x11: vu_i_itof4(vu); return;
            case 0x12: vu_i_itof12(vu); return;
            case 0x13: vu_i_itof15(vu); return;
            case 0x14: vu_i_ftoi0(vu); return;
            case 0x15: vu_i_ftoi4(vu); return;
            case 0x16: vu_i_ftoi12(vu); return;
            case 0x17: vu_i_ftoi15(vu); return;
            case 0x18: vu_i_mulax(vu); return;
            case 0x19: vu_i_mulay(vu); return;
            case 0x1A: vu_i_mulaz(vu); return;
            case 0x1B: vu_i_mulaw(vu); return;
            case 0x1C: vu_i_mulaq(vu); return;
            case 0x1D: vu_i_abs(vu); return;
            case 0x1E: vu_i_mulai(vu); return;
            case 0x1F: vu_i_clip(vu); return;
            case 0x20: vu_i_addaq(vu); return;
            case 0x21: vu_i_maddaq(vu); return;
            case 0x22: vu_i_addai(vu); return;
            case 0x23: vu_i_maddai(vu); return;
            case 0x24: vu_i_subaq(vu); return;
            case 0x25: vu_i_msubaq(vu); return;
            case 0x26: vu_i_subai(vu); return;
            case 0x27: vu_i_msubai(vu); return;
            case 0x28: vu_i_adda(vu); return;
            case 0x29: vu_i_madda(vu); return;
            case 0x2A: vu_i_mula(vu); return;
            case 0x2C: vu_i_suba(vu); return;
            case 0x2D: vu_i_msuba(vu); return;
            case 0x2E: vu_i_opmula(vu); return;
            case 0x2F: vu_i_nop(vu); return;
        }
    } else {
        // Decode 0000003F style instruction
        switch (opcode & 0x3f) {
            case 0x00: vu_i_addx(vu); return;
            case 0x01: vu_i_addy(vu); return;
            case 0x02: vu_i_addz(vu); return;
            case 0x03: vu_i_addw(vu); return;
            case 0x04: vu_i_subx(vu); return;
            case 0x05: vu_i_suby(vu); return;
            case 0x06: vu_i_subz(vu); return;
            case 0x07: vu_i_subw(vu); return;
            case 0x08: vu_i_maddx(vu); return;
            case 0x09: vu_i_maddy(vu); return;
            case 0x0A: vu_i_maddz(vu); return;
            case 0x0B: vu_i_maddw(vu); return;
            case 0x0C: vu_i_msubx(vu); return;
            case 0x0D: vu_i_msuby(vu); return;
            case 0x0E: vu_i_msubz(vu); return;
            case 0x0F: vu_i_msubw(vu); return;
            case 0x10: vu_i_maxx(vu); return;
            case 0x11: vu_i_maxy(vu); return;
            case 0x12: vu_i_maxz(vu); return;
            case 0x13: vu_i_maxw(vu); return;
            case 0x14: vu_i_minix(vu); return;
            case 0x15: vu_i_miniy(vu); return;
            case 0x16: vu_i_miniz(vu); return;
            case 0x17: vu_i_miniw(vu); return;
            case 0x18: vu_i_mulx(vu); return;
            case 0x19: vu_i_muly(vu); return;
            case 0x1A: vu_i_mulz(vu); return;
            case 0x1B: vu_i_mulw(vu); return;
            case 0x1C: vu_i_mulq(vu); return;
            case 0x1D: vu_i_maxi(vu); return;
            case 0x1E: vu_i_muli(vu); return;
            case 0x1F: vu_i_minii(vu); return;
            case 0x20: vu_i_addq(vu); return;
            case 0x21: vu_i_maddq(vu); return;
            case 0x22: vu_i_addi(vu); return;
            case 0x23: vu_i_maddi(vu); return;
            case 0x24: vu_i_subq(vu); return;
            case 0x25: vu_i_msubq(vu); return;
            case 0x26: vu_i_subi(vu); return;
            case 0x27: vu_i_msubi(vu); return;
            case 0x28: vu_i_add(vu); return;
            case 0x29: vu_i_madd(vu); return;
            case 0x2A: vu_i_mul(vu); return;
            case 0x2B: vu_i_max(vu); return;
            case 0x2C: vu_i_sub(vu); return;
            case 0x2D: vu_i_msub(vu); return;
            case 0x2E: vu_i_opmsub(vu); return;
            case 0x2F: vu_i_mini(vu); return;
        }
    }
}

static inline void vu_execute_lower(struct vu_state* vu, uint32_t opcode) {
    switch ((opcode & 0xFE000000) >> 25) {
        case 0x00: vu_i_lq(vu); return;
        case 0x01: vu_i_sq(vu); return;
        case 0x04: vu_i_ilw(vu); return;
        case 0x05: vu_i_isw(vu); return;
        case 0x08: vu_i_iaddiu(vu); return;
        case 0x09: vu_i_isubiu(vu); return;
        case 0x10: vu_i_fceq(vu); return;
        case 0x11: vu_i_fcset(vu); return;
        case 0x12: vu_i_fcand(vu); return;
        case 0x13: vu_i_fcor(vu); return;
        case 0x14: vu_i_fseq(vu); return;
        case 0x15: vu_i_fsset(vu); return;
        case 0x16: vu_i_fsand(vu); return;
        case 0x17: vu_i_fsor(vu); return;
        case 0x18: vu_i_fmeq(vu); return;
        case 0x1A: vu_i_fmand(vu); return;
        case 0x1B: vu_i_fmor(vu); return;
        case 0x1C: vu_i_fcget(vu); return;
        case 0x20: vu_i_b(vu); return;
        case 0x21: vu_i_bal(vu); return;
        case 0x24: vu_i_jr(vu); return;
        case 0x25: vu_i_jalr(vu); return;
        case 0x28: vu_i_ibeq(vu); return;
        case 0x29: vu_i_ibne(vu); return;
        case 0x2C: vu_i_ibltz(vu); return;
        case 0x2D: vu_i_ibgtz(vu); return;
        case 0x2E: vu_i_iblez(vu); return;
        case 0x2F: vu_i_ibgez(vu); return;
        case 0x40: {
            if ((opcode & 0x3C) == 0x3C) {
                switch (((opcode & 0x7C0) >> 4) | (opcode & 3)) {
                    case 0x30: vu_i_move(vu); return;
                    case 0x31: vu_i_mr32(vu); return;
                    case 0x34: vu_i_lqi(vu); return;
                    case 0x35: vu_i_sqi(vu); return;
                    case 0x36: vu_i_lqd(vu); return;
                    case 0x37: vu_i_sqd(vu); return;
                    case 0x38: vu_i_div(vu); return;
                    case 0x39: vu_i_sqrt(vu); return;
                    case 0x3A: vu_i_rsqrt(vu); return;
                    case 0x3B: vu_i_waitq(vu); return;
                    case 0x3C: vu_i_mtir(vu); return;
                    case 0x3D: vu_i_mfir(vu); return;
                    case 0x3E: vu_i_ilwr(vu); return;
                    case 0x3F: vu_i_iswr(vu); return;
                    case 0x40: vu_i_rnext(vu); return;
                    case 0x41: vu_i_rget(vu); return;
                    case 0x42: vu_i_rinit(vu); return;
                    case 0x43: vu_i_rxor(vu); return;
                    case 0x64: vu_i_mfp(vu); return;
                    case 0x68: vu_i_xtop(vu); return;
                    case 0x69: vu_i_xitop(vu); return;
                    case 0x6C: vu_i_xgkick(vu); return;
                    case 0x70: vu_i_esadd(vu); return;
                    case 0x71: vu_i_ersadd(vu); return;
                    case 0x72: vu_i_eleng(vu); return;
                    case 0x73: vu_i_erleng(vu); return;
                    case 0x74: vu_i_eatanxy(vu); return;
                    case 0x75: vu_i_eatanxz(vu); return;
                    case 0x76: vu_i_esum(vu); return;
                    case 0x78: vu_i_esqrt(vu); return;
                    case 0x79: vu_i_ersqrt(vu); return;
                    case 0x7A: vu_i_ercpr(vu); return;
                    case 0x7B: vu_i_waitp(vu); return;
                    case 0x7C: vu_i_esin(vu); return;
                    case 0x7D: vu_i_eatan(vu); return;
                    case 0x7E: vu_i_eexp(vu); return;
                }
            } else {
                switch (opcode & 0x3F) {
                    case 0x30: vu_i_iadd(vu); return;
                    case 0x31: vu_i_isub(vu); return;
                    case 0x32: vu_i_iaddi(vu); return;
                    case 0x34: vu_i_iand(vu); return;
                    case 0x35: vu_i_ior(vu); return;
                }
            }
        } break;
    }
}

void vu_execute_program(struct vu_state* vu, uint32_t addr) {
    // printf("vu%d: Executing program at %08x (%08x) TOP=%08x\n", vu->id, addr, addr << 3, vu->vif->top);
    // Disable VU1
    // if (vu->id == 1)
    //     return;

    struct vu_dis_state ds;

    ds.print_address = 0;
    ds.print_opcode = 0;

    vu->tpc = addr;
    vu->next_tpc = addr + 1;

    vu->upper = 0;
    vu->lower = 0;
    vu->i_bit = 0;
    vu->e_bit = 0;
    vu->m_bit = 0;
    vu->d_bit = 0;
    vu->t_bit = 0;

    int delayed_e_bit = 0;

    while (!delayed_e_bit) {
        uint32_t tpc = vu->tpc;
        uint64_t liw = vu->micro_mem[vu->tpc];

        vu->tpc = vu->next_tpc;
        vu->next_tpc = vu->tpc + 1;
        vu->tpc &= 0x7ff;
        vu->next_tpc &= 0x7ff;

        ds.addr = tpc;

        delayed_e_bit = vu->e_bit != 0;

        vu->upper = liw >> 32;
        vu->lower = liw & 0xffffffff;
        vu->i_bit = (vu->upper & 0x80000000) != 0;
        vu->e_bit = (vu->upper & 0x40000000) != 0;
        vu->m_bit = (vu->upper & 0x20000000) != 0;
        vu->d_bit = (vu->upper & 0x10000000) != 0;
        vu->t_bit = (vu->upper & 0x08000000) != 0;

        vu->q_delay--;

        vu_update_status(vu);

        // printf("%04x: %08x %08x %s", tpc, vu->upper, vu->lower, vu->e_bit ? "[e] " : " ");

        // vu->status = vu->mac_pipeline[3];

        vu_execute_upper(vu, vu->upper & 0x7ffffff);
        
        if (vu->i_bit) {
            // printf("loi %08x\n", vu->lower);

            // LOI
            vu->i.u32 = vu->lower;
        } else {
            // char ud[512], ld[512];

            // printf("%-40s%-40s\n", vu_disassemble_upper(ud, vu->upper, &ds), vu_disassemble_lower(ld, vu->lower, &ds));

            vu_execute_lower(vu, vu->lower);
        }

        vu->mac_pipeline[3] = vu->mac_pipeline[2];
        vu->mac_pipeline[2] = vu->mac_pipeline[1];
        vu->mac_pipeline[1] = vu->mac_pipeline[0];
        vu->mac_pipeline[0] = vu->mac;
        
        vu->clip_pipeline[3] = vu->clip_pipeline[2];
        vu->clip_pipeline[2] = vu->clip_pipeline[1];
        vu->clip_pipeline[1] = vu->clip_pipeline[0];
        vu->clip_pipeline[0] = vu->clip;

        // printf("mac_pipeline: %08x %08x %08x %08x mac=%08x\n",
        //     vu->mac_pipeline[0],
        //     vu->mac_pipeline[1],
        //     vu->mac_pipeline[2],
        //     vu->mac_pipeline[3],
        //     vu->mac
        // );
    }
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
    vu->next_tpc = 1;
    vu->upper = 0;
    vu->lower = 0;
    vu->i_bit = 0;
    vu->e_bit = 0;
    vu->m_bit = 0;
    vu->d_bit = 0;
    vu->t_bit = 0;

    vu->vf[0].w = 1.0;
}

// #undef printf