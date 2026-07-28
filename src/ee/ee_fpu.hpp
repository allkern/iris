#pragma once

#include <cstdint>
#include <asmjit/ujit.h>

namespace ee_fpu {

using asmjit::ujit::UniCompiler;
using asmjit::ujit::Gp;
using asmjit::ujit::Vec;
using asmjit::Imm;
using asmjit::ujit::cmp_eq;
using asmjit::ujit::cmp_ne;
using asmjit::ujit::test_nz;
using asmjit::ujit::ucmp_le;

static inline Gp cvtf(UniCompiler& uc, const Gp& x) {
    Gp exp = uc.new_gp32();
    Gp sign = uc.new_gp32();
    Gp inf = uc.new_gp32();
    Gp r1 = uc.new_gp32();
    Gp r2 = uc.new_gp32();

    uc.and_(exp, x, Imm(0x7f800000));
    uc.and_(sign, x, Imm(0x80000000u));
    uc.or_(inf, sign, Imm(0x7f7fffff));
    uc.select(r1, inf, x, cmp_eq(exp, Imm(0x7f800000)));
    uc.select(r2, sign, r1, cmp_eq(exp, Imm(0)));

    return r2;
}

static inline Gp clamp_result(UniCompiler& uc, const Gp& r, Gp& is_ovf, Gp& is_unf) {
    Gp rabs = uc.new_gp32();
    Gp rexp = uc.new_gp32();
    Gp rman = uc.new_gp32();
    Gp rsign = uc.new_gp32();
    Gp expz = uc.new_gp32();
    Gp manz = uc.new_gp32();
    Gp r_ovf = uc.new_gp32();
    Gp res1 = uc.new_gp32();
    Gp res = uc.new_gp32();
    is_ovf = uc.new_gp32();
    is_unf = uc.new_gp32();

    uc.and_(rabs, r, Imm(0x7fffffff));
    uc.and_(rexp, r, Imm(0x7f800000));
    uc.and_(rman, r, Imm(0x007fffff));
    uc.and_(rsign, r, Imm(0x80000000u));
    uc.select(is_ovf, Imm(1), Imm(0), cmp_eq(rabs, Imm(0x7f800000)));
    uc.select(expz, Imm(1), Imm(0), cmp_eq(rexp, Imm(0)));
    uc.select(manz, Imm(1), Imm(0), cmp_ne(rman, Imm(0)));
    uc.and_(is_unf, expz, manz);
    uc.or_(r_ovf, rsign, Imm(0x7f7fffff));
    uc.select(res1, rsign, r, test_nz(is_unf));
    uc.select(res, r_ovf, res1, test_nz(is_ovf));

    return res;
}

enum Op { ADD, SUB, MUL, DIV };

static inline Gp arith(UniCompiler& uc, Op op, const Gp& fs, const Gp& ft) {
    Gp cs = cvtf(uc, fs);
    Gp ct = cvtf(uc, ft);

    Vec xs = uc.new_vec128();
    Vec xt = uc.new_vec128();
    Vec xr = uc.new_vec128();
    Gp r = uc.new_gp32();

    uc.s_mov_u32(xs, cs);
    uc.s_mov_u32(xt, ct);

    switch (op) {
        case ADD: uc.s_add_f32(xr, xs, xt); break;
        case SUB: uc.s_sub_f32(xr, xs, xt); break;
        case MUL: uc.s_mul_f32(xr, xs, xt); break;
        case DIV: uc.s_div_f32(xr, xs, xt); break;
    }

    uc.s_mov_u32(r, xr);

    return r;
}

static inline Gp arith_raw(UniCompiler& uc, Op op, const Gp& x, const Gp& y) {
    Vec xs = uc.new_vec128();
    Vec xt = uc.new_vec128();
    Vec xr = uc.new_vec128();
    Gp r = uc.new_gp32();

    uc.s_mov_u32(xs, x);
    uc.s_mov_u32(xt, y);

    switch (op) {
        case ADD: uc.s_add_f32(xr, xs, xt); break;
        case SUB: uc.s_sub_f32(xr, xs, xt); break;
        case MUL: uc.s_mul_f32(xr, xs, xt); break;
        case DIV: uc.s_div_f32(xr, xs, xt); break;
    }

    uc.s_mov_u32(r, xr);

    return r;
}

static inline void flag_clamp(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& r) {
    Gp is_ovf, is_unf;
    Gp res = clamp_result(uc, r, is_ovf, is_unf);

    Gp fcr_noovf = uc.new_gp32();
    Gp fcr_unf = uc.new_gp32();
    Gp fcr_nounf = uc.new_gp32();
    Gp fcr_else = uc.new_gp32();
    Gp fcr_ovf = uc.new_gp32();
    Gp fcr_out = uc.new_gp32();

    uc.mov(out, res);
    uc.and_(fcr_noovf, fcr, Imm(~(uint32_t)FPU_FLG_O));
    uc.or_(fcr_unf, fcr_noovf, Imm(FPU_FLG_U | FPU_FLG_SU));
    uc.and_(fcr_nounf, fcr_noovf, Imm(~(uint32_t)FPU_FLG_U));
    uc.select(fcr_else, fcr_unf, fcr_nounf, test_nz(is_unf));
    uc.or_(fcr_ovf, fcr, Imm(FPU_FLG_O | FPU_FLG_SO));
    uc.select(fcr_out, fcr_ovf, fcr_else, test_nz(is_ovf));
    uc.mov(fcr, fcr_out);
}

static inline void binop(UniCompiler& uc, Op op, const Gp& out, const Gp& fcr, const Gp& fs, const Gp& ft) {
    flag_clamp(uc, out, fcr, arith(uc, op, fs, ft));
}

static inline void madds(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& acc, const Gp& fs, const Gp& ft) {
    Gp temp = arith(uc, MUL, fs, ft);

    binop(uc, ADD, out, fcr, acc, temp);
}

static inline void msubs(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& acc, const Gp& fs, const Gp& ft) {
    Gp temp = arith(uc, MUL, fs, ft);

    binop(uc, SUB, out, fcr, acc, temp);
}

static inline void maddas(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& acc, const Gp& fs, const Gp& ft) {
    Gp prod = arith(uc, MUL, fs, ft);

    flag_clamp(uc, out, fcr, arith_raw(uc, ADD, acc, prod));
}

static inline void msubas(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& acc, const Gp& fs, const Gp& ft) {
    Gp prod = arith(uc, MUL, fs, ft);

    flag_clamp(uc, out, fcr, arith_raw(uc, SUB, acc, prod));
}

static inline void sqrts(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& ft) {
    Gp exp = uc.new_gp32();
    Gp sign = uc.new_gp32();
    Gp absf = uc.new_gp32();
    Vec xv = uc.new_vec128();
    Gp sq = uc.new_gp32();
    Gp res = uc.new_gp32();
    Gp withI = uc.new_gp32();
    Gp t = uc.new_gp32();
    Gp fcr_out = uc.new_gp32();

    uc.and_(fcr, fcr, Imm(~(uint32_t)(FPU_FLG_I | FPU_FLG_D)));
    uc.and_(exp, ft, Imm(0x7f800000));
    uc.and_(sign, ft, Imm(0x80000000u));

    Gp cf = cvtf(uc, ft);

    uc.and_(absf, cf, Imm(0x7fffffff));
    uc.s_mov_u32(xv, absf);
    uc.s_sqrt_f32(xv, xv);
    uc.s_mov_u32(sq, xv);
    uc.select(res, sign, sq, cmp_eq(exp, Imm(0)));
    uc.mov(out, res);

    uc.or_(withI, fcr, Imm(FPU_FLG_I | FPU_FLG_SI));
    uc.select(t, withI, fcr, test_nz(ft, Imm(0x80000000u)));
    uc.select(fcr_out, t, fcr, test_nz(exp, Imm(0x7f800000)));
    uc.mov(fcr, fcr_out);
}

static inline void rsqrts(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& fs, const Gp& ft) {
    Gp exp = uc.new_gp32();
    Gp absf = uc.new_gp32();
    Vec xv = uc.new_vec128();
    Gp sq = uc.new_gp32();
    Gp zsign = uc.new_gp32();
    Gp zmax = uc.new_gp32();
    Gp res = uc.new_gp32();
    Gp withD = uc.new_gp32();
    Gp withI = uc.new_gp32();
    Gp inner = uc.new_gp32();
    Gp fcr_out = uc.new_gp32();
    Gp is_ovf, is_unf;

    uc.and_(fcr, fcr, Imm(~(uint32_t)(FPU_FLG_I | FPU_FLG_D)));
    uc.and_(exp, ft, Imm(0x7f800000));

    Gp cft = cvtf(uc, ft);

    uc.and_(absf, cft, Imm(0x7fffffff));
    uc.s_mov_u32(xv, absf);
    uc.s_sqrt_f32(xv, xv);
    uc.s_mov_u32(sq, xv);

    Gp cfs = cvtf(uc, fs);
    Gp v = arith_raw(uc, DIV, cfs, sq);
    Gp vc = clamp_result(uc, v, is_ovf, is_unf);

    uc.and_(zsign, ft, Imm(0x80000000u));
    uc.or_(zmax, zsign, Imm(0x7f7fffff));
    uc.select(res, zmax, vc, cmp_eq(exp, Imm(0)));
    uc.mov(out, res);

    uc.or_(withD, fcr, Imm(FPU_FLG_D | FPU_FLG_SD));
    uc.or_(withI, fcr, Imm(FPU_FLG_I | FPU_FLG_SI));
    uc.select(inner, withI, fcr, test_nz(ft, Imm(0x80000000u)));
    uc.select(fcr_out, withD, inner, cmp_eq(exp, Imm(0)));
    uc.mov(fcr, fcr_out);
}

static inline Gp maxmin(UniCompiler& uc, bool is_max, const Gp& fs, const Gp& ft) {
    Gp hi = uc.new_gp32();
    Gp lo = uc.new_gp32();
    Gp both = uc.new_gp32();
    Gp out = uc.new_gp32();

    uc.smax(hi, fs, ft);
    uc.smin(lo, fs, ft);
    uc.and_(both, fs, ft);

    if (is_max) {
        uc.select(out, lo, hi, test_nz(both, Imm(0x80000000u)));
    } else {
        uc.select(out, hi, lo, test_nz(both, Imm(0x80000000u)));
    }

    return out;
}

static inline void maxs(UniCompiler& uc, const Gp& out, const Gp& fs, const Gp& ft) { uc.mov(out, maxmin(uc, true,  fs, ft)); }
static inline void mins(UniCompiler& uc, const Gp& out, const Gp& fs, const Gp& ft) { uc.mov(out, maxmin(uc, false, fs, ft)); }

static inline void adds(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& fs, const Gp& ft) { binop(uc, ADD, out, fcr, fs, ft); }
static inline void subs(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& fs, const Gp& ft) { binop(uc, SUB, out, fcr, fs, ft); }
static inline void muls(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& fs, const Gp& ft) { binop(uc, MUL, out, fcr, fs, ft); }

static inline void divs(UniCompiler& uc, const Gp& out, const Gp& fcr, const Gp& fs, const Gp& ft) {
    Gp t_exp = uc.new_gp32();
    Gp s_exp = uc.new_gp32();
    Gp t_zero = uc.new_gp32();
    Gp s_zero = uc.new_gp32();
    Gp xr = uc.new_gp32();
    Gp res = uc.new_gp32();
    Gp fcr_base = uc.new_gp32();
    Gp fcr_i = uc.new_gp32();
    Gp fcr_d = uc.new_gp32();
    Gp fcr_tz = uc.new_gp32();
    Gp fcr_out = uc.new_gp32();
    Gp is_ovf, is_unf;

    uc.and_(t_exp, ft, Imm(0x7f800000));
    uc.and_(s_exp, fs, Imm(0x7f800000));
    uc.select(t_zero, Imm(1), Imm(0), cmp_eq(t_exp, Imm(0)));
    uc.select(s_zero, Imm(1), Imm(0), cmp_eq(s_exp, Imm(0)));

    uc.xor_(xr, ft, fs);
    uc.and_(xr, xr, Imm(0x80000000u));
    uc.or_(xr, xr, Imm(0x7f7fffff));

    Gp rn = arith(uc, DIV, fs, ft);
    Gp rn_clamped = clamp_result(uc, rn, is_ovf, is_unf);

    uc.select(res, xr, rn_clamped, test_nz(t_zero));
    uc.mov(out, res);

    uc.and_(fcr_base, fcr, Imm(~(uint32_t)(FPU_FLG_I | FPU_FLG_D)));
    uc.or_(fcr_i, fcr_base, Imm(FPU_FLG_I | FPU_FLG_SI));
    uc.or_(fcr_d, fcr_base, Imm(FPU_FLG_D | FPU_FLG_SD));
    uc.select(fcr_tz, fcr_i, fcr_d, test_nz(s_zero));
    uc.select(fcr_out, fcr_tz, fcr_base, test_nz(t_zero));

    uc.mov(fcr, fcr_out);
}

static inline void cf(UniCompiler& uc, const Gp& fcr) {
    uc.and_(fcr, fcr, Imm(~(uint32_t)FPU_FLG_C));
}

enum Cmp { LT, LE, EQ };

static inline void ccond(UniCompiler& uc, Cmp c, const Gp& fcr, const Gp& fs, const Gp& ft) {
    Gp cs = cvtf(uc, fs);
    Gp ct = cvtf(uc, ft);

    Vec xs = uc.new_vec128();
    Vec xt = uc.new_vec128();
    Vec mask = uc.new_vec128();
    Gp m = uc.new_gp32();
    Gp fcr_set = uc.new_gp32();
    Gp fcr_clr = uc.new_gp32();
    Gp fcr_out = uc.new_gp32();

    uc.s_mov_u32(xs, cs);
    uc.s_mov_u32(xt, ct);

    switch (c) {
        case LT: uc.v_cmp_lt_f32(mask, xs, xt); break;
        case LE: uc.v_cmp_le_f32(mask, xs, xt); break;
        case EQ: uc.v_cmp_eq_f32(mask, xs, xt); break;
    }

    uc.s_mov_u32(m, mask);
    uc.or_(fcr_set, fcr, Imm(FPU_FLG_C));
    uc.and_(fcr_clr, fcr, Imm(~(uint32_t)FPU_FLG_C));
    uc.select(fcr_out, fcr_set, fcr_clr, test_nz(m));
    uc.mov(fcr, fcr_out);
}

static inline void clt(UniCompiler& uc, const Gp& fcr, const Gp& fs, const Gp& ft) { ccond(uc, LT, fcr, fs, ft); }
static inline void cle(UniCompiler& uc, const Gp& fcr, const Gp& fs, const Gp& ft) { ccond(uc, LE, fcr, fs, ft); }
static inline void ceq(UniCompiler& uc, const Gp& fcr, const Gp& fs, const Gp& ft) { ccond(uc, EQ, fcr, fs, ft); }

static inline void cvtsw(UniCompiler& uc, const Gp& out, const Gp& fs) {
    Vec xf = uc.new_vec128();
    Gp r = uc.new_gp32();
    Gp exp = uc.new_gp32();
    Gp sign = uc.new_gp32();
    Gp inf = uc.new_gp32();
    Gp r1 = uc.new_gp32();
    Gp r2 = uc.new_gp32();

    uc.s_cvt_int_to_f32(xf, fs);
    uc.s_mov_u32(r, xf);
    uc.and_(exp, r, Imm(0x7f800000));
    uc.and_(sign, r, Imm(0x80000000u));
    uc.or_(inf, sign, Imm(0x7f7fffff));
    uc.select(r1, inf, r, cmp_eq(exp, Imm(0x7f800000)));
    uc.select(r2, sign, r1, cmp_eq(exp, Imm(0)));

    uc.mov(out, r2);
}

static inline void cvtws(UniCompiler& uc, const Gp& out, const Gp& fs) {
    Gp sexp = uc.new_gp32();
    Gp sign = uc.new_gp32();
    Vec xcs = uc.new_vec128();
    Gp trunc = uc.new_gp32();
    Gp ovf = uc.new_gp32();
    Gp res = uc.new_gp32();

    uc.and_(sexp, fs, Imm(0x7f800000));
    uc.and_(sign, fs, Imm(0x80000000u));

    Gp cs = cvtf(uc, fs);

    uc.s_mov_u32(xcs, cs);
    uc.s_cvt_trunc_f32_to_int(trunc, xcs);
    uc.select(ovf, Imm(0x80000000u), Imm(0x7fffffff), test_nz(sign));
    uc.select(res, trunc, ovf, ucmp_le(sexp, Imm(0x4e800000)));
    uc.mov(out, res);
}

}
