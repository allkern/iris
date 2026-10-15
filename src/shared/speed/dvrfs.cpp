/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <cstring>
#include <ctime>
#include <vector>
#include <algorithm>

#include "ata.hpp"
#include "dvrfs.hpp"

namespace iris::speed::dvrfs {

inline constexpr uint32_t APA_MAGIC = 0x00415041;
inline constexpr uint32_t APA_HEADER_SIZE = 1024;
inline constexpr uint16_t APA_TYPE_MBR = 0x0001;
inline constexpr uint16_t APA_TYPE_PFS = 0x0100;
inline constexpr uint16_t APA_MODVER = 0x0203;
inline constexpr uint32_t APA_MBR_VERSION = 2;

inline constexpr uint32_t EXTEND_SECTORS = 0x40000;
inline constexpr uint32_t XDATA_SECTORS = 0x200000;
inline constexpr uint32_t XCONTENTS_ALIGN = 0x800;

inline constexpr uint32_t PFS_SUPER_MAGIC = 0x50465300;
inline constexpr uint32_t PFS_JOURNAL_MAGIC = 0x5046534c;
inline constexpr uint32_t PFS_SEGD_MAGIC = 0x53454744;
inline constexpr uint32_t PFS_FORMAT_VERSION = 3;
inline constexpr uint32_t PFS_MODVER = 0x0201;
inline constexpr uint32_t PFS_SUPER_SECTOR = 8192;
inline constexpr uint32_t PFS_SUPER_BACKUP_SECTOR = 8193;
inline constexpr uint32_t PFS_META_SIZE = 1024;
inline constexpr uint16_t PFS_DIR_MODE = 0x11ff;
inline constexpr uint16_t PFS_DIR_ATTR = 0x00a0;
inline constexpr uint16_t PFS_DENTRY_DIR = 0x1000;
inline constexpr uint32_t PFS_SEGI_MAGIC = 0x53454749;
inline constexpr uint32_t PFS_SEGD_SLOTS = 114;
inline constexpr uint32_t PFS_SEGI_SLOTS = 123;
inline constexpr uint32_t PFS_DENTRY_GROUP = 512;
inline constexpr uint32_t PFS_MAX_NAME = 255;
inline constexpr uint16_t PFS_TYPE_MASK = 0xf000;
inline constexpr uint16_t PFS_TYPE_DIR = 0x1000;
inline constexpr uint16_t PFS_TYPE_FILE = 0x2000;
inline constexpr uint16_t PFS_ATTR_CLOSED = 0x0080;
inline constexpr uint16_t PFS_OWNER = 0xffff;

inline constexpr uint32_t OPEN_WRITE = 0x0002;
inline constexpr uint32_t OPEN_APPEND = 0x0100;
inline constexpr uint32_t OPEN_CREATE = 0x0200;
inline constexpr uint32_t OPEN_TRUNCATE = 0x0400;
inline constexpr uint32_t OPEN_EXCLUSIVE = 0x0800;

enum : int {
    ERR_NOENT = -2,
    ERR_IO = -5,
    ERR_BADF = -9,
    ERR_EXIST = -17,
    ERR_NOTDIR = -20,
    ERR_ISDIR = -21,
    ERR_INVAL = -22,
    ERR_NOSPC = -28,
    ERR_NOTEMPTY = -90,
    ERR_NAMETOOLONG = -91
};

static inline void put16(uint8_t* p, uint16_t v) {
    p[0] = v;
    p[1] = v >> 8;
}

static inline void put32(uint8_t* p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

static inline uint32_t get32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t get16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint64_t get64(const uint8_t* p) {
    return (uint64_t)get32(p) | ((uint64_t)get32(p + 4) << 32);
}

static inline void put64(uint8_t* p, uint64_t v) {
    put32(p, (uint32_t)v);
    put32(p + 4, (uint32_t)(v >> 32));
}

static void put_time(uint8_t* p) {
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);

    p[0] = 0;
    p[1] = tm.tm_sec;
    p[2] = tm.tm_min;
    p[3] = tm.tm_hour;
    p[4] = tm.tm_mday;
    p[5] = tm.tm_mon + 1;

    put16(&p[6], tm.tm_year + 1900);
}

static uint32_t checksum(const uint8_t* buf) {
    uint32_t sum = 0;

    for (int i = 4; i < 1024; i += 4) {
        sum += get32(&buf[i]);
    }

    return sum;
}

static void read_sectors(ata::Ata* ata, uint64_t lba, uint8_t* buf, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        ata->hdd.read_sector(ata->hdd.udata, lba + i, buf + (i * 512));
    }
}

static void write_sectors(ata::Ata* ata, uint64_t lba, const uint8_t* buf, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        ata->hdd.write_sector(ata->hdd.udata, lba + i, buf + (i * 512));
    }
}

static void write_apa_header(ata::Ata* ata, uint32_t start, uint32_t next, uint32_t prev, const char* id, uint32_t length, uint16_t type) {
    uint8_t h[APA_HEADER_SIZE] = {};

    put32(&h[0x04], APA_MAGIC);
    put32(&h[0x08], next);
    put32(&h[0x0c], prev);

    strncpy((char*)&h[0x10], id, 32);

    put32(&h[0x40], start);
    put32(&h[0x44], length);
    put16(&h[0x48], type);
    put_time(&h[0x50]);

    if (type == APA_TYPE_MBR) {
        put32(&h[0x120], APA_MBR_VERSION);
    } else {
        put16(&h[0x60], APA_MODVER);
    }

    put32(&h[0x00], checksum(h));

    write_sectors(ata, start, h, APA_HEADER_SIZE / 512);
}

static void clear_stale_extend(ata::Ata* ata, uint64_t start) {
    const uint64_t presets[] = { ata::DVR_MAXLBA28_2GB, ata::DVR_MAXLBA28_40GB };

    for (uint64_t lba : presets) {
        if (lba >= start || !ata::has_extend_at(ata, lba))
            continue;

        uint8_t zero[APA_HEADER_SIZE] = {};

        write_sectors(ata, lba, zero, APA_HEADER_SIZE / 512);
    }
}

