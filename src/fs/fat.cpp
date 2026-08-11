#include <cctype>
#include <cstdio>
#include <cstring>

#include "fs/fat.hpp"

namespace iris::fs::fat {

inline constexpr uint32_t DIRENT_SIZE = 32;
inline constexpr uint32_t FAT_WINDOW = 4096;
inline constexpr uint32_t LFN_MAX_CHARS = 260;
inline constexpr uint64_t FREE_SCAN_LIMIT = 16 * 1024 * 1024;

enum : int {
    FAT12,
    FAT16,
    FAT32
};

enum : uint8_t {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN = 0x02,
    ATTR_SYSTEM = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE = 0x20,
    ATTR_LFN = 0x0f
};

struct Fat {
    blk::Device* dev = nullptr;

    int type = FAT32;

    uint32_t bytes_per_sector = 0;
    uint32_t sectors_per_cluster = 0;
    uint32_t cluster_size = 0;
    uint32_t cluster_count = 0;
    uint64_t fat_start = 0;
    uint64_t fat_bytes = 0;
    uint64_t data_start = 0;
    uint64_t root_start = 0;
    uint32_t root_bytes = 0;
    uint32_t root_entries = 0;
    uint32_t root_cluster = 0;

    std::vector <uint8_t> fat_window;
    uint64_t fat_window_offset = UINT64_MAX;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

struct DirRef {
    uint32_t cluster = 0;
    bool fixed_root = false;
};

struct FileHandle {
    std::vector <uint32_t> chain;
    uint64_t size = 0;
};

static uint16_t rd16(const uint8_t* p, uint32_t off) {
    return (uint16_t)(p[off] | (p[off + 1] << 8));
}

static uint32_t rd32(const uint8_t* p, uint32_t off) {
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) | ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
}

static Time rd_date_time(uint16_t date, uint16_t time) {
    Time t;

    if (!date)
        return t;

    t.year = ((date >> 9) & 0x7f) + 1980;
    t.month = (date >> 5) & 0x0f;
    t.day = date & 0x1f;
    t.hour = (time >> 11) & 0x1f;
    t.minute = (time >> 5) & 0x3f;
    t.second = (time & 0x1f) * 2;

    t.valid = t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31;

    return t;
}

static uint64_t cluster_offset(Fat* f, uint32_t cluster) {
    return f->data_start + (uint64_t)(cluster - 2) * f->cluster_size;
}

static int fat_bytes_at(Fat* f, uint64_t offset, uint8_t* out, uint32_t size) {
    uint64_t window = offset & ~(uint64_t)(FAT_WINDOW - 1);

    if (offset + size <= window + FAT_WINDOW && window + FAT_WINDOW <= f->fat_bytes) {
        if (window != f->fat_window_offset) {
            if (blk::read(f->dev, f->fat_start + window, f->fat_window.data(), FAT_WINDOW) < 0)
                return FS_ERR_IO;

            f->fat_window_offset = window;
        }

        memcpy(out, f->fat_window.data() + (offset - window), size);

        return FS_OK;
    }

    return blk::read(f->dev, f->fat_start + offset, out, size) < 0 ? FS_ERR_IO : FS_OK;
}

static int fat_get(Fat* f, uint32_t cluster, uint32_t* out) {
    if (cluster < 2 || cluster >= f->cluster_count + 2)
        return FS_ERR_CORRUPT;

    uint8_t b[4] = {};

    if (f->type == FAT12) {
        uint64_t offset = cluster + (cluster / 2);

        int r = fat_bytes_at(f, offset, b, 2);

        if (r < 0)
            return r;

        uint16_t v = (uint16_t)(b[0] | (b[1] << 8));

        *out = (cluster & 1) ? (uint32_t)(v >> 4) : (uint32_t)(v & 0xfff);
    } else if (f->type == FAT16) {
        int r = fat_bytes_at(f, (uint64_t)cluster * 2, b, 2);

        if (r < 0)
            return r;

        *out = rd16(b, 0);
    } else {
        int r = fat_bytes_at(f, (uint64_t)cluster * 4, b, 4);

        if (r < 0)
            return r;

        *out = rd32(b, 0) & 0x0fffffff;
    }

    return FS_OK;
}

static bool is_end(Fat* f, uint32_t value) {
    if (f->type == FAT12)
        return value >= 0xff8;

    if (f->type == FAT16)
        return value >= 0xfff8;

    return value >= 0x0ffffff8;
}

static int chain_collect(Fat* f, uint32_t first, uint64_t max_clusters, std::vector <uint32_t>* out) {
    out->clear();

    uint32_t cluster = first;

    while (out->size() < max_clusters) {
        if (cluster < 2 || cluster >= f->cluster_count + 2)
            return FS_ERR_CORRUPT;

        out->push_back(cluster);

        uint32_t next;

        int r = fat_get(f, cluster, &next);

        if (r < 0)
            return r;

        if (is_end(f, next))
            break;

        cluster = next;

        if (out->size() > f->cluster_count)
            return FS_ERR_CORRUPT;
    }

    return FS_OK;
}

static uint8_t short_checksum(const uint8_t* name) {
    uint8_t sum = 0;

    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + name[i]);

