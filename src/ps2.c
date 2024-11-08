#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "ps2.h"

struct ps2_state* ps2_create(void) {
    return malloc(sizeof(struct ps2_state));
}

void ps2_init(struct ps2_state* ps2) {
    memset(ps2, 0, sizeof(struct ps2_state));

    ps2->sched = sched_create();
    sched_init(ps2->sched);

    ps2->ee = ee_create();
    ps2->iop = iop_create();
    ps2->ee_bus = ee_bus_create();
    ps2->iop_bus = iop_bus_create();
    ps2->gif = ps2_gif_create();
    ps2->gs = ps2_gs_create();
    ps2->ee_dma = ps2_dmac_create();
    ps2->ee_ram = ps2_ram_create();
    ps2->ee_intc = ps2_intc_create();
    ps2->iop_dma = ps2_iop_dma_create();
    ps2->iop_intc = ps2_iop_intc_create();
    ps2->iop_timers = ps2_iop_timers_create();
    ps2->iop_ram = ps2_ram_create();
    ps2->bios = ps2_bios_create();
    ps2->sif = ps2_sif_create();

    // Initialize EE
    ee_bus_init(ps2->ee_bus, NULL);

    struct ee_bus_s ee_bus_data;
    ee_bus_data.read8 = ee_bus_read8;
    ee_bus_data.read16 = ee_bus_read16;
    ee_bus_data.read32 = ee_bus_read32;
    ee_bus_data.read64 = ee_bus_read64;
    ee_bus_data.read128 = ee_bus_read128;
    ee_bus_data.write8 = ee_bus_write8;
    ee_bus_data.write16 = ee_bus_write16;
    ee_bus_data.write32 = ee_bus_write32;
    ee_bus_data.write64 = ee_bus_write64;
    ee_bus_data.write128 = ee_bus_write128;
    ee_bus_data.udata = ps2->ee_bus;

    ee_init(ps2->ee, ee_bus_data);

    // Initialize IOP
    iop_bus_init(ps2->iop_bus, NULL);

    struct iop_bus_s iop_bus_data;
    iop_bus_data.read8 = iop_bus_read8;
    iop_bus_data.read16 = iop_bus_read16;
    iop_bus_data.read32 = iop_bus_read32;
    iop_bus_data.write8 = iop_bus_write8;
    iop_bus_data.write16 = iop_bus_write16;
    iop_bus_data.write32 = iop_bus_write32;
    iop_bus_data.udata = ps2->iop_bus;

    iop_init(ps2->iop, iop_bus_data);

    // Initialize devices
    ps2_gif_init(ps2->gif, NULL);
    ps2_dmac_init(ps2->ee_dma, ps2->sif, ps2->iop_dma, ps2->ee, ps2->ee_bus);
    ps2_ram_init(ps2->ee_ram, RAM_SIZE_32MB);
    ps2_gif_init(ps2->gif, ps2->gs);
    ps2_gs_init(ps2->gs, ps2->ee_intc, ps2->iop_intc, ps2->sched);
    ps2_intc_init(ps2->ee_intc, ps2->ee);
    ps2_ram_init(ps2->iop_ram, RAM_SIZE_2MB);
    ps2_iop_dma_init(ps2->iop_dma, ps2->iop_intc, ps2->sif, ps2->ee_dma, ps2->iop_bus);
    ps2_iop_intc_init(ps2->iop_intc, ps2->iop);
    ps2_iop_timers_init(ps2->iop_timers, ps2->iop_intc, ps2->sched);
    ps2_bios_init(ps2->bios, NULL);
    ps2_sif_init(ps2->sif);

    // Initialize bus pointers
    iop_bus_init_bios(ps2->iop_bus, ps2->bios);
    iop_bus_init_iop_ram(ps2->iop_bus, ps2->iop_ram);
    iop_bus_init_sif(ps2->iop_bus, ps2->sif);
    iop_bus_init_dma(ps2->iop_bus, ps2->iop_dma);
    iop_bus_init_intc(ps2->iop_bus, ps2->iop_intc);
    iop_bus_init_timers(ps2->iop_bus, ps2->iop_timers);
    ee_bus_init_bios(ps2->ee_bus, ps2->bios);
    ee_bus_init_iop_ram(ps2->ee_bus, ps2->iop_ram);
    ee_bus_init_sif(ps2->ee_bus, ps2->sif);
    ee_bus_init_dmac(ps2->ee_bus, ps2->ee_dma);
    ee_bus_init_intc(ps2->ee_bus, ps2->ee_intc);
    ee_bus_init_gif(ps2->ee_bus, ps2->gif);
    ee_bus_init_gs(ps2->ee_bus, ps2->gs);
    ee_bus_init_ram(ps2->ee_bus, ps2->ee_ram);
}

void ps2_init_kputchar(struct ps2_state* ps2, void (*ee_kputchar)(void*, char), void* ee_udata, void (*iop_kputchar)(void*, char), void* iop_udata) {
    ee_bus_init_kputchar(ps2->ee_bus, ee_kputchar, ee_udata);
    iop_init_kputchar(ps2->iop, iop_kputchar, iop_udata);
}

void ps2_load_bios(struct ps2_state* ps2, const char* path) {
    ps2_bios_init(ps2->bios, path);
}

void ps2_reset(struct ps2_state* ps2) {
    ee_reset(ps2->ee);
    iop_reset(ps2->iop);
}

void ps2_cycle(struct ps2_state* ps2) {
    // for (int i = 0; i < 8; i++)
        ee_cycle(ps2->ee);

    iop_cycle(ps2->iop);
    iop_cycle(ps2->iop);
}

void ps2_destroy(struct ps2_state* ps2) {
    sched_destroy(ps2->sched);
    ee_destroy(ps2->ee);
    iop_destroy(ps2->iop);
    ee_bus_destroy(ps2->ee_bus);
    iop_bus_destroy(ps2->iop_bus);
    ps2_gif_destroy(ps2->gif);
    ps2_dmac_destroy(ps2->ee_dma);
    ps2_ram_destroy(ps2->ee_ram);
    ps2_intc_destroy(ps2->ee_intc);
    ps2_iop_dma_destroy(ps2->iop_dma);
    ps2_iop_intc_destroy(ps2->iop_intc);
    ps2_iop_timers_destroy(ps2->iop_timers);
    ps2_ram_destroy(ps2->iop_ram);
    ps2_bios_destroy(ps2->bios);
    ps2_sif_destroy(ps2->sif);

    free(ps2);
}