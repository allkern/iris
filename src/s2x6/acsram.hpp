#pragma once

#include <string>

#include "logger.hpp"

namespace iris::s2x6::acsram {

inline constexpr auto BASE_ADDR = 0x12500000;
inline constexpr auto SIZE = 0x8000;
inline constexpr auto ADDR_SIZE = SIZE * 2;

struct Acsram {
    std::string path;

    uint8_t buf[SIZE];
    uint8_t written_regions[SIZE >> 10];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Acsram* create(logger::Logger* logger);
int load(Acsram* acsram, const char* path);
uint64_t read8(Acsram* acsram, uint32_t addr);
uint64_t read16(Acsram* acsram, uint32_t addr);
void write16(Acsram* acsram, uint32_t addr, uint64_t data);
void destroy(Acsram* acsram);
}