    return sum;
}

static void short_name(const uint8_t* e, char* out) {
    char base[9] = {};
    char ext[4] = {};

    memcpy(base, e, 8);
    memcpy(ext, e + 8, 3);

    // 0x05 stands in for a leading 0xe5, which would otherwise mean "deleted"
    if ((uint8_t)base[0] == 0x05)
        base[0] = (char)0xe5;

    for (int i = 7; i >= 0 && base[i] == ' '; i--)
        base[i] = '\0';

    for (int i = 2; i >= 0 && ext[i] == ' '; i--)
        ext[i] = '\0';

    uint8_t nt = e[0x0c];

    if (nt & 0x08) {
        for (char* p = base; *p; p++)
            *p = (char)tolower((unsigned char)*p);
    }

    if (nt & 0x10) {
        for (char* p = ext; *p; p++)
            *p = (char)tolower((unsigned char)*p);
    }

    if (ext[0]) {
        snprintf(out, 256, "%s.%s", base, ext);
    } else {
        snprintf(out, 256, "%s", base);
    }
}

struct DirWalk {
    Fat* fat = nullptr;
    std::vector <uint64_t> extents;
    std::vector <uint8_t> buf;
    uint32_t extent_size = 0;
    uint32_t per_extent = 0;
    uint32_t index = 0;
    uint32_t count = 0;
    uint32_t loaded = UINT32_MAX;
};

static int walk_begin(Fat* f, const DirRef& dir, DirWalk* w) {
    w->fat = f;
    w->index = 0;
    w->loaded = UINT32_MAX;
    w->extents.clear();

    if (dir.fixed_root) {
        w->extent_size = f->root_bytes;
        w->per_extent = f->root_entries;
        w->count = f->root_entries;

        w->extents.push_back(f->root_start);
    } else {
        std::vector <uint32_t> chain;

        int r = chain_collect(f, dir.cluster, f->cluster_count, &chain);

        if (r < 0)
            return r;

        w->extent_size = f->cluster_size;
        w->per_extent = f->cluster_size / DIRENT_SIZE;
        w->count = (uint32_t)chain.size() * w->per_extent;

        for (uint32_t cluster : chain)
            w->extents.push_back(cluster_offset(f, cluster));
    }

    w->buf.resize(w->extent_size);

    return FS_OK;
}

// 1 when an entry was produced, 0 at the end of the directory, negative on error
static int walk_next(DirWalk* w, const uint8_t** entry) {
    if (w->index >= w->count)
        return 0;

    uint32_t extent = w->index / w->per_extent;

    if (extent >= w->extents.size())
        return 0;

    if (extent != w->loaded) {
        if (blk::read(w->fat->dev, w->extents[extent], w->buf.data(), w->extent_size) < 0)
            return FS_ERR_IO;

        w->loaded = extent;
    }

    *entry = w->buf.data() + (w->index % w->per_extent) * DIRENT_SIZE;

    w->index++;

    return 1;
}

struct Parsed {
    char name[256] = {};
    uint8_t attr = 0;
    uint32_t cluster = 0;
    uint64_t size = 0;
    Time created;
    Time modified;
};

struct NameState {
    uint16_t chars[LFN_MAX_CHARS] = {};
    uint32_t length = 0;
    uint8_t checksum = 0;
    int expected = 0;
};

static void name_reset(NameState* s) {
    s->length = 0;
    s->expected = 0;
}

