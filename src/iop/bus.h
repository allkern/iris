#ifndef IOP_BUS_H
#define IOP_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u128.h"

#include "shared/ram.h"
#include "shared/sif.h"
#include "shared/bios.h"

#include "iop/dma.h"
#include "iop/intc.h"

struct iop_bus {
    // EE/IOP
    struct ps2_bios* bios;
    struct ps2_ram* iop_ram;
    struct ps2_sif* sif;
    struct ps2_iop_dma* dma;
    struct ps2_iop_intc* intc;
};

void iop_bus_init_bios(struct iop_bus* bus, struct ps2_bios* bios);
void iop_bus_init_iop_ram(struct iop_bus* bus, struct ps2_ram* iop_ram);
void iop_bus_init_sif(struct iop_bus* bus, struct ps2_sif* sif);
void iop_bus_init_dma(struct iop_bus* bus, struct ps2_iop_dma* dma);
void iop_bus_init_intc(struct iop_bus* bus, struct ps2_iop_intc* intc);

#ifdef __cplusplus
}
#endif

#endif