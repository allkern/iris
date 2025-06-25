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
#include "scheduler.h"

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
    uint32_t stat;
    uint32_t fbrst;
    uint32_t err;
    uint32_t mark;
    uint32_t cycle;
    uint32_t mode;
    uint32_t num;
    uint32_t mask;
    uint32_t code;
    uint32_t itops;
    uint32_t base;
    uint32_t ofst;
    uint32_t tops;
    uint32_t itop;
    uint32_t top;
    uint32_t r[4];
    uint32_t c[4];

    int state;
    int pending_words;
    int shift;
    uint32_t cmd;
    uint128_t data;

    uint32_t addr;
    uint32_t unpack_fmt;
    uint32_t unpack_usn;
    uint32_t unpack_cl;
    uint32_t unpack_wl;
    uint32_t unpack_skip;
    uint32_t unpack_wl_count;
    uint32_t unpack_buf[16];

    int id;

    struct vu_state* vu;
    struct sched_state* sched;
    struct ps2_gif* gif;
    struct ps2_intc* intc;
    struct ee_bus* bus;
};

struct ps2_vif* ps2_vif_create(void);
void ps2_vif_init(struct ps2_vif* vif, int id, struct vu_state* vu, struct ps2_gif* gif, struct ps2_intc* intc, struct sched_state* sched, struct ee_bus* bus);
void ps2_vif_destroy(struct ps2_vif* vif);
uint64_t ps2_vif_read32(struct ps2_vif* vif, uint32_t addr);
void ps2_vif_write32(struct ps2_vif* vif, uint32_t addr, uint64_t data);
uint128_t ps2_vif_read128(struct ps2_vif* vif, uint32_t addr);
void ps2_vif_write128(struct ps2_vif* vif, uint32_t addr, uint128_t data);

#ifdef __cplusplus
}
#endif

#endif