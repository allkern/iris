#ifndef IOP_BUS_H
#define IOP_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u128.h"

#include "shared/ram.h"
#include "shared/sif.h"
#include "shared/bios.h"

struct iop_bus {
    struct ps2_bios* bios;
    struct ps2_ram* iop_ram;
    struct ps2_sif* sif;
};

struct iop_bus* iop_bus_create(void);
void iop_bus_init(struct iop_bus* bus, const char* bios_path);
void iop_bus_init_bios(struct iop_bus* bus, struct ps2_bios* bios);
void iop_bus_init_iop_ram(struct iop_bus* bus, struct ps2_ram* iop_ram);
void iop_bus_init_sif(struct iop_bus* bus, struct ps2_sif* sif);
void iop_bus_destroy(struct iop_bus* bus);

uint32_t iop_bus_read8(void* udata, uint32_t addr);
uint32_t iop_bus_read16(void* udata, uint32_t addr);
uint32_t iop_bus_read32(void* udata, uint32_t addr);
void iop_bus_write8(void* udata, uint32_t addr, uint32_t data);
void iop_bus_write16(void* udata, uint32_t addr, uint32_t data);
void iop_bus_write32(void* udata, uint32_t addr, uint32_t data);

#ifdef __cplusplus
}
#endif

#endif