int format_area(ata::Ata* ata) {
    if (!ata->hdd.write_sector || !ata->maxlba28)
        return ERR_INVAL;

    uint64_t total = ata::get_sector_count(ata);
    uint64_t start = ata->maxlba28;
    uint64_t xdata = start + EXTEND_SECTORS;
    uint64_t xcontents = xdata + XDATA_SECTORS;

    if (total <= xcontents + PFS_SUPER_BACKUP_SECTOR + XCONTENTS_ALIGN)
        return ERR_NOSPC;

    uint32_t xcontents_length = (uint32_t)((total - xcontents) & ~(uint64_t)(XCONTENTS_ALIGN - 1));

    write_apa_header(ata, (uint32_t)start, (uint32_t)xdata, (uint32_t)xcontents, "__extend", EXTEND_SECTORS, APA_TYPE_MBR);
    write_apa_header(ata, (uint32_t)xdata, (uint32_t)xcontents, (uint32_t)start, "__xdata", XDATA_SECTORS, APA_TYPE_PFS);
    write_apa_header(ata, (uint32_t)xcontents, 0, (uint32_t)xdata, "__xcontents", xcontents_length, APA_TYPE_PFS);

    clear_stale_extend(ata, start);

    return 0;
}

static bool find_partition(ata::Ata* ata, const std::string& name, uint64_t* start, uint32_t* length) {
    uint64_t lba = ata->maxlba28;
    uint64_t total = ata::get_sector_count(ata);

    for (int i = 0; i < 64 && lba && lba < total; i++) {
        uint8_t h[APA_HEADER_SIZE];

        read_sectors(ata, lba, h, APA_HEADER_SIZE / 512);

        if (get32(&h[0x04]) != APA_MAGIC)
            return false;

        if (!strncmp((const char*)&h[0x10], name.c_str(), 32)) {
            *start = get32(&h[0x40]);
            *length = get32(&h[0x44]);

            return true;
        }

        uint32_t next = get32(&h[0x08]);

        if (next == ata->maxlba28)
            return false;

        lba = next;
    }

    return false;
}

static uint32_t bitmap_sectors(uint32_t scale, uint32_t size) {
    uint32_t zones = size >> scale;

    zones = zones / 8 + (zones & 7);

    return zones / 512 + (zones & 511);
}

static uint32_t bitmap_blocks(uint32_t scale, uint32_t size) {
    uint32_t sectors = bitmap_sectors(scale, size);

    return (sectors >> scale) + ((sectors & ((1u << scale) - 1)) ? 1 : 0);
}

static void put_blockinfo(uint8_t* p, uint32_t number, uint16_t count) {
    put32(&p[0], number);
    put16(&p[4], 0);
    put16(&p[6], count);
}

int format_pfs(ata::Ata* ata, const std::string& partition, uint32_t zone_size, uint32_t fragment) {
    if (!ata->hdd.write_sector || !ata->maxlba28)
        return ERR_INVAL;

    if ((zone_size & (zone_size - 1)) || zone_size < 2048 || zone_size > 16384 * 1024)
        return ERR_INVAL;

    uint64_t start = 0;
    uint32_t size = 0;

    if (!find_partition(ata, partition, &start, &size))
        return ERR_NOENT;

    uint32_t scale = 0;

    while ((512u << scale) != zone_size) {
        scale++;
    }

    uint32_t log_number = bitmap_blocks(scale, size) + (0x2000 >> scale) + 1;
    uint16_t log_count = std::max<uint32_t>(0x20000 / zone_size, 1);
    uint32_t root_number = log_number + log_count;

    if (((uint64_t)(root_number + 2) << scale) >= size)
        return ERR_NOSPC;

    uint8_t journal[PFS_META_SIZE] = {};

    put32(&journal[0], PFS_JOURNAL_MAGIC);

    write_sectors(ata, start + ((uint64_t)log_number << scale), journal, 2);

    uint8_t dentry[PFS_META_SIZE] = {};

    put32(&dentry[0], root_number);
    dentry[5] = 1;
    put16(&dentry[6], 12 | PFS_DENTRY_DIR);
    dentry[8] = '.';

    put32(&dentry[12], root_number);
    dentry[17] = 2;
    put16(&dentry[18], 500 | PFS_DENTRY_DIR);
    dentry[20] = '.';
    dentry[21] = '.';

    write_sectors(ata, start + ((uint64_t)(root_number + 1) << scale), dentry, 2);

    uint8_t inode[PFS_META_SIZE] = {};

    put32(&inode[0x04], PFS_SEGD_MAGIC);
    put_blockinfo(&inode[0x08], root_number, 1);
    put_blockinfo(&inode[0x18], root_number, 1);
    put_blockinfo(&inode[0x28], root_number, 1);
    put_blockinfo(&inode[0x30], root_number + 1, 1);
    put16(&inode[0x3b8], PFS_DIR_MODE);
    put16(&inode[0x3ba], PFS_DIR_ATTR);
    put_time(&inode[0x3c0]);
    put_time(&inode[0x3c8]);
    put_time(&inode[0x3d0]);
    put32(&inode[0x3d8], 512);
    put32(&inode[0x3e0], 2);
    put32(&inode[0x3e4], 2);
    put32(&inode[0x3e8], 1);
    put32(&inode[0x00], checksum(inode));

    write_sectors(ata, start + ((uint64_t)root_number << scale), inode, 2);

    uint32_t reserved = (0x2000 >> scale) + log_count + 3 + bitmap_blocks(scale, size);
    uint32_t count = bitmap_sectors(scale, size);
    uint64_t sector = start + (1u << scale) + 0x2000;

    std::vector <uint8_t> bitmap(512);

    for (uint32_t i = 0; i < count; i++) {
        for (int w = 0; w < 128; w++) {
            put32(&bitmap[w * 4], fragment);
        }

        if (i == 0) {
            for (uint32_t bit = 0; bit < reserved && bit < 4096; bit++) {
                bitmap[bit >> 3] |= 1 << (bit & 7);
            }
        }

        write_sectors(ata, sector + i, bitmap.data(), 1);
    }

    uint8_t super[512] = {};

    put32(&super[0x00], PFS_SUPER_MAGIC);
    put32(&super[0x04], PFS_FORMAT_VERSION);
    put32(&super[0x08], PFS_MODVER);
    put32(&super[0x10], zone_size);
    put_blockinfo(&super[0x18], log_number, log_count);
    put_blockinfo(&super[0x20], root_number, 1);

    write_sectors(ata, start + PFS_SUPER_BACKUP_SECTOR, super, 1);
    write_sectors(ata, start + PFS_SUPER_SECTOR, super, 1);

    return 0;
}


