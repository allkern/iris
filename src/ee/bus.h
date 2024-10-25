#ifndef EE_BUS_H
#define EE_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u128.h"

#include "dmac.h"
#include "gif.h"

#include "shared/ram.h"
#include "shared/sif.h"
#include "shared/bios.h"

struct ee_bus {
    // EE-only
    struct ps2_ram* ee_ram;
    struct ps2_dmac* dmac;
    struct ps2_gif* gif;

    // EE/IOP
    struct ps2_bios* bios;
    struct ps2_ram* iop_ram;
    struct ps2_sif* sif;

    uint32_t mch_ricm;
    uint32_t mch_drd;
    uint32_t rdram_sdevid;
};

void ee_bus_init_ram(struct ee_bus* bus, struct ps2_ram* ram);
void ee_bus_init_dmac(struct ee_bus* bus, struct ps2_dmac* dmac);
void ee_bus_init_gif(struct ee_bus* bus, struct ps2_gif* gif);
void ee_bus_init_bios(struct ee_bus* bus, struct ps2_bios* bios);
void ee_bus_init_iop_ram(struct ee_bus* bus, struct ps2_ram* iop_ram);
void ee_bus_init_sif(struct ee_bus* bus, struct ps2_sif* sif);

#ifdef __cplusplus
}
#endif

#endif