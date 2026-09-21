#pragma once

#include <cstdlib>
#include <cstring>

#include <asmjit/ujit.h>

#include "vu.hpp"
#include "vu_def.hpp"
#include "jit_invoke.hpp"
#include "../jit_call.hpp"

namespace iris::vu::jit {

enum {
    EMIT_MINMAX = 1u << 0,
    EMIT_CONV   = 1u << 1,
    EMIT_MOVE   = 1u << 2,
    EMIT_BRANCH = 1u << 3,
    EMIT_FLAGS  = 1u << 4,
    EMIT_INT    = 1u << 5,
    EMIT_MEM    = 1u << 6,
    EMIT_WAITQ  = 1u << 7,
    EMIT_NOP    = 1u << 8,
    EMIT_CLAMP  = 1u << 9,
    EMIT_FMAC   = 1u << 10,
    EMIT_CLIPW  = 1u << 11,
    EMIT_QDIV   = 1u << 12,
    EMIT_RAND   = 1u << 13,
    EMIT_EFU    = 1u << 14,
    EMIT_GIF    = 1u << 15
};

inline uint32_t emit_disabled() {
    return 0;
}

inline bool emit_off(uint32_t group) {
    return (emit_disabled() & group) != 0;
}

using namespace asmjit;

enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_MADD,
    OP_MSUB
};

enum {
    SRC_VEC,
    SRC_BCX,
    SRC_BCY,
    SRC_BCZ,
    SRC_BCW,
    SRC_I,
    SRC_Q
};

struct Fmac {
    int op;
    bool to_acc;
    int tk;
};

inline bool classify_fmac(uint32_t opcode, Fmac* out) {
    static const int quad[4] = { OP_ADD, OP_SUB, OP_MADD, OP_MSUB };

    bool to_acc = (opcode & 0x3c) == 0x3c;

    uint32_t sel = to_acc ? (((opcode & 0x3c0) >> 4) | (opcode & 3))
                          : (opcode & 0x3f);

    if (sel <= 0x0f) {
        out->op = quad[sel >> 2];
        out->to_acc = to_acc;
        out->tk = SRC_BCX + (int)(sel & 3);

        return true;
    }

    if (sel >= 0x18 && sel <= 0x1b) {
        out->op = OP_MUL;
        out->to_acc = to_acc;
        out->tk = SRC_BCX + (int)(sel - 0x18);

        return true;
    }

    out->to_acc = to_acc;

    switch (sel) {
        case 0x1c: out->op = OP_MUL;  out->tk = SRC_Q;   return true;
        case 0x1e: out->op = OP_MUL;  out->tk = SRC_I;   return true;
        case 0x20: out->op = OP_ADD;  out->tk = SRC_Q;   return true;
        case 0x21: out->op = OP_MADD; out->tk = SRC_Q;   return true;
        case 0x22: out->op = OP_ADD;  out->tk = SRC_I;   return true;
        case 0x23: out->op = OP_MADD; out->tk = SRC_I;   return true;
        case 0x24: out->op = OP_SUB;  out->tk = SRC_Q;   return true;
        case 0x25: out->op = OP_MSUB; out->tk = SRC_Q;   return true;
        case 0x26: out->op = OP_SUB;  out->tk = SRC_I;   return true;
        case 0x27: out->op = OP_MSUB; out->tk = SRC_I;   return true;
        case 0x28: out->op = OP_ADD;  out->tk = SRC_VEC; return true;
        case 0x29: out->op = OP_MADD; out->tk = SRC_VEC; return true;
        case 0x2a: out->op = OP_MUL;  out->tk = SRC_VEC; return true;
        case 0x2c: out->op = OP_SUB;  out->tk = SRC_VEC; return true;
        case 0x2d: out->op = OP_MSUB; out->tk = SRC_VEC; return true;
    }

    return false;
}

inline uint32_t upper_key(uint32_t opcode) {
    if ((opcode & 0x3c) == 0x3c) {
        return 0x80 | (((opcode & 0x3c0) >> 4) | (opcode & 3));
    }

    return opcode & 0x3f;
}

inline uint32_t lower_key(uint32_t opcode) {
    uint32_t hi = (opcode & 0xfe000000) >> 25;

    if (hi != 0x40) {
        return hi;
    }

    if ((opcode & 0x3c) == 0x3c) {
        return 0x80 | (((opcode & 0x7c0) >> 4) | (opcode & 3));
    }

    return 0x40 | (opcode & 0x3f);
}

inline bool lower_touches_flags(uint32_t key) {
    if (key >= 0x10 && key <= 0x1c) {
        return true;
    }

    return key == (0x80 | 0x38) || key == (0x80 | 0x39) || key == (0x80 | 0x3a);
}

inline bool lower_touches_q(uint32_t key) {
    return key >= (0x80 | 0x38) && key <= (0x80 | 0x3b);
}

inline bool lower_reads_tpc(uint32_t key) {
    return key >= 0x20 && key <= 0x2f;
}

inline bool upper_touches_flags(uint32_t key) {
    return key == 0x2e || key == (0x80 | 0x2e) || key == (0x80 | 0x1f);
}

inline uint32_t nibble_mask(uint32_t field) {
    uint32_t nm = 0;

    for (int i = 0; i < 4; i++) {
        if (field & (8 >> i)) {
            nm |= 0x1111u << (3 - i);
        }
    }

    return nm;
}

constexpr int REG_ACC = 32;
constexpr int REG_COUNT = 33;

enum {
    K_EXP,
    K_SIGN,
    K_MANT,
    K_MAXN,
    K_LANE,
    K_ONES,
    K_ZERO,
    K_ABS,
    K_2P31,
    K_SCALE,
    K_WRITE = K_SCALE + 8,
    K_CLIPGT = K_WRITE + 16,
    K_CLIPLT,
    K_MACU,     // underflow, bit 8
    K_MACO      // overflow, bit 12
};

constexpr int K_COUNT = K_MACO + 1;

extern const uint32_t g_vu_const[K_COUNT][4];

struct Emitter {
    Vu* vu;
    ujit::UniCompiler* uc;

    ujit::Gp state;
    ujit::Gp consts;

    ujit::Vec reg[REG_COUNT];
    bool valid[REG_COUNT];
    bool dirty[REG_COUNT];

    uint8_t clean[REG_COUNT];

    ujit::Vec clamped[REG_COUNT];
    bool clamped_valid[REG_COUNT] = {};

    ujit::Vec konst[K_COUNT];
    bool konst_valid[K_COUNT];

    ujit::Vec scratch[4];
    bool scratch_valid[4] = {};

    ujit::Vec t(int i) {
        if (!scratch_valid[i]) {
            scratch[i] = uc->new_vec128();
            scratch_valid[i] = true;
        }

        return scratch[i];
    }

    bool scalars_loaded = false;
    ujit::Gp q_delay_reg;
    ujit::Gp cycle_reg;

    bool status_pending = false;
    ujit::Gp status_or;
    ujit::Gp status_last;

    int shadow_left = 1;
    int shadow_reg = -1;

    bool flags_loaded = false;
    ujit::Gp mac_reg;
    ujit::Gp mac_ring[4];

    int clip_shifts = 0;

    ujit::Mem mem(size_t offset) {
        return ujit::mem_ptr(state, offset);
    }

    void load_scalars() {
        if (scalars_loaded) {
            return;
        }

        scalars_loaded = true;

        q_delay_reg = uc->new_gp32();
        cycle_reg = uc->new_gp64();

        uc->load_i32(q_delay_reg, mem(offsetof(Vu, q_delay)));
        uc->load_u64(cycle_reg, mem(offsetof(Vu, vu_cycle)));
    }

    void store_scalars() {
        if (!scalars_loaded) {
            return;
        }

        uc->store_u32(mem(offsetof(Vu, q_delay)), q_delay_reg);
        uc->store_u64(mem(offsetof(Vu, vu_cycle)), cycle_reg);
    }

    void drop_scalars() {
        store_scalars();

        scalars_loaded = false;
    }

    void load_flags() {
        if (flags_loaded) {
            return;
        }

        flags_loaded = true;

        mac_reg = uc->new_gp32();

        uc->load_u32(mac_reg, mem(offsetof(Vu, mac)));

        for (int i = 0; i < 4; i++) {
            mac_ring[i] = uc->new_gp32();

            uc->load_u32(mac_ring[i], mem(offsetof(Vu, mac_pipeline) + (size_t)i * 4));
        }
    }

    void flush_clip(bool consume) {
        if (!clip_shifts) {
            return;
        }

        int shifts = clip_shifts > 4 ? 4 : clip_shifts;

        ujit::Gp old[4];

        for (int i = 0; i + shifts < 4; i++) {
            old[i] = uc->new_gp32();

            uc->load_u32(old[i], mem(offsetof(Vu, clip_pipeline) + (size_t)i * 4));
        }

        ujit::Gp clip = uc->new_gp32();

        uc->load_u32(clip, mem(offsetof(Vu, clip)));

        for (int i = 3; i >= 0; i--) {
            ujit::Gp src = i >= shifts ? old[i - shifts] : clip;

            uc->store_u32(mem(offsetof(Vu, clip_pipeline) + (size_t)i * 4), src);
        }

        if (consume) {
            clip_shifts = 0;
        }
    }

