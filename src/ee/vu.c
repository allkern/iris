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
        vu->micro_mem_size = 0x3ff;
        vu->vu_mem_size = 0x1ff;
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

// Upper pipeline
void vu_i_abs(struct vu_state* vu) { printf("vu: abs unimplemented\n"); exit(1); }
void vu_i_add(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) + vu_vf_i(vu, t, i));
    }
}
void vu_i_addi(struct vu_state* vu) { printf("vu: addi unimplemented\n"); exit(1); }
void vu_i_addq(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) + vu->q.f);
    }
}
void vu_i_addx(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) + bc);
    }
}
void vu_i_addy(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) + bc);
    }
}
void vu_i_addz(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) + bc);
    }
}
void vu_i_addw(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) + bc);
    }
}
void vu_i_adda(struct vu_state* vu) { printf("vu: adda unimplemented\n"); exit(1); }
void vu_i_addai(struct vu_state* vu) { printf("vu: addai unimplemented\n"); exit(1); }
void vu_i_addaq(struct vu_state* vu) { printf("vu: addaq unimplemented\n"); exit(1); }
void vu_i_addax(struct vu_state* vu) { printf("vu: addax unimplemented\n"); exit(1); }
void vu_i_adday(struct vu_state* vu) { printf("vu: adday unimplemented\n"); exit(1); }
void vu_i_addaz(struct vu_state* vu) { printf("vu: addaz unimplemented\n"); exit(1); }
void vu_i_addaw(struct vu_state* vu) { printf("vu: addaw unimplemented\n"); exit(1); }
void vu_i_sub(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) - vu_vf_i(vu, t, i));
    }
}
void vu_i_subi(struct vu_state* vu) { printf("vu: subi unimplemented\n"); exit(1); }
void vu_i_subq(struct vu_state* vu) { printf("vu: subq unimplemented\n"); exit(1); }
void vu_i_subx(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) - bc);
    }
}
void vu_i_suby(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) - bc);
    }
}
void vu_i_subz(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) - bc);
    }
}
void vu_i_subw(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) - bc);
    }
}
void vu_i_suba(struct vu_state* vu) { printf("vu: suba unimplemented\n"); exit(1); }
void vu_i_subai(struct vu_state* vu) { printf("vu: subai unimplemented\n"); exit(1); }
void vu_i_subaq(struct vu_state* vu) { printf("vu: subaq unimplemented\n"); exit(1); }
void vu_i_subax(struct vu_state* vu) { printf("vu: subax unimplemented\n"); exit(1); }
void vu_i_subay(struct vu_state* vu) { printf("vu: subay unimplemented\n"); exit(1); }
void vu_i_subaz(struct vu_state* vu) { printf("vu: subaz unimplemented\n"); exit(1); }
void vu_i_subaw(struct vu_state* vu) { printf("vu: subaw unimplemented\n"); exit(1); }
void vu_i_mul(struct vu_state* vu) {
    int s = VU_UD_S;
    int d = VU_UD_D;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) * vu_vf_i(vu, t, i));
    }
}
void vu_i_muli(struct vu_state* vu) { printf("vu: muli unimplemented\n"); exit(1); }
void vu_i_mulq(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) * vu->q.f);
    }
}
void vu_i_mulx(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) * bc);
    }
}
void vu_i_muly(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) * bc);
    }
}
void vu_i_mulz(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) * bc);
    }
}
void vu_i_mulw(struct vu_state* vu) {
    int d = VU_UD_D;
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu_vf_i(vu, s, i) * bc);
    }
}
void vu_i_mula(struct vu_state* vu) { printf("vu: mula unimplemented\n"); exit(1); }
void vu_i_mulai(struct vu_state* vu) { printf("vu: mulai unimplemented\n"); exit(1); }
void vu_i_mulaq(struct vu_state* vu) { printf("vu: mulaq unimplemented\n"); exit(1); }
void vu_i_mulax(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu->acc.f[i] = vu_vf_i(vu, s, i) * bc;
    }
}
void vu_i_mulay(struct vu_state* vu) { printf("vu: mulay unimplemented\n"); exit(1); }
void vu_i_mulaz(struct vu_state* vu) { printf("vu: mulaz unimplemented\n"); exit(1); }
void vu_i_mulaw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu->acc.f[i] = vu_vf_i(vu, s, i) * bc;
    }
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
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu->acc.f[i] + vu_vf_i(vu, s, i) * bc * 0.5);
    }
}
void vu_i_maddy(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu->acc.f[i] + vu_vf_i(vu, s, i) * bc);
    }
}
void vu_i_maddz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu->acc.f[i] + vu_vf_i(vu, s, i) * bc);
    }
}
void vu_i_maddw(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;
    int d = VU_UD_D;

    float bc = vu_vf_w(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, d, i, vu->acc.f[i] + vu_vf_i(vu, s, i) * bc);
    }
}
void vu_i_madda(struct vu_state* vu) { printf("vu: madda unimplemented\n"); exit(1); }
void vu_i_maddai(struct vu_state* vu) { printf("vu: maddai unimplemented\n"); exit(1); }
void vu_i_maddaq(struct vu_state* vu) { printf("vu: maddaq unimplemented\n"); exit(1); }
void vu_i_maddax(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_x(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu->acc.f[i] += vu_vf_i(vu, s, i) * bc;
    }
}
void vu_i_madday(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_y(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu->acc.f[i] += vu_vf_i(vu, s, i) * bc;
    }
}
void vu_i_maddaz(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    float bc = vu_vf_z(vu, t);

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu->acc.f[i] += vu_vf_i(vu, s, i) * bc;
    }
}
void vu_i_maddaw(struct vu_state* vu) { printf("vu: maddaw unimplemented\n"); exit(1); }
void vu_i_msub(struct vu_state* vu) { printf("vu: msub unimplemented\n"); exit(1); }
void vu_i_msubi(struct vu_state* vu) { printf("vu: msubi unimplemented\n"); exit(1); }
void vu_i_msubq(struct vu_state* vu) { printf("vu: msubq unimplemented\n"); exit(1); }
void vu_i_msubx(struct vu_state* vu) { printf("vu: msubx unimplemented\n"); exit(1); }
void vu_i_msuby(struct vu_state* vu) { printf("vu: msuby unimplemented\n"); exit(1); }
void vu_i_msubz(struct vu_state* vu) { printf("vu: msubz unimplemented\n"); exit(1); }
void vu_i_msubw(struct vu_state* vu) { printf("vu: msubw unimplemented\n"); exit(1); }
void vu_i_msuba(struct vu_state* vu) { printf("vu: msuba unimplemented\n"); exit(1); }
void vu_i_msubai(struct vu_state* vu) { printf("vu: msubai unimplemented\n"); exit(1); }
void vu_i_msubaq(struct vu_state* vu) { printf("vu: msubaq unimplemented\n"); exit(1); }
void vu_i_msubax(struct vu_state* vu) { printf("vu: msubax unimplemented\n"); exit(1); }
void vu_i_msubay(struct vu_state* vu) { printf("vu: msubay unimplemented\n"); exit(1); }
void vu_i_msubaz(struct vu_state* vu) { printf("vu: msubaz unimplemented\n"); exit(1); }
void vu_i_msubaw(struct vu_state* vu) { printf("vu: msubaw unimplemented\n"); exit(1); }
void vu_i_max(struct vu_state* vu) { printf("vu: max unimplemented\n"); exit(1); }
void vu_i_maxi(struct vu_state* vu) { printf("vu: maxi unimplemented\n"); exit(1); }
void vu_i_maxx(struct vu_state* vu) { printf("vu: maxx unimplemented\n"); exit(1); }
void vu_i_maxy(struct vu_state* vu) { printf("vu: maxy unimplemented\n"); exit(1); }
void vu_i_maxz(struct vu_state* vu) { printf("vu: maxz unimplemented\n"); exit(1); }
void vu_i_maxw(struct vu_state* vu) { printf("vu: maxw unimplemented\n"); exit(1); }
void vu_i_mini(struct vu_state* vu) { printf("vu: mini unimplemented\n"); exit(1); }
void vu_i_minii(struct vu_state* vu) { printf("vu: minii unimplemented\n"); exit(1); }
void vu_i_minix(struct vu_state* vu) { printf("vu: minix unimplemented\n"); exit(1); }
void vu_i_miniy(struct vu_state* vu) { printf("vu: miniy unimplemented\n"); exit(1); }
void vu_i_miniz(struct vu_state* vu) { printf("vu: miniz unimplemented\n"); exit(1); }
void vu_i_miniw(struct vu_state* vu) { printf("vu: miniw unimplemented\n"); exit(1); }
void vu_i_opmula(struct vu_state* vu) {
    int s = VU_LD_S;
    int t = VU_LD_T;

    vu->acc.x = vu_vf_y(vu, s) * vu_vf_z(vu, t);
    vu->acc.y = vu_vf_z(vu, s) * vu_vf_x(vu, t);
    vu->acc.z = vu_vf_x(vu, s) * vu_vf_y(vu, t);
}
void vu_i_opmsub(struct vu_state* vu) {
    int d = VU_LD_D;
    int s = VU_LD_S;
    int t = VU_LD_T;

    vu_set_vf_x(vu, d, vu->acc.x - vu_vf_y(vu, s) * vu_vf_z(vu, t));
    vu_set_vf_y(vu, d, vu->acc.y - vu_vf_z(vu, s) * vu_vf_x(vu, t));
    vu_set_vf_z(vu, d, vu->acc.z - vu_vf_x(vu, s) * vu_vf_y(vu, t));
}
void vu_i_nop(struct vu_state* vu) {
    // No operation
}
void vu_i_ftoi0(struct vu_state* vu) { printf("vu: ftoi0 unimplemented\n"); exit(1); }
void vu_i_ftoi4(struct vu_state* vu) {
    int s = VU_UD_S;
    int t = VU_UD_T;

    for (int i = 0; i < 4; i++) {
        if (VU_UD_DI(i)) vu_set_vf(vu, t, i, vu_cvti(vu_vf_i(vu, s, i) * (1.0f / 0.0625f)));
    }
}
void vu_i_ftoi12(struct vu_state* vu) { printf("vu: ftoi12 unimplemented\n"); exit(1); }
void vu_i_ftoi15(struct vu_state* vu) { printf("vu: ftoi15 unimplemented\n"); exit(1); }
void vu_i_itof0(struct vu_state* vu) { printf("vu: itof0 unimplemented\n"); exit(1); }
void vu_i_itof4(struct vu_state* vu) { printf("vu: itof4 unimplemented\n"); exit(1); }
void vu_i_itof12(struct vu_state* vu) { printf("vu: itof12 unimplemented\n"); exit(1); }
void vu_i_itof15(struct vu_state* vu) { printf("vu: itof15 unimplemented\n"); exit(1); }
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
void vu_i_ilwr(struct vu_state* vu) { printf("vu: ilwr unimplemented\n"); exit(1); }
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
    struct vu_reg rs = vu->vf[VU_LD_S];
    int t = VU_LD_T;

    if (!t) return;

    for (int i = 0; i < 4; i++) {
        if (VU_LD_DI(i)) vu->vf[t].u32[i] = rs.u32[(i + 1) & 3];
    }
}
void vu_i_mtir(struct vu_state* vu) { printf("vu: mtir unimplemented\n"); exit(1); }
void vu_i_rget(struct vu_state* vu) { printf("vu: rget unimplemented\n"); exit(1); }
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
}
void vu_i_rxor(struct vu_state* vu) { printf("vu: rxor unimplemented\n"); exit(1); }
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
void vu_i_sqrt(struct vu_state* vu) { printf("vu: sqrt unimplemented\n"); exit(1); }
void vu_i_waitp(struct vu_state* vu) { printf("vu: waitp unimplemented\n"); exit(1); }
void vu_i_waitq(struct vu_state* vu) {
    // No operation
}
void vu_i_xgkick(struct vu_state* vu) { printf("vu: xgkick unimplemented\n"); exit(1); }
void vu_i_xitop(struct vu_state* vu) { printf("vu: xitop unimplemented\n"); exit(1); }
void vu_i_xtop(struct vu_state* vu) { printf("vu: xtop unimplemented\n"); exit(1); }