#include <stdio.h>

#include "vu_dis.h"

#define VU_LD_DEST ((o >> 21) & 0xf)
#define VU_LD_DI(i) (o & (1 << (24 - i)))
#define VU_LD_DX ((o >> 24) & 1)
#define VU_LD_DY ((o >> 23) & 1)
#define VU_LD_DZ ((o >> 22) & 1)
#define VU_LD_DW ((o >> 21) & 1)
#define VU_LD_D ((o >> 6) & 0x1f)
#define VU_LD_S ((o >> 11) & 0x1f)
#define VU_LD_T ((o >> 16) & 0x1f)
#define VU_LD_SF ((o >> 21) & 3)
#define VU_LD_TF ((o >> 23) & 3)
#define VU_LD_IMM5 (((int32_t)(VU_LD_D << 27)) >> 27)
#define VU_LD_IMM11 (((int32_t)((o & 0x7ff) << 21)) >> 21)
#define VU_LD_IMM12 (o & 0x7ff)
#define VU_LD_IMM15 ((o & 0x7ff) | ((o & 0x1e00000) >> 10))
#define VU_LD_IMM24 (o & 0xffffff)
#define VU_UD_DEST ((o >> 21) & 0xf)
#define VU_UD_DI(i) (o & (1 << (24 - i)))
#define VU_UD_DX ((o >> 24) & 1)
#define VU_UD_DY ((o >> 23) & 1)
#define VU_UD_DZ ((o >> 22) & 1)
#define VU_UD_DW ((o >> 21) & 1)
#define VU_UD_D ((o >> 6) & 0x1f)
#define VU_UD_S ((o >> 11) & 0x1f)
#define VU_UD_T ((o >> 16) & 0x1f)

// Print broadcast fields
static inline char* vu_d_bc(struct vu_dis_state* s, char* p, uint32_t bc) {
    if (!bc)
        return p;

    *p++ = '.';

    for (int i = 0; i < 4; i++) {
        if (bc & (1 << (3 - i))) *p++ = "xyzw"[i];
    }

    *p = '\0';

    return p;
}

static inline char* vu_d_mnemonic(struct vu_dis_state* s, char* p, const char* m, uint32_t bc) {
    char buf[32];
    char* ptr = buf;

    ptr += sprintf(ptr, "%s", m);
    ptr = vu_d_bc(s, ptr, bc);

    p += sprintf(p, "%-12s", buf);

    return p;
}

static inline char* vu_d_mnemonic_nd(struct vu_dis_state* s, char* p, const char* m) {
    p += sprintf(p, "%-12s", m);

    return p;
}

