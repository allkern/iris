#include "dev9.hpp"

namespace iris::dev9 {

inline constexpr uint32_t REV = 0x1f80146e;

Dev9* create(logger::Logger* logger, Model model) {
    Dev9* dev9 = new Dev9();

    dev9->logger = logger;
    dev9->logger_id = logger::register_source(logger, "dev9");

    dev9->rev = (uint16_t)model;

    return dev9;
}

void destroy(Dev9* dev9) {
    delete dev9;
}

uint64_t read8(Dev9* dev9, uint32_t addr) {
    if (addr == REV)
        return dev9->rev;

    iris_warning(dev9, "Unknown 8-bit read at {:08x}", addr);

    return 0;
}

uint64_t read16(Dev9* dev9, uint32_t addr) {
    if (addr == REV)
        return dev9->rev;

    iris_warning(dev9, "Unknown 16-bit read at {:08x}", addr);

    return 0;
}

uint64_t read32(Dev9* dev9, uint32_t addr) {
    if (addr == REV)
        return dev9->rev;

    iris_warning(dev9, "Unknown 32-bit read at {:08x}", addr);

    return 0;
}

void write8(Dev9* dev9, uint32_t addr, uint64_t data) {
    iris_warning(dev9, "Unknown 8-bit write at {:08x} ({:04x})", addr, data);
}

void write16(Dev9* dev9, uint32_t addr, uint64_t data) {}

void write32(Dev9* dev9, uint32_t addr, uint64_t data) {
    iris_warning(dev9, "Unknown 32-bit write at {:08x} ({:04x})", addr, data);
}

}
