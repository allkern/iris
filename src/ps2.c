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

    // Initialize shared devices
    ps2->bios = ps2_bios_create();
    ps2_bios_init(ps2->bios, NULL);

    ps2->iop_ram = ps2_ram_create();
    ps2_ram_init(ps2->iop_ram, RAM_SIZE_2MB);

    ps2->sif = ps2_sif_create();
    ps2_sif_init(ps2->sif, 0);

    // Initialize EE bus
    ps2->ee_bus = ee_bus_create();
    ee_bus_init(ps2->ee_bus, NULL);

    // Initialize EE
    ps2->ee = ee_create();

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

    // Initialize IOP bus
    ps2->iop_bus = iop_bus_create();
    iop_bus_init(ps2->iop_bus, NULL);

    // Initialize IOP
    ps2->iop = iop_create();

    struct iop_bus_s iop_bus_data;
    iop_bus_data.read8 = iop_bus_read8;
    iop_bus_data.read16 = iop_bus_read16;
    iop_bus_data.read32 = iop_bus_read32;
    iop_bus_data.write8 = iop_bus_write8;
    iop_bus_data.write16 = iop_bus_write16;
    iop_bus_data.write32 = iop_bus_write32;
    iop_bus_data.udata = ps2->iop_bus;

    iop_init(ps2->iop, iop_bus_data);

    // Initialize bus shared devices
    iop_bus_init_bios(ps2->iop_bus, ps2->bios);
    iop_bus_init_iop_ram(ps2->iop_bus, ps2->iop_ram);
    iop_bus_init_sif(ps2->iop_bus, ps2->sif);
    ee_bus_init_bios(ps2->ee_bus, ps2->bios);
    ee_bus_init_iop_ram(ps2->ee_bus, ps2->iop_ram);
    ee_bus_init_sif(ps2->ee_bus, ps2->sif);
}

void ps2_load_bios(struct ps2_state* ps2, const char* path) {
    ps2_bios_init(ps2->bios, path);
}

void ps2_cycle(struct ps2_state* ps2) {
    ee_cycle(ps2->ee);
    iop_cycle(ps2->iop);
}

void ps2_destroy(struct ps2_state* ps2) {
    ps2_bios_destroy(ps2->bios);
    ps2_ram_destroy(ps2->iop_ram);
    ps2_sif_destroy(ps2->sif);
    ee_bus_destroy(ps2->ee_bus);
    iop_bus_destroy(ps2->iop_bus);
    ee_destroy(ps2->ee);
    iop_destroy(ps2->iop);

    free(ps2);
}