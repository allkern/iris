#ifndef BIOS_H
#define BIOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u128.h"

struct ps2_bios {
    uint8_t* buf;
    size_t size;
};

struct ps2_bios* ps2_bios_create(void);
void ps2_bios_init(struct ps2_bios* bios);
int ps2_bios_load(struct ps2_bios* bios, const char* path);
void ps2_bios_destroy(struct ps2_bios* bios);

uint64_t ps2_bios_read8(struct ps2_bios* bios, uint32_t addr);
uint64_t ps2_bios_read16(struct ps2_bios* bios, uint32_t addr);
uint64_t ps2_bios_read32(struct ps2_bios* bios, uint32_t addr);
uint64_t ps2_bios_read64(struct ps2_bios* bios, uint32_t addr);
uint128_t ps2_bios_read128(struct ps2_bios* bios, uint32_t addr);
void ps2_bios_write8(struct ps2_bios* bios, uint32_t addr, uint64_t data);
void ps2_bios_write16(struct ps2_bios* bios, uint32_t addr, uint64_t data);
void ps2_bios_write32(struct ps2_bios* bios, uint32_t addr, uint64_t data);
void ps2_bios_write64(struct ps2_bios* bios, uint32_t addr, uint64_t data);
void ps2_bios_write128(struct ps2_bios* bios, uint32_t addr, uint128_t data);

#ifdef __cplusplus
}
#endif

#endif