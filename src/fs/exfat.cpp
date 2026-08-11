#include <cstdio>
#include <cstring>

#include "fs/exfat.hpp"

namespace iris::fs::exfat {

inline constexpr uint32_t ENTRY_SIZE = 32;
inline constexpr uint32_t NAME_CHARS_PER_ENTRY = 15;
inline constexpr uint32_t MAX_NAME_CHARS = 255;
inline constexpr uint32_t FAT_WINDOW = 4096;

enum : uint8_t {
    TYPE_BITMAP = 0x81,
    TYPE_UPCASE = 0x82,
    TYPE_LABEL = 0x83,
    TYPE_FILE = 0x85,
    TYPE_STREAM = 0xc0,
    TYPE_NAME = 0xc1
};

enum : uint16_t {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN = 0x02,
    ATTR_SYSTEM = 0x04,
    ATTR_DIRECTORY = 0x10
};

// Stream extension flags
enum : uint8_t {
    FLAG_ALLOCATED = 0x01,
    FLAG_NO_FAT_CHAIN = 0x02
};

struct Exfat {
    blk::Device* dev = nullptr;

    uint32_t bytes_per_sector = 0;
    uint32_t cluster_size = 0;
    uint32_t cluster_count = 0;
    uint32_t root_cluster = 0;

    uint64_t fat_start = 0;
    uint64_t fat_bytes = 0;
    uint64_t heap_start = 0;

