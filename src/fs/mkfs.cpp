#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

#include "fs/mkfs.hpp"

namespace iris::fs::mkfs {

#ifdef _MSC_VER
#define mkfs_fseek64 _fseeki64
#define mkfs_ftell64 _ftelli64
#elif defined(_WIN32)
#define mkfs_fseek64 fseeko64
#define mkfs_ftell64 ftello64
#else
#define mkfs_fseek64 fseek
#define mkfs_ftell64 ftell
#endif

inline constexpr uint32_t SECTOR = 512;
inline constexpr uint64_t CHUNK = 1024 * 1024;
inline constexpr uint64_t PART_LBA = 2048;

struct Image {
    FILE* file = nullptr;

    uint64_t base = 0;
    uint64_t size = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

static void wr16(uint8_t* p, uint32_t off, uint16_t v) {
    p[off] = (uint8_t)v;
    p[off + 1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t* p, uint32_t off, uint32_t v) {
    p[off] = (uint8_t)v;
    p[off + 1] = (uint8_t)(v >> 8);
    p[off + 2] = (uint8_t)(v >> 16);
    p[off + 3] = (uint8_t)(v >> 24);
}

static void wr64(uint8_t* p, uint32_t off, uint64_t v) {
    wr32(p, off, (uint32_t)v);
    wr32(p, off + 4, (uint32_t)(v >> 32));
}

static uint64_t align_up(uint64_t value, uint64_t to) {
    return ((value + to - 1) / to) * to;
}

static uint64_t div_up(uint64_t value, uint64_t by) {
    return (value + by - 1) / by;
}

static bool put(Image* img, uint64_t offset, const void* buf, uint64_t size) {
    if (offset > img->size || size > img->size - offset) {
        iris_error(img, "A {} byte write at {} would run past the end of the image", size, offset);

        return false;
    }

    if (mkfs_fseek64(img->file, (int64_t)(img->base + offset), SEEK_SET))
        return false;

    return fwrite(buf, 1, (size_t)size, img->file) == size;
}

static bool fill(Image* img, uint64_t offset, uint8_t byte, uint64_t size) {
    std::vector <uint8_t> buf((size_t)(size < CHUNK ? size : CHUNK), byte);

    while (size) {
        uint64_t chunk = size < CHUNK ? size : CHUNK;

        if (!put(img, offset, buf.data(), chunk))
            return false;

        offset += chunk;
        size -= chunk;
    }

    return true;
}

inline constexpr uint32_t MCD_PAGE = PAGE_SIZE;
inline constexpr uint32_t MCD_ECC = PAGE_ECC_SIZE;
inline constexpr uint32_t MCD_PAGES_PER_CLUSTER = 2;
inline constexpr uint32_t MCD_PAGES_PER_BLOCK = 16;
inline constexpr uint32_t MCD_CLUSTER = MCD_PAGE * MCD_PAGES_PER_CLUSTER;
inline constexpr uint32_t MCD_DIRENT = 512;

enum : uint16_t {
    DF_READ = 0x0001,
    DF_WRITE = 0x0002,
    DF_EXECUTE = 0x0004,
    DF_DIRECTORY = 0x0020,
    DF_0400 = 0x0400,
    DF_HIDDEN = 0x2000,
    DF_EXISTS = 0x8000
};

struct Ecc {
    uint8_t column[256] = {};
    uint8_t parity[256] = {};
};

static void ecc_init(Ecc* ecc) {
    static const uint8_t masks[8] = { 0x07, 0x16, 0x25, 0x34, 0x43, 0x52, 0x61, 0x70 };

    for (int b = 0; b < 256; b++) {
        for (int i = 0; i < 8; i++) {
            if (!(b & (1 << i)))
                continue;

            ecc->column[b] ^= masks[i];
            ecc->parity[b] ^= 1;
        }
    }
}

static void ecc_chunk(const Ecc* ecc, const uint8_t* data, uint8_t* out) {
    uint8_t column = 0x77;
    uint8_t line0 = 0x7f;
    uint8_t line1 = 0x7f;

    for (int i = 0; i < 128; i++) {
        column ^= ecc->column[data[i]];

        if (ecc->parity[data[i]]) {
            line0 ^= (uint8_t)~i;
            line1 ^= (uint8_t)i;
        }
    }

    out[0] = column;
    out[1] = line0 & 0x7f;
    out[2] = line1;
}

void page_ecc(const uint8_t* page, uint8_t* out) {
    static Ecc ecc;
    static bool ready = false;

    if (!ready) {
        ecc_init(&ecc);

        ready = true;
    }

    memset(out, 0, MCD_ECC);

    for (uint32_t chunk = 0; chunk < MCD_PAGE / 128; chunk++)
        ecc_chunk(&ecc, page + chunk * 128, out + chunk * 3);
}

struct Mcd {
    Image* img = nullptr;
    Ecc ecc;

    uint32_t ecc_size = MCD_ECC;
    uint32_t clusters_per_card = 0;
    uint32_t clusters_per_block = 0;
    uint32_t entries_per_cluster = 0;
    uint32_t ifc_start = 0;
    uint32_t ifc_count = 0;
    uint32_t fat_start = 0;
    uint32_t fat_count = 0;
    uint32_t alloc_offset = 0;
    uint32_t alloc_end = 0;
};

static bool mcd_write_cluster(Mcd* mcd, uint32_t cluster, const uint8_t* data) {
    uint8_t page[MCD_PAGE + MCD_ECC];

    for (uint32_t i = 0; i < MCD_PAGES_PER_CLUSTER; i++) {
        const uint8_t* src = data + i * MCD_PAGE;

        memcpy(page, src, MCD_PAGE);

        if (mcd->ecc_size) {
            memset(page + MCD_PAGE, 0, mcd->ecc_size);

            for (uint32_t chunk = 0; chunk < MCD_PAGE / 128; chunk++)
                ecc_chunk(&mcd->ecc, src + chunk * 128, page + MCD_PAGE + chunk * 3);
        }

        uint64_t offset = (uint64_t)(cluster * MCD_PAGES_PER_CLUSTER + i) * (MCD_PAGE + mcd->ecc_size);

        if (!put(mcd->img, offset, page, MCD_PAGE + mcd->ecc_size))
            return false;
    }

    return true;
}

static bool mcd_layout(Mcd* mcd, uint32_t pages) {
    mcd->clusters_per_card = pages / MCD_PAGES_PER_CLUSTER;
    mcd->clusters_per_block = MCD_PAGES_PER_BLOCK / MCD_PAGES_PER_CLUSTER;
    mcd->entries_per_cluster = MCD_CLUSTER / 4;

    uint32_t per_block = mcd->clusters_per_block;

    if (mcd->clusters_per_card < 4 * per_block)
        return false;

    uint32_t usable = mcd->clusters_per_card - 2 * per_block;
    uint32_t alloc = usable - per_block;
    uint32_t fat = 0;
    uint32_t ifc = 0;

    for (int i = 0; i < 8; i++) {
        fat = (uint32_t)div_up(alloc, mcd->entries_per_cluster);
        ifc = (uint32_t)div_up(fat, mcd->entries_per_cluster);

        uint32_t next = usable - per_block - fat - ifc;

        if (next == alloc)
            break;

        alloc = next;
    }

    mcd->ifc_start = per_block;
    mcd->ifc_count = ifc;
    mcd->fat_start = per_block + ifc;
    mcd->fat_count = fat;
    mcd->alloc_offset = per_block + ifc + fat;
    mcd->alloc_end = usable - mcd->alloc_offset;

    return ifc && ifc <= 32 && mcd->alloc_end && mcd->alloc_offset < usable;
}

static void mcd_time(uint8_t* p, uint32_t off) {
    time_t now = time(nullptr);
    struct tm* t = gmtime(&now);

    if (!t)
        return;

    p[off + 1] = (uint8_t)t->tm_sec;
    p[off + 2] = (uint8_t)t->tm_min;
    p[off + 3] = (uint8_t)t->tm_hour;
    p[off + 4] = (uint8_t)t->tm_mday;
    p[off + 5] = (uint8_t)(t->tm_mon + 1);

    wr16(p, off + 6, (uint16_t)(t->tm_year + 1900));
}

static void mcd_dirent(uint8_t* p, uint16_t mode, uint32_t length, const char* name) {
    wr16(p, 0x00, mode);
    wr32(p, 0x04, length);

    mcd_time(p, 0x08);
    mcd_time(p, 0x18);

    strcpy((char*)p + 0x40, name);
}

static int mcd_format(Image* img, uint32_t pages, bool ecc) {
    Mcd mcd;

    mcd.img = img;
    mcd.ecc_size = ecc ? MCD_ECC : 0;

    ecc_init(&mcd.ecc);

    if (!mcd_layout(&mcd, pages)) {
        iris_error(img, "{} pages is not a usable PS2 memory card layout", pages);

        return FS_ERR_UNSUPPORTED;
    }

    if (!fill(img, 0, 0xff, img->size))
        return FS_ERR_IO;

    std::vector <uint8_t> cluster(MCD_CLUSTER);

    memset(cluster.data(), 0xff, cluster.size());
    memset(cluster.data(), 0, MCD_PAGE);

    memcpy(cluster.data(), "Sony PS2 Memory Card Format ", 28);
    memcpy(cluster.data() + 0x1c, "1.2.0.0", 7);

    wr16(cluster.data(), 0x28, MCD_PAGE);
    wr16(cluster.data(), 0x2a, MCD_PAGES_PER_CLUSTER);
    wr16(cluster.data(), 0x2c, MCD_PAGES_PER_BLOCK);
    wr16(cluster.data(), 0x2e, 0xff00);
    wr32(cluster.data(), 0x30, mcd.clusters_per_card);
    wr32(cluster.data(), 0x34, mcd.alloc_offset);
    wr32(cluster.data(), 0x38, mcd.alloc_end);
    wr32(cluster.data(), 0x3c, 0);
    wr32(cluster.data(), 0x40, mcd.clusters_per_card / mcd.clusters_per_block - 1);
    wr32(cluster.data(), 0x44, mcd.clusters_per_card / mcd.clusters_per_block - 2);

    for (uint32_t i = 0; i < mcd.ifc_count; i++)
        wr32(cluster.data(), 0x50 + i * 4, mcd.ifc_start + i);

    for (uint32_t i = 0; i < 32; i++)
        wr32(cluster.data(), 0xd0 + i * 4, 0xffffffff);

    cluster[0x150] = 2;
    cluster[0x151] = 0x2b;

    memset(cluster.data() + 0x152, 0xff, MCD_PAGE - 0x152);

    if (!mcd_write_cluster(&mcd, 0, cluster.data()))
        return FS_ERR_IO;

    for (uint32_t i = 0; i < mcd.ifc_count; i++) {
        memset(cluster.data(), 0xff, cluster.size());

        for (uint32_t j = 0; j < mcd.entries_per_cluster; j++) {
            uint32_t index = i * mcd.entries_per_cluster + j;

            if (index < mcd.fat_count)
                wr32(cluster.data(), j * 4, mcd.fat_start + index);
        }

        if (!mcd_write_cluster(&mcd, mcd.ifc_start + i, cluster.data()))
            return FS_ERR_IO;
    }

    for (uint32_t i = 0; i < mcd.fat_count; i++) {
        for (uint32_t j = 0; j < mcd.entries_per_cluster; j++) {
            uint32_t index = i * mcd.entries_per_cluster + j;

            wr32(cluster.data(), j * 4, index && index < mcd.alloc_end ? 0x7fffffff : 0xffffffff);
        }

        if (!mcd_write_cluster(&mcd, mcd.fat_start + i, cluster.data()))
            return FS_ERR_IO;
    }

    memset(cluster.data(), 0, cluster.size());

    mcd_dirent(cluster.data(), DF_EXISTS | DF_0400 | DF_DIRECTORY | DF_READ | DF_WRITE | DF_EXECUTE, 2, ".");
    mcd_dirent(cluster.data() + MCD_DIRENT, DF_EXISTS | DF_HIDDEN | DF_0400 | DF_DIRECTORY | DF_WRITE | DF_EXECUTE, 0, "..");

    if (!mcd_write_cluster(&mcd, mcd.alloc_offset, cluster.data()))
        return FS_ERR_IO;

    iris_debug(img, "Formatted a PS2 memory card, {} clusters, {} allocatable, ECC {}",
        mcd.clusters_per_card, mcd.alloc_end, ecc ? "on" : "off");

    return FS_OK;
}

inline constexpr uint32_t PS1_FRAME = 128;
inline constexpr uint32_t PS1_BLOCK = 8192;
inline constexpr uint32_t PS1_BLOCKS = 16;

static void ps1_checksum(uint8_t* frame) {
    uint8_t sum = 0;

    for (uint32_t i = 0; i < PS1_FRAME - 1; i++)
        sum ^= frame[i];

    frame[PS1_FRAME - 1] = sum;
}

static int ps1_format(Image* img) {
    if (img->size < PS1_BLOCK * PS1_BLOCKS) {
        iris_error(img, "A PS1 memory card image needs at least {} bytes", PS1_BLOCK * PS1_BLOCKS);

        return FS_ERR_UNSUPPORTED;
    }

    if (!fill(img, 0, 0, img->size))
        return FS_ERR_IO;

    std::vector <uint8_t> block(PS1_BLOCK, 0);

    uint8_t* frame = block.data();

    frame[0] = 'M';
    frame[1] = 'C';

    ps1_checksum(frame);

    for (uint32_t i = 1; i < PS1_BLOCKS; i++) {
        frame = block.data() + i * PS1_FRAME;

        wr32(frame, 0x00, 0xa0);
        wr16(frame, 0x08, 0xffff);

        ps1_checksum(frame);
    }

    for (uint32_t i = 16; i < 36; i++) {
        frame = block.data() + i * PS1_FRAME;

        wr32(frame, 0x00, 0xffffffff);

        ps1_checksum(frame);
    }

    memset(block.data() + 36 * PS1_FRAME, 0xff, (63 - 36) * PS1_FRAME);

    frame = block.data() + 63 * PS1_FRAME;

    frame[0] = 'M';
    frame[1] = 'C';

    ps1_checksum(frame);

    if (!put(img, 0, block.data(), PS1_BLOCK))
        return FS_ERR_IO;

    iris_debug(img, "Formatted a PS1 memory card");

    return FS_OK;
}

inline constexpr uint32_t FAT_RESERVED = 32;
inline constexpr uint32_t FAT_MIN_CLUSTERS = 65525;

static uint32_t fat_sectors_per_cluster(uint64_t bytes) {
    if (bytes <= 260ull * 1024 * 1024) return 1;
    if (bytes <= 8ull * 1024 * 1024 * 1024) return 8;
    if (bytes <= 16ull * 1024 * 1024 * 1024) return 16;
    if (bytes <= 32ull * 1024 * 1024 * 1024) return 32;

    return 64;
}

static void fat_label(uint8_t* out, const char* label) {
    memset(out, ' ', 11);

    for (uint32_t i = 0; i < 11 && label[i]; i++)
        out[i] = (uint8_t)toupper((unsigned char)label[i]);
}

static int fat32_format(Image* img, const char* label, uint64_t hidden) {
    uint64_t total = img->size / SECTOR;

    if (total > 0xffffffffull)
        total = 0xffffffffull;

    uint32_t spc = fat_sectors_per_cluster(img->size);
    uint32_t fat_size = 1;
    uint64_t clusters = 0;

    for (int i = 0; i < 64; i++) {
        uint64_t reserved = FAT_RESERVED + (uint64_t)2 * fat_size;

        if (total <= reserved)
            break;

        clusters = (total - reserved) / spc;

        uint32_t need = (uint32_t)div_up((clusters + 2) * 4, SECTOR);

        if (need <= fat_size)
            break;

        fat_size = need;
    }

    if (clusters < FAT_MIN_CLUSTERS || clusters > 0x0ffffff0 ||
        div_up((clusters + 2) * 4, SECTOR) > fat_size) {
        iris_error(img, "A FAT32 volume needs at least {} clusters and this image only fits {}",
            FAT_MIN_CLUSTERS, clusters);

        return FS_ERR_UNSUPPORTED;
    }

    uint64_t fat_start = (uint64_t)FAT_RESERVED * SECTOR;
    uint64_t data_start = fat_start + (uint64_t)2 * fat_size * SECTOR;

    if (!fill(img, 0, 0, data_start + (uint64_t)spc * SECTOR))
        return FS_ERR_IO;

    uint8_t boot[SECTOR] = {};

    boot[0] = 0xeb;
    boot[1] = 0x58;
    boot[2] = 0x90;

    memcpy(boot + 0x03, "MSWIN4.1", 8);

    wr16(boot, 0x0b, SECTOR);
    boot[0x0d] = (uint8_t)spc;
    wr16(boot, 0x0e, FAT_RESERVED);
    boot[0x10] = 2;
    boot[0x15] = 0xf8;
    wr16(boot, 0x18, 63);
    wr16(boot, 0x1a, 255);
    wr32(boot, 0x1c, (uint32_t)hidden);
    wr32(boot, 0x20, (uint32_t)total);
    wr32(boot, 0x24, fat_size);
    wr32(boot, 0x2c, 2);
    wr16(boot, 0x30, 1);
    wr16(boot, 0x32, 6);
    boot[0x40] = 0x80;
    boot[0x42] = 0x29;
    wr32(boot, 0x43, (uint32_t)time(nullptr));

    fat_label(boot + 0x47, label);

    memcpy(boot + 0x52, "FAT32   ", 8);

    boot[0x1fe] = 0x55;
    boot[0x1ff] = 0xaa;

    uint8_t fsinfo[SECTOR] = {};

    wr32(fsinfo, 0x000, 0x41615252);
    wr32(fsinfo, 0x1e4, 0x61417272);
    wr32(fsinfo, 0x1e8, (uint32_t)(clusters - 1));
    wr32(fsinfo, 0x1ec, 2);
    wr32(fsinfo, 0x1fc, 0xaa550000);

    if (!put(img, 0, boot, SECTOR) || !put(img, 6 * SECTOR, boot, SECTOR))
        return FS_ERR_IO;

    if (!put(img, SECTOR, fsinfo, SECTOR) || !put(img, 7 * SECTOR, fsinfo, SECTOR))
        return FS_ERR_IO;

    uint8_t head[SECTOR] = {};

    wr32(head, 0, 0x0ffffff8);
    wr32(head, 4, 0x0fffffff);
    wr32(head, 8, 0x0fffffff);

    for (int copy = 0; copy < 2; copy++) {
        if (!put(img, fat_start + (uint64_t)copy * fat_size * SECTOR, head, SECTOR))
            return FS_ERR_IO;
    }

    if (label[0]) {
        uint8_t entry[32] = {};

        fat_label(entry, label);

        entry[0x0b] = 0x08;

        if (!put(img, data_start, entry, sizeof(entry)))
            return FS_ERR_IO;
    }

    iris_debug(img, "Formatted a FAT32 volume, {} clusters of {} bytes", clusters, spc * SECTOR);

    return FS_OK;
}

inline constexpr uint32_t EXFAT_BOOT_SECTORS = 12;
inline constexpr uint32_t EXFAT_FAT_OFFSET = 128;
inline constexpr uint32_t EXFAT_UPCASE_BYTES = 65536 * 2;

static uint32_t exfat_cluster_shift(uint64_t bytes) {
    if (bytes <= 256ull * 1024 * 1024) return 3;
    if (bytes <= 32ull * 1024 * 1024 * 1024) return 6;

    return 8;
}

static uint32_t exfat_checksum(uint32_t sum, const uint8_t* data, uint64_t size, bool boot) {
    for (uint64_t i = 0; i < size; i++) {
        if (boot && (i == 106 || i == 107 || i == 112))
            continue;

        sum = ((sum << 31) | (sum >> 1)) + data[i];
    }

    return sum;
}

static void exfat_upcase(uint8_t* out) {
    for (uint32_t c = 0; c < 65536; c++)
        wr16(out, c * 2, (uint16_t)c);

    for (uint32_t c = 0x61; c <= 0x7a; c++) wr16(out, c * 2, (uint16_t)(c - 0x20));
    for (uint32_t c = 0xe0; c <= 0xfe; c++) wr16(out, c * 2, (uint16_t)(c - 0x20));
    for (uint32_t c = 0x3b1; c <= 0x3c9; c++) wr16(out, c * 2, (uint16_t)(c - 0x20));
    for (uint32_t c = 0x430; c <= 0x44f; c++) wr16(out, c * 2, (uint16_t)(c - 0x20));
    for (uint32_t c = 0x450; c <= 0x45f; c++) wr16(out, c * 2, (uint16_t)(c - 0x50));

    wr16(out, 0xf7 * 2, 0xf7);
    wr16(out, 0xff * 2, 0x178);
    wr16(out, 0x3c2 * 2, 0x3a3);
}

static int exfat_format(Image* img, const char* label, uint64_t partition_lba) {
    uint32_t cluster_shift = exfat_cluster_shift(img->size);
    uint32_t spc = 1u << cluster_shift;
    uint32_t cluster_size = spc * SECTOR;

    uint64_t total = img->size / SECTOR;
    uint64_t clusters = total > EXFAT_FAT_OFFSET ? (total - EXFAT_FAT_OFFSET) / spc : 0;
    uint64_t fat_length = 0;
    uint64_t heap = 0;

    for (int i = 0; i < 8 && clusters; i++) {
        fat_length = align_up(div_up((clusters + 2) * 4, SECTOR), spc);
        heap = align_up(EXFAT_FAT_OFFSET + fat_length, spc);

        uint64_t next = heap < total ? (total - heap) / spc : 0;

        if (next == clusters)
            break;

        clusters = next;
    }

    uint64_t bitmap_bytes = div_up(clusters, 8);
    uint64_t bitmap_clusters = div_up(bitmap_bytes, cluster_size);
    uint64_t upcase_clusters = div_up(EXFAT_UPCASE_BYTES, cluster_size);
    uint64_t used = bitmap_clusters + upcase_clusters + 1;

    if (!clusters || clusters > 0xfffffff5ull || used >= clusters || heap + clusters * spc > total) {
        iris_error(img, "This image is too small to hold an exFAT volume");

        return FS_ERR_UNSUPPORTED;
    }

    uint32_t bitmap_cluster = 2;
    uint32_t upcase_cluster = (uint32_t)(bitmap_cluster + bitmap_clusters);
    uint32_t root_cluster = (uint32_t)(upcase_cluster + upcase_clusters);

    if (!fill(img, 0, 0, heap * SECTOR + used * cluster_size))
        return FS_ERR_IO;

    std::vector <uint8_t> boot(EXFAT_BOOT_SECTORS * SECTOR, 0);

    uint8_t* sec = boot.data();

    sec[0] = 0xeb;
    sec[1] = 0x76;
    sec[2] = 0x90;

    memcpy(sec + 0x03, "EXFAT   ", 8);

    wr64(sec, 0x40, partition_lba);
    wr64(sec, 0x48, total);
    wr32(sec, 0x50, EXFAT_FAT_OFFSET);
    wr32(sec, 0x54, (uint32_t)fat_length);
    wr32(sec, 0x58, (uint32_t)heap);
    wr32(sec, 0x5c, (uint32_t)clusters);
    wr32(sec, 0x60, root_cluster);
    wr32(sec, 0x64, (uint32_t)time(nullptr));
    wr16(sec, 0x68, 0x0100);
    sec[0x6c] = 9;
    sec[0x6d] = (uint8_t)cluster_shift;
    sec[0x6e] = 1;
    sec[0x6f] = 0x80;
    sec[0x70] = (uint8_t)(used * 100 / clusters);
    sec[0x1fe] = 0x55;
    sec[0x1ff] = 0xaa;

    for (uint32_t i = 1; i <= 8; i++)
        wr32(sec + i * SECTOR, SECTOR - 4, 0xaa550000);

    uint32_t sum = exfat_checksum(0, boot.data(), 11 * SECTOR, true);

    for (uint32_t i = 0; i < SECTOR / 4; i++)
        wr32(sec + 11 * SECTOR, i * 4, sum);

    if (!put(img, 0, boot.data(), boot.size()))
        return FS_ERR_IO;

    if (!put(img, (uint64_t)EXFAT_BOOT_SECTORS * SECTOR, boot.data(), boot.size()))
        return FS_ERR_IO;

    std::vector <uint8_t> fat((size_t)align_up((used + 2) * 4, SECTOR), 0);

    wr32(fat.data(), 0, 0xfffffff8);
    wr32(fat.data(), 4, 0xffffffff);

    for (uint32_t i = 0; i < bitmap_clusters; i++)
        wr32(fat.data(), (bitmap_cluster + i) * 4, i + 1 == bitmap_clusters ? 0xffffffff : bitmap_cluster + i + 1);

    for (uint32_t i = 0; i < upcase_clusters; i++)
        wr32(fat.data(), (upcase_cluster + i) * 4, i + 1 == upcase_clusters ? 0xffffffff : upcase_cluster + i + 1);

    wr32(fat.data(), root_cluster * 4, 0xffffffff);

    if (!put(img, (uint64_t)EXFAT_FAT_OFFSET * SECTOR, fat.data(), fat.size()))
        return FS_ERR_IO;

    std::vector <uint8_t> bitmap((size_t)(bitmap_clusters * cluster_size), 0);

    for (uint64_t i = 0; i < used; i++)
        bitmap[i / 8] |= (uint8_t)(1 << (i % 8));

    if (!put(img, heap * SECTOR, bitmap.data(), bitmap.size()))
        return FS_ERR_IO;

    std::vector <uint8_t> upcase((size_t)(upcase_clusters * cluster_size), 0);

    exfat_upcase(upcase.data());

    uint32_t upcase_sum = exfat_checksum(0, upcase.data(), EXFAT_UPCASE_BYTES, false);

    if (!put(img, heap * SECTOR + (uint64_t)(upcase_cluster - 2) * cluster_size, upcase.data(), upcase.size()))
        return FS_ERR_IO;

    std::vector <uint8_t> root(cluster_size, 0);

    uint8_t* entry = root.data();

    entry[0] = 0x83;

    for (uint32_t i = 0; i < 11 && label[i]; i++) {
        wr16(entry, 2 + i * 2, (uint16_t)(unsigned char)label[i]);

        entry[1] = (uint8_t)(i + 1);
    }

    entry += 32;

    entry[0] = 0x81;
    wr32(entry, 0x14, bitmap_cluster);
    wr64(entry, 0x18, bitmap_bytes);

    entry += 32;

    entry[0] = 0x82;
    wr32(entry, 0x04, upcase_sum);
    wr32(entry, 0x14, upcase_cluster);
    wr64(entry, 0x18, EXFAT_UPCASE_BYTES);

    if (!put(img, heap * SECTOR + (uint64_t)(root_cluster - 2) * cluster_size, root.data(), root.size()))
        return FS_ERR_IO;

    iris_debug(img, "Formatted an exFAT volume, {} clusters of {} bytes", clusters, cluster_size);

    return FS_OK;
}

static void mbr_chs(uint8_t* out, uint64_t lba) {
    uint64_t cylinder = lba / (255 * 63);
    uint64_t head = (lba / 63) % 255;
    uint64_t sector = (lba % 63) + 1;

    if (cylinder > 1023) {
        cylinder = 1023;
        head = 254;
        sector = 63;
    }

    out[0] = (uint8_t)head;
    out[1] = (uint8_t)(sector | ((cylinder >> 2) & 0xc0));
    out[2] = (uint8_t)cylinder;
}

static int write_mbr(Image* img, uint64_t start, uint64_t sectors, uint8_t type) {
    uint8_t sector[SECTOR] = {};

    wr32(sector, 0x1b8, (uint32_t)time(nullptr));

    uint8_t* entry = sector + 0x1be;

    mbr_chs(entry + 1, start);

    entry[4] = type;

    mbr_chs(entry + 5, start + sectors - 1);

    wr32(entry, 8, (uint32_t)start);
    wr32(entry, 12, (uint32_t)sectors);

    sector[0x1fe] = 0x55;
    sector[0x1ff] = 0xaa;

    return put(img, 0, sector, SECTOR) ? FS_OK : FS_ERR_IO;
}

int create_image(logger::Logger* logger, const char* path, uint64_t size, uint8_t fill_byte) {
    struct { logger::Logger* logger; size_t logger_id; } src = { logger, get_logger_id(logger) };

    FILE* file = fopen(path, "wb");

    if (!file) {
        iris_error(&src, "Could not create \"{}\"", path);

        return FS_ERR_IO;
    }

    fclose(file);

    std::error_code ec;

    std::filesystem::resize_file(std::filesystem::path(path), size, ec);

    if (ec) {
        iris_error(&src, "Could not resize \"{}\" to {} bytes: {}", path, size, ec.message());

        return FS_ERR_IO;
    }

    if (!fill_byte)
        return FS_OK;

    Image img;

    img.file = fopen(path, "r+b");
    img.size = size;
    img.logger = logger;
    img.logger_id = src.logger_id;

    if (!img.file) {
        iris_error(&src, "Could not open \"{}\"", path);

        return FS_ERR_IO;
    }

    int r = fill(&img, 0, fill_byte, size) ? FS_OK : FS_ERR_IO;

    fclose(img.file);

    return r;
}

int format(logger::Logger* logger, const char* path, const Params& params) {
    struct { logger::Logger* logger; size_t logger_id; } src = { logger, get_logger_id(logger) };

    bool card = params.type == MKFS_PS2_MCD || params.type == MKFS_PS1_MCD;

    if (params.size) {
        int r = create_image(logger, path, params.size, 0);

        if (r < 0)
            return r;
    }

    Image img;

    img.file = fopen(path, "r+b");
    img.logger = logger;
    img.logger_id = src.logger_id;

    if (!img.file) {
        iris_error(&src, "Could not open \"{}\" for writing", path);

        return FS_ERR_IO;
    }

    mkfs_fseek64(img.file, 0, SEEK_END);

    uint64_t total = (uint64_t)mkfs_ftell64(img.file);

    img.size = total;

    int r = FS_ERR_UNSUPPORTED;

    if (params.partition && !card) {
        if (total <= (PART_LBA + 1) * SECTOR) {
            iris_error(&src, "\"{}\" is too small to hold a partition table", path);

            fclose(img.file);

            return FS_ERR_UNSUPPORTED;
        }

        uint64_t sectors = total / SECTOR - PART_LBA;

        r = write_mbr(&img, PART_LBA, sectors, params.type == MKFS_EXFAT ? 0x07 : 0x0c);

        if (r < 0) {
            fclose(img.file);

            return r;
        }

        img.base = PART_LBA * SECTOR;
        img.size = sectors * SECTOR;
    }

    switch (params.type) {
        case MKFS_PS2_MCD: {
            bool ecc = !(total % (MCD_PAGE + MCD_ECC));

            if (!ecc && (total % MCD_PAGE)) {
                iris_error(&src, "{} bytes is not a whole number of memory card pages", total);

                break;
            }

            r = mcd_format(&img, (uint32_t)(total / (ecc ? MCD_PAGE + MCD_ECC : MCD_PAGE)), ecc);
        } break;

        case MKFS_PS1_MCD: {
            r = ps1_format(&img);
        } break;

        case MKFS_FAT32: {
            r = fat32_format(&img, params.label, img.base / SECTOR);
        } break;

        case MKFS_EXFAT: {
            r = exfat_format(&img, params.label, img.base / SECTOR);
        } break;
    }

    fclose(img.file);

    return r;
}

}