struct Volume {
    ata::Ata* ata = nullptr;
    std::string partition;

    uint64_t start = 0;
    uint32_t sectors = 0;
    uint32_t zone_size = 0;
    uint32_t scale = 0;
    uint32_t zones = 0;
    uint32_t root = 0;

    uint32_t bitmap_sector = 0;
    std::vector <uint8_t> bitmap;
    std::vector <bool> dirty;
    uint32_t free = 0;
};

struct Extent {
    uint32_t number = 0;
    uint32_t count = 0;
};

struct Node {
    uint32_t zone = 0;
    uint8_t raw[PFS_META_SIZE] = {};
    std::vector <uint32_t> descriptors;
    std::vector <Extent> extents;
    bool extents_dirty = false;
};

struct Dentry {
    uint32_t inode = 0;
    uint16_t type = 0;
    std::string name;
};

static uint64_t zone_lba(Volume* volume, uint32_t zone) {
    return volume->start + ((uint64_t)zone << volume->scale);
}

static void read_meta(Volume* volume, uint32_t zone, uint8_t* buf) {
    read_sectors(volume->ata, zone_lba(volume, zone), buf, PFS_META_SIZE / 512);
}

static void write_meta(Volume* volume, uint32_t zone, uint8_t* buf) {
    put32(&buf[0x00], checksum(buf));

    write_sectors(volume->ata, zone_lba(volume, zone), buf, PFS_META_SIZE / 512);
}

static void read_bytes(Volume* volume, uint64_t offset, uint8_t* buf, uint64_t size) {
    uint8_t sector[512];

    while (size) {
        uint64_t lba = volume->start + offset / 512;
        uint32_t within = (uint32_t)(offset % 512);
        uint32_t chunk = (uint32_t)std::min<uint64_t>(512 - within, size);

        if (!within && chunk == 512) {
            read_sectors(volume->ata, lba, buf, 1);
        } else {
            read_sectors(volume->ata, lba, sector, 1);

            memcpy(buf, &sector[within], chunk);
        }

        offset += chunk;
        buf += chunk;
        size -= chunk;
    }
}

static void write_bytes(Volume* volume, uint64_t offset, const uint8_t* buf, uint64_t size) {
    uint8_t sector[512];

    while (size) {
        uint64_t lba = volume->start + offset / 512;
        uint32_t within = (uint32_t)(offset % 512);
        uint32_t chunk = (uint32_t)std::min<uint64_t>(512 - within, size);

        if (!within && chunk == 512) {
            write_sectors(volume->ata, lba, buf, 1);
        } else {
            read_sectors(volume->ata, lba, sector, 1);

            memcpy(&sector[within], buf, chunk);

            write_sectors(volume->ata, lba, sector, 1);
        }

        offset += chunk;
        buf += chunk;
        size -= chunk;
    }
}

static bool zone_used(Volume* volume, uint32_t zone) {
    return volume->bitmap[zone >> 3] & (1 << (zone & 7));
}

static void mark_zone(Volume* volume, uint32_t zone, bool used) {
    uint8_t bit = 1 << (zone & 7);
    uint8_t& byte = volume->bitmap[zone >> 3];

    if (((byte & bit) != 0) == used)
        return;

    if (used) {
        byte |= bit;
        volume->free--;
    } else {
        byte &= ~bit;
        volume->free++;
    }

    volume->dirty[zone >> 12] = true;
}

static void flush_bitmap(Volume* volume) {
    for (size_t i = 0; i < volume->dirty.size(); i++) {
        if (!volume->dirty[i])
            continue;

        write_sectors(volume->ata, volume->start + volume->bitmap_sector + i, &volume->bitmap[i * 512], 1);

        volume->dirty[i] = false;
    }
}

static bool find_run(Volume* volume, uint32_t from, uint32_t to, uint32_t length, uint32_t* out) {
    uint32_t run = 0;

    for (uint32_t zone = from; zone < to; zone++) {
        if (zone_used(volume, zone)) {
            run = 0;

            continue;
        }

        if (++run == length) {
            *out = zone + 1 - length;

            return true;
        }
    }

    return false;
}

static bool allocate(Volume* volume, uint32_t hint, uint32_t want, Extent* out) {
    want = std::clamp<uint32_t>(want, 1, 0xffff);

    if (hint >= volume->zones) {
        hint = 0;
    }

    for (uint32_t length = want; length; length /= 2) {
        uint32_t at = 0;

        if (!find_run(volume, hint, volume->zones, length, &at) && !find_run(volume, 0, hint, length, &at))
            continue;

        for (uint32_t i = 0; i < length; i++) {
            mark_zone(volume, at + i, true);
        }

        out->number = at;
        out->count = length;

        return true;
    }

    return false;
}

static uint32_t grow_extent(Volume* volume, Extent& extent, uint32_t want) {
    uint32_t grown = 0;

    while (grown < want && extent.count < 0xffff) {
        uint32_t zone = extent.number + extent.count;

        if (zone >= volume->zones || zone_used(volume, zone))
            break;

        mark_zone(volume, zone, true);

        extent.count++;
        grown++;
    }

    return grown;
}

