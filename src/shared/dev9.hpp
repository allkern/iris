#pragma once

#include <cstddef>
#include <cstdint>

#include "logger.hpp"

namespace iris::dev9 {

enum class Model : uint16_t {
    PCMCIA = 0x20, // CXD9566
    EXPBAY = 0x32  // CXD9611
};

inline constexpr auto REG_BASE = 0x1f801460;
inline constexpr auto REG_COUNT = 16;

struct Dev9 {
    uint16_t regs[REG_COUNT] = { 0 };
    uint16_t rev = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Dev9* create(logger::Logger* logger, Model model);
void destroy(Dev9* dev9);

uint64_t read8(Dev9* dev9, uint32_t addr);
uint64_t read16(Dev9* dev9, uint32_t addr);
uint64_t read32(Dev9* dev9, uint32_t addr);
void write8(Dev9* dev9, uint32_t addr, uint64_t data);
void write16(Dev9* dev9, uint32_t addr, uint64_t data);
void write32(Dev9* dev9, uint32_t addr, uint64_t data);

}
