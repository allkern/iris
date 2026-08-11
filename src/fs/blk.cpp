#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "fs/blk.hpp"

#include "shared/speed/ata.hpp"
#include "iop/disc.hpp"

namespace iris::fs {

size_t get_logger_id(logger::Logger* logger) {
    static logger::Logger* cached = nullptr;
    static size_t id = 0;

    if (logger != cached) {
        cached = logger;
        id = logger::register_source(logger, "fs");
    }

    return id;
}

}

namespace iris::fs::blk {

#ifdef _MSC_VER
#define blk_fseek64 _fseeki64
#define blk_ftell64 _ftelli64
#elif defined(_WIN32)
#define blk_fseek64 fseeko64
#define blk_ftell64 ftello64
#else
#define blk_fseek64 fseek
#define blk_ftell64 ftell
#endif

static Device* make(logger::Logger* logger, void* udata) {
    Device* dev = new Device();

    dev->udata = udata;
    dev->logger = logger;
    dev->logger_id = get_logger_id(logger);

    return dev;
}

static bool in_range(uint64_t offset, uint64_t size, uint64_t total) {
    return offset <= total && size <= total - offset;
}

struct File {
    FILE* file;
    uint64_t size;
};

static int64_t file_read(void* udata, uint64_t offset, void* buf, uint64_t size) {
    File* f = (File*)udata;

    if (!in_range(offset, size, f->size))
        return -1;

    if (blk_fseek64(f->file, (int64_t)offset, SEEK_SET))
        return -1;

    if (fread(buf, 1, size, f->file) != size)
        return -1;

    return (int64_t)size;
}

static uint64_t file_get_size(void* udata) {
    return ((File*)udata)->size;
}

static void file_close(void* udata) {
    File* f = (File*)udata;

    fclose(f->file);

    delete f;
}

Device* open_file(logger::Logger* logger, const char* path) {
    FILE* file = fopen(path, "rb");

    if (!file)
        return nullptr;

    blk_fseek64(file, 0, SEEK_END);

    int64_t size = blk_ftell64(file);

    if (size <= 0) {
        fclose(file);

        return nullptr;
    }

    Device* dev = make(logger, new File{ file, (uint64_t)size });

    dev->read = file_read;
    dev->get_size = file_get_size;
    dev->close = file_close;

    return dev;
}

struct Memory {
    uint8_t* buf;
    uint64_t size;
    bool owns;
};

static int64_t memory_read(void* udata, uint64_t offset, void* buf, uint64_t size) {
    Memory* m = (Memory*)udata;

    if (!in_range(offset, size, m->size))
        return -1;

    memcpy(buf, m->buf + offset, size);

    return (int64_t)size;
}

static uint64_t memory_get_size(void* udata) {
    return ((Memory*)udata)->size;
}

static void memory_close(void* udata) {
    Memory* m = (Memory*)udata;

    if (m->owns)
        free(m->buf);

    delete m;
}

Device* open_memory(logger::Logger* logger, uint8_t* buf, uint64_t size, bool owns) {
    if (!buf || !size)
        return nullptr;

    Device* dev = make(logger, new Memory{ buf, size, owns });

    dev->read = memory_read;
    dev->get_size = memory_get_size;
    dev->close = memory_close;

    return dev;
}

struct Slice {
    Device* parent;
    uint64_t offset;
    uint64_t size;
    bool owns_parent;
};

static int64_t slice_read(void* udata, uint64_t offset, void* buf, uint64_t size) {
    Slice* s = (Slice*)udata;

    if (!in_range(offset, size, s->size))
        return -1;

    return read(s->parent, s->offset + offset, buf, size);
}

static uint64_t slice_get_size(void* udata) {
    return ((Slice*)udata)->size;
}

static void slice_close(void* udata) {
    Slice* s = (Slice*)udata;

    if (s->owns_parent)
        close(s->parent);

    delete s;
}

Device* open_slice(logger::Logger* logger, Device* parent, uint64_t offset, uint64_t size, bool owns_parent) {
    if (!parent)
        return nullptr;

    uint64_t total = get_size(parent);

    if (offset >= total)
        return nullptr;

    if (!size || size > total - offset)
        size = total - offset;

    Device* dev = make(logger, new Slice{ parent, offset, size, owns_parent });

    dev->read = slice_read;
    dev->get_size = slice_get_size;
    dev->close = slice_close;

    return dev;
}

struct Ecc {
    Device* parent;
    uint32_t data_size;
    uint32_t page_size;
    uint64_t size;
    bool owns_parent;
};

static int64_t ecc_read(void* udata, uint64_t offset, void* buf, uint64_t size) {
    Ecc* s = (Ecc*)udata;

    if (!in_range(offset, size, s->size))
        return -1;

    uint8_t* out = (uint8_t*)buf;
    uint64_t left = size;

    while (left) {
        uint64_t page = offset / s->data_size;
        uint64_t in_page = offset % s->data_size;
        uint64_t chunk = s->data_size - in_page;

        if (chunk > left)
            chunk = left;

        if (read(s->parent, page * s->page_size + in_page, out, chunk) < 0)
            return -1;

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)size;
}

static uint64_t ecc_get_size(void* udata) {
    return ((Ecc*)udata)->size;
}

static void ecc_close(void* udata) {
    Ecc* s = (Ecc*)udata;

    if (s->owns_parent)
        close(s->parent);

    delete s;
}

Device* open_ecc(logger::Logger* logger, Device* parent, uint32_t data_size, uint32_t ecc_size, bool owns_parent) {
    if (!parent || !data_size)
        return nullptr;

    uint32_t page_size = data_size + ecc_size;
    uint64_t pages = get_size(parent) / page_size;

    if (!pages)
        return nullptr;

    Device* dev = make(logger, new Ecc{ parent, data_size, page_size, pages * data_size, owns_parent });

    dev->read = ecc_read;
    dev->get_size = ecc_get_size;
    dev->close = ecc_close;

    return dev;
}

struct AtaDisk {
    speed::ata::Hdd* hdd;
    uint64_t size;
};

static int64_t ata_disk_read(void* udata, uint64_t offset, void* buf, uint64_t size) {
    AtaDisk* a = (AtaDisk*)udata;

    if (!in_range(offset, size, a->size))
        return -1;

    constexpr uint64_t ss = speed::ata::SECTOR_SIZE;

    uint8_t sector[ss];
    uint8_t* out = (uint8_t*)buf;
    uint64_t left = size;

    while (left) {
        uint64_t lba = offset / ss;
        uint64_t in_sector = offset % ss;
        uint64_t chunk = ss - in_sector;

        if (chunk > left)
            chunk = left;

        if (!in_sector && chunk == ss) {
            a->hdd->read_sector(a->hdd->udata, lba, out);
        } else {
            a->hdd->read_sector(a->hdd->udata, lba, sector);

            memcpy(out, sector + in_sector, chunk);
        }

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)size;
}

static uint64_t ata_disk_get_size(void* udata) {
    return ((AtaDisk*)udata)->size;
}

static void ata_disk_close(void* udata) {
    delete (AtaDisk*)udata;
}

Device* open_ata(logger::Logger* logger, speed::ata::Hdd* hdd) {
    if (!hdd || !hdd->read_sector || !hdd->get_sector_count)
        return nullptr;

    uint64_t size = hdd->get_sector_count(hdd->udata) * speed::ata::SECTOR_SIZE;

    if (!size)
        return nullptr;

    Device* dev = make(logger, new AtaDisk{ hdd, size });

    dev->read = ata_disk_read;
    dev->get_size = ata_disk_get_size;
    dev->close = ata_disk_close;

    return dev;
}

inline constexpr uint32_t DISC_DATA_SIZE = 2048;

struct DiscDev {
    iop::disc::Disc* disc;
    uint64_t size;
    uint64_t cached_lba;
    bool cached;
    uint8_t sector[DISC_DATA_SIZE];
};

static int64_t disc_read(void* udata, uint64_t offset, void* buf, uint64_t size) {
    DiscDev* d = (DiscDev*)udata;

    if (!in_range(offset, size, d->size))
        return -1;

    uint8_t* out = (uint8_t*)buf;
    uint64_t left = size;

    while (left) {
        uint64_t lba = offset / DISC_DATA_SIZE;
        uint32_t in_sector = offset % DISC_DATA_SIZE;
        uint64_t chunk = DISC_DATA_SIZE - in_sector;

        if (chunk > left)
            chunk = left;

        if (!d->cached || d->cached_lba != lba) {
            if (!iop::disc::read_sector(d->disc, d->sector, lba, iop::disc::DISC_SS_DATA))
                return -1;

            d->cached_lba = lba;
            d->cached = true;
        }

        memcpy(out, d->sector + in_sector, chunk);

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)size;
}

static uint64_t disc_get_size(void* udata) {
    return ((DiscDev*)udata)->size;
}

static void disc_close(void* udata) {
    delete (DiscDev*)udata;
}

Device* open_disc(logger::Logger* logger, iop::disc::Disc* disc) {
    if (!disc)
        return nullptr;

    int sector_size = iop::disc::get_sector_size(disc);

    if (sector_size <= 0)
        sector_size = DISC_DATA_SIZE;

    // get_size reports the backing image, which counts raw sectors on CD
    // formats; the browsable space is the user area of each one
    uint64_t sectors = iop::disc::get_size(disc) / (uint64_t)sector_size;

    if (!sectors)
        return nullptr;

    DiscDev* d = new DiscDev();

    d->disc = disc;
    d->size = sectors * DISC_DATA_SIZE;
    d->cached_lba = 0;
    d->cached = false;

    Device* dev = make(logger, d);

    dev->read = disc_read;
    dev->get_size = disc_get_size;
    dev->close = disc_close;

    return dev;
}

int64_t read(Device* dev, uint64_t offset, void* buf, uint64_t size) {
    if (!dev)
        return -1;

    if (!size)
        return 0;

    return dev->read(dev->udata, offset, buf, size);
}

uint64_t get_size(Device* dev) {
    return dev ? dev->get_size(dev->udata) : 0;
}

void close(Device* dev) {
    if (!dev)
        return;

    dev->close(dev->udata);

    delete dev;
}

}
