#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bus.h"

struct iop_bus* iop_bus_create(void) {
    return malloc(sizeof(struct iop_bus));
}

void iop_bus_init(struct iop_bus* bus, const char* bios_path) {
    memset(bus, 0, sizeof(struct iop_bus));
}

void iop_bus_init_bios(struct iop_bus* bus, struct ps2_bios* bios) {
    bus->bios = bios;
}

void iop_bus_init_iop_ram(struct iop_bus* bus, struct ps2_ram* iop_ram) {
    bus->iop_ram = iop_ram;
}

void iop_bus_init_sif(struct iop_bus* bus, struct ps2_sif* sif) {
    bus->sif = sif;
}

void iop_bus_destroy(struct iop_bus* bus) {
    free(bus);
}

#define MAP_READ(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) return ps2_ ## d ## _read ## b (bus->n, addr - l);

#define MAP_WRITE(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) { ps2_ ## d ## _write ## b (bus->n, addr - l, data); return; }

uint32_t iop_bus_read8(void* udata, uint32_t addr) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_READ(8, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_READ(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("iop_bus: Unhandled 8-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint32_t iop_bus_read16(void* udata, uint32_t addr) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_READ(16, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_READ(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("iop_bus: Unhandled 16-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint32_t iop_bus_read32(void* udata, uint32_t addr) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_READ(32, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_READ(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_READ(32, 0x1D000000, 0x1D00006F, sif, sif);

    // printf("iop_bus: Unhandled 32-bit read from physical address 0x%08x\n", addr);

    return 0;
}

void iop_bus_write8(void* udata, uint32_t addr, uint32_t data) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_WRITE(8, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_WRITE(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("iop_bus: Unhandled 8-bit write to physical address 0x%08x (0x%02x)\n", addr, data);
}

void iop_bus_write16(void* udata, uint32_t addr, uint32_t data) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_WRITE(16, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_WRITE(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("iop_bus: Unhandled 16-bit write to physical address 0x%08x (0x%04x)\n", addr, data);
}

void iop_bus_write32(void* udata, uint32_t addr, uint32_t data) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_WRITE(32, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_WRITE(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_WRITE(32, 0x1D000000, 0x1D00006F, sif, sif);

    // printf("iop_bus: Unhandled 32-bit write to physical address 0x%08x (0x%08x)\n", addr, data);
}

#undef MAP_READ
#undef MAP_WRITE