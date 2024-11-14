#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ee_dis.h"

static struct ee_dis_state *s;
char *ptr;

#define EE_D_RS ((opcode >> 21) & 0x1f)
#define EE_D_RT ((opcode >> 16) & 0x1f)
#define EE_D_RD ((opcode >> 11) & 0x1f)
#define EE_D_SA ((opcode >> 6) & 0x1f)
#define EE_D_I16 (opcode & 0xffff)
#define EE_D_I26 (opcode & 0x3ffffff)
#define EE_D_SI26 ((int32_t)(EE_D_I26 << 6) >> 4)
#define EE_D_SI16 ((int32_t)(EE_D_I16 << 16) >> 14)

static const char *ee_cop0_r[] = {
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

static const char *ee_cc_r[] = {
    "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"};

static inline void ee_d_abss(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "abs.s", EE_D_RD, EE_D_RS); }
static inline void ee_d_add(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "add", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_addas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "adda.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_addi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "addi", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_addiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "addiu", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_adds(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "add.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_addu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "addu", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_and(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "and", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_andi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "andi", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_bc0f(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc0f", s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bc0fl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc0fl", s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bc0t(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc0t", s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bc0tl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc0tl", s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bc1f(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc1f", s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bc1fl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc1fl", s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bc1t(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc1t", s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bc1tl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%x", "bc1tl", s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bc2f(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "bc2f"); }
static inline void ee_d_bc2fl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "bc2fl"); }
static inline void ee_d_bc2t(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "bc2t"); }
static inline void ee_d_bc2tl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "bc2tl"); }
static inline void ee_d_beq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, 0x%x", "beq", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_beql(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, 0x%x", "beql", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bgez(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgez", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bgezal(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgezal", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bgezall(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgezall", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bgezl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgezl", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bgtz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgtz", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bgtzl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bgtzl", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_blez(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "blez", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_blezl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "blezl", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bltz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bltz", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bltzal(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bltzal", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bltzall(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bltzall", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bltzl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, 0x%x", "bltzl", ee_cc_r[EE_D_RS], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bne(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, 0x%x", "bne", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_bnel(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, 0x%x", "bnel", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT], s->pc + 4 + EE_D_SI16); }
static inline void ee_d_break(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "break"); }
static inline void ee_d_cache(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%02x, %d($%s)", "cache", EE_D_RT, (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_callmsr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "callmsr"); }
static inline void ee_d_ceq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "c.eq.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_cfc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $f%d", "cfc1", ee_cc_r[EE_D_RT], EE_D_RS); }
static inline void ee_d_cfc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "cfc2"); }
static inline void ee_d_cf(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "c.f.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_cle(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "c.le.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_clt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "c.lt.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_ctc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $f%d", "ctc1", ee_cc_r[EE_D_RT], EE_D_RS); }
static inline void ee_d_ctc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "ctc2"); }
static inline void ee_d_cvts(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "cvt.s.w", EE_D_RD, EE_D_RS); }
static inline void ee_d_cvtw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "cvt.w.s", EE_D_RD, EE_D_RS); }
static inline void ee_d_dadd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dadd", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_daddi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "daddi", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_daddiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "daddiu", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_daddu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "daddu", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_di(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "di"); }
static inline void ee_d_div(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "div", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_div1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "div1", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_divs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "div.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_divu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "divu", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_divu1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "divu1", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_dsll(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsll", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_dsll32(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsll32", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_dsllv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsllv", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_dsra(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsra", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_dsra32(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsra32", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_dsrav(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsrav", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_dsrl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsrl", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_dsrl32(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "dsrl32", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_dsrlv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsrlv", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_dsub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsub", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_dsubu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "dsubu", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_ei(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "ei"); }
static inline void ee_d_eret(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "eret"); }
static inline void ee_d_j(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%08x", "j", ((s->pc + 4) & 0xf0000000) | (EE_D_I26 << 2)); }
static inline void ee_d_jal(uint32_t opcode) { ptr += sprintf(ptr, "%-8s 0x%08x", "jal", ((s->pc + 4) & 0xf0000000) | (EE_D_I26 << 2)); }
static inline void ee_d_jalr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "jalr", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS]); }
static inline void ee_d_jr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "jr", ee_cc_r[EE_D_RS]); }
static inline void ee_d_lb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lb", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lbu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lbu", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_ld(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "ld", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_ldl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "ldl", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_ldr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "ldr", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lh", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lhu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lhu", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lq", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lqc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "lqc2"); }
static inline void ee_d_lui(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "lui", ee_cc_r[EE_D_RT], EE_D_I16); }
static inline void ee_d_lw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lw", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lwc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, %d($%s)", "lwc1", EE_D_RT, (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lwl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lwl", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lwr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lwr", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_lwu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "lwu", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_madd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "madd", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_madd1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "madd1", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_maddas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "madda.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_madds(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "madd.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_maddu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "maddu", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_maddu1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "maddu1", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_maxs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "max.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_mfc0(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "mfc0", ee_cc_r[EE_D_RT], ee_cop0_r[EE_D_RD]); }
static inline void ee_d_mfc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $f%d", "mfc1", ee_cc_r[EE_D_RT], EE_D_RS); }
static inline void ee_d_mfhi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mfhi", ee_cc_r[EE_D_RD]); }
static inline void ee_d_mfhi1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mfhi1", ee_cc_r[EE_D_RD]); }
static inline void ee_d_mflo(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mflo", ee_cc_r[EE_D_RD]); }
static inline void ee_d_mflo1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mflo1", ee_cc_r[EE_D_RD]); }
static inline void ee_d_mfsa(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mfsa", ee_cc_r[EE_D_RD]); }
static inline void ee_d_mins(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "min.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_movn(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "movn", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_movs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "mov.s", EE_D_RD, EE_D_RS); }
static inline void ee_d_movz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "movz", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_msubas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "msuba.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_msubs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "msub.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_mtc0(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "mtc0", ee_cc_r[EE_D_RT], ee_cop0_r[EE_D_RD]); }
static inline void ee_d_mtc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $f%d", "mtc1", ee_cc_r[EE_D_RT], EE_D_RS); }
static inline void ee_d_mthi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mthi", ee_cc_r[EE_D_RS]); }
static inline void ee_d_mthi1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mthi1", ee_cc_r[EE_D_RS]); }
static inline void ee_d_mtlo(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mtlo", ee_cc_r[EE_D_RS]); }
static inline void ee_d_mtlo1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mtlo1", ee_cc_r[EE_D_RS]); }
static inline void ee_d_mtsa(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "mtsa", ee_cc_r[EE_D_RS]); }
static inline void ee_d_mtsab(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "mtsab", ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_mtsah(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "mtsah", ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_mulas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "mula.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_muls(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "mul.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_mult(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "mult", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_mult1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "mult1", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_multu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "multu", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_multu1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "multu1", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_negs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "neg.s", EE_D_RD, EE_D_RS); }
static inline void ee_d_nor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "nor", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_or(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "or", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_ori(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "ori", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_pabsh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pabsh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pabsw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pabsw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_paddb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddb", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_paddh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_paddsb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddsb", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_paddsh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddsh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_paddsw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddsw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_paddub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddub", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_padduh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "padduh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_padduw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "padduw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_paddw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "paddw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_padsbh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "padsbh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pand(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pand", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pceqb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pceqb", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pceqh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pceqh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pceqw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pceqw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pcgtb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcgtb", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pcgth(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcgth", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pcgtw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcgtw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pcpyh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pcpyh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pcpyld(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcpyld", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pcpyud(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pcpyud", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pdivbw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pdivbw", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pdivuw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pdivuw", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pdivw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pdivw", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pexch(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pexch", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pexcw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pexcw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pexeh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pexeh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pexew(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pexew", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pext5(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "pext5", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pextlb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextlb", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pextlh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextlh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pextlw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextlw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pextub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextub", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pextuh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextuh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pextuw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pextuw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_phmadh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "phmadh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_phmsbh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "phmsbh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pinteh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pinteh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pinth(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pinth", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_plzcw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "plzcw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS]); }
static inline void ee_d_pmaddh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmaddh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmadduw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmadduw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmaddw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmaddw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmaxh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmaxh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmaxw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmaxw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmfhi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhi", ee_cc_r[EE_D_RD]); }
static inline void ee_d_pmfhllw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.lw", ee_cc_r[EE_D_RD]); }
static inline void ee_d_pmfhluw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.uw", ee_cc_r[EE_D_RD]); }
static inline void ee_d_pmfhlslw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.slw", ee_cc_r[EE_D_RD]); }
static inline void ee_d_pmfhllh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.lh", ee_cc_r[EE_D_RD]); }
static inline void ee_d_pmfhlsh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmfhl.sh", ee_cc_r[EE_D_RD]); }
static inline void ee_d_pmflo(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmflo", ee_cc_r[EE_D_RD]); }
static inline void ee_d_pminh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pminh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pminw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pminw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmsubh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmsubh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmsubw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmsubw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmthi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmthi", ee_cc_r[EE_D_RS]); }
static inline void ee_d_pmthl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmthl", ee_cc_r[EE_D_RS]); }
static inline void ee_d_pmtlo(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s", "pmtlo", ee_cc_r[EE_D_RS]); }
static inline void ee_d_pmulth(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmulth", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmultuw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmultuw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pmultw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pmultw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pnor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pnor", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_por(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "por", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_ppac5(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "ppac5", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_ppacb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "ppacb", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_ppach(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "ppach", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_ppacw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "ppacw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pref(uint32_t opcode) { ptr += sprintf(ptr, "%-8s %d($%s)", "pref", (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_prevh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "prevh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_prot3w(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "prot3w", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psllh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psllh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_psllvw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psllvw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_psllw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psllw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_psrah(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psrah", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_psravw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psravw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_psraw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psraw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_psrlh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psrlh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_psrlvw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psrlvw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_psrlw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "psrlw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_psubb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubb", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psubh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psubsb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubsb", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psubsh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubsh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psubsw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubsw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psubub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubub", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psubuh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubuh", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psubuw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubuw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_psubw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "psubw", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_pxor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "pxor", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_qfsrv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "qfsrv", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_qmfc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "qmfc2"); }
static inline void ee_d_qmtc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "qmtc2"); }
static inline void ee_d_rsqrts(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "rsqrt.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_sb(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sb", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_sd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sd", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_sdl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sdl", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_sdr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sdr", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_sh(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sh", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_sll(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "sll", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_sllv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "sllv", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_slt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "slt", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_slti(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "slti", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_sltiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "sltiu", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_sltu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "sltu", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_sq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sq", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_sqc2(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "sqc2"); }
static inline void ee_d_sqrts(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "sqrt.s", EE_D_RD, EE_D_RT); }
static inline void ee_d_sra(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "sra", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_srav(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "srav", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_srl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "srl", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], EE_D_SA); }
static inline void ee_d_srlv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "srlv", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS]); }
static inline void ee_d_sub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "sub", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_subas(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d", "suba.s", EE_D_RS, EE_D_RT); }
static inline void ee_d_subs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, $f%d, $f%d", "sub.s", EE_D_RD, EE_D_RS, EE_D_RT); }
static inline void ee_d_subu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "subu", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_sw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "sw", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_swc1(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $f%d, %d($%s)", "swc1", EE_D_RT, (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_swl(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "swl", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_swr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d($%s)", "swr", ee_cc_r[EE_D_RT], (int16_t)EE_D_I16, ee_cc_r[EE_D_RS]); }
static inline void ee_d_sync(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "sync"); }
static inline void ee_d_syscall(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "syscall"); }
static inline void ee_d_teq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "teq", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_teqi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "teqi", ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_tge(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tge", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_tgei(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tgei", ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_tgeiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tgeiu", ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_tgeu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tgeu", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_tlbp(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "tlbp"); }
static inline void ee_d_tlbr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "tlbr"); }
static inline void ee_d_tlbwi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "tlbwi"); }
static inline void ee_d_tlbwr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "tlbwr"); }
static inline void ee_d_tlt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tlt", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_tlti(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tlti", ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_tltiu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tltiu", ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_tltu(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tltu", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_tne(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s", "tne", ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_tnei(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, %d", "tnei", ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_vabs(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vabs"); }
static inline void ee_d_vadd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vadd"); }
static inline void ee_d_vadda(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vadda"); }
static inline void ee_d_vaddai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddai"); }
static inline void ee_d_vaddaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddaq"); }
static inline void ee_d_vaddaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddaw"); }
static inline void ee_d_vaddax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddax"); }
static inline void ee_d_vadday(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vadday"); }
static inline void ee_d_vaddaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddaz"); }
static inline void ee_d_vaddi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddi"); }
static inline void ee_d_vaddq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddq"); }
static inline void ee_d_vaddw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddw"); }
static inline void ee_d_vaddx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddx"); }
static inline void ee_d_vaddy(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddy"); }
static inline void ee_d_vaddz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vaddz"); }
static inline void ee_d_vcallms(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vcallms"); }
static inline void ee_d_vclipw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vclipw"); }
static inline void ee_d_vdiv(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vdiv"); }
static inline void ee_d_vftoi0(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vftoi0"); }
static inline void ee_d_vftoi12(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vftoi12"); }
static inline void ee_d_vftoi15(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vftoi15"); }
static inline void ee_d_vftoi4(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vftoi4"); }
static inline void ee_d_viadd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "viadd"); }
static inline void ee_d_viaddi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "viaddi"); }
static inline void ee_d_viand(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "viand"); }
static inline void ee_d_vilwr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vilwr"); }
static inline void ee_d_vior(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vior"); }
static inline void ee_d_visub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "visub"); }
static inline void ee_d_viswr(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "viswr"); }
static inline void ee_d_vitof0(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vitof0"); }
static inline void ee_d_vitof12(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vitof12"); }
static inline void ee_d_vitof15(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vitof15"); }
static inline void ee_d_vitof4(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vitof4"); }
static inline void ee_d_vlqd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vlqd"); }
static inline void ee_d_vlqi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vlqi"); }
static inline void ee_d_vmadd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmadd"); }
static inline void ee_d_vmadda(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmadda"); }
static inline void ee_d_vmaddai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddai"); }
static inline void ee_d_vmaddaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddaq"); }
static inline void ee_d_vmaddaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddaw"); }
static inline void ee_d_vmaddax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddax"); }
static inline void ee_d_vmadday(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmadday"); }
static inline void ee_d_vmaddaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddaz"); }
static inline void ee_d_vmaddi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddi"); }
static inline void ee_d_vmaddq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddq"); }
static inline void ee_d_vmaddw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddw"); }
static inline void ee_d_vmaddx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddx"); }
static inline void ee_d_vmaddy(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddy"); }
static inline void ee_d_vmaddz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaddz"); }
static inline void ee_d_vmax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmax"); }
static inline void ee_d_vmaxi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxi"); }
static inline void ee_d_vmaxw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxw"); }
static inline void ee_d_vmaxx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxx"); }
static inline void ee_d_vmaxy(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxy"); }
static inline void ee_d_vmaxz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmaxz"); }
static inline void ee_d_vmfir(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmfir"); }
static inline void ee_d_vmini(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmini"); }
static inline void ee_d_vminii(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminii"); }
static inline void ee_d_vminiw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminiw"); }
static inline void ee_d_vminix(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminix"); }
static inline void ee_d_vminiy(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminiy"); }
static inline void ee_d_vminiz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vminiz"); }
static inline void ee_d_vmove(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmove"); }
static inline void ee_d_vmr32(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmr32"); }
static inline void ee_d_vmsub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsub"); }
static inline void ee_d_vmsuba(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsuba"); }
static inline void ee_d_vmsubai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubai"); }
static inline void ee_d_vmsubaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubaq"); }
static inline void ee_d_vmsubaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubaw"); }
static inline void ee_d_vmsubax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubax"); }
static inline void ee_d_vmsubay(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubay"); }
static inline void ee_d_vmsubaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubaz"); }
static inline void ee_d_vmsubi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubi"); }
static inline void ee_d_vmsubq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubq"); }
static inline void ee_d_vmsubw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubw"); }
static inline void ee_d_vmsubx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubx"); }
static inline void ee_d_vmsuby(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsuby"); }
static inline void ee_d_vmsubz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmsubz"); }
static inline void ee_d_vmtir(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmtir"); }
static inline void ee_d_vmul(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmul"); }
static inline void ee_d_vmula(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmula"); }
static inline void ee_d_vmulai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulai"); }
static inline void ee_d_vmulaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulaq"); }
static inline void ee_d_vmulaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulaw"); }
static inline void ee_d_vmulax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulax"); }
static inline void ee_d_vmulay(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulay"); }
static inline void ee_d_vmulaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulaz"); }
static inline void ee_d_vmuli(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmuli"); }
static inline void ee_d_vmulq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulq"); }
static inline void ee_d_vmulw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulw"); }
static inline void ee_d_vmulx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulx"); }
static inline void ee_d_vmuly(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmuly"); }
static inline void ee_d_vmulz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vmulz"); }
static inline void ee_d_vnop(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vnop"); }
static inline void ee_d_vopmsub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vopmsub"); }
static inline void ee_d_vopmula(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vopmula"); }
static inline void ee_d_vrget(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrget"); }
static inline void ee_d_vrinit(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrinit"); }
static inline void ee_d_vrnext(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrnext"); }
static inline void ee_d_vrsqrt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrsqrt"); }
static inline void ee_d_vrxor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vrxor"); }
static inline void ee_d_vsqd(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsqd"); }
static inline void ee_d_vsqi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsqi"); }
static inline void ee_d_vsqrt(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsqrt"); }
static inline void ee_d_vsub(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsub"); }
static inline void ee_d_vsuba(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsuba"); }
static inline void ee_d_vsubai(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubai"); }
static inline void ee_d_vsubaq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubaq"); }
static inline void ee_d_vsubaw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubaw"); }
static inline void ee_d_vsubax(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubax"); }
static inline void ee_d_vsubay(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubay"); }
static inline void ee_d_vsubaz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubaz"); }
static inline void ee_d_vsubi(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubi"); }
static inline void ee_d_vsubq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubq"); }
static inline void ee_d_vsubw(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubw"); }
static inline void ee_d_vsubx(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubx"); }
static inline void ee_d_vsuby(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsuby"); }
static inline void ee_d_vsubz(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vsubz"); }
static inline void ee_d_vwaitq(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "vwaitq"); }
static inline void ee_d_xor(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, $%s", "xor", ee_cc_r[EE_D_RD], ee_cc_r[EE_D_RS], ee_cc_r[EE_D_RT]); }
static inline void ee_d_xori(uint32_t opcode) { ptr += sprintf(ptr, "%-8s $%s, $%s, %d", "xori", ee_cc_r[EE_D_RT], ee_cc_r[EE_D_RS], EE_D_I16); }
static inline void ee_d_invalid(uint32_t opcode) { ptr += sprintf(ptr, "%-8s", "<invalid>"); }

char *ee_disassemble(char *buf, uint32_t opcode, struct ee_dis_state *dis_state) {
    s = dis_state;

    ptr = buf;

    if (dis_state) if (dis_state->print_address)
        ptr += sprintf(ptr, "%08x: ", dis_state->pc);

    if (dis_state) if (dis_state->print_opcode)
        ptr += sprintf(ptr, "%08x ", opcode);

    switch (opcode & 0xFC000000) {
        case 0x00000000: { // special
            switch (opcode & 0x0000003F) {
                case 0x00000000: ee_d_sll(opcode); return buf;
                case 0x00000002: ee_d_srl(opcode); return buf;
                case 0x00000003: ee_d_sra(opcode); return buf;
                case 0x00000004: ee_d_sllv(opcode); return buf;
                case 0x00000006: ee_d_srlv(opcode); return buf;
                case 0x00000007: ee_d_srav(opcode); return buf;
                case 0x00000008: ee_d_jr(opcode); return buf;
                case 0x00000009: ee_d_jalr(opcode); return buf;
                case 0x0000000A: ee_d_movz(opcode); return buf;
                case 0x0000000B: ee_d_movn(opcode); return buf;
                case 0x0000000C: ee_d_syscall(opcode); return buf;
                case 0x0000000D: ee_d_break(opcode); return buf;
                case 0x0000000F: ee_d_sync(opcode); return buf;
                case 0x00000010: ee_d_mfhi(opcode); return buf;
                case 0x00000011: ee_d_mthi(opcode); return buf;
                case 0x00000012: ee_d_mflo(opcode); return buf;
                case 0x00000013: ee_d_mtlo(opcode); return buf;
                case 0x00000014: ee_d_dsllv(opcode); return buf;
                case 0x00000016: ee_d_dsrlv(opcode); return buf;
                case 0x00000017: ee_d_dsrav(opcode); return buf;
                case 0x00000018: ee_d_mult(opcode); return buf;
                case 0x00000019: ee_d_multu(opcode); return buf;
                case 0x0000001A: ee_d_div(opcode); return buf;
                case 0x0000001B: ee_d_divu(opcode); return buf;
                case 0x00000020: ee_d_add(opcode); return buf;
                case 0x00000021: ee_d_addu(opcode); return buf;
                case 0x00000022: ee_d_sub(opcode); return buf;
                case 0x00000023: ee_d_subu(opcode); return buf;
                case 0x00000024: ee_d_and(opcode); return buf;
                case 0x00000025: ee_d_or(opcode); return buf;
                case 0x00000026: ee_d_xor(opcode); return buf;
                case 0x00000027: ee_d_nor(opcode); return buf;
                case 0x00000028: ee_d_mfsa(opcode); return buf;
                case 0x00000029: ee_d_mtsa(opcode); return buf;
                case 0x0000002A: ee_d_slt(opcode); return buf;
                case 0x0000002B: ee_d_sltu(opcode); return buf;
                case 0x0000002C: ee_d_dadd(opcode); return buf;
                case 0x0000002D: ee_d_daddu(opcode); return buf;
                case 0x0000002E: ee_d_dsub(opcode); return buf;
                case 0x0000002F: ee_d_dsubu(opcode); return buf;
                case 0x00000030: ee_d_tge(opcode); return buf;
                case 0x00000031: ee_d_tgeu(opcode); return buf;
                case 0x00000032: ee_d_tlt(opcode); return buf;
                case 0x00000033: ee_d_tltu(opcode); return buf;
                case 0x00000034: ee_d_teq(opcode); return buf;
                case 0x00000036: ee_d_tne(opcode); return buf;
                case 0x00000038: ee_d_dsll(opcode); return buf;
                case 0x0000003A: ee_d_dsrl(opcode); return buf;
                case 0x0000003B: ee_d_dsra(opcode); return buf;
                case 0x0000003C: ee_d_dsll32(opcode); return buf;
                case 0x0000003E: ee_d_dsrl32(opcode); return buf;
                case 0x0000003F: ee_d_dsra32(opcode); return buf;
            }
        } break;
        case 0x04000000: { // regimm
            switch (opcode & 0x001F0000) {
                case 0x00000000: ee_d_bltz(opcode); return buf;
                case 0x00010000: ee_d_bgez(opcode); return buf;
                case 0x00020000: ee_d_bltzl(opcode); return buf;
                case 0x00030000: ee_d_bgezl(opcode); return buf;
                case 0x00080000: ee_d_tgei(opcode); return buf;
                case 0x00090000: ee_d_tgeiu(opcode); return buf;
                case 0x000A0000: ee_d_tlti(opcode); return buf;
                case 0x000B0000: ee_d_tltiu(opcode); return buf;
                case 0x000C0000: ee_d_teqi(opcode); return buf;
                case 0x000E0000: ee_d_tnei(opcode); return buf;
                case 0x00100000: ee_d_bltzal(opcode); return buf;
                case 0x00110000: ee_d_bgezal(opcode); return buf;
                case 0x00120000: ee_d_bltzall(opcode); return buf;
                case 0x00130000: ee_d_bgezall(opcode); return buf;
                case 0x00180000: ee_d_mtsab(opcode); return buf;
                case 0x00190000: ee_d_mtsah(opcode); return buf;
            }
        } break;
        case 0x08000000: ee_d_j(opcode); return buf;
        case 0x0C000000: ee_d_jal(opcode); return buf;
        case 0x10000000: ee_d_beq(opcode); return buf;
        case 0x14000000: ee_d_bne(opcode); return buf;
        case 0x18000000: ee_d_blez(opcode); return buf;
        case 0x1C000000: ee_d_bgtz(opcode); return buf;
        case 0x20000000: ee_d_addi(opcode); return buf;
        case 0x24000000: ee_d_addiu(opcode); return buf;
        case 0x28000000: ee_d_slti(opcode); return buf;
        case 0x2C000000: ee_d_sltiu(opcode); return buf;
        case 0x30000000: ee_d_andi(opcode); return buf;
        case 0x34000000: ee_d_ori(opcode); return buf;
        case 0x38000000: ee_d_xori(opcode); return buf;
        case 0x3C000000: ee_d_lui(opcode); return buf;
        case 0x40000000: { // cop0
            switch (opcode & 0x03E00000) {
                case 0x00000000: ee_d_mfc0(opcode); return buf;
                case 0x00800000: ee_d_mtc0(opcode); return buf;
                case 0x01000000: {
                    switch (opcode & 0x001F0000) {
                        case 0x00000000: ee_d_bc0f(opcode); return buf;
                        case 0x00010000: ee_d_bc0t(opcode); return buf;
                        case 0x00020000: ee_d_bc0fl(opcode); return buf;
                        case 0x00030000: ee_d_bc0tl(opcode); return buf;
                    }
                } break;
                case 0x02000000: {
                    switch (opcode & 0x0000003F) {
                    case 0x00000001: ee_d_tlbr(opcode); return buf;
                    case 0x00000002: ee_d_tlbwi(opcode); return buf;
                    case 0x00000006: ee_d_tlbwr(opcode); return buf;
                    case 0x00000008: ee_d_tlbp(opcode); return buf;
                    case 0x00000018: ee_d_eret(opcode); return buf;
                    case 0x00000038: ee_d_ei(opcode); return buf;
                    case 0x00000039: ee_d_di(opcode); return buf;
                    }
                } break;
            }
        } break;
        case 0x44000000: { // cop1
            switch (opcode & 0x03E00000) {
                case 0x00000000: ee_d_mfc1(opcode); return buf;
                case 0x00400000: ee_d_cfc1(opcode); return buf;
                case 0x00800000: ee_d_mtc1(opcode); return buf;
                case 0x00C00000: ee_d_ctc1(opcode); return buf;
                case 0x01000000: {
                    switch (opcode & 0x001F0000) {
                        case 0x00000000: ee_d_bc1f(opcode); return buf;
                        case 0x00010000: ee_d_bc1t(opcode); return buf;
                        case 0x00020000: ee_d_bc1fl(opcode); return buf;
                        case 0x00030000: ee_d_bc1tl(opcode); return buf;
                    }
                } break;
                case 0x02000000: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000000: ee_d_adds(opcode); return buf;
                        case 0x00000001: ee_d_subs(opcode); return buf;
                        case 0x00000002: ee_d_muls(opcode); return buf;
                        case 0x00000003: ee_d_divs(opcode); return buf;
                        case 0x00000004: ee_d_sqrts(opcode); return buf;
                        case 0x00000005: ee_d_abss(opcode); return buf;
                        case 0x00000006: ee_d_movs(opcode); return buf;
                        case 0x00000007: ee_d_negs(opcode); return buf;
                        case 0x00000016: ee_d_rsqrts(opcode); return buf;
                        case 0x00000018: ee_d_addas(opcode); return buf;
                        case 0x00000019: ee_d_subas(opcode); return buf;
                        case 0x0000001A: ee_d_mulas(opcode); return buf;
                        case 0x0000001C: ee_d_madds(opcode); return buf;
                        case 0x0000001D: ee_d_msubs(opcode); return buf;
                        case 0x0000001E: ee_d_maddas(opcode); return buf;
                        case 0x0000001F: ee_d_msubas(opcode); return buf;
                        case 0x00000024: ee_d_cvtw(opcode); return buf;
                        case 0x00000028: ee_d_maxs(opcode); return buf;
                        case 0x00000029: ee_d_mins(opcode); return buf;
                        case 0x00000030: ee_d_cf(opcode); return buf;
                        case 0x00000032: ee_d_ceq(opcode); return buf;
                        case 0x00000034: ee_d_clt(opcode); return buf;
                        case 0x00000036: ee_d_cle(opcode); return buf;
                    }
                } break;
                case 0x02800000: {
                    switch (opcode & 0x0000003F) {
                        case 0x00000020: ee_d_cvts(opcode); return buf;
                    }
                } break;
            }
        } break;
        case 0x48000000: { // cop2
            switch (opcode & 0x03E00000) {
                case 0x00200000: ee_d_qmfc2(opcode); return buf;
                case 0x00400000: ee_d_cfc2(opcode); return buf;
                case 0x00A00000: ee_d_qmtc2(opcode); return buf;
                case 0x00C00000: ee_d_ctc2(opcode); return buf;
                case 0x01000000: {
                    switch (opcode & 0x001F0000) {
                    case 0x00000000: ee_d_bc2f(opcode); return buf;
                    case 0x00010000: ee_d_bc2t(opcode); return buf;
                    case 0x00020000: ee_d_bc2fl(opcode); return buf;
                    case 0x00030000: ee_d_bc2tl(opcode); return buf;
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
                        case 0x00000000: ee_d_vaddx(opcode); return buf;
                        case 0x00000001: ee_d_vaddy(opcode); return buf;
                        case 0x00000002: ee_d_vaddz(opcode); return buf;
                        case 0x00000003: ee_d_vaddw(opcode); return buf;
                        case 0x00000004: ee_d_vsubx(opcode); return buf;
                        case 0x00000005: ee_d_vsuby(opcode); return buf;
                        case 0x00000006: ee_d_vsubz(opcode); return buf;
                        case 0x00000007: ee_d_vsubw(opcode); return buf;
                        case 0x00000008: ee_d_vmaddx(opcode); return buf;
                        case 0x00000009: ee_d_vmaddy(opcode); return buf;
                        case 0x0000000A: ee_d_vmaddz(opcode); return buf;
                        case 0x0000000B: ee_d_vmaddw(opcode); return buf;
                        case 0x0000000C: ee_d_vmsubx(opcode); return buf;
                        case 0x0000000D: ee_d_vmsuby(opcode); return buf;
                        case 0x0000000E: ee_d_vmsubz(opcode); return buf;
                        case 0x0000000F: ee_d_vmsubw(opcode); return buf;
                        case 0x00000010: ee_d_vmaxx(opcode); return buf;
                        case 0x00000011: ee_d_vmaxy(opcode); return buf;
                        case 0x00000012: ee_d_vmaxz(opcode); return buf;
                        case 0x00000013: ee_d_vmaxw(opcode); return buf;
                        case 0x00000014: ee_d_vminix(opcode); return buf;
                        case 0x00000015: ee_d_vminiy(opcode); return buf;
                        case 0x00000016: ee_d_vminiz(opcode); return buf;
                        case 0x00000017: ee_d_vminiw(opcode); return buf;
                        case 0x00000018: ee_d_vmulx(opcode); return buf;
                        case 0x00000019: ee_d_vmuly(opcode); return buf;
                        case 0x0000001A: ee_d_vmulz(opcode); return buf;
                        case 0x0000001B: ee_d_vmulw(opcode); return buf;
                        case 0x0000001C: ee_d_vmulq(opcode); return buf;
                        case 0x0000001D: ee_d_vmaxi(opcode); return buf;
                        case 0x0000001E: ee_d_vmuli(opcode); return buf;
                        case 0x0000001F: ee_d_vminii(opcode); return buf;
                        case 0x00000020: ee_d_vaddq(opcode); return buf;
                        case 0x00000021: ee_d_vmaddq(opcode); return buf;
                        case 0x00000022: ee_d_vaddi(opcode); return buf;
                        case 0x00000023: ee_d_vmaddi(opcode); return buf;
                        case 0x00000024: ee_d_vsubq(opcode); return buf;
                        case 0x00000025: ee_d_vmsubq(opcode); return buf;
                        case 0x00000026: ee_d_vsubi(opcode); return buf;
                        case 0x00000027: ee_d_vmsubi(opcode); return buf;
                        case 0x00000028: ee_d_vadd(opcode); return buf;
                        case 0x00000029: ee_d_vmadd(opcode); return buf;
                        case 0x0000002A: ee_d_vmul(opcode); return buf;
                        case 0x0000002B: ee_d_vmax(opcode); return buf;
                        case 0x0000002C: ee_d_vsub(opcode); return buf;
                        case 0x0000002D: ee_d_vmsub(opcode); return buf;
                        case 0x0000002E: ee_d_vopmsub(opcode); return buf;
                        case 0x0000002F: ee_d_vmini(opcode); return buf;
                        case 0x00000030: ee_d_viadd(opcode); return buf;
                        case 0x00000031: ee_d_visub(opcode); return buf;
                        case 0x00000032: ee_d_viaddi(opcode); return buf;
                        case 0x00000034: ee_d_viand(opcode); return buf;
                        case 0x00000035: ee_d_vior(opcode); return buf;
                        case 0x00000038: ee_d_vcallms(opcode); return buf;
                        case 0x00000039: ee_d_callmsr(opcode); return buf;
                        case 0x0000003C:
                        case 0x0000003D:
                        case 0x0000003E:
                        case 0x0000003F: {
                            uint32_t func = (opcode & 3) | ((opcode & 0x7c0) >> 4);

                            switch (func) {
                                case 0x00000000: ee_d_vaddax(opcode); return buf;
                                case 0x00000001: ee_d_vadday(opcode); return buf;
                                case 0x00000002: ee_d_vaddaz(opcode); return buf;
                                case 0x00000003: ee_d_vaddaw(opcode); return buf;
                                case 0x00000004: ee_d_vsubax(opcode); return buf;
                                case 0x00000005: ee_d_vsubay(opcode); return buf;
                                case 0x00000006: ee_d_vsubaz(opcode); return buf;
                                case 0x00000007: ee_d_vsubaw(opcode); return buf;
                                case 0x00000008: ee_d_vmaddax(opcode); return buf;
                                case 0x00000009: ee_d_vmadday(opcode); return buf;
                                case 0x0000000A: ee_d_vmaddaz(opcode); return buf;
                                case 0x0000000B: ee_d_vmaddaw(opcode); return buf;
                                case 0x0000000C: ee_d_vmsubax(opcode); return buf;
                                case 0x0000000D: ee_d_vmsubay(opcode); return buf;
                                case 0x0000000E: ee_d_vmsubaz(opcode); return buf;
                                case 0x0000000F: ee_d_vmsubaw(opcode); return buf;
                                case 0x00000010: ee_d_vitof0(opcode); return buf;
                                case 0x00000011: ee_d_vitof4(opcode); return buf;
                                case 0x00000012: ee_d_vitof12(opcode); return buf;
                                case 0x00000013: ee_d_vitof15(opcode); return buf;
                                case 0x00000014: ee_d_vftoi0(opcode); return buf;
                                case 0x00000015: ee_d_vftoi4(opcode); return buf;
                                case 0x00000016: ee_d_vftoi12(opcode); return buf;
                                case 0x00000017: ee_d_vftoi15(opcode); return buf;
                                case 0x00000018: ee_d_vmulax(opcode); return buf;
                                case 0x00000019: ee_d_vmulay(opcode); return buf;
                                case 0x0000001A: ee_d_vmulaz(opcode); return buf;
                                case 0x0000001B: ee_d_vmulaw(opcode); return buf;
                                case 0x0000001C: ee_d_vmulaq(opcode); return buf;
                                case 0x0000001D: ee_d_vabs(opcode); return buf;
                                case 0x0000001E: ee_d_vmulai(opcode); return buf;
                                case 0x0000001F: ee_d_vclipw(opcode); return buf;
                                case 0x00000020: ee_d_vaddaq(opcode); return buf;
                                case 0x00000021: ee_d_vmaddaq(opcode); return buf;
                                case 0x00000022: ee_d_vaddai(opcode); return buf;
                                case 0x00000023: ee_d_vmaddai(opcode); return buf;
                                case 0x00000024: ee_d_vsubaq(opcode); return buf;
                                case 0x00000025: ee_d_vmsubaq(opcode); return buf;
                                case 0x00000026: ee_d_vsubai(opcode); return buf;
                                case 0x00000027: ee_d_vmsubai(opcode); return buf;
                                case 0x00000028: ee_d_vadda(opcode); return buf;
                                case 0x00000029: ee_d_vmadda(opcode); return buf;
                                case 0x0000002A: ee_d_vmula(opcode); return buf;
                                case 0x0000002C: ee_d_vsuba(opcode); return buf;
                                case 0x0000002D: ee_d_vmsuba(opcode); return buf;
                                case 0x0000002E: ee_d_vopmula(opcode); return buf;
                                case 0x0000002F: ee_d_vnop(opcode); return buf;
                                case 0x00000030: ee_d_vmove(opcode); return buf;
                                case 0x00000031: ee_d_vmr32(opcode); return buf;
                                case 0x00000034: ee_d_vlqi(opcode); return buf;
                                case 0x00000035: ee_d_vsqi(opcode); return buf;
                                case 0x00000036: ee_d_vlqd(opcode); return buf;
                                case 0x00000037: ee_d_vsqd(opcode); return buf;
                                case 0x00000038: ee_d_vdiv(opcode); return buf;
                                case 0x00000039: ee_d_vsqrt(opcode); return buf;
                                case 0x0000003A: ee_d_vrsqrt(opcode); return buf;
                                case 0x0000003B: ee_d_vwaitq(opcode); return buf;
                                case 0x0000003C: ee_d_vmtir(opcode); return buf;
                                case 0x0000003D: ee_d_vmfir(opcode); return buf;
                                case 0x0000003E: ee_d_vilwr(opcode); return buf;
                                case 0x0000003F: ee_d_viswr(opcode); return buf;
                                case 0x00000040: ee_d_vrnext(opcode); return buf;
                                case 0x00000041: ee_d_vrget(opcode); return buf;
                                case 0x00000042: ee_d_vrinit(opcode); return buf;
                                case 0x00000043: ee_d_vrxor(opcode); return buf;
                            }
                        } break;
                    }
                } break;
            }
        } break;
        case 0x50000000: ee_d_beql(opcode); return buf;
        case 0x54000000: ee_d_bnel(opcode); return buf;
        case 0x58000000: ee_d_blezl(opcode); return buf;
        case 0x5C000000: ee_d_bgtzl(opcode); return buf;
        case 0x60000000: ee_d_daddi(opcode); return buf;
        case 0x64000000: ee_d_daddiu(opcode); return buf;
        case 0x68000000: ee_d_ldl(opcode); return buf;
        case 0x6C000000: ee_d_ldr(opcode); return buf;
        case 0x70000000: { // mmi
            switch (opcode & 0x0000003F) {
                case 0x00000000: ee_d_madd(opcode); return buf;
                case 0x00000001: ee_d_maddu(opcode); return buf;
                case 0x00000004: ee_d_plzcw(opcode); return buf;
                case 0x00000008: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000000: ee_d_paddw(opcode); return buf;
                        case 0x00000040: ee_d_psubw(opcode); return buf;
                        case 0x00000080: ee_d_pcgtw(opcode); return buf;
                        case 0x000000C0: ee_d_pmaxw(opcode); return buf;
                        case 0x00000100: ee_d_paddh(opcode); return buf;
                        case 0x00000140: ee_d_psubh(opcode); return buf;
                        case 0x00000180: ee_d_pcgth(opcode); return buf;
                        case 0x000001C0: ee_d_pmaxh(opcode); return buf;
                        case 0x00000200: ee_d_paddb(opcode); return buf;
                        case 0x00000240: ee_d_psubb(opcode); return buf;
                        case 0x00000280: ee_d_pcgtb(opcode); return buf;
                        case 0x00000400: ee_d_paddsw(opcode); return buf;
                        case 0x00000440: ee_d_psubsw(opcode); return buf;
                        case 0x00000480: ee_d_pextlw(opcode); return buf;
                        case 0x000004C0: ee_d_ppacw(opcode); return buf;
                        case 0x00000500: ee_d_paddsh(opcode); return buf;
                        case 0x00000540: ee_d_psubsh(opcode); return buf;
                        case 0x00000580: ee_d_pextlh(opcode); return buf;
                        case 0x000005C0: ee_d_ppach(opcode); return buf;
                        case 0x00000600: ee_d_paddsb(opcode); return buf;
                        case 0x00000640: ee_d_psubsb(opcode); return buf;
                        case 0x00000680: ee_d_pextlb(opcode); return buf;
                        case 0x000006C0: ee_d_ppacb(opcode); return buf;
                        case 0x00000780: ee_d_pext5(opcode); return buf;
                        case 0x000007C0: ee_d_ppac5(opcode); return buf;
                    }
                } break;
                case 0x00000009: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000000: ee_d_pmaddw(opcode); return buf;
                        case 0x00000080: ee_d_psllvw(opcode); return buf;
                        case 0x000000C0: ee_d_psrlvw(opcode); return buf;
                        case 0x00000100: ee_d_pmsubw(opcode); return buf;
                        case 0x00000200: ee_d_pmfhi(opcode); return buf;
                        case 0x00000240: ee_d_pmflo(opcode); return buf;
                        case 0x00000280: ee_d_pinth(opcode); return buf;
                        case 0x00000300: ee_d_pmultw(opcode); return buf;
                        case 0x00000340: ee_d_pdivw(opcode); return buf;
                        case 0x00000380: ee_d_pcpyld(opcode); return buf;
                        case 0x00000400: ee_d_pmaddh(opcode); return buf;
                        case 0x00000440: ee_d_phmadh(opcode); return buf;
                        case 0x00000480: ee_d_pand(opcode); return buf;
                        case 0x000004C0: ee_d_pxor(opcode); return buf;
                        case 0x00000500: ee_d_pmsubh(opcode); return buf;
                        case 0x00000540: ee_d_phmsbh(opcode); return buf;
                        case 0x00000680: ee_d_pexeh(opcode); return buf;
                        case 0x000006C0: ee_d_prevh(opcode); return buf;
                        case 0x00000700: ee_d_pmulth(opcode); return buf;
                        case 0x00000740: ee_d_pdivbw(opcode); return buf;
                        case 0x00000780: ee_d_pexew(opcode); return buf;
                        case 0x000007C0: ee_d_prot3w(opcode); return buf;
                    }
                } break;
                case 0x00000010: ee_d_mfhi1(opcode); return buf;
                case 0x00000011: ee_d_mthi1(opcode); return buf;
                case 0x00000012: ee_d_mflo1(opcode); return buf;
                case 0x00000013: ee_d_mtlo1(opcode); return buf;
                case 0x00000018: ee_d_mult1(opcode); return buf;
                case 0x00000019: ee_d_multu1(opcode); return buf;
                case 0x0000001A: ee_d_div1(opcode); return buf;
                case 0x0000001B: ee_d_divu1(opcode); return buf;
                case 0x00000020: ee_d_madd1(opcode); return buf;
                case 0x00000021: ee_d_maddu1(opcode); return buf;
                case 0x00000028: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000040: ee_d_pabsw(opcode); return buf;
                        case 0x00000080: ee_d_pceqw(opcode); return buf;
                        case 0x000000C0: ee_d_pminw(opcode); return buf;
                        case 0x00000100: ee_d_padsbh(opcode); return buf;
                        case 0x00000140: ee_d_pabsh(opcode); return buf;
                        case 0x00000180: ee_d_pceqh(opcode); return buf;
                        case 0x000001C0: ee_d_pminh(opcode); return buf;
                        case 0x00000280: ee_d_pceqb(opcode); return buf;
                        case 0x00000400: ee_d_padduw(opcode); return buf;
                        case 0x00000440: ee_d_psubuw(opcode); return buf;
                        case 0x00000480: ee_d_pextuw(opcode); return buf;
                        case 0x00000500: ee_d_padduh(opcode); return buf;
                        case 0x00000540: ee_d_psubuh(opcode); return buf;
                        case 0x00000580: ee_d_pextuh(opcode); return buf;
                        case 0x00000600: ee_d_paddub(opcode); return buf;
                        case 0x00000640: ee_d_psubub(opcode); return buf;
                        case 0x00000680: ee_d_pextub(opcode); return buf;
                        case 0x000006C0: ee_d_qfsrv(opcode); return buf;
                    }
                } break;
                case 0x00000029: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000000: ee_d_pmadduw(opcode); return buf;
                        case 0x000000C0: ee_d_psravw(opcode); return buf;
                        case 0x00000200: ee_d_pmthi(opcode); return buf;
                        case 0x00000240: ee_d_pmtlo(opcode); return buf;
                        case 0x00000280: ee_d_pinteh(opcode); return buf;
                        case 0x00000300: ee_d_pmultuw(opcode); return buf;
                        case 0x00000340: ee_d_pdivuw(opcode); return buf;
                        case 0x00000380: ee_d_pcpyud(opcode); return buf;
                        case 0x00000480: ee_d_por(opcode); return buf;
                        case 0x000004C0: ee_d_pnor(opcode); return buf;
                        case 0x00000680: ee_d_pexch(opcode); return buf;
                        case 0x000006C0: ee_d_pcpyh(opcode); return buf;
                        case 0x00000780: ee_d_pexcw(opcode); return buf;
                    }
                } break;
                case 0x00000030: {
                    switch (opcode & 0x000007C0) {
                        case 0x00000000: ee_d_pmfhllw(opcode); return buf;
                        case 0x00000040: ee_d_pmfhluw(opcode); return buf;
                        case 0x00000080: ee_d_pmfhlslw(opcode); return buf;
                        case 0x000000c0: ee_d_pmfhllh(opcode); return buf;
                        case 0x00000100: ee_d_pmfhlsh(opcode); return buf;
                    }
                } break;
                case 0x00000031: ee_d_pmthl(opcode); return buf;
                case 0x00000034: ee_d_psllh(opcode); return buf;
                case 0x00000036: ee_d_psrlh(opcode); return buf;
                case 0x00000037: ee_d_psrah(opcode); return buf;
                case 0x0000003C: ee_d_psllw(opcode); return buf;
                case 0x0000003E: ee_d_psrlw(opcode); return buf;
                case 0x0000003F: ee_d_psraw(opcode); return buf;
            }
        } break;
        case 0x78000000: ee_d_lq(opcode); return buf;
        case 0x7C000000: ee_d_sq(opcode); return buf;
        case 0x80000000: ee_d_lb(opcode); return buf;
        case 0x84000000: ee_d_lh(opcode); return buf;
        case 0x88000000: ee_d_lwl(opcode); return buf;
        case 0x8C000000: ee_d_lw(opcode); return buf;
        case 0x90000000: ee_d_lbu(opcode); return buf;
        case 0x94000000: ee_d_lhu(opcode); return buf;
        case 0x98000000: ee_d_lwr(opcode); return buf;
        case 0x9C000000: ee_d_lwu(opcode); return buf;
        case 0xA0000000: ee_d_sb(opcode); return buf;
        case 0xA4000000: ee_d_sh(opcode); return buf;
        case 0xA8000000: ee_d_swl(opcode); return buf;
        case 0xAC000000: ee_d_sw(opcode); return buf;
        case 0xB0000000: ee_d_sdl(opcode); return buf;
        case 0xB4000000: ee_d_sdr(opcode); return buf;
        case 0xB8000000: ee_d_swr(opcode); return buf;
        case 0xBC000000: ee_d_cache(opcode); return buf;
        case 0xC4000000: ee_d_lwc1(opcode); return buf;
        case 0xCC000000: ee_d_pref(opcode); return buf;
        case 0xD8000000: ee_d_lqc2(opcode); return buf;
        case 0xDC000000: ee_d_ld(opcode); return buf;
        case 0xE4000000: ee_d_swc1(opcode); return buf;
        case 0xF8000000: ee_d_sqc2(opcode); return buf;
        case 0xFC000000: ee_d_sd(opcode); return buf;
    }

    ee_d_invalid(opcode);
    
    return buf;
}