    void store_flags(bool consume) {
        flush_clip(consume);

        if (!flags_loaded) {
            return;
        }

        uc->store_u32(mem(offsetof(Vu, mac)), mac_reg);

        for (int i = 0; i < 4; i++) {
            uc->store_u32(mem(offsetof(Vu, mac_pipeline) + (size_t)i * 4), mac_ring[i]);
        }
    }

    void drop_flags() {
        emit_status(true);
        store_flags(true);

        flags_loaded = false;
    }

    void shift_flags() {
        load_flags();

        for (int i = 3; i > 0; i--) {
            mac_ring[i] = mac_ring[i - 1];
        }

        mac_ring[0] = mac_reg;

        clip_shifts++;
    }

    ujit::Mem reg_mem(int i) {
        if (i == REG_ACC) {
            return mem(offsetof(Vu, acc));
        }

        return mem(offsetof(Vu, vf) + (size_t)i * sizeof(Reg128));
    }

    ujit::Mem k(int i) {
        return ujit::mem_ptr(consts, i * 16);
    }

    ujit::Vec kv(int i) {
        if (!konst_valid[i]) {
            konst[i] = uc->new_vec128();

            uc->v_loadu128(konst[i], k(i));

            konst_valid[i] = true;
        }

        return konst[i];
    }

    ujit::Vec get(int i) {
        if (valid[i]) {
            return reg[i];
        }

        reg[i] = uc->new_vec128();

        uc->v_loadu128(reg[i], reg_mem(i));

        valid[i] = true;
        dirty[i] = false;

        return reg[i];
    }

    void set(int i, const ujit::Vec& v) {
        reg[i] = v;
        valid[i] = true;
        dirty[i] = true;

        clamped_valid[i] = false;
    }

    void set_clamped(int i, const ujit::Vec& v, uint32_t field) {
        set(i, v);

        clean[i] |= (uint8_t)field;
    }

    void set_copied(int i, const ujit::Vec& v, int from, uint32_t field) {
        set(i, v);

        clean[i] = (uint8_t)((clean[i] & ~field) | (clean[from] & field));
    }

    void forget(int i) {
        flush_one(i);

        clean[i] = 0;
        clamped_valid[i] = false;
    }

    void mark_status(const ujit::Gp& ring3) {
        if (!status_pending) {
            status_or = uc->new_gp32();

            uc->mov(status_or, ring3);
        } else {
            uc->or_(status_or, status_or, ring3);
        }

        status_last = ring3;
        status_pending = true;
    }

    ujit::Gp status_bits(const ujit::Gp& value) {
        ujit::Gp out = uc->new_gp32();

        uc->mov(out, Imm(0));

        for (int i = 0; i < 4; i++) {
            ujit::Gp nibble = uc->new_gp32();
            ujit::Gp one = uc->new_gp32();

            uc->and_(nibble, value, Imm(0xfu << (i * 4)));
            uc->select(one, Imm(1 << i), Imm(0), ujit::test_nz(nibble));
            uc->or_(out, out, one);
        }

        return out;
    }

    void emit_status(bool consume) {
        if (!status_pending) {
            return;
        }

        ujit::Gp st = uc->new_gp32();
        ujit::Gp low = status_bits(status_last);
        ujit::Gp high = status_bits(status_or);

        uc->load_u32(st, mem(offsetof(Vu, status)));

        uc->and_(st, st, Imm(~0x3fu));
        uc->shl(high, high, Imm(6));
        uc->or_(st, st, low);
        uc->or_(st, st, high);

        uc->store_u32(mem(offsetof(Vu, status)), st);

        if (consume) {
            status_pending = false;
        }
    }

    void store_dirty() {
        for (int i = 0; i < REG_COUNT; i++) {
            if (valid[i] && dirty[i]) {
                uc->v_storeu128(reg_mem(i), reg[i]);
            }
        }

        emit_status(false);
        store_flags(false);
        store_scalars();
    }

    void flush_all() {
        drop_flags();
        drop_scalars();

        for (int i = 0; i < REG_COUNT; i++) {
            clean[i] = 0;
            clamped_valid[i] = false;
        }

        for (int i = 0; i < REG_COUNT; i++) {
            if (valid[i] && dirty[i]) {
                uc->v_storeu128(reg_mem(i), reg[i]);
            }

            valid[i] = false;
            dirty[i] = false;
        }
    }

    void flush_one(int i) {
        if (valid[i] && dirty[i]) {
            uc->v_storeu128(reg_mem(i), reg[i]);
        }

        valid[i] = false;
        dirty[i] = false;
        clamped_valid[i] = false;
    }

    void flush_for(const Instruction& ins) {
        uint32_t key = lower_key(ins.opcode);

        if (lower_touches_flags(key)) {
            drop_flags();
        }

        if (lower_touches_q(key)) {
            drop_scalars();
        }

        int regs[3] = { ins.src[0].reg, ins.src[1].reg, ins.dst.reg };

        for (int r : regs) {
            if (r > 0 && r < 32) {
                forget(r);
            }
        }
    }
};

inline ujit::Vec emit_cvtf(Emitter& e, const ujit::Vec& v) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Vec exp = e.t(0);
    ujit::Vec is0 = e.t(1);
    ujit::Vec is255 = e.t(2);
    ujit::Vec sign = e.t(3);
    ujit::Vec out = uc.new_vec128();

    uc.v_and_i32(exp, v, e.k(K_EXP));
    uc.v_cmp_eq_i32(is0, exp, e.k(K_ZERO));
    uc.v_cmp_eq_i32(is255, exp, e.k(K_EXP));
    uc.v_and_i32(sign, v, e.k(K_SIGN));

    uc.v_blendv_u8(out, v, sign, is0);

    uc.v_or_i32(exp, sign, e.k(K_MAXN));
    uc.v_blendv_u8(out, out, exp, is255);

    return out;
}

inline ujit::Vec emit_load_cvtf(Emitter& e, int reg, uint32_t need) {
    ujit::Vec v = e.get(reg);

    if ((e.clean[reg] & need) == need && !emit_off(EMIT_CLAMP)) {
        return v;
    }

    if (e.clamped_valid[reg] && !emit_off(EMIT_CLAMP)) {
        return e.clamped[reg];
    }

    ujit::Vec out = emit_cvtf(e, v);

    e.clamped[reg] = out;
    e.clamped_valid[reg] = true;

    return out;
}

inline ujit::Vec emit_flags4(Emitter& e, const ujit::Vec& v, uint32_t field) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Vec a = e.t(0);
    ujit::Vec b = e.t(1);
    ujit::Vec c = e.t(2);
    ujit::Vec d = e.t(3);

    ujit::Vec out = uc.new_vec128();

    uc.v_and_i32(a, v, e.k(K_EXP));
    uc.v_cmp_eq_i32(b, a, e.k(K_ZERO));
    uc.v_cmp_eq_i32(c, a, e.k(K_EXP));

    uc.v_and_i32(a, v, e.k(K_MANT));
    uc.v_cmp_eq_i32(d, a, e.k(K_ZERO));
    uc.v_xor_i32(d, d, e.k(K_ONES));
    uc.v_and_i32(d, b, d);

    uc.v_and_i32(a, v, e.k(K_SIGN));

    uc.v_blendv_u8(out, v, a, b);

    uc.v_or_i32(a, a, e.k(K_MAXN));
    uc.v_blendv_u8(out, out, a, c);

    uc.v_srli_u32(a, b, 31);
    uc.v_srli_u32(b, v, 31);
    uc.v_slli_i32(b, b, 4);
    uc.v_and_i32(d, d, e.k(K_MACU));
    uc.v_and_i32(c, c, e.k(K_MACO));

    uc.v_or_i32(a, a, b);
    uc.v_or_i32(c, c, d);
    uc.v_or_i32(a, a, c);

    uc.v_mul_i16(a, a, e.k(K_LANE));

    uc.v_swizzle_u32x4(b, a, ujit::swizzle(2, 3, 0, 1));
    uc.v_or_i32(a, a, b);
    uc.v_swizzle_u32x4(b, a, ujit::swizzle(1, 0, 3, 2));
    uc.v_or_i32(a, a, b);

    ujit::Gp newmac = uc.new_gp32();
    ujit::Gp mac = uc.new_gp32();

    uc.s_extract_u32(newmac, a, 0);

    if (nibble_mask(field) != 0xffffu) {
        uc.and_(newmac, newmac, Imm(nibble_mask(field)));
    }

    e.load_flags();

    uc.mov(mac, e.mac_reg);
    uc.and_(mac, mac, Imm(0xffff0000u));
    uc.or_(mac, mac, newmac);

    e.mac_reg = mac;

    return out;
}

inline void emit_update_status(Emitter& e) {
    e.load_flags();
    e.mark_status(e.mac_ring[3]);
}

