#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "logger.hpp"
#include "u128.h"

namespace iris::iop::intc { struct Intc; }

namespace iris::sif {

struct Fifo {
    std::vector <uint128_t> data;
    int read_index = 0;
    int write_index = 0;
};

struct Sif {
    uint32_t mscom = 0;
    uint32_t smcom = 0;
    uint32_t msflg = 0;
    uint32_t smflg = 0;
    uint32_t ctrl = 0;
    uint32_t bd6 = 0;

    iop::intc::Intc* iop_intc = nullptr;

    Fifo sif0;
    Fifo sif1;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Sif* create(logger::Logger* logger);
void connect(Sif* sif, iop::intc::Intc* iop_intc);
void destroy(Sif* sif);

uint64_t read32(Sif* sif, uint32_t addr);
void write32(Sif* sif, uint32_t addr, uint64_t data);

void fifo_write(Fifo& fifo, uint128_t data);
uint128_t fifo_read(Fifo& fifo);
void fifo_reset(Fifo& fifo);
bool fifo_is_empty(const Fifo& fifo);

}
