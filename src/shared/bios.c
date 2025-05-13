#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bios.h"

struct ps2_bios* ps2_bios_create(void) {
    return malloc(sizeof(struct ps2_bios));
}

static int get_file_size(FILE* file) {
    int prev = ftell(file);
    int size;

    fseek(file, 0, SEEK_END);

    size = ftell(file);

    fseek(file, prev, SEEK_SET);

    return size;
}

int ps2_bios_init(struct ps2_bios* bios, const char* path) {
    memset(bios, 0, sizeof(struct ps2_bios));

    if (!path) {
        bios->buf = malloc(0x400000);
        bios->size = 0x3fffff;

        memset(bios->buf, 0, 0x400000);

        return 0;
    }

    FILE* file = fopen(path, "rb");

    if (!file) {
        // Dummy data
        bios->buf = malloc(0x400000);
        bios->size = 0x3fffff;

        memset(bios->buf, 0, 0x400000);

        return 1;
    }

    bios->size = get_file_size(file) - 1;
    bios->buf = malloc(bios->size);

    if (!fread(bios->buf, 1, bios->size + 1, file)) {
        printf("bios: Couldn't read binary\n");

        return 1;
    }

    return 0;
}

void ps2_bios_destroy(struct ps2_bios* bios) {
    free(bios->buf);
    free(bios);
}

uint64_t ps2_bios_read8(struct ps2_bios* bios, uint32_t addr) {
    return *(uint8_t*)(bios->buf + (addr & bios->size));
}

uint64_t ps2_bios_read16(struct ps2_bios* bios, uint32_t addr) {
    return *(uint16_t*)(bios->buf + (addr & bios->size));
}

uint64_t ps2_bios_read32(struct ps2_bios* bios, uint32_t addr) {
    return *(uint32_t*)(bios->buf + (addr & bios->size));
}

uint64_t ps2_bios_read64(struct ps2_bios* bios, uint32_t addr) {
    return *(uint64_t*)(bios->buf + (addr & bios->size));
}

uint128_t ps2_bios_read128(struct ps2_bios* bios, uint32_t addr) {
    return *(uint128_t*)(bios->buf + (addr & bios->size));
}

void ps2_bios_write8(struct ps2_bios* bios, uint32_t addr, uint64_t data) {
    *(uint8_t*)(bios->buf + (addr & bios->size)) = data;
}
void ps2_bios_write16(struct ps2_bios* bios, uint32_t addr, uint64_t data) {
    *(uint16_t*)(bios->buf + (addr & bios->size)) = data;
}
void ps2_bios_write32(struct ps2_bios* bios, uint32_t addr, uint64_t data) {
    *(uint32_t*)(bios->buf + (addr & bios->size)) = data;
}
void ps2_bios_write64(struct ps2_bios* bios, uint32_t addr, uint64_t data) {
    *(uint64_t*)(bios->buf + (addr & bios->size)) = data;
}
void ps2_bios_write128(struct ps2_bios* bios, uint32_t addr, uint128_t data) {
    *(uint128_t*)(bios->buf + (addr & bios->size)) = data;
}