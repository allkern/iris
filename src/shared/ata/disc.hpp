#pragma once

#include <cstdint>

#include "iop/disc.hpp"
#include "logger.hpp"

namespace iris::ata::disc {

bool is_compressed(const char* path);

iop::disc::Disc* open(logger::Logger* logger, const char* path);

void ata_read_sector(void* udata, uint64_t lba, uint8_t* buf);
void ata_write_sector(void* udata, uint64_t lba, const uint8_t* buf);
int ata_get_identify(void* udata, uint8_t* buf);
uint64_t ata_get_sector_count(void* udata);
void ata_close(void* udata);

}