inline void emit_epilogue(Emitter& e, const BlockEntry* entry, bool vi_shadow_live) {
    ujit::UniCompiler& uc = *e.uc;

    e.shift_flags();

    if (vi_shadow_live) {
        Label done = uc.new_label();

        ujit::Gp cycles = uc.new_gp32();

        uc.load_i32(cycles, e.mem(offsetof(Vu, vi_backup_cycles)));
        uc.j(done, ujit::test_z(cycles));

        uc.sub(cycles, cycles, Imm(1));
        uc.store_u32(e.mem(offsetof(Vu, vi_backup_cycles)), cycles);
        uc.j(done, ujit::test_nz(cycles));

        ujit::Gp zero = uc.new_gp32();

        uc.mov(zero, Imm(0));
        uc.store_u32(e.mem(offsetof(Vu, vi_backup_reg)), zero);
        uc.store_u32(e.mem(offsetof(Vu, vi_backup_value)), zero);

        uc.bind(done);
    }

    e.load_scalars();

    int writes[2][2] = {
        { entry->uw_reg, entry->uw_mask },
        { entry->lw_reg, entry->lw_mask }
    };

    bool any = writes[0][0] || writes[1][0];

    ujit::Gp ready;

    if (any) {
        ready = uc.new_gp64();

        uc.add(ready, e.cycle_reg, Imm(VF_LATENCY));
    }

    for (auto& w : writes) {
        if (!w[0]) {
            continue;
        }

        for (int c = 0; c < 4; c++) {
            if (!(w[1] & (1 << c))) {
                continue;
            }

            size_t off = offsetof(Vu, vf_ready)
                       + ((size_t)w[0] * 4 + (size_t)c) * sizeof(uint64_t);

            uc.store_u64(e.mem(off), ready);
        }
    }

    uc.add(e.cycle_reg, e.cycle_reg, Imm(1));
}

inline void emit_prologue(Emitter& e, int stall) {
    ujit::UniCompiler& uc = *e.uc;

    e.load_scalars();

    if (stall) {
        uc.add(e.cycle_reg, e.cycle_reg, Imm(stall));

        for (int i = 0; i < stall; i++) {
            e.shift_flags();
        }
    }

    ujit::Gp waiting = uc.new_gp32();
    ujit::Gp less = uc.new_gp32();

    uc.mov(waiting, e.q_delay_reg);
    uc.mov(less, waiting);
    uc.sub(less, less, Imm(stall + 1));

    uc.mov(e.q_delay_reg, Imm(0));
    uc.cmov(e.q_delay_reg, less, ujit::scmp_gt(waiting, Imm(stall)));

    emit_update_status(e);
}

inline bool emit_move(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int s = (int)ins.ld_s;
    int t = (int)ins.ld_t;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!t || !field) {
        return true;
    }

    ujit::Vec src = e.get(s);

    if (field == 0xf) {
        e.set_copied(t, src, s, field);

        return true;
    }

    ujit::Vec cur = e.get(t);
    ujit::Vec merged = uc.new_vec128();

    uc.v_blendv_u8(merged, cur, src, e.kv(K_WRITE + (int)field));

    e.set_copied(t, merged, s, field);

    return true;
}

inline void emit_arm_branch(Emitter& e, uint32_t target, bool may_delay) {
    ujit::UniCompiler& uc = *e.uc;

    auto arm = [&](size_t delay_field, size_t pc_field, uint32_t delay) {
        ujit::Gp t = uc.new_gp32();

        uc.mov(t, Imm(delay));
        uc.store_u32(e.mem(delay_field), t);

        uc.mov(t, Imm(target));
        uc.store_u32(e.mem(pc_field), t);
    };

    if (!may_delay) {
        arm(offsetof(Vu, branch_delay), offsetof(Vu, branch_pc), 2);

        return;
    }

    Label chained = uc.new_label();
    Label done = uc.new_label();

    ujit::Gp pending = uc.new_gp32();

    uc.load_i32(pending, e.mem(offsetof(Vu, branch_delay)));
    uc.j(chained, ujit::test_nz(pending));

    arm(offsetof(Vu, branch_delay), offsetof(Vu, branch_pc), 2);

    uc.j(done);

    uc.bind(chained);

    ujit::Gp one = uc.new_gp32();

    uc.mov(one, Imm(1));
    uc.store_u8(e.mem(offsetof(Vu, delay_branch)), one);

    uc.mov(one, Imm(target));
    uc.store_u32(e.mem(offsetof(Vu, delay_branch_pc)), one);

    uc.bind(done);
}

inline bool emit_flag_compare(Emitter& e, const Instruction& ins, uint32_t key) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)(ins.ld_t & 0xf);

    if (!t) {
        return true;
    }

    e.load_flags();

    ujit::Gp value = uc.new_gp32();
    ujit::Gp is = uc.new_gp32();

    uc.load_u16(is, e.mem(offsetof(Vu, vi) + (size_t)(ins.ld_s & 0xf) * 2));
    uc.mov(value, e.mac_ring[3]);

    switch (key) {
        case 0x18: {
            uc.and_(value, value, Imm(0xffff));
            uc.select(value, Imm(1), Imm(0), ujit::cmp_eq(value, is));
        } break;

        case 0x1a: {
            uc.and_(value, value, is);
        } break;

        default: {
            uc.and_(value, value, Imm(0xffff));
            uc.or_(value, value, is);
        } break;
    }

    uc.store_u16(e.mem(offsetof(Vu, vi) + (size_t)t * 2), value);

    return true;
}

inline bool emit_branch(Emitter& e, const Instruction& ins, uint32_t next_tpc,
                        uint32_t mask, bool may_delay) {
    ujit::UniCompiler& uc = *e.uc;

    uint32_t target = (next_tpc + (uint32_t)ins.ld_imm11) & mask;

    emit_arm_branch(e, target, may_delay);

    return true;
}

inline ujit::Gp emit_get_branch_vi(Emitter& e, int reg, bool shadow_live, bool sign) {
    ujit::UniCompiler& uc = *e.uc;

    reg &= 0xf;

    ujit::Gp out = uc.new_gp32();

    auto shadowed = [&]() {
        uc.load_u32(out, e.mem(offsetof(Vu, vi_backup_value)));

        if (sign) {
            uc.shl(out, out, Imm(16));
            uc.sar(out, out, Imm(16));
        } else {
            uc.and_(out, out, Imm(0xffff));
        }
    };

    auto plain = [&]() {
        if (sign) {
            uc.load_i16(out, e.mem(offsetof(Vu, vi) + (size_t)reg * 2));
        } else {
            uc.load_u16(out, e.mem(offsetof(Vu, vi) + (size_t)reg * 2));
        }
    };

    if (!shadow_live && !(e.shadow_left > 0 && e.shadow_reg < 0)) {
        if (e.shadow_left > 0 && e.shadow_reg == reg) {
            shadowed();
        } else {
            plain();
        }

        return out;
    }

    plain();

    Label done = uc.new_label();

    ujit::Gp cycles = uc.new_gp32();
    ujit::Gp held = uc.new_gp32();

    uc.load_i32(cycles, e.mem(offsetof(Vu, vi_backup_cycles)));
    uc.j(done, ujit::test_z(cycles));

    uc.load_i32(held, e.mem(offsetof(Vu, vi_backup_reg)));
    uc.j(done, ujit::cmp_ne(held, Imm(reg)));

    shadowed();

    uc.bind(done);

    return out;
}

inline bool emit_cond_branch(Emitter& e, const Instruction& ins, uint32_t key,
                             uint32_t next_tpc, uint32_t mask, bool may_delay,
                             bool shadow_live) {
    ujit::UniCompiler& uc = *e.uc;

    uint32_t target = (next_tpc + (uint32_t)ins.ld_imm11) & mask;

    Label skip = uc.new_label();

    if (key == 0x28 || key == 0x29) {
        ujit::Gp t = emit_get_branch_vi(e, (int)ins.ld_t, shadow_live, false);
        ujit::Gp s = emit_get_branch_vi(e, (int)ins.ld_s, shadow_live, false);

        uc.j(skip, key == 0x28 ? ujit::cmp_ne(t, s) : ujit::cmp_eq(t, s));
    } else {
        ujit::Gp s = emit_get_branch_vi(e, (int)ins.ld_s, shadow_live, true);

        switch (key) {
            case 0x2c: uc.j(skip, ujit::scmp_ge(s, Imm(0))); break;
            case 0x2d: uc.j(skip, ujit::scmp_le(s, Imm(0))); break;
            case 0x2e: uc.j(skip, ujit::scmp_gt(s, Imm(0))); break;
            default:   uc.j(skip, ujit::scmp_lt(s, Imm(0))); break;
        }
    }

    emit_arm_branch(e, target, may_delay);

    uc.bind(skip);

    return true;
}

constexpr uint32_t UPPER_NOP = 0x80 | 0x2f;

