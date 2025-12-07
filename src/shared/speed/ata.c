#include <string.h>
#include <stdio.h>

#include "ata.h"

struct ps2_ata* ps2_ata_create(void) {
    return malloc(sizeof(struct ps2_ata));
}

int ps2_ata_init(struct ps2_ata* ata, struct ps2_speed* speed, const char* path) {
    memset(ata, 0, sizeof(struct ps2_ata));

    ata->speed = speed;
    ata->nsector = 1;
    ata->sector = 1;

    return 1;
}

void ps2_ata_destroy(struct ps2_ata* ata) {
    free(ata);
}

uint64_t ps2_ata_read16(struct ps2_ata* ata, uint32_t addr) {
    printf("ata: read16 %08x\n", addr);

    switch (addr) {
        case 0x0040: return ata->data;
        case 0x0042: return ata->error;
        case 0x0044: return ata->nsector;
        case 0x0046: return ata->sector;
        case 0x0048: return ata->lcyl;
        case 0x004a: return ata->hcyl;
        case 0x004c: return ata->select;
        case 0x004e: return 0x48;
        case 0x005c: return 0x40;
    }
}

uint64_t ps2_ata_read32(struct ps2_ata* ata, uint32_t addr) {
    printf("ata: read32 %08x\n", addr);
}

void ps2_ata_write16(struct ps2_ata* ata, uint32_t addr, uint64_t data) {
    printf("ata: write16 %08x %08lx\n", addr, data);

    switch (addr) {
        // case 0x0040: return ata->data;
        // case 0x0042: return ata->error;
        // case 0x0044: return ata->nsector;
        // case 0x0046: return ata->sector;
        case 0x0048: ata->lcyl = data; return;
        case 0x004a: ata->hcyl = data; return;
        case 0x004c: ata->select = data; return;
        case 0x004e: {
            // COMMAND
            ps2_speed_send_irq(ata->speed, 0x0001);

            return;
        } break;
        // case 0x005c: return ata->control;
    }
}

void ps2_ata_write32(struct ps2_ata* ata, uint32_t addr, uint64_t data) {
    printf("ata: write32 %08x %08lx\n", addr, data);
}