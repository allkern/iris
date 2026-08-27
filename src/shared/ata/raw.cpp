#include "raw.hpp"

namespace iris::ata::raw {

#ifdef _MSC_VER
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#elif defined(_WIN32)
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

Raw* open(logger::Logger* logger, const char* path) {
    Raw* raw = new Raw();

    raw->logger = logger;
    raw->logger_id = logger::register_source(logger, "raw");

    raw->file = fopen(path, "r+b");

    if (!raw->file) {
        iris_error(raw, "Unable to open file");

        delete raw;

        return nullptr;
    }

    fseek64(raw->file, 0, SEEK_END);

    raw->sector_count = ftell64(raw->file) / 512;

    return raw;
}

uint64_t get_sector_count(Raw* raw) {
    return raw->sector_count;
}

void read_sector(Raw* raw, uint64_t lba, uint8_t* buf) {
    if (lba >= raw->sector_count) {
        iris_warning(raw, "Read at sector {} is past the end of the image", lba);

        memset(buf, 0, 512);

        return;
    }

    fseek64(raw->file, lba * 512, SEEK_SET);
    fread(buf, 512, 1, raw->file);
}

void write_sector(Raw* raw, uint64_t lba, const uint8_t* buf) {
    if (lba >= raw->sector_count) {
        iris_warning(raw, "Write at sector {} is past the end of the image", lba);

        return;
    }

    fseek64(raw->file, lba * 512, SEEK_SET);
    fwrite(buf, 512, 1, raw->file);
}

int get_identify(Raw* raw, uint8_t* buf) {
    // We can't really get identify data from a raw image, so we'll just return zero
    memset(buf, 0, 512);

    return 0;
}

void close(Raw* raw) {
    if (raw->file) {
        fclose(raw->file);
    }

    delete raw;
}

void ata_read_sector(void* udata, uint64_t lba, uint8_t* buf) {
    Raw* raw = (Raw*)udata;

    read_sector(raw, lba, buf);

}
void ata_write_sector(void* udata, uint64_t lba, const uint8_t* buf) {
    Raw* raw = (Raw*)udata;

    write_sector(raw, lba, buf);
}

int ata_get_identify(void* udata, uint8_t* buf) {
    Raw* raw = (Raw*)udata;

    return get_identify(raw, buf);
}

uint64_t ata_get_sector_count(void* udata) {
    Raw* raw = (Raw*)udata;

    return get_sector_count(raw);
}

void ata_close(void* udata) {
    Raw* raw = (Raw*)udata;

    close(raw);
}

}