static void name_add(NameState* s, const uint8_t* e) {
    uint8_t ord = e[0];
    int seq = ord & 0x3f;

    if (seq < 1 || seq > 20) {
        name_reset(s);

        return;
    }

    if (ord & 0x40) {
        name_reset(s);

        s->checksum = e[0x0d];
        s->length = (uint32_t)seq * 13;
        s->expected = seq;
    } else if (seq != s->expected) {
        name_reset(s);

        return;
    }

    static const int slots[13] = { 0x01, 0x03, 0x05, 0x07, 0x09, 0x0e, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1c, 0x1e };

    for (int i = 0; i < 13; i++)
        s->chars[(seq - 1) * 13 + i] = rd16(e, slots[i]);

    s->expected = seq - 1;
}

static bool name_take(NameState* s, const uint8_t* e, char* out) {
    if (s->expected || !s->length || short_checksum(e) != s->checksum)
        return false;

    uint32_t used = 0;

    while (used < s->length && s->chars[used] && s->chars[used] != 0xffff)
        used++;

    if (!used)
        return false;

    utf16_to_utf8(s->chars, used, out, 256, nullptr);

    return true;
}

static void parse(const uint8_t* e, const char* name, Parsed* out) {
    snprintf(out->name, sizeof(out->name), "%s", name);

    out->attr = e[0x0b];
    out->cluster = ((uint32_t)rd16(e, 0x14) << 16) | rd16(e, 0x1a);
    out->size = rd32(e, 0x1c);
    out->created = rd_date_time(rd16(e, 0x10), rd16(e, 0x0e));
    out->modified = rd_date_time(rd16(e, 0x18), rd16(e, 0x16));
}

template <typename Fn>
static int for_each(Fat* f, const DirRef& dir, Fn fn) {
    DirWalk w;

    int r = walk_begin(f, dir, &w);

    if (r < 0)
        return r;

    NameState state;

    const uint8_t* e;

    while ((r = walk_next(&w, &e)) == 1) {
        if (e[0] == 0x00)
            break;

        if (e[0] == 0xe5) {
            name_reset(&state);

            continue;
        }

        uint8_t attr = e[0x0b];

        if ((attr & ATTR_LFN) == ATTR_LFN) {
            name_add(&state, e);

            continue;
        }

        if (attr & ATTR_VOLUME_ID) {
            name_reset(&state);

            continue;
        }

        char name[256];

        if (!name_take(&state, e, name))
            short_name(e, name);

        name_reset(&state);

        if (!name[0] || !strcmp(name, ".") || !strcmp(name, ".."))
            continue;

        Parsed p;

        parse(e, name, &p);

        if (!fn(p))
            return FS_OK;
    }

    return r < 0 ? r : FS_OK;
}

static DirRef root_ref(Fat* f) {
    DirRef d;

    if (f->type == FAT32) {
        d.cluster = f->root_cluster;
    } else {
        d.fixed_root = true;
    }

    return d;
}

static int find_in_dir(Fat* f, const DirRef& dir, const std::string& name, Parsed* out) {
    bool found = false;

    int r = for_each(f, dir, [&](const Parsed& p) {
        if (strcmp(p.name, name.c_str()))
            return true;

        *out = p;
        found = true;

        return false;
    });

    if (r < 0)
        return r;

    return found ? FS_OK : FS_ERR_NOT_FOUND;
}

static int resolve(Fat* f, const char* path, DirRef* dir, Parsed* out, bool* is_dir) {
    *dir = root_ref(f);
    *is_dir = true;

    *out = Parsed();

    out->attr = ATTR_DIRECTORY;

    for (const std::string& part : path_split(path)) {
        if (!*is_dir)
            return FS_ERR_NOT_DIRECTORY;

        Parsed next;

        int r = find_in_dir(f, *dir, part, &next);

        if (r < 0)
            return r;

        *out = next;
        *is_dir = (next.attr & ATTR_DIRECTORY) != 0;

        dir->fixed_root = false;
        dir->cluster = next.cluster;
    }

    return FS_OK;
}

static void fill_entry(const Parsed& p, Entry* out) {
    *out = Entry();

    snprintf(out->name, sizeof(out->name), "%s", p.name);

    out->size = (p.attr & ATTR_DIRECTORY) ? 0 : p.size;
    out->created = p.created;
    out->modified = p.modified;
    out->cookie = p.cluster;

    if (p.attr & ATTR_DIRECTORY) out->flags |= ENTRY_DIRECTORY;
    if (p.attr & ATTR_READ_ONLY) out->flags |= ENTRY_READ_ONLY;
    if (p.attr & ATTR_HIDDEN) out->flags |= ENTRY_HIDDEN;
    if (p.attr & ATTR_SYSTEM) out->flags |= ENTRY_SYSTEM;
}

