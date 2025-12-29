#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "speed.h"

struct ps2_speed* ps2_speed_create(void) {
    return malloc(sizeof(struct ps2_speed));
}

void ps2_speed_init(struct ps2_speed* speed, struct ps2_iop_intc* iop_intc) {
    memset(speed, 0, sizeof(struct ps2_speed));

    speed->iop_intc = iop_intc;
    speed->flash = ps2_flash_create();
    speed->ata = ps2_ata_create();

    ps2_flash_init(speed->flash);
    ps2_ata_init(speed->ata, speed);

    speed->rev8 |= 2;
}

void ps2_speed_destroy(struct ps2_speed* speed) {
    ps2_flash_destroy(speed->flash);
    ps2_ata_destroy(speed->ata);

    free(speed);
}

uint64_t ps2_speed_read8(struct ps2_speed* speed, uint32_t addr) {
    addr &= 0xffff;

    printf("speed: read8 %08x %08x\n", addr);

    switch (addr) {
        case 0x002e: return 0;
    }

    return 0;

    // exit(1);
}
uint64_t ps2_speed_read16(struct ps2_speed* speed, uint32_t addr) {
    addr &= 0xffff;

    if (addr >= 0x4800 && addr < 0x4820) {
        return ps2_flash_read16(speed->flash, addr);
    }

    if (addr >= 0x0040 && addr < 0x0060) {
        return ps2_ata_read16(speed->ata, addr);
    }

    switch (addr) {
        case 0x0000: return speed->rev;
        case 0x0002: return speed->rev1;
        case 0x0004: return speed->rev3;
        case 0x000e: return speed->rev8;
        case 0x0024: return speed->dma_ctrl;
        case 0x0028: return speed->intr_stat;
        case 0x002a: return speed->intr_mask;
        case 0x0032: return speed->xfr_ctrl;
        case 0x0038: return speed->unknown38;
        case 0x0064: return speed->if_ctrl;
    }

    printf("speed: read16 %08x\n", addr); // exit(1);

    return 0;
}
uint64_t ps2_speed_read32(struct ps2_speed* speed, uint32_t addr) {
    addr &= 0xffff;

    if (addr >= 0x4800 && addr < 0x4820) {
        return ps2_flash_read32(speed->flash, addr);
    }

    printf("speed: read32 %08x\n", addr); // exit(1);

    return 0;
}
void ps2_speed_write8(struct ps2_speed* speed, uint32_t addr, uint64_t data) {
    addr &= 0xffff;

    printf("speed: write8 %08x %08x\n", addr, data);

    switch (addr) {
        case 0x002c: speed->pio_dir = data; return;
        case 0x002e: speed->pio_data = data; return;
    }

    // exit(1);
}
void ps2_speed_write16(struct ps2_speed* speed, uint32_t addr, uint64_t data) {
    addr &= 0xffff;

    if (addr >= 0x4800 && addr < 0x4820) {
        ps2_flash_write16(speed->flash, addr, data);

        return;
    }

    if (addr >= 0x0040 && addr < 0x0060) {
        ps2_ata_write16(speed->ata, addr, data);

        return;
    }

    switch (addr) {
        case 0x0024: speed->dma_ctrl = data; return;
        case 0x0032: speed->xfr_ctrl = data; return;
        case 0x0038: speed->unknown38 = data; /* ??? */ return;
        case 0x0064: speed->if_ctrl = data; return;
        case 0x0070: speed->pio_mode = data; return;
        case 0x0072: speed->mwdma_mode = data; return;
        case 0x0074: speed->udma_mode = data; return;
        case 0x002a: speed->intr_mask = data; return;
    }

    printf("speed: write16 %08x %08x\n", addr, data); // exit(1);
}
void ps2_speed_write32(struct ps2_speed* speed, uint32_t addr, uint64_t data) {
    addr &= 0xffff;

    if (addr >= 0x4800 && addr < 0x4820) {
        ps2_flash_write32(speed->flash, addr, data);

        return;
    }

    printf("speed: write32 %08x %08x\n", addr, data); // exit(1);
}

void ps2_speed_send_irq(struct ps2_speed* speed, uint16_t irq) {
    speed->intr_stat |= irq;

    if (speed->intr_stat & speed->intr_mask) {
        ps2_iop_intc_irq(speed->iop_intc, IOP_INTC_DEV9);
    }
}

int ps2_speed_load_flash(struct ps2_speed* speed, const char* path) {
    int ret = ps2_flash_load(speed->flash, path);

    if (ret) {
        speed->rev3 |= SPD_CAPS_FLASH;
    }

    return ret;
}