#pragma once

#include <asmjit/ujit.h>

namespace ee_mmi {

using asmjit::ujit::UniCompiler;
using asmjit::ujit::Vec;
using asmjit::ujit::Gp;
using asmjit::ujit::swizzle;
using asmjit::ujit::cmp_eq;
using asmjit::Imm;

static inline void mmi_sext32(UniCompiler& uc, const Gp& dst, const Gp& src) {
#if defined(ASMJIT_UJIT_AARCH64)
    uc.cc->sxtw(dst.r64(), src.r32());
#else
    uc.cc->movsxd(dst.r64(), src.r32());
#endif
}

static inline void paddb(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_add_u8(d, s, t); }
static inline void paddh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_add_u16(d, s, t); }
static inline void paddw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_add_u32(d, s, t); }
static inline void psubb(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_sub_u8(d, s, t); }
static inline void psubh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_sub_u16(d, s, t); }
static inline void psubw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_sub_u32(d, s, t); }
static inline void paddsb(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_adds_i8(d, s, t); }
static inline void paddsh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_adds_i16(d, s, t); }
static inline void psubsb(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_subs_i8(d, s, t); }
static inline void psubsh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_subs_i16(d, s, t); }
static inline void paddub(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_adds_u8(d, s, t); }
static inline void padduh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_adds_u16(d, s, t); }
static inline void psubub(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_subs_u8(d, s, t); }
static inline void psubuh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_subs_u16(d, s, t); }

static inline void padduw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec na = uc.new_vec128();
    Vec m = uc.new_vec128();

    uc.v_not_u32(na, s);
    uc.v_min_u32(m, t, na);
    uc.v_add_u32(d, s, m);
}

static inline void psubuw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec m = uc.new_vec128();

    uc.v_min_u32(m, s, t);
    uc.v_sub_u32(d, s, m);
}

static inline Vec mmi_sat32(UniCompiler& uc, const Vec& s) {
    Vec sgn = uc.new_vec128();
    Vec mx = uc.new_vec128();

    uc.v_srai_i32(sgn, s, 31);
    uc.v_ones_i(mx);
    uc.v_srli_u32(mx, mx, 1);
    uc.v_xor_u32(sgn, sgn, mx);

    return sgn;
}

static inline void paddsw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec sum = uc.new_vec128();
    Vec axb = uc.new_vec128();
    Vec axs = uc.new_vec128();
    Vec ovf = uc.new_vec128();

    uc.v_add_u32(sum, s, t);
    uc.v_xor_u32(axb, s, t);
    uc.v_xor_u32(axs, s, sum);
    uc.v_bic_u32(ovf, axs, axb);
    uc.v_srai_i32(ovf, ovf, 31);

    Vec sat = mmi_sat32(uc, s);

    uc.v_blendv_u8(d, sum, sat, ovf);
}

static inline void psubsw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec dif = uc.new_vec128();
    Vec axb = uc.new_vec128();
    Vec axd = uc.new_vec128();
    Vec ovf = uc.new_vec128();

    uc.v_sub_u32(dif, s, t);
    uc.v_xor_u32(axb, s, t);
    uc.v_xor_u32(axd, s, dif);
    uc.v_and_u32(ovf, axb, axd);
    uc.v_srai_i32(ovf, ovf, 31);

    Vec sat = mmi_sat32(uc, s);

    uc.v_blendv_u8(d, dif, sat, ovf);
}

static inline void padsbh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec sub = uc.new_vec128();
    Vec add = uc.new_vec128();

    uc.v_sub_u16(sub, s, t);
    uc.v_add_u16(add, s, t);
    uc.v_swap_u64(add, add);
    uc.v_interleave_lo_u64(d, sub, add);
}