static int fat_list(void* udata, const char* path, std::vector <Entry>* out) {
    Fat* f = (Fat*)udata;

    DirRef dir;
    Parsed p;
    bool is_dir;

    int r = resolve(f, path, &dir, &p, &is_dir);

    if (r < 0)
        return r;

    if (!is_dir)
        return FS_ERR_NOT_DIRECTORY;

    return for_each(f, dir, [&](const Parsed& entry) {
        if (out->size() >= MAX_DIR_ENTRIES)
            return false;

        Entry filled;

        fill_entry(entry, &filled);

        out->push_back(filled);

        return true;
    });
}

static int fat_stat(void* udata, const char* path, Entry* out) {
    Fat* f = (Fat*)udata;

    DirRef dir;
    Parsed p;
    bool is_dir;

    int r = resolve(f, path, &dir, &p, &is_dir);

    if (r < 0)
        return r;

    fill_entry(p, out);

    if (is_dir)
        out->flags |= ENTRY_DIRECTORY;

    return FS_OK;
}

static int fat_open(void* udata, const char* path, Handle** out) {
    Fat* f = (Fat*)udata;

    DirRef dir;
    Parsed p;
    bool is_dir;

    int r = resolve(f, path, &dir, &p, &is_dir);

    if (r < 0)
        return r;

    if (is_dir)
        return FS_ERR_IS_DIRECTORY;

    FileHandle* h = new FileHandle();

    h->size = p.size;

    if (p.size) {
        uint64_t needed = (p.size + f->cluster_size - 1) / f->cluster_size;

        r = chain_collect(f, p.cluster, needed, &h->chain);

        if (r < 0) {
            delete h;

            return r;
        }
    }

    *out = (Handle*)h;

    return FS_OK;
}

static int64_t fat_read(void* udata, Handle* handle, uint64_t offset, void* buf, uint64_t size) {
    Fat* f = (Fat*)udata;
    FileHandle* h = (FileHandle*)handle;

    if (offset >= h->size)
        return 0;

    if (size > h->size - offset)
        size = h->size - offset;

    uint8_t* out = (uint8_t*)buf;
    uint64_t left = size;

    while (left) {
        uint64_t index = offset / f->cluster_size;
        uint32_t in_cluster = offset % f->cluster_size;
        uint64_t chunk = f->cluster_size - in_cluster;

        if (chunk > left)
            chunk = left;

        if (index >= h->chain.size())
            return -1;

        if (blk::read(f->dev, cluster_offset(f, h->chain[index]) + in_cluster, out, chunk) < 0)
            return -1;

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)size;
}

static void fat_close_handle(void*, Handle* handle) {
    delete (FileHandle*)handle;
}

static void fat_close(void* udata) {
    delete (Fat*)udata;
}

static uint64_t count_free(Fat* f) {
    if (f->fat_bytes > FREE_SCAN_LIMIT)
        return 0;

    uint64_t free_clusters = 0;

    for (uint32_t cluster = 2; cluster < f->cluster_count + 2; cluster++) {
        uint32_t value;

        if (fat_get(f, cluster, &value) < 0)
            return 0;

        if (!value)
            free_clusters++;
    }

    return free_clusters * f->cluster_size;
}

static void read_label(Fat* f, const uint8_t* boot, char* out, size_t out_size) {
    const uint8_t* raw = boot + (f->type == FAT32 ? 0x47 : 0x2b);

    char fallback[12] = {};

    memcpy(fallback, raw, 11);

    for (int i = 10; i >= 0 && fallback[i] == ' '; i--)
        fallback[i] = '\0';

    DirWalk w;

    if (walk_begin(f, root_ref(f), &w) == FS_OK) {
        const uint8_t* e;

        while (walk_next(&w, &e) == 1) {
            if (e[0] == 0x00)
                break;

            if (e[0] == 0xe5 || (e[0x0b] & ATTR_LFN) == ATTR_LFN)
                continue;

            if (!(e[0x0b] & ATTR_VOLUME_ID))
                continue;

            char label[12] = {};

            memcpy(label, e, 11);

            for (int i = 10; i >= 0 && label[i] == ' '; i--)
                label[i] = '\0';

            if (label[0]) {
                snprintf(out, out_size, "%s", label);

                return;
            }
        }
    }

    snprintf(out, out_size, "%s", fallback[0] ? fallback : "FAT volume");
}

