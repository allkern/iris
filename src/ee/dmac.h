struct ps2_dmac;

#ifndef DMAC_H
#define DMAC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"

#include "shared/sif.h"

#include "bus_decl.h"
#include "ee.h"

#include "iop/dma.h"

#define TAG_QWC(d) (d.u64[0] & 0xffff)
#define TAG_PCT(d) ((d.u64[0] >> 26) & 3)
#define TAG_ID(d) ((d.u64[0] >> 28) & 7)
#define TAG_IRQ(d) ((d.u64[0] >> 31) & 1)
#define TAG_ADDR(d) ((d.u64[0] >> 32) & 0xfffffff0)
#define TAG_DATA(d) (d.u64[1])

#define DMAC_VIF0 0
#define DMAC_VIF1 1
#define DMAC_GIF 2
#define DMAC_IPU_FROM 3
#define DMAC_IPU_TO 4
#define DMAC_SIF0 5
#define DMAC_SIF1 6
#define DMAC_SIF2 7
#define DMAC_SPR_FROM 8
#define DMAC_SPR_TO 9
#define DMAC_MEIS 14

struct dmac_tag {
    uint64_t qwc;
    uint64_t pct;
    uint64_t id;
    uint64_t irq;
    uint64_t addr;
    uint64_t data;
    int end;
};

struct dmac_channel {
    uint32_t chcr;
    uint32_t madr;
    uint32_t tadr;
    uint32_t qwc;
    uint32_t asr0;
    uint32_t asr1;
    uint32_t sadr;

    int dreq;

    struct dmac_tag tag;
};

struct ps2_dmac {
    struct ee_bus* bus;

    struct dmac_channel vif0;
    struct dmac_channel vif1;
    struct dmac_channel gif;
    struct dmac_channel ipu_from;
    struct dmac_channel ipu_to;
    struct dmac_channel sif0;
    struct dmac_channel sif1;
    struct dmac_channel sif2;
    struct dmac_channel spr_from;
    struct dmac_channel spr_to;
    struct dmac_channel* mfifo_drain;

    uint32_t ctrl;
    uint32_t stat;
    uint32_t pcr;
    uint32_t sqwc;
    uint32_t rbsr;
    uint32_t rbor;
    uint32_t enable;

    struct ps2_ram* spr;
    struct ps2_sif* sif;
    struct ps2_iop_dma* iop_dma;
    struct ee_state* ee;
    struct sched_state* sched;
};

struct ps2_dmac* ps2_dmac_create(void);
void ps2_dmac_init(struct ps2_dmac* dmac, struct ps2_sif* sif, struct ps2_iop_dma* iop_dma, struct ps2_ram* spr, struct ee_state* ee, struct sched_state* sched, struct ee_bus* bus);
void ps2_dmac_destroy(struct ps2_dmac* dmac);
uint64_t ps2_dmac_read8(struct ps2_dmac* dmac, uint32_t addr);
uint64_t ps2_dmac_read16(struct ps2_dmac* dmac, uint32_t addr);
uint64_t ps2_dmac_read32(struct ps2_dmac* dmac, uint32_t addr);
void ps2_dmac_write8(struct ps2_dmac* dmac, uint32_t addr, uint64_t data);
void ps2_dmac_write16(struct ps2_dmac* dmac, uint32_t addr, uint64_t data);
void ps2_dmac_write32(struct ps2_dmac* dmac, uint32_t addr, uint64_t data);

void dmac_handle_vif0_transfer(struct ps2_dmac* dmac);
void dmac_handle_vif1_transfer(struct ps2_dmac* dmac);
void dmac_handle_gif_transfer(struct ps2_dmac* dmac);
void dmac_handle_ipu_from_transfer(struct ps2_dmac* dmac);
void dmac_handle_ipu_to_transfer(struct ps2_dmac* dmac);
void dmac_handle_sif0_transfer(struct ps2_dmac* dmac);
void dmac_handle_sif1_transfer(struct ps2_dmac* dmac);
void dmac_handle_sif2_transfer(struct ps2_dmac* dmac);
void dmac_handle_spr_from_transfer(struct ps2_dmac* dmac);
void dmac_handle_spr_to_transfer(struct ps2_dmac* dmac);

#ifdef __cplusplus
}
#endif

#endif