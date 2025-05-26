#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "vu.h"

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

void vu_init(struct vu_state* vu, int id, struct vu_state* vu1) {
    memset(vu, 0, sizeof(struct vu_state));

    vu->id = id;
    vu->vu1 = vu1;

    if (!id) {
        vu->micro_mem_size = 0x1ff;
        vu->vu_mem_size = 0xff;
    } else {
        vu->micro_mem_size = 0x7ff;
        vu->vu_mem_size = 0x3ff;
        vu->vu1 = vu1;
    }

    vu->vf[0].x = 0.0;
    vu->vf[0].y = 0.0;
    vu->vf[0].z = 0.0;
    vu->vf[0].w = 1.0;
}

void vu_destroy(struct vu_state* vu) {
    free(vu);
}

static inline void vu_update_status(struct vu_state* vu) {
    vu->status &= ~0x3f;

    vu->status |= (vu->mac & 0x000f) ? 1 : 0;
    vu->status |= (vu->mac & 0x00f0) ? 2 : 0;
    vu->status |= (vu->mac & 0x0f00) ? 4 : 0;
    vu->status |= (vu->mac & 0xf000) ? 8 : 0;

    vu->status |= (vu->status & 0x3F) << 6;
}

static inline float vu_update_flags(struct vu_state* vu, float value, int index) {
    uint32_t value_u = *(uint32_t*)&value;

    int flag_id = 3 - index;

    //Sign flag
    if (value_u & 0x80000000)
        vu->mac |= 0x10 << flag_id;
    else
        vu->mac &= ~(0x10 << flag_id);

    //Zero flag, clear under/overflow
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
    if (r) vu->vf[r].i32[f] = v;
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
void vu_i_addi(struct vu_state* vu) { printf("vu: addi unimplemented\n"); exit(1); }
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
void vu_i_addai(struct vu_state* vu) { printf("vu: addai unimplemented\n"); exit(1); }
void vu_i_addaq(struct vu_state* vu) { printf("vu: addaq unimplemented\n"); exit(1); }
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
void vu_i_subi(struct vu_state* vu) { printf("vu: subi unimplemented\n"); exit(1); }
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
void vu_i_suba(struct vu_state* vu) { printf("vu: suba unimplemented\n"); exit(1); }
void vu_i_subai(struct vu_state* vu) { printf("vu: subai unimplemented\n"); exit(1); }
void vu_i_subaq(struct vu_state* vu) { printf("vu: subaq unimplemented\n"); exit(1); }
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
void vu_i_muli(struct vu_state* vu) { printf("vu: muli unimplemented\n"); exit(1); }
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
void vu_i_mulai(struct vu_state* vu) { printf("vu: mulai unimplemented\n"); exit(1); }
void vu_i_mulaq(struct vu_state* vu) { printf("vu: mulaq unimplemented\n"); exit(1); }
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
void vu_i_madd(struct vu_state* vu) { printf("vu: madd unimplemented\n"); exit(1); }
void vu_i_maddi(struct vu_state* vu) { printf("vu: maddi unimplemented\n"); exit(1); }
void vu_i_maddq(struct vu_state* vu) { printf("vu: maddq unimplemented\n"); exit(1); }
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
void vu_i_madda(struct vu_state* vu) { printf("vu: madda unimplemented\n"); exit(1); }
void vu_i_maddai(struct vu_state* vu) { printf("vu: maddai unimplemented\n"); exit(1); }
void vu_i_maddaq(struct vu_state* vu) { printf("vu: maddaq unimplemented\n"); exit(1); }
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
void vu_i_msub(struct vu_state* vu) { printf("vu: msub unimplemented\n"); exit(1); }
void vu_i_msubi(struct vu_state* vu) { printf("vu: msubi unimplemented\n"); exit(1); }
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
void vu_i_msuba(struct vu_state* vu) { printf("vu: msuba unimplemented\n"); exit(1); }
void vu_i_msubai(struct vu_state* vu) { printf("vu: msubai unimplemented\n"); exit(1); }
void vu_i_msubaq(struct vu_state* vu) { printf("vu: msubaq unimplemented\n"); exit(1); }
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

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);
            float ft = vu_vf_i(vu, t, i);

            vu_set_vf(vu, d, i, (fs > ft) ? fs : ft);
        }
    }
}
void vu_i_maxi(struct vu_state* vu) { printf("vu: maxi unimplemented\n"); exit(1); }
void vu_i_maxx(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu_set_vf(vu, d, i, (fs > bc) ? fs : bc);
        }
    }
}
void vu_i_maxy(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu_set_vf(vu, d, i, (fs > bc) ? fs : bc);
        }
    }
}
void vu_i_maxz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu_set_vf(vu, d, i, (fs > bc) ? fs : bc);
        }
    }
}
void vu_i_maxw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu_set_vf(vu, d, i, (fs > bc) ? fs : bc);
        }
    }
}
void vu_i_mini(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);
            float ft = vu_vf_i(vu, t, i);

            vu_set_vf(vu, d, i, (fs < ft) ? fs : ft);
        }
    }
}
void vu_i_minii(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);
            float ft = vu_vf_i(vu, t, i);

            vu_set_vf(vu, d, i, (fs < ft) ? fs : ft);
        }
    }
}
void vu_i_minix(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu_set_vf(vu, d, i, (fs < bc) ? fs : bc);
        }
    }
}
void vu_i_miniy(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu_set_vf(vu, d, i, (fs < bc) ? fs : bc);
        }
    }
}
void vu_i_miniz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu_set_vf(vu, d, i, (fs < bc) ? fs : bc);
        }
    }
}
void vu_i_miniw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) {
            float fs = vu_vf_i(vu, s, i);

            vu_set_vf(vu, d, i, (fs < bc) ? fs : bc);
        }
    }
}
void vu_i_opmula(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

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
void vu_i_opmsub(struct vu_state* vu) {
    int d = VU_LD_D;
    int s = VU_LD_S;
    int t = VU_LD_T;

    struct vu_reg tmp;

    tmp.f[0] = vu->acc.x - vu_vf_y(vu, s) * vu_vf_z(vu, t);
    tmp.f[1] = vu->acc.y - vu_vf_z(vu, s) * vu_vf_x(vu, t);
    tmp.f[2] = vu->acc.z - vu_vf_x(vu, s) * vu_vf_y(vu, t);

    vu_set_vf_x(vu, d, vu_update_flags(vu, tmp.f[0], 0));
    vu_set_vf_y(vu, d, vu_update_flags(vu, tmp.f[1], 1));
    vu_set_vf_z(vu, d, vu_update_flags(vu, tmp.f[2], 2));

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
void vu_i_ftoi12(struct vu_state* vu) { printf("vu: ftoi12 unimplemented\n"); exit(1); }
void vu_i_ftoi15(struct vu_state* vu) { printf("vu: ftoi15 unimplemented\n"); exit(1); }
void vu_i_itof0(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, (float)vu->vf[s].i32[i]);
    }
}
void vu_i_itof4(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, (float)((float)(vu->vf[s].i32[i]) * 0.0625f));
    }
}
void vu_i_itof12(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, (float)((float)(vu->vf[s].i32[i]) * 0.000244140625f));
    }
}
void vu_i_itof15(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, (float)((float)(vu->vf[s].i32[i]) * 0.000030517578125f));
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
void vu_i_b(struct vu_state* vu) { printf("vu: b unimplemented\n"); exit(1); }
void vu_i_bal(struct vu_state* vu) { printf("vu: bal unimplemented\n"); exit(1); }
void vu_i_div(struct vu_state* vu) {
    int t = VU_LD_T;
    int s = VU_LD_S;
    int tf = VU_LD_TF;
    int sf = VU_LD_SF;

    vu->q.f = vu_vf_i(vu, s, sf) / vu_vf_i(vu, t, tf);
    vu->q.f = vu_cvtf(vu->q.u32);
}
void vu_i_eatan(struct vu_state* vu) { printf("vu: eatan unimplemented\n"); exit(1); }
void vu_i_eatanxy(struct vu_state* vu) { printf("vu: eatanxy unimplemented\n"); exit(1); }
void vu_i_eatanxz(struct vu_state* vu) { printf("vu: eatanxz unimplemented\n"); exit(1); }
void vu_i_eexp(struct vu_state* vu) { printf("vu: eexp unimplemented\n"); exit(1); }
void vu_i_eleng(struct vu_state* vu) { printf("vu: eleng unimplemented\n"); exit(1); }
void vu_i_ercpr(struct vu_state* vu) { printf("vu: ercpr unimplemented\n"); exit(1); }
void vu_i_erleng(struct vu_state* vu) { printf("vu: erleng unimplemented\n"); exit(1); }
void vu_i_ersadd(struct vu_state* vu) { printf("vu: ersadd unimplemented\n"); exit(1); }
void vu_i_ersqrt(struct vu_state* vu) { printf("vu: ersqrt unimplemented\n"); exit(1); }
void vu_i_esadd(struct vu_state* vu) { printf("vu: esadd unimplemented\n"); exit(1); }
void vu_i_esin(struct vu_state* vu) { printf("vu: esin unimplemented\n"); exit(1); }
void vu_i_esqrt(struct vu_state* vu) { printf("vu: esqrt unimplemented\n"); exit(1); }
void vu_i_esum(struct vu_state* vu) { printf("vu: esum unimplemented\n"); exit(1); }
void vu_i_fcand(struct vu_state* vu) { printf("vu: fcand unimplemented\n"); exit(1); }
void vu_i_fceq(struct vu_state* vu) { printf("vu: fceq unimplemented\n"); exit(1); }
void vu_i_fcget(struct vu_state* vu) { printf("vu: fcget unimplemented\n"); exit(1); }
void vu_i_fcor(struct vu_state* vu) { printf("vu: fcor unimplemented\n"); exit(1); }
void vu_i_fcset(struct vu_state* vu) { printf("vu: fcset unimplemented\n"); exit(1); }
void vu_i_fmand(struct vu_state* vu) { printf("vu: fmand unimplemented\n"); exit(1); }
void vu_i_fmeq(struct vu_state* vu) { printf("vu: fmeq unimplemented\n"); exit(1); }
void vu_i_fmor(struct vu_state* vu) { printf("vu: fmor unimplemented\n"); exit(1); }
void vu_i_fsand(struct vu_state* vu) { printf("vu: fsand unimplemented\n"); exit(1); }
void vu_i_fseq(struct vu_state* vu) { printf("vu: fseq unimplemented\n"); exit(1); }
void vu_i_fsor(struct vu_state* vu) { printf("vu: fsor unimplemented\n"); exit(1); }
void vu_i_fsset(struct vu_state* vu) { printf("vu: fsset unimplemented\n"); exit(1); }
void vu_i_iadd(struct vu_state* vu) {
    vu_set_vi(vu, VU_LD_D, VU_IS + VU_IT);
}
void vu_i_iaddi(struct vu_state* vu) { printf("vu: iaddi unimplemented\n"); exit(1); }
void vu_i_iaddiu(struct vu_state* vu) { printf("vu: iaddiu unimplemented\n"); exit(1); }
void vu_i_iand(struct vu_state* vu) { printf("vu: iand unimplemented\n"); exit(1); }
void vu_i_ibeq(struct vu_state* vu) { printf("vu: ibeq unimplemented\n"); exit(1); }
void vu_i_ibgez(struct vu_state* vu) { printf("vu: ibgez unimplemented\n"); exit(1); }
void vu_i_ibgtz(struct vu_state* vu) { printf("vu: ibgtz unimplemented\n"); exit(1); }
void vu_i_iblez(struct vu_state* vu) { printf("vu: iblez unimplemented\n"); exit(1); }
void vu_i_ibltz(struct vu_state* vu) { printf("vu: ibltz unimplemented\n"); exit(1); }
void vu_i_ibne(struct vu_state* vu) { printf("vu: ibne unimplemented\n"); exit(1); }
void vu_i_ilw(struct vu_state* vu) { printf("vu: ilw unimplemented\n"); exit(1); }
void vu_i_ilwr(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s] & vu->vu_mem_size;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vi[t] = vu->vu_mem[addr].u32[i];
    }
}
void vu_i_ior(struct vu_state* vu) { printf("vu: ior unimplemented\n"); exit(1); }
void vu_i_isub(struct vu_state* vu) { printf("vu: isub unimplemented\n"); exit(1); }
void vu_i_isubiu(struct vu_state* vu) { printf("vu: isubiu unimplemented\n"); exit(1); }
void vu_i_isw(struct vu_state* vu) { printf("vu: isw unimplemented\n"); exit(1); }
void vu_i_iswr(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[s] & vu->vu_mem_size;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vu_mem[addr].u32[i] = vu->vi[t];
    }
}
void vu_i_jalr(struct vu_state* vu) { printf("vu: jalr unimplemented\n"); exit(1); }
void vu_i_jr(struct vu_state* vu) { printf("vu: jr unimplemented\n"); exit(1); }
void vu_i_lq(struct vu_state* vu) { printf("vu: lq unimplemented\n"); exit(1); }
void vu_i_lqd(struct vu_state* vu) { printf("vu: lqd unimplemented\n"); exit(1); }
void vu_i_lqi(struct vu_state* vu) { printf("vu: lqi unimplemented\n"); exit(1); }
void vu_i_mfir(struct vu_state* vu) { printf("vu: mfir unimplemented\n"); exit(1); }
void vu_i_mfp(struct vu_state* vu) { printf("vu: mfp unimplemented\n"); exit(1); }
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
void vu_i_mtir(struct vu_state* vu) { printf("vu: mtir unimplemented\n"); exit(1); }
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
void vu_i_sq(struct vu_state* vu) { printf("vu: sq unimplemented\n"); exit(1); }
void vu_i_sqd(struct vu_state* vu) { printf("vu: sqd unimplemented\n"); exit(1); }
void vu_i_sqi(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    uint32_t addr = vu->vi[t] & vu->vu_mem_size;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vu_mem[addr].u32[i] = vu_vf_i(vu, s, i);
    }

    vu_set_vi(vu, t, vu->vi[t] + 1);
}
void vu_i_sqrt(struct vu_state* vu) {
    vu->q.f = sqrtf(vu_vf_i(vu, VU_LD_T, VU_LD_TF));
    vu->q.f = vu_cvtf(vu->q.u32);
}
void vu_i_waitp(struct vu_state* vu) { printf("vu: waitp unimplemented\n"); exit(1); }
void vu_i_waitq(struct vu_state* vu) {
    // No operation
}
void vu_i_xgkick(struct vu_state* vu) { printf("vu: xgkick unimplemented\n"); exit(1); }
void vu_i_xitop(struct vu_state* vu) { printf("vu: xitop unimplemented\n"); exit(1); }
void vu_i_xtop(struct vu_state* vu) { printf("vu: xtop unimplemented\n"); exit(1); }