static inline void pand(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_and_u32(d, s, t); }
static inline void por(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_or_u32(d, s, t); }
static inline void pxor(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_xor_u32(d, s, t); }
static inline void pnor(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_or_u32(d, s, t); uc.v_not_u32(d, d); }
static inline void pceqb(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_cmp_eq_u8(d, s, t); }
static inline void pceqh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_cmp_eq_u16(d, s, t); }
static inline void pceqw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_cmp_eq_u32(d, s, t); }
static inline void pcgtb(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_cmp_gt_i8(d, s, t); }
static inline void pcgth(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_cmp_gt_i16(d, s, t); }
static inline void pcgtw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_cmp_gt_i32(d, s, t); }
static inline void pmaxh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_max_i16(d, s, t); }
static inline void pmaxw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_max_i32(d, s, t); }
static inline void pminh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_min_i16(d, s, t); }
static inline void pminw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_min_i32(d, s, t); }
static inline void pextlb(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_interleave_lo_u8(d, t, s); }
static inline void pextlh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_interleave_lo_u16(d, t, s); }
static inline void pextlw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_interleave_lo_u32(d, t, s); }
static inline void pextub(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_interleave_hi_u8(d, t, s); }
static inline void pextuh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_interleave_hi_u16(d, t, s); }
static inline void pextuw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_interleave_hi_u32(d, t, s); }
static inline void pcpyld(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_interleave_lo_u64(d, t, s); }
static inline void pcpyud(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { uc.v_interleave_hi_u64(d, s, t); }
static inline void pinth(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec hi = uc.new_vec128();

    uc.v_srlb_u128(hi, s, 8);
    uc.v_interleave_lo_u16(d, t, hi);
}

static inline void pinteh(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec m = uc.new_vec128();
    Vec tm = uc.new_vec128();
    Vec ss = uc.new_vec128();

    uc.v_ones_i(m);
    uc.v_srli_u32(m, m, 16);
    uc.v_and_u32(tm, t, m);
    uc.v_slli_u32(ss, s, 16);
    uc.v_or_u32(d, tm, ss);
}

static inline void ppacb(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec m = uc.new_vec128();
    Vec tm = uc.new_vec128();
    Vec sm = uc.new_vec128();

    uc.v_ones_i(m);
    uc.v_srli_u16(m, m, 8);
    uc.v_and_u32(tm, t, m);
    uc.v_and_u32(sm, s, m);
    uc.v_packs_i16_u8(d, tm, sm);
}

static inline void ppach(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec m = uc.new_vec128();
    Vec tm = uc.new_vec128();
    Vec sm = uc.new_vec128();

    uc.v_ones_i(m);
    uc.v_srli_u32(m, m, 16);
    uc.v_and_u32(tm, t, m);
    uc.v_and_u32(sm, s, m);
    uc.v_packs_i32_u16(d, tm, sm);
}

static inline void ppacw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) {
    Vec tt = uc.new_vec128();
    Vec st = uc.new_vec128();

    uc.v_swizzle_u32x4(tt, t, swizzle(3, 1, 2, 0));
    uc.v_swizzle_u32x4(st, s, swizzle(3, 1, 2, 0));
    uc.v_interleave_lo_u64(d, tt, st);
}

static inline void pcpyh(UniCompiler& uc, const Vec& d, const Vec& t) {
    uc.v_swizzle_lo_u16x4(d, t, swizzle(0, 0, 0, 0));
    uc.v_swizzle_hi_u16x4(d, d, swizzle(0, 0, 0, 0));
}

static inline void pexeh(UniCompiler& uc, const Vec& d, const Vec& t) {
    uc.v_swizzle_lo_u16x4(d, t, swizzle(3, 0, 1, 2));
    uc.v_swizzle_hi_u16x4(d, d, swizzle(3, 0, 1, 2));
}

static inline void prevh(UniCompiler& uc, const Vec& d, const Vec& t) {
    uc.v_swizzle_lo_u16x4(d, t, swizzle(0, 1, 2, 3));
    uc.v_swizzle_hi_u16x4(d, d, swizzle(0, 1, 2, 3));
}

