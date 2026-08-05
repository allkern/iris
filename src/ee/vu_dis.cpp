
#include <cstdio>

#include "vu_dis.hpp"

namespace iris::vu::dis {

#define LD_DEST ((o >> 21) & 0xf)
#define LD_DI(i) (o & (1 << (24 - i)))
#define LD_DX ((o >> 24) & 1)
#define LD_DY ((o >> 23) & 1)
#define LD_DZ ((o >> 22) & 1)
#define LD_DW ((o >> 21) & 1)
#define LD_D ((o >> 6) & 0x1f)
#define LD_S ((o >> 11) & 0x1f)
#define LD_T ((o >> 16) & 0x1f)
#define LD_SF ((o >> 21) & 3)
#define LD_TF ((o >> 23) & 3)
#define LD_IMM5 (((int32_t)(LD_D << 27)) >> 27)
#define LD_IMM11 (((int32_t)((o & 0x7ff) << 21)) >> 21)
#define LD_IMM12 (o & 0x7ff)
#define LD_IMM15 ((o & 0x7ff) | ((o & 0x1e00000) >> 10))
#define LD_IMM24 (o & 0xffffff)
#define UD_DEST ((o >> 21) & 0xf)
#define UD_DI(i) (o & (1 << (24 - i)))
#define UD_DX ((o >> 24) & 1)
#define UD_DY ((o >> 23) & 1)
#define UD_DZ ((o >> 22) & 1)
#define UD_DW ((o >> 21) & 1)
#define UD_D ((o >> 6) & 0x1f)
#define UD_S ((o >> 11) & 0x1f)
#define UD_T ((o >> 16) & 0x1f)

// Print broadcast fields
static inline char* d_bc(Dis* s, char* p, uint32_t bc) {
    if (!bc)
        return p;

    *p++ = '.';

    for (int i = 0; i < 4; i++) {
        if (bc & (1 << (3 - i))) *p++ = "xyzw"[i];
    }

    *p = '\0';

    return p;
}

static inline char* d_mnemonic(Dis* s, char* p, const char* m, uint32_t bc) {
    char buf[32];
    char* ptr = buf;

    ptr += sprintf(ptr, "%s", m);
    ptr = d_bc(s, ptr, bc);

    p += sprintf(p, "%-12s", buf);

    return p;
}

static inline char* d_mnemonic_nd(Dis* s, char* p, const char* m) {
    p += sprintf(p, "%-12s", m);

    return p;
}

static inline char* d_addax(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addax", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_adday(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "adday", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_addaz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addaz", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_addaw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addaw", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_subax(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subax", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_subay(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subay", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_subaz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subaz", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_subaw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subaw", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_maddax(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddax", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_madday(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "madday", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_maddaz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddaz", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_maddaw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddaw", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_msubax(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubax", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_msubay(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubay", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_msubaz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubaz", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_msubaw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubaw", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_itof0(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "itof0", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_itof4(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "itof4", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_itof12(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "itof12", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_itof15(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "itof15", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_ftoi0(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "ftoi0", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_ftoi4(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "ftoi4", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_ftoi12(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "ftoi12", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_ftoi15(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "ftoi15", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_mulax(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulax", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_mulay(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulay", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_mulaz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulaz", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_mulaw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulaw", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_mulaq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulaq", UD_DEST); p += sprintf(p, "acc, vf%02u, q", UD_S); return p;
}
static inline char* d_abs(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "abs", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_T, UD_S); return p;
}
static inline char* d_mulai(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulai", UD_DEST); p += sprintf(p, "acc, vf%02u, i", UD_S); return p;
}
static inline char* d_clip(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "clipw", UD_DEST); p += sprintf(p, "vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_addaq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addaq", UD_DEST); p += sprintf(p, "acc, vf%02u, q", UD_S); return p;
}
static inline char* d_maddaq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddaq", UD_DEST); p += sprintf(p, "acc, vf%02u, q", UD_S); return p;
}
static inline char* d_addai(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addai", UD_DEST); p += sprintf(p, "acc, vf%02u, i", UD_S); return p;
}
static inline char* d_maddai(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddai", UD_DEST); p += sprintf(p, "acc, vf%02u, i", UD_S); return p;
}
static inline char* d_subaq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subaq", UD_DEST); p += sprintf(p, "acc, vf%02u, q", UD_S); return p;
}
static inline char* d_msubaq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubaq", UD_DEST); p += sprintf(p, "acc, vf%02u, q", UD_S); return p;
}
static inline char* d_subai(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subai", UD_DEST); p += sprintf(p, "acc, vf%02u, i", UD_S); return p;
}
static inline char* d_msubai(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubai", UD_DEST); p += sprintf(p, "acc, vf%02u, i", UD_S); return p;
}
static inline char* d_adda(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "adda", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_madda(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "madda", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_mula(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mula", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_suba(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "suba", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_msuba(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msuba", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_opmula(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "opmula", UD_DEST); p += sprintf(p, "acc, vf%02u, vf%02u", UD_S, UD_T); return p;
}
static inline char* d_nop(Dis* s, char* p, uint32_t o) {
    p += sprintf(p, "%s", "nop"); return p;
}
static inline char* d_addx(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addx", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_addy(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addy", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_addz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addz", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_addw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addw", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_subx(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subx", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_suby(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "suby", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_subz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subz", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_subw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subw", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_maddx(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddx", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_maddy(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddy", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_maddz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddz", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_maddw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddw", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_msubx(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubx", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_msuby(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msuby", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_msubz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubz", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_msubw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubw", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_maxx(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maxx", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_maxy(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maxy", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_maxz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maxz", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_maxw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maxw", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_minix(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "minix", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_miniy(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "miniy", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_miniz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "miniz", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_miniw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "miniw", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_mulx(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulx", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_muly(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "muly", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_mulz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulz", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_mulw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulw", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_mulq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mulq", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", UD_D, UD_S); return p;
}
static inline char* d_maxi(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maxi", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", UD_D, UD_S); return p;
}
static inline char* d_muli(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "muli", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", UD_D, UD_S); return p;
}
static inline char* d_minii(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "minii", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", UD_D, UD_S); return p;
}
static inline char* d_addq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addq", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", UD_D, UD_S); return p;
}
static inline char* d_maddq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddq", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", UD_D, UD_S); return p;
}
static inline char* d_addi(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "addi", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", UD_D, UD_S); return p;
}
static inline char* d_maddi(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "maddi", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", UD_D, UD_S); return p;
}
static inline char* d_subq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subq", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", UD_D, UD_S); return p;
}
static inline char* d_msubq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubq", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, q", UD_D, UD_S); return p;
}
static inline char* d_subi(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "subi", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", UD_D, UD_S); return p;
}
static inline char* d_msubi(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msubi", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, i", UD_D, UD_S); return p;
}
static inline char* d_add(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "add", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_madd(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "madd", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_mul(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mul", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_max(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "max", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_sub(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "sub", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_msub(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "msub", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_opmsub(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "opmsub", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}
static inline char* d_mini(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mini", UD_DEST); p += sprintf(p, "vf%02u, vf%02u, vf%02u", UD_D, UD_S, UD_T); return p;
}

static inline char* d_lq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "lq", LD_DEST); p += sprintf(p, "vf%02u, %d(vi%02u)", LD_T, LD_IMM11, LD_S); return p;
}
static inline char* d_sq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "sq", LD_DEST); p += sprintf(p, "vf%02u, %d(vi%02u)", LD_S, LD_IMM11, LD_T); return p;
}
static inline char* d_ilw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "ilw", LD_DEST); p += sprintf(p, "vi%02u, %d(vi%02u)", LD_T, LD_IMM11, LD_S); return p;
}
static inline char* d_isw(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "isw", LD_DEST); p += sprintf(p, "vi%02u, %d(vi%02u)", LD_T, LD_IMM11, LD_S); return p;
}
static inline char* d_iaddiu(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "iaddiu"); p += sprintf(p, "vi%02u, vi%02u, %d", LD_T, LD_S, LD_IMM15); return p;
}
static inline char* d_isubiu(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "isubiu"); p += sprintf(p, "vi%02u, vi%02u, %d", LD_T, LD_S, LD_IMM15); return p;
}
static inline char* d_fceq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fceq"); p += sprintf(p, "vi01, 0x%06x", LD_IMM24); return p;
}
static inline char* d_fcset(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fcset"); p += sprintf(p, "0x%06x", LD_IMM24); return p;
}
static inline char* d_fcand(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fcand"); p += sprintf(p, "vi01, 0x%06x", LD_IMM24); return p;
}
static inline char* d_fcor(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fcor"); p += sprintf(p, "vi01, 0x%06x", LD_IMM24); return p;
}
static inline char* d_fseq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fseq"); p += sprintf(p, "0x%04x", LD_IMM12); return p;
}
static inline char* d_fsset(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fsset"); p += sprintf(p, "0x%04x", LD_IMM12); return p;
}
static inline char* d_fsand(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fsand"); p += sprintf(p, "0x%04x", LD_IMM12); return p;
}
static inline char* d_fsor(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fsor"); p += sprintf(p, "0x%04x", LD_IMM12); return p;
}
static inline char* d_fmeq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fmeq"); p += sprintf(p, "vi%02u, vi%02u", LD_T, LD_S); return p;
}
static inline char* d_fmand(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fmand"); p += sprintf(p, "vi%02u, vi%02u", LD_T, LD_S); return p;
}
static inline char* d_fmor(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fmor"); p += sprintf(p, "vi%02u, vi%02u", LD_T, LD_S); return p;
}
static inline char* d_fcget(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "fcget"); p += sprintf(p, "vi%02u", LD_T); return p;
}
static inline char* d_b(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "b"); p += sprintf(p, "%04x", s ? (s->addr + 1 + LD_IMM11) : LD_IMM11); return p;
}
static inline char* d_bal(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "bal"); p += sprintf(p, "vi%02u, 0x%04x", LD_T, s ? (s->addr + 1 + LD_IMM11) : LD_IMM11); return p;
}
static inline char* d_jr(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "jr"); p += sprintf(p, "vi%02u", LD_S); return p;
}
static inline char* d_jalr(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "jalr"); p += sprintf(p, "vi%02u, vi%02u", LD_T, LD_S); return p;
}
static inline char* d_ibeq(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ibeq"); p += sprintf(p, "vi%02u, vi%02u, 0x%04x", LD_T, LD_S, s ? (s->addr + 1 + LD_IMM11) : LD_IMM11); return p;
}
static inline char* d_ibne(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ibne"); p += sprintf(p, "vi%02u, vi%02u, 0x%04x", LD_T, LD_S, s ? (s->addr + 1 + LD_IMM11) : LD_IMM11); return p;
}
static inline char* d_ibltz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ibltz"); p += sprintf(p, "vi%02u, 0x%04x", LD_S, s ? (s->addr + 1 + LD_IMM11) : LD_IMM11); return p;
}
static inline char* d_ibgtz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ibgtz"); p += sprintf(p, "vi%02u, 0x%04x", LD_S, s ? (s->addr + 1 + LD_IMM11) : LD_IMM11); return p;
}
static inline char* d_iblez(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "iblez"); p += sprintf(p, "vi%02u, 0x%04x", LD_S, s ? (s->addr + 1 + LD_IMM11) : LD_IMM11); return p;
}
static inline char* d_ibgez(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ibgez"); p += sprintf(p, "vi%02u, 0x%04x", LD_S, s ? (s->addr + 1 + LD_IMM11) : LD_IMM11); return p;
}
static inline char* d_move(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "move", LD_DEST); p += sprintf(p, "vf%02u, vf%02u", LD_T, LD_S); return p;
}
static inline char* d_mr32(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mr32", LD_DEST); p += sprintf(p, "vf%02u, vf%02u", LD_T, LD_S); return p;
}
static inline char* d_lqi(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "lqi", LD_DEST); p += sprintf(p, "vf%02u, (vi%02u++)", LD_T, LD_S); return p;
}
static inline char* d_sqi(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "sqi", LD_DEST); p += sprintf(p, "vf%02u, (vi%02u++)", LD_S, LD_T); return p;
}
static inline char* d_lqd(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "lqd", LD_DEST); p += sprintf(p, "vf%02u, (--vi%02u)", LD_T, LD_S); return p;
}
static inline char* d_sqd(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "sqd", LD_DEST); p += sprintf(p, "vf%02u, (--vi%02u)", LD_S, LD_T); return p;
}
static inline char* d_div(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "div"); p += sprintf(p, "q, vf%02u%c, vf%02u%c", LD_S, "xyzw"[LD_SF], LD_T, "xyzw"[LD_TF]); return p;
}
static inline char* d_sqrt(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "sqrt"); p += sprintf(p, "q, vf%02u%c", LD_T, "xyzw"[LD_TF]); return p;
}
static inline char* d_rsqrt(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "rsqrt"); p += sprintf(p, "q, vf%02u%c, vf%02u%c", LD_S, "xyzw"[LD_SF], LD_T, "xyzw"[LD_TF]); return p;
}
static inline char* d_waitq(Dis* s, char* p, uint32_t o) {
    p += sprintf(p, "waitq"); return p;
}
static inline char* d_mtir(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "mtir"); p += sprintf(p, "vi%02u, vf%02u%c", LD_T, LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_mfir(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mfir", LD_DEST); p += sprintf(p, "vf%02u, vi%02u", LD_T, LD_S); return p;
}
static inline char* d_ilwr(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "ilwr", LD_DEST); p += sprintf(p, "vi%02u, (vi%02u)", LD_T, LD_S); return p;
}
static inline char* d_iswr(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "iswr", LD_DEST); p += sprintf(p, "vi%02u, (vi%02u)", LD_T, LD_S); return p;
}
static inline char* d_rnext(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "rnext", LD_DEST); p += sprintf(p, "vf%02u, r", LD_T); return p;
}
static inline char* d_rget(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "rget", LD_DEST); p += sprintf(p, "vf%02u, r", LD_T); return p;
}
static inline char* d_rinit(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "rinit"); p += sprintf(p, "r, vf%02u%c", LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_rxor(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "rxor"); p += sprintf(p, "r, vf%02u%c", LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_mfp(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic(s, p, "mfp", LD_DEST); p += sprintf(p, "vf%02u, p", LD_T); return p;
}
static inline char* d_xtop(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "xtop"); p += sprintf(p, "vi%02u", LD_T); return p;
}
static inline char* d_xitop(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "xitop"); p += sprintf(p, "vi%02u", LD_T); return p;
}
static inline char* d_xgkick(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "xgkick"); p += sprintf(p, "vi%02u", LD_S); return p;
}
static inline char* d_esadd(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "esadd"); p += sprintf(p, "p, vf%02u", LD_S); return p;
}
static inline char* d_ersadd(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ersadd"); p += sprintf(p, "p, vf%02u", LD_S); return p;
}
static inline char* d_eleng(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "eleng"); p += sprintf(p, "p, vf%02u", LD_S); return p;
}
static inline char* d_erleng(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "erleng"); p += sprintf(p, "p, vf%02u", LD_S); return p;
}
static inline char* d_eatanxy(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "eatan.xy"); p += sprintf(p, "p, vf%02u", LD_S); return p;
}
static inline char* d_eatanxz(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "eatan.xz"); p += sprintf(p, "p, vf%02u", LD_S); return p;
}
static inline char* d_esum(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "esum"); p += sprintf(p, "p, vf%02u", LD_S); return p;
}
static inline char* d_esqrt(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "esqrt"); p += sprintf(p, "p, vf%02u%c", LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_ersqrt(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ersqrt"); p += sprintf(p, "p, vf%02u%c", LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_ercpr(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ercpr"); p += sprintf(p, "p, vf%02u%c", LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_waitp(Dis* s, char* p, uint32_t o) {
    p += sprintf(p, "waitp"); return p;
}
static inline char* d_esin(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "esin"); p += sprintf(p, "p, vf%02u%c", LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_eatan(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "eatan"); p += sprintf(p, "p, vf%02u%c", LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_eexp(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "eexp"); p += sprintf(p, "p, vf%02u%c", LD_S, "xyzw"[LD_SF]); return p;
}
static inline char* d_iadd(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "iadd"); p += sprintf(p, "vi%02u, vi%02u, vi%02u", LD_D, LD_S, LD_T); return p;
}
static inline char* d_isub(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "isub"); p += sprintf(p, "vi%02u, vi%02u, vi%02u", LD_D, LD_S, LD_T); return p;
}
static inline char* d_iaddi(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "iaddi"); p += sprintf(p, "vi%02u, vi%02u, %d", LD_T, LD_S, LD_IMM5); return p;
}
static inline char* d_iand(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "iand"); p += sprintf(p, "vi%02u, vi%02u, vi%02u", LD_D, LD_S, LD_T); return p;
}
static inline char* d_ior(Dis* s, char* p, uint32_t o) {
    p = d_mnemonic_nd(s, p, "ior"); p += sprintf(p, "vi%02u, vi%02u, vi%02u", LD_D, LD_S, LD_T); return p;
}
static inline char* d_invalid(Dis* s, char* p, uint32_t o) {
    p += sprintf(p, "<invalid>"); return p;
}

