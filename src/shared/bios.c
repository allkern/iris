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

    if (!path)
        return 0;

    FILE* file = fopen(path, "rb");

    if (!file) {
        return 1;
    }

    int size = get_file_size(file);

    bios->buf = malloc(size);

    if (!fread(bios->buf, 1, size, file)) {
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
    return *(uint8_t*)(bios->buf + addr);
}

uint64_t ps2_bios_read16(struct ps2_bios* bios, uint32_t addr) {
    return *(uint16_t*)(bios->buf + addr);
}

uint64_t ps2_bios_read32(struct ps2_bios* bios, uint32_t addr) {
    return *(uint32_t*)(bios->buf + addr);
}

uint64_t ps2_bios_read64(struct ps2_bios* bios, uint32_t addr) {
    return *(uint64_t*)(bios->buf + addr);
}

uint128_t ps2_bios_read128(struct ps2_bios* bios, uint32_t addr) {
    return *(uint128_t*)(bios->buf + addr);
}

void ps2_bios_write8(struct ps2_bios* bios, uint32_t addr, uint64_t data) {}
void ps2_bios_write16(struct ps2_bios* bios, uint32_t addr, uint64_t data) {}
void ps2_bios_write32(struct ps2_bios* bios, uint32_t addr, uint64_t data) {}
void ps2_bios_write64(struct ps2_bios* bios, uint32_t addr, uint64_t data) {}
void ps2_bios_write128(struct ps2_bios* bios, uint32_t addr, uint128_t data) {}