static inline void pexch(UniCompiler& uc, const Vec& d, const Vec& t) {
    uc.v_swizzle_lo_u16x4(d, t, swizzle(3, 1, 2, 0));
    uc.v_swizzle_hi_u16x4(d, d, swizzle(3, 1, 2, 0));
}

static inline void pexew(UniCompiler& uc, const Vec& d, const Vec& t) { uc.v_swizzle_u32x4(d, t, swizzle(3, 0, 1, 2)); }
static inline void pexcw(UniCompiler& uc, const Vec& d, const Vec& t) { uc.v_swizzle_u32x4(d, t, swizzle(3, 1, 2, 0)); }
static inline void prot3w(UniCompiler& uc, const Vec& d, const Vec& t) { uc.v_swizzle_u32x4(d, t, swizzle(3, 0, 2, 1)); }

static inline void pabsh(UniCompiler& uc, const Vec& d, const Vec& t) {
    Vec k = uc.new_vec128();
    Vec eq = uc.new_vec128();
    Vec ab = uc.new_vec128();

    uc.v_ones_i(k);
    uc.v_slli_u16(k, k, 15);
    uc.v_cmp_eq_u16(eq, t, k);
    uc.v_abs_i16(ab, t);
    uc.v_add_u16(d, ab, eq);
}
static inline void pabsw(UniCompiler& uc, const Vec& d, const Vec& t) {
    Vec k = uc.new_vec128();
    Vec eq = uc.new_vec128();
    Vec ab = uc.new_vec128();

    uc.v_ones_i(k);
    uc.v_slli_u32(k, k, 31);
    uc.v_cmp_eq_u32(eq, t, k);
    uc.v_abs_i32(ab, t);
    uc.v_add_u32(d, ab, eq);
}

static inline void pshiftv(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t, int mode) {
    Gp r[2];

    for (int k = 0; k < 2; k++) {
        const int lane = k * 2;

        Gp val = uc.new_gp32();
        Gp amt = uc.new_gp32();

        uc.s_extract_u32(val, t, lane);
        uc.s_extract_u32(amt, s, lane);
        uc.and_(amt, amt, Imm(31));

        Gp sh = uc.new_gp32();

        if (mode == 0) {
            uc.shl(sh, val, amt);
        } else if (mode == 1) {
            uc.shr(sh, val, amt);
        } else {
            uc.sar(sh, val, amt);
        }

        r[k] = uc.new_gp64();

        mmi_sext32(uc, r[k], sh);
    }

    uc.s_mov_u64(d, r[0]);
    uc.s_insert_u64(d, r[1], 1);
}

static inline void plzcw(UniCompiler& uc, const Vec& d, const Vec& t) {
    Gp c[2];

    for (int j = 0; j < 2; j++) {
        Gp w = uc.new_gp32();
        Gp s = uc.new_gp32();
        Gp wp = uc.new_gp32();
        Gp clzv = uc.new_gp32();
        Gp m1 = uc.new_gp32();
        Gp cnt = uc.new_gp32();

        uc.s_extract_u32(w, t, j);
        uc.sar(s, w, Imm(31));
        uc.xor_(wp, w, s);
        uc.clz(clzv, wp);
        uc.sub(m1, clzv, Imm(1));
        uc.select(cnt, Imm(31), m1, cmp_eq(wp, Imm(0)));

        c[j] = cnt;
    }

    Gp z = uc.new_gp32();

    uc.mov(z, Imm(0));
    uc.s_mov_u32(d, c[0]);
    uc.s_insert_u32(d, c[1], 1);
    uc.s_insert_u32(d, z, 2);
    uc.s_insert_u32(d, z, 3);
}

