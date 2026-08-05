#pragma once

#include <cstdint>
#include <cstdio>

#include "logger.hpp"

namespace iris::ata::raw {

struct Raw {
    FILE* file = nullptr;

    uint64_t sector_count = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Raw* open(logger::Logger* logger, const char* path);
uint64_t get_sector_count(Raw* raw);
void read_sector(Raw* raw, uint64_t lba, uint8_t* buf);
void write_sector(Raw* raw, uint64_t lba, const uint8_t* buf);
int get_identify(Raw* raw, uint8_t* buf);
void close(Raw* raw);

// ATA adapters
void ata_read_sector(void* udata, uint64_t lba, uint8_t* buf);
void ata_write_sector(void* udata, uint64_t lba, const uint8_t* buf);
int ata_get_identify(void* udata, uint8_t* buf);
uint64_t ata_get_sector_count(void* udata);
void ata_close(void* udata);

}
