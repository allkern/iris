#pragma once

#include <cstdint>

#include "vu.h"

struct vu_state {
    struct vu_reg128 vf[32];
    uint16_t vi[16];
    struct vu_reg128 acc;

    struct vu_instruction upper, lower;

    struct {
        struct {
            uint8_t reg;
            uint8_t field;
        } dst;
    } upper_pipeline[4], lower_pipeline[4];

	int vi_backup_cycles;
    int vi_backup_reg;
    int vi_backup_value;

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
    uint32_t next_tpc;

    // MAC flags pipeline
    uint32_t mac_pipeline[4];
    uint32_t clip_pipeline[4];

    int q_delay;
    struct vu_reg32 prev_q;
    struct vu_reg32 p;

    int xgkick_pending;
    int xgkick_addr;

    union {
        uint32_t cr[16];

        struct {
            uint32_t status;
            uint32_t mac;
            uint32_t clip;
            uint32_t rsv0;
            struct vu_reg32 r;
            struct vu_reg32 i;
            struct vu_reg32 q;
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

    struct ps2_gif* gif;
    struct ps2_vif* vif;
    struct vu_state* vu1;
};