static inline void psllvw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { pshiftv(uc, d, s, t, 0); }
static inline void psrlvw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { pshiftv(uc, d, s, t, 1); }
static inline void psravw(UniCompiler& uc, const Vec& d, const Vec& s, const Vec& t) { pshiftv(uc, d, s, t, 2); }
static inline void psllh(UniCompiler& uc, const Vec& d, const Vec& t, uint32_t sa) { uc.v_slli_u16(d, t, sa & 0xf); }
static inline void psllw(UniCompiler& uc, const Vec& d, const Vec& t, uint32_t sa) { uc.v_slli_u32(d, t, sa & 0x1f); }
static inline void psrlh(UniCompiler& uc, const Vec& d, const Vec& t, uint32_t sa) { uc.v_srli_u16(d, t, sa & 0xf); }
static inline void psrlw(UniCompiler& uc, const Vec& d, const Vec& t, uint32_t sa) { uc.v_srli_u32(d, t, sa & 0x1f); }
static inline void psrah(UniCompiler& uc, const Vec& d, const Vec& t, uint32_t sa) { uc.v_srai_i16(d, t, sa & 0xf); }
static inline void psraw(UniCompiler& uc, const Vec& d, const Vec& t, uint32_t sa) { uc.v_srai_i32(d, t, sa & 0x1f); }

static inline void mmi_gather_even_u32(UniCompiler& uc, const Vec& d, const Vec& s) {
    uc.v_swizzle_u32x4(d, s, swizzle(3, 2, 2, 0));
}

static inline void mmi_gather_odd_u32(UniCompiler& uc, const Vec& d, const Vec& s) {
    uc.v_swizzle_u32x4(d, s, swizzle(3, 3, 3, 1));
}

static inline Vec mmi_splat32(UniCompiler& uc, uint32_t c) {
    Gp g = uc.new_gp32();
    Vec v = uc.new_vec128();

    uc.mov(g, Imm(c));
    uc.s_mov_u32(v, g);
    uc.v_swizzle_u32x4(v, v, swizzle(0, 0, 0, 0));

    return v;
}

static inline void mmi_spill_words_to_hilo(UniCompiler& uc, const Vec& hi, const Vec& lo, const Vec& rd) {
    Vec e = uc.new_vec128();

    mmi_gather_even_u32(uc, e, rd);

    uc.v_cvt_i32_lo_to_i64(lo, e);
    uc.v_swizzle_u32x4(e, rd, swizzle(3, 3, 3, 1));
    uc.v_cvt_i32_lo_to_i64(hi, e);
}

static inline void pmfhi(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec&, const Vec&, const Vec&) { uc.v_mov(rd, hi); }
static inline void pmflo(UniCompiler& uc, const Vec& rd, const Vec&, const Vec& lo, const Vec&, const Vec&) { uc.v_mov(rd, lo); }
static inline void pmfhllw(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec&, const Vec&) {
    Vec le = uc.new_vec128();
    Vec he = uc.new_vec128();

    mmi_gather_even_u32(uc, le, lo);
    mmi_gather_even_u32(uc, he, hi);

    uc.v_interleave_lo_u32(rd, le, he);
}

static inline void pmfhlsh(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec&, const Vec&) {
    Vec a = uc.new_vec128();
    Vec b = uc.new_vec128();

    uc.v_interleave_lo_u64(a, lo, hi);
    uc.v_interleave_hi_u64(b, lo, hi);
    uc.v_packs_i32_i16(rd, a, b);
}

static inline void pmfhllh(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec&, const Vec&) {
    auto pack = [&](const Vec& src, int base) -> Gp {
        Gp a = uc.new_gp32();
        Gp b = uc.new_gp32();

        uc.s_extract_u32(a, src, base);
        uc.s_extract_u32(b, src, base + 1);
        uc.and_(a, a, Imm(0xffff));
        uc.and_(b, b, Imm(0xffff));
        uc.shl(b, b, Imm(16));
        uc.or_(a, a, b);

        return a;
    };

    Gp w0 = pack(lo, 0);
    Gp w1 = pack(hi, 0);
    Gp w2 = pack(lo, 2);
    Gp w3 = pack(hi, 2);

    uc.s_mov_u32(rd, w0);
    uc.s_insert_u32(rd, w1, 1);
    uc.s_insert_u32(rd, w2, 2);
    uc.s_insert_u32(rd, w3, 3);
}

