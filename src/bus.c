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

    printf("bus: Unhandled 8-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint64_t ps2_bus_read16(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(16, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(16, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    printf("bus: Unhandled 16-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint64_t ps2_bus_read32(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(32, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(32, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("bus: Unhandled 32-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint64_t ps2_bus_read64(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(64, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(64, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(64, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    printf("bus: Unhandled 64-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint128_t ps2_bus_read128(void* udata, uint32_t addr) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_READ(128, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_READ(128, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_READ(128, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    printf("bus: Unhandled 128-bit read from physical address 0x%08x\n", addr);

    return (uint128_t)0;
}

void ps2_bus_write8(void* udata, uint32_t addr, uint64_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(8, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(8, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    if (addr == 0x1000f180) { putchar(data & 0xff); return; }

    printf("bus: Unhandled 8-bit write to physical address 0x%08x (0x%02lx)\n", addr, data);
}

void ps2_bus_write16(void* udata, uint32_t addr, uint64_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(16, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(16, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    printf("bus: Unhandled 16-bit write to physical address 0x%08x (0x%04lx)\n", addr, data);
}

void ps2_bus_write32(void* udata, uint32_t addr, uint64_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(32, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(32, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("bus: Unhandled 32-bit write to physical address 0x%08x (0x%08lx)\n", addr, data);
}

void ps2_bus_write64(void* udata, uint32_t addr, uint64_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(64, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(64, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(64, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    printf("bus: Unhandled 64-bit write to physical address 0x%08x (0x%08lx%08lx)\n", addr, data >> 32, data & 0xffffffff);
}

void ps2_bus_write128(void* udata, uint32_t addr, uint128_t data) {
    struct ps2_bus* bus = (struct ps2_bus*)udata;

    MAP_WRITE(128, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_WRITE(128, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_WRITE(128, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    printf("bus: Unhandled 128-bit write to physical address 0x%08x (0x%08x%08x%08x%08x)\n", addr, data.u32[3], data.u32[2], data.u32[1], data.u32[0]);
}