Fs* open(logger::Logger* logger, blk::Device* dev, bool take_ownership) {
    struct { logger::Logger* logger; size_t logger_id; } src = { logger, get_logger_id(logger) };

    uint8_t boot[512];

    if (blk::read(dev, 0, boot, sizeof(boot)) < 0)
        return nullptr;

    if (boot[0x1fe] != 0x55 || boot[0x1ff] != 0xaa)
        return nullptr;

    uint32_t bytes_per_sector = rd16(boot, 0x0b);
    uint32_t sectors_per_cluster = boot[0x0d];
    uint32_t reserved = rd16(boot, 0x0e);
    uint32_t num_fats = boot[0x10];
    uint32_t root_entries = rd16(boot, 0x11);
    uint32_t total_16 = rd16(boot, 0x13);
    uint8_t media = boot[0x15];
    uint32_t fat_size_16 = rd16(boot, 0x16);
    uint32_t total_32 = rd32(boot, 0x20);
    uint32_t fat_size_32 = rd32(boot, 0x24);
    uint32_t root_cluster = rd32(boot, 0x2c);

    uint32_t fat_size = fat_size_16 ? fat_size_16 : fat_size_32;
    uint64_t total_sectors = total_16 ? total_16 : total_32;

    bool sane =
        (bytes_per_sector == 512 || bytes_per_sector == 1024 || bytes_per_sector == 2048 || bytes_per_sector == 4096) &&
        sectors_per_cluster && sectors_per_cluster <= 128 && !(sectors_per_cluster & (sectors_per_cluster - 1)) &&
        reserved && (num_fats == 1 || num_fats == 2) && media >= 0xf0 && fat_size && total_sectors;

    if (!sane)
        return nullptr;

    if (total_sectors * bytes_per_sector > blk::get_size(dev))
        return nullptr;

    uint32_t root_sectors = (root_entries * DIRENT_SIZE + bytes_per_sector - 1) / bytes_per_sector;
    uint64_t first_data_sector = reserved + (uint64_t)num_fats * fat_size + root_sectors;

    if (first_data_sector >= total_sectors)
        return nullptr;

    uint64_t clusters = (total_sectors - first_data_sector) / sectors_per_cluster;

    if (!clusters || clusters > 0x0ffffff0)
        return nullptr;

    Fat* f = new Fat();

    f->type = clusters < 4085 ? FAT12 : clusters < 65525 ? FAT16 : FAT32;

    if ((f->type == FAT32) != (root_entries == 0)) {
        iris_error(&src, "FAT root layout does not match the cluster count ({} clusters, {} root entries)",
            clusters, root_entries);

        delete f;

        return nullptr;
    }

    f->dev = dev;
    f->bytes_per_sector = bytes_per_sector;
    f->sectors_per_cluster = sectors_per_cluster;
    f->cluster_size = bytes_per_sector * sectors_per_cluster;
    f->cluster_count = (uint32_t)clusters;
    f->fat_start = (uint64_t)reserved * bytes_per_sector;
    f->fat_bytes = (uint64_t)fat_size * bytes_per_sector;
    f->data_start = first_data_sector * bytes_per_sector;
    f->root_start = ((uint64_t)reserved + (uint64_t)num_fats * fat_size) * bytes_per_sector;
    f->root_bytes = root_sectors * bytes_per_sector;
    f->root_entries = root_entries;
    f->root_cluster = root_cluster;
    f->logger = logger;
    f->logger_id = src.logger_id;

    f->fat_window.resize(FAT_WINDOW);

    if (f->type == FAT32 && (root_cluster < 2 || root_cluster >= f->cluster_count + 2)) {
        iris_error(&src, "FAT32 root cluster {} is out of range", root_cluster);

        delete f;

        return nullptr;
    }

    Fs* fs = new Fs();

    fs->list = fat_list;
    fs->stat = fat_stat;
    fs->open = fat_open;
    fs->read = fat_read;
    fs->close_handle = fat_close_handle;
    fs->close = fat_close;
    fs->udata = f;
    fs->dev = dev;
    fs->owns_dev = take_ownership;
    fs->type = FS_FAT;

    snprintf(fs->variant, sizeof(fs->variant), "%s", f->type == FAT12 ? "FAT12" : f->type == FAT16 ? "FAT16" : "FAT32");

    fs->total_bytes = clusters * f->cluster_size;
    fs->free_bytes = count_free(f);
    fs->logger = logger;
    fs->logger_id = src.logger_id;

    char label[64];

    read_label(f, boot, label, sizeof(label));

    snprintf(fs->label, sizeof(fs->label), "%s", label);

    return fs;
}

}
