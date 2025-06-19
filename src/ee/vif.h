#ifndef VIF_H
#define VIF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"
#include "bus.h"

#include "ee/intc.h"
#include "ee/vu.h"
#include "sched.h"

enum {
    VIF_IDLE,
    VIF_RECV_DATA
};

#define VIF_CMD_NOP 0x00
#define VIF_CMD_STCYCL 0x01
#define VIF_CMD_OFFSET 0x02
#define VIF_CMD_BASE 0x03
#define VIF_CMD_ITOP 0x04
#define VIF_CMD_STMOD 0x05
#define VIF_CMD_MSKPATH3 0x06
#define VIF_CMD_MARK 0x07
#define VIF_CMD_FLUSHE 0x10
#define VIF_CMD_FLUSH 0x11
#define VIF_CMD_FLUSHA 0x13
#define VIF_CMD_MSCAL 0x14
#define VIF_CMD_MSCALF 0x15
#define VIF_CMD_MSCNT 0x17
#define VIF_CMD_STMASK 0x20
#define VIF_CMD_STROW 0x30
#define VIF_CMD_STCOL 0x31
#define VIF_CMD_MPG 0x4A
#define VIF_CMD_DIRECT 0x50
#define VIF_CMD_DIRECTHL 0x51
// 60h-7Fh UNPACK

#define UNPACK_S_32  0
#define UNPACK_S_16  1
#define UNPACK_S_8   2
#define UNPACK_V2_32 4
#define UNPACK_V2_16 5
#define UNPACK_V2_8  6
#define UNPACK_V3_32 8
#define UNPACK_V3_16 9
#define UNPACK_V3_8  10
#define UNPACK_V4_32 12
#define UNPACK_V4_16 13
#define UNPACK_V4_8  14
#define UNPACK_V4_5  15

struct ps2_vif {
    uint32_t vif0_stat;
    uint32_t vif0_fbrst;
    uint32_t vif0_err;
    uint32_t vif0_mark;
    uint32_t vif0_cycle;
    uint32_t vif0_mode;
    uint32_t vif0_num;
    uint32_t vif0_mask;
    uint32_t vif0_code;
    uint32_t vif0_itops;
    uint32_t vif0_itop;
    uint32_t vif0_r[4];
    uint32_t vif0_c[4];
    uint32_t vif1_stat;
    uint32_t vif1_fbrst;
    uint32_t vif1_err;
    uint32_t vif1_mark;
    uint32_t vif1_cycle;
    uint32_t vif1_mode;
    uint32_t vif1_num;
    uint32_t vif1_mask;
    uint32_t vif1_code;
    uint32_t vif1_itops;
    uint32_t vif1_base;
    uint32_t vif1_ofst;
    uint32_t vif1_tops;
    uint32_t vif1_itop;
    uint32_t vif1_top;
    uint32_t vif1_r[4];
    uint32_t vif1_c[4];

    int vif0_state;
    uint32_t vif0_cmd;
    int vif0_pending_words;
    int vif1_state;
    uint32_t vif1_cmd;
    int vif1_pending_words;
    int vif1_shift;
    int vif0_shift;
    uint128_t vif1_data;
    uint128_t vif0_data;

    uint32_t vif1_addr;
    uint32_t vif1_unpack_fmt;
    uint32_t vif1_unpack_usn;

    struct vu_state* vu0;
    struct vu_state* vu1;

    struct sched_state* sched;
    struct ps2_intc* intc;
    struct ee_bus* bus;
};

struct ps2_vif* ps2_vif_create(void);
void ps2_vif_init(struct ps2_vif* vif, struct vu_state* vu0, struct vu_state* vu1, struct ps2_intc* intc, struct sched_state* sched, struct ee_bus* bus);
void ps2_vif_destroy(struct ps2_vif* vif);
uint64_t ps2_vif_read32(struct ps2_vif* vif, uint32_t addr);
void ps2_vif_write32(struct ps2_vif* vif, uint32_t addr, uint64_t data);
uint128_t ps2_vif_read128(struct ps2_vif* vif, uint32_t addr);
void ps2_vif_write128(struct ps2_vif* vif, uint32_t addr, uint128_t data);

#ifdef __cplusplus
}
#endif

#endif