#pragma once

#include <cstddef>
#include <cstdint>

#include "logger.hpp"

namespace iris::dev9 {

enum class Model : uint16_t {
    PCMCIA = 0x20, // CXD9566
    EXPBAY = 0x30  // CXD9611
};

struct Dev9 {
    uint16_t r_1460 = 0;
    uint16_t r_1462 = 0;
    uint16_t r_1464 = 0;
    uint16_t r_1466 = 0;
    uint16_t r_1468 = 0;
    uint16_t r_146a = 0;
    uint16_t power = 0;
    uint16_t rev = 0;
    uint16_t r_1470 = 0;
    uint16_t r_1472 = 0;
    uint16_t r_1474 = 0;
    uint16_t r_1476 = 0;
    uint16_t r_1478 = 0;
    uint16_t r_147a = 0;
    uint16_t r_147c = 0;
    uint16_t r_147e = 0;

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
