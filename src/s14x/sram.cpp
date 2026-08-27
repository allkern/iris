#include <new>
#include "sram.hpp"

namespace iris::s14x::sram {

Sram* create(logger::Logger* logger, int* write_flag) {
    Sram* sram = new Sram();

    sram->logger = logger;
    sram->logger_id = logger::register_source(logger, "sram");

    sram->write_flag = write_flag;

    return sram;
}

int load(Sram* sram, const char* path) {
    FILE* file = fopen(path, "rb");

    sram->path = path;

    // If the file doesn't exist then we'll create it
    if (!file) {
        file = fopen(path, "wb");

        if (!file) {
            iris_error(sram, "Couldn't create SRAM file \"{}\"", path);

            return 0;
        }

        fwrite(sram->buf, 1, SIZE, file);
        fclose(file);

        return 0;
    }

    fread(sram->buf, 1, SIZE, file);
    fclose(file);

    return 1;
}

uint64_t read8(Sram* sram, uint32_t addr) {
    return sram->buf[addr & 0x7fff];
}

uint64_t read16(Sram* sram, uint32_t addr) {
    return *(uint16_t*)(sram->buf + (addr & 0x7fff));
}

uint64_t read32(Sram* sram, uint32_t addr) {
    return *(uint32_t*)(sram->buf + (addr & 0x7fff));
}

void write8(Sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    sram->buf[addr & 0x7fff] = data & 0xff;
}

void write16(Sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    *(uint16_t*)(sram->buf + (addr & 0x7fff)) = data & 0xffff;
}

void write32(Sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    *(uint32_t*)(sram->buf + (addr & 0x7fff)) = data & 0xffffffff;
}

void destroy(Sram* sram) {
    if (sram->path.size()) {
        FILE* file = fopen(sram->path.c_str(), "wb");

        if (file) {
            fwrite(sram->buf, 1, SIZE, file);
            fclose(file);
        } else {
            iris_error(sram, "Couldn't write SRAM back to \"{}\"", sram->path);
        }
    }

    delete sram;
}

}
