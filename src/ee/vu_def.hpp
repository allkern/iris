#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "vu.hpp"

namespace iris::gif { struct Gif; }
namespace iris::vif { struct Vif; }

namespace iris::vu {

struct BlockEntry {
    Instruction upper, lower;
    int i_bit;
    int e_bit;
    int m_bit;
    int hazard0;
    int hazard1;
    int hazard2;
    int hazard3;
    int branch;

    uint8_t uw_reg, uw_mask;
    uint8_t lw_reg, lw_mask;
    uint8_t is_mtir;
    uint8_t mtir_reg, mtir_comp;
    uint8_t is_waitq;
    uint8_t lower_is_nop;
};

struct Block {
    std::vector <BlockEntry> entries;

    uint32_t tpc;
    int cycles = 0;
};

struct Vu {
    Reg128 vf[32];
    uint16_t vi[16];
    Reg128 acc;

    std::vector <Block> block_cache;
    int block_cache_size;

    // Single-entry block cache for fast lookup (avoid hash computation)
    uint32_t last_block_lookup_tpc;
    Block* last_block_ptr;

    uint64_t cache_hits;
    uint64_t cache_misses;

    Instruction upper, lower;

    struct {
        struct {
            uint8_t reg;
            uint8_t field;
        } dst;
    } upper_pipeline[4], lower_pipeline[4];

    int vi_backup_cycles;
    int vi_backup_reg;
    int vi_backup_value;

    int branch_delay;
    uint32_t branch_pc;
    bool delay_branch;
    uint32_t delay_branch_pc;

    bool waiting_for_interlock;

    uint64_t micro_mem[0x800];
    uint128_t vu_mem[0x400];

    int micro_mem_size;
    int vu_mem_size;
    int id;

    int i_bit;
    int e_bit;
    int m_bit;
    int d_bit;
    int t_bit;

    // MAC flags pipeline
    uint32_t mac_pipeline[4];
    uint32_t clip_pipeline[4];

    uint64_t vu_cycle;
    uint64_t vf_ready[32][4];

    int q_delay;
    Reg32 prev_q;
    Reg32 p;

    int xgkick_pending;
    int xgkick_addr;

    union {
        uint32_t cr[16];

        struct {
            uint32_t status;
            uint32_t mac;
            uint32_t clip;
            uint32_t rsv0;
            Reg32 r;
            Reg32 i;
            Reg32 q;
            uint32_t rsv1;
            uint32_t rsv2;
            uint32_t rsv3;
            uint32_t tpc;
            uint32_t cmsar0;
            uint32_t fbrst;
            uint32_t vpu_stat;
            uint32_t rsv4;
            uint32_t cmsar1;
        };
    };

    bool disable;

    gif::Gif* gif;
    vif::Vif* vif;
    Vu* vu1;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

// Upper pipeline
template <uint32_t di> void i_abs(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_add(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addi(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addx(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addy(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_adda(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addai(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addaq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addax(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_adday(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addaz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_addaw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_sub(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subi(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subx(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_suby(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_suba(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subai(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subaq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subax(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subay(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subaz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_subaw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mul(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_muli(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulx(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_muly(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mula(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulai(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulaq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulax(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulay(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulaz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mulaw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_madd(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddi(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddx(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddy(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_madda(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddai(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddaq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddax(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_madday(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddaz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maddaw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msub(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubi(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubx(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msuby(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msuba(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubai(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubaq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubax(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubay(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubaz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_msubaw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_max(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maxi(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maxx(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maxy(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maxz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_maxw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mini(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_minii(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_minix(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_miniy(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_miniz(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_miniw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_ftoi0(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_ftoi4(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_ftoi12(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_ftoi15(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_itof0(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_itof4(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_itof12(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_itof15(Vu* vu, const Instruction* ins);
void i_opmula(Vu* vu, const Instruction* ins);
void i_opmsub(Vu* vu, const Instruction* ins);
void i_nop(Vu* vu, const Instruction* ins);
void i_clip(Vu* vu, const Instruction* ins);

// Lower pipeline
template <uint32_t di> void i_ilw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_ilwr(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_isw(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_iswr(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_lq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_lqd(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_lqi(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mfir(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mfp(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_move(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_mr32(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_rget(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_rnext(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_sq(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_sqd(Vu* vu, const Instruction* ins);
template <uint32_t di> void i_sqi(Vu* vu, const Instruction* ins);
void i_b(Vu* vu, const Instruction* ins);
void i_bal(Vu* vu, const Instruction* ins);
void i_div(Vu* vu, const Instruction* ins);
void i_eatan(Vu* vu, const Instruction* ins);
void i_eatanxy(Vu* vu, const Instruction* ins);
void i_eatanxz(Vu* vu, const Instruction* ins);
void i_eexp(Vu* vu, const Instruction* ins);
void i_eleng(Vu* vu, const Instruction* ins);
void i_ercpr(Vu* vu, const Instruction* ins);
void i_erleng(Vu* vu, const Instruction* ins);
void i_ersadd(Vu* vu, const Instruction* ins);
void i_ersqrt(Vu* vu, const Instruction* ins);
void i_esadd(Vu* vu, const Instruction* ins);
void i_esin(Vu* vu, const Instruction* ins);
void i_esqrt(Vu* vu, const Instruction* ins);
void i_esum(Vu* vu, const Instruction* ins);
void i_fcand(Vu* vu, const Instruction* ins);
void i_fceq(Vu* vu, const Instruction* ins);
void i_fcget(Vu* vu, const Instruction* ins);
void i_fcor(Vu* vu, const Instruction* ins);
void i_fcset(Vu* vu, const Instruction* ins);
void i_fmand(Vu* vu, const Instruction* ins);
void i_fmeq(Vu* vu, const Instruction* ins);
void i_fmor(Vu* vu, const Instruction* ins);
void i_fsand(Vu* vu, const Instruction* ins);
void i_fseq(Vu* vu, const Instruction* ins);
void i_fsor(Vu* vu, const Instruction* ins);
void i_fsset(Vu* vu, const Instruction* ins);
void i_iadd(Vu* vu, const Instruction* ins);
void i_iaddi(Vu* vu, const Instruction* ins);
void i_iaddiu(Vu* vu, const Instruction* ins);
void i_iand(Vu* vu, const Instruction* ins);
void i_ibeq(Vu* vu, const Instruction* ins);
void i_ibgez(Vu* vu, const Instruction* ins);
void i_ibgtz(Vu* vu, const Instruction* ins);
void i_iblez(Vu* vu, const Instruction* ins);
void i_ibltz(Vu* vu, const Instruction* ins);
void i_ibne(Vu* vu, const Instruction* ins);
void i_ior(Vu* vu, const Instruction* ins);
void i_isub(Vu* vu, const Instruction* ins);
void i_isubiu(Vu* vu, const Instruction* ins);
void i_jalr(Vu* vu, const Instruction* ins);
void i_jr(Vu* vu, const Instruction* ins);
void i_mtir(Vu* vu, const Instruction* ins);
void i_rinit(Vu* vu, const Instruction* ins);
void i_rsqrt(Vu* vu, const Instruction* ins);
void i_rxor(Vu* vu, const Instruction* ins);
void i_sqrt(Vu* vu, const Instruction* ins);
void i_waitp(Vu* vu, const Instruction* ins);
void i_waitq(Vu* vu, const Instruction* ins);
void i_xgkick(Vu* vu, const Instruction* ins);
void i_xitop(Vu* vu, const Instruction* ins);
void i_xtop(Vu* vu, const Instruction* ins);

}