char* disassemble_upper(char* buf, uint64_t opcode, Dis* s) {
    char* ptr = buf;

    ptr = buf;

    if (s) if (s->print_address)
        ptr += sprintf(ptr, "%08x: ", s->addr);

    if (s) if (s->print_opcode)
        ptr += sprintf(ptr, "%08x ", (uint32_t)opcode);

    // Decode 000007FF style instruction
    if ((opcode & 0x3c) == 0x3c) {
        switch (((opcode & 0x3c0) >> 4) | (opcode & 3)) {
            case 0x00: d_addax(s, ptr, opcode); return buf;
            case 0x01: d_adday(s, ptr, opcode); return buf;
            case 0x02: d_addaz(s, ptr, opcode); return buf;
            case 0x03: d_addaw(s, ptr, opcode); return buf;
            case 0x04: d_subax(s, ptr, opcode); return buf;
            case 0x05: d_subay(s, ptr, opcode); return buf;
            case 0x06: d_subaz(s, ptr, opcode); return buf;
            case 0x07: d_subaw(s, ptr, opcode); return buf;
            case 0x08: d_maddax(s, ptr, opcode); return buf;
            case 0x09: d_madday(s, ptr, opcode); return buf;
            case 0x0A: d_maddaz(s, ptr, opcode); return buf;
            case 0x0B: d_maddaw(s, ptr, opcode); return buf;
            case 0x0C: d_msubax(s, ptr, opcode); return buf;
            case 0x0D: d_msubay(s, ptr, opcode); return buf;
            case 0x0E: d_msubaz(s, ptr, opcode); return buf;
            case 0x0F: d_msubaw(s, ptr, opcode); return buf;
            case 0x10: d_itof0(s, ptr, opcode); return buf;
            case 0x11: d_itof4(s, ptr, opcode); return buf;
            case 0x12: d_itof12(s, ptr, opcode); return buf;
            case 0x13: d_itof15(s, ptr, opcode); return buf;
            case 0x14: d_ftoi0(s, ptr, opcode); return buf;
            case 0x15: d_ftoi4(s, ptr, opcode); return buf;
            case 0x16: d_ftoi12(s, ptr, opcode); return buf;
            case 0x17: d_ftoi15(s, ptr, opcode); return buf;
            case 0x18: d_mulax(s, ptr, opcode); return buf;
            case 0x19: d_mulay(s, ptr, opcode); return buf;
            case 0x1A: d_mulaz(s, ptr, opcode); return buf;
            case 0x1B: d_mulaw(s, ptr, opcode); return buf;
            case 0x1C: d_mulaq(s, ptr, opcode); return buf;
            case 0x1D: d_abs(s, ptr, opcode); return buf;
            case 0x1E: d_mulai(s, ptr, opcode); return buf;
            case 0x1F: d_clip(s, ptr, opcode); return buf;
            case 0x20: d_addaq(s, ptr, opcode); return buf;
            case 0x21: d_maddaq(s, ptr, opcode); return buf;
            case 0x22: d_addai(s, ptr, opcode); return buf;
            case 0x23: d_maddai(s, ptr, opcode); return buf;
            case 0x24: d_subaq(s, ptr, opcode); return buf;
            case 0x25: d_msubaq(s, ptr, opcode); return buf;
            case 0x26: d_subai(s, ptr, opcode); return buf;
            case 0x27: d_msubai(s, ptr, opcode); return buf;
            case 0x28: d_adda(s, ptr, opcode); return buf;
            case 0x29: d_madda(s, ptr, opcode); return buf;
            case 0x2A: d_mula(s, ptr, opcode); return buf;
            case 0x2C: d_suba(s, ptr, opcode); return buf;
            case 0x2D: d_msuba(s, ptr, opcode); return buf;
            case 0x2E: d_opmula(s, ptr, opcode); return buf;
            case 0x2F: d_nop(s, ptr, opcode); return buf;
        }
    } else {
        // Decode 0000003F style instruction
        switch (opcode & 0x3f) {
            case 0x00: d_addx(s, ptr, opcode); return buf;
            case 0x01: d_addy(s, ptr, opcode); return buf;
            case 0x02: d_addz(s, ptr, opcode); return buf;
            case 0x03: d_addw(s, ptr, opcode); return buf;
            case 0x04: d_subx(s, ptr, opcode); return buf;
            case 0x05: d_suby(s, ptr, opcode); return buf;
            case 0x06: d_subz(s, ptr, opcode); return buf;
            case 0x07: d_subw(s, ptr, opcode); return buf;
            case 0x08: d_maddx(s, ptr, opcode); return buf;
            case 0x09: d_maddy(s, ptr, opcode); return buf;
            case 0x0A: d_maddz(s, ptr, opcode); return buf;
            case 0x0B: d_maddw(s, ptr, opcode); return buf;
            case 0x0C: d_msubx(s, ptr, opcode); return buf;
            case 0x0D: d_msuby(s, ptr, opcode); return buf;
            case 0x0E: d_msubz(s, ptr, opcode); return buf;
            case 0x0F: d_msubw(s, ptr, opcode); return buf;
            case 0x10: d_maxx(s, ptr, opcode); return buf;
            case 0x11: d_maxy(s, ptr, opcode); return buf;
            case 0x12: d_maxz(s, ptr, opcode); return buf;
            case 0x13: d_maxw(s, ptr, opcode); return buf;
            case 0x14: d_minix(s, ptr, opcode); return buf;
            case 0x15: d_miniy(s, ptr, opcode); return buf;
            case 0x16: d_miniz(s, ptr, opcode); return buf;
            case 0x17: d_miniw(s, ptr, opcode); return buf;
            case 0x18: d_mulx(s, ptr, opcode); return buf;
            case 0x19: d_muly(s, ptr, opcode); return buf;
            case 0x1A: d_mulz(s, ptr, opcode); return buf;
            case 0x1B: d_mulw(s, ptr, opcode); return buf;
            case 0x1C: d_mulq(s, ptr, opcode); return buf;
            case 0x1D: d_maxi(s, ptr, opcode); return buf;
            case 0x1E: d_muli(s, ptr, opcode); return buf;
            case 0x1F: d_minii(s, ptr, opcode); return buf;
            case 0x20: d_addq(s, ptr, opcode); return buf;
            case 0x21: d_maddq(s, ptr, opcode); return buf;
            case 0x22: d_addi(s, ptr, opcode); return buf;
            case 0x23: d_maddi(s, ptr, opcode); return buf;
            case 0x24: d_subq(s, ptr, opcode); return buf;
            case 0x25: d_msubq(s, ptr, opcode); return buf;
            case 0x26: d_subi(s, ptr, opcode); return buf;
            case 0x27: d_msubi(s, ptr, opcode); return buf;
            case 0x28: d_add(s, ptr, opcode); return buf;
            case 0x29: d_madd(s, ptr, opcode); return buf;
            case 0x2A: d_mul(s, ptr, opcode); return buf;
            case 0x2B: d_max(s, ptr, opcode); return buf;
            case 0x2C: d_sub(s, ptr, opcode); return buf;
            case 0x2D: d_msub(s, ptr, opcode); return buf;
            case 0x2E: d_opmsub(s, ptr, opcode); return buf;
            case 0x2F: d_mini(s, ptr, opcode); return buf;
        }
    }

    d_invalid(s, ptr, opcode);

    return buf;
}