inline bool emit_shadow_write(Emitter& e, int dst) {
    ujit::UniCompiler& uc = *e.uc;

    dst &= 0xf;

    if (!dst) {
        return true;
    }

    auto record = [&]() {
        ujit::Gp reg = uc.new_gp32();
        ujit::Gp old = uc.new_gp32();

        uc.mov(reg, Imm(dst));
        uc.store_u32(e.mem(offsetof(Vu, vi_backup_reg)), reg);

        uc.load_u16(old, e.mem(offsetof(Vu, vi) + (size_t)dst * 2));
        uc.store_u32(e.mem(offsetof(Vu, vi_backup_value)), old);
    };

    bool known = !(e.shadow_left > 0 && e.shadow_reg < 0);

    ujit::Gp cycles;
    ujit::Gp held;

    if (!known) {
        cycles = uc.new_gp32();
        held = uc.new_gp32();

        uc.load_i32(cycles, e.mem(offsetof(Vu, vi_backup_cycles)));
        uc.load_i32(held, e.mem(offsetof(Vu, vi_backup_reg)));
    }

    ujit::Gp two = uc.new_gp32();

    uc.mov(two, Imm(2));
    uc.store_u32(e.mem(offsetof(Vu, vi_backup_cycles)), two);

    if (known) {
        if (!(e.shadow_left > 0 && e.shadow_reg == dst)) {
            record();
        }
    } else {
        Label write_it = uc.new_label();
        Label done = uc.new_label();

        uc.j(write_it, ujit::test_z(cycles));
        uc.j(done, ujit::cmp_eq(held, Imm(dst)));

        uc.bind(write_it);

        record();

        uc.bind(done);
    }

    e.shadow_left = 2;
    e.shadow_reg = dst;

    return true;
}

inline void emit_set_vi(Emitter& e, int dst, const ujit::Gp& value) {
    dst &= 0xf;

    if (!dst) {
        return;
    }

    e.uc->store_u16(e.mem(offsetof(Vu, vi) + (size_t)dst * 2), value);
}

inline ujit::Gp emit_get_vi(Emitter& e, int src) {
    ujit::Gp out = e.uc->new_gp32();

    e.uc->load_u16(out, e.mem(offsetof(Vu, vi) + (size_t)(src & 0xf) * 2));

    return out;
}

inline bool emit_int_alu(Emitter& e, const Instruction& ins, uint32_t key) {
    ujit::UniCompiler& uc = *e.uc;

    int d = key == 0x70 || key == 0x71 || key == 0x74 || key == 0x75
          ? (int)ins.ld_d : (int)ins.ld_t;

    if (!emit_shadow_write(e, d)) {
        return false;
    }

    ujit::Gp value = emit_get_vi(e, (int)ins.ld_s);

    switch (key) {
        case 0x08: uc.add(value, value, Imm(ins.ld_imm15)); break;
        case 0x09: uc.sub(value, value, Imm(ins.ld_imm15)); break;
        case 0x72: uc.add(value, value, Imm(ins.ld_imm5)); break;

        default: {
            ujit::Gp t = emit_get_vi(e, (int)ins.ld_t);

            switch (key) {
                case 0x70: uc.add(value, value, t); break;
                case 0x71: uc.sub(value, value, t); break;
                case 0x74: uc.and_(value, value, t); break;
                default:   uc.or_(value, value, t); break;
            }
        } break;
    }

    emit_set_vi(e, d, value);

    return true;
}

inline ujit::Gp emit_mem_addr(Emitter& e, int vi_reg, int32_t offset) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Gp addr = uc.new_gp_ptr();

    uc.load_u16(addr, e.mem(offsetof(Vu, vi) + (size_t)(vi_reg & 0xf) * 2));

    if (offset) {
        uc.add(addr, addr, Imm(offset));
    }

    uc.and_(addr, addr, Imm(0x3ff));
    uc.shl(addr, addr, Imm(4));

    return addr;
}

inline ujit::Gp emit_mem_addr_raw(Emitter& e, int vi_reg, int32_t offset) {
    ujit::Gp addr = e.uc->new_gp32();

    e.uc->load_u16(addr, e.mem(offsetof(Vu, vi) + (size_t)(vi_reg & 0xf) * 2));

    if (offset) {
        e.uc->add(addr, addr, Imm(offset));
    }

    return addr;
}

inline ujit::Vec emit_mem_load(Emitter& e, int vi_reg, int32_t offset) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Vec value = uc.new_vec128();

    if (e.vu->id == 0) {
        jit_function_call(&uc, &vu::jit_mem_load, e.state,
                          emit_mem_addr_raw(e, vi_reg, offset));

        uc.v_loadu128(value, e.mem(offsetof(Vu, jit_quad)));

        return value;
    }

    ujit::Gp base = uc.new_gp_ptr();

    uc.mov(base, Imm((uintptr_t)e.vu->vu_mem));
    uc.add(base, base, emit_mem_addr(e, vi_reg, offset));

    uc.v_loadu128(value, ujit::mem_ptr(base, 0));

    return value;
}

inline void emit_mem_store(Emitter& e, int vi_reg, int32_t offset,
                           const ujit::Vec& value, uint32_t field) {
    ujit::UniCompiler& uc = *e.uc;

    if (e.vu->id == 0) {
        ujit::Gp addr = emit_mem_addr_raw(e, vi_reg, offset);

        uc.v_storeu128(e.mem(offsetof(Vu, jit_quad)), value);

        ujit::Gp mask = uc.new_gp32();

        uc.mov(mask, Imm(field));

        jit_function_call(&uc, &vu::jit_mem_store, e.state, addr, mask);

        return;
    }

    ujit::Gp base = uc.new_gp_ptr();

    uc.mov(base, Imm((uintptr_t)e.vu->vu_mem));
    uc.add(base, base, emit_mem_addr(e, vi_reg, offset));

    if (field == 0xf) {
        uc.v_storeu128(ujit::mem_ptr(base, 0), value);

        return;
    }

    ujit::Vec merged = uc.new_vec128();
    ujit::Vec current = uc.new_vec128();

    uc.v_loadu128(current, ujit::mem_ptr(base, 0));
    uc.v_blendv_u8(merged, current, value, e.kv(K_WRITE + (int)field));
    uc.v_storeu128(ujit::mem_ptr(base, 0), merged);
}

inline void emit_quad_write(Emitter& e, int dst, const ujit::Vec& value, uint32_t field) {
    ujit::UniCompiler& uc = *e.uc;

    if (field == 0xf) {
        e.set(dst, value);
    } else {
        ujit::Vec merged = uc.new_vec128();

        uc.v_blendv_u8(merged, e.get(dst), value, e.kv(K_WRITE + (int)field));

        e.set(dst, merged);
    }

    e.clean[dst] = (uint8_t)(e.clean[dst] & ~field);
}

inline void emit_broadcast_write(Emitter& e, int t, uint32_t field, const ujit::Gp& value) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Vec out = uc.new_vec128();

    uc.v_broadcast_u32(out, value);

    if (field == 0xf) {
        e.set(t, out);
    } else {
        ujit::Vec merged = uc.new_vec128();

        uc.v_blendv_u8(merged, e.get(t), out, e.kv(K_WRITE + (int)field));

        e.set(t, merged);
    }

    e.clean[t] = (uint8_t)(e.clean[t] & ~field);
}

inline bool emit_mfir(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ld_t;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!t || !field) {
        return true;
    }

    ujit::Gp value = uc.new_gp32();

    uc.load_i16(value, e.mem(offsetof(Vu, vi) + (size_t)(ins.ld_s & 0xf) * 2));

    emit_broadcast_write(e, t, field, value);

    return true;
}

inline bool emit_mr32(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ld_t;
    int s = (int)ins.ld_s;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!t || !field) {
        return true;
    }

    ujit::Vec rotated = uc.new_vec128();

    uc.v_swizzle_u32x4(rotated, e.get(s), ujit::swizzle(0, 3, 2, 1));

    uint8_t src = e.clean[s];
    uint8_t rot = (uint8_t)(((src << 1) | (src >> 3)) & 0xf);

    if (field == 0xf) {
        e.set(t, rotated);
        e.clean[t] = rot;
    } else {
        ujit::Vec merged = uc.new_vec128();

        uc.v_blendv_u8(merged, e.get(t), rotated, e.kv(K_WRITE + (int)field));

        e.set(t, merged);
        e.clean[t] = (uint8_t)((e.clean[t] & ~field) | (rot & field));
    }

    return true;
}

inline bool emit_load_quad(Emitter& e, const Instruction& ins, int form) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ld_t;
    int s = (int)ins.ld_s;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (form != 0 && !emit_shadow_write(e, s)) {
        return false;
    }

    if (form == 2) {
        ujit::Gp walked = emit_get_vi(e, s);

        uc.sub(walked, walked, Imm(1));

        emit_set_vi(e, s, walked);
    }

    if (t && field) {
        emit_quad_write(e, t, emit_mem_load(e, s, form == 0 ? ins.ld_imm11 : 0), field);
    }

    if (form == 1) {
        ujit::Gp walked = emit_get_vi(e, s);

        uc.add(walked, walked, Imm(1));

        emit_set_vi(e, s, walked);
    }

    return true;
}

