#ifndef SIF_H
#define SIF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"

#define SIF_EE_SIDE 0
#define SIF_IOP_SIDE 1

struct sif_fifo {
    int read_index;
    int write_index;
    int ready;
    uint128_t data[128];
};

struct ps2_sif {
    uint32_t mscom;
    uint32_t smcom;
    uint32_t msflg;
    uint32_t smflg;
    uint32_t ctrl;
    uint32_t bd6;

    struct sif_fifo fifo;
};

struct ps2_sif* ps2_sif_create(void);
void ps2_sif_init(struct ps2_sif* sif);
void ps2_sif_destroy(struct ps2_sif* sif);
uint64_t ps2_sif_read32(struct ps2_sif* sif, uint32_t addr);
void ps2_sif_write32(struct ps2_sif* sif, uint32_t addr, uint64_t data);

// DMA stuff
void ps2_sif_fifo_write(struct ps2_sif* sif, uint128_t data);
uint128_t ps2_sif_fifo_read(struct ps2_sif* sif);
void ps2_sif_fifo_reset(struct ps2_sif* sif);
int ps2_sif_fifo_is_empty(struct ps2_sif* sif);

#ifdef __cplusplus
}
#endif

#endif