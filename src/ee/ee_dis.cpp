
#include <cstdio>

#include "ee_dis.hpp"

namespace iris::ee::dis {

static Dis *s;
char *ptr;

#define EE_D_RS ((opcode >> 21) & 0x1f)
#define EE_D_RT ((opcode >> 16) & 0x1f)
#define EE_D_RD ((opcode >> 11) & 0x1f)
#define EE_D_SA ((opcode >> 6) & 0x1f)
#define EE_D_I16 (opcode & 0xffff)
#define EE_D_I26 (opcode & 0x3ffffff)
#define EE_D_SI26 ((int32_t)(EE_D_I26 << 6) >> 4)
#define EE_D_SI16 ((int32_t)(EE_D_I16 << 16) >> 14)

static const char *cop0_r[] = {
    "Index",
    "Random",
    "EntryLo0",
    "EntryLo1",
    "Context",
    "PageMask",
    "Wired",
    "Unused7",
    "BadVAddr",
    "Count",
    "EntryHi",
    "Compare",
    "Status",
    "Cause",
    "EPC",
    "PRId",
    "Config",
    "Unused17",
    "Unused18",
    "Unused19",
    "Unused20",
    "Unused21",
    "Unused22",
    "BadPAddr",
    "Debug",
    "Perf",
    "Unused26",
    "Unused27",
    "TagLo",
    "TagHi",
    "ErrorEPC",
    "Unused31"};

static const char *cc_r[] = {
    "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

static inline void d_abss(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "abs.s", EE_D_RD, EE_D_RS); }
static inline void d_add(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "add", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_addas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "adda.s", EE_D_RS, EE_D_RT); }
static inline void d_addi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "addi", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_addiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "addiu", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_adds(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "add.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void d_addu(uint32_t opcode) {
    if (EE_D_RS == 0 || EE_D_RT == 0) {
        ptr += sprintf(ptr, "%-8s $%s, $%s", "move", cc_r[EE_D_RD], cc_r[EE_D_RS == 0 ? EE_D_RT : EE_D_RS]);
    } else {
        ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "addu", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]);
    }
}
static inline void d_and(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "and", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_andi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "andi", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_bc0f(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc0f", s->pc + 4 + EE_D_SI16); }
static inline void d_bc0fl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc0fl", s->pc + 4 + EE_D_SI16); }
static inline void d_bc0t(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc0t", s->pc + 4 + EE_D_SI16); }
static inline void d_bc0tl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc0tl", s->pc + 4 + EE_D_SI16); }
static inline void d_bc1f(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc1f", s->pc + 4 + EE_D_SI16); }
static inline void d_bc1fl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc1fl", s->pc + 4 + EE_D_SI16); }
static inline void d_bc1t(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc1t", s->pc + 4 + EE_D_SI16); }
static inline void d_bc1tl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc1tl", s->pc + 4 + EE_D_SI16); }
static inline void d_bc2f(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "bc2f"); }
static inline void d_bc2fl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "bc2fl"); }
static inline void d_bc2t(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "bc2t"); }
static inline void d_bc2tl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "bc2tl"); }
static inline void d_beq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, 0x%x", "beq", cc_r[EE_D_RS], cc_r[EE_D_RT], s->pc + 4 + EE_D_SI16); }
static inline void d_beql(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, 0x%x", "beql", cc_r[EE_D_RS], cc_r[EE_D_RT], s->pc + 4 + EE_D_SI16); }
static inline void d_bgez(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgez", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bgezal(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgezal", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bgezall(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgezall", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bgezl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgezl", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bgtz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgtz", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bgtzl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgtzl", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_blez(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "blez", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_blezl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "blezl", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bltz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bltz", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bltzal(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bltzal", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bltzall(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bltzall", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bltzl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bltzl", cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void d_bne(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, 0x%x", "bne", cc_r[EE_D_RS], cc_r[EE_D_RT], s->pc + 4 + EE_D_SI16); }
static inline void d_bnel(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, 0x%x", "bnel", cc_r[EE_D_RS], cc_r[EE_D_RT], s->pc + 4 + EE_D_SI16); }
static inline void d_break(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "break"); }
static inline void d_cache(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%02x, %d($%s)", "cache", EE_D_RT, (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_callmsr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "callmsr"); }
static inline void d_ceq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "c.eq.s", EE_D_RS, EE_D_RT); }
static inline void d_cfc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $f%d", "cfc1", cc_r[EE_D_RT], EE_D_RS); }
static inline void d_cfc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "cfc2"); }
static inline void d_cf(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "c.f.s", EE_D_RS, EE_D_RT); }
static inline void d_cle(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "c.le.s", EE_D_RS, EE_D_RT); }
static inline void d_clt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "c.lt.s", EE_D_RS, EE_D_RT); }
static inline void d_ctc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $f%d", "ctc1", cc_r[EE_D_RT], EE_D_RS); }
static inline void d_ctc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "ctc2"); }
static inline void d_cvts(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "cvt.s.w", EE_D_RD, EE_D_RS); }
static inline void d_cvtw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "cvt.w.s", EE_D_RD, EE_D_RS); }
static inline void d_dadd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dadd", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_daddi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "daddi", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_daddiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "daddiu", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_daddu(uint32_t opcode) { 
    if (EE_D_RS == 0 || EE_D_RT == 0) {
        ptr += sprintf(ptr, "%-8s $%s, $%s", "move", cc_r[EE_D_RD], cc_r[EE_D_RS == 0 ? EE_D_RT : EE_D_RS]);
    } else {
        ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "daddu", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]);
    }
}
static inline void d_di(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "di"); }
static inline void d_div(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "div", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_div1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "div1", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_divs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "div.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void d_divu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "divu", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_divu1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "divu1", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_dsll(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsll", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_dsll32(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsll32", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_dsllv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsllv", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_dsra(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsra", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_dsra32(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsra32", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_dsrav(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsrav", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_dsrl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsrl", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_dsrl32(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsrl32", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_dsrlv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsrlv", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_dsub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsub", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_dsubu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsubu", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_ei(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "ei"); }
static inline void d_eret(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "eret"); }
static inline void d_j(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%08x", "j", ((s->pc + 4) & 0xf0000000) | (EE_D_I26 << 2)); }
static inline void d_jal(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%08x", "jal", ((s->pc + 4) & 0xf0000000) | (EE_D_I26 << 2)); }
static inline void d_jalr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "jalr", cc_r[EE_D_RD], cc_r[EE_D_RS]); }
static inline void d_jr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "jr", cc_r[EE_D_RS]); }
static inline void d_lb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lb", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lbu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lbu", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_ld(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "ld", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_ldl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "ldl", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_ldr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "ldr", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lh", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lhu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lhu", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lq", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lqc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "lqc2"); }
static inline void d_lui(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "lui", cc_r[EE_D_RT], EE_D_I16); }
static inline void d_lw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lw", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lwc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, %d($%s)", "lwc1", EE_D_RT, (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lwl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lwl", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lwr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lwr", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_lwu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lwu", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_madd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "madd", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_madd1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "madd1", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_maddas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "madda.s", EE_D_RS, EE_D_RT); }
static inline void d_madds(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "madd.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void d_maddu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "maddu", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_maddu1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "maddu1", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_maxs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "max.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static const char* cop0_debug_mf[8] = {
    "mfbpc", nullptr, "mfiab", "mfiabm", "mfdab", "mfdabm", "mfdvb", "mfdvbm"
};

static const char* cop0_debug_mt[8] = {
    "mtbpc", nullptr, "mtiab", "mtiabm", "mtdab", "mtdabm", "mtdvb", "mtdvbm"
};

static inline void d_mfc0(uint32_t opcode) {
    uint32_t sel = opcode & 0x7ff;

    if (EE_D_RD == 24 && sel < 8 && cop0_debug_mf[sel]) {
        ptr += sprintf(ptr, "%-8s $%s", cop0_debug_mf[sel], cc_r[EE_D_RT]);

        return;
    }

    if (EE_D_RD == 25) {
        if (sel == 0) {
            ptr += sprintf(ptr, "%-8s $%s", "mfps", cc_r[EE_D_RT]);

            return;
        }

        if (sel == 1 || sel == 3) {
            ptr += sprintf(ptr, "%-8s $%s, %d", "mfpc", cc_r[EE_D_RT], sel == 1 ? 0 : 1);

            return;
        }
    }

    ptr += sprintf(ptr, "%-8s $%s, $%s", "mfc0", cc_r[EE_D_RT], cop0_r[EE_D_RD]);
}
static inline void d_mfc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $f%d", "mfc1", cc_r[EE_D_RT], EE_D_RS); }
static inline void d_mfhi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mfhi", cc_r[EE_D_RD]); }
static inline void d_mfhi1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mfhi1", cc_r[EE_D_RD]); }
static inline void d_mflo(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mflo", cc_r[EE_D_RD]); }
static inline void d_mflo1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mflo1", cc_r[EE_D_RD]); }
static inline void d_mfsa(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mfsa", cc_r[EE_D_RD]); }
static inline void d_mins(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "min.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void d_movn(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "movn", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_movs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "mov.s", EE_D_RD, EE_D_RS); }
static inline void d_movz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "movz", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_msubas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "msuba.s", EE_D_RS, EE_D_RT); }
static inline void d_msubs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "msub.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void d_mtc0(uint32_t opcode) {
    uint32_t sel = opcode & 0x7ff;

    if (EE_D_RD == 24 && sel < 8 && cop0_debug_mt[sel]) {
        ptr += sprintf(ptr, "%-8s $%s", cop0_debug_mt[sel], cc_r[EE_D_RT]);

        return;
    }

    if (EE_D_RD == 25) {
        if (sel == 0) {
            ptr += sprintf(ptr, "%-8s $%s", "mtps", cc_r[EE_D_RT]);

            return;
        }

        if (sel == 1 || sel == 3) {
            ptr += sprintf(ptr, "%-8s $%s, %d", "mtpc", cc_r[EE_D_RT], sel == 1 ? 0 : 1);

            return;
        }
    }

    ptr += sprintf(ptr, "%-8s $%s, $%s", "mtc0", cc_r[EE_D_RT], cop0_r[EE_D_RD]);
}
static inline void d_mtc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $f%d", "mtc1", cc_r[EE_D_RT], EE_D_RS); }
static inline void d_mthi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mthi", cc_r[EE_D_RS]); }
static inline void d_mthi1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mthi1", cc_r[EE_D_RS]); }
static inline void d_mtlo(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mtlo", cc_r[EE_D_RS]); }
static inline void d_mtlo1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mtlo1", cc_r[EE_D_RS]); }
static inline void d_mtsa(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mtsa", cc_r[EE_D_RS]); }
static inline void d_mtsab(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "mtsab", cc_r[EE_D_RS], EE_D_I16); }
static inline void d_mtsah(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "mtsah", cc_r[EE_D_RS], EE_D_I16); }
static inline void d_mulas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "mula.s", EE_D_RS, EE_D_RT); }
static inline void d_muls(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "mul.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void d_mult(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "mult", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_mult1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "mult1", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_multu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "multu", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_multu1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "multu1", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_negs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "neg.s", EE_D_RD, EE_D_RS); }
static inline void d_nor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "nor", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_or(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "or", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_ori(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "ori", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_pabsh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pabsh", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_pabsw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pabsw", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_paddb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddb", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_paddh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_paddsb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddsb", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_paddsh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddsh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_paddsw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddsw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_paddub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddub", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_padduh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "padduh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_padduw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "padduw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_paddw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_padsbh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "padsbh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pand(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pand", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pceqb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pceqb", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pceqh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pceqh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pceqw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pceqw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pcgtb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcgtb", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pcgth(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcgth", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pcgtw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcgtw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pcpyh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pcpyh", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_pcpyld(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcpyld", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pcpyud(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcpyud", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pdivbw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pdivbw", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pdivuw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pdivuw", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pdivw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pdivw", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pexch(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pexch", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_pexcw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pexcw", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_pexeh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pexeh", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_pexew(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pexew", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_pext5(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pext5", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_pextlb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextlb", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pextlh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextlh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pextlw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextlw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pextub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextub", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pextuh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextuh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pextuw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextuw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_phmadh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "phmadh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_phmsbh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "phmsbh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pinteh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pinteh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pinth(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pinth", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_plzcw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "plzcw", cc_r[EE_D_RD], cc_r[EE_D_RS]); }
static inline void d_pmaddh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmaddh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmadduw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmadduw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmaddw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmaddw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmaxh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmaxh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmaxw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmaxw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmfhi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhi", cc_r[EE_D_RD]); }
static inline void d_pmfhllw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.lw", cc_r[EE_D_RD]); }
static inline void d_pmfhluw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.uw", cc_r[EE_D_RD]); }
static inline void d_pmfhlslw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.slw", cc_r[EE_D_RD]); }
static inline void d_pmfhllh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.lh", cc_r[EE_D_RD]); }
static inline void d_pmfhlsh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.sh", cc_r[EE_D_RD]); }
static inline void d_pmflo(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmflo", cc_r[EE_D_RD]); }
static inline void d_pminh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pminh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pminw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pminw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmsubh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmsubh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmsubw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmsubw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmthi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmthi", cc_r[EE_D_RS]); }
static inline void d_pmthl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmthl", cc_r[EE_D_RS]); }
static inline void d_pmtlo(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmtlo", cc_r[EE_D_RS]); }
static inline void d_pmulth(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmulth", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmultuw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmultuw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pmultw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmultw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pnor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pnor", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_por(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "por", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_ppac5(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "ppac5", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_ppacb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "ppacb", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_ppach(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "ppach", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_ppacw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "ppacw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pref(uint32_t opcode) { ptr += sprintf(ptr, "%-8s %d($%s)", "pref", (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_prevh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "prevh", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_prot3w(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "prot3w", cc_r[EE_D_RD], cc_r[EE_D_RT]); }
static inline void d_psllh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psllh", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_psllvw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psllvw", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_psllw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psllw", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_psrah(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psrah", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_psravw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psravw", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_psraw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psraw", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_psrlh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psrlh", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_psrlvw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psrlvw", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_psrlw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psrlw", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_psubb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubb", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_psubh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_psubsb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubsb", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_psubsh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubsh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_psubsw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubsw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_psubub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubub", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_psubuh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubuh", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_psubuw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubuw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_psubw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubw", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_pxor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pxor", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_qfsrv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "qfsrv", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_qmfc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "qmfc2"); }
static inline void d_qmtc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "qmtc2"); }
static inline void d_rsqrts(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "rsqrt.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void d_sb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sb", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_sd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sd", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_sdl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sdl", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_sdr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sdr", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_sh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sh", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_sll(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "sll", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_sllv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "sllv", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_slt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "slt", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_slti(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "slti", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_sltiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "sltiu", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_sltu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "sltu", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_sq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sq", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_sqc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "sqc2"); }
static inline void d_sqrts(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "sqrt.s", EE_D_RD, EE_D_RT); }
static inline void d_sra(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "sra", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_srav(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "srav", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_srl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "srl", cc_r[EE_D_RD], cc_r[EE_D_RT], EE_D_SA); }
static inline void d_srlv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "srlv", cc_r[EE_D_RD], cc_r[EE_D_RT], cc_r[EE_D_RS]); }
static inline void d_sub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "sub", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_subas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "suba.s", EE_D_RS, EE_D_RT); }
static inline void d_subs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "sub.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void d_subu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "subu", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_sw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sw", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_swc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, %d($%s)", "swc1", EE_D_RT, (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_swl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "swl", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_swr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "swr", cc_r[EE_D_RT], (int16_t)EE_D_I16, cc_r[EE_D_RS]); }
static inline void d_sync(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "sync"); }
static inline void d_syscall(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "syscall"); }
static inline void d_teq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "teq", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_teqi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "teqi", cc_r[EE_D_RS], EE_D_I16); }
static inline void d_tge(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tge", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_tgei(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tgei", cc_r[EE_D_RS], EE_D_I16); }
static inline void d_tgeiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tgeiu", cc_r[EE_D_RS], EE_D_I16); }
static inline void d_tgeu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tgeu", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_tlbp(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "tlbp"); }
static inline void d_tlbr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "tlbr"); }
static inline void d_tlbwi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "tlbwi"); }
static inline void d_tlbwr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "tlbwr"); }
static inline void d_tlt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tlt", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_tlti(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tlti", cc_r[EE_D_RS], EE_D_I16); }
static inline void d_tltiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tltiu", cc_r[EE_D_RS], EE_D_I16); }
static inline void d_tltu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tltu", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_tne(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tne", cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_tnei(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tnei", cc_r[EE_D_RS], EE_D_I16); }
static inline void d_vabs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vabs"); }
static inline void d_vadd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vadd"); }
static inline void d_vadda(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vadda"); }
static inline void d_vaddai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddai"); }
static inline void d_vaddaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddaq"); }
static inline void d_vaddaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddaw"); }
static inline void d_vaddax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddax"); }
static inline void d_vadday(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vadday"); }
static inline void d_vaddaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddaz"); }
static inline void d_vaddi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddi"); }
static inline void d_vaddq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddq"); }
static inline void d_vaddw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddw"); }
static inline void d_vaddx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddx"); }
static inline void d_vaddy(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddy"); }
static inline void d_vaddz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddz"); }
static inline void d_vcallms(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vcallms"); }
static inline void d_vclipw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vclipw"); }
static inline void d_vdiv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vdiv"); }
static inline void d_vftoi0(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vftoi0"); }
static inline void d_vftoi12(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vftoi12"); }
static inline void d_vftoi15(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vftoi15"); }
static inline void d_vftoi4(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vftoi4"); }
static inline void d_viadd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "viadd"); }
static inline void d_viaddi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "viaddi"); }
static inline void d_viand(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "viand"); }
static inline void d_vilwr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vilwr"); }
static inline void d_vior(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vior"); }
static inline void d_visub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "visub"); }
static inline void d_viswr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "viswr"); }
static inline void d_vitof0(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vitof0"); }
static inline void d_vitof12(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vitof12"); }
static inline void d_vitof15(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vitof15"); }
static inline void d_vitof4(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vitof4"); }
static inline void d_vlqd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vlqd"); }
static inline void d_vlqi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vlqi"); }
static inline void d_vmadd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmadd"); }
static inline void d_vmadda(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmadda"); }
static inline void d_vmaddai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddai"); }
static inline void d_vmaddaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddaq"); }
static inline void d_vmaddaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddaw"); }
static inline void d_vmaddax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddax"); }
static inline void d_vmadday(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmadday"); }
static inline void d_vmaddaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddaz"); }
static inline void d_vmaddi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddi"); }
static inline void d_vmaddq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddq"); }
static inline void d_vmaddw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddw"); }
static inline void d_vmaddx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddx"); }
static inline void d_vmaddy(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddy"); }
static inline void d_vmaddz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddz"); }
static inline void d_vmax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmax"); }
static inline void d_vmaxi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxi"); }
static inline void d_vmaxw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxw"); }
static inline void d_vmaxx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxx"); }
static inline void d_vmaxy(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxy"); }
static inline void d_vmaxz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxz"); }
static inline void d_vmfir(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmfir"); }
static inline void d_vmini(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmini"); }
static inline void d_vminii(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminii"); }
static inline void d_vminiw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminiw"); }
static inline void d_vminix(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminix"); }
static inline void d_vminiy(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminiy"); }
static inline void d_vminiz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminiz"); }
static inline void d_vmove(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmove"); }
static inline void d_vmr32(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmr32"); }
static inline void d_vmsub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsub"); }
static inline void d_vmsuba(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsuba"); }
static inline void d_vmsubai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubai"); }
static inline void d_vmsubaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubaq"); }
static inline void d_vmsubaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubaw"); }
static inline void d_vmsubax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubax"); }
static inline void d_vmsubay(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubay"); }
static inline void d_vmsubaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubaz"); }
static inline void d_vmsubi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubi"); }
static inline void d_vmsubq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubq"); }
static inline void d_vmsubw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubw"); }
static inline void d_vmsubx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubx"); }
static inline void d_vmsuby(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsuby"); }
static inline void d_vmsubz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubz"); }
static inline void d_vmtir(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmtir"); }
static inline void d_vmul(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmul"); }
static inline void d_vmula(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmula"); }
static inline void d_vmulai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulai"); }
static inline void d_vmulaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulaq"); }
static inline void d_vmulaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulaw"); }
static inline void d_vmulax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulax"); }
static inline void d_vmulay(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulay"); }
static inline void d_vmulaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulaz"); }
static inline void d_vmuli(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmuli"); }
static inline void d_vmulq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulq"); }
static inline void d_vmulw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulw"); }
static inline void d_vmulx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulx"); }
static inline void d_vmuly(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmuly"); }
static inline void d_vmulz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulz"); }
static inline void d_vnop(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vnop"); }
static inline void d_vopmsub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vopmsub"); }
static inline void d_vopmula(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vopmula"); }
static inline void d_vrget(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrget"); }
static inline void d_vrinit(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrinit"); }
static inline void d_vrnext(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrnext"); }
static inline void d_vrsqrt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrsqrt"); }
static inline void d_vrxor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrxor"); }
static inline void d_vsqd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsqd"); }
static inline void d_vsqi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsqi"); }
static inline void d_vsqrt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsqrt"); }
static inline void d_vsub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsub"); }
static inline void d_vsuba(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsuba"); }
static inline void d_vsubai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubai"); }
static inline void d_vsubaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubaq"); }
static inline void d_vsubaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubaw"); }
static inline void d_vsubax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubax"); }
static inline void d_vsubay(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubay"); }
static inline void d_vsubaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubaz"); }
static inline void d_vsubi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubi"); }
static inline void d_vsubq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubq"); }
static inline void d_vsubw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubw"); }
static inline void d_vsubx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubx"); }
static inline void d_vsuby(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsuby"); }
static inline void d_vsubz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubz"); }
static inline void d_vwaitq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vwaitq"); }
static inline void d_xor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "xor", cc_r[EE_D_RD], cc_r[EE_D_RS], cc_r[EE_D_RT]); }
static inline void d_xori(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "xori", cc_r[EE_D_RT], cc_r[EE_D_RS], EE_D_I16); }
static inline void d_invalid(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "<invalid>"); }
static inline void d_nop(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "nop"); }

char *disassemble(char *buf, uint32_t opcode, Dis *dis_state) {
    s = dis_state;

    ptr = buf;

    if (dis_state) if (dis_state->print_address)
        ptr += sprintf(ptr, "%08x: ", dis_state->pc);

    if (dis_state) if (dis_state->print_opcode)
        ptr += sprintf(ptr, "%08x ", opcode);

    if (opcode == 0) {
        d_nop(opcode);

        return buf;
    }

    switch (opcode & 0xFC000000) {
        case 0x00000000: { // special
            switch (opcode & 0x0000003F) {
                case 0x00000000: d_sll(opcode); return buf;
                case 0x00000002: d_srl(opcode); return buf;
                case 0x00000003: d_sra(opcode); return buf;
                case 0x00000004: d_sllv(opcode); return buf;
                case 0x00000006: d_srlv(opcode); return buf;
                case 0x00000007: d_srav(opcode); return buf;
                case 0x00000008: d_jr(opcode); return buf;
                case 0x00000009: d_jalr(opcode); return buf;
                case 0x0000000A: d_movz(opcode); return buf;
                case 0x0000000B: d_movn(opcode); return buf;
                case 0x0000000C: d_syscall(opcode); return buf;
                case 0x0000000D: d_break(opcode); return buf;
                case 0x0000000F: d_sync(opcode); return buf;
                case 0x00000010: d_mfhi(opcode); return buf;
                case 0x00000011: d_mthi(opcode); return buf;
                case 0x00000012: d_mflo(opcode); return buf;
                case 0x00000013: d_mtlo(opcode); return buf;
                case 0x00000014: d_dsllv(opcode); return buf;
                case 0x00000016: d_dsrlv(opcode); return buf;
                case 0x00000017: d_dsrav(opcode); return buf;
                case 0x00000018: d_mult(opcode); return buf;
                case 0x00000019: d_multu(opcode); return buf;
                case 0x0000001A: d_div(opcode); return buf;
                case 0x0000001B: d_divu(opcode); return buf;
                case 0x00000020: d_add(opcode); return buf;
                case 0x00000021: d_addu(opcode); return buf;
                case 0x00000022: d_sub(opcode); return buf;
                case 0x00000023: d_subu(opcode); return buf;
                case 0x00000024: d_and(opcode); return buf;
                case 0x00000025: d_or(opcode); return buf;
                case 0x00000026: d_xor(opcode); return buf;
                case 0x00000027: d_nor(opcode); return buf;
                case 0x00000028: d_mfsa(opcode); return buf;
                case 0x00000029: d_mtsa(opcode); return buf;
                case 0x0000002A: d_slt(opcode); return buf;
                case 0x0000002B: d_sltu(opcode); return buf;
                case 0x0000002C: d_dadd(opcode); return buf;
                case 0x0000002D: d_daddu(opcode); return buf;
                case 0x0000002E: d_dsub(opcode); return buf;
                case 0x0000002F: d_dsubu(opcode); return buf;
                case 0x00000030: d_tge(opcode); return buf;
                case 0x00000031: d_tgeu(opcode); return buf;
                case 0x00000032: d_tlt(opcode); return buf;
                case 0x00000033: d_tltu(opcode); return buf;
                case 0x00000034: d_teq(opcode); return buf;
                case 0x00000036: d_tne(opcode); return buf;
                case 0x00000038: d_dsll(opcode); return buf;
                case 0x0000003A: d_dsrl(opcode); return buf;
                case 0x0000003B: d_dsra(opcode); return buf;
                case 0x0000003C: d_dsll32(opcode); return buf;
                case 0x0000003E: d_dsrl32(opcode); return buf;
                case 0x0000003F: d_dsra32(opcode); return buf;
            }
        } break;
        case 0x04000000: { // regimm
            switch (opcode & 0x001F0000) {
                case 0x00000000: d_bltz(opcode); return buf;
                case 0x00010000: d_bgez(opcode); return buf;
                case 0x00020000: d_bltzl(opcode); return buf;
                case 0x00030000: d_bgezl(opcode); return buf;
                case 0x00080000: d_tgei(opcode); return buf;
                case 0x00090000: d_tgeiu(opcode); return buf;
                case 0x000A0000: d_tlti(opcode); return buf;
                case 0x000B0000: d_tltiu(opcode); return buf;
                case 0x000C0000: d_teqi(opcode); return buf;
                case 0x000E0000: d_tnei(opcode); return buf;
                case 0x00100000: d_bltzal(opcode); return buf;
                case 0x00110000: d_bgezal(opcode); return buf;
                case 0x00120000: d_bltzall(opcode); return buf;
                case 0x00130000: d_bgezall(opcode); return buf;
                case 0x00180000: d_mtsab(opcode); return buf;
                case 0x00190000: d_mtsah(opcode); return buf;
            }
        } break;
        case 0x08000000: d_j(opcode); return buf;
        case 0x0C000000: d_jal(opcode); return buf;
        case 0x10000000: d_beq(opcode); return buf;
        case 0x14000000: d_bne(opcode); return buf;
        case 0x18000000: d_blez(opcode); return buf;
        case 0x1C000000: d_bgtz(opcode); return buf;
        case 0x20000000: d_addi(opcode); return buf;
        case 0x24000000: d_addiu(opcode); return buf;
        case 0x28000000: d_slti(opcode); return buf;
        case 0x2C000000: d_sltiu(opcode); return buf;
        case 0x30000000: d_andi(opcode); return buf;
        case 0x34000000: d_ori(opcode); return buf;
        case 0x38000000: d_xori(opcode); return buf;
        case 0x3C000000: d_lui(opcode); return buf;
        case 0x40000000: { // cop0
            switch (opcode & 0x03E00000) {
                case 0x00000000: d_mfc0(opcode); return buf;
                case 0x00800000: d_mtc0(opcode); return buf;
                case 0x01000000: {
                    switch (opcode & 0x001F0000) {
                        case 0x00000000: d_bc0f(opcode); return buf;
                        case 0x00010000: d_bc0t(opcode); return buf;
                        case 0x00020000: d_bc0fl(opcode); return buf;
                        case 0x00030000: d_bc0tl(opcode); return buf;
                    }
                } break;
                case 0x02000000: {
                    switch (opcode & 0x0000003F) {
                    case 0x00000001: d_tlbr(opcode); return buf;
                    case 0x00000002: d_tlbwi(opcode); return buf;
                    case 0x00000006: d_tlbwr(opcode); return buf;
                    case 0x00000008: d_tlbp(opcode); return buf;
                    case 0x00000018: d_eret(opcode); return buf;
                    case 0x00000038: d_ei(opcode); return buf;
                    case 0x00000039: d_di(opcode); return buf;
                    }
                } break;
            }
        } break;
        case 0x44000000: { // cop1
            switch (opcode & 0x03E00000) {
                case 0x00000000: d_mfc1(opcode); return buf;
                case 0x00400000: d_cfc1(opcode); return buf;
                case 0x00800000: d_mtc1(opcode); return buf;
                case 0x00C00000: d_ctc1(opcode); return buf;
                case 0x01000000: {
                    switch (opcode & 0x001F0000) {
                        case 0x00000000: d_bc1f(opcode); return buf;
                        case 0x00010000: d_bc1t(opcode); return buf;
                        case 0x00020000: d_bc1fl(opcode); return buf;
                        case 0x00030000: d_bc1tl(opcode); return buf;
                    }
                } break;
                case 0x02000000: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000000: d_adds(opcode); return buf;
                        case 0x00000001: d_subs(opcode); return buf;
                        case 0x00000002: d_muls(opcode); return buf;
                        case 0x00000003: d_divs(opcode); return buf;
                        case 0x00000004: d_sqrts(opcode); return buf;
                        case 0x00000005: d_abss(opcode); return buf;
                        case 0x00000006: d_movs(opcode); return buf;
                        case 0x00000007: d_negs(opcode); return buf;
                        case 0x00000016: d_rsqrts(opcode); return buf;
                        case 0x00000018: d_addas(opcode); return buf;
                        case 0x00000019: d_subas(opcode); return buf;
                        case 0x0000001A: d_mulas(opcode); return buf;
                        case 0x0000001C: d_madds(opcode); return buf;
                        case 0x0000001D: d_msubs(opcode); return buf;
                        case 0x0000001E: d_maddas(opcode); return buf;
                        case 0x0000001F: d_msubas(opcode); return buf;
                        case 0x00000024: d_cvtw(opcode); return buf;
                        case 0x00000028: d_maxs(opcode); return buf;
                        case 0x00000029: d_mins(opcode); return buf;
                        case 0x00000030: d_cf(opcode); return buf;
                        case 0x00000032: d_ceq(opcode); return buf;
                        case 0x00000034: d_clt(opcode); return buf;
                        case 0x00000036: d_cle(opcode); return buf;
                    }
                } break;
                case 0x02800000: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000020: d_cvts(opcode); return buf;
                    }
                } break;
            }
        } break;
        case 0x48000000: { // cop2
            switch (opcode & 0x03E00000) {
                case 0x00200000: d_qmfc2(opcode); return buf;
                case 0x00400000: d_cfc2(opcode); return buf;
                case 0x00A00000: d_qmtc2(opcode); return buf;
                case 0x00C00000: d_ctc2(opcode); return buf;
                case 0x01000000: {
                    switch (opcode & 0x001F0000) {
                    case 0x00000000: d_bc2f(opcode); return buf;
                    case 0x00010000: d_bc2t(opcode); return buf;
                    case 0x00020000: d_bc2fl(opcode); return buf;
                    case 0x00030000: d_bc2tl(opcode); return buf;
                    }
                }
                break;
                case 0x02000000:
                case 0x02200000:
                case 0x02400000:
                case 0x02600000:
                case 0x02800000:
                case 0x02A00000:
                case 0x02C00000:
                case 0x02E00000:
                case 0x03000000:
                case 0x03200000:
                case 0x03400000:
                case 0x03600000:
                case 0x03800000:
                case 0x03A00000:
                case 0x03C00000:
                case 0x03E00000: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000000: d_vaddx(opcode); return buf;
                        case 0x00000001: d_vaddy(opcode); return buf;
                        case 0x00000002: d_vaddz(opcode); return buf;
                        case 0x00000003: d_vaddw(opcode); return buf;
                        case 0x00000004: d_vsubx(opcode); return buf;
                        case 0x00000005: d_vsuby(opcode); return buf;
                        case 0x00000006: d_vsubz(opcode); return buf;
                        case 0x00000007: d_vsubw(opcode); return buf;
                        case 0x00000008: d_vmaddx(opcode); return buf;
                        case 0x00000009: d_vmaddy(opcode); return buf;
                        case 0x0000000A: d_vmaddz(opcode); return buf;
                        case 0x0000000B: d_vmaddw(opcode); return buf;
                        case 0x0000000C: d_vmsubx(opcode); return buf;
                        case 0x0000000D: d_vmsuby(opcode); return buf;
                        case 0x0000000E: d_vmsubz(opcode); return buf;
                        case 0x0000000F: d_vmsubw(opcode); return buf;
                        case 0x00000010: d_vmaxx(opcode); return buf;
                        case 0x00000011: d_vmaxy(opcode); return buf;
                        case 0x00000012: d_vmaxz(opcode); return buf;
                        case 0x00000013: d_vmaxw(opcode); return buf;
                        case 0x00000014: d_vminix(opcode); return buf;
                        case 0x00000015: d_vminiy(opcode); return buf;
                        case 0x00000016: d_vminiz(opcode); return buf;
                        case 0x00000017: d_vminiw(opcode); return buf;
                        case 0x00000018: d_vmulx(opcode); return buf;
                        case 0x00000019: d_vmuly(opcode); return buf;
                        case 0x0000001A: d_vmulz(opcode); return buf;
                        case 0x0000001B: d_vmulw(opcode); return buf;
                        case 0x0000001C: d_vmulq(opcode); return buf;
                        case 0x0000001D: d_vmaxi(opcode); return buf;
                        case 0x0000001E: d_vmuli(opcode); return buf;
                        case 0x0000001F: d_vminii(opcode); return buf;
                        case 0x00000020: d_vaddq(opcode); return buf;
                        case 0x00000021: d_vmaddq(opcode); return buf;
                        case 0x00000022: d_vaddi(opcode); return buf;
                        case 0x00000023: d_vmaddi(opcode); return buf;
                        case 0x00000024: d_vsubq(opcode); return buf;
                        case 0x00000025: d_vmsubq(opcode); return buf;
                        case 0x00000026: d_vsubi(opcode); return buf;
                        case 0x00000027: d_vmsubi(opcode); return buf;
                        case 0x00000028: d_vadd(opcode); return buf;
                        case 0x00000029: d_vmadd(opcode); return buf;
                        case 0x0000002A: d_vmul(opcode); return buf;
                        case 0x0000002B: d_vmax(opcode); return buf;
                        case 0x0000002C: d_vsub(opcode); return buf;
                        case 0x0000002D: d_vmsub(opcode); return buf;
                        case 0x0000002E: d_vopmsub(opcode); return buf;
                        case 0x0000002F: d_vmini(opcode); return buf;
                        case 0x00000030: d_viadd(opcode); return buf;
                        case 0x00000031: d_visub(opcode); return buf;
                        case 0x00000032: d_viaddi(opcode); return buf;
                        case 0x00000034: d_viand(opcode); return buf;
                        case 0x00000035: d_vior(opcode); return buf;
                        case 0x00000038: d_vcallms(opcode); return buf;
                        case 0x00000039: d_callmsr(opcode); return buf;
                        case 0x0000003C:
                        case 0x0000003D:
                        case 0x0000003E:
                        case 0x0000003F: {
                            uint32_t func = (opcode & 3) | ((opcode & 0x7c0) >> 4);

                            switch (func) {
                                case 0x00000000: d_vaddax(opcode); return buf;
                                case 0x00000001: d_vadday(opcode); return buf;
                                case 0x00000002: d_vaddaz(opcode); return buf;
                                case 0x00000003: d_vaddaw(opcode); return buf;
                                case 0x00000004: d_vsubax(opcode); return buf;
                                case 0x00000005: d_vsubay(opcode); return buf;
                                case 0x00000006: d_vsubaz(opcode); return buf;
                                case 0x00000007: d_vsubaw(opcode); return buf;
                                case 0x00000008: d_vmaddax(opcode); return buf;
                                case 0x00000009: d_vmadday(opcode); return buf;
                                case 0x0000000A: d_vmaddaz(opcode); return buf;
                                case 0x0000000B: d_vmaddaw(opcode); return buf;
                                case 0x0000000C: d_vmsubax(opcode); return buf;
                                case 0x0000000D: d_vmsubay(opcode); return buf;
                                case 0x0000000E: d_vmsubaz(opcode); return buf;
                                case 0x0000000F: d_vmsubaw(opcode); return buf;
                                case 0x00000010: d_vitof0(opcode); return buf;
                                case 0x00000011: d_vitof4(opcode); return buf;
                                case 0x00000012: d_vitof12(opcode); return buf;
                                case 0x00000013: d_vitof15(opcode); return buf;
                                case 0x00000014: d_vftoi0(opcode); return buf;
                                case 0x00000015: d_vftoi4(opcode); return buf;
                                case 0x00000016: d_vftoi12(opcode); return buf;
                                case 0x00000017: d_vftoi15(opcode); return buf;
                                case 0x00000018: d_vmulax(opcode); return buf;
                                case 0x00000019: d_vmulay(opcode); return buf;
                                case 0x0000001A: d_vmulaz(opcode); return buf;
                                case 0x0000001B: d_vmulaw(opcode); return buf;
                                case 0x0000001C: d_vmulaq(opcode); return buf;
                                case 0x0000001D: d_vabs(opcode); return buf;
                                case 0x0000001E: d_vmulai(opcode); return buf;
                                case 0x0000001F: d_vclipw(opcode); return buf;
                                case 0x00000020: d_vaddaq(opcode); return buf;
                                case 0x00000021: d_vmaddaq(opcode); return buf;
                                case 0x00000022: d_vaddai(opcode); return buf;
                                case 0x00000023: d_vmaddai(opcode); return buf;
                                case 0x00000024: d_vsubaq(opcode); return buf;
                                case 0x00000025: d_vmsubaq(opcode); return buf;
                                case 0x00000026: d_vsubai(opcode); return buf;
                                case 0x00000027: d_vmsubai(opcode); return buf;
                                case 0x00000028: d_vadda(opcode); return buf;
                                case 0x00000029: d_vmadda(opcode); return buf;
                                case 0x0000002A: d_vmula(opcode); return buf;
                                case 0x0000002C: d_vsuba(opcode); return buf;
                                case 0x0000002D: d_vmsuba(opcode); return buf;
                                case 0x0000002E: d_vopmula(opcode); return buf;
                                case 0x0000002F: d_vnop(opcode); return buf;
                                case 0x00000030: d_vmove(opcode); return buf;
                                case 0x00000031: d_vmr32(opcode); return buf;
                                case 0x00000034: d_vlqi(opcode); return buf;
                                case 0x00000035: d_vsqi(opcode); return buf;
                                case 0x00000036: d_vlqd(opcode); return buf;
                                case 0x00000037: d_vsqd(opcode); return buf;
                                case 0x00000038: d_vdiv(opcode); return buf;
                                case 0x00000039: d_vsqrt(opcode); return buf;
                                case 0x0000003A: d_vrsqrt(opcode); return buf;
                                case 0x0000003B: d_vwaitq(opcode); return buf;
                                case 0x0000003C: d_vmtir(opcode); return buf;
                                case 0x0000003D: d_vmfir(opcode); return buf;
                                case 0x0000003E: d_vilwr(opcode); return buf;
                                case 0x0000003F: d_viswr(opcode); return buf;
                                case 0x00000040: d_vrnext(opcode); return buf;
                                case 0x00000041: d_vrget(opcode); return buf;
                                case 0x00000042: d_vrinit(opcode); return buf;
                                case 0x00000043: d_vrxor(opcode); return buf;
                            }
                        } break;
                    }
                } break;
            }
        } break;
        case 0x50000000: d_beql(opcode); return buf;
        case 0x54000000: d_bnel(opcode); return buf;
        case 0x58000000: d_blezl(opcode); return buf;
        case 0x5C000000: d_bgtzl(opcode); return buf;
        case 0x60000000: d_daddi(opcode); return buf;
        case 0x64000000: d_daddiu(opcode); return buf;
        case 0x68000000: d_ldl(opcode); return buf;
        case 0x6C000000: d_ldr(opcode); return buf;
        case 0x70000000: { // mmi
            switch (opcode & 0x0000003F) {
                case 0x00000000: d_madd(opcode); return buf;
                case 0x00000001: d_maddu(opcode); return buf;
                case 0x00000004: d_plzcw(opcode); return buf;
                case 0x00000008: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000000: d_paddw(opcode); return buf;
                        case 0x00000040: d_psubw(opcode); return buf;
                        case 0x00000080: d_pcgtw(opcode); return buf;
                        case 0x000000C0: d_pmaxw(opcode); return buf;
                        case 0x00000100: d_paddh(opcode); return buf;
                        case 0x00000140: d_psubh(opcode); return buf;
                        case 0x00000180: d_pcgth(opcode); return buf;
                        case 0x000001C0: d_pmaxh(opcode); return buf;
                        case 0x00000200: d_paddb(opcode); return buf;
                        case 0x00000240: d_psubb(opcode); return buf;
                        case 0x00000280: d_pcgtb(opcode); return buf;
                        case 0x00000400: d_paddsw(opcode); return buf;
                        case 0x00000440: d_psubsw(opcode); return buf;
                        case 0x00000480: d_pextlw(opcode); return buf;
                        case 0x000004C0: d_ppacw(opcode); return buf;
                        case 0x00000500: d_paddsh(opcode); return buf;
                        case 0x00000540: d_psubsh(opcode); return buf;
                        case 0x00000580: d_pextlh(opcode); return buf;
                        case 0x000005C0: d_ppach(opcode); return buf;
                        case 0x00000600: d_paddsb(opcode); return buf;
                        case 0x00000640: d_psubsb(opcode); return buf;
                        case 0x00000680: d_pextlb(opcode); return buf;
                        case 0x000006C0: d_ppacb(opcode); return buf;
                        case 0x00000780: d_pext5(opcode); return buf;
                        case 0x000007C0: d_ppac5(opcode); return buf;
                    }
                } break;
                case 0x00000009: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000000: d_pmaddw(opcode); return buf;
                        case 0x00000080: d_psllvw(opcode); return buf;
                        case 0x000000C0: d_psrlvw(opcode); return buf;
                        case 0x00000100: d_pmsubw(opcode); return buf;
                        case 0x00000200: d_pmfhi(opcode); return buf;
                        case 0x00000240: d_pmflo(opcode); return buf;
                        case 0x00000280: d_pinth(opcode); return buf;
                        case 0x00000300: d_pmultw(opcode); return buf;
                        case 0x00000340: d_pdivw(opcode); return buf;
                        case 0x00000380: d_pcpyld(opcode); return buf;
                        case 0x00000400: d_pmaddh(opcode); return buf;
                        case 0x00000440: d_phmadh(opcode); return buf;
                        case 0x00000480: d_pand(opcode); return buf;
                        case 0x000004C0: d_pxor(opcode); return buf;
                        case 0x00000500: d_pmsubh(opcode); return buf;
                        case 0x00000540: d_phmsbh(opcode); return buf;
                        case 0x00000680: d_pexeh(opcode); return buf;
                        case 0x000006C0: d_prevh(opcode); return buf;
                        case 0x00000700: d_pmulth(opcode); return buf;
                        case 0x00000740: d_pdivbw(opcode); return buf;
                        case 0x00000780: d_pexew(opcode); return buf;
                        case 0x000007C0: d_prot3w(opcode); return buf;
                    }
                } break;
                case 0x00000010: d_mfhi1(opcode); return buf;
                case 0x00000011: d_mthi1(opcode); return buf;
                case 0x00000012: d_mflo1(opcode); return buf;
                case 0x00000013: d_mtlo1(opcode); return buf;
                case 0x00000018: d_mult1(opcode); return buf;
                case 0x00000019: d_multu1(opcode); return buf;
                case 0x0000001A: d_div1(opcode); return buf;
                case 0x0000001B: d_divu1(opcode); return buf;
                case 0x00000020: d_madd1(opcode); return buf;
                case 0x00000021: d_maddu1(opcode); return buf;
                case 0x00000028: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000040: d_pabsw(opcode); return buf;
                        case 0x00000080: d_pceqw(opcode); return buf;
                        case 0x000000C0: d_pminw(opcode); return buf;
                        case 0x00000100: d_padsbh(opcode); return buf;
                        case 0x00000140: d_pabsh(opcode); return buf;
                        case 0x00000180: d_pceqh(opcode); return buf;
                        case 0x000001C0: d_pminh(opcode); return buf;
                        case 0x00000280: d_pceqb(opcode); return buf;
                        case 0x00000400: d_padduw(opcode); return buf;
                        case 0x00000440: d_psubuw(opcode); return buf;
                        case 0x00000480: d_pextuw(opcode); return buf;
                        case 0x00000500: d_padduh(opcode); return buf;
                        case 0x00000540: d_psubuh(opcode); return buf;
                        case 0x00000580: d_pextuh(opcode); return buf;
                        case 0x00000600: d_paddub(opcode); return buf;
                        case 0x00000640: d_psubub(opcode); return buf;
                        case 0x00000680: d_pextub(opcode); return buf;
                        case 0x000006C0: d_qfsrv(opcode); return buf;
                    }
                } break;
                case 0x00000029: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000000: d_pmadduw(opcode); return buf;
                        case 0x000000C0: d_psravw(opcode); return buf;
                        case 0x00000200: d_pmthi(opcode); return buf;
                        case 0x00000240: d_pmtlo(opcode); return buf;
                        case 0x00000280: d_pinteh(opcode); return buf;
                        case 0x00000300: d_pmultuw(opcode); return buf;
                        case 0x00000340: d_pdivuw(opcode); return buf;
                        case 0x00000380: d_pcpyud(opcode); return buf;
                        case 0x00000480: d_por(opcode); return buf;
                        case 0x000004C0: d_pnor(opcode); return buf;
                        case 0x00000680: d_pexch(opcode); return buf;
                        case 0x000006C0: d_pcpyh(opcode); return buf;
                        case 0x00000780: d_pexcw(opcode); return buf;
                    }
                } break;
                case 0x00000030: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000000: d_pmfhllw(opcode); return buf;
                        case 0x00000040: d_pmfhluw(opcode); return buf;
                        case 0x00000080: d_pmfhlslw(opcode); return buf;
                        case 0x000000c0: d_pmfhllh(opcode); return buf;
                        case 0x00000100: d_pmfhlsh(opcode); return buf;
                    }
                } break;
                case 0x00000031: d_pmthl(opcode); return buf;
                case 0x00000034: d_psllh(opcode); return buf;
                case 0x00000036: d_psrlh(opcode); return buf;
                case 0x00000037: d_psrah(opcode); return buf;
                case 0x0000003C: d_psllw(opcode); return buf;
                case 0x0000003E: d_psrlw(opcode); return buf;
                case 0x0000003F: d_psraw(opcode); return buf;
            }
        } break;
        case 0x78000000: d_lq(opcode); return buf;
        case 0x7C000000: d_sq(opcode); return buf;
        case 0x80000000: d_lb(opcode); return buf;
        case 0x84000000: d_lh(opcode); return buf;
        case 0x88000000: d_lwl(opcode); return buf;
        case 0x8C000000: d_lw(opcode); return buf;
        case 0x90000000: d_lbu(opcode); return buf;
        case 0x94000000: d_lhu(opcode); return buf;
        case 0x98000000: d_lwr(opcode); return buf;
        case 0x9C000000: d_lwu(opcode); return buf;
        case 0xA0000000: d_sb(opcode); return buf;
        case 0xA4000000: d_sh(opcode); return buf;
        case 0xA8000000: d_swl(opcode); return buf;
        case 0xAC000000: d_sw(opcode); return buf;
        case 0xB0000000: d_sdl(opcode); return buf;
        case 0xB4000000: d_sdr(opcode); return buf;
        case 0xB8000000: d_swr(opcode); return buf;
        case 0xBC000000: d_cache(opcode); return buf;
        case 0xC4000000: d_lwc1(opcode); return buf;
        case 0xCC000000: d_pref(opcode); return buf;
        case 0xD8000000: d_lqc2(opcode); return buf;
        case 0xDC000000: d_ld(opcode); return buf;
        case 0xE4000000: d_swc1(opcode); return buf;
        case 0xF8000000: d_sqc2(opcode); return buf;
        case 0xFC000000: d_sd(opcode); return buf;
    }

    d_invalid(opcode);
    
    return buf;
}

}
