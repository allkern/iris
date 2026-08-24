#include "disc.hpp"

#include "shared/speed/ata.hpp"

namespace iris::ata::disc {

bool is_compressed(const char* path) {
    int ext = iop::disc::get_extension(path);

    return ext == iop::disc::DISC_EXT_CHD ||
           ext == iop::disc::DISC_EXT_CSO ||
           ext == iop::disc::DISC_EXT_ZSO;
}

iop::disc::Disc* open(logger::Logger* logger, const char* path) {
    return iop::disc::open(logger, path);
}

void ata_read_sector(void* udata, uint64_t lba, uint8_t* buf) {
    iop::disc::read_sector((iop::disc::Disc*)udata, buf, lba, iop::disc::DISC_SS_DATA);
}

void ata_write_sector(void* udata, uint64_t lba, const uint8_t* buf) {
    // Compressed images are read only
}

int ata_get_identify(void* udata, uint8_t* buf) {
    return 0;
}

uint64_t ata_get_sector_count(void* udata) {
    iop::disc::Disc* disc = (iop::disc::Disc*)udata;

    int sector_size = iop::disc::get_sector_size(disc);

    if (sector_size <= 0)
        sector_size = speed::ata::SECTOR_SIZE;

    return iop::disc::get_size(disc) / sector_size;
}

void ata_close(void* udata) {
    iop::disc::close((iop::disc::Disc*)udata);
}

}