static inline char* vu_d_addax(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addax", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_adday(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "adday", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_addaz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addaz", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_addaw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addaw", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subax(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subax", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subay(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subay", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subaz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subaz", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subaw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subaw", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddax(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddax", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_madday(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "madday", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddaz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddaz", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddaw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddaw", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubax(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubax", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubay(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubay", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubaz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubaz", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubaw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubaw", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_itof0(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "itof0", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_itof4(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "itof4", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_itof12(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "itof12", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_itof15(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "itof15", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_ftoi0(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "ftoi0", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_ftoi4(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "ftoi4", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_ftoi12(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "ftoi12", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_ftoi15(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "ftoi15", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_mulax(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulax", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mulay(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulay", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mulaz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulaz", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mulaw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulaw", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mulaq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulaq", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, q", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_abs(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "abs", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_T, VU_UD_S); return p;
}
static inline char* vu_d_mulai(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulai", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, i", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_clip(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "clipw", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_addaq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addaq", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, q", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddaq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddaq", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, q", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_addai(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addai", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, i", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddai(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddai", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, i", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subaq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subaq", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, q", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubaq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubaq", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, q", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subai(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subai", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, i", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubai(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubai", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, i", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_adda(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "adda", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_madda(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "madda", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mula(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mula", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_suba(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "suba", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msuba(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msuba", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_opmula(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "opmula", VU_UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_nop(struct vu_dis_state* s, char* p, uint32_t o) {
    p += sprintf(p, "%s", "nop"); return p;
}
static inline char* vu_d_addx(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addx", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_addy(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addy", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_addz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addz", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_addw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addw", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subx(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subx", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_suby(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "suby", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subz", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_subw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subw", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddx(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddx", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddy(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddy", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddz", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maddw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddw", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubx(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubx", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msuby(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msuby", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubz", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msubw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubw", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maxx(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maxx", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maxy(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maxy", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maxz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maxz", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_maxw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maxw", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_minix(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "minix", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_miniy(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "miniy", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_miniz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "miniz", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_miniw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "miniw", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mulx(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulx", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_muly(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "muly", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mulz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulz", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mulw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulw", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mulq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mulq", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_maxi(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maxi", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_muli(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "muli", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_minii(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "minii", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_addq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addq", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_maddq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddq", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_addi(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "addi", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_maddi(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "maddi", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_subq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subq", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_msubq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubq", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_subi(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "subi", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_msubi(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msubi", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", VU_UD_D, VU_UD_S); return p;
}
static inline char* vu_d_add(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "add", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_madd(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "madd", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mul(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mul", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_max(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "max", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_sub(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "sub", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_msub(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "msub", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_opmsub(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "opmsub", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}
static inline char* vu_d_mini(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mini", VU_UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", VU_UD_D, VU_UD_S, VU_UD_T); return p;
}

static inline char* vu_d_lq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "lq", VU_LD_DEST); p += sprintf(p, "vf%02u, %d(vi%02u)", VU_LD_T, VU_LD_IMM11, VU_LD_S); return p;
}
static inline char* vu_d_sq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "sq", VU_LD_DEST); p += sprintf(p, "vf%02u, %d(vi%02u)", VU_LD_S, VU_LD_IMM11, VU_LD_T); return p;
}
static inline char* vu_d_ilw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "ilw", VU_LD_DEST); p += sprintf(p, "vi%02u, %d(vi%02u)", VU_LD_T, VU_LD_IMM11, VU_LD_S); return p;
}
static inline char* vu_d_isw(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "isw", VU_LD_DEST); p += sprintf(p, "vi%02u, %d(vi%02u)", VU_LD_T, VU_LD_IMM11, VU_LD_S); return p;
}
static inline char* vu_d_iaddiu(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "iaddiu"); p += sprintf(p, "vi%02u, vi%02u, %d", VU_LD_T, VU_LD_S, VU_LD_IMM15); return p;
}
static inline char* vu_d_isubiu(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "isubiu"); p += sprintf(p, "vi%02u, vi%02u, %d", VU_LD_T, VU_LD_S, VU_LD_IMM15); return p;
}
static inline char* vu_d_fceq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fceq"); p += sprintf(p, "vi01, 0x%06x", VU_LD_IMM24); return p;
}
static inline char* vu_d_fcset(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fcset"); p += sprintf(p, "0x%06x", VU_LD_IMM24); return p;
}
static inline char* vu_d_fcand(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fcand"); p += sprintf(p, "vi01, 0x%06x", VU_LD_IMM24); return p;
}
static inline char* vu_d_fcor(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fcor"); p += sprintf(p, "vi01, 0x%06x", VU_LD_IMM24); return p;
}
static inline char* vu_d_fseq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fseq"); p += sprintf(p, "0x%04x", VU_LD_IMM12); return p;
}
static inline char* vu_d_fsset(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fsset"); p += sprintf(p, "0x%04x", VU_LD_IMM12); return p;
}
static inline char* vu_d_fsand(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fsand"); p += sprintf(p, "0x%04x", VU_LD_IMM12); return p;
}
static inline char* vu_d_fsor(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fsor"); p += sprintf(p, "0x%04x", VU_LD_IMM12); return p;
}
static inline char* vu_d_fmeq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fmeq"); p += sprintf(p, "vi%02u, vi%02u", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_fmand(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fmand"); p += sprintf(p, "vi%02u, vi%02u", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_fmor(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fmor"); p += sprintf(p, "vi%02u, vi%02u", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_fcget(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "fcget"); p += sprintf(p, "vi%02u", VU_LD_T); return p;
}
static inline char* vu_d_b(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "b"); p += sprintf(p, "%04x", s ? (s->addr + 1 + VU_LD_IMM11) : VU_LD_IMM11); return p;
}
static inline char* vu_d_bal(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "bal"); p += sprintf(p, "vi%02u, 0x%04x", VU_LD_T, s ? (s->addr + 1 + VU_LD_IMM11) : VU_LD_IMM11); return p;
}
static inline char* vu_d_jr(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "jr"); p += sprintf(p, "vi%02u", VU_LD_S); return p;
}
static inline char* vu_d_jalr(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "jalr"); p += sprintf(p, "vi%02u, vi%02u", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_ibeq(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ibeq"); p += sprintf(p, "vi%02u, vi%02u, 0x%04x", VU_LD_T, VU_LD_S, s ? (s->addr + 1 + VU_LD_IMM11) : VU_LD_IMM11); return p;
}
static inline char* vu_d_ibne(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ibne"); p += sprintf(p, "vi%02u, vi%02u, 0x%04x", VU_LD_T, VU_LD_S, s ? (s->addr + 1 + VU_LD_IMM11) : VU_LD_IMM11); return p;
}
static inline char* vu_d_ibltz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ibltz"); p += sprintf(p, "vi%02u, 0x%04x", VU_LD_S, s ? (s->addr + 1 + VU_LD_IMM11) : VU_LD_IMM11); return p;
}
static inline char* vu_d_ibgtz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ibgtz"); p += sprintf(p, "vi%02u, 0x%04x", VU_LD_S, s ? (s->addr + 1 + VU_LD_IMM11) : VU_LD_IMM11); return p;
}
static inline char* vu_d_iblez(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "iblez"); p += sprintf(p, "vi%02u, 0x%04x", VU_LD_S, s ? (s->addr + 1 + VU_LD_IMM11) : VU_LD_IMM11); return p;
}
static inline char* vu_d_ibgez(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ibgez"); p += sprintf(p, "vi%02u, 0x%04x", VU_LD_S, s ? (s->addr + 1 + VU_LD_IMM11) : VU_LD_IMM11); return p;
}
static inline char* vu_d_move(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "move", VU_LD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_mr32(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mr32", VU_LD_DEST); p += sprintf(p, "vf%02u, vf%02u", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_lqi(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "lqi", VU_LD_DEST); p += sprintf(p, "vf%02u, (vi%02u++)", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_sqi(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "sqi", VU_LD_DEST); p += sprintf(p, "vf%02u, (vi%02u++)", VU_LD_S, VU_LD_T); return p;
}
static inline char* vu_d_lqd(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "lqd", VU_LD_DEST); p += sprintf(p, "vf%02u, (--vi%02u)", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_sqd(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "sqd", VU_LD_DEST); p += sprintf(p, "vf%02u, (--vi%02u)", VU_LD_S, VU_LD_T); return p;
}
static inline char* vu_d_div(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "div"); p += sprintf(p, "q, vf%02u%c, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF], VU_LD_T, "xyzw"[VU_LD_TF]); return p;
}
static inline char* vu_d_sqrt(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "sqrt"); p += sprintf(p, "q, vf%02u%c", VU_LD_T, "xyzw"[VU_LD_TF]); return p;
}
static inline char* vu_d_rsqrt(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "rsqrt"); p += sprintf(p, "q, vf%02u%c, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF], VU_LD_T, "xyzw"[VU_LD_TF]); return p;
}
static inline char* vu_d_waitq(struct vu_dis_state* s, char* p, uint32_t o) {
    p += sprintf(p, "waitq"); return p;
}
static inline char* vu_d_mtir(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "mtir"); p += sprintf(p, "vi%02u, vf%02u%c", VU_LD_T, VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_mfir(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mfir", VU_LD_DEST); p += sprintf(p, "vf%02u, vi%02u", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_ilwr(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "ilwr", VU_LD_DEST); p += sprintf(p, "vi%02u, (vi%02u)", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_iswr(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "iswr", VU_LD_DEST); p += sprintf(p, "vi%02u, (vi%02u)", VU_LD_T, VU_LD_S); return p;
}
static inline char* vu_d_rnext(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "rnext", VU_LD_DEST); p += sprintf(p, "vf%02u, r", VU_LD_T); return p;
}
static inline char* vu_d_rget(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "rget", VU_LD_DEST); p += sprintf(p, "vf%02u, r", VU_LD_T); return p;
}
static inline char* vu_d_rinit(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "rinit"); p += sprintf(p, "r, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_rxor(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "rxor"); p += sprintf(p, "r, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_mfp(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic(s, p, "mfp", VU_LD_DEST); p += sprintf(p, "vf%02u, p", VU_LD_T); return p;
}
static inline char* vu_d_xtop(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "xtop"); p += sprintf(p, "vi%02u", VU_LD_T); return p;
}
static inline char* vu_d_xitop(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "xitop"); p += sprintf(p, "vi%02u", VU_LD_T); return p;
}
static inline char* vu_d_xgkick(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "xgkick"); p += sprintf(p, "vi%02u", VU_LD_S); return p;
}
static inline char* vu_d_esadd(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "esadd"); p += sprintf(p, "p, vf%02u", VU_LD_S); return p;
}
static inline char* vu_d_ersadd(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ersadd"); p += sprintf(p, "p, vf%02u", VU_LD_S); return p;
}
static inline char* vu_d_eleng(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "eleng"); p += sprintf(p, "p, vf%02u", VU_LD_S); return p;
}
static inline char* vu_d_erleng(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "erleng"); p += sprintf(p, "p, vf%02u", VU_LD_S); return p;
}
static inline char* vu_d_eatanxy(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "eatan.xy"); p += sprintf(p, "p, vf%02u", VU_LD_S); return p;
}
static inline char* vu_d_eatanxz(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "eatan.xz"); p += sprintf(p, "p, vf%02u", VU_LD_S); return p;
}
static inline char* vu_d_esum(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "esum"); p += sprintf(p, "p, vf%02u", VU_LD_S); return p;
}
static inline char* vu_d_esqrt(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "esqrt"); p += sprintf(p, "p, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_ersqrt(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ersqrt"); p += sprintf(p, "p, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_ercpr(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ercpr"); p += sprintf(p, "p, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_waitp(struct vu_dis_state* s, char* p, uint32_t o) {
    p += sprintf(p, "waitp"); return p;
}
static inline char* vu_d_esin(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "esin"); p += sprintf(p, "p, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_eatan(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "eatan"); p += sprintf(p, "p, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_eexp(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "eexp"); p += sprintf(p, "p, vf%02u%c", VU_LD_S, "xyzw"[VU_LD_SF]); return p;
}
static inline char* vu_d_iadd(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "iadd"); p += sprintf(p, "vi%02u, vi%02u, vi%02u", VU_LD_D, VU_LD_S, VU_LD_T); return p;
}
static inline char* vu_d_isub(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "isub"); p += sprintf(p, "vi%02u, vi%02u, vi%02u", VU_LD_D, VU_LD_S, VU_LD_T); return p;
}
static inline char* vu_d_iaddi(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "iaddi"); p += sprintf(p, "vi%02u, vi%02u, %d", VU_LD_D, VU_LD_S, VU_LD_IMM5); return p;
}
static inline char* vu_d_iand(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "iand"); p += sprintf(p, "vi%02u, vi%02u, vi%02u", VU_LD_D, VU_LD_S, VU_LD_T); return p;
}
static inline char* vu_d_ior(struct vu_dis_state* s, char* p, uint32_t o) {
    p = vu_d_mnemonic_nd(s, p, "ior"); p += sprintf(p, "vi%02u, vi%02u, vi%02u", VU_LD_D, VU_LD_S, VU_LD_T); return p;
}
static inline char* vu_d_invalid(struct vu_dis_state* s, char* p, uint32_t o) {
    p += sprintf(p, "<invalid>"); return p;
}

char* vu_disassemble_upper(char* buf, uint64_t opcode, struct vu_dis_state* s) {
    char* ptr = buf;

    ptr = buf;

    if (s) if (s->print_address)
        ptr += sprintf(ptr, "%08x: ", s->addr);

    if (s) if (s->print_opcode)
        ptr += sprintf(ptr, "%08x ", opcode);

    // Decode 000007FF style instruction
    if ((opcode & 0x3c) == 0x3c) {
        switch (((opcode & 0x3c0) >> 4) | (opcode & 3)) {
            case 0x00: vu_d_addax(s, ptr, opcode); return buf;
            case 0x01: vu_d_adday(s, ptr, opcode); return buf;
            case 0x02: vu_d_addaz(s, ptr, opcode); return buf;
            case 0x03: vu_d_addaw(s, ptr, opcode); return buf;
            case 0x04: vu_d_subax(s, ptr, opcode); return buf;
            case 0x05: vu_d_subay(s, ptr, opcode); return buf;
            case 0x06: vu_d_subaz(s, ptr, opcode); return buf;
            case 0x07: vu_d_subaw(s, ptr, opcode); return buf;
            case 0x08: vu_d_maddax(s, ptr, opcode); return buf;
            case 0x09: vu_d_madday(s, ptr, opcode); return buf;
            case 0x0A: vu_d_maddaz(s, ptr, opcode); return buf;
            case 0x0B: vu_d_maddaw(s, ptr, opcode); return buf;
            case 0x0C: vu_d_msubax(s, ptr, opcode); return buf;
            case 0x0D: vu_d_msubay(s, ptr, opcode); return buf;
            case 0x0E: vu_d_msubaz(s, ptr, opcode); return buf;
            case 0x0F: vu_d_msubaw(s, ptr, opcode); return buf;
            case 0x10: vu_d_itof0(s, ptr, opcode); return buf;
            case 0x11: vu_d_itof4(s, ptr, opcode); return buf;
            case 0x12: vu_d_itof12(s, ptr, opcode); return buf;
            case 0x13: vu_d_itof15(s, ptr, opcode); return buf;
            case 0x14: vu_d_ftoi0(s, ptr, opcode); return buf;
            case 0x15: vu_d_ftoi4(s, ptr, opcode); return buf;
            case 0x16: vu_d_ftoi12(s, ptr, opcode); return buf;
            case 0x17: vu_d_ftoi15(s, ptr, opcode); return buf;
            case 0x18: vu_d_mulax(s, ptr, opcode); return buf;
            case 0x19: vu_d_mulay(s, ptr, opcode); return buf;
            case 0x1A: vu_d_mulaz(s, ptr, opcode); return buf;
            case 0x1B: vu_d_mulaw(s, ptr, opcode); return buf;
            case 0x1C: vu_d_mulaq(s, ptr, opcode); return buf;
            case 0x1D: vu_d_abs(s, ptr, opcode); return buf;
            case 0x1E: vu_d_mulai(s, ptr, opcode); return buf;
            case 0x1F: vu_d_clip(s, ptr, opcode); return buf;
            case 0x20: vu_d_addaq(s, ptr, opcode); return buf;
            case 0x21: vu_d_maddaq(s, ptr, opcode); return buf;
            case 0x22: vu_d_addai(s, ptr, opcode); return buf;
            case 0x23: vu_d_maddai(s, ptr, opcode); return buf;
            case 0x24: vu_d_subaq(s, ptr, opcode); return buf;
            case 0x25: vu_d_msubaq(s, ptr, opcode); return buf;
            case 0x26: vu_d_subai(s, ptr, opcode); return buf;
            case 0x27: vu_d_msubai(s, ptr, opcode); return buf;
            case 0x28: vu_d_adda(s, ptr, opcode); return buf;
            case 0x29: vu_d_madda(s, ptr, opcode); return buf;
            case 0x2A: vu_d_mula(s, ptr, opcode); return buf;
            case 0x2C: vu_d_suba(s, ptr, opcode); return buf;
            case 0x2D: vu_d_msuba(s, ptr, opcode); return buf;
            case 0x2E: vu_d_opmula(s, ptr, opcode); return buf;
            case 0x2F: vu_d_nop(s, ptr, opcode); return buf;
        }
    } else {
        // Decode 0000003F style instruction
        switch (opcode & 0x3f) {
            case 0x00: vu_d_addx(s, ptr, opcode); return buf;
            case 0x01: vu_d_addy(s, ptr, opcode); return buf;
            case 0x02: vu_d_addz(s, ptr, opcode); return buf;
            case 0x03: vu_d_addw(s, ptr, opcode); return buf;
            case 0x04: vu_d_subx(s, ptr, opcode); return buf;
            case 0x05: vu_d_suby(s, ptr, opcode); return buf;
            case 0x06: vu_d_subz(s, ptr, opcode); return buf;
            case 0x07: vu_d_subw(s, ptr, opcode); return buf;
            case 0x08: vu_d_maddx(s, ptr, opcode); return buf;
            case 0x09: vu_d_maddy(s, ptr, opcode); return buf;
            case 0x0A: vu_d_maddz(s, ptr, opcode); return buf;
            case 0x0B: vu_d_maddw(s, ptr, opcode); return buf;
            case 0x0C: vu_d_msubx(s, ptr, opcode); return buf;
            case 0x0D: vu_d_msuby(s, ptr, opcode); return buf;
            case 0x0E: vu_d_msubz(s, ptr, opcode); return buf;
            case 0x0F: vu_d_msubw(s, ptr, opcode); return buf;
            case 0x10: vu_d_maxx(s, ptr, opcode); return buf;
            case 0x11: vu_d_maxy(s, ptr, opcode); return buf;
            case 0x12: vu_d_maxz(s, ptr, opcode); return buf;
            case 0x13: vu_d_maxw(s, ptr, opcode); return buf;
            case 0x14: vu_d_minix(s, ptr, opcode); return buf;
            case 0x15: vu_d_miniy(s, ptr, opcode); return buf;
            case 0x16: vu_d_miniz(s, ptr, opcode); return buf;
            case 0x17: vu_d_miniw(s, ptr, opcode); return buf;
            case 0x18: vu_d_mulx(s, ptr, opcode); return buf;
            case 0x19: vu_d_muly(s, ptr, opcode); return buf;
            case 0x1A: vu_d_mulz(s, ptr, opcode); return buf;
            case 0x1B: vu_d_mulw(s, ptr, opcode); return buf;
            case 0x1C: vu_d_mulq(s, ptr, opcode); return buf;
            case 0x1D: vu_d_maxi(s, ptr, opcode); return buf;
            case 0x1E: vu_d_muli(s, ptr, opcode); return buf;
            case 0x1F: vu_d_minii(s, ptr, opcode); return buf;
            case 0x20: vu_d_addq(s, ptr, opcode); return buf;
            case 0x21: vu_d_maddq(s, ptr, opcode); return buf;
            case 0x22: vu_d_addi(s, ptr, opcode); return buf;
            case 0x23: vu_d_maddi(s, ptr, opcode); return buf;
            case 0x24: vu_d_subq(s, ptr, opcode); return buf;
            case 0x25: vu_d_msubq(s, ptr, opcode); return buf;
            case 0x26: vu_d_subi(s, ptr, opcode); return buf;
            case 0x27: vu_d_msubi(s, ptr, opcode); return buf;
            case 0x28: vu_d_add(s, ptr, opcode); return buf;
            case 0x29: vu_d_madd(s, ptr, opcode); return buf;
            case 0x2A: vu_d_mul(s, ptr, opcode); return buf;
            case 0x2B: vu_d_max(s, ptr, opcode); return buf;
            case 0x2C: vu_d_sub(s, ptr, opcode); return buf;
            case 0x2D: vu_d_msub(s, ptr, opcode); return buf;
            case 0x2E: vu_d_opmsub(s, ptr, opcode); return buf;
            case 0x2F: vu_d_mini(s, ptr, opcode); return buf;
        }
    }

    vu_d_invalid(s, ptr, opcode);

    return buf;
}

char* vu_disassemble_lower(char* buf, uint64_t opcode, struct vu_dis_state* s) {
    char* ptr = buf;

    ptr = buf;

    if (s) if (s->print_address)
        ptr += sprintf(ptr, "%08x: ", s->addr);

    if (s) if (s->print_opcode)
        ptr += sprintf(ptr, "%08x ", opcode);

    switch ((opcode & 0xFE000000) >> 25) {
        case 0x00: vu_d_lq(s, ptr, opcode); return buf;
        case 0x01: vu_d_sq(s, ptr, opcode); return buf;
        case 0x04: vu_d_ilw(s, ptr, opcode); return buf;
        case 0x05: vu_d_isw(s, ptr, opcode); return buf;
        case 0x08: vu_d_iaddiu(s, ptr, opcode); return buf;
        case 0x09: vu_d_isubiu(s, ptr, opcode); return buf;
        case 0x10: vu_d_fceq(s, ptr, opcode); return buf;
        case 0x11: vu_d_fcset(s, ptr, opcode); return buf;
        case 0x12: vu_d_fcand(s, ptr, opcode); return buf;
        case 0x13: vu_d_fcor(s, ptr, opcode); return buf;
        case 0x14: vu_d_fseq(s, ptr, opcode); return buf;
        case 0x15: vu_d_fsset(s, ptr, opcode); return buf;
        case 0x16: vu_d_fsand(s, ptr, opcode); return buf;
        case 0x17: vu_d_fsor(s, ptr, opcode); return buf;
        case 0x18: vu_d_fmeq(s, ptr, opcode); return buf;
        case 0x1A: vu_d_fmand(s, ptr, opcode); return buf;
        case 0x1B: vu_d_fmor(s, ptr, opcode); return buf;
        case 0x1C: vu_d_fcget(s, ptr, opcode); return buf;
        case 0x20: vu_d_b(s, ptr, opcode); return buf;
        case 0x21: vu_d_bal(s, ptr, opcode); return buf;
        case 0x24: vu_d_jr(s, ptr, opcode); return buf;
        case 0x25: vu_d_jalr(s, ptr, opcode); return buf;
        case 0x28: vu_d_ibeq(s, ptr, opcode); return buf;
        case 0x29: vu_d_ibne(s, ptr, opcode); return buf;
        case 0x2C: vu_d_ibltz(s, ptr, opcode); return buf;
        case 0x2D: vu_d_ibgtz(s, ptr, opcode); return buf;
        case 0x2E: vu_d_iblez(s, ptr, opcode); return buf;
        case 0x2F: vu_d_ibgez(s, ptr, opcode); return buf;
        case 0x40: {
            if ((opcode & 0x3C) == 0x3C) {
                switch (((opcode & 0x7C0) >> 4) | (opcode & 3)) {
                    case 0x30: vu_d_move(s, ptr, opcode); return buf;
                    case 0x31: vu_d_mr32(s, ptr, opcode); return buf;
                    case 0x34: vu_d_lqi(s, ptr, opcode); return buf;
                    case 0x35: vu_d_sqi(s, ptr, opcode); return buf;
                    case 0x36: vu_d_lqd(s, ptr, opcode); return buf;
                    case 0x37: vu_d_sqd(s, ptr, opcode); return buf;
                    case 0x38: vu_d_div(s, ptr, opcode); return buf;
                    case 0x39: vu_d_sqrt(s, ptr, opcode); return buf;
                    case 0x3A: vu_d_rsqrt(s, ptr, opcode); return buf;
                    case 0x3B: vu_d_waitq(s, ptr, opcode); return buf;
                    case 0x3C: vu_d_mtir(s, ptr, opcode); return buf;
                    case 0x3D: vu_d_mfir(s, ptr, opcode); return buf;
                    case 0x3E: vu_d_ilwr(s, ptr, opcode); return buf;
                    case 0x3F: vu_d_iswr(s, ptr, opcode); return buf;
                    case 0x40: vu_d_rnext(s, ptr, opcode); return buf;
                    case 0x41: vu_d_rget(s, ptr, opcode); return buf;
                    case 0x42: vu_d_rinit(s, ptr, opcode); return buf;
                    case 0x43: vu_d_rxor(s, ptr, opcode); return buf;
                    case 0x64: vu_d_mfp(s, ptr, opcode); return buf;
                    case 0x68: vu_d_xtop(s, ptr, opcode); return buf;
                    case 0x69: vu_d_xitop(s, ptr, opcode); return buf;
                    case 0x6C: vu_d_xgkick(s, ptr, opcode); return buf;
                    case 0x70: vu_d_esadd(s, ptr, opcode); return buf;
                    case 0x71: vu_d_ersadd(s, ptr, opcode); return buf;
                    case 0x72: vu_d_eleng(s, ptr, opcode); return buf;
                    case 0x73: vu_d_erleng(s, ptr, opcode); return buf;
                    case 0x74: vu_d_eatanxy(s, ptr, opcode); return buf;
                    case 0x75: vu_d_eatanxz(s, ptr, opcode); return buf;
                    case 0x76: vu_d_esum(s, ptr, opcode); return buf;
                    case 0x78: vu_d_esqrt(s, ptr, opcode); return buf;
                    case 0x79: vu_d_ersqrt(s, ptr, opcode); return buf;
                    case 0x7A: vu_d_ercpr(s, ptr, opcode); return buf;
                    case 0x7B: vu_d_waitp(s, ptr, opcode); return buf;
                    case 0x7C: vu_d_esin(s, ptr, opcode); return buf;
                    case 0x7D: vu_d_eatan(s, ptr, opcode); return buf;
                    case 0x7E: vu_d_eexp(s, ptr, opcode); return buf;
                }
            } else {
                switch (opcode & 0x3F) {
                    case 0x30: vu_d_iadd(s, ptr, opcode); return buf;
                    case 0x31: vu_d_isub(s, ptr, opcode); return buf;
                    case 0x32: vu_d_iaddi(s, ptr, opcode); return buf;
                    case 0x34: vu_d_iand(s, ptr, opcode); return buf;
                    case 0x35: vu_d_ior(s, ptr, opcode); return buf;
                }
            }
        } break;
    }

    vu_d_invalid(s, ptr, opcode);

    return buf;
}