inline bool emit_store_quad(Emitter& e, const Instruction& ins, int form) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ld_t;
    int s = (int)ins.ld_s;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (form != 0 && !emit_shadow_write(e, t)) {
        return false;
    }

    if (form == 2) {
        ujit::Gp walked = emit_get_vi(e, t);

        uc.sub(walked, walked, Imm(1));

        emit_set_vi(e, t, walked);
    }

    if (field) {
        emit_mem_store(e, t, form == 0 ? ins.ld_imm11 : 0, e.get(s), field);
    }

    if (form == 1) {
        ujit::Gp walked = emit_get_vi(e, t);

        uc.add(walked, walked, Imm(1));

        emit_set_vi(e, t, walked);
    }

    return true;
}

constexpr uint32_t LOWER_MOVE = 0x80 | 0x30;
constexpr uint32_t LOWER_MR32 = 0x80 | 0x31;
constexpr uint32_t LOWER_MFIR = 0x80 | 0x3d;
constexpr uint32_t LOWER_WAITQ = 0x80 | 0x3b;
constexpr uint32_t LOWER_B = 0x20;

inline bool emit_int_load(Emitter& e, const Instruction& ins, bool has_imm) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)(ins.ld_t & 0xf);

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!t || !field) {
        return true;
    }

    int lane = 0;

    for (int i = 0; i < 4; i++) {
        if (field & (8u >> i)) {
            lane = i;
        }
    }

    ujit::Gp value = uc.new_gp32();

    if (e.vu->id == 0) {
        ujit::Vec quad = emit_mem_load(e, (int)ins.ld_s, has_imm ? ins.ld_imm11 : 0);

        uc.s_extract_u32(value, quad, (uint32_t)lane);
    } else {
        ujit::Gp base = uc.new_gp_ptr();

        uc.mov(base, Imm((uintptr_t)e.vu->vu_mem));
        uc.add(base, base, emit_mem_addr(e, (int)ins.ld_s, has_imm ? ins.ld_imm11 : 0));

        uc.load_u32(value, ujit::mem_ptr(base, lane * 4));
    }

    emit_set_vi(e, t, value);

    return true;
}

inline bool emit_int_store(Emitter& e, const Instruction& ins, bool has_imm) {
    ujit::UniCompiler& uc = *e.uc;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!field) {
        return true;
    }

    ujit::Gp value = emit_get_vi(e, (int)ins.ld_t);

    if (e.vu->id == 0) {
        ujit::Vec quad = uc.new_vec128();

        uc.v_broadcast_u32(quad, value);

        emit_mem_store(e, (int)ins.ld_s, has_imm ? ins.ld_imm11 : 0, quad, field);

        return true;
    }

    ujit::Gp base = uc.new_gp_ptr();

    uc.mov(base, Imm((uintptr_t)e.vu->vu_mem));
    uc.add(base, base, emit_mem_addr(e, (int)ins.ld_s, has_imm ? ins.ld_imm11 : 0));

    for (int i = 0; i < 4; i++) {
        if (field & (8u >> i)) {
            uc.store_u32(ujit::mem_ptr(base, i * 4), value);
        }
    }

    return true;
}

inline void emit_arm_branch_reg(Emitter& e, const ujit::Gp& target, bool may_delay) {
    ujit::UniCompiler& uc = *e.uc;

    auto arm = [&]() {
        ujit::Gp two = uc.new_gp32();

        uc.mov(two, Imm(2));
        uc.store_u32(e.mem(offsetof(Vu, branch_delay)), two);
        uc.store_u32(e.mem(offsetof(Vu, branch_pc)), target);
    };

    if (!may_delay) {
        arm();

        return;
    }

    Label chained = uc.new_label();
    Label done = uc.new_label();

    ujit::Gp pending = uc.new_gp32();

    uc.load_i32(pending, e.mem(offsetof(Vu, branch_delay)));
    uc.j(chained, ujit::test_nz(pending));

    arm();

    uc.j(done);

    uc.bind(chained);

    ujit::Gp one = uc.new_gp32();

    uc.mov(one, Imm(1));
    uc.store_u8(e.mem(offsetof(Vu, delay_branch)), one);
    uc.store_u32(e.mem(offsetof(Vu, delay_branch_pc)), target);

    uc.bind(done);
}

inline ujit::Gp emit_delay_link(Emitter& e, uint32_t next_tpc, uint32_t mask, bool may_delay) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Gp link = uc.new_gp32();

    uc.mov(link, Imm((next_tpc + 1) & mask));

    if (!may_delay) {
        return link;
    }

    Label done = uc.new_label();

    ujit::Gp pending = uc.new_gp32();

    uc.load_i32(pending, e.mem(offsetof(Vu, branch_delay)));
    uc.j(done, ujit::test_z(pending));

    uc.load_u32(link, e.mem(offsetof(Vu, branch_pc)));
    uc.add(link, link, Imm(1));
    uc.and_(link, link, Imm(mask));

    uc.bind(done);

    return link;
}

inline bool emit_bal(Emitter& e, const Instruction& ins, uint32_t next_tpc,
                     uint32_t mask, bool may_delay) {
    ujit::UniCompiler& uc = *e.uc;

    uint32_t target = (next_tpc + (uint32_t)ins.ld_imm11) & mask;

    ujit::Gp link = emit_delay_link(e, next_tpc, mask, may_delay);

    uc.store_u16(e.mem(offsetof(Vu, vi) + (size_t)(ins.ld_t & 0xf) * 2), link);

    emit_arm_branch(e, target, may_delay);

    return true;
}

inline bool emit_reg_branch(Emitter& e, const Instruction& ins, uint32_t key,
                            uint32_t next_tpc, uint32_t mask, bool may_delay) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Gp target = emit_get_vi(e, (int)ins.ld_s);

    uc.and_(target, target, Imm(mask));

    if (key == 0x25) {
        ujit::Gp link = emit_delay_link(e, next_tpc, mask, may_delay);

        uc.store_u16(e.mem(offsetof(Vu, vi) + (size_t)(ins.ld_t & 0xf) * 2), link);
    }

    emit_arm_branch_reg(e, target, may_delay);

    return true;
}

inline bool emit_status_op(Emitter& e, const Instruction& ins, uint32_t key) {
    ujit::UniCompiler& uc = *e.uc;

    e.emit_status(true);

    ujit::Gp st = uc.new_gp32();

    uc.load_u32(st, e.mem(offsetof(Vu, status)));

    if (key == 0x15) {
        uc.and_(st, st, Imm(0x3f));
        uc.or_(st, st, Imm(ins.ld_imm12 & 0xfc0));
        uc.store_u32(e.mem(offsetof(Vu, status)), st);

        return true;
    }

    switch (key) {
        case 0x14: {
            uc.and_(st, st, Imm(0xfff));
            uc.select(st, Imm(1), Imm(0), ujit::cmp_eq(st, Imm(ins.ld_imm12)));
        } break;

        case 0x16: {
            uc.and_(st, st, Imm(ins.ld_imm12));
        } break;

        default: {
            uc.and_(st, st, Imm(0xfff));
            uc.or_(st, st, Imm(ins.ld_imm12));
        } break;
    }

    emit_set_vi(e, (int)ins.ld_t, st);

    return true;
}

inline bool emit_clip_op(Emitter& e, const Instruction& ins, uint32_t key) {
    ujit::UniCompiler& uc = *e.uc;

    e.flush_clip(true);

    if (key == 0x11) {
        ujit::Gp value = uc.new_gp32();

        uc.mov(value, Imm(ins.ld_imm24));
        uc.store_u32(e.mem(offsetof(Vu, clip)), value);

        return true;
    }

    ujit::Gp cf = uc.new_gp32();

    uc.load_u32(cf, e.mem(offsetof(Vu, clip_pipeline) + 3 * 4));
    uc.and_(cf, cf, Imm(0xffffff));

    if (key == 0x1c) {
        uc.and_(cf, cf, Imm(0xfff));

        emit_set_vi(e, (int)ins.ld_t, cf);

        return true;
    }

    switch (key) {
        case 0x10: {
            uc.select(cf, Imm(1), Imm(0), ujit::cmp_eq(cf, Imm(ins.ld_imm24)));
        } break;

        case 0x12: {
            uc.and_(cf, cf, Imm(ins.ld_imm24));
            uc.select(cf, Imm(1), Imm(0), ujit::test_nz(cf));
        } break;

        default: {
            uc.or_(cf, cf, Imm(ins.ld_imm24));
            uc.select(cf, Imm(1), Imm(0), ujit::cmp_eq(cf, Imm(0xffffff)));
        } break;
    }

    uc.store_u16(e.mem(offsetof(Vu, vi) + 2), cf);

    return true;
}

inline bool emit_mtir(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)(ins.ld_t & 0xf);

    if (!emit_shadow_write(e, t)) {
        return false;
    }

    ujit::Gp value = uc.new_gp32();

    uc.s_extract_u32(value, e.get((int)ins.ld_s), (uint32_t)ins.ld_sf);

    emit_set_vi(e, t, value);

    return true;
}

