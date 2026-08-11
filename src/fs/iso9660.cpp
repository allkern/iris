#include <cstdio>
#include <cstring>

#include "fs/iso9660.hpp"

namespace iris::fs::iso9660 {

inline constexpr uint32_t SECTOR_SIZE = 2048;
inline constexpr uint32_t FIRST_DESCRIPTOR_LBA = 16;
inline constexpr uint32_t MAX_DESCRIPTORS = 32;
inline constexpr uint32_t MAX_DIR_SECTORS = 4096;

enum : uint8_t {
    VD_PRIMARY = 1,
    VD_SUPPLEMENTARY = 2,
    VD_TERMINATOR = 255
};

enum : uint8_t {
    FLAG_HIDDEN = 0x01,
    FLAG_DIRECTORY = 0x02,
    FLAG_MULTI_EXTENT = 0x80
};

struct Iso {
    blk::Device* dev = nullptr;

    uint32_t root_lba = 0;
    uint32_t root_size = 0;
    bool joliet = false;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

struct Extent {
    uint32_t lba = 0;
    uint32_t size = 0;
};

struct Record {
    char name[256] = {};
    bool directory = false;
    uint64_t size = 0;
    Time recorded;
    std::vector<Extent> extents;
};

struct FileHandle {
    std::vector<Extent> extents;
    uint64_t size = 0;
};

static uint16_t rd16(const uint8_t* p, uint32_t off) {
    return (uint16_t)(p[off] | (p[off + 1] << 8));
}

static uint32_t rd32(const uint8_t* p, uint32_t off) {
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) | ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
}

static Time rd_record_time(const uint8_t* p, uint32_t off) {
    Time t;

    t.year = 1900 + p[off + 0];
    t.month = p[off + 1];
    t.day = p[off + 2];
    t.hour = p[off + 3];
    t.minute = p[off + 4];
    t.second = p[off + 5];

    t.valid = t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31 && t.year >= 1900 && t.year <= 2100;

    return t;
}

static int read_sector(Iso* iso, uint32_t lba, uint8_t* out) {
    return blk::read(iso->dev, (uint64_t)lba * SECTOR_SIZE, out, SECTOR_SIZE) < 0 ? FS_ERR_IO : FS_OK;
}

static void decode_name(const uint8_t* raw, uint32_t length, bool joliet, char* out) {
    // 0x00 and 0x01 are the "." and ".." records
    if (length == 1 && (raw[0] == 0x00 || raw[0] == 0x01)) {
        out[0] = '.';
        out[1] = raw[0] == 0x01 ? '.' : '\0';
        out[2] = '\0';

        return;
    }

    if (joliet) {
        uint16_t chars[128];

        uint32_t count = length / 2;

        if (count > 128)
            count = 128;

        for (uint32_t i = 0; i < count; i++)
            chars[i] = (uint16_t)((raw[i * 2] << 8) | raw[i * 2 + 1]);

        utf16_to_utf8(chars, count, out, 256, nullptr);
    } else {
        uint32_t count = length < 255 ? length : 255;

        memcpy(out, raw, count);

        out[count] = '\0';
    }

    char* semi = strrchr(out, ';');

    if (semi)
        *semi = '\0';

    size_t len = strlen(out);

    if (len && out[len - 1] == '.')
        out[len - 1] = '\0';
}

static bool is_dot(const char* name) {
    return !name[0] || !strcmp(name, ".") || !strcmp(name, "..");
}

template <typename Fn>
static int for_each(Iso* iso, uint32_t lba, uint32_t size, Fn fn) {
    uint32_t sectors = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;

    if (!sectors || sectors > MAX_DIR_SECTORS)
        return FS_ERR_CORRUPT;

    std::vector<uint8_t> buf(SECTOR_SIZE);

    Record pending;
    bool have_pending = false;

    for (uint32_t s = 0; s < sectors; s++) {
        int r = read_sector(iso, lba + s, buf.data());

        if (r < 0)
            return r;

        uint32_t offset = 0;

        while (offset + 33 <= SECTOR_SIZE) {
            uint32_t length = buf[offset];

            if (!length)
                break;

            if (offset + length > SECTOR_SIZE)
                break;

            const uint8_t* e = buf.data() + offset;

            uint32_t id_len = e[32];

            if (offset + 33 + id_len > SECTOR_SIZE)
                break;

            Record rec;

            decode_name(e + 33, id_len, iso->joliet, rec.name);

            rec.directory = (e[25] & FLAG_DIRECTORY) != 0;
            rec.recorded = rd_record_time(e, 18);

            Extent extent;

            extent.lba = rd32(e, 2) + e[1];
            extent.size = rd32(e, 10);

            bool more = (e[25] & FLAG_MULTI_EXTENT) != 0;

            offset += length;

            if (have_pending) {
                pending.extents.push_back(extent);
                pending.size += extent.size;

                have_pending = more;

                if (more)
                    continue;

                if (!fn(pending))
                    return FS_OK;

                pending = Record();

                continue;
            }

            if (is_dot(rec.name))
                continue;

            rec.extents.push_back(extent);
            rec.size = extent.size;

            if (more) {
                pending = rec;
                have_pending = true;

                continue;
            }

            if (!fn(rec))
                return FS_OK;
        }
    }

    return FS_OK;
}

static int find_in_dir(Iso* iso, uint32_t lba, uint32_t size, const std::string& name, Record* out) {
    bool found = false;

    int r = for_each(iso, lba, size, [&](const Record& rec) {
        if (name.size() != strlen(rec.name))
            return true;

        for (size_t i = 0; i < name.size(); i++) {
            char a = name[i], b = rec.name[i];

            if (a >= 'a' && a <= 'z') a -= 32;
            if (b >= 'a' && b <= 'z') b -= 32;

            if (a != b)
                return true;
        }

        *out = rec;
        found = true;

        return false;
    });

    if (r < 0)
        return r;

    return found ? FS_OK : FS_ERR_NOT_FOUND;
}

static int resolve(Iso* iso, const char* path, Record* out) {
    *out = Record();

    out->directory = true;
    out->size = iso->root_size;

    Extent root;

    root.lba = iso->root_lba;
    root.size = iso->root_size;

    out->extents.push_back(root);

    for (const std::string& part : path_split(path)) {
        if (!out->directory)
            return FS_ERR_NOT_DIRECTORY;

        Record next;

        int r = find_in_dir(iso, out->extents[0].lba, (uint32_t)out->size, part, &next);

        if (r < 0)
            return r;

        *out = next;
    }

    return FS_OK;
}

static void fill_entry(const Record& rec, Entry* out) {
    *out = Entry();

    snprintf(out->name, sizeof(out->name), "%s", rec.name);

    out->size = rec.directory ? 0 : rec.size;
    out->modified = rec.recorded;
    out->cookie = rec.extents.empty() ? 0 : rec.extents[0].lba;

    if (rec.directory)
        out->flags |= ENTRY_DIRECTORY;

    out->flags |= ENTRY_READ_ONLY;
}

static int iso_list(void* udata, const char* path, std::vector<Entry>* out) {
    Iso* iso = (Iso*)udata;

    Record dir;

    int r = resolve(iso, path, &dir);

    if (r < 0)
        return r;

    if (!dir.directory)
        return FS_ERR_NOT_DIRECTORY;

    return for_each(iso, dir.extents[0].lba, (uint32_t)dir.size, [&](const Record& rec) {
        if (out->size() >= MAX_DIR_ENTRIES)
            return false;

        Entry entry;

        fill_entry(rec, &entry);

        out->push_back(entry);

        return true;
    });
}

static int iso_stat(void* udata, const char* path, Entry* out) {
    Iso* iso = (Iso*)udata;

    Record rec;

    int r = resolve(iso, path, &rec);

    if (r < 0)
        return r;

    fill_entry(rec, out);

    return FS_OK;
}

static int iso_open(void* udata, const char* path, Handle** out) {
    Iso* iso = (Iso*)udata;

    Record rec;

    int r = resolve(iso, path, &rec);

    if (r < 0)
        return r;

    if (rec.directory)
        return FS_ERR_IS_DIRECTORY;

    FileHandle* h = new FileHandle();

    h->extents = rec.extents;
    h->size = rec.size;

    *out = (Handle*)h;

    return FS_OK;
}

static int64_t iso_read(void* udata, Handle* handle, uint64_t offset, void* buf, uint64_t size) {
    Iso* iso = (Iso*)udata;
    FileHandle* h = (FileHandle*)handle;

    if (offset >= h->size)
        return 0;

    if (size > h->size - offset)
        size = h->size - offset;

    uint8_t* out = (uint8_t*)buf;
    uint64_t left = size;

    while (left) {
        uint64_t base = 0;
        const Extent* found = nullptr;

        for (const Extent& e : h->extents) {
            if (offset < base + e.size) {
                found = &e;

                break;
            }

            base += e.size;
        }

        if (!found)
            return -1;

        uint64_t within = offset - base;
        uint64_t chunk = found->size - within;

        if (chunk > left)
            chunk = left;

        if (blk::read(iso->dev, (uint64_t)found->lba * SECTOR_SIZE + within, out, chunk) < 0)
            return -1;

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)size;
}

static void iso_close_handle(void*, Handle* handle) {
    delete (FileHandle*)handle;
}

static void iso_close(void* udata) {
    delete (Iso*)udata;
}

static void trim(char* s) {
    size_t len = strlen(s);

    while (len && s[len - 1] == ' ')
        s[--len] = '\0';
}

Fs* open(logger::Logger* logger, blk::Device* dev, bool take_ownership) {
    struct { logger::Logger* logger; size_t logger_id; } src = { logger, get_logger_id(logger) };

    uint8_t sector[SECTOR_SIZE];

    if (blk::read(dev, (uint64_t)FIRST_DESCRIPTOR_LBA * SECTOR_SIZE, sector, sizeof(sector)) < 0)
        return nullptr;

    if (sector[0] != VD_PRIMARY || memcmp(sector + 1, "CD001", 5))
        return nullptr;

    Iso* iso = new Iso();

    iso->dev = dev;
    iso->logger = logger;
    iso->logger_id = src.logger_id;

    char label[64] = {};

    memcpy(label, sector + 40, 32);

    label[32] = '\0';

    trim(label);

    const uint8_t* root = sector + 156;

    iso->root_lba = rd32(root, 2) + root[1];
    iso->root_size = rd32(root, 10);

    uint64_t total = (uint64_t)rd32(sector, 80) * SECTOR_SIZE;

    for (uint32_t i = 1; i < MAX_DESCRIPTORS; i++) {
        if (blk::read(dev, (uint64_t)(FIRST_DESCRIPTOR_LBA + i) * SECTOR_SIZE, sector, sizeof(sector)) < 0)
            break;

        if (memcmp(sector + 1, "CD001", 5))
            break;

        if (sector[0] == VD_TERMINATOR)
            break;

        if (sector[0] != VD_SUPPLEMENTARY)
            continue;

        const uint8_t* escape = sector + 88;

        bool is_joliet = escape[0] == 0x25 && escape[1] == 0x2f &&
            (escape[2] == 0x40 || escape[2] == 0x43 || escape[2] == 0x45);

        if (!is_joliet)
            continue;

        const uint8_t* joliet_root = sector + 156;

        iso->root_lba = rd32(joliet_root, 2) + joliet_root[1];
        iso->root_size = rd32(joliet_root, 10);
        iso->joliet = true;

        break;
    }

    if (!iso->root_size) {
        iris_error(&src, "ISO 9660 root directory is empty");

        delete iso;

        return nullptr;
    }

    Fs* fs = new Fs();

    fs->list = iso_list;
    fs->stat = iso_stat;
    fs->open = iso_open;
    fs->read = iso_read;
    fs->close_handle = iso_close_handle;
    fs->close = iso_close;
    fs->udata = iso;
    fs->dev = dev;
    fs->owns_dev = take_ownership;
    fs->type = FS_ISO9660;
    fs->total_bytes = total;
    fs->free_bytes = 0;
    fs->logger = logger;
    fs->logger_id = src.logger_id;

    snprintf(fs->variant, sizeof(fs->variant), "%s", iso->joliet ? "ISO 9660 + Joliet" : "ISO 9660");
    snprintf(fs->label, sizeof(fs->label), "%s", label[0] ? label : "Disc");

    return fs;
}

}
