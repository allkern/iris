#ifndef EE_BUS_H
#define EE_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u128.h"

#include "dmac.h"
#include "gif.h"
#include "intc.h"
#include "timers.h"
#include "vif.h"
#include "ipu/ipu.h"
#include "../iop/cdvd.h"
#include "../iop/usb.h"

#include "shared/ram.h"
#include "shared/sif.h"
#include "shared/bios.h"

struct ee_bus {
    // EE-only
    struct ps2_ram* ee_ram;
    struct ps2_dmac* dmac;
    struct ps2_gif* gif;
    struct ps2_gs* gs;
    struct ps2_ipu* ipu;
    struct ps2_intc* intc;
    struct ps2_vif* vif;
    struct vu_state* vu0;
    struct vu_state* vu1;
    struct ps2_ee_timers* timers;
    
    // EE/IOP
    struct ps2_cdvd* cdvd;
    struct ps2_usb* usb;
    struct ps2_bios* bios;
    struct ps2_bios* rom1;
    struct ps2_bios* rom2;
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
void ee_bus_init_vif(struct ee_bus* bus, struct ps2_vif* vif);
void ee_bus_init_gs(struct ee_bus* bus, struct ps2_gs* gs);
void ee_bus_init_ipu(struct ee_bus* bus, struct ps2_ipu* ipu);
void ee_bus_init_timers(struct ee_bus* bus, struct ps2_ee_timers* timers);
void ee_bus_init_bios(struct ee_bus* bus, struct ps2_bios* bios);
void ee_bus_init_rom1(struct ee_bus* bus, struct ps2_bios* rom1);
void ee_bus_init_rom2(struct ee_bus* bus, struct ps2_bios* rom2);
void ee_bus_init_iop_ram(struct ee_bus* bus, struct ps2_ram* iop_ram);
void ee_bus_init_sif(struct ee_bus* bus, struct ps2_sif* sif);
void ee_bus_init_cdvd(struct ee_bus* bus, struct ps2_cdvd* cdvd);
void ee_bus_init_usb(struct ee_bus* bus, struct ps2_usb* usb);
void ee_bus_init_vu0(struct ee_bus* bus, struct vu_state* vu);
void ee_bus_init_vu1(struct ee_bus* bus, struct vu_state* vu);
void ee_bus_init_kputchar(struct ee_bus* bus, void (*kputchar)(void*, char), void* udata);
void ee_bus_init_fastmem(struct ee_bus* bus);

#ifdef __cplusplus
}
#endif

#endif