static inline void pmfhlslw(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec&, const Vec&) {
    Gp r[2];

    for (int k = 0; k < 2; k++) {
        const int w = k * 2;

        Gp low = uc.new_gp32();
        Gp hiw = uc.new_gp32();
        Gp acc = uc.new_gp64();
        Gp t = uc.new_gp64();

        uc.s_extract_u32(low, lo, w);
        uc.s_extract_u32(hiw, hi, w);
        uc.mov(acc.r32(), low);
        uc.mov(t.r32(), hiw);
        uc.shl(t, t, Imm(32));
        uc.or_(acc, acc, t);
        uc.smin(acc, acc, Imm((int64_t)0x7fffffff));
        uc.smax(acc, acc, Imm((int64_t)(int32_t)0x80000000));

        r[k] = acc;
    }

    uc.s_mov_u64(rd, r[0]);
    uc.s_insert_u64(rd, r[1], 1);
}

static inline void pmthi(UniCompiler& uc, const Vec&, const Vec& hi, const Vec&, const Vec& rs, const Vec&) { uc.v_mov(hi, rs); }
static inline void pmtlo(UniCompiler& uc, const Vec&, const Vec&, const Vec& lo, const Vec& rs, const Vec&) { uc.v_mov(lo, rs); }

static inline void pmthl(UniCompiler& uc, const Vec&, const Vec& hi, const Vec& lo, const Vec& rs, const Vec&) {
    Vec m = uc.new_vec128();
    Vec sh = uc.new_vec128();

    uc.v_ones_i(m);
    uc.v_srli_u64(m, m, 32);
    uc.v_blendv_u8(lo, lo, rs, m);
    uc.v_swizzle_u32x4(sh, rs, swizzle(3, 3, 1, 1));
    uc.v_blendv_u8(hi, hi, sh, m);
}

static inline void mmi_umul_even(UniCompiler& uc, const Vec& d, const Vec& a, const Vec& b) {
    Vec m = uc.new_vec128();
    Vec am = uc.new_vec128();

    uc.v_ones_i(m);
    uc.v_srli_u64(m, m, 32);
    uc.v_and_u32(am, a, m);
    uc.v_mul_u64_lo_u32(d, am, b);
}

static inline void mmi_smul_even(UniCompiler& uc, const Vec& d, const Vec& a, const Vec& b) {
    mmi_umul_even(uc, d, a, b);

    Vec sa = uc.new_vec128();
    Vec sb = uc.new_vec128();
    Vec bh = uc.new_vec128();
    Vec ah = uc.new_vec128();

    uc.v_srai_i32(sa, a, 31);
    uc.v_srai_i32(sb, b, 31);
    uc.v_slli_u64(sa, sa, 32);
    uc.v_slli_u64(sb, sb, 32);
    uc.v_slli_u64(bh, b, 32);
    uc.v_slli_u64(ah, a, 32);
    uc.v_and_u32(bh, bh, sa);
    uc.v_and_u32(ah, ah, sb);
    uc.v_sub_i64(d, d, bh);
    uc.v_sub_i64(d, d, ah);
}

static inline void mmi_build_acc_w(UniCompiler& uc, const Vec& acc, const Vec& hi, const Vec& lo) {
    Vec m = uc.new_vec128();
    Vec al = uc.new_vec128();
    Vec ah = uc.new_vec128();

    uc.v_ones_i(m);
    uc.v_srli_u64(m, m, 32);
    uc.v_and_u32(al, lo, m);
    uc.v_slli_u64(ah, hi, 32);
    uc.v_or_u32(acc, al, ah);
}

static inline void pmultw(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    mmi_smul_even(uc, rd, rs, rt);
    mmi_spill_words_to_hilo(uc, hi, lo, rd);
}

static inline void pmultuw(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    mmi_umul_even(uc, rd, rs, rt);
    mmi_spill_words_to_hilo(uc, hi, lo, rd);
}

