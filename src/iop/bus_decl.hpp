#pragma once

#include <cstdint>
#include "logger.hpp"

namespace iris::iop::bus {

struct Bus;

// The interface the CPU core uses to reach the bus, kept here so iop.h can
// take it without pulling in the whole bus header.
struct Iface {
    void* udata;
    uint32_t (*read8)(void* udata, uint32_t addr);
    uint32_t (*read16)(void* udata, uint32_t addr);
    uint32_t (*read32)(void* udata, uint32_t addr);
    void (*write8)(void* udata, uint32_t addr, uint32_t data);
    void (*write16)(void* udata, uint32_t addr, uint32_t data);
    void (*write32)(void* udata, uint32_t addr, uint32_t data);
};

Bus* create(logger::Logger* logger);
void destroy(Bus* bus);

uint32_t read8(void* udata, uint32_t addr);
uint32_t read16(void* udata, uint32_t addr);
uint32_t read32(void* udata, uint32_t addr);
void write8(void* udata, uint32_t addr, uint32_t data);
void write16(void* udata, uint32_t addr, uint32_t data);
void write32(void* udata, uint32_t addr, uint32_t data);

}
