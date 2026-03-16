#include "sram.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct s14x_sram* s14x_sram_create(void) {
    return malloc(sizeof(struct s14x_sram));
}

int s14x_sram_init(struct s14x_sram* sram, int* write_flag) {
    memset(sram, 0, sizeof(struct s14x_sram));

    sram->write_flag = write_flag;

    return 0;
}

int s14x_sram_load(struct s14x_sram* sram, const char* path) {
    FILE* file = fopen(path, "rb");

    sram->path = path;

    // If the file doesn't exist then we'll create it
    if (!file) {
        fclose(file);

        file = fopen(path, "wb");

        fwrite(sram->buf, 1, S14X_SRAM_SIZE, file);
        fclose(file);

        return 0;
    }

    fread(sram->buf, 1, S14X_SRAM_SIZE, file);
    fclose(file);

    return 1;
}

uint64_t s14x_sram_read8(struct s14x_sram* sram, uint32_t addr) {
    return sram->buf[addr & 0x7fff];
}

uint64_t s14x_sram_read16(struct s14x_sram* sram, uint32_t addr) {
    return *(uint16_t*)(sram->buf + (addr & 0x7fff));
}

uint64_t s14x_sram_read32(struct s14x_sram* sram, uint32_t addr) {
    return *(uint32_t*)(sram->buf + (addr & 0x7fff));
}

void s14x_sram_write8(struct s14x_sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    sram->buf[addr & 0x7fff] = data & 0xff;
}

void s14x_sram_write16(struct s14x_sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    *(uint16_t*)(sram->buf + (addr & 0x7fff)) = data & 0xffff;
}

void s14x_sram_write32(struct s14x_sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    *(uint32_t*)(sram->buf + (addr & 0x7fff)) = data & 0xffffffff;
}

void s14x_sram_destroy(struct s14x_sram* sram) {
    if (sram->path) {
        FILE* file = fopen(sram->path, "wb");

        fwrite(sram->buf, 1, S14X_SRAM_SIZE, file);
        fclose(file);
    }

    free(sram);
}