static uint32_t fix_index(uint32_t index) {
    if (index < PFS_SEGD_SLOTS)
        return index;

    return (index - PFS_SEGD_SLOTS) % PFS_SEGI_SLOTS;
}

static uint64_t node_size(const Node& node) {
    return get64(&node.raw[0x3d8]);
}

static uint16_t node_mode(const Node& node) {
    return get16(&node.raw[0x3b8]);
}

static uint16_t node_attr(const Node& node) {
    return get16(&node.raw[0x3ba]);
}

static bool node_is_dir(const Node& node) {
    return (node_mode(node) & PFS_TYPE_MASK) == PFS_TYPE_DIR;
}

static uint64_t node_capacity(Volume* volume, const Node& node) {
    uint64_t zones = 0;

    for (const Extent& extent : node.extents) {
        zones += extent.count;
    }

    return zones * volume->zone_size;
}

static int load_node(Volume* volume, uint32_t zone, Node* node) {
    if (zone >= volume->zones)
        return ERR_IO;

    read_meta(volume, zone, node->raw);

    if (get32(&node->raw[0x04]) != PFS_SEGD_MAGIC)
        return ERR_IO;

    node->zone = zone;
    node->descriptors.assign(1, zone);
    node->extents.clear();
    node->extents_dirty = false;

    uint32_t slots = get32(&node->raw[0x3e4]);
    uint8_t segment[PFS_META_SIZE];

    memcpy(segment, node->raw, PFS_META_SIZE);

    for (uint32_t i = 1; i < slots && i < 0x100000; i++) {
        uint32_t index = fix_index(i);

        if (!index) {
            uint32_t next = get32(&segment[0x10]);

            if (!next || next >= volume->zones)
                return ERR_IO;

            read_meta(volume, next, segment);

            if (get32(&segment[0x04]) != PFS_SEGI_MAGIC)
                return ERR_IO;

            node->descriptors.push_back(next);

            continue;
        }

        Extent extent;

        extent.number = get32(&segment[0x28 + index * 8]);
        extent.count = get16(&segment[0x28 + index * 8 + 6]);

        if (extent.count) {
            node->extents.push_back(extent);
        }
    }

    return 0;
}

static int store_node(Volume* volume, Node& node) {
    if (node.extents_dirty) {
        size_t count = node.extents.size();
        size_t first = PFS_SEGD_SLOTS - 1;
        size_t per_segment = PFS_SEGI_SLOTS - 1;
        size_t needed = 1 + (count > first ? (count - first + per_segment - 1) / per_segment : 0);

        while (node.descriptors.size() < needed) {
            Extent extent;

            if (!allocate(volume, node.descriptors.back() + 1, 1, &extent)) {
                flush_bitmap(volume);

                return ERR_NOSPC;
            }

            node.descriptors.push_back(extent.number);
        }

        while (node.descriptors.size() > needed) {
            mark_zone(volume, node.descriptors.back(), false);

            node.descriptors.pop_back();
        }

        uint64_t zones = 0;

        for (const Extent& extent : node.extents) {
            zones += extent.count;
        }

        memset(&node.raw[0x10], 0, 8);
        memset(&node.raw[0x28], 0, PFS_SEGD_SLOTS * 8);

        put_blockinfo(&node.raw[0x18], node.descriptors.back(), 1);
        put_blockinfo(&node.raw[0x28], node.zone, 1);

        if (needed > 1) {
            put_blockinfo(&node.raw[0x10], node.descriptors[1], 1);
        }

        size_t next = 0;

        for (uint32_t slot = 1; slot < PFS_SEGD_SLOTS && next < count; slot++, next++) {
            put_blockinfo(&node.raw[0x28 + slot * 8], node.extents[next].number, (uint16_t)node.extents[next].count);
        }

        for (size_t d = 1; d < needed; d++) {
            uint8_t segment[PFS_META_SIZE] = {};

            put32(&segment[0x04], PFS_SEGI_MAGIC);
            put_blockinfo(&segment[0x08], node.zone, 1);
            put_blockinfo(&segment[0x18], node.descriptors[d - 1], 1);
            put_blockinfo(&segment[0x28], node.descriptors[d], 1);

            if (d + 1 < needed) {
                put_blockinfo(&segment[0x10], node.descriptors[d + 1], 1);
            }

            for (uint32_t slot = 1; slot < PFS_SEGI_SLOTS && next < count; slot++, next++) {
                put_blockinfo(&segment[0x28 + slot * 8], node.extents[next].number, (uint16_t)node.extents[next].count);
            }

            write_meta(volume, node.descriptors[d], segment);
        }

        put32(&node.raw[0x3e0], (uint32_t)(zones + needed));
        put32(&node.raw[0x3e4], (uint32_t)(count + needed));
        put32(&node.raw[0x3e8], (uint32_t)needed);

        node.extents_dirty = false;
    }

    write_meta(volume, node.zone, node.raw);
    flush_bitmap(volume);

    return 0;
}

static int reserve(Volume* volume, Node& node, uint64_t bytes) {
    uint64_t needed = (bytes + volume->zone_size - 1) / volume->zone_size;
    uint64_t have = node_capacity(volume, node) / volume->zone_size;

    if (needed <= have)
        return 0;

    uint32_t missing = (uint32_t)(needed - have);

    if (!node.extents.empty()) {
        uint32_t grown = grow_extent(volume, node.extents.back(), missing);

        if (grown) {
            node.extents_dirty = true;
        }

        missing -= grown;
    }

    while (missing) {
        uint32_t hint = node.extents.empty() ? node.zone + 1 : node.extents.back().number + node.extents.back().count;
        Extent extent;

        if (!allocate(volume, hint, missing, &extent))
            return ERR_NOSPC;

        node.extents.push_back(extent);
        node.extents_dirty = true;

        missing -= extent.count;
    }

    return 0;
}

