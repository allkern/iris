#pragma once

#include <cstddef>
#include <cstdint>

#include "logger.hpp"
#include "u128.h"

namespace iris::bios {

struct Bios {
    uint8_t* buf = nullptr;

    // Really a mask, not a length: reads are addr & size
    size_t size = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Bios* create(logger::Logger* logger);
bool load(Bios* bios, const char* path);
void destroy(Bios* bios);

uint64_t read8(Bios* bios, uint32_t addr);
uint64_t read16(Bios* bios, uint32_t addr);
uint64_t read32(Bios* bios, uint32_t addr);
uint64_t read64(Bios* bios, uint32_t addr);
uint128_t read128(Bios* bios, uint32_t addr);
void write8(Bios* bios, uint32_t addr, uint64_t data);
void write16(Bios* bios, uint32_t addr, uint64_t data);
void write32(Bios* bios, uint32_t addr, uint64_t data);
void write64(Bios* bios, uint32_t addr, uint64_t data);
void write128(Bios* bios, uint32_t addr, uint128_t data);

}
