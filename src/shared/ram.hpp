#pragma once

#include <cstddef>
#include <cstdint>

#include "logger.hpp"
#include "u128.h"

namespace iris::ram {

inline constexpr size_t SIZE_1KB = 0x400;
inline constexpr size_t SIZE_2MB = 0x200000;
inline constexpr size_t SIZE_4MB = 0x400000;
inline constexpr size_t SIZE_8MB = 0x800000;
inline constexpr size_t SIZE_16MB = 0x1000000;
inline constexpr size_t SIZE_32MB = 0x2000000;
inline constexpr size_t SIZE_64MB = 0x4000000;
inline constexpr size_t SIZE_128MB = 0x8000000;
inline constexpr size_t SIZE_256MB = 0x10000000;

struct Ram {
    uint8_t* buf = nullptr;
    size_t size = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Ram* create(logger::Logger* logger, size_t size);
void reset(Ram* ram);
void destroy(Ram* ram);

uint64_t read8(Ram* ram, uint32_t addr);
uint64_t read16(Ram* ram, uint32_t addr);
uint64_t read32(Ram* ram, uint32_t addr);
uint64_t read64(Ram* ram, uint32_t addr);
uint128_t read128(Ram* ram, uint32_t addr);
void write8(Ram* ram, uint32_t addr, uint64_t data);
void write16(Ram* ram, uint32_t addr, uint64_t data);
void write32(Ram* ram, uint32_t addr, uint64_t data);
void write64(Ram* ram, uint32_t addr, uint64_t data);
void write128(Ram* ram, uint32_t addr, uint128_t data);

}