inline bool emit_qdiv(Emitter& e, const Instruction& ins, uint32_t key) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Gp a = uc.new_gp32();
    ujit::Gp b = uc.new_gp32();

    if (key == 0x39) {
        uc.s_extract_u32(a, e.get((int)ins.ld_t), (uint32_t)ins.ld_tf);
    } else {
        uc.s_extract_u32(a, e.get((int)ins.ld_s), (uint32_t)ins.ld_sf);
        uc.s_extract_u32(b, e.get((int)ins.ld_t), (uint32_t)ins.ld_tf);
    }

    e.emit_status(true);

    ujit::Gp result = uc.new_gp64();

    InvokeNode* call;

    if (key == 0x39) {
        call = jit_invoke(uc, (uintptr_t)&jit_sqrt_math,
                          FuncSignature::build<uint64_t, uint32_t>());

        call->set_arg(0, a);
    } else {
        call = jit_invoke(uc, (uintptr_t)(key == 0x38 ? &jit_div_math : &jit_rsqrt_math),
                          FuncSignature::build<uint64_t, uint32_t, uint32_t>());

        call->set_arg(0, a);
        call->set_arg(1, b);
    }

    call->set_ret(0, result);

    ujit::Gp st = uc.new_gp32();
    ujit::Gp flags = uc.new_gp64();

    uc.load_u32(st, e.mem(offsetof(Vu, status)));
    uc.and_(st, st, Imm(~0x30u));
    uc.mov(flags, result);
    uc.shr(flags, flags, Imm(32));
    uc.or_(st, st, flags.r32());
    uc.store_u32(e.mem(offsetof(Vu, status)), st);

    ujit::Gp old_q = uc.new_gp32();

    uc.load_u32(old_q, e.mem(offsetof(Vu, q)));
    uc.store_u32(e.mem(offsetof(Vu, prev_q)), old_q);
    uc.store_u32(e.mem(offsetof(Vu, q)), result.r32());

    e.load_scalars();

    uc.mov(e.q_delay_reg, Imm(key == 0x3a ? 13 : 7));

    return true;
}

inline bool emit_mfp(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ld_t;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!t || !field) {
        return true;
    }

    ujit::Gp p = uc.new_gp32();

    uc.load_u32(p, e.mem(offsetof(Vu, p)));

    emit_broadcast_write(e, t, field, p);

    return true;
}

inline bool emit_rget(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ld_t;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!t || !field) {
        return true;
    }

    ujit::Gp r = uc.new_gp32();

    uc.load_u32(r, e.mem(offsetof(Vu, r)));

    emit_broadcast_write(e, t, field, r);

    return true;
}

inline bool emit_rnext(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ld_t;

    if (!t) {
        return true;
    }

    ujit::Gp r = uc.new_gp32();
    ujit::Gp bit = uc.new_gp32();
    ujit::Gp tap = uc.new_gp32();

    uc.load_u32(r, e.mem(offsetof(Vu, r)));

    uc.shr(bit, r, Imm(4));
    uc.shr(tap, r, Imm(22));
    uc.xor_(bit, bit, tap);
    uc.and_(bit, bit, Imm(1));

    uc.shl(r, r, Imm(1));
    uc.xor_(r, r, bit);
    uc.and_(r, r, Imm(0x007fffff));
    uc.or_(r, r, Imm(0x3f800000));

    uc.store_u32(e.mem(offsetof(Vu, r)), r);

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (field) {
        emit_broadcast_write(e, t, field, r);
    }

    return true;
}

inline bool emit_rinit(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int s = (int)ins.ld_s;

    ujit::Gp r = uc.new_gp32();

    if (s) {
        uc.s_extract_u32(r, e.get(s), (uint32_t)ins.ld_sf);
        uc.and_(r, r, Imm(0x007fffff));
        uc.or_(r, r, Imm(0x3f800000));
    } else {
        uc.mov(r, Imm(0x3f800000));
    }

    uc.store_u32(e.mem(offsetof(Vu, r)), r);

    return true;
}

inline bool emit_rxor(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Gp r = uc.new_gp32();
    ujit::Gp v = uc.new_gp32();

    uc.load_u32(r, e.mem(offsetof(Vu, r)));
    uc.s_extract_u32(v, e.get((int)ins.ld_s), (uint32_t)ins.ld_sf);

    uc.xor_(r, r, v);
    uc.and_(r, r, Imm(0x007fffff));
    uc.or_(r, r, Imm(0x3f800000));

    uc.store_u32(e.mem(offsetof(Vu, r)), r);

    return true;
}

inline bool emit_xtop(Emitter& e, const Instruction& ins, bool itop) {
    ujit::UniCompiler& uc = *e.uc;

    if (!e.vu->vif) {
        return false;
    }

    int t = (int)(ins.ld_t & 0xf);

    if (!t) {
        return true;
    }

    const uint32_t* src = itop ? jit_vif_itop(e.vu) : jit_vif_top(e.vu);

    ujit::Gp base = uc.new_gp_ptr();
    ujit::Gp value = uc.new_gp32();

    uc.mov(base, Imm((uintptr_t)src));
    uc.load_u32(value, ujit::mem_ptr(base));

    emit_set_vi(e, t, value);

    return true;
}

inline bool emit_xgkick(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Gp addr = uc.new_gp32();

    uc.load_u16(addr, e.mem(offsetof(Vu, vi) + (size_t)(ins.ld_s & 0xf) * 2));

    jit_function_call(&uc, &vu::jit_xgkick, e.state, addr);

    return true;
}

inline bool emit_efu(Emitter& e, const Instruction& ins, uint32_t key) {
    ujit::UniCompiler& uc = *e.uc;

    if (key == 0x7b) {
        return true;
    }

    ujit::Gp selector = uc.new_gp32();

    uc.mov(selector, Imm(key));

    InvokeNode* call;

    if (key >= 0x78) {
        ujit::Gp lane = uc.new_gp32();

        uc.s_extract_u32(lane, e.get((int)ins.ld_s), (uint32_t)ins.ld_sf);

        call = jit_invoke(uc, (uintptr_t)&jit_efu_scalar,
                          FuncSignature::build<uint32_t, uint32_t, uint32_t>());

        call->set_arg(0, selector);
        call->set_arg(1, lane);
    } else {
        ujit::Vec src = e.get((int)ins.ld_s);

        ujit::Gp lane[4];

        for (int i = 0; i < 4; i++) {
            lane[i] = uc.new_gp32();

            uc.s_extract_u32(lane[i], src, (uint32_t)i);
        }

        call = jit_invoke(uc, (uintptr_t)&jit_efu_vector,
                          FuncSignature::build<uint32_t, uint32_t, uint32_t,
                                               uint32_t, uint32_t, uint32_t>());

        call->set_arg(0, selector);

        for (int i = 0; i < 4; i++) {
            call->set_arg(i + 1, lane[i]);
        }
    }

    ujit::Gp result = uc.new_gp32();

    call->set_ret(0, result);

    uc.store_u32(e.mem(offsetof(Vu, p)), result);

    return true;
}

