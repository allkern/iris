#pragma once

#include <cstdint>
#include <asmjit/ujit.h>

namespace iris::ee {

namespace lsw {

using asmjit::ujit::UniCompiler;
using asmjit::ujit::Gp;
using asmjit::Imm;
using asmjit::ujit::cmp_eq;

static inline Gp sext32(UniCompiler& uc, const Gp& x) {
    Gp r = uc.new_gp64();
#if defined(ASMJIT_UJIT_AARCH64)
    uc.cc->sxtw(r.r64(), x.r32());
#else
    uc.cc->movsxd(r.r64(), x.r32());
#endif
    return r;
}

static inline void bk(UniCompiler& uc, const Gp& shift, Gp& b, Gp& k) {
    b = uc.new_gp32();
    k = uc.new_gp32();

    uc.shl(b, shift, Imm(3));
    uc.mov(k, Imm(24));
    uc.sub(k, k, b);
}

static inline Gp lwl(UniCompiler& uc, const Gp& rt, const Gp& mem, const Gp& shift) {
    Gp b, k;
    Gp mask = uc.new_gp32();
    Gp lo = uc.new_gp32();
    Gp hi = uc.new_gp32();
    Gp m = uc.new_gp32();

    bk(uc, shift, b, k);

    uc.mov(mask, Imm(0x00ffffff));
    uc.shr(mask, mask, b);
    uc.and_(lo, rt.r32(), mask);
    uc.shl(hi, mem.r32(), k);
    uc.or_(m, lo, hi);

    return sext32(uc, m);
}

static inline Gp lwr(UniCompiler& uc, const Gp& rt, const Gp& mem, const Gp& shift) {
    Gp b, k;
    Gp mask = uc.new_gp32();
    Gp lo = uc.new_gp32();
    Gp hi = uc.new_gp32();
    Gp m = uc.new_gp32();
    Gp keep_hi = uc.new_gp64();
    Gp mz = uc.new_gp64();
    Gp keep = uc.new_gp64();
    Gp out = uc.new_gp64();

    bk(uc, shift, b, k);

    uc.mov(mask, Imm(0xffffffff));
    uc.shr(mask, mask, b);
    uc.not_(mask, mask);
    uc.and_(lo, rt.r32(), mask);
    uc.shr(hi, mem.r32(), b);
    uc.or_(m, lo, hi);

    Gp sext = sext32(uc, m);

    uc.shr(keep_hi, rt.r64(), Imm(32));
    uc.shl(keep_hi, keep_hi, Imm(32));
    uc.mov(mz.r32(), m);
    uc.or_(keep, keep_hi, mz);

    uc.select(out, sext, keep, cmp_eq(shift, Imm(0)));
    return out;
}

static inline Gp swl(UniCompiler& uc, const Gp& rt, const Gp& mem, const Gp& shift) {
    Gp b, k;
    Gp mask = uc.new_gp32();
    Gp mm = uc.new_gp32();
    Gp rs = uc.new_gp32();
    Gp out = uc.new_gp32();

    bk(uc, shift, b, k);

    uc.mov(mask, Imm(0xffffff00));
    uc.shl(mask, mask, b);
    uc.and_(mm, mem.r32(), mask);
    uc.shr(rs, rt.r32(), k);
    uc.or_(out, rs, mm);
    return out;
}

static inline Gp swr(UniCompiler& uc, const Gp& rt, const Gp& mem, const Gp& shift) {
    Gp b, k;
    Gp mask = uc.new_gp32();
    Gp mm = uc.new_gp32();
    Gp rs = uc.new_gp32();
    Gp out = uc.new_gp32();

    bk(uc, shift, b, k);

    uc.mov(mask, Imm(0x00ffffff));
    uc.shr(mask, mask, k);
    uc.and_(mm, mem.r32(), mask);
    uc.shl(rs, rt.r32(), b);
    uc.or_(out, rs, mm);
    return out;
}

}

}
