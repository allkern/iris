#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dev9.h"

struct ps2_dev9* ps2_dev9_create(void) {
    return malloc(sizeof(struct ps2_dev9));
}

void ps2_dev9_init(struct ps2_dev9* dev9, int model) {
    memset(dev9, 0, sizeof(struct ps2_dev9));

    dev9->rev = model;
}

void ps2_dev9_destroy(struct ps2_dev9* dev9) {
    free(dev9);
}

uint64_t ps2_dev9_read8(struct ps2_dev9* dev9, uint32_t addr) {
    switch (addr) {
        case 0x1f80146e: return dev9->rev;
    }

    printf("dev9: Unknown 8-bit read at address %08x\n", addr);

    return 0;
}

uint64_t ps2_dev9_read16(struct ps2_dev9* dev9, uint32_t addr) {
    switch (addr) {
        case 0x1f80146e: return dev9->rev;
    }

    printf("dev9: Unknown 16-bit read at address %08x\n", addr);

    return 0;
}

uint64_t ps2_dev9_read32(struct ps2_dev9* dev9, uint32_t addr) {
    switch (addr) {
        case 0x1f80146e: return dev9->rev;
    }

    printf("dev9: Unknown 32-bit read at address %08x\n", addr);

    return 0;
}

void ps2_dev9_write8(struct ps2_dev9* dev9, uint32_t addr, uint64_t data) {
    printf("dev9: Unknown 8-bit write at address %08x (%04lx)\n", addr, data);

    return;
}

void ps2_dev9_write16(struct ps2_dev9* dev9, uint32_t addr, uint64_t data) {
    printf("dev9: Unknown 16-bit write at address %08x (%04lx)\n", addr, data);

    return;
}

void ps2_dev9_write32(struct ps2_dev9* dev9, uint32_t addr, uint64_t data) {
    printf("dev9: Unknown 32-bit write at address %08x (%04lx)\n", addr, data);

    return;
}