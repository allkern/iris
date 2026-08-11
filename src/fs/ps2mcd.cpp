#include <cstdio>
#include <cstring>

#include "fs/ps2mcd.hpp"

namespace iris::fs::ps2mcd {

static const char MAGIC[] = "Sony PS2 Memory Card Format";

inline constexpr uint32_t ECC_SIZE = 16;
inline constexpr uint32_t DIRENT_SIZE = 512;
inline constexpr uint32_t IFC_COUNT = 32;

inline constexpr uint32_t FAT_ALLOCATED = 0x80000000;
inline constexpr uint32_t FAT_END = 0xffffffff;

enum : uint16_t {
    DF_READ = 0x0001,
    DF_WRITE = 0x0002,
    DF_EXECUTE = 0x0004,
    DF_PROTECTED = 0x0008,
    DF_FILE = 0x0010,
    DF_DIRECTORY = 0x0020,
    DF_POCKETSTN = 0x0800,
    DF_PSX = 0x1000,
    DF_HIDDEN = 0x2000,
    DF_EXISTS = 0x8000
};

struct DirEnt {
    uint16_t mode = 0;
    uint32_t length = 0;
    uint32_t cluster = 0;
    Time created;
    Time modified;
    char name[33] = {};
};

struct Mcd {
    blk::Device* dev = nullptr;

    uint32_t page_len = 0;
    uint32_t pages_per_cluster = 0;
    uint32_t cluster_size = 0;
    uint32_t clusters_per_card = 0;
    uint32_t alloc_offset = 0;
    uint32_t alloc_end = 0;
    uint32_t rootdir_cluster = 0;
    uint32_t entries_per_cluster = 0;
    uint32_t ifc_list[IFC_COUNT] = {};

    uint32_t root_count = 0;

