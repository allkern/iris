#ifndef SIF_H
#define SIF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SIF_EE_SIDE 0
#define SIF_IOP_SIDE 1

struct ps2_sif {
    int side;
    uint32_t mscom;
    uint32_t smcom;
    uint32_t msflg;
    uint32_t smflg;
    uint32_t ctrl;
    uint32_t bd6;

    struct ps2_sif* iop;
    struct ps2_sif* ee;
};

struct ps2_sif* ps2_sif_create(void);
void ps2_sif_init(struct ps2_sif* sif, int side);
void ps2_sif_destroy(struct ps2_sif* sif);
uint64_t ps2_sif_read32(struct ps2_sif* sif, uint32_t addr);
void ps2_sif_write32(struct ps2_sif* sif, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif