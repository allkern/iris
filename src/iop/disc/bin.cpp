
#include "../disc.hpp"
#include "bin.hpp"

namespace iris::iop::disc::bin {

Bin* create(logger::Logger* logger) {
    Bin* bin = new Bin();

    bin->logger = logger;
    bin->logger_id = logger::register_source(logger, "bin");

    return bin;
}

int init(Bin* bin, const char* path) {
    bin->file = fopen(path, "rb");

    if (!bin->file) {
        delete bin;

        return 0;
    }

    return 1;
}

void destroy(Bin* bin) {
    fclose(bin->file);
    delete bin;
}

// Disc IF
int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    Bin* bin = (Bin*)udata;

    int s, r;

    if (size == DISC_SS_DATA) {
        s = fseek64(bin->file, (lba * 2352) + 0x18, SEEK_SET);
        r = fread(buf, 1, 2048, bin->file);
    } else {
        s = fseek64(bin->file, lba * 2352, SEEK_SET);
        r = fread(buf, 1, 2352, bin->file);
    }

    return r && !s;
}

uint64_t get_size(void* udata) {
    Bin* bin = (Bin*)udata;

    fseek64(bin->file, 0, SEEK_END);

    return ftell64(bin->file);
}

int get_sector_size(void* udata) {
    return 2352;
}

int get_track_count(void* udata) {
    return 1;
}

int get_track_info(void* udata, int track, TrackInfo* info) {
    return 0;
}

int get_track_number(void* udata, uint64_t lba) {
    return 1;
}

#undef fseek64
#undef ftell64

}