uint64_t ps2_vu_read8(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF)
        return ((uint8_t*)vu->micro_mem)[addr & vu->micro_mem_size];

    return ((uint8_t*)vu->vu_mem)[addr & vu->vu_mem_size];
}
uint64_t ps2_vu_read16(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF)
        return ((uint16_t*)vu->micro_mem)[addr & vu->micro_mem_size];

    return ((uint16_t*)vu->vu_mem)[addr & vu->vu_mem_size];
}
uint64_t ps2_vu_read32(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF)
        return ((uint32_t*)vu->micro_mem)[addr & vu->micro_mem_size];

    return ((uint32_t*)vu->vu_mem)[addr & vu->vu_mem_size];
}
uint64_t ps2_vu_read64(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF)
        return vu->micro_mem[addr & vu->micro_mem_size];

    return ((uint64_t*)vu->vu_mem)[addr & vu->vu_mem_size];
}
uint128_t ps2_vu_read128(struct vu_state* vu, uint32_t addr) {
    if (addr <= 0x3FFF)
        return ((uint128_t*)vu->micro_mem)[addr & vu->micro_mem_size];

    return vu->vu_mem[addr & vu->vu_mem_size];
}
void ps2_vu_write8(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        ((uint8_t*)vu->micro_mem)[addr & vu->micro_mem_size] = data;
    } else {
        ((uint8_t*)vu->vu_mem)[addr & vu->vu_mem_size] = data;
    }
}
void ps2_vu_write16(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        ((uint16_t*)vu->micro_mem)[addr & vu->micro_mem_size] = data;
    } else {
        ((uint16_t*)vu->vu_mem)[addr & vu->vu_mem_size] = data;
    }
}
void ps2_vu_write32(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        ((uint32_t*)vu->micro_mem)[addr & vu->micro_mem_size] = data;
    } else {
        ((uint32_t*)vu->vu_mem)[addr & vu->vu_mem_size] = data;
    }
}
void ps2_vu_write64(struct vu_state* vu, uint32_t addr, uint64_t data) {
    if (addr <= 0x3FFF) {
        vu->micro_mem[addr & vu->micro_mem_size] = data;
    } else {
        ((uint64_t*)vu->vu_mem)[addr & vu->vu_mem_size] = data;
    }
}
void ps2_vu_write128(struct vu_state* vu, uint32_t addr, uint128_t data) {
    if (addr <= 0x3FFF) {
        ((uint128_t*)vu->micro_mem)[addr & vu->micro_mem_size] = data;
    } else {
        vu->vu_mem[addr & vu->vu_mem_size] = data;
    }
}