static uint64_t node_io(Volume* volume, const Node& node, uint64_t offset, uint8_t* buf, uint64_t size, bool write) {
    uint64_t base = 0;
    uint64_t left = size;

    for (const Extent& extent : node.extents) {
        if (!left)
            break;

        uint64_t length = (uint64_t)extent.count * volume->zone_size;

        if (offset < base + length) {
            uint64_t within = offset - base;
            uint64_t chunk = std::min(length - within, left);
            uint64_t at = ((uint64_t)extent.number << volume->scale) * 512 + within;

            if (write) {
                write_bytes(volume, at, buf, chunk);
            } else {
                read_bytes(volume, at, buf, chunk);
            }

            offset += chunk;
            buf += chunk;
            left -= chunk;
        }

        base += length;
    }

    return size - left;
}

static void touch(Node& node, bool modified) {
    put_time(&node.raw[0x3c0]);

    if (modified) {
        put_time(&node.raw[0x3d0]);
    }
}

static void fill_stat(const Node& node, Stat* stat) {
    stat->mode = node_mode(node);
    stat->attr = node_attr(node);
    stat->size = node_size(node);

    memcpy(stat->atime, &node.raw[0x3c0], 8);
    memcpy(stat->ctime, &node.raw[0x3c8], 8);
    memcpy(stat->mtime, &node.raw[0x3d0], 8);

    stat->uid = get16(&node.raw[0x3bc]);
    stat->gid = get16(&node.raw[0x3be]);
    stat->blocks = get32(&node.raw[0x3e0]);
    stat->data = get32(&node.raw[0x3e4]);
}

static void free_node(Volume* volume, const Node& node) {
    for (const Extent& extent : node.extents) {
        for (uint32_t i = 0; i < extent.count; i++) {
            mark_zone(volume, extent.number + i, false);
        }
    }

    for (uint32_t zone : node.descriptors) {
        mark_zone(volume, zone, false);
    }

    flush_bitmap(volume);
}

static int read_dir(Volume* volume, const Node& node, std::vector <uint8_t>* out) {
    uint64_t size = node_size(node);

    if (size % PFS_DENTRY_GROUP || size > node_capacity(volume, node))
        return ERR_IO;

    out->assign((size_t)size, 0);

    node_io(volume, node, 0, out->data(), size, false);

    return 0;
}

static int list_dir(Volume* volume, const Node& node, std::vector <Dentry>* out) {
    std::vector <uint8_t> data;

    int r = read_dir(volume, node, &data);

    if (r < 0)
        return r;

    out->clear();

    for (size_t group = 0; group < data.size(); group += PFS_DENTRY_GROUP) {
        uint32_t offset = 0;

        while (offset < PFS_DENTRY_GROUP) {
            const uint8_t* d = &data[group + offset];
            uint16_t alen = get16(&d[6]) & 0x0fff;

            if (alen < 8 || (alen & 3) || offset + alen > PFS_DENTRY_GROUP)
                return ERR_IO;

            if (d[5] && 8u + d[5] <= alen) {
                Dentry entry;

                entry.inode = get32(d);
                entry.type = get16(&d[6]) & PFS_TYPE_MASK;
                entry.name.assign((const char*)&d[8], d[5]);

                out->push_back(entry);
            }

            offset += alen;
        }
    }

    return 0;
}

static int lookup(Volume* volume, uint32_t dir, const std::string& name, Dentry* out) {
    Node node;

    int r = load_node(volume, dir, &node);

    if (r < 0)
        return r;

    if (!node_is_dir(node))
        return ERR_NOTDIR;

    std::vector <Dentry> entries;

    r = list_dir(volume, node, &entries);

    if (r < 0)
        return r;

    for (const Dentry& entry : entries) {
        if (entry.name == name) {
            *out = entry;

            return 0;
        }
    }

    return ERR_NOENT;
}

static void fill_dentry(uint8_t* d, uint32_t inode, const std::string& name, uint32_t length, uint16_t type) {
    put32(&d[0], inode);

    d[4] = 0;
    d[5] = (uint8_t)name.size();

    put16(&d[6], (uint16_t)(length | type));

    memcpy(&d[8], name.data(), name.size());
}

static int add_entry(Volume* volume, uint32_t dir, const std::string& name, uint32_t inode, uint16_t type) {
    Node node;

    int r = load_node(volume, dir, &node);

    if (r < 0)
        return r;

    std::vector <uint8_t> data;

    r = read_dir(volume, node, &data);

    if (r < 0)
        return r;

    uint32_t needed = ((uint32_t)name.size() + 8 + 3) & ~3u;

    for (size_t group = 0; group < data.size(); group += PFS_DENTRY_GROUP) {
        uint32_t offset = 0;

        while (offset < PFS_DENTRY_GROUP) {
            uint8_t* d = &data[group + offset];
            uint16_t alen = get16(&d[6]) & 0x0fff;

            if (alen < 8 || (alen & 3) || offset + alen > PFS_DENTRY_GROUP)
                return ERR_IO;

            uint32_t used = (d[5] + 8 + 3) & 0x1fc;
            bool fits = (get32(d) || alen < needed) ? (alen >= used && alen - used >= needed) : true;

            if (fits) {
                uint32_t length = alen;

                if (d[5]) {
                    length -= (d[5] + 11) & 0x1fc;
                }

                put16(&d[6], (uint16_t)((get16(&d[6]) & PFS_TYPE_MASK) | (alen - length)));

                fill_dentry(d + (alen - length), inode, name, length, type);

                node_io(volume, node, group, &data[group], PFS_DENTRY_GROUP, true);

                touch(node, true);

                return store_node(volume, node);
            }

            offset += alen;
        }
    }

    uint64_t size = node_size(node);

    r = reserve(volume, node, size + PFS_DENTRY_GROUP);

    if (r < 0) {
        flush_bitmap(volume);

        return r;
    }

    uint8_t group[PFS_DENTRY_GROUP] = {};

    fill_dentry(group, inode, name, PFS_DENTRY_GROUP, type);

    node_io(volume, node, size, group, PFS_DENTRY_GROUP, true);

    put64(&node.raw[0x3d8], size + PFS_DENTRY_GROUP);

    touch(node, true);

    return store_node(volume, node);
}