    uint32_t cached_indirect = FAT_END;
    uint32_t cached_fat = FAT_END;
    std::vector <uint8_t> indirect_buf;
    std::vector <uint8_t> fat_buf;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
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

static Time rd_time(const uint8_t* p, uint32_t off) {
    Time t;

    t.second = p[off + 1];
    t.minute = p[off + 2];
    t.hour = p[off + 3];
    t.day = p[off + 4];
    t.month = p[off + 5];
    t.year = rd16(p, off + 6);

    t.valid = t.year >= 1970 && t.year <= 2100 && t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31;

    return t;
}

static int read_cluster(Mcd* mcd, uint32_t cluster, uint8_t* buf) {
    if (cluster >= mcd->clusters_per_card)
        return FS_ERR_CORRUPT;

    uint64_t offset = (uint64_t)cluster * mcd->cluster_size;

    return blk::read(mcd->dev, offset, buf, mcd->cluster_size) < 0 ? FS_ERR_IO : FS_OK;
}

static int fat_get(Mcd* mcd, uint32_t index, uint32_t* out) {
    uint32_t epc = mcd->entries_per_cluster;

    uint32_t fat_offset = index % epc;
    uint32_t indirect_index = index / epc;
    uint32_t indirect_offset = indirect_index % epc;
    uint32_t dbl_index = indirect_index / epc;

    if (dbl_index >= IFC_COUNT)
        return FS_ERR_CORRUPT;

    uint32_t indirect_cluster = mcd->ifc_list[dbl_index];

    if (indirect_cluster != mcd->cached_indirect) {
        int r = read_cluster(mcd, indirect_cluster, mcd->indirect_buf.data());

        if (r < 0)
            return r;

        mcd->cached_indirect = indirect_cluster;
        mcd->cached_fat = FAT_END;
    }

    uint32_t fat_cluster = rd32(mcd->indirect_buf.data(), indirect_offset * 4);

    if (fat_cluster != mcd->cached_fat) {
        int r = read_cluster(mcd, fat_cluster, mcd->fat_buf.data());

        if (r < 0)
            return r;

        mcd->cached_fat = fat_cluster;
    }

    *out = rd32(mcd->fat_buf.data(), fat_offset * 4);

    return FS_OK;
}

static int chain_collect(Mcd* mcd, uint32_t first, uint64_t max_clusters, std::vector <uint32_t>* out) {
    out->clear();

    uint32_t cluster = first;
    uint32_t usable = mcd->alloc_end;

    while (out->size() < max_clusters) {
        if (cluster >= usable)
            return FS_ERR_CORRUPT;

        out->push_back(mcd->alloc_offset + cluster);

        uint32_t entry;

        int r = fat_get(mcd, cluster, &entry);

        if (r < 0)
            return r;

        if (entry == FAT_END)
            break;

        if (!(entry & FAT_ALLOCATED))
            return FS_ERR_CORRUPT;

        cluster = entry & ~FAT_ALLOCATED;

        if (out->size() > usable)
            return FS_ERR_CORRUPT;
    }

    return FS_OK;
}

static void parse_dirent(const uint8_t* p, DirEnt* out) {
    out->mode = rd16(p, 0x00);
    out->length = rd32(p, 0x04);
    out->created = rd_time(p, 0x08);
    out->cluster = rd32(p, 0x10);
    out->modified = rd_time(p, 0x18);

    memcpy(out->name, p + 0x40, 32);

    out->name[32] = '\0';
}

struct DirWalk {
    Mcd* mcd = nullptr;
    std::vector <uint32_t> chain;
    std::vector <uint8_t> buf;
    uint32_t index = 0;
    uint32_t count = 0;
    uint32_t loaded = FAT_END;
};

static int walk_begin(Mcd* mcd, uint32_t first_cluster, uint32_t count, DirWalk* w) {
    w->mcd = mcd;
    w->index = 0;
    w->count = count;
    w->loaded = FAT_END;

    w->buf.resize(mcd->cluster_size);

    uint32_t per_cluster = mcd->cluster_size / DIRENT_SIZE;
    uint64_t needed = per_cluster ? (count + per_cluster - 1) / per_cluster : 0;

    return chain_collect(mcd, first_cluster, needed ? needed : 1, &w->chain);
}

// 1 when an entry was produced, 0 at the end, negative on error
static int walk_next(DirWalk* w, DirEnt* out) {
    if (w->index >= w->count)
        return 0;

    uint32_t per_cluster = w->mcd->cluster_size / DIRENT_SIZE;
    uint32_t cluster_index = w->index / per_cluster;

    if (cluster_index >= w->chain.size())
        return 0;

    if (cluster_index != w->loaded) {
        int r = read_cluster(w->mcd, w->chain[cluster_index], w->buf.data());

        if (r < 0)
            return r;

        w->loaded = cluster_index;
    }

    parse_dirent(w->buf.data() + (w->index % per_cluster) * DIRENT_SIZE, out);

    w->index++;

    return 1;
}

static bool is_dot(const char* name) {
    return !strcmp(name, ".") || !strcmp(name, "..");
}

static int find_in_dir(Mcd* mcd, uint32_t cluster, uint32_t count, const char* name, DirEnt* out) {
    DirWalk w;

    int r = walk_begin(mcd, cluster, count, &w);

    if (r < 0)
        return r;

    DirEnt e;

    while ((r = walk_next(&w, &e)) == 1) {
        if (!(e.mode & DF_EXISTS))
            continue;

        if (!strcmp(e.name, name)) {
            *out = e;

            return FS_OK;
        }
    }

    return r < 0 ? r : FS_ERR_NOT_FOUND;
}

static void root_entry(Mcd* mcd, DirEnt* out) {
    *out = DirEnt();

    out->mode = DF_EXISTS | DF_DIRECTORY | DF_READ | DF_WRITE;
    out->length = mcd->root_count;
    out->cluster = mcd->rootdir_cluster;

    strcpy(out->name, "/");
}

static int resolve(Mcd* mcd, const char* path, DirEnt* out) {
    root_entry(mcd, out);

    for (const std::string& part : path_split(path)) {
        if (!(out->mode & DF_DIRECTORY))
            return FS_ERR_NOT_DIRECTORY;

        DirEnt next;

        int r = find_in_dir(mcd, out->cluster, out->length, part.c_str(), &next);

        if (r < 0)
            return r;

        *out = next;
    }

    return FS_OK;
}

static void fill_entry(const DirEnt& e, Entry* out) {
    *out = Entry();

    snprintf(out->name, sizeof(out->name), "%s", e.name);

    out->size = e.mode & DF_DIRECTORY ? 0 : e.length;
    out->created = e.created;
    out->modified = e.modified;
    out->cookie = e.cluster;

    if (e.mode & DF_DIRECTORY) out->flags |= ENTRY_DIRECTORY;
    if (!(e.mode & DF_WRITE)) out->flags |= ENTRY_READ_ONLY;
    if (e.mode & DF_PROTECTED) out->flags |= ENTRY_PROTECTED;
    if (e.mode & DF_HIDDEN) out->flags |= ENTRY_HIDDEN;
    if (e.mode & DF_PSX) out->flags |= ENTRY_PSX_SAVE;
    if (e.mode & DF_POCKETSTN) out->flags |= ENTRY_POCKETSTATION;
}

static int mcd_list(void* udata, const char* path, std::vector <Entry>* out) {
    Mcd* mcd = (Mcd*)udata;

    DirEnt dir;

    int r = resolve(mcd, path, &dir);

    if (r < 0)
        return r;

    if (!(dir.mode & DF_DIRECTORY))
        return FS_ERR_NOT_DIRECTORY;

    DirWalk w;

    r = walk_begin(mcd, dir.cluster, dir.length, &w);

    if (r < 0)
        return r;

    DirEnt e;

    while ((r = walk_next(&w, &e)) == 1) {
        if (!(e.mode & DF_EXISTS) || is_dot(e.name))
            continue;

        if (out->size() >= MAX_DIR_ENTRIES)
            break;

        Entry entry;

        fill_entry(e, &entry);

        out->push_back(entry);
    }

    return r < 0 ? r : FS_OK;
}

static int mcd_stat(void* udata, const char* path, Entry* out) {
    Mcd* mcd = (Mcd*)udata;

    DirEnt e;

    int r = resolve(mcd, path, &e);

    if (r < 0)
        return r;

    fill_entry(e, out);

    return FS_OK;
}

static int mcd_open(void* udata, const char* path, Handle** out) {
    Mcd* mcd = (Mcd*)udata;

    DirEnt e;

    int r = resolve(mcd, path, &e);

    if (r < 0)
        return r;

    if (e.mode & DF_DIRECTORY)
        return FS_ERR_IS_DIRECTORY;

    FileHandle* h = new FileHandle();

    h->size = e.length;

    if (e.length) {
        uint64_t needed = (e.length + mcd->cluster_size - 1) / mcd->cluster_size;

        r = chain_collect(mcd, e.cluster, needed, &h->chain);

        if (r < 0) {
            delete h;

            return r;
        }
    }

    *out = (Handle*)h;

    return FS_OK;
}

static int64_t mcd_read(void* udata, Handle* handle, uint64_t offset, void* buf, uint64_t size) {
    Mcd* mcd = (Mcd*)udata;
    FileHandle* h = (FileHandle*)handle;

    if (offset >= h->size)
        return 0;

    if (size > h->size - offset)
        size = h->size - offset;

    uint8_t* out = (uint8_t*)buf;
    uint64_t left = size;

    std::vector <uint8_t> cluster(mcd->cluster_size);

    while (left) {
        uint64_t index = offset / mcd->cluster_size;
        uint32_t in_cluster = offset % mcd->cluster_size;
        uint64_t chunk = mcd->cluster_size - in_cluster;

        if (chunk > left)
            chunk = left;

        if (index >= h->chain.size())
            return -1;

        if (read_cluster(mcd, h->chain[index], cluster.data()) < 0)
            return -1;

        memcpy(out, cluster.data() + in_cluster, chunk);

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)size;
}

static void mcd_close_handle(void*, Handle* handle) {
    delete (FileHandle*)handle;
}

static void mcd_close(void* udata) {
    delete (Mcd*)udata;
}

static uint64_t count_free(Mcd* mcd) {
    uint64_t free_clusters = 0;

    for (uint32_t i = 0; i < mcd->alloc_end; i++) {
        uint32_t entry;

        if (fat_get(mcd, i, &entry) < 0)
            return 0;

        if (!(entry & FAT_ALLOCATED))
            free_clusters++;
    }

    return free_clusters * mcd->cluster_size;
}

Fs* open(logger::Logger* logger, blk::Device* raw, bool take_ownership) {
    uint8_t sb[DIRENT_SIZE];

    if (blk::read(raw, 0, sb, sizeof(sb)) < 0)
        return nullptr;

    if (memcmp(sb, MAGIC, sizeof(MAGIC) - 1))
        return nullptr;

    uint32_t page_len = rd16(sb, 0x28);
    uint32_t pages_per_cluster = rd16(sb, 0x2a);
    uint32_t clusters_per_card = rd32(sb, 0x30);
    uint32_t alloc_offset = rd32(sb, 0x34);
    uint32_t alloc_end = rd32(sb, 0x38);
    uint32_t rootdir_cluster = rd32(sb, 0x3c);
    uint8_t card_type = sb[0x150];

    struct Probe {
        logger::Logger* logger;
        size_t logger_id;
    } probe = { logger, get_logger_id(logger) };

    bool sane =
        card_type == 2 &&
        (page_len == 512 || page_len == 1024) &&
        (pages_per_cluster == 1 || pages_per_cluster == 2 || pages_per_cluster == 4 || pages_per_cluster == 8) &&
        clusters_per_card && clusters_per_card <= 0x800000 &&
        alloc_offset < clusters_per_card &&
        alloc_end && alloc_end <= clusters_per_card - alloc_offset;

    if (!sane) {
        iris_error(&probe, "PS2 memory card superblock is out of range (type {}, page {}, ppc {}, clusters {})",
            card_type, page_len, pages_per_cluster, clusters_per_card);

        return nullptr;
    }

    uint64_t image_size = blk::get_size(raw);
    uint64_t pages = (uint64_t)clusters_per_card * pages_per_cluster;
    uint64_t with_ecc = pages * (page_len + ECC_SIZE);
    uint64_t without_ecc = pages * page_len;
    uint32_t ecc;

    if (image_size == with_ecc) {
        ecc = ECC_SIZE;
    } else if (image_size == without_ecc) {
        ecc = 0;
    } else if (image_size >= with_ecc) {
        ecc = ECC_SIZE;
    } else if (image_size >= without_ecc) {
        ecc = 0;
    } else {
        iris_error(&probe, "PS2 memory card image is truncated ({} bytes, expected {})", image_size, without_ecc);

        return nullptr;
    }

    blk::Device* dev = raw;
    bool owns_dev = take_ownership;

    if (ecc) {
        dev = blk::open_ecc(logger, raw, page_len, ecc, take_ownership);

        if (!dev)
            return nullptr;

        owns_dev = true;
    }

    Mcd* mcd = new Mcd();

    mcd->dev = dev;
    mcd->page_len = page_len;
    mcd->pages_per_cluster = pages_per_cluster;
    mcd->cluster_size = page_len * pages_per_cluster;
    mcd->clusters_per_card = clusters_per_card;
    mcd->alloc_offset = alloc_offset;
    mcd->alloc_end = alloc_end;
    mcd->rootdir_cluster = rootdir_cluster;
    mcd->entries_per_cluster = mcd->cluster_size / 4;
    mcd->logger = logger;
    mcd->logger_id = probe.logger_id;

    for (uint32_t i = 0; i < IFC_COUNT; i++)
        mcd->ifc_list[i] = rd32(sb, 0x50 + i * 4);

    mcd->indirect_buf.resize(mcd->cluster_size);
    mcd->fat_buf.resize(mcd->cluster_size);

    std::vector <uint8_t> buf(mcd->cluster_size);

    if (read_cluster(mcd, alloc_offset + rootdir_cluster, buf.data()) < 0) {
        delete mcd;

        if (ecc)
            blk::close(dev);

        return nullptr;
    }

    DirEnt root;

    parse_dirent(buf.data(), &root);

    if (!(root.mode & DF_DIRECTORY) || !root.length) {
        iris_error(&probe, "PS2 memory card root directory is not usable");

        delete mcd;

        if (ecc)
            blk::close(dev);

        return nullptr;
    }

    mcd->root_count = root.length;

    Fs* fs = new Fs();

    fs->list = mcd_list;
    fs->stat = mcd_stat;
    fs->open = mcd_open;
    fs->read = mcd_read;
    fs->close_handle = mcd_close_handle;
    fs->close = mcd_close;
    fs->udata = mcd;
    fs->dev = dev;
    fs->owns_dev = owns_dev;
    fs->type = FS_PS2_MCD;
    fs->total_bytes = (uint64_t)alloc_end * mcd->cluster_size;
    fs->free_bytes = count_free(mcd);
    fs->logger = logger;
    fs->logger_id = probe.logger_id;

    snprintf(fs->label, sizeof(fs->label), "%s", ecc ? "PS2 memory card" : "PS2 memory card (no ECC)");

    return fs;
}

}
