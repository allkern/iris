#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fw.h"

struct ps2_fw* ps2_fw_create(void) {
    return malloc(sizeof(struct ps2_fw));
}

void ps2_fw_init(struct ps2_fw* fw, struct ps2_iop_intc* intc) {
    memset(fw, 0, sizeof(struct ps2_fw));

    fw->intc = intc;
}

void ps2_fw_destroy(struct ps2_fw* fw) {
    free(fw);
}

void fw_read_phy(struct ps2_fw* fw) {
    uint8_t reg = (fw->phy_access >> 24) & 0xF;

    fw->phy_access &= ~0x80000000; //Cancel read request
    fw->phy_access |= fw->phy_r[reg] | ((uint16_t)reg << 8);

    if (fw->intr0mask & 0x40000000) {
        fw->intr0 |= 0x40000000;

        ps2_iop_intc_irq(fw->intc, IOP_INTC_FWRE);
    }

    // printf("fw: PHY read from reg %d (%08x)\n", reg, fw->phy_access & 0xff);
}

void fw_write_phy(struct ps2_fw* fw) {
    uint8_t reg = (fw->phy_access >> 8) & 0xF;
    uint8_t value = fw->phy_access & 0xFF;

    fw->phy_r[reg] = value;

    fw->phy_access &= ~0x4000ffff;

    // printf("fw: PHY write to reg %d (%08x)\n", reg, value);
}

uint64_t ps2_fw_read32(struct ps2_fw* fw, uint32_t addr) {
    uint32_t reg = addr & 0x1ff;

    switch (reg) {
        case 0x0: return 0xffc00001;
        case 0x8: return fw->ctrl0;
        case 0x10: return fw->ctrl2;
        case 0x14: return fw->phy_access;
        case 0x20: return fw->intr0;
        case 0x24: return fw->intr0mask;
        case 0x28: return fw->intr1;
        case 0x2C: return fw->intr1mask;
        case 0x30: return fw->intr2;
        case 0x34: return fw->intr2mask;
        case 0x7C: return 0x10000001; //Value related to NodeID somehow
    }

    printf("fw: Unhandled 32-bit read from %08x\n", addr);

    return 0;
}

void ps2_fw_write32(struct ps2_fw* fw, uint32_t addr, uint64_t data) {
    uint32_t reg = addr & 0x1ff;
    
    switch (reg) {
        case 0x8: {
            fw->ctrl0 = data;
            fw->ctrl0 &= ~0x3800000;
        } return;
        case 0x10: {
            if (data & 0x2) //Power On
                fw->ctrl2 |= 0x8; //SCLK OK
        } return;
        case 0x14: {
            fw->phy_access = data;

            if (fw->phy_access & 0x40000000) {
                fw_write_phy(fw);
            } else if (fw->phy_access & 0x80000000) {
                fw_read_phy(fw);
            }
        } return;
        case 0x20: {
            fw->intr0 &= ~data;
        } return;
        case 0x24: {
            fw->intr0mask = data;
        } return;
        case 0x28: {
            fw->intr1 &= ~data;
        } return;
        case 0x2C: {
            fw->intr1mask = data;
        } return;
        case 0x30: {
            fw->intr2 &= ~data;
        } return;
        case 0x34: {
            fw->intr2mask = data;
        } return;
        case 0xB8: {
            fw->dma_ctrl_sr0 = data;
        } return;
        case 0x138: {
            fw->dma_ctrl_sr1 = data;
        } return;
    }

    printf("fw: Unhandled 32-bit write to %08x (%08lx)\n", addr, data);
}