static int remove_entry(Volume* volume, uint32_t dir, const std::string& name) {
    Node node;

    int r = load_node(volume, dir, &node);

    if (r < 0)
        return r;

    std::vector <uint8_t> data;

    r = read_dir(volume, node, &data);

    if (r < 0)
        return r;

    for (size_t group = 0; group < data.size(); group += PFS_DENTRY_GROUP) {
        uint32_t offset = 0;
        int previous = -1;

        while (offset < PFS_DENTRY_GROUP) {
            uint8_t* d = &data[group + offset];
            uint16_t alen = get16(&d[6]) & 0x0fff;

            if (alen < 8 || (alen & 3) || offset + alen > PFS_DENTRY_GROUP)
                return ERR_IO;

            if (d[5] == name.size() && !memcmp(&d[8], name.data(), name.size())) {
                if (previous >= 0) {
                    uint8_t* p = &data[group + previous];

                    put16(&p[6], (uint16_t)((get16(&p[6]) & PFS_TYPE_MASK) | ((get16(&p[6]) & 0x0fff) + alen)));
                } else {
                    put32(d, 0);

                    d[5] = 0;

                    if (group + offset + alen >= node_size(node)) {
                        put64(&node.raw[0x3d8], node_size(node) - alen);
                    }
                }

                node_io(volume, node, group, &data[group], PFS_DENTRY_GROUP, true);

                touch(node, true);

                return store_node(volume, node);
            }

            previous = (int)offset;
            offset += alen;
        }
    }

    return ERR_NOENT;
}

static std::vector <std::string> split_path(const std::string& path) {
    std::vector <std::string> parts;
    std::string part;

    for (char c : path) {
        if (c == '/' || c == '\\') {
            if (!part.empty() && part != ".") {
                parts.push_back(part);
            }

            part.clear();

            continue;
        }

        part += c;
    }

    if (!part.empty() && part != ".") {
        parts.push_back(part);
    }

    return parts;
}

static int resolve(Volume* volume, const std::vector <std::string>& parts, size_t count, uint32_t* out) {
    uint32_t zone = volume->root;

    for (size_t i = 0; i < count; i++) {
        Dentry entry;

        int r = lookup(volume, zone, parts[i], &entry);

        if (r < 0)
            return r;

        zone = entry.inode;
    }

    *out = zone;

    return 0;
}

static int resolve_parent(Volume* volume, const std::string& path, uint32_t* parent, std::string* name) {
    std::vector <std::string> parts = split_path(path);

    if (parts.empty())
        return ERR_INVAL;

    if (parts.back().size() > PFS_MAX_NAME)
        return ERR_NAMETOOLONG;

    *name = parts.back();

    return resolve(volume, parts, parts.size() - 1, parent);
}

static int create_node(Volume* volume, uint32_t parent, uint16_t mode, uint32_t* out) {
    Extent inode;

    if (!allocate(volume, parent + 1, 1, &inode)) {
        flush_bitmap(volume);

        return ERR_NOSPC;
    }

    Node node;

    node.zone = inode.number;
    node.descriptors.assign(1, inode.number);
    node.extents_dirty = true;

    put32(&node.raw[0x04], PFS_SEGD_MAGIC);
    put_blockinfo(&node.raw[0x08], inode.number, 1);
    put16(&node.raw[0x3b8], mode);
    put16(&node.raw[0x3bc], PFS_OWNER);
    put16(&node.raw[0x3be], PFS_OWNER);

    touch(node, true);

    put_time(&node.raw[0x3c8]);

    bool dir = (mode & PFS_TYPE_MASK) == PFS_TYPE_DIR;

    if (dir) {
        Extent data;

        if (!allocate(volume, inode.number + 1, 1, &data)) {
            mark_zone(volume, inode.number, false);
            flush_bitmap(volume);

            return ERR_NOSPC;
        }

        node.extents.push_back(data);

        put16(&node.raw[0x3ba], PFS_DIR_ATTR);
        put64(&node.raw[0x3d8], PFS_DENTRY_GROUP);

        uint8_t group[PFS_DENTRY_GROUP] = {};

        fill_dentry(&group[0], inode.number, ".", 12, PFS_TYPE_DIR);
        fill_dentry(&group[12], parent, "..", 500, PFS_TYPE_DIR);

        node_io(volume, node, 0, group, PFS_DENTRY_GROUP, true);
    } else {
        uint8_t entry[PFS_META_SIZE] = {};

        put16(&entry[2], PFS_META_SIZE);

        write_sectors(volume->ata, zone_lba(volume, inode.number) + PFS_META_SIZE / 512, entry, PFS_META_SIZE / 512);
    }

    int r = store_node(volume, node);

    if (r < 0)
        return r;

    *out = inode.number;

    return 0;
}

static int truncate_node(Volume* volume, Node& node) {
    for (const Extent& extent : node.extents) {
        for (uint32_t i = 0; i < extent.count; i++) {
            mark_zone(volume, extent.number + i, false);
        }
    }

    node.extents.clear();
    node.extents_dirty = true;

    put64(&node.raw[0x3d8], 0);
    put16(&node.raw[0x3ba], node_attr(node) & ~PFS_ATTR_CLOSED);

    uint8_t entry[PFS_META_SIZE] = {};

    put16(&entry[2], PFS_META_SIZE);

    write_sectors(volume->ata, zone_lba(volume, node.zone) + PFS_META_SIZE / 512, entry, PFS_META_SIZE / 512);

    touch(node, true);

    return store_node(volume, node);
}