inline bool emit_lower(Emitter& e, const Instruction& ins, uint32_t next_tpc,
                       uint32_t mask, bool may_delay, bool shadow_live) {
    uint32_t off = emit_disabled();

    switch (lower_key(ins.opcode)) {
        case LOWER_MOVE: return (off & EMIT_MOVE) ? false : emit_move(e, ins);
        case LOWER_MR32: return (off & EMIT_MOVE) ? false : emit_mr32(e, ins);
        case LOWER_MFIR: return (off & EMIT_MOVE) ? false : emit_mfir(e, ins);

        case LOWER_B: {
            if (off & EMIT_BRANCH) {
                return false;
            }

            return emit_branch(e, ins, next_tpc, mask, may_delay);
        }

        case 0x21: {
            if (off & EMIT_BRANCH) {
                return false;
            }

            return emit_bal(e, ins, next_tpc, mask, may_delay);
        }

        case 0x24:
        case 0x25: {
            if (off & EMIT_BRANCH) {
                return false;
            }

            return emit_reg_branch(e, ins, lower_key(ins.opcode), next_tpc, mask, may_delay);
        }

        case 0x18:
        case 0x1a:
        case 0x1b: {
            if (off & EMIT_FLAGS) {
                return false;
            }

            return emit_flag_compare(e, ins, lower_key(ins.opcode));
        }

        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17: {
            if (off & EMIT_FLAGS) {
                return false;
            }

            return emit_status_op(e, ins, lower_key(ins.opcode));
        }

        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x1c: {
            if (off & EMIT_FLAGS) {
                return false;
            }

            return emit_clip_op(e, ins, lower_key(ins.opcode));
        }

        case 0x28:
        case 0x29:
        case 0x2c:
        case 0x2d:
        case 0x2e:
        case 0x2f: {
            if (off & EMIT_BRANCH) {
                return false;
            }

            return emit_cond_branch(e, ins, lower_key(ins.opcode), next_tpc, mask, may_delay, shadow_live);
        }

        case 0x08:
        case 0x09:
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x74:
        case 0x75: {
            if (off & EMIT_INT) {
                return false;
            }

            return emit_int_alu(e, ins, lower_key(ins.opcode));
        }

        case 0x00: return (off & EMIT_MEM) ? false : emit_load_quad(e, ins, 0);
        case 0x01: return (off & EMIT_MEM) ? false : emit_store_quad(e, ins, 0);
        case 0x80 | 0x34: return (off & EMIT_MEM) ? false : emit_load_quad(e, ins, 1);
        case 0x80 | 0x35: return (off & EMIT_MEM) ? false : emit_store_quad(e, ins, 1);
        case 0x80 | 0x36: return (off & EMIT_MEM) ? false : emit_load_quad(e, ins, 2);
        case 0x80 | 0x37: return (off & EMIT_MEM) ? false : emit_store_quad(e, ins, 2);
        case 0x04: return (off & EMIT_MEM) ? false : emit_int_load(e, ins, true);
        case 0x05: return (off & EMIT_MEM) ? false : emit_int_store(e, ins, true);
        case 0x80 | 0x3e: return (off & EMIT_MEM) ? false : emit_int_load(e, ins, false);
        case 0x80 | 0x3f: return (off & EMIT_MEM) ? false : emit_int_store(e, ins, false);
        case 0x80 | 0x3c: return (off & EMIT_INT) ? false : emit_mtir(e, ins);
        case 0x80 | 0x40: return (off & EMIT_RAND) ? false : emit_rnext(e, ins);
        case 0x80 | 0x41: return (off & EMIT_RAND) ? false : emit_rget(e, ins);
        case 0x80 | 0x42: return (off & EMIT_RAND) ? false : emit_rinit(e, ins);
        case 0x80 | 0x43: return (off & EMIT_RAND) ? false : emit_rxor(e, ins);
        case 0x80 | 0x64: return (off & EMIT_MOVE) ? false : emit_mfp(e, ins);
        case 0x80 | 0x68: return (off & EMIT_GIF) ? false : emit_xtop(e, ins, false);
        case 0x80 | 0x69: return (off & EMIT_GIF) ? false : emit_xtop(e, ins, true);
        case 0x80 | 0x6c: return (off & EMIT_GIF) ? false : emit_xgkick(e, ins);

        case 0x80 | 0x70:
        case 0x80 | 0x71:
        case 0x80 | 0x72:
        case 0x80 | 0x73:
        case 0x80 | 0x74:
        case 0x80 | 0x75:
        case 0x80 | 0x76:
        case 0x80 | 0x78:
        case 0x80 | 0x79:
        case 0x80 | 0x7a:
        case 0x80 | 0x7b:
        case 0x80 | 0x7c:
        case 0x80 | 0x7d:
        case 0x80 | 0x7e: {
            if (off & EMIT_EFU) {
                return false;
            }

            return emit_efu(e, ins, lower_key(ins.opcode) & 0x7f);
        }

        case 0x80 | 0x38:
        case 0x80 | 0x39:
        case 0x80 | 0x3a: {
            if (off & EMIT_QDIV) {
                return false;
            }

            return emit_qdiv(e, ins, lower_key(ins.opcode) & 0x7f);
        }

        case LOWER_WAITQ: {
            if (off & EMIT_WAITQ) {
                return false;
            }

            e.load_scalars();

            e.uc->mov(e.q_delay_reg, Imm(0));

            return true;
        }
    }

    return false;
}

inline ujit::Vec emit_raw_operand(Emitter& e, const Instruction& ins, int tk) {
    ujit::UniCompiler& uc = *e.uc;

    if (tk == SRC_VEC) {
        return e.get((int)ins.ud_t);
    }

    ujit::Vec out = uc.new_vec128();

    if (tk == SRC_I) {
        ujit::Gp scalar = uc.new_gp32();

        uc.load_u32(scalar, e.mem(offsetof(Vu, i)));
        uc.v_broadcast_u32(out, scalar);

        return out;
    }

    int lane = tk - SRC_BCX;

    uc.v_swizzle_u32x4(out, e.get((int)ins.ud_t),
                       ujit::swizzle((uint8_t)lane, (uint8_t)lane, (uint8_t)lane, (uint8_t)lane));

    return out;
}

inline bool emit_minmax(Emitter& e, const Instruction& ins, bool is_max, int tk) {
    ujit::UniCompiler& uc = *e.uc;

    int d = (int)ins.ud_d;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!d || !field) {
        return true;
    }

    ujit::Vec a = e.get((int)ins.ud_s);
    ujit::Vec b = emit_raw_operand(e, ins, tk);

    ujit::Vec ka = e.t(0);
    ujit::Vec kb = e.t(1);
    ujit::Vec gt = e.t(2);

    uc.v_srai_i32(ka, a, 31);
    uc.v_and_i32(ka, ka, e.k(K_ABS));
    uc.v_xor_i32(ka, ka, a);

    uc.v_srai_i32(kb, b, 31);
    uc.v_and_i32(kb, kb, e.k(K_ABS));
    uc.v_xor_i32(kb, kb, b);

    uc.v_cmp_gt_i32(gt, ka, kb);

    ujit::Vec picked = uc.new_vec128();

    if (is_max) {
        uc.v_blendv_u8(picked, b, a, gt);
    } else {
        uc.v_blendv_u8(picked, a, b, gt);
    }

    uint8_t clean = (uint8_t)(e.clean[(int)ins.ud_s] & (tk == SRC_VEC ? e.clean[(int)ins.ud_t] : 0));

    if (field == 0xf) {
        e.set(d, picked);
        e.clean[d] = clean;
    } else {
        ujit::Vec merged = uc.new_vec128();

        uc.v_blendv_u8(merged, e.get(d), picked, e.kv(K_WRITE + (int)field));

        e.set(d, merged);
        e.clean[d] = (uint8_t)((e.clean[d] & ~field) | (clean & field));
    }

    return true;
}

inline bool emit_ftoi(Emitter& e, const Instruction& ins, int scale) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ud_t;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    ujit::Vec f = uc.new_vec128();

    uc.v_mul_f32(f, emit_load_cvtf(e, (int)ins.ud_s, 0xf), e.k(K_SCALE + scale));

    if (!t || !field) {
        return true;
    }

    ujit::Vec iv = uc.new_vec128();
    ujit::Vec over = e.t(0);

    uc.v_cvt_trunc_f32_to_i32(iv, f);
    uc.v_cmp_ge_f32(over, f, e.k(K_2P31));
    uc.v_blendv_u8(iv, iv, e.kv(K_ABS), over);

    if (field == 0xf) {
        e.set(t, iv);
    } else {
        ujit::Vec merged = uc.new_vec128();

        uc.v_blendv_u8(merged, e.get(t), iv, e.kv(K_WRITE + (int)field));

        e.set(t, merged);
    }

    e.clean[t] = (uint8_t)(e.clean[t] & ~field);

    return true;
}

inline bool emit_itof(Emitter& e, const Instruction& ins, int scale) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ud_t;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!t || !field) {
        return true;
    }

    ujit::Vec f = uc.new_vec128();

    uc.v_cvt_i32_to_f32(f, e.get((int)ins.ud_s));
    uc.v_mul_f32(f, f, e.k(K_SCALE + 4 + scale));

    if (field == 0xf) {
        e.set(t, f);
    } else {
        ujit::Vec merged = uc.new_vec128();

        uc.v_blendv_u8(merged, e.get(t), f, e.kv(K_WRITE + (int)field));

        e.set(t, merged);
    }

    e.clean[t] = (uint8_t)(e.clean[t] & ~field);

    return true;
}

inline bool emit_abs(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    int t = (int)ins.ud_t;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    if (!t || !field) {
        return true;
    }

    ujit::Vec out = uc.new_vec128();

    uc.v_and_i32(out, emit_load_cvtf(e, (int)ins.ud_s, field), e.k(K_ABS));

    if (field == 0xf) {
        e.set_clamped(t, out, field);
    } else {
        ujit::Vec merged = uc.new_vec128();

        uc.v_blendv_u8(merged, e.get(t), out, e.kv(K_WRITE + (int)field));

        e.set_clamped(t, merged, field);
    }

    return true;
}

inline bool emit_opmula(Emitter& e, const Instruction& ins, bool emit_status) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Vec sv = emit_load_cvtf(e, (int)ins.ud_s, 0xe);
    ujit::Vec tv = emit_load_cvtf(e, (int)ins.ud_t, 0xe);

    ujit::Vec syzx = uc.new_vec128();
    ujit::Vec tzxy = uc.new_vec128();

    uc.v_swizzle_u32x4(syzx, sv, ujit::swizzle(3, 0, 2, 1));
    uc.v_swizzle_u32x4(tzxy, tv, ujit::swizzle(3, 1, 0, 2));

    ujit::Vec prod = uc.new_vec128();

    uc.v_mul_f32(prod, syzx, tzxy);

    ujit::Vec pre = emit_cvtf(e, prod);
    ujit::Vec clamped = emit_flags4(e, pre, 0xe);

    ujit::Vec merged = uc.new_vec128();

    uc.v_blendv_u8(merged, e.get(REG_ACC), clamped, e.kv(K_WRITE + 0xe));

    e.set_clamped(REG_ACC, merged, 0xe);

    if (emit_status) {
        emit_update_status(e);
    }

    return true;
}

