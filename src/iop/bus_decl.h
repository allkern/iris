#ifndef IOP_BUS_DECL_H
#define IOP_BUS_DECL_H

#include <stdint.h>

struct iop_bus;

struct iop_bus* iop_bus_create(void);
void iop_bus_init(struct iop_bus* bus, const char* bios_path);
void iop_bus_destroy(struct iop_bus* bus);

uint32_t iop_bus_read8(void* udata, uint32_t addr);
uint32_t iop_bus_read16(void* udata, uint32_t addr);
uint32_t iop_bus_read32(void* udata, uint32_t addr);
void iop_bus_write8(void* udata, uint32_t addr, uint32_t data);
void iop_bus_write16(void* udata, uint32_t addr, uint32_t data);
void iop_bus_write32(void* udata, uint32_t addr, uint32_t data);

#endif