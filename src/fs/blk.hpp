#pragma once

#include <cstdint>

#include "logger.hpp"

namespace iris::speed::ata {

struct Hdd;

}

namespace iris::iop::disc {

struct Disc;

}

namespace iris::fs {

size_t get_logger_id(logger::Logger* logger);

}

namespace iris::fs::blk {

struct Device {
    int64_t (*read)(void* udata, uint64_t offset, void* buf, uint64_t size);
    uint64_t (*get_size)(void* udata);
    void (*close)(void* udata);

    void* udata;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Device* open_file(logger::Logger* logger, const char* path);
Device* open_memory(logger::Logger* logger, uint8_t* buf, uint64_t size, bool owns);
Device* open_slice(logger::Logger* logger, Device* parent, uint64_t offset, uint64_t size, bool owns_parent);
Device* open_ecc(logger::Logger* logger, Device* parent, uint32_t data_size, uint32_t ecc_size, bool owns_parent);
Device* open_ata(logger::Logger* logger, speed::ata::Hdd* hdd);
Device* open_disc(logger::Logger* logger, iop::disc::Disc* disc);

int64_t read(Device* dev, uint64_t offset, void* buf, uint64_t size);
uint64_t get_size(Device* dev);
void close(Device* dev);

}
