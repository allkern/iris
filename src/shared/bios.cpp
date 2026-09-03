#include <cstdio>
#include <cstring>

#include "bios.hpp"

namespace iris::bios {

inline constexpr size_t DUMMY_SIZE = 0x400000;

inline constexpr const char* ROMDIR_MAGIC = "ROMDIR";
inline constexpr size_t ROMDIR_SEARCH_SIZE = 0x20000;

static bool check_romdir(FILE* file, size_t size) {
    if (size < ROMDIR_SEARCH_SIZE)
        return false;

    uint8_t head[ROMDIR_SEARCH_SIZE];

    fseek(file, 0, SEEK_SET);

    bool read = fread(head, 1, ROMDIR_SEARCH_SIZE, file) == ROMDIR_SEARCH_SIZE;

    fseek(file, 0, SEEK_SET);

    if (!read)
        return false;

    for (size_t i = 0; i + sizeof(ROMDIR_MAGIC) - 1 <= ROMDIR_SEARCH_SIZE; i++) {
        if (!memcmp(head + i, ROMDIR_MAGIC, sizeof(ROMDIR_MAGIC) - 1)) {
            return true;
        }
    }

    return false;
}

static size_t get_file_size(FILE* file) {
    fseek(file, 0, SEEK_END);

    size_t size = ftell(file);

    fseek(file, 0, SEEK_SET);

    return size;
}

Bios* create(logger::Logger* logger) {
    Bios* bios = new Bios();

    bios->logger = logger;
    bios->logger_id = logger::register_source(logger, "bios");
    bios->buf = new uint8_t[DUMMY_SIZE]();
    bios->size = DUMMY_SIZE - 1;

    // b 0x00000000, so a BIOS-less boot spins instead of running garbage
    *(uint32_t*)bios->buf = 0x1000fffe;

    return bios;
}

bool load(Bios* bios, const char* path) {
    if (!path)
        return false;

    FILE* file = fopen(path, "rb");

    if (!file) {
        iris_error(bios, "Couldn't open '{}'", path);

        return false;
    }

    size_t size = get_file_size(file);

    if (!check_romdir(file, size)) {
        iris_error(bios, "'{}' is not a PlayStation 2 ROM, it has no ROMDIR", path);

        fclose(file);

        return false;
    }

    size_t capacity = DUMMY_SIZE;

    while (capacity < size) {
        capacity *= 2;
    }

    delete[] bios->buf;

    bios->buf = new uint8_t[capacity]();
    bios->size = capacity - 1;

    bool ok = fread(bios->buf, 1, size, file) == size;

    if (!ok)
        iris_error(bios, "Couldn't read binary from '{}'", path);

    fclose(file);

    return ok;
}

void destroy(Bios* bios) {
    delete[] bios->buf;
    delete bios;
}

uint64_t read8(Bios* bios, uint32_t addr) {
    return *(uint8_t*)(bios->buf + (addr & bios->size));
}

uint64_t read16(Bios* bios, uint32_t addr) {
    return *(uint16_t*)(bios->buf + (addr & bios->size));
}

uint64_t read32(Bios* bios, uint32_t addr) {
    return *(uint32_t*)(bios->buf + (addr & bios->size));
}

uint64_t read64(Bios* bios, uint32_t addr) {
    return *(uint64_t*)(bios->buf + (addr & bios->size));
}

uint128_t read128(Bios* bios, uint32_t addr) {
    return *(uint128_t*)(bios->buf + (addr & bios->size));
}

void write8(Bios* bios, uint32_t addr, uint64_t data) {
    *(uint8_t*)(bios->buf + (addr & bios->size)) = data;
}

void write16(Bios* bios, uint32_t addr, uint64_t data) {
    *(uint16_t*)(bios->buf + (addr & bios->size)) = data;
}

void write32(Bios* bios, uint32_t addr, uint64_t data) {
    *(uint32_t*)(bios->buf + (addr & bios->size)) = data;
}

void write64(Bios* bios, uint32_t addr, uint64_t data) {
    *(uint64_t*)(bios->buf + (addr & bios->size)) = data;
}

void write128(Bios* bios, uint32_t addr, uint128_t data) {
    *(uint128_t*)(bios->buf + (addr & bios->size)) = data;
}

}