Volume* mount(ata::Ata* ata, const std::string& partition) {
    if (!ata->hdd.read_sector || !ata->hdd.write_sector || !ata->maxlba28)
        return nullptr;

    uint64_t start = 0;
    uint32_t length = 0;

    if (!find_partition(ata, partition, &start, &length))
        return nullptr;

    uint8_t super[512];

    read_sectors(ata, start + PFS_SUPER_SECTOR, super, 1);

    uint32_t zone_size = get32(&super[0x10]);

    if (get32(&super[0x00]) != PFS_SUPER_MAGIC || get32(&super[0x14]) != 0)
        return nullptr;

    if (!zone_size || (zone_size & (zone_size - 1)) || zone_size < 2048)
        return nullptr;

    Volume* volume = new Volume();

    volume->ata = ata;
    volume->partition = partition;
    volume->start = start;
    volume->sectors = length;
    volume->zone_size = zone_size;

    while ((512u << volume->scale) != zone_size) {
        volume->scale++;
    }

    volume->zones = length >> volume->scale;
    volume->root = get32(&super[0x20]);
    volume->bitmap_sector = (1u << volume->scale) + 0x2000;

    uint32_t count = bitmap_sectors(volume->scale, length);

    volume->bitmap.assign((size_t)count * 512, 0);
    volume->dirty.assign(count, false);

    read_sectors(ata, start + volume->bitmap_sector, volume->bitmap.data(), count);

    volume->zones = std::min<uint32_t>(volume->zones, count * 4096);

    uint64_t total = ata::get_sector_count(ata);

    if (start < total) {
        volume->zones = (uint32_t)std::min<uint64_t>(volume->zones, (total - start) >> volume->scale);
    }

    for (uint32_t zone = 0; zone < volume->zones; zone++) {
        if (!zone_used(volume, zone)) {
            volume->free++;
        }
    }

    Node root;

    if (load_node(volume, volume->root, &root) < 0 || !node_is_dir(root)) {
        delete volume;

        return nullptr;
    }

    return volume;
}

void unmount(Volume* volume) {
    if (!volume)
        return;

    flush_bitmap(volume);

    delete volume;
}

const std::string& partition_of(Volume* volume) {
    return volume->partition;
}

int open(Volume* volume, const std::string& path, uint32_t flags, uint16_t mode, Handle* out) {
    uint32_t parent = 0;
    std::string name;

    int r = resolve_parent(volume, path, &parent, &name);

    if (r == ERR_INVAL)
        return ERR_ISDIR;

    if (r < 0)
        return r;

    Dentry entry;

    r = lookup(volume, parent, name, &entry);

    if (r < 0 && r != ERR_NOENT)
        return r;

    Node node;

    if (r == ERR_NOENT) {
        if (!(flags & OPEN_CREATE))
            return ERR_NOENT;

        uint32_t zone = 0;

        r = create_node(volume, parent, (uint16_t)((mode & 0x0fff) | PFS_TYPE_FILE), &zone);

        if (r < 0)
            return r;

        r = add_entry(volume, parent, name, zone, PFS_TYPE_FILE);

        if (r < 0)
            return r;

        r = load_node(volume, zone, &node);
    } else {
        if ((flags & OPEN_CREATE) && (flags & OPEN_EXCLUSIVE))
            return ERR_EXIST;

        if (entry.type == PFS_TYPE_DIR)
            return ERR_ISDIR;

        r = load_node(volume, entry.inode, &node);

        if (r == 0 && (flags & OPEN_TRUNCATE) && (flags & OPEN_WRITE)) {
            r = truncate_node(volume, node);
        }
    }

    if (r < 0)
        return r;

    if ((flags & OPEN_WRITE) && (node_attr(node) & PFS_ATTR_CLOSED)) {
        put16(&node.raw[0x3ba], node_attr(node) & ~PFS_ATTR_CLOSED);

        store_node(volume, node);
    }

    *out = Handle();

    out->inode = node.zone;
    out->flags = flags;
    out->position = (flags & OPEN_APPEND) ? node_size(node) : 0;

    return 0;
}

int close(Volume* volume, Handle* handle) {
    if (handle->directory || !(handle->flags & OPEN_WRITE))
        return 0;

    Node node;

    int r = load_node(volume, handle->inode, &node);

    if (r < 0)
        return r;

    put16(&node.raw[0x3ba], node_attr(node) | PFS_ATTR_CLOSED);

    return store_node(volume, node);
}

int64_t read(Volume* volume, Handle* handle, uint8_t* buf, uint32_t size) {
    if (handle->directory)
        return ERR_BADF;

    Node node;

    int r = load_node(volume, handle->inode, &node);

    if (r < 0)
        return r;

    uint64_t length = node_size(node);

    if (handle->position >= length)
        return 0;

    uint64_t want = std::min<uint64_t>(size, length - handle->position);
    uint64_t done = node_io(volume, node, handle->position, buf, want, false);

    handle->position += done;

    return (int64_t)done;
}

int64_t write(Volume* volume, Handle* handle, const uint8_t* buf, uint32_t size) {
    if (handle->directory || !(handle->flags & OPEN_WRITE))
        return ERR_BADF;

    Node node;

    int r = load_node(volume, handle->inode, &node);

    if (r < 0)
        return r;

    if (handle->flags & OPEN_APPEND) {
        handle->position = node_size(node);
    }

    uint64_t end = handle->position + size;

    r = reserve(volume, node, end);

    if (r < 0) {
        store_node(volume, node);

        return r;
    }

    uint64_t done = node_io(volume, node, handle->position, (uint8_t*)buf, size, true);

    handle->position += done;

    if (handle->position > node_size(node)) {
        put64(&node.raw[0x3d8], handle->position);
    }

    touch(node, true);

    r = store_node(volume, node);

    if (r < 0)
        return r;

    return (int64_t)done;
}

int64_t seek(Volume* volume, Handle* handle, int64_t offset, int whence) {
    if (handle->directory)
        return ERR_BADF;

    Node node;

    int r = load_node(volume, handle->inode, &node);

    if (r < 0)
        return r;

    int64_t size = (int64_t)node_size(node);
    int64_t base = whence == 1 ? (int64_t)handle->position : (whence == 2 ? size : 0);
    int64_t position = base + offset;

    if (position < 0 || position > size)
        return ERR_INVAL;

    handle->position = (uint64_t)position;

    return position;
}

