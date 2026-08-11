#include <cstdio>
#include <cstring>

#include "fs/pfs.hpp"

namespace iris::fs::pfs {

// From ps2sdk iop/hdd/libpfs
inline constexpr uint32_t SUPER_MAGIC = 0x50465300;     // "PFS\0"
inline constexpr uint32_t SEGD_MAGIC = 0x53454744;      // "SEGD"
inline constexpr uint32_t SEGI_MAGIC = 0x53454749;      // "SEGI"
inline constexpr uint64_t SUPER_OFFSET = 0x400000;
inline constexpr uint32_t INODE_SIZE = 1024;
inline constexpr uint32_t INODE_MAX_BLOCKS = 114;
inline constexpr uint32_t DENTRY_GROUP = 512;
inline constexpr uint32_t MAX_SEGMENTS = 4096;

enum : uint16_t {
    ATTR_SUBDIR = 0x0020,
    ATTR_PSX = 0x1000,
    ATTR_HIDDEN = 0x4000
};

inline constexpr uint16_t ALEN_MASK = 0x0fff;
inline constexpr uint16_t MODE_DIRECTORY = 0x1000;

struct BlockInfo {
    uint32_t number = 0;
    uint16_t subpart = 0;
    uint16_t count = 0;
};

struct Run {
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct Pfs {
    blk::Device* dev = nullptr;

    std::vector <part::Extent> extents;

    uint32_t zone_size = 0;
    BlockInfo root;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

struct Node {
    BlockInfo block;
    uint64_t size = 0;
    uint16_t mode = 0;
    uint16_t attr = 0;
    Time created;
    Time modified;
    std::vector <Run> runs;
};

struct FileHandle {
    std::vector <Run> runs;
    uint64_t size = 0;
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

static BlockInfo rd_block(const uint8_t* p, uint32_t off) {
    BlockInfo b;

    b.number = rd32(p, off);
    b.subpart = rd16(p, off + 4);
    b.count = rd16(p, off + 6);

    return b;
}

static Time rd_time(const uint8_t* p, uint32_t off) {
    Time t;

    t.second = p[off + 1];
    t.minute = p[off + 2];
    t.hour = p[off + 3];
    t.day = p[off + 4];
    t.month = p[off + 5];
    t.year = rd16(p, off + 6);

    t.valid = t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31 && t.year >= 1970 && t.year <= 2100;

    return t;
}

static int locate(Pfs* fs, const BlockInfo& block, uint32_t zones, uint64_t* offset, uint64_t* size) {
    if (block.subpart >= fs->extents.size())
        return FS_ERR_CORRUPT;

    const part::Extent& extent = fs->extents[block.subpart];

    uint64_t at = (uint64_t)block.number * fs->zone_size;
    uint64_t length = (uint64_t)zones * fs->zone_size;

    if (at >= extent.size || length > extent.size - at)
        return FS_ERR_CORRUPT;

    *offset = extent.offset + at;
    *size = length;

    return FS_OK;
}

static int read_inode(Pfs* fs, const BlockInfo& block, uint8_t* out) {
    uint64_t offset, size;

    int r = locate(fs, block, 1, &offset, &size);

    if (r < 0)
        return r;

    if (blk::read(fs->dev, offset, out, INODE_SIZE) < 0)
        return FS_ERR_IO;

    uint32_t magic = rd32(out, 4);

    if (magic != SEGD_MAGIC && magic != SEGI_MAGIC)
        return FS_ERR_CORRUPT;

    return FS_OK;
}

// data[0] is the inode's own zone, so content starts at index 1 and continues
// through any chained segment descriptors
static int collect_runs(Pfs* fs, const BlockInfo& block, Node* out) {
    uint8_t inode[INODE_SIZE];

    int r = read_inode(fs, block, inode);

    if (r < 0)
        return r;

    out->block = block;
    out->size = rd64(inode, 0x3d8);
    out->mode = rd16(inode, 0x3b8);
    out->attr = rd16(inode, 0x3ba);
    out->created = rd_time(inode, 0x3c8);
    out->modified = rd_time(inode, 0x3d0);

    out->runs.clear();

    BlockInfo segment = block;

    for (uint32_t step = 0; step < MAX_SEGMENTS; step++) {
        uint32_t count = rd32(inode, 0x3e4);

        if (count > INODE_MAX_BLOCKS)
            count = INODE_MAX_BLOCKS;

        for (uint32_t i = 1; i < count; i++) {
            BlockInfo data = rd_block(inode, 0x28 + i * 8);

            if (!data.count)
                continue;

            Run run;

            if (locate(fs, data, data.count, &run.offset, &run.size) < 0)
                return FS_ERR_CORRUPT;

            out->runs.push_back(run);
        }

        BlockInfo next = rd_block(inode, 0x10);

        if (!next.count)
            break;

        segment = next;

        r = read_inode(fs, segment, inode);

        if (r < 0)
            return r;
    }

    return FS_OK;
}

static int64_t read_runs(Pfs* fs, const std::vector <Run>& runs, uint64_t size, uint64_t offset, void* buf, uint64_t want) {
    if (offset >= size)
        return 0;

    if (want > size - offset)
        want = size - offset;

    uint8_t* out = (uint8_t*)buf;
    uint64_t left = want;

    while (left) {
        uint64_t base = 0;
        const Run* found = nullptr;

        for (const Run& run : runs) {
            if (offset < base + run.size) {
                found = &run;

                break;
            }

            base += run.size;
        }

        if (!found)
            return -1;

        uint64_t within = offset - base;
        uint64_t chunk = found->size - within;

        if (chunk > left)
            chunk = left;

        if (blk::read(fs->dev, found->offset + within, out, chunk) < 0)
            return -1;

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)want;
}

struct Dirent {
    char name[256] = {};
    BlockInfo block;
    bool directory = false;
};

template <typename Fn>
static int for_each(Pfs* fs, const Node& dir, Fn fn) {
    uint64_t total = 0;

    for (const Run& run : dir.runs)
        total += run.size;

    if (dir.size < total)
        total = dir.size;

    std::vector <uint8_t> group(DENTRY_GROUP);

    for (uint64_t at = 0; at + DENTRY_GROUP <= total; at += DENTRY_GROUP) {
        if (read_runs(fs, dir.runs, total, at, group.data(), DENTRY_GROUP) != (int64_t)DENTRY_GROUP)
            return FS_ERR_IO;

        uint32_t offset = 0;

        while (offset + 8 <= DENTRY_GROUP) {
            uint32_t inode = rd32(group.data(), offset);
            uint8_t sub = group[offset + 4];
            uint8_t length = group[offset + 5];
            uint16_t alen = rd16(group.data(), offset + 6);

            uint16_t step = alen & ALEN_MASK;

            if (step < 12 || (step & 3) || offset + step > DENTRY_GROUP)
                break;

            if (length && offset + 8 + length <= DENTRY_GROUP) {
                Dirent entry;

                memcpy(entry.name, group.data() + offset + 8, length);

                entry.name[length] = '\0';
                entry.block.number = inode;
                entry.block.subpart = sub;
                entry.block.count = 1;
                entry.directory = (alen & ~ALEN_MASK) == MODE_DIRECTORY;

                if (strcmp(entry.name, ".") && strcmp(entry.name, "..")) {
                    if (!fn(entry))
                        return FS_OK;
                }
            }

            offset += step;
        }
    }

    return FS_OK;
}

static int resolve(Pfs* fs, const char* path, Node* out) {
    int r = collect_runs(fs, fs->root, out);

    if (r < 0)
        return r;

    for (const std::string& part : path_split(path)) {
        if (!(out->mode & MODE_DIRECTORY))
            return FS_ERR_NOT_DIRECTORY;

        bool found = false;

        BlockInfo next;

        r = for_each(fs, *out, [&](const Dirent& entry) {
            if (part != entry.name)
                return true;

            next = entry.block;
            found = true;

            return false;
        });

        if (r < 0)
            return r;

        if (!found)
            return FS_ERR_NOT_FOUND;

        r = collect_runs(fs, next, out);

        if (r < 0)
            return r;
    }

    return FS_OK;
}

static void fill_entry(const char* name, const Node& node, bool directory, Entry* out) {
    *out = Entry();

    snprintf(out->name, sizeof(out->name), "%s", name);

    out->size = directory ? 0 : node.size;
    out->created = node.created;
    out->modified = node.modified;
    out->cookie = node.block.number;

    if (directory) out->flags |= ENTRY_DIRECTORY;
    if (node.attr & ATTR_HIDDEN) out->flags |= ENTRY_HIDDEN;
    if (node.attr & ATTR_PSX) out->flags |= ENTRY_PSX_SAVE;
}

static int pfs_list(void* udata, const char* path, std::vector <Entry>* out) {
    Pfs* fs = (Pfs*)udata;

    Node dir;

    int r = resolve(fs, path, &dir);

    if (r < 0)
        return r;

    if (!(dir.mode & MODE_DIRECTORY))
        return FS_ERR_NOT_DIRECTORY;

    return for_each(fs, dir, [&](const Dirent& entry) {
        if (out->size() >= MAX_DIR_ENTRIES)
            return false;

        Node node;

        Entry filled;

        if (collect_runs(fs, entry.block, &node) < 0) {
            node = Node();

            node.block = entry.block;
        }

        fill_entry(entry.name, node, entry.directory, &filled);

        out->push_back(filled);

        return true;
    });
}

static int pfs_stat(void* udata, const char* path, Entry* out) {
    Pfs* fs = (Pfs*)udata;

    Node node;

    int r = resolve(fs, path, &node);

    if (r < 0)
        return r;

    fill_entry(path_basename(path), node, (node.mode & MODE_DIRECTORY) != 0, out);

    return FS_OK;
}

static int pfs_open(void* udata, const char* path, Handle** out) {
    Pfs* fs = (Pfs*)udata;

    Node node;

    int r = resolve(fs, path, &node);

    if (r < 0)
        return r;

    if (node.mode & MODE_DIRECTORY)
        return FS_ERR_IS_DIRECTORY;

    FileHandle* h = new FileHandle();

    h->runs = node.runs;
    h->size = node.size;

    *out = (Handle*)h;

    return FS_OK;
}

static int64_t pfs_read(void* udata, Handle* handle, uint64_t offset, void* buf, uint64_t size) {
    Pfs* fs = (Pfs*)udata;
    FileHandle* h = (FileHandle*)handle;

    return read_runs(fs, h->runs, h->size, offset, buf, size);
}

static void pfs_close_handle(void*, Handle* handle) {
    delete (FileHandle*)handle;
}

static void pfs_close(void* udata) {
    delete (Pfs*)udata;
}

Fs* open(logger::Logger* logger, blk::Device* dev, const std::vector <part::Extent>& extents, bool take_ownership) {
    struct { logger::Logger* logger; size_t logger_id; } src = { logger, get_logger_id(logger) };

    if (extents.empty() || extents[0].size <= SUPER_OFFSET)
        return nullptr;

    uint8_t super[64];

    if (blk::read(dev, extents[0].offset + SUPER_OFFSET, super, sizeof(super)) < 0)
        return nullptr;

    if (rd32(super, 0) != SUPER_MAGIC)
        return nullptr;

    uint32_t version = rd32(super, 4);
    uint32_t zone_size = rd32(super, 0x10);
    uint32_t num_subs = rd32(super, 0x14);

    bool sane =
        zone_size >= 2048 && zone_size <= 128 * 1024 && !(zone_size & (zone_size - 1)) &&
        num_subs < extents.size() + 64;

    if (!sane) {
        iris_error(&src, "PFS superblock is out of range (version {}, zone size {}, subs {})",
            version, zone_size, num_subs);

        return nullptr;
    }

    if (num_subs + 1 > extents.size()) {
        iris_error(&src, "PFS claims {} sub-partitions but the partition has {}", num_subs, extents.size() - 1);

        return nullptr;
    }

    Pfs* fs = new Pfs();

    fs->dev = dev;
    fs->extents = extents;
    fs->zone_size = zone_size;
    fs->root = rd_block(super, 0x20);
    fs->logger = logger;
    fs->logger_id = src.logger_id;

    Node root;

    if (collect_runs(fs, fs->root, &root) < 0 || !(root.mode & MODE_DIRECTORY)) {
        iris_error(&src, "PFS root directory is not usable");

        delete fs;

        return nullptr;
    }

    uint64_t total = 0;

    for (const part::Extent& extent : extents)
        total += extent.size;

    Fs* out = new Fs();

    out->list = pfs_list;
    out->stat = pfs_stat;
    out->open = pfs_open;
    out->read = pfs_read;
    out->close_handle = pfs_close_handle;
    out->close = pfs_close;
    out->udata = fs;
    out->dev = dev;
    out->owns_dev = take_ownership;
    out->type = FS_PFS;
    out->total_bytes = total;
    out->free_bytes = 0;
    out->logger = logger;
    out->logger_id = src.logger_id;

    snprintf(out->variant, sizeof(out->variant), "PFS v%u", version);
    snprintf(out->label, sizeof(out->label), "PFS volume");

    return out;
}

}
