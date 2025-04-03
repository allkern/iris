#ifndef RAM_H
#define RAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "u128.h"

struct ps2_ram {
    uint8_t* buf;
    size_t size;
};

#define RAM_SIZE_2MB 0x200000
#define RAM_SIZE_32MB 0x2000000
#define RAM_SIZE_64MB 0x4000000
#define RAM_SIZE_128MB 0x8000000
#define RAM_SIZE_256MB 0x10000000

struct ps2_ram* ps2_ram_create(void);
void ps2_ram_init(struct ps2_ram* ram, int size);
void ps2_ram_reset(struct ps2_ram* ram);
void ps2_ram_destroy(struct ps2_ram* ram);

uint64_t ps2_ram_read8(struct ps2_ram* ram, uint32_t addr);
uint64_t ps2_ram_read16(struct ps2_ram* ram, uint32_t addr);
uint64_t ps2_ram_read32(struct ps2_ram* ram, uint32_t addr);
uint64_t ps2_ram_read64(struct ps2_ram* ram, uint32_t addr);
uint128_t ps2_ram_read128(struct ps2_ram* ram, uint32_t addr);
void ps2_ram_write8(struct ps2_ram* ram, uint32_t addr, uint64_t data);
void ps2_ram_write16(struct ps2_ram* ram, uint32_t addr, uint64_t data);
void ps2_ram_write32(struct ps2_ram* ram, uint32_t addr, uint64_t data);
void ps2_ram_write64(struct ps2_ram* ram, uint32_t addr, uint64_t data);
void ps2_ram_write128(struct ps2_ram* ram, uint32_t addr, uint128_t data);

#ifdef __cplusplus
}
#endif

#endif