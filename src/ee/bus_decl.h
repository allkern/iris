#ifndef EE_BUS_DECL_H
#define EE_BUS_DECL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u128.h"

#include "dmac.h"

#include "shared/ram.h"
#include "shared/sif.h"
#include "shared/bios.h"

struct ee_bus;

struct ee_bus* ee_bus_create(void);
void ee_bus_init(struct ee_bus* bus, const char* bios_path);
void ee_bus_destroy(struct ee_bus* bus);

uint64_t ee_bus_read8(void* udata, uint32_t addr);
uint64_t ee_bus_read16(void* udata, uint32_t addr);
uint64_t ee_bus_read32(void* udata, uint32_t addr);
uint64_t ee_bus_read64(void* udata, uint32_t addr);
uint128_t ee_bus_read128(void* udata, uint32_t addr);
void ee_bus_write8(void* udata, uint32_t addr, uint64_t data);
void ee_bus_write16(void* udata, uint32_t addr, uint64_t data);
void ee_bus_write32(void* udata, uint32_t addr, uint64_t data);
void ee_bus_write64(void* udata, uint32_t addr, uint64_t data);
void ee_bus_write128(void* udata, uint32_t addr, uint128_t data);

#ifdef __cplusplus
}
#endif

#endif