inline bool emit_opmsub(Emitter& e, const Instruction& ins, bool emit_status) {
    ujit::UniCompiler& uc = *e.uc;

    ujit::Vec sv = emit_load_cvtf(e, (int)ins.ud_s, 0xe);
    ujit::Vec tv = emit_load_cvtf(e, (int)ins.ud_t, 0xe);

    ujit::Vec syzx = uc.new_vec128();
    ujit::Vec tzxy = uc.new_vec128();

    uc.v_swizzle_u32x4(syzx, sv, ujit::swizzle(3, 0, 2, 1));
    uc.v_swizzle_u32x4(tzxy, tv, ujit::swizzle(3, 1, 0, 2));

    ujit::Vec prod = uc.new_vec128();
    ujit::Vec diff = uc.new_vec128();

    uc.v_mul_f32(prod, syzx, tzxy);
    uc.v_sub_f32(diff, e.get(REG_ACC), prod);

    ujit::Vec clamped = emit_flags4(e, diff, 0xe);

    int d = (int)ins.ud_d;

    if (d) {
        ujit::Vec merged = uc.new_vec128();

        uc.v_blendv_u8(merged, e.get(d), clamped, e.kv(K_WRITE + 0xe));

        e.set_clamped(d, merged, 0xe);
    }

    if (emit_status) {
        emit_update_status(e);
    }

    return true;
}

inline bool emit_clipw(Emitter& e, const Instruction& ins) {
    ujit::UniCompiler& uc = *e.uc;

    e.flush_clip(true);

    ujit::Vec sv = e.get((int)ins.ud_s);
    ujit::Vec tv = e.get((int)ins.ud_t);

    ujit::Vec sflat = uc.new_vec128();
    ujit::Vec wabs = uc.new_vec128();
    ujit::Vec zmask = uc.new_vec128();
    ujit::Vec szero = uc.new_vec128();

    uc.v_and_i32(zmask, sv, e.k(K_EXP));
    uc.v_cmp_eq_i32(zmask, zmask, e.k(K_ZERO));
    uc.v_and_i32(szero, sv, e.k(K_SIGN));
    uc.v_blendv_u8(sflat, sv, szero, zmask);

    uc.v_swizzle_u32x4(wabs, tv, ujit::swizzle(3, 3, 3, 3));
    uc.v_and_i32(wabs, wabs, e.k(K_ABS));

    uc.v_and_i32(zmask, wabs, e.k(K_EXP));
    uc.v_cmp_eq_i32(zmask, zmask, e.k(K_ZERO));
    uc.v_blendv_u8(wabs, wabs, e.kv(K_ZERO), zmask);

    ujit::Vec ks = e.t(0);
    ujit::Vec kwm = e.t(1);
    ujit::Vec gt = e.t(2);
    ujit::Vec lt = e.t(3);

    uc.v_srai_i32(ks, sflat, 31);
    uc.v_and_i32(ks, ks, e.k(K_ABS));
    uc.v_xor_i32(ks, ks, sflat);

    uc.v_xor_i32(kwm, wabs, e.k(K_ONES));

    uc.v_cmp_gt_i32(gt, ks, wabs);
    uc.v_cmp_gt_i32(lt, kwm, ks);

    uc.v_and_i32(gt, gt, e.k(K_CLIPGT));
    uc.v_and_i32(lt, lt, e.k(K_CLIPLT));
    uc.v_or_i32(gt, gt, lt);

    ujit::Vec fold = uc.new_vec128();

    uc.v_swizzle_u32x4(fold, gt, ujit::swizzle(2, 3, 0, 1));
    uc.v_or_i32(gt, gt, fold);
    uc.v_swizzle_u32x4(fold, gt, ujit::swizzle(1, 0, 3, 2));
    uc.v_or_i32(gt, gt, fold);

    ujit::Gp bits = uc.new_gp32();
    ujit::Gp clip = uc.new_gp32();

    uc.s_extract_u32(bits, gt, 0);

    uc.load_u32(clip, e.mem(offsetof(Vu, clip)));
    uc.shl(clip, clip, Imm(6));
    uc.or_(clip, clip, bits);
    uc.and_(clip, clip, Imm(0xffffff));
    uc.store_u32(e.mem(offsetof(Vu, clip)), clip);

    return true;
}

inline bool emit_upper_extra(Emitter& e, const Instruction& ins, bool after_lower) {
    uint32_t key = upper_key(ins.opcode);

    bool no_minmax = emit_off(EMIT_MINMAX);
    bool no_conv = emit_off(EMIT_CONV);

    if (key < 0x80) {
        if (key == 0x2e) {
            return emit_off(EMIT_FMAC) ? false : emit_opmsub(e, ins, after_lower);
        }

        if (no_minmax) {
            return false;
        }

        if (key <= 0x13) {
            return emit_minmax(e, ins, true, SRC_BCX + (int)(key - 0x10));
        }

        if (key <= 0x17) {
            return emit_minmax(e, ins, false, SRC_BCX + (int)(key - 0x14));
        }

        switch (key) {
            case 0x1d: return emit_minmax(e, ins, true, SRC_I);
            case 0x1f: return emit_minmax(e, ins, false, SRC_I);
            case 0x2b: return emit_minmax(e, ins, true, SRC_VEC);
            case 0x2f: return emit_minmax(e, ins, false, SRC_VEC);
        }

        return false;
    }

    uint32_t sub = key & 0x7f;

    if (sub == 0x2e) {
        return emit_off(EMIT_FMAC) ? false : emit_opmula(e, ins, after_lower);
    }

    if (no_conv) {
        return false;
    }

    if (sub >= 0x10 && sub <= 0x13) {
        return emit_itof(e, ins, (int)(sub - 0x10));
    }

    if (sub >= 0x14 && sub <= 0x17) {
        return emit_ftoi(e, ins, (int)(sub - 0x14));
    }

    if (sub == 0x1d) {
        return emit_abs(e, ins);
    }

    if (sub == 0x1f) {
        return emit_off(EMIT_CLIPW) ? false : emit_clipw(e, ins);
    }

    return false;
}

inline void emit_fmac(Emitter& e, const Instruction& ins, const Fmac& f, bool emit_status) {
    ujit::UniCompiler& uc = *e.uc;

    uint32_t field = (ins.opcode >> 21) & 0xf;

    ujit::Vec sv = emit_load_cvtf(e, (int)ins.ud_s, field);
    ujit::Vec tv;

    switch (f.tk) {
        case SRC_VEC: {
            tv = emit_load_cvtf(e, (int)ins.ud_t, field);
        } break;

        case SRC_I:
        case SRC_Q: {
            ujit::Gp scalar = uc.new_gp32();

            if (f.tk == SRC_I) {
                uc.load_u32(scalar, e.mem(offsetof(Vu, i)));
            } else {
                ujit::Gp pending = uc.new_gp32();
                ujit::Gp current = uc.new_gp32();

                e.load_scalars();

                uc.load_u32(current, e.mem(offsetof(Vu, q)));
                uc.load_u32(pending, e.mem(offsetof(Vu, prev_q)));
                uc.select(scalar, pending, current, ujit::test_nz(e.q_delay_reg));
            }

            tv = uc.new_vec128();

            uc.v_broadcast_u32(tv, scalar);
        } break;

        default: {
            int lane = f.tk - SRC_BCX;

            ujit::Vec tf = emit_load_cvtf(e, (int)ins.ud_t, 8u >> lane);

            tv = uc.new_vec128();

            uc.v_swizzle_u32x4(tv, tf, ujit::swizzle((uint8_t)lane, (uint8_t)lane,
                                                     (uint8_t)lane, (uint8_t)lane));
        } break;
    }

    ujit::Vec result = uc.new_vec128();

    switch (f.op) {
        case OP_ADD: uc.v_add_f32(result, sv, tv); break;
        case OP_SUB: uc.v_sub_f32(result, sv, tv); break;
        case OP_MUL: uc.v_mul_f32(result, sv, tv); break;

        default: {
            ujit::Vec acc = emit_load_cvtf(e, REG_ACC, field);
            ujit::Vec prod = uc.new_vec128();

            uc.v_mul_f32(prod, sv, tv);

            if (f.op == OP_MADD) {
                uc.v_add_f32(result, acc, prod);
            } else {
                uc.v_sub_f32(result, acc, prod);
            }
        } break;
    }

    ujit::Vec clamped = emit_flags4(e, result, field);

    int dst = f.to_acc ? REG_ACC : (int)ins.ud_d;

    if (f.to_acc || dst) {
        if (field == 0xf) {
            e.set_clamped(dst, clamped, field);
        } else if (field) {
            ujit::Vec cur = e.get(dst);
            ujit::Vec merged = uc.new_vec128();

            uc.v_blendv_u8(merged, cur, clamped, e.kv(K_WRITE + (int)field));

            e.set_clamped(dst, merged, field);
        }
    }

    if (emit_status) {
        emit_update_status(e);
    }
}

}
