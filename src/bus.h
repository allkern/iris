#ifndef BUS_H
#define BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u128.h"

#include "bios.h"
#include "ram.h"

struct ps2_bus {
    struct ps2_bios* bios;
    struct ps2_ram* ee_ram;
    struct ps2_ram* iop_ram;
};

struct ps2_bus* ps2_bus_create(void);
void ps2_bus_init(struct ps2_bus* bus, const char* bios_path);
void ps2_bus_destroy(struct ps2_bus* bus);

uint64_t ps2_bus_read8(void* udata, uint32_t addr);
uint64_t ps2_bus_read16(void* udata, uint32_t addr);
uint64_t ps2_bus_read32(void* udata, uint32_t addr);
uint64_t ps2_bus_read64(void* udata, uint32_t addr);
uint128_t ps2_bus_read128(void* udata, uint32_t addr);
void ps2_bus_write8(void* udata, uint32_t addr, uint64_t data);
void ps2_bus_write16(void* udata, uint32_t addr, uint64_t data);
void ps2_bus_write32(void* udata, uint32_t addr, uint64_t data);
void ps2_bus_write64(void* udata, uint32_t addr, uint64_t data);
void ps2_bus_write128(void* udata, uint32_t addr, uint128_t data);

#ifdef __cplusplus
}
#endif

#endif