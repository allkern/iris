#include <algorithm>

#include "ram.hpp"

namespace iris::ram {

Ram* create(logger::Logger* logger, size_t size) {
    Ram* ram = new Ram();

    ram->logger = logger;
    ram->logger_id = logger::register_source(logger, "ram");
    ram->buf = new uint8_t[size]();
    ram->size = size;

    return ram;
}

Ram* create(logger::Logger* logger, Size size) {
    return create(logger, (size_t)size);
}

void reset(Ram* ram) {
    std::fill_n(ram->buf, ram->size, 0);
}

void destroy(Ram* ram) {
    delete[] ram->buf;
    delete ram;
}

uint64_t read8(Ram* ram, uint32_t addr) {
    return *(uint8_t*)(ram->buf + (addr & (ram->size - 1)));
}

uint64_t read16(Ram* ram, uint32_t addr) {
    return *(uint16_t*)(ram->buf + (addr & (ram->size - 1)));
}

uint64_t read32(Ram* ram, uint32_t addr) {
    return *(uint32_t*)(ram->buf + (addr & (ram->size - 1)));
}

uint64_t read64(Ram* ram, uint32_t addr) {
    return *(uint64_t*)(ram->buf + (addr & (ram->size - 1)));
}

uint128_t read128(Ram* ram, uint32_t addr) {
    return *(uint128_t*)(ram->buf + (addr & (ram->size - 1)));
}

void write8(Ram* ram, uint32_t addr, uint64_t data) {
    *(uint8_t*)(ram->buf + (addr & (ram->size - 1))) = data;
}

void write16(Ram* ram, uint32_t addr, uint64_t data) {
    *(uint16_t*)(ram->buf + (addr & (ram->size - 1))) = data;
}

void write32(Ram* ram, uint32_t addr, uint64_t data) {
    *(uint32_t*)(ram->buf + (addr & (ram->size - 1))) = data;
}

void write64(Ram* ram, uint32_t addr, uint64_t data) {
    *(uint64_t*)(ram->buf + (addr & (ram->size - 1))) = data;
}

void write128(Ram* ram, uint32_t addr, uint128_t data) {
    *(uint128_t*)(ram->buf + (addr & (ram->size - 1))) = data;
}

}
