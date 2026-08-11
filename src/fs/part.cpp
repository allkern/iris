#include <cstdio>
#include <cstring>

#include "fs/apa.hpp"
#include "fs/part.hpp"
#include "fs/fs.hpp"

namespace iris::fs::part {

inline constexpr uint64_t LBA = 512;

inline constexpr uint32_t MAX_EXTENDED = 64;
inline constexpr uint32_t MAX_GPT_ENTRIES = 256;

static uint32_t rd32(const uint8_t* p, uint32_t off) {
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) | ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
}

static uint64_t rd64(const uint8_t* p, uint32_t off) {
    return (uint64_t)rd32(p, off) | ((uint64_t)rd32(p, off + 4) << 32);
}

static bool is_extended(uint8_t type) {
    return type == 0x05 || type == 0x0f || type == 0x85;
}

static bool add(blk::Device* dev, std::vector <Partition>* out, uint64_t start_lba, uint64_t sectors, uint8_t type) {
    if (!sectors)
        return false;

    uint64_t total = blk::get_size(dev);
    uint64_t offset = start_lba * LBA;

    if (offset >= total)
        return false;

    uint64_t size = sectors * LBA;

    if (size > total - offset)
        size = total - offset;

    Partition p;

    p.offset = offset;
    p.size = size;
    p.type = type;

    set_mbr_type_name(&p);

    out->push_back(p);

    return true;
}

static int scan_gpt(logger::Logger* logger, blk::Device* dev, std::vector <Partition>* out) {
    struct { logger::Logger* logger; size_t logger_id; } src = { logger, get_logger_id(logger) };

    uint8_t header[LBA];

    if (blk::read(dev, LBA, header, sizeof(header)) < 0)
        return 0;

    if (memcmp(header, "EFI PART", 8))
        return 0;

    uint64_t entry_lba = rd64(header, 0x48);
    uint32_t count = rd32(header, 0x50);
    uint32_t entry_size = rd32(header, 0x54);

    if (entry_size < 128 || entry_size > 4096) {
        iris_error(&src, "GPT entry size {} is out of range", entry_size);

        return 0;
    }

    if (count > MAX_GPT_ENTRIES)
        count = MAX_GPT_ENTRIES;

    std::vector <uint8_t> entry(entry_size);

    for (uint32_t i = 0; i < count; i++) {
        if (blk::read(dev, entry_lba * LBA + (uint64_t)i * entry_size, entry.data(), entry_size) < 0)
            break;

        bool unused = true;

        for (int b = 0; b < 16; b++)
            unused &= entry[b] == 0;

        if (unused)
            continue;

        uint64_t first = rd64(entry.data(), 0x20);
        uint64_t last = rd64(entry.data(), 0x28);

        if (last < first)
            continue;

        if (!add(dev, out, first, last - first + 1, 0))
            continue;

        uint16_t name[36];

        for (int c = 0; c < 36; c++)
            name[c] = (uint16_t)(entry[0x38 + c * 2] | (entry[0x38 + c * 2 + 1] << 8));

        int used = 0;

        while (used < 36 && name[used])
            used++;

        utf16_to_utf8(name, used, out->back().name, sizeof(out->back().name), nullptr);
    }

    return (int)out->size();
}

static void scan_extended(blk::Device* dev, std::vector <Partition>* out, uint64_t base) {
    uint64_t current = base;

    for (uint32_t step = 0; step < MAX_EXTENDED && current; step++) {
        uint8_t sector[LBA];

        if (blk::read(dev, current * LBA, sector, sizeof(sector)) < 0)
            return;

        if (sector[0x1fe] != 0x55 || sector[0x1ff] != 0xaa)
            return;

        uint64_t next = 0;

        for (int i = 0; i < 2; i++) {
            const uint8_t* e = sector + 0x1be + i * 16;

            uint8_t type = e[4];
            uint64_t start = rd32(e, 8);
            uint64_t sectors = rd32(e, 12);

            if (!type || !sectors)
                continue;

            if (is_extended(type)) {
                next = base + start;
            } else {
                add(dev, out, current + start, sectors, type);
            }
        }

        current = next;
    }
}

int scan(logger::Logger* logger, blk::Device* dev, std::vector <Partition>* out) {
    out->clear();

    if (!dev || blk::get_size(dev) < 2 * LBA)
        return 0;

    uint8_t sector[LBA];

    if (blk::read(dev, 0, sector, sizeof(sector)) < 0)
        return 0;

    if (sector[0x1fe] != 0x55 || sector[0x1ff] != 0xaa)
        return apa::scan(logger, dev, out);

    bool protective = false;

    for (int i = 0; i < 4; i++) {
        if (sector[0x1be + i * 16 + 4] == 0xee)
            protective = true;
    }

    if (protective)
        return scan_gpt(logger, dev, out);

    for (int i = 0; i < 4; i++) {
        const uint8_t* e = sector + 0x1be + i * 16;

        uint8_t type = e[4];
        uint64_t start = rd32(e, 8);
        uint64_t sectors = rd32(e, 12);

        if (!type || !sectors)
            continue;

        if (is_extended(type)) {
            scan_extended(dev, out, start);
        } else {
            add(dev, out, start, sectors, type);
        }
    }

    return (int)out->size();
}

void set_mbr_type_name(Partition* partition) {
    const char* name = "";

    switch (partition->type) {
        case 0x01: name = "FAT12"; break;
        case 0x04: case 0x06: case 0x0e: name = "FAT16"; break;
        case 0x07: name = "exFAT/NTFS"; break;
        case 0x0b: case 0x0c: name = "FAT32"; break;
        case 0x83: name = "Linux"; break;
        case 0xef: name = "EFI System"; break;
    }

    snprintf(partition->type_name, sizeof(partition->type_name), "%s", name);
}

}