    std::vector <uint8_t> fat_window;
    uint64_t fat_window_offset = UINT64_MAX;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

struct Stream {
    uint32_t first_cluster = 0;
    uint64_t length = 0;
    uint64_t valid_length = 0;
    bool contiguous = false;
};

struct Parsed {
    char name[256] = {};
    uint16_t attributes = 0;
    Stream stream;
    Time created;
    Time modified;
};

struct FileHandle {
    Exfat* fs = nullptr;
    Stream stream;
    std::vector <uint32_t> chain;
};

static uint16_t rd16(const uint8_t* p, uint32_t off) {
    return (uint16_t)(p[off] | (p[off + 1] << 8));
}

static uint32_t rd32(const uint8_t* p, uint32_t off) {
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) | ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
}

static uint64_t rd64(const uint8_t* p, uint32_t off) {
    return (uint64_t)rd32(p, off) | ((uint64_t)rd32(p, off + 4) << 32);
}

static Time rd_timestamp(uint32_t stamp) {
    Time t;

    if (!stamp)
        return t;

    t.year = (int)((stamp >> 25) & 0x7f) + 1980;
    t.month = (int)((stamp >> 21) & 0x0f);
    t.day = (int)((stamp >> 16) & 0x1f);
    t.hour = (int)((stamp >> 11) & 0x1f);
    t.minute = (int)((stamp >> 5) & 0x3f);
    t.second = (int)(stamp & 0x1f) * 2;

    t.valid = t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31;

    return t;
}

static uint64_t cluster_offset(Exfat* f, uint32_t cluster) {
    return f->heap_start + (uint64_t)(cluster - 2) * f->cluster_size;
}

static int fat_get(Exfat* f, uint32_t cluster, uint32_t* out) {
    if (cluster < 2 || cluster >= f->cluster_count + 2)
        return FS_ERR_CORRUPT;

    uint64_t offset = (uint64_t)cluster * 4;

    if (offset + 4 > f->fat_bytes)
        return FS_ERR_CORRUPT;

    uint64_t window = offset & ~(uint64_t)(FAT_WINDOW - 1);

    if (offset + 4 <= window + FAT_WINDOW && window + FAT_WINDOW <= f->fat_bytes) {
        if (window != f->fat_window_offset) {
            if (blk::read(f->dev, f->fat_start + window, f->fat_window.data(), FAT_WINDOW) < 0)
                return FS_ERR_IO;

            f->fat_window_offset = window;
        }

        *out = rd32(f->fat_window.data(), (uint32_t)(offset - window));

        return FS_OK;
    }

    uint8_t b[4];

    if (blk::read(f->dev, f->fat_start + offset, b, 4) < 0)
        return FS_ERR_IO;

    *out = rd32(b, 0);

    return FS_OK;
}

static int chain_collect(Exfat* f, const Stream& stream, uint64_t max_clusters, std::vector <uint32_t>* out) {
    out->clear();

    if (!stream.first_cluster)
        return FS_OK;

    if (stream.contiguous) {
        for (uint64_t i = 0; i < max_clusters; i++) {
            uint32_t cluster = stream.first_cluster + (uint32_t)i;

            if (cluster < 2 || cluster >= f->cluster_count + 2)
                return FS_ERR_CORRUPT;

            out->push_back(cluster);
        }

        return FS_OK;
    }

    uint32_t cluster = stream.first_cluster;

    while (out->size() < max_clusters) {
        if (cluster < 2 || cluster >= f->cluster_count + 2)
            return FS_ERR_CORRUPT;

        out->push_back(cluster);

        uint32_t next;

        int r = fat_get(f, cluster, &next);

        if (r < 0)
            return r;

        if (next == 0xffffffff)
            break;

        cluster = next;

        if (out->size() > f->cluster_count)
            return FS_ERR_CORRUPT;
    }

    return FS_OK;
}

static uint64_t clusters_for(Exfat* f, uint64_t length) {
    return (length + f->cluster_size - 1) / f->cluster_size;
}

struct DirWalk {
    Exfat* fs = nullptr;
    std::vector <uint32_t> chain;
    std::vector <uint8_t> buf;
    uint32_t per_cluster = 0;
    uint32_t index = 0;
    uint32_t loaded = UINT32_MAX;
};

static int walk_begin(Exfat* f, const Stream& dir, DirWalk* w) {
    w->fs = f;
    w->index = 0;
    w->loaded = UINT32_MAX;
    w->per_cluster = f->cluster_size / ENTRY_SIZE;

    w->buf.resize(f->cluster_size);

    uint64_t wanted = dir.length ? clusters_for(f, dir.length) : f->cluster_count;

    return chain_collect(f, dir, wanted, &w->chain);
}

static int walk_next(DirWalk* w, const uint8_t** entry) {
    uint32_t cluster_index = w->index / w->per_cluster;

    if (cluster_index >= w->chain.size())
        return 0;

    if (cluster_index != w->loaded) {
        if (blk::read(w->fs->dev, cluster_offset(w->fs, w->chain[cluster_index]), w->buf.data(), w->fs->cluster_size) < 0)
            return FS_ERR_IO;

        w->loaded = cluster_index;
    }

    *entry = w->buf.data() + (w->index % w->per_cluster) * ENTRY_SIZE;

    w->index++;

    return 1;
}

template <typename Fn>
static int for_each(Exfat* f, const Stream& dir, Fn fn) {
    DirWalk w;

    int r = walk_begin(f, dir, &w);

    if (r < 0)
        return r;

    const uint8_t* e;

    while ((r = walk_next(&w, &e)) == 1) {
        uint8_t type = e[0];

        if (!type)
            break;

        if (type != TYPE_FILE)
            continue;

        int secondary = e[1];

        Parsed p;

        p.attributes = rd16(e, 0x04);
        p.created = rd_timestamp(rd32(e, 0x08));
        p.modified = rd_timestamp(rd32(e, 0x0c));

        uint16_t name[MAX_NAME_CHARS + 1] = {};
        uint32_t name_length = 0;
        uint32_t collected = 0;
        bool have_stream = false;
        bool broken = false;

        for (int i = 0; i < secondary; i++) {
            const uint8_t* s;

            if (walk_next(&w, &s) != 1) {
                broken = true;

                break;
            }

            if (s[0] == TYPE_STREAM) {
                p.stream.contiguous = (s[0x01] & FLAG_NO_FAT_CHAIN) != 0;
                name_length = s[0x03];
                p.stream.valid_length = rd64(s, 0x08);
                p.stream.first_cluster = rd32(s, 0x14);
                p.stream.length = rd64(s, 0x18);

                have_stream = true;
            } else if (s[0] == TYPE_NAME) {
                for (uint32_t c = 0; c < NAME_CHARS_PER_ENTRY; c++) {
                    if (collected < MAX_NAME_CHARS)
                        name[collected++] = rd16(s, 0x02 + c * 2);
                }
            }
        }

        if (broken)
            break;

        if (!have_stream)
            continue;

        if (name_length > collected)
            name_length = collected;

        utf16_to_utf8(name, name_length, p.name, sizeof(p.name), nullptr);

        if (!p.name[0])
            continue;

        if (!fn(p))
            return FS_OK;
    }

    return r < 0 ? r : FS_OK;
}

static Stream root_stream(Exfat* f) {
    Stream s;

    s.first_cluster = f->root_cluster;
    s.length = 0;
    s.contiguous = false;

    return s;
}

static int find_in_dir(Exfat* f, const Stream& dir, const std::string& name, Parsed* out) {
    bool found = false;

    int r = for_each(f, dir, [&](const Parsed& p) {
        if (name.size() != strlen(p.name))
            return true;

        for (size_t i = 0; i < name.size(); i++) {
            char a = name[i], b = p.name[i];

            if (a >= 'a' && a <= 'z') a -= 32;
            if (b >= 'a' && b <= 'z') b -= 32;

            if (a != b)
                return true;
        }

        *out = p;
        found = true;

        return false;
    });

    if (r < 0)
        return r;

    return found ? FS_OK : FS_ERR_NOT_FOUND;
}

static int resolve(Exfat* f, const char* path, Stream* dir, Parsed* out, bool* is_dir) {
    *dir = root_stream(f);
    *is_dir = true;
    *out = Parsed();

    out->attributes = ATTR_DIRECTORY;
    out->stream = *dir;

    for (const std::string& part : path_split(path)) {
        if (!*is_dir)
            return FS_ERR_NOT_DIRECTORY;

        Parsed next;

        int r = find_in_dir(f, *dir, part, &next);

        if (r < 0)
            return r;

        *out = next;
        *is_dir = (next.attributes & ATTR_DIRECTORY) != 0;
        *dir = next.stream;
    }

    return FS_OK;
}

static void fill_entry(const Parsed& p, Entry* out) {
    *out = Entry();

    snprintf(out->name, sizeof(out->name), "%s", p.name);

    out->size = (p.attributes & ATTR_DIRECTORY) ? 0 : p.stream.length;
    out->created = p.created;
    out->modified = p.modified;
    out->cookie = p.stream.first_cluster;

    if (p.attributes & ATTR_DIRECTORY) out->flags |= ENTRY_DIRECTORY;
    if (p.attributes & ATTR_READ_ONLY) out->flags |= ENTRY_READ_ONLY;
    if (p.attributes & ATTR_HIDDEN) out->flags |= ENTRY_HIDDEN;
    if (p.attributes & ATTR_SYSTEM) out->flags |= ENTRY_SYSTEM;
}

static int exfat_list(void* udata, const char* path, std::vector <Entry>* out) {
    Exfat* f = (Exfat*)udata;

    Stream dir;
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

static int exfat_stat(void* udata, const char* path, Entry* out) {
    Exfat* f = (Exfat*)udata;

    Stream dir;
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

static int exfat_open(void* udata, const char* path, Handle** out) {
    Exfat* f = (Exfat*)udata;

    Stream dir;
    Parsed p;
    bool is_dir;

    int r = resolve(f, path, &dir, &p, &is_dir);

    if (r < 0)
        return r;

    if (is_dir)
        return FS_ERR_IS_DIRECTORY;

    FileHandle* h = new FileHandle();

    h->fs = f;
    h->stream = p.stream;

    if (p.stream.length) {
        r = chain_collect(f, p.stream, clusters_for(f, p.stream.length), &h->chain);

        if (r < 0) {
            delete h;

            return r;
        }
    }

    *out = (Handle*)h;

    return FS_OK;
}

static int64_t exfat_read(void* udata, Handle* handle, uint64_t offset, void* buf, uint64_t size) {
    Exfat* f = (Exfat*)udata;
    FileHandle* h = (FileHandle*)handle;

    if (offset >= h->stream.length)
        return 0;

    if (size > h->stream.length - offset)
        size = h->stream.length - offset;

    uint8_t* out = (uint8_t*)buf;
    uint64_t left = size;

    while (left) {
        uint64_t index = offset / f->cluster_size;
        uint32_t in_cluster = offset % f->cluster_size;
        uint64_t chunk = f->cluster_size - in_cluster;

        if (chunk > left)
            chunk = left;

        if (offset >= h->stream.valid_length) {
            memset(out, 0, chunk);
        } else {
            uint64_t real = chunk;

            if (offset + real > h->stream.valid_length)
                real = h->stream.valid_length - offset;

            if (index >= h->chain.size())
                return -1;

            if (blk::read(f->dev, cluster_offset(f, h->chain[index]) + in_cluster, out, real) < 0)
                return -1;

            if (real < chunk)
                memset(out + real, 0, chunk - real);
        }

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)size;
}

static void exfat_close_handle(void*, Handle* handle) {
    delete (FileHandle*)handle;
}

static void exfat_close(void* udata) {
    delete (Exfat*)udata;
}

static void read_label(Exfat* f, char* out, size_t out_size) {
    snprintf(out, out_size, "exFAT volume");

    DirWalk w;

    if (walk_begin(f, root_stream(f), &w) < 0)
        return;

    const uint8_t* e;

    while (walk_next(&w, &e) == 1) {
        if (!e[0])
            break;

        if (e[0] != TYPE_LABEL)
            continue;

        uint32_t count = e[1];

        if (count > 11)
            count = 11;

        uint16_t chars[12] = {};

        for (uint32_t i = 0; i < count; i++)
            chars[i] = rd16(e, 0x02 + i * 2);

        char label[64];

        utf16_to_utf8(chars, count, label, sizeof(label), nullptr);

        if (label[0])
            snprintf(out, out_size, "%s", label);

        return;
    }
}

static uint64_t count_free(Exfat* f) {
    if (f->fat_bytes > 16 * 1024 * 1024)
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

Fs* open(logger::Logger* logger, blk::Device* dev, bool take_ownership) {
    struct { logger::Logger* logger; size_t logger_id; } src = { logger, get_logger_id(logger) };

    uint8_t boot[512];

    if (blk::read(dev, 0, boot, sizeof(boot)) < 0)
        return nullptr;

    if (memcmp(boot + 0x03, "EXFAT   ", 8))
        return nullptr;

    for (uint32_t i = 0x0b; i < 0x40; i++) {
        if (boot[i])
            return nullptr;
    }

    if (boot[0x1fe] != 0x55 || boot[0x1ff] != 0xaa)
        return nullptr;

    uint32_t fat_offset = rd32(boot, 0x50);
    uint32_t fat_length = rd32(boot, 0x54);
    uint32_t heap_offset = rd32(boot, 0x58);
    uint32_t cluster_count = rd32(boot, 0x5c);
    uint32_t root_cluster = rd32(boot, 0x60);
    uint8_t sector_shift = boot[0x6c];
    uint8_t cluster_shift = boot[0x6d];
    uint8_t fat_count = boot[0x6e];

    bool sane =
        sector_shift >= 9 && sector_shift <= 12 &&
        cluster_shift <= 25 - sector_shift &&
        fat_length && cluster_count && (fat_count == 1 || fat_count == 2) &&
        root_cluster >= 2 && root_cluster < cluster_count + 2;

    if (!sane) {
        iris_error(&src, "exFAT boot sector is out of range (sector shift {}, cluster shift {}, clusters {})",
            sector_shift, cluster_shift, cluster_count);

        return nullptr;
    }

    Exfat* f = new Exfat();

    f->dev = dev;
    f->bytes_per_sector = 1u << sector_shift;
    f->cluster_size = 1u << (sector_shift + cluster_shift);
    f->cluster_count = cluster_count;
    f->root_cluster = root_cluster;
    f->fat_start = (uint64_t)fat_offset << sector_shift;
    f->fat_bytes = (uint64_t)fat_length << sector_shift;
    f->heap_start = (uint64_t)heap_offset << sector_shift;
    f->logger = logger;
    f->logger_id = src.logger_id;

    f->fat_window.resize(FAT_WINDOW);

    if (f->heap_start + (uint64_t)cluster_count * f->cluster_size > blk::get_size(dev)) {
        iris_error(&src, "exFAT cluster heap runs past the end of the image");

        delete f;

        return nullptr;
    }

    Fs* fs = new Fs();

    fs->list = exfat_list;
    fs->stat = exfat_stat;
    fs->open = exfat_open;
    fs->read = exfat_read;
    fs->close_handle = exfat_close_handle;
    fs->close = exfat_close;
    fs->udata = f;
    fs->dev = dev;
    fs->owns_dev = take_ownership;
    fs->type = FS_EXFAT;
    fs->total_bytes = (uint64_t)cluster_count * f->cluster_size;
    fs->free_bytes = count_free(f);
    fs->logger = logger;
    fs->logger_id = src.logger_id;

    snprintf(fs->variant, sizeof(fs->variant), "exFAT");

    read_label(f, fs->label, sizeof(fs->label));

    return fs;
}

}
