#ifndef EE_BUS_H
#define EE_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u128.h"

#include "dmac.h"
#include "gif.h"
#include "intc.h"

#include "shared/ram.h"
#include "shared/sif.h"
#include "shared/bios.h"

struct ee_bus {
    // EE-only
    struct ps2_ram* ee_ram;
    struct ps2_dmac* dmac;
    struct ps2_gif* gif;
    struct ps2_gs* gs;
    struct ps2_intc* intc;

    // EE/IOP
    struct ps2_bios* bios;
    struct ps2_ram* iop_ram;
    struct ps2_sif* sif;

    uint32_t mch_ricm;
    uint32_t mch_drd;
    uint32_t rdram_sdevid;

    void (*kputchar)(void*, char);
    void* kputchar_udata;
};

void ee_bus_init_ram(struct ee_bus* bus, struct ps2_ram* ram);
void ee_bus_init_dmac(struct ee_bus* bus, struct ps2_dmac* dmac);
void ee_bus_init_intc(struct ee_bus* bus, struct ps2_intc* intc);
void ee_bus_init_gif(struct ee_bus* bus, struct ps2_gif* gif);
void ee_bus_init_gs(struct ee_bus* bus, struct ps2_gs* gs);
void ee_bus_init_bios(struct ee_bus* bus, struct ps2_bios* bios);
void ee_bus_init_iop_ram(struct ee_bus* bus, struct ps2_ram* iop_ram);
void ee_bus_init_sif(struct ee_bus* bus, struct ps2_sif* sif);
void ee_bus_init_kputchar(struct ee_bus* bus, void (*kputchar)(void*, char), void* udata);

#ifdef __cplusplus
}
#endif

#endif