#pragma once

#include "intc.hpp"
#include "logger.hpp"

namespace iris::fw {

struct Fw {
    struct {
        iop::intc::Intc* intc;
    } hw;

    uint32_t intr0;
    uint32_t intr1;
    uint32_t intr2;
    uint32_t intr0mask;
    uint32_t intr1mask;
    uint32_t intr2mask;
    uint32_t ctrl0;
    uint32_t ctrl1;
    uint32_t ctrl2;
    uint32_t dma_ctrl_sr0;
    uint32_t dma_ctrl_sr1;
    uint32_t phy_access;
    uint8_t phy_r[16];


    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Fw* create(logger::Logger* logger, iop::intc::Intc* intc);
void reset(Fw* fw);
void destroy(Fw* fw);
uint64_t read32(Fw* fw, uint32_t addr);
void write32(Fw* fw, uint32_t addr, uint64_t data);

}