char* disassemble_lower(char* buf, uint64_t opcode, Dis* s, int ibit) {
    char* ptr = buf;

    ptr = buf;

    if (s) if (s->print_address)
        ptr += sprintf(ptr, "%08x: ", s->addr);

    if (s) if (s->print_opcode)
        ptr += sprintf(ptr, "%08x ", (uint32_t)opcode);

    if (ibit) {
        ptr = d_mnemonic_nd(s, ptr, "loi");
        ptr += sprintf(ptr, "%f", *(float*)&opcode);

        return buf;
    }

    switch ((opcode & 0xFE000000) >> 25) {
        case 0x00: d_lq(s, ptr, opcode); return buf;
        case 0x01: d_sq(s, ptr, opcode); return buf;
        case 0x04: d_ilw(s, ptr, opcode); return buf;
        case 0x05: d_isw(s, ptr, opcode); return buf;
        case 0x08: d_iaddiu(s, ptr, opcode); return buf;
        case 0x09: d_isubiu(s, ptr, opcode); return buf;
        case 0x10: d_fceq(s, ptr, opcode); return buf;
        case 0x11: d_fcset(s, ptr, opcode); return buf;
        case 0x12: d_fcand(s, ptr, opcode); return buf;
        case 0x13: d_fcor(s, ptr, opcode); return buf;
        case 0x14: d_fseq(s, ptr, opcode); return buf;
        case 0x15: d_fsset(s, ptr, opcode); return buf;
        case 0x16: d_fsand(s, ptr, opcode); return buf;
        case 0x17: d_fsor(s, ptr, opcode); return buf;
        case 0x18: d_fmeq(s, ptr, opcode); return buf;
        case 0x1A: d_fmand(s, ptr, opcode); return buf;
        case 0x1B: d_fmor(s, ptr, opcode); return buf;
        case 0x1C: d_fcget(s, ptr, opcode); return buf;
        case 0x20: d_b(s, ptr, opcode); return buf;
        case 0x21: d_bal(s, ptr, opcode); return buf;
        case 0x24: d_jr(s, ptr, opcode); return buf;
        case 0x25: d_jalr(s, ptr, opcode); return buf;
        case 0x28: d_ibeq(s, ptr, opcode); return buf;
        case 0x29: d_ibne(s, ptr, opcode); return buf;
        case 0x2C: d_ibltz(s, ptr, opcode); return buf;
        case 0x2D: d_ibgtz(s, ptr, opcode); return buf;
        case 0x2E: d_iblez(s, ptr, opcode); return buf;
        case 0x2F: d_ibgez(s, ptr, opcode); return buf;
        case 0x40: {
            if ((opcode & 0x3C) == 0x3C) {
                switch (((opcode & 0x7C0) >> 4) | (opcode & 3)) {
                    case 0x30: d_move(s, ptr, opcode); return buf;
                    case 0x31: d_mr32(s, ptr, opcode); return buf;
                    case 0x34: d_lqi(s, ptr, opcode); return buf;
                    case 0x35: d_sqi(s, ptr, opcode); return buf;
                    case 0x36: d_lqd(s, ptr, opcode); return buf;
                    case 0x37: d_sqd(s, ptr, opcode); return buf;
                    case 0x38: d_div(s, ptr, opcode); return buf;
                    case 0x39: d_sqrt(s, ptr, opcode); return buf;
                    case 0x3A: d_rsqrt(s, ptr, opcode); return buf;
                    case 0x3B: d_waitq(s, ptr, opcode); return buf;
                    case 0x3C: d_mtir(s, ptr, opcode); return buf;
                    case 0x3D: d_mfir(s, ptr, opcode); return buf;
                    case 0x3E: d_ilwr(s, ptr, opcode); return buf;
                    case 0x3F: d_iswr(s, ptr, opcode); return buf;
                    case 0x40: d_rnext(s, ptr, opcode); return buf;
                    case 0x41: d_rget(s, ptr, opcode); return buf;
                    case 0x42: d_rinit(s, ptr, opcode); return buf;
                    case 0x43: d_rxor(s, ptr, opcode); return buf;
                    case 0x64: d_mfp(s, ptr, opcode); return buf;
                    case 0x68: d_xtop(s, ptr, opcode); return buf;
                    case 0x69: d_xitop(s, ptr, opcode); return buf;
                    case 0x6C: d_xgkick(s, ptr, opcode); return buf;
                    case 0x70: d_esadd(s, ptr, opcode); return buf;
                    case 0x71: d_ersadd(s, ptr, opcode); return buf;
                    case 0x72: d_eleng(s, ptr, opcode); return buf;
                    case 0x73: d_erleng(s, ptr, opcode); return buf;
                    case 0x74: d_eatanxy(s, ptr, opcode); return buf;
                    case 0x75: d_eatanxz(s, ptr, opcode); return buf;
                    case 0x76: d_esum(s, ptr, opcode); return buf;
                    case 0x78: d_esqrt(s, ptr, opcode); return buf;
                    case 0x79: d_ersqrt(s, ptr, opcode); return buf;
                    case 0x7A: d_ercpr(s, ptr, opcode); return buf;
                    case 0x7B: d_waitp(s, ptr, opcode); return buf;
                    case 0x7C: d_esin(s, ptr, opcode); return buf;
                    case 0x7D: d_eatan(s, ptr, opcode); return buf;
                    case 0x7E: d_eexp(s, ptr, opcode); return buf;
                }
            } else {
                switch (opcode & 0x3F) {
                    case 0x30: d_iadd(s, ptr, opcode); return buf;
                    case 0x31: d_isub(s, ptr, opcode); return buf;
                    case 0x32: d_iaddi(s, ptr, opcode); return buf;
                    case 0x34: d_iand(s, ptr, opcode); return buf;
                    case 0x35: d_ior(s, ptr, opcode); return buf;
                }
            }
        } break;
    }

    d_invalid(s, ptr, opcode);

    return buf;
}

}
