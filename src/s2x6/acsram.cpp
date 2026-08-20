#include <new>

#include "acsram.hpp"

namespace iris::s2x6::acsram {

Acsram* create(logger::Logger* logger) {
    Acsram* acsram = new Acsram();

    acsram->logger = logger;
    acsram->logger_id = logger::register_source(logger, "acsram");

    return acsram;
}

int load(Acsram* acsram, const char* path) {
    acsram->path = path;

    FILE* file = fopen(path, "rb");

    if (!file)
        return 0;

    fread(acsram->buf, 1, SIZE, file);
    fclose(file);

    iris_debug(acsram, "Loaded settings from '{}'", path);

    return 1;
}

uint64_t read8(Acsram* acsram, uint32_t addr) {
    return acsram->buf[(addr >> 1) & (SIZE - 1)];
}

uint64_t read16(Acsram* acsram, uint32_t addr) {
    return acsram->buf[(addr >> 1) & (SIZE - 1)];
}

void write16(Acsram* acsram, uint32_t addr, uint64_t data) {
    uint32_t offset = (addr >> 1) & (SIZE - 1);
    uint32_t region = offset >> 10;

    if (!acsram->written_regions[region]) {
        acsram->written_regions[region] = 1;

        iris_debug(acsram, "Write {:04x} = {:02x}", offset, (uint8_t)data);
    }

    acsram->buf[offset] = data & 0xff;
}

void destroy(Acsram* acsram) {
    if (acsram->path.size()) {
        FILE* file = fopen(acsram->path.c_str(), "wb");

        if (file) {
            fwrite(acsram->buf, 1, SIZE, file);
            fclose(file);
        }
    }

    delete acsram;
}
}