int dopen(Volume* volume, const std::string& path, Handle* out) {
    std::vector <std::string> parts = split_path(path);
    uint32_t zone = 0;

    int r = resolve(volume, parts, parts.size(), &zone);

    if (r < 0)
        return r;

    Node node;

    r = load_node(volume, zone, &node);

    if (r < 0)
        return r;

    if (!node_is_dir(node))
        return ERR_NOTDIR;

    std::vector <Dentry> entries;

    r = list_dir(volume, node, &entries);

    if (r < 0)
        return r;

    *out = Handle();

    out->inode = zone;
    out->directory = true;

    for (const Dentry& entry : entries) {
        out->names.push_back(entry.name);
        out->inodes.push_back(entry.inode);
    }

    return 0;
}

int dread(Volume* volume, Handle* handle, std::string* name, Stat* stat) {
    if (!handle->directory)
        return ERR_BADF;

    if (handle->index >= handle->names.size())
        return 0;

    size_t index = handle->index++;
    Node node;

    *name = handle->names[index];
    *stat = Stat();

    if (load_node(volume, handle->inodes[index], &node) == 0) {
        fill_stat(node, stat);
    }

    return (int)name->size();
}

int getstat(Volume* volume, const std::string& path, Stat* stat) {
    std::vector <std::string> parts = split_path(path);
    uint32_t zone = 0;

    int r = resolve(volume, parts, parts.size(), &zone);

    if (r < 0)
        return r;

    Node node;

    r = load_node(volume, zone, &node);

    if (r < 0)
        return r;

    fill_stat(node, stat);

    return 0;
}

int mkdir(Volume* volume, const std::string& path, uint16_t mode) {
    uint32_t parent = 0;
    std::string name;

    int r = resolve_parent(volume, path, &parent, &name);

    if (r == ERR_INVAL)
        return ERR_EXIST;

    if (r < 0)
        return r;

    Dentry entry;

    r = lookup(volume, parent, name, &entry);

    if (r == 0)
        return ERR_EXIST;

    if (r != ERR_NOENT)
        return r;

    uint32_t zone = 0;

    r = create_node(volume, parent, (uint16_t)((mode & 0x0fff) | PFS_TYPE_DIR), &zone);

    if (r < 0)
        return r;

    return add_entry(volume, parent, name, zone, PFS_TYPE_DIR);
}

static int dir_is_empty(Volume* volume, const Node& node) {
    std::vector <Dentry> entries;

    int r = list_dir(volume, node, &entries);

    if (r < 0)
        return r;

    for (const Dentry& entry : entries) {
        if (entry.name != "." && entry.name != "..")
            return 0;
    }

    return 1;
}

static int unlink(Volume* volume, uint32_t parent, const std::string& name, bool dir) {
    Dentry entry;

    int r = lookup(volume, parent, name, &entry);

    if (r < 0)
        return r;

    if (dir && entry.type != PFS_TYPE_DIR)
        return ERR_NOTDIR;

    if (!dir && entry.type == PFS_TYPE_DIR)
        return ERR_ISDIR;

    Node node;

    r = load_node(volume, entry.inode, &node);

    if (r < 0)
        return r;

    if (dir) {
        r = dir_is_empty(volume, node);

        if (r < 0)
            return r;

        if (!r)
            return ERR_NOTEMPTY;
    }

    r = remove_entry(volume, parent, name);

    if (r < 0)
        return r;

    free_node(volume, node);

    return 0;
}

int remove(Volume* volume, const std::string& path) {
    uint32_t parent = 0;
    std::string name;

    int r = resolve_parent(volume, path, &parent, &name);

    if (r == ERR_INVAL)
        return ERR_ISDIR;

    if (r < 0)
        return r;

    return unlink(volume, parent, name, false);
}

int rmdir(Volume* volume, const std::string& path) {
    uint32_t parent = 0;
    std::string name;

    int r = resolve_parent(volume, path, &parent, &name);

    if (r < 0)
        return r;

    if (name == "." || name == "..")
        return ERR_INVAL;

    return unlink(volume, parent, name, true);
}

int rename(Volume* volume, const std::string& from, const std::string& to) {
    uint32_t from_parent = 0;
    uint32_t to_parent = 0;
    std::string from_name;
    std::string to_name;

    int r = resolve_parent(volume, from, &from_parent, &from_name);

    if (r < 0)
        return r;

    r = resolve_parent(volume, to, &to_parent, &to_name);

    if (r < 0)
        return r;

    Dentry source;

    r = lookup(volume, from_parent, from_name, &source);

    if (r < 0)
        return r;

    Dentry target;

    r = lookup(volume, to_parent, to_name, &target);

    if (r == 0) {
        if (target.inode == source.inode)
            return 0;

        if (target.type == PFS_TYPE_DIR && source.type != PFS_TYPE_DIR)
            return ERR_ISDIR;

        if (target.type != PFS_TYPE_DIR && source.type == PFS_TYPE_DIR)
            return ERR_NOTDIR;

        r = unlink(volume, to_parent, to_name, target.type == PFS_TYPE_DIR);

        if (r < 0)
            return r;
    } else if (r != ERR_NOENT) {
        return r;
    }

    r = add_entry(volume, to_parent, to_name, source.inode, source.type);

    if (r < 0)
        return r;

    r = remove_entry(volume, from_parent, from_name);

    if (r < 0)
        return r;

    if (source.type != PFS_TYPE_DIR || from_parent == to_parent)
        return 0;

    Node node;

    r = load_node(volume, source.inode, &node);

    if (r < 0)
        return r;

    uint8_t group[PFS_DENTRY_GROUP];

    node_io(volume, node, 0, group, PFS_DENTRY_GROUP, false);

    put32(&group[12], to_parent);

    node_io(volume, node, 0, group, PFS_DENTRY_GROUP, true);

    return 0;
}

uint32_t zone_size(Volume* volume) {
    return volume->zone_size;
}

uint32_t free_zones(Volume* volume) {
    return volume->free;
}

}
