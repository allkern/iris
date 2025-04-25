#ifndef FW_H
#define FW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "intc.h"

struct ps2_fw {
    uint32_t intr0;
    uint32_t intr1;
    uint32_t intr2;
    uint32_t intr0mask;
    uint32_t intr1mask;
    uint32_t intr2mask;
    uint32_t ctrl0;
    uint32_t ctrl1;
    uint32_t ctrl2;
    uint32_t dma_ctrl_sr0;
    uint32_t dma_ctrl_sr1;
    uint32_t phy_access;
    uint8_t phy_r[16];

    struct ps2_iop_intc* intc;
};

struct ps2_fw* ps2_fw_create(void);
void ps2_fw_init(struct ps2_fw* fw, struct ps2_iop_intc* intc);
void ps2_fw_destroy(struct ps2_fw* fw);
uint64_t ps2_fw_read32(struct ps2_fw* fw, uint32_t addr);
void ps2_fw_write32(struct ps2_fw* fw, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif