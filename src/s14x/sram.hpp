#pragma once

#include <string>
#include "logger.hpp"

namespace iris::s14x::sram {

inline constexpr auto SIZE = 0x8000;

struct Sram {
    std::string path;
    int* write_flag;
    uint8_t buf[SIZE];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Sram* create(logger::Logger* logger, int* write_flag);
int load(Sram* sram, const char* path);
uint64_t read8(Sram* sram, uint32_t addr);
uint64_t read16(Sram* sram, uint32_t addr);
uint64_t read32(Sram* sram, uint32_t addr);
void write8(Sram* sram, uint32_t addr, uint64_t data);
void write16(Sram* sram, uint32_t addr, uint64_t data);
void write32(Sram* sram, uint32_t addr, uint64_t data);
void destroy(Sram* sram);

}