static inline void pmaddw(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    Vec prod = uc.new_vec128();
    Vec acc = uc.new_vec128();
    
    mmi_smul_even(uc, prod, rs, rt);
    mmi_build_acc_w(uc, acc, hi, lo);
    uc.v_add_i64(rd, prod, acc);
    mmi_spill_words_to_hilo(uc, hi, lo, rd);
}

static inline void pmadduw(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    Vec prod = uc.new_vec128();
    Vec acc = uc.new_vec128();

    mmi_umul_even(uc, prod, rs, rt);
    mmi_build_acc_w(uc, acc, hi, lo);
    uc.v_add_i64(rd, prod, acc);
    mmi_spill_words_to_hilo(uc, hi, lo, rd);
}

static inline void pmsubw(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    Vec prod = uc.new_vec128();
    Vec acc = uc.new_vec128();

    mmi_smul_even(uc, prod, rs, rt);
    mmi_build_acc_w(uc, acc, hi, lo);
    uc.v_sub_u64(rd, acc, prod);
    mmi_spill_words_to_hilo(uc, hi, lo, rd);
}

static inline void mmi_mul_h(UniCompiler& uc, const Vec& plo, const Vec& phi, const Vec& rs, const Vec& rt) {
    Vec l = uc.new_vec128();
    Vec h = uc.new_vec128();

    uc.v_mul_i16(l, rs, rt);
    uc.v_mulh_i16(h, rs, rt);
    uc.v_interleave_lo_u16(plo, l, h);
    uc.v_interleave_hi_u16(phi, l, h);
}

static inline void mmi_layout_h(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& plo, const Vec& phi) {
    uc.v_interleave_lo_u64(lo, plo, phi);
    uc.v_interleave_hi_u64(hi, plo, phi);
    
    Vec a = uc.new_vec128();
    Vec b = uc.new_vec128();
    
    mmi_gather_even_u32(uc, a, plo);
    mmi_gather_even_u32(uc, b, phi);

    uc.v_interleave_lo_u64(rd, a, b);
}

static inline void pmulth(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    Vec plo = uc.new_vec128();
    Vec phi = uc.new_vec128();

    mmi_mul_h(uc, plo, phi, rs, rt);
    mmi_layout_h(uc, rd, hi, lo, plo, phi);
}

static inline void pmaddh(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    Vec plo = uc.new_vec128();
    Vec phi = uc.new_vec128();
    Vec clo = uc.new_vec128();
    Vec chi = uc.new_vec128();

    mmi_mul_h(uc, plo, phi, rs, rt);
    uc.v_interleave_lo_u64(clo, lo, hi);
    uc.v_interleave_hi_u64(chi, lo, hi);
    uc.v_add_i32(plo, plo, clo);
    uc.v_add_i32(phi, phi, chi);
    mmi_layout_h(uc, rd, hi, lo, plo, phi);
}

static inline void pmsubh(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    Vec plo = uc.new_vec128();
    Vec phi = uc.new_vec128();
    Vec clo = uc.new_vec128();
    Vec chi = uc.new_vec128();

    mmi_mul_h(uc, plo, phi, rs, rt);
    uc.v_interleave_lo_u64(clo, lo, hi);
    uc.v_interleave_hi_u64(chi, lo, hi);
    uc.v_sub_u32(plo, clo, plo);
    uc.v_sub_u32(phi, chi, phi);
    mmi_layout_h(uc, rd, hi, lo, plo, phi);
}

static inline void mmi_prod_even_odd(UniCompiler& uc, const Vec& ev, const Vec& od, const Vec& rs, const Vec& rt) {
    Vec plo = uc.new_vec128();
    Vec phi = uc.new_vec128();
    mmi_mul_h(uc, plo, phi, rs, rt);

    Vec a = uc.new_vec128();
    Vec b = uc.new_vec128();
    mmi_gather_even_u32(uc, a, plo);
    mmi_gather_even_u32(uc, b, phi);
    uc.v_interleave_lo_u64(ev, a, b);

    Vec c = uc.new_vec128();
    Vec e = uc.new_vec128();
    mmi_gather_odd_u32(uc, c, plo);
    mmi_gather_odd_u32(uc, e, phi);
    uc.v_interleave_lo_u64(od, c, e);
}

