#include <stdlib.h>
#include <string.h>

#include "ram.h"

struct ps2_ram* ps2_ram_create(void) {
    return malloc(sizeof(struct ps2_ram));
}

void ps2_ram_init(struct ps2_ram* ram, int size) {
    memset(ram, 0, sizeof(struct ps2_ram));

    ram->buf = malloc(size);
}

void ps2_ram_destroy(struct ps2_ram* ram) {
    free(ram->buf);
    free(ram);
}

uint64_t ps2_ram_read8(struct ps2_ram* ram, uint32_t addr) {
    return *(uint8_t*)(ram->buf + (addr & 0x1ffffff));
}

uint64_t ps2_ram_read16(struct ps2_ram* ram, uint32_t addr) {
    return *(uint16_t*)(ram->buf + (addr & 0x1ffffff));
}

uint64_t ps2_ram_read32(struct ps2_ram* ram, uint32_t addr) {
    return *(uint32_t*)(ram->buf + (addr & 0x1ffffff));
}

uint64_t ps2_ram_read64(struct ps2_ram* ram, uint32_t addr) {
    return *(uint64_t*)(ram->buf + (addr & 0x1ffffff));
}

uint128_t ps2_ram_read128(struct ps2_ram* ram, uint32_t addr) {
    return *(uint128_t*)(ram->buf + (addr & 0x1ffffff));
}

void ps2_ram_write8(struct ps2_ram* ram, uint32_t addr, uint64_t data) {
    *(uint8_t*)(ram->buf + (addr & 0x1ffffff)) = data;
}

void ps2_ram_write16(struct ps2_ram* ram, uint32_t addr, uint64_t data) {
    *(uint16_t*)(ram->buf + (addr & 0x1ffffff)) = data;
}

void ps2_ram_write32(struct ps2_ram* ram, uint32_t addr, uint64_t data) {
    *(uint32_t*)(ram->buf + (addr & 0x1ffffff)) = data;
}

void ps2_ram_write64(struct ps2_ram* ram, uint32_t addr, uint64_t data) {
    *(uint64_t*)(ram->buf + (addr & 0x1ffffff)) = data;
}

void ps2_ram_write128(struct ps2_ram* ram, uint32_t addr, uint128_t data) {
    *(uint128_t*)(ram->buf + (addr & 0x1ffffff)) = data;
}
