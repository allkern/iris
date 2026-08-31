#pragma once

#include <cstdint>

#include "u128.h"
#include "logger.hpp"

namespace iris::ee::bus {

struct Bus;

Bus* create(logger::Logger* logger);
void destroy(Bus* bus);

uint64_t read8(void* udata, uint32_t addr);
uint64_t read16(void* udata, uint32_t addr);
uint64_t read32(void* udata, uint32_t addr);
uint64_t read64(void* udata, uint32_t addr);
uint128_t read128(void* udata, uint32_t addr);
void write8(void* udata, uint32_t addr, uint64_t data);
void write16(void* udata, uint32_t addr, uint64_t data);
void write32(void* udata, uint32_t addr, uint64_t data);
void write64(void* udata, uint32_t addr, uint64_t data);
void write128(void* udata, uint32_t addr, uint128_t data);

}