static inline void phmadh(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    Vec ev = uc.new_vec128();
    Vec od = uc.new_vec128();
    Vec re = uc.new_vec128();
    Vec oe = uc.new_vec128();
    Vec ro = uc.new_vec128();
    Vec oo = uc.new_vec128();

    mmi_prod_even_odd(uc, ev, od, rs, rt);

    uc.v_add_u32(rd, ev, od);

    mmi_gather_even_u32(uc, re, rd);
    mmi_gather_even_u32(uc, oe, od);
    uc.v_interleave_lo_u32(lo, re, oe);

    mmi_gather_odd_u32(uc, ro, rd);
    mmi_gather_odd_u32(uc, oo, od);
    uc.v_interleave_lo_u32(hi, ro, oo);
}

static inline void phmsbh(UniCompiler& uc, const Vec& rd, const Vec& hi, const Vec& lo, const Vec& rs, const Vec& rt) {
    Vec ev = uc.new_vec128();
    Vec od = uc.new_vec128();
    Vec no = uc.new_vec128();
    Vec re = uc.new_vec128();
    Vec ne = uc.new_vec128();
    Vec ro = uc.new_vec128();
    Vec no2 = uc.new_vec128();

    mmi_prod_even_odd(uc, ev, od, rs, rt);

    uc.v_sub_u32(rd, od, ev);
    uc.v_not_u32(no, od);

    mmi_gather_even_u32(uc, re, rd);
    mmi_gather_even_u32(uc, ne, no);
    uc.v_interleave_lo_u32(lo, re, ne);

    mmi_gather_odd_u32(uc, ro, rd);
    mmi_gather_odd_u32(uc, no2, no);
    uc.v_interleave_lo_u32(hi, ro, no2);
}

static inline void pext5(UniCompiler& uc, const Vec& d, const Vec& t) {
    Vec a = uc.new_vec128();
    Vec b = uc.new_vec128();
    Vec c = uc.new_vec128();
    Vec e = uc.new_vec128();

    uc.v_and_u32(a, t, mmi_splat32(uc, 0x0000001f));
    uc.v_slli_u32(a, a, 3);
    uc.v_and_u32(b, t, mmi_splat32(uc, 0x000003e0));
    uc.v_slli_u32(b, b, 6);
    uc.v_and_u32(c, t, mmi_splat32(uc, 0x00007c00));
    uc.v_slli_u32(c, c, 9);
    uc.v_and_u32(e, t, mmi_splat32(uc, 0x00008000));
    uc.v_slli_u32(e, e, 16);
    uc.v_or_u32(a, a, b);
    uc.v_or_u32(c, c, e);
    uc.v_or_u32(d, a, c);
}

static inline void ppac5(UniCompiler& uc, const Vec& d, const Vec& t) {
    Vec a = uc.new_vec128();
    Vec b = uc.new_vec128();
    Vec c = uc.new_vec128();
    Vec e = uc.new_vec128();

    uc.v_and_u32(a, t, mmi_splat32(uc, 0x000000f8));
    uc.v_srli_u32(a, a, 3);
    uc.v_and_u32(b, t, mmi_splat32(uc, 0x0000f800));
    uc.v_srli_u32(b, b, 6);
    uc.v_and_u32(c, t, mmi_splat32(uc, 0x00f80000));
    uc.v_srli_u32(c, c, 9);
    uc.v_and_u32(e, t, mmi_splat32(uc, 0x80000000));
    uc.v_srli_u32(e, e, 16);
    uc.v_or_u32(a, a, b);
    uc.v_or_u32(c, c, e);
    uc.v_or_u32(d, a, c);
}

}
