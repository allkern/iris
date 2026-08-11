#include <cstdio>
#include <cstring>

#include "fs/apa.hpp"
#include "fs/fs.hpp"

namespace iris::fs::apa {

inline constexpr uint64_t LBA = 512;
inline constexpr uint32_t HEADER_SIZE = 1024;
inline constexpr uint32_t MAX_PARTITIONS = 256;
inline constexpr uint32_t MAX_SUBS = 64;

enum : uint16_t {
    TYPE_EMPTY = 0x0000,
    TYPE_MBR = 0x0001,
    TYPE_EXT2_SWAP = 0x0082,
    TYPE_EXT2 = 0x0083,
    TYPE_PFS = 0x0100,
    TYPE_HDL = 0x1337
};

static uint16_t rd16(const uint8_t* p, uint32_t off) {
    return (uint16_t)(p[off] | (p[off + 1] << 8));
}

static uint32_t rd32(const uint8_t* p, uint32_t off) {
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) | ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
}

const char* type_name(uint16_t type) {
    switch (type) {
        case TYPE_MBR: return "APA MBR";
        case TYPE_EXT2_SWAP: return "Linux swap";
        case TYPE_EXT2: return "ext2";
        case TYPE_PFS: return "PFS";
        case TYPE_HDL: return "HDLoader";
    }

    return "APA";
}

int scan(logger::Logger* logger, blk::Device* dev, std::vector <part::Partition>* out) {
    struct { logger::Logger* logger; size_t logger_id; } src = { logger, get_logger_id(logger) };

    out->clear();

    uint8_t header[HEADER_SIZE];

    if (blk::read(dev, 0, header, sizeof(header)) < 0)
        return 0;

    if (memcmp(header + 4, "APA\0", 4))
        return 0;

    uint64_t total = blk::get_size(dev);
    uint64_t lba = 0;

    std::vector <uint64_t> seen;

    for (uint32_t step = 0; step < MAX_PARTITIONS; step++) {
        if (lba * LBA + HEADER_SIZE > total) {
            iris_error(&src, "APA partition header at sector {} is past the end of the image", lba);

            break;
        }

        if (blk::read(dev, lba * LBA, header, sizeof(header)) < 0)
            break;

        if (memcmp(header + 4, "APA\0", 4)) {
            iris_error(&src, "APA chain broke at sector {}", lba);

            break;
        }

        uint64_t next = rd32(header, 0x08);
        uint64_t start = rd32(header, 0x40);
        uint64_t length = rd32(header, 0x44);
        uint16_t type = rd16(header, 0x48);
        uint32_t nsub = rd32(header, 0x4c);
        uint32_t main = rd32(header, 0x58);

        if (type != TYPE_EMPTY && !main && length) {
            part::Partition p;

            memcpy(p.name, header + 0x10, 32);

            p.name[32] = '\0';
            p.type = type;
            p.offset = start * LBA;
            p.size = length * LBA;

            snprintf(p.type_name, sizeof(p.type_name), "%s", type_name(type));

            part::Extent extent;

            extent.offset = p.offset;
            extent.size = p.size;

            p.extents.push_back(extent);

            if (nsub > MAX_SUBS) {
                iris_error(&src, "APA partition \"{}\" claims {} sub-partitions", p.name, nsub);

                nsub = MAX_SUBS;
            }

            for (uint32_t i = 0; i < nsub; i++) {
                part::Extent sub;

                sub.offset = (uint64_t)rd32(header, 0x100 + i * 8) * LBA;
                sub.size = (uint64_t)rd32(header, 0x104 + i * 8) * LBA;

                if (sub.size)
                    p.extents.push_back(sub);
            }

            out->push_back(p);
        }

        if (!next)
            break;

        bool looped = false;

        for (uint64_t visited : seen)
            looped |= visited == next;

        if (looped) {
            iris_error(&src, "APA chain loops back to sector {}", next);

            break;
        }

        seen.push_back(next);

        lba = next;
    }

    return (int)out->size();
}

}
