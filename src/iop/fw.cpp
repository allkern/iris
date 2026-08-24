#include <new>

#include "fw.hpp"

namespace iris::fw {

Fw* create(logger::Logger* logger, iop::intc::Intc* intc) {
    Fw* fw = new Fw();

    fw->logger = logger;
    fw->logger_id = logger::register_source(logger, "fw");

    fw->hw.intc = intc;

    return fw;
}

void reset(Fw* fw) {
    auto hw = fw->hw;

    logger::Logger* logger = fw->logger;
    size_t logger_id = fw->logger_id;

    new (fw) Fw();

    fw->logger = logger;
    fw->logger_id = logger_id;

    fw->hw = hw;
}

void destroy(Fw* fw) {
    delete fw;
}

void fw_read_phy(Fw* fw) {
    uint8_t reg = (fw->phy_access >> 24) & 0xF;

    fw->phy_access &= ~0x80000000; //Cancel read request
    fw->phy_access |= fw->phy_r[reg] | ((uint16_t)reg << 8);

    if (fw->intr0mask & 0x40000000) {
        fw->intr0 |= 0x40000000;

        iop::intc::irq(fw->hw.intc, iop::intc::FWRE);
    }

    // iris_debug(fw, "PHY read from reg {} ({:08x})", reg, fw->phy_access & 0xff);
}

void fw_write_phy(Fw* fw) {
    uint8_t reg = (fw->phy_access >> 8) & 0xF;
    uint8_t value = fw->phy_access & 0xFF;

    fw->phy_r[reg] = value;

    fw->phy_access &= ~0x4000ffff;

    // iris_debug(fw, "PHY write to reg {} ({:08x})", reg, value);
}

uint64_t read32(Fw* fw, uint32_t addr) {
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

    iris_debug(fw, "Unhandled 32-bit read from {:08x}", addr);

    return 0;
}

void write32(Fw* fw, uint32_t addr, uint64_t data) {
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

    iris_debug(fw, "Unhandled 32-bit write to {:08x} ({:08x})", addr, data);
}

}
