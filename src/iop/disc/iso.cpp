
#include "iso.hpp"

namespace iris::iop::disc::iso {

Iso* create(logger::Logger* logger) {
    Iso* iso = new Iso();

    iso->logger = logger;
    iso->logger_id = logger::register_source(logger, "iso");

    return iso;
}

int init(Iso* iso, const char* path) {
    iso->file = fopen(path, "rb");

    if (!iso->file) {
        delete iso;

        return 0;
    }

    return 1;
}

void destroy(Iso* iso) {
    fclose(iso->file);
    delete iso;
}

// Disc IF
int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    Iso* iso = (Iso*)udata;

    int s, r;

    s = fseek64(iso->file, lba * 0x800, SEEK_SET);
    r = fread(buf, 1, 0x800, iso->file);

    return r && !s;
}

uint64_t get_size(void* udata) {
    Iso* iso = (Iso*)udata;

    fseek64(iso->file, 0, SEEK_END);

    return ftell64(iso->file);
}

int get_sector_size(void* udata) {
    return 2048;
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
