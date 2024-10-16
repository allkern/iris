#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bus.h"

struct ps2_bus* ps2_bus_create(void) {
    return malloc(sizeof(struct ps2_bus));
}

void ps2_bus_init(struct ps2_bus* bus, const char* bios_path) {
    memset(bus, 0, sizeof(struct ps2_bus));

    bus->bios = ps2_bios_create();
    bus->ee_ram = ps2_ram_create();
    bus->iop_ram = ps2_ram_create();

    ps2_bios_init(bus->bios, bios_path);
    ps2_ram_init(bus->ee_ram, RAM_SIZE_32MB);
    ps2_ram_init(bus->iop_ram, RAM_SIZE_2MB);
}

void ps2_bus_destroy(struct ps2_bus* bus) {
    ps2_bios_destroy(bus->bios);
    ps2_ram_destroy(bus->ee_ram);
    ps2_ram_destroy(bus->iop_ram);

    free(bus);
}

#define MAP_READ(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) return ps2_ ## d ## _read ## b (bus->n, addr - l);

#define MAP_WRITE(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) { ps2_ ## d ## _write ## b (bus->n, addr - l, data); return; }

uint64_t ps2_bus_read8(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(8, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(8, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

uint64_t ps2_bus_read16(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(16, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(16, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

uint64_t ps2_bus_read32(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(32, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(32, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

uint64_t ps2_bus_read64(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(64, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(64, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(64, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

uint128_t ps2_bus_read128(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(128, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(128, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(128, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

void ps2_bus_write8(void* udata, uint32_t addr, uint64_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(8, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(8, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

void ps2_bus_write16(void* udata, uint32_t addr, uint64_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(16, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(16, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

void ps2_bus_write32(void* udata, uint32_t addr, uint64_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(32, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(32, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

void ps2_bus_write64(void* udata, uint32_t addr, uint64_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(64, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(64, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(64, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}

void ps2_bus_write128(void* udata, uint32_t addr, uint128_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(128, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(128, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(128, 0x1FC00000, 0x1FFFFFFF, bios, bios);
}
