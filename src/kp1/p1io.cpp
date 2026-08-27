#include <cstdio>
#include <cstring>

#include <fmt/format.h>

#include "p1io.hpp"
#include "iop/fw.hpp"
#include "shared/ata/disc.hpp"
#include "fs/blk.hpp"
#include "fs/part.hpp"

namespace iris::kp1::p1io {

using iris::fw::device::Device;

static int load_blob(P1io* p1io, const char* path, uint8_t* buf, uint32_t size, const char* what) {
    if (!path || !path[0])
        return 0;

    FILE* file = fopen(path, "rb");

    if (!file) {
        iris_error(p1io, "Failed to open {} \"{}\"", what, path);

        return 0;
    }

    size_t read = fread(buf, 1, size, file);

    fclose(file);

    if (read != size) {
        iris_error(p1io, "{} \"{}\" is {} bytes, expected {}", what, path, read, size);

        return 0;
    }

    return 1;
}

static const uint32_t default_config_rom[] = {
    0x0404a1a4, 0x31333934, 0x407d8002, 0x00000000, 0x00000000, 0x00053f04,
    0x03000679, 0x8100000a, 0x0c0083c0, 0xc3000005, 0xd1000001, 0x00020901,
    0x12000679, 0x13001000, 0x000231e6, 0x17000000, 0x81000006, 0x0004f2d6,
    0x00000000, 0x00000000, 0x4b4f4e41, 0x4d490000, 0x000428a6, 0x00000000,
    0x00000000, 0x5053322d, 0x41430000
};

static const int default_config_rom_size = sizeof(default_config_rom) / sizeof(default_config_rom[0]);

static int read_default_config_rom(uint32_t start, uint8_t* buf, int len) {
    if (start & 3)
        return fw::device::RESP_ADDRESS_ERROR;

    for (int i = 0; i < len; i += 4) {
        int index = (int)((start + i) >> 2);

        if (index >= default_config_rom_size)
            return fw::device::RESP_ADDRESS_ERROR;

        uint32_t value = default_config_rom[index];

        buf[i + 0] = (uint8_t)(value >> 24);
        buf[i + 1] = (uint8_t)(value >> 16);
        buf[i + 2] = (uint8_t)(value >> 8);
        buf[i + 3] = (uint8_t)value;
    }

    return fw::device::RESP_COMPLETE;
}

P1io* from_device(Device* dev) {
    return (P1io*)dev->priv;
}

void set_io_mode(P1io* p1io, int mode) {
    p1io->io_mode = mode;

    iris_info(p1io, "I/O mode {}", mode);
}

static const uint8_t P1IO_DEFAULT_DALLAS_ID[6] = { 0x5f, 0x04, 0x00, 0x01, 0x00, 0x01 };
static const uint8_t P1IO_FACTORY_MAC[6] = { 0x00, 0x04, 0x5f, 0x00, 0x00, 0x01 };

static uint8_t fsci_hex_digit(uint8_t value) {
    return value < 10 ? (uint8_t)('0' + value) : (uint8_t)('A' + value - 10);
}

static void write_fsci_hex_byte(uint8_t* dest, uint8_t value) {
    dest[0] = fsci_hex_digit(value >> 4);
    dest[1] = fsci_hex_digit(value & 0x0f);
}

static uint8_t fsci_checksum(const uint8_t* data, uint32_t size) {
    uint32_t checksum = 0;

    for (uint32_t i = 0; i < size; i++) {
        checksum += data[i];
    }

    while (checksum > 0xff) {
        checksum = (checksum & 0xff) + (checksum >> 8);
    }

    return (uint8_t)checksum;
}

static void build_fsci_mac_frames(P1io* p1io) {
    for (uint32_t frame = 0; frame < P1IO_FSCI_MAC_FRAMES; frame++) {
        uint8_t* dest = p1io->fsci_mac_stream + frame * P1IO_FSCI_MAC_FRAME_SIZE;

        dest[0] = '@';

        write_fsci_hex_byte(dest + 1, 0x0d);

        dest[3] = 'M';

        for (uint32_t i = 0; i < 6; i++) {
            write_fsci_hex_byte(dest + 4 + i * 2, p1io->factory_mac[i]);
        }

        write_fsci_hex_byte(dest + 16, fsci_checksum(dest + 3, 0x0d));
    }
}

static int bootrom_mac_backup_valid(const uint8_t* record) {
    int all_zero = 1;
    int all_ff = 1;

    for (uint32_t i = 0; i < 6; i++) {
        all_zero &= record[i] == 0x00;
        all_ff &= record[i] == 0xff;
    }

    if (all_zero || all_ff || (record[0] & 1))
        return 0;

    for (uint32_t i = 0; i < 6; i++) {
        if ((record[i] ^ record[i + 6]) != 0xff)
            return 0;
    }

    return 1;
}

static void update_factory_mac(P1io* p1io) {
    memcpy(p1io->factory_mac, P1IO_FACTORY_MAC, sizeof(p1io->factory_mac));

    if (p1io->bootrom_loaded) {
        const uint8_t* record = p1io->bootrom + P1IO_BOOTROM_MAC_BACKUP;

        if (bootrom_mac_backup_valid(record))
            memcpy(p1io->factory_mac, record, sizeof(p1io->factory_mac));
        else
            iris_warning(p1io, "Invalid boot ROM MAC backup, using default");
    }

    // iris_info(p1io, "Factory MAC {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
    //     p1io->factory_mac[0], p1io->factory_mac[1], p1io->factory_mac[2],
    //     p1io->factory_mac[3], p1io->factory_mac[4], p1io->factory_mac[5]);

    build_fsci_mac_frames(p1io);
}

static uint8_t dallas_crc8(const uint8_t* data, uint32_t size) {
    uint8_t crc = 0;

    for (uint32_t index = 0; index < size; index++) {
        uint8_t value = data[index];

        for (uint32_t bit = 0; bit < 8; bit++) {
            uint8_t mix = (crc ^ value) & 1;

            crc >>= 1;

            if (mix) {
                crc ^= 0x8c;
            }

            value >>= 1;
        }
    }

    return crc;
}

static void set_dallas_key_response(P1io* p1io, const uint8_t* id) {
    uint8_t* serial = p1io->dongle[DONGLE_INTERNAL];

    serial[0] = 0x14;

    memcpy(serial + 1, id, 6);

    serial[7] = dallas_crc8(serial, P1IO_DALLAS_SERIAL_SIZE - 1);
}

static int normalize_dallas_dongle(const uint8_t* raw, uint8_t* out) {
    if (dallas_crc8(raw + P1IO_DALLAS_PAYLOAD_SIZE, P1IO_DALLAS_SERIAL_SIZE - 1) == raw[P1IO_DONGLE_SIZE - 1]) {
        memcpy(out, raw + P1IO_DALLAS_PAYLOAD_SIZE, P1IO_DALLAS_SERIAL_SIZE);
        memcpy(out + P1IO_DALLAS_SERIAL_SIZE, raw, P1IO_DALLAS_PAYLOAD_SIZE);

        return 1;
    }

    if (dallas_crc8(raw, P1IO_DALLAS_SERIAL_SIZE - 1) == raw[P1IO_DALLAS_SERIAL_SIZE - 1]) {
        memcpy(out, raw, P1IO_DONGLE_SIZE);

        return 1;
    }

    return 0;
}

static void update_dallas_key_response(P1io* p1io) {
    if (p1io->dongle_loaded[DONGLE_INTERNAL])
        return;

    set_dallas_key_response(p1io, P1IO_DEFAULT_DALLAS_ID);

    if (!p1io->bootrom_loaded)
        return;

    for (uint32_t offset = P1IO_BOOTROM_ID_BASE; offset <= P1IO_BOOTROM_SIZE - 0x20; offset += 0x10) {
        const uint8_t* record = p1io->bootrom + offset;

        if (record[0x10] != 'G' || record[0x1a] != 'J' || record[0x1b] != 'A')
            continue;

        set_dallas_key_response(p1io, record);

        return;
    }
}

int load_config_rom(P1io* p1io, const char* path) {
    p1io->config_rom_loaded = load_blob(p1io, path, p1io->config_rom, P1IO_CONFIG_ROM_SIZE, "config ROM");

    return p1io->config_rom_loaded;
}

int load_bootrom(P1io* p1io, const char* path) {
    p1io->bootrom_loaded = load_blob(p1io, path, p1io->bootrom, P1IO_BOOTROM_SIZE, "boot ROM");

    update_dallas_key_response(p1io);
    update_factory_mac(p1io);

    return p1io->bootrom_loaded;
}

int load_dongle(P1io* p1io, int which, const char* path) {
    if (which < 0 || which >= DONGLE_COUNT)
        return 0;

    uint8_t raw[P1IO_DONGLE_SIZE];

    if (!load_blob(p1io, path, raw, P1IO_DONGLE_SIZE, "dongle"))
        return 0;

    if (!normalize_dallas_dongle(raw, p1io->dongle[which])) {
        iris_error(p1io, "Dongle \"{}\" has no valid serial checksum", path);

        return 0;
    }

    p1io->dongle_loaded[which] = 1;

    const uint8_t* serial = p1io->dongle[which];

    // iris_info(p1io, "Dongle {} serial {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", which,
    //     serial[0], serial[1], serial[2], serial[3], serial[4], serial[5], serial[6], serial[7]);

    return 1;
}

int load_bbsram(P1io* p1io, const char* path) {
    if (!path || !path[0])
        return 0;

    strncpy(p1io->bbsram_path, path, sizeof(p1io->bbsram_path) - 1);

    p1io->bbsram_path[sizeof(p1io->bbsram_path) - 1] = '\0';

    FILE* file = fopen(path, "rb");

    if (!file) {
        memset(p1io->bbsram, 0, sizeof(p1io->bbsram));

        return 1;
    }

    fread(p1io->bbsram, 1, sizeof(p1io->bbsram), file);

    fclose(file);

    return 1;
}

static void flush_bbsram(P1io* p1io) {
    if (!p1io->bbsram_path[0])
        return;

    FILE* file = fopen(p1io->bbsram_path, "wb");

    if (!file)
        return;

    fwrite(p1io->bbsram, 1, sizeof(p1io->bbsram), file);

    fclose(file);
}

static int64_t cf_block_read(void* udata, uint64_t offset, void* buf, uint64_t size) {
    iop::disc::Disc* disc = (iop::disc::Disc*)udata;

    uint8_t sector[P1IO_SECTOR_SIZE];
    uint8_t* out = (uint8_t*)buf;

    uint64_t done = 0;

    while (done < size) {
        uint64_t position = offset + done;
        uint64_t index = position / P1IO_SECTOR_SIZE;
        uint32_t start = (uint32_t)(position % P1IO_SECTOR_SIZE);

        uint64_t chunk = P1IO_SECTOR_SIZE - start;

        if (chunk > size - done)
            chunk = size - done;

        ata::disc::ata_read_sector(disc, (uint32_t)index, sector);

        memcpy(out + done, sector + start, (size_t)chunk);

        done += chunk;
    }

    return (int64_t)done;
}

static uint64_t cf_block_size(void* udata) {
    return ata::disc::ata_get_sector_count((iop::disc::Disc*)udata) * (uint64_t)P1IO_SECTOR_SIZE;
}

static void cf_block_close(void* udata) {
}

static void open_cf_container(P1io* p1io) {
    p1io->cf_blk = new fs::blk::Device();

    p1io->cf_blk->read = cf_block_read;
    p1io->cf_blk->get_size = cf_block_size;
    p1io->cf_blk->close = cf_block_close;
    p1io->cf_blk->udata = p1io->cf;
    p1io->cf_blk->logger = p1io->logger;
    p1io->cf_blk->logger_id = fs::get_logger_id(p1io->logger);

    std::vector <fs::part::Partition> partitions;

    fs::part::scan(p1io->logger, p1io->cf_blk, &partitions);

    for (const fs::part::Partition& partition : partitions) {
        fs::blk::Device* slice = fs::blk::open_slice(p1io->logger, p1io->cf_blk, partition.offset, partition.size, false);

        if (!slice)
            continue;

        fs::Fs* filesystem = fs::probe(p1io->logger, slice, true);

        if (!filesystem)
            continue;

        std::vector <fs::Entry> entries;

        if (fs::list(filesystem, "/", &entries) == fs::FS_OK) {
            for (const fs::Entry& entry : entries) {
                if (entry.flags & fs::ENTRY_DIRECTORY)
                    continue;

                if (!entry.size)
                    continue;

                fs::Handle* handle = nullptr;

                if (fs::open(filesystem, entry.name, &handle) != fs::FS_OK)
                    continue;

                p1io->cf_slice = slice;
                p1io->cf_fs = filesystem;
                p1io->cf_file = handle;
                p1io->cf_file_size = entry.size;

                iris_info(p1io, "CF container \"{}\", {} bytes", entry.name, entry.size);

                return;
            }
        }

        fs::close(filesystem);
    }
}

int load_cf(P1io* p1io, const char* path) {
    if (!path || !path[0])
        return 0;

    p1io->cf = ata::disc::open(p1io->logger, path);

    if (!p1io->cf) {
        iris_error(p1io, "Failed to open CF image \"{}\"", path);

        return 0;
    }

    iris_info(p1io, "CF image \"{}\", {} sectors", path, ata::disc::ata_get_sector_count(p1io->cf));

    open_cf_container(p1io);

    return 1;
}

int load_hdd(P1io* p1io, const char* path) {
    if (!path || !path[0])
        return 0;

    p1io->hdd = ata::disc::open(p1io->logger, path);

    if (!p1io->hdd) {
        iris_error(p1io, "Failed to open HDD image \"{}\"", path);

        return 0;
    }

    iris_info(p1io, "HDD image \"{}\", {} sectors", path, ata::disc::ata_get_sector_count(p1io->hdd));

    return 1;
}

void press_switch(P1io* p1io, uint32_t mask) {
    p1io->jamma |= mask;
}

void release_switch(P1io* p1io, uint32_t mask) {
    p1io->jamma &= ~mask;
}

void insert_coin(P1io* p1io, int slot) {
    if (slot < 0 || slot > 1)
        return;

    p1io->coins[slot]++;
}

static int region_read(P1io* p1io, uint32_t reg, uint8_t* buf, int len) {
    if (reg >= P1IO_BOOTROM && reg < P1IO_BOOTROM + P1IO_BOOTROM_SIZE) {
        uint32_t start = reg - P1IO_BOOTROM;

        if (!p1io->bootrom_loaded || start + len > P1IO_BOOTROM_SIZE)
            return fw::device::RESP_ADDRESS_ERROR;

        memcpy(buf, p1io->bootrom + start, len);

        return fw::device::RESP_COMPLETE;
    }

    if (reg >= P1IO_BBSRAM && reg < P1IO_BBSRAM + P1IO_BBSRAM_SIZE) {
        uint32_t start = reg - P1IO_BBSRAM;

        if (start + len > P1IO_BBSRAM_SIZE)
            return fw::device::RESP_ADDRESS_ERROR;

        memcpy(buf, p1io->bbsram + start, len);

        return fw::device::RESP_COMPLETE;
    }

    // iris_debug(p1io, "Unhandled read from {:06x} ({} bytes)", reg, len);

    memset(buf, 0, len);

    return fw::device::RESP_COMPLETE;
}

static uint32_t get_le32(const uint8_t* buf) {
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static uint32_t read_status_checksum(const uint8_t* data, uint32_t size) {
    uint32_t checksum = 0;

    for (uint32_t offset = 0; offset < size; offset += 2) {
        uint32_t word = (uint32_t)data[offset] << 8;

        if (offset + 1 < size)
            word |= data[offset + 1];

        checksum = (checksum + word) & 0x7fffffffu;
    }

    return checksum;
}

static uint32_t byteswap32(uint32_t value) {
    return (value >> 24) | ((value >> 8) & 0xff00) | ((value << 8) & 0xff0000) | (value << 24);
}

static int read_sectors(P1io* p1io, fw::Fw* host, iop::disc::Disc* image, uint32_t sector, uint32_t count, uint32_t dest, uint32_t status_offset) {
    if (!count)
        return 1;

    int container = image == p1io->cf && p1io->cf_file;

    if (!image && !container) {
        iris_debug(p1io, "No storage image for sector read at {:x}", sector);

        return 0;
    }

    uint64_t bytes = (uint64_t)count * P1IO_SECTOR_SIZE;

    if (bytes > P1IO_MAX_READ_BYTES) {
        iris_error(p1io, "Refusing oversized sector read sector {:x} count {:x}", sector, count);

        return 0;
    }

    uint64_t total = container
        ? p1io->cf_file_size / P1IO_SECTOR_SIZE
        : ata::disc::ata_get_sector_count(image);

    if ((uint64_t)sector + count > total) {
        iris_error(p1io, "Sector read {:x} count {:x} past end of image ({} sectors)", sector, count, total);

        return 0;
    }

    uint8_t* data = new uint8_t[bytes];

    if (container) {
        uint64_t offset = (uint64_t)sector * P1IO_SECTOR_SIZE;

        if (fs::read(p1io->cf_fs, p1io->cf_file, offset, data, bytes) != (int64_t)bytes) {
            iris_error(p1io, "CF container read {:x} count {:x} failed", sector, count);

            delete[] data;

            return 0;
        }
    } else {
        for (uint32_t i = 0; i < count; i++) {
            ata::disc::ata_read_sector(image, sector + i, data + ((uint64_t)i * P1IO_SECTOR_SIZE));
        }
    }

    fw::write_iop_memory(host, dest, data, (uint32_t)bytes);

    uint32_t checksum = read_status_checksum(data, (uint32_t)bytes);

    delete[] data;

    // iris_debug(p1io, "Sector read {:x} count {:x} -> iop {:08x}", sector, count, dest);

    fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, status_offset, byteswap32(checksum));

    return 1;
}

static int write_sectors(P1io* p1io, fw::Fw* host, iop::disc::Disc* image, uint32_t sector, uint32_t count, uint32_t src, uint32_t status_offset) {
    iris_debug(p1io, "Sector write {:x} count {:x} from iop {:08x} (ignored)", sector, count, src);

    fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, status_offset, 0);

    return 1;
}

static int cmd_storage(P1io* p1io, fw::Fw* host, uint32_t reg, const uint8_t* buf, int len) {
    int quads = len / 4;

    if (quads < 4 || !host)
        return 0;

    uint32_t payload[8] = {};

    for (int i = 0; i < quads && i < 8; i++) {
        payload[i] = get_le32(buf + (i * 4));
    }

    int is_ata = reg == P1IO_CMD_ATA;
    int extended = is_ata && quads >= 5;

    uint32_t subop = payload[0];
    uint32_t sector = extended ? payload[2] : payload[1];
    uint32_t count = extended ? payload[3] : payload[2];
    uint32_t dest = extended ? payload[4] : payload[3];

    iris_debug(p1io, "cmd {:03x} q{} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x}",
        reg, quads, payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7]);

    iop::disc::Disc* image = is_ata ? p1io->hdd : (p1io->cf ? p1io->cf : p1io->hdd);

    uint32_t status_offset = is_ata ? P1IO_ATA : P1IO_CF;

    if (subop == 0)
        return read_sectors(p1io, host, image, sector, count, dest, status_offset);

    if (subop == 2)
        return write_sectors(p1io, host, image, sector, count, dest, status_offset);

    if (subop == 4 && !is_ata) {
        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, status_offset, 0);

        return 1;
    }

    if (subop == 7 && is_ata) {
        uint32_t device = payload[1];

        if (device == 0x14)
            p1io->pythonfs_formatted = 1;

        uint32_t status = (device == 0x0a && !p1io->pythonfs_formatted) ? 0xffffffd1u : 0;

        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, status_offset, byteswap32(status));

        return 1;
    }

    iris_debug(p1io, "Unhandled storage command {:03x} subop {:x} sector {:x} count {:x}", reg, subop, sector, count);

    return 0;
}

struct JammaBit {
    uint32_t mask;
    uint32_t source;
    uint32_t status;
};

static const JammaBit P1IO_JAMMA_BITS[] = {
    { JAMMA_TEST,       1u << 15, 0x0001 },
    { JAMMA_SERVICE,    1u << 14, 0x0002 },
    { JAMMA_COIN1,      0,        0x0004 },
    { JAMMA_COIN2,      0,        0x0008 },
    { JAMMA_P1_START,   1u << 11, 0x0010 },
    { JAMMA_P1_UP,      1u << 2,  0x0020 },
    { JAMMA_P1_DOWN,    1u << 3,  0x0040 },
    { JAMMA_P1_LEFT,    1u << 0,  0x0080 },
    { JAMMA_P1_RIGHT,   1u << 1,  0x0100 },
    { JAMMA_P1_BUTTON1, 1u << 4,  0x0200 },
    { JAMMA_P1_BUTTON2, 1u << 5,  0x0400 },
    { JAMMA_P1_BUTTON3, 1u << 6,  0x0800 },
    { JAMMA_P2_START,   1u << 27, 0 },
    { JAMMA_P2_UP,      1u << 18, 0 },
    { JAMMA_P2_DOWN,    1u << 19, 0 },
    { JAMMA_P2_LEFT,    1u << 16, 0 },
    { JAMMA_P2_RIGHT,   1u << 17, 0 },
    { JAMMA_P2_BUTTON1, 1u << 20, 0 },
    { JAMMA_P2_BUTTON2, 1u << 21, 0 },
    { JAMMA_P2_BUTTON3, 1u << 22, 0 }
};

static uint32_t jamma_source_bits(P1io* p1io) {
    uint32_t source = 0;

    for (const JammaBit& bit : P1IO_JAMMA_BITS) {
        if (p1io->jamma & bit.mask)
            source |= bit.source;
    }

    return source;
}

static uint32_t jamma_status(P1io* p1io) {
    uint32_t status = P1IO_JAMMA_STATUS_NEUTRAL;

    for (const JammaBit& bit : P1IO_JAMMA_BITS) {
        if (p1io->jamma & bit.mask)
            status &= ~(bit.status & P1IO_JAMMA_ACTIVE_LOW_MASK);
    }

    return status;
}

static void send_jamma_report(P1io* p1io, fw::Fw* host, const uint32_t* quads) {
    if (p1io->jamma_dest == P1IO_JAMMA_DEST_INVALID)
        return;

    fw::queue_remote_write_quads(host, fw::OFFSET_HIGH_IOP_DMA, p1io->jamma_dest, quads, P1IO_JAMMA_REPORT_QUADS);
}

static uint32_t pack_serial_word(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void dallas_no_key(fw::Fw* host) {
    const uint32_t response[3] = { 0x01000000, 0x00000000, 0x00000000 };

    fw::queue_remote_write_quads(host, P1IO_REGION_HIGH, P1IO_DALLAS, response, 3);
}

static int cmd_dallas(P1io* p1io, fw::Fw* host, const uint8_t* buf, int len) {
    int quads = len / 4;

    if (quads < 1 || !host)
        return 0;

    uint32_t subop = get_le32(buf);
    uint32_t key = quads > 1 ? get_le32(buf + 4) : 0;
    uint32_t offset = quads > 2 ? get_le32(buf + 8) : 0;
    uint32_t count = quads > 3 ? get_le32(buf + 12) : 0;
    uint32_t dest = quads > 4 ? get_le32(buf + 16) : 0;

    int loaded = key < DONGLE_COUNT && p1io->dongle_loaded[key];

    iris_debug(p1io, "Dallas subop {} key {} offset {:x} count {:x} loaded {}", subop, key, offset, count, loaded);

    if (subop == 0) {
        if (!loaded) {
            dallas_no_key(host);

            return 1;
        }

        const uint8_t* dongle = p1io->dongle[key];

        uint32_t response[3] = { 0, pack_serial_word(dongle), pack_serial_word(dongle + 4) };

        fw::queue_remote_write_quads(host, P1IO_REGION_HIGH, P1IO_DALLAS, response, 3);

        return 1;
    }

    int in_range = offset <= P1IO_DALLAS_PAYLOAD_SIZE && count <= P1IO_DALLAS_PAYLOAD_SIZE - offset;

    if (!loaded || !in_range) {
        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_DALLAS, 0x01000000);

        return 1;
    }

    uint8_t* payload = p1io->dongle[key] + P1IO_DALLAS_SERIAL_SIZE + offset;

    if (subop == 1) {
        if (count && dest)
            fw::queue_remote_write_bytes(host, fw::OFFSET_HIGH_IOP_DMA, dest, payload, count);

        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_DALLAS, 0);

        return 1;
    }

    if (subop == 2) {
        if (count && dest)
            fw::read_iop_memory(host, dest, payload, count);

        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_DALLAS, 0);

        return 1;
    }

    fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_DALLAS, 0x01000000);

    return 1;
}

static int cmd_jamma_init(P1io* p1io, fw::Fw* host, const uint8_t* buf, int len) {
    if (len / 4 < 8 || !host)
        return 0;

    p1io->jamma_dest = get_le32(buf);

    iris_debug(p1io, "JAMMA init, reports to iop {:08x}", p1io->jamma_dest);

    uint32_t status = jamma_status(p1io);

    const uint32_t init[P1IO_JAMMA_REPORT_QUADS] = {
        0xfe4b109c, 0x00000000, 0x00000000, 0x00000000,
        P1IO_JAMMA_JVS_PRESENT,
        0x81000100, 0x00000000, 0x01020101,
        status
    };

    const uint32_t attached[P1IO_JAMMA_REPORT_QUADS] = {
        0x98062caa, 0x00000000, 0x00000000, 0x00000000,
        P1IO_JAMMA_JVS_PRESENT,
        0x80000000, 0x00000000, 0x01020101,
        status
    };

    send_jamma_report(p1io, host, init);

    for (uint32_t i = 0; i < P1IO_JAMMA_ATTACH_REPORTS; i++) {
        send_jamma_report(p1io, host, attached);
    }

    return 1;
}

static int cmd_jamma_output(P1io* p1io, fw::Fw* host, const uint8_t* buf, int len) {
    if (len / 4 < 8 || !host)
        return 0;

    if (p1io->jamma_dest == P1IO_JAMMA_DEST_INVALID)
        return 1;

    uint32_t present = p1io->io_mode == IO_MODE_POPN ? 0 : P1IO_JAMMA_JVS_PRESENT;

    const uint32_t live[P1IO_JAMMA_REPORT_QUADS] = {
        byteswap32(p1io->coins[0]), byteswap32(p1io->coins[1]), 0x00000000, 0x00000000,
        byteswap32(jamma_source_bits(p1io)) | present,
        0x80000000, 0x00000000, 0x01020101,
        jamma_status(p1io)
    };

    uint8_t bytes[P1IO_JAMMA_REPORT_QUADS * 4];

    for (uint32_t i = 0; i < P1IO_JAMMA_REPORT_QUADS; i++) {
        bytes[i * 4 + 0] = (uint8_t)live[i];
        bytes[i * 4 + 1] = (uint8_t)(live[i] >> 8);
        bytes[i * 4 + 2] = (uint8_t)(live[i] >> 16);
        bytes[i * 4 + 3] = (uint8_t)(live[i] >> 24);
    }

    fw::write_iop_memory(host, p1io->jamma_dest, bytes, sizeof(bytes));

    return 1;
}

static int cmd_adpcm(P1io* p1io, fw::Fw* host, const uint8_t* buf, int len) {
    int quads = len / 4;

    if (quads < 1 || !host)
        return 0;

    uint32_t requested = get_le32(buf);
    uint32_t count = requested + 1;

    if (count > (uint32_t)quads - 1)
        count = (uint32_t)quads - 1;

    int have_sector_high = 0;
    int have_sector_low = 0;

    uint32_t sector_high = 0;
    uint32_t sector_low = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t word = get_le32(buf + 4 + i * 4);

        switch (word >> 24) {
            case 0x00: {
                if (word == 0)
                    p1io->adpcm_playing = 0;
            } break;

            case 0x02: {
                sector_high = word & 0xffff;
                have_sector_high = 1;
            } break;

            case 0x03: {
                sector_low = word & 0xffff;
                have_sector_low = 1;
            } break;

            case 0x05: {
                p1io->adpcm_volume[0] = (uint16_t)(word & 0xffff);
            } break;

            case 0x06: {
                p1io->adpcm_volume[1] = (uint16_t)(word & 0xffff);
            } break;
        }
    }

    if (have_sector_high && have_sector_low) {
        p1io->adpcm_sector = (sector_high << 16) | sector_low;
        p1io->adpcm_playing = 1;

        iris_debug(p1io, "ADPCM play sector {:x} volume {} {}", p1io->adpcm_sector, p1io->adpcm_volume[0], p1io->adpcm_volume[1]);
    }

    fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_ADPCM, 0);

    return 1;
}

static int cmd_bbsram(P1io* p1io, fw::Fw* host, const uint8_t* buf, int len) {
    int quads = len / 4;

    if (quads < 4 || !host)
        return 0;

    uint32_t subop = get_le32(buf);
    uint32_t offset = get_le32(buf + 4);
    uint32_t count = get_le32(buf + 8);
    uint32_t dest = get_le32(buf + 12);

    if (offset > P1IO_BBSRAM_SIZE || count > P1IO_BBSRAM_SIZE - offset) {
        iris_debug(p1io, "BBSRAM command out of range offset {:x} count {:x}", offset, count);

        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_BBSRAM, 0);

        return 1;
    }

    if (subop == 0 && count && dest) {
        iris_debug(p1io, "BBSRAM read {:x} count {:x} -> iop {:08x}", offset, count, dest);

        for (uint32_t done = 0; done < count; done += P1IO_DMA_CHUNK) {
            uint32_t chunk = count - done;

            if (chunk > P1IO_DMA_CHUNK)
                chunk = P1IO_DMA_CHUNK;

            fw::queue_remote_write_bytes(host, fw::OFFSET_HIGH_IOP_DMA, dest + done, p1io->bbsram + offset + done, chunk);
        }
    }

    if (subop == 1 && count) {
        iris_debug(p1io, "BBSRAM write {:x} count {:x} <- iop {:08x}", offset, count, dest);

        if (dest) {
            fw::read_iop_memory(host, dest, p1io->bbsram + offset, count);
        } else {
            uint32_t available = quads > 4 ? (uint32_t)(quads - 4) * 4 : 0;

            if (count > available) {
                fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_BBSRAM, 0);

                return 1;
            }

            for (uint32_t i = 0; i < count; i++) {
                uint32_t word = get_le32(buf + 16 + (i / 4) * 4);

                p1io->bbsram[offset + i] = (uint8_t)(word >> (24 - ((i & 3) * 8)));
            }
        }

        if (offset != 0 || count != P1IO_BBSRAM_VOLATILE_TEST)
            flush_bbsram(p1io);
    }

    fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_BBSRAM, 0);

    return 1;
}

static int is_serial_mode(P1io* p1io) {
    return p1io->io_mode == IO_MODE_B22 || p1io->io_mode == IO_MODE_DOGSTATIONDX;
}

static uint8_t acio_checksum(const uint8_t* data, uint32_t count) {
    uint8_t checksum = 0;

    for (uint32_t i = 1; i < count; i++) {
        checksum = (uint8_t)(checksum + data[i]);
    }

    return checksum;
}

static void queue_uart_bytes(P1io* p1io, const uint8_t* data, uint32_t count) {
    if (!count || count > P1IO_UART_RX_LIMIT)
        return;

    if (p1io->uart_rx_size + count > P1IO_UART_RX_LIMIT) {
        uint32_t overflow = p1io->uart_rx_size + count - P1IO_UART_RX_LIMIT;

        if (overflow > p1io->uart_rx_size)
            overflow = p1io->uart_rx_size;

        memmove(p1io->uart_rx, p1io->uart_rx + overflow, p1io->uart_rx_size - overflow);

        p1io->uart_rx_size -= overflow;
    }

    memcpy(p1io->uart_rx + p1io->uart_rx_size, data, count);

    p1io->uart_rx_size += count;
}

static int queue_uart_read_data(P1io* p1io, fw::Fw* host, uint32_t requested) {
    uint32_t count = requested < p1io->uart_rx_size ? requested : p1io->uart_rx_size;

    if (!count)
        return 0;

    uint32_t quads = 2 + ((count + 1) / 2);

    uint32_t callback[2 + (P1IO_UART_READ_LIMIT + 1) / 2] = {};

    callback[1] = (count << 16) | p1io->uart_rx[0];

    for (uint32_t i = 1; i < count; i++) {
        uint32_t index = 2 + ((i - 1) / 2);

        if (((i - 1) & 1) == 0)
            callback[index] |= (uint32_t)p1io->uart_rx[i] << 16;
        else
            callback[index] |= p1io->uart_rx[i];
    }

    for (uint32_t i = 0; i < quads; i++) {
        callback[i] = byteswap32(callback[i]);
    }

    fw::queue_remote_write_quads(host, P1IO_REGION_HIGH, P1IO_UART, callback, quads);

    memmove(p1io->uart_rx, p1io->uart_rx + count, p1io->uart_rx_size - count);

    p1io->uart_rx_size -= count;

    return 1;
}

static void queue_external_io_product(P1io* p1io, uint8_t device) {
    uint8_t response[28] = {
        0xaa, 0xaa, device, 0x02, 0x00, 0x00,
        0xaa, 0xa5, device, 0x02, 0x05,
        0x43, 0x52, 0x2d, 0x32, 0x36, 0x01, 0x00, 0x07, 0x42, 0x32, 0x32, 0x43, 0x00, 0x00, 0x02, 0x01,
        0x00
    };

    response[5] = acio_checksum(response, 5);
    response[27] = acio_checksum(response + 6, 21);

    queue_uart_bytes(p1io, response, sizeof(response));
}

static void queue_external_io_ack(P1io* p1io, uint8_t device, uint8_t command) {
    const uint8_t response[13] = {
        0xaa, 0xaa, device, command, 0x00, (uint8_t)(0xaa + device + command),
        0xaa, 0xa5, device, command, 0x01, 0x00, (uint8_t)(0xa6 + device + command)
    };

    queue_uart_bytes(p1io, response, sizeof(response));
}

static void queue_b22_bulk_ack(P1io* p1io, uint8_t node) {
    uint8_t response[0x4c] = {};

    response[0] = 0xaa;
    response[1] = 0xff;
    response[2] = node;
    response[3] = 0x02;
    response[4] = 0x00;
    response[5] = (uint8_t)(node + 1);
    response[6] = 0xaa;
    response[7] = 0xf0;
    response[8] = node;
    response[9] = 0x02;
    response[10] = 0x07;

    memset(response + 11, 0xff, 7);

    queue_uart_bytes(p1io, response, sizeof(response));
}

static void queue_wrapped_response(P1io* p1io, const uint8_t* request, uint32_t request_size, const uint8_t* response, uint32_t response_size) {
    uint8_t packet[512];

    if (request_size + 4 + response_size + 1 > sizeof(packet))
        return;

    uint32_t size = request_size;

    memcpy(packet, request, request_size);

    packet[size++] = 0xaa;
    packet[size++] = is_serial_mode(p1io) ? 0x01 : 0x00;
    packet[size++] = request[2];
    packet[size++] = request[3];

    memcpy(packet + size, response, response_size);

    size += response_size;

    packet[size++] = acio_checksum(packet + request_size, response_size + 4);

    queue_uart_bytes(p1io, packet, size);
}

static uint32_t find_acio_packet_size(const uint8_t* bytes, uint32_t size) {
    for (uint32_t packet = 6; packet <= size; packet++) {
        if (acio_checksum(bytes, packet - 1) == bytes[packet - 1])
            return packet;
    }

    return 0;
}

static void handle_acio_write(P1io* p1io, const uint8_t* bytes, uint32_t size) {
    if (size >= 4 && bytes[0] == 0xaa && bytes[1] == 0xaa && bytes[2] == 0xaa && bytes[3] == 0x55) {
        const uint8_t sync[4] = { 0xaa, 0xaa, 0xaa, 0x55 };

        queue_uart_bytes(p1io, sync, sizeof(sync));

        return;
    }

    if (size >= 4 && bytes[0] == 0xaa && bytes[1] == 0xaa && bytes[2] == 0x00 && bytes[3] == 0x00) {
        const uint8_t reset[4] = { 0xaa, 0xaa, 0x00, 0x00 };

        queue_uart_bytes(p1io, reset, sizeof(reset));

        return;
    }

    if (size >= 4 && bytes[0] == 0xaa && bytes[1] == 0xaa && bytes[2] == 0x00 && bytes[3] == 0x01) {
        const uint8_t b22[14] = {
            0xaa, 0xaa, 0x00, 0x01, 0x01, 0x00, 0xac,
            0xaa, 0xaa, 0x00, 0x01, 0x01, 0x01, 0xad
        };

        const uint8_t dogstationdx[7] = { 0xaa, 0xaa, 0x00, 0x01, 0x01, 0x02, 0xae };
        const uint8_t generic[7] = { 0xaa, 0xaa, 0x00, 0x01, 0x01, 0x01, 0xad };

        if (p1io->io_mode == IO_MODE_B22)
            queue_uart_bytes(p1io, b22, sizeof(b22));
        else if (p1io->io_mode == IO_MODE_DOGSTATIONDX)
            queue_uart_bytes(p1io, dogstationdx, sizeof(dogstationdx));
        else
            queue_uart_bytes(p1io, generic, sizeof(generic));

        return;
    }

    if (is_serial_mode(p1io) && size >= 6 && bytes[0] == 0xaa && bytes[1] == 0xaa && bytes[2] >= 0x01 && bytes[2] <= 0x02) {
        if (bytes[3] == 0x02) {
            queue_external_io_product(p1io, bytes[2]);

            return;
        }

        if (bytes[3] == 0x03 || bytes[3] == 0x04) {
            queue_external_io_ack(p1io, bytes[2], bytes[3]);

            return;
        }
    }

    if (p1io->io_mode == IO_MODE_B22 && size >= 6 && bytes[0] == 0xaa && bytes[1] == 0xff) {
        queue_b22_bulk_ack(p1io, bytes[2]);

        return;
    }

    if (size >= 6 && bytes[0] == 0xaa && bytes[1] == 0x00) {
        uint32_t packet = find_acio_packet_size(bytes, size);

        if (!packet)
            return;

        uint8_t command = is_serial_mode(p1io) ? (uint8_t)(bytes[3] & ~0x40) : bytes[3];

        const uint8_t ok[2] = {
            (uint8_t)(is_serial_mode(p1io) ? 0x01 : 0x00),
            (uint8_t)(is_serial_mode(p1io) && bytes[2] != 0x01 ? 0x01 : 0x00)
        };

        switch (command) {
            case 0x00:
            case 0x01:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x14:
            case 0x15:
            case 0x16:
            case 0x1e:
            case 0x1f: {
                queue_wrapped_response(p1io, bytes, packet, ok, sizeof(ok));
            } break;

            case 0x18: {
                uint8_t empty[0x82] = {};

                queue_wrapped_response(p1io, bytes, packet, empty, sizeof(empty));
            } break;

            case 0x26:
            case 0x36: {
                if (!is_serial_mode(p1io))
                    break;

                uint8_t status[9] = { 0x04 };

                queue_wrapped_response(p1io, bytes, packet, status, sizeof(status));
            } break;
        }
    }
}

static int cmd_uart(P1io* p1io, fw::Fw* host, const uint8_t* buf, int len) {
    int quads = len / 4;

    if (quads < 1 || !host)
        return 0;

    uint32_t subop = get_le32(buf);
    uint32_t count = quads > 1 ? get_le32(buf + 4) : 0;

    int extio = p1io->io_mode == IO_MODE_EXTIO;
    int queued = 0;

    if (subop == 1 && count) {
        uint8_t bytes[P1IO_UART_RX_LIMIT];

        if (count > sizeof(bytes))
            count = sizeof(bytes);

        uint32_t inline_bytes = quads > 2 ? (uint32_t)(quads - 2) * 4 : 0;

        if (count > inline_bytes) {
            uint32_t source = quads > 2 ? get_le32(buf + 8) : 0;

            if (!source)
                return 1;

            fw::read_iop_memory(host, source, bytes, count);
        } else {
            for (uint32_t i = 0; i < count; i++) {
                uint32_t word = get_le32(buf + 8 + (i / 4) * 4);

                bytes[i] = (uint8_t)(word >> (24 - ((i & 3) * 8)));
            }
        }

        iris_debug(p1io, "UART write {:x} bytes, head {:02x} {:02x} {:02x} {:02x}", count,
            bytes[0], count > 1 ? bytes[1] : 0, count > 2 ? bytes[2] : 0, count > 3 ? bytes[3] : 0);

        handle_acio_write(p1io, bytes, count);
    } else if (subop == 2 && (extio || p1io->uart_rx_size)) {
        queued = queue_uart_read_data(p1io, host, P1IO_UART_READ_LIMIT);
    }

    fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_UART, byteswap32(2));

    if ((extio && subop == 2) || queued)
        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_UART, byteswap32(1));

    return 1;
}

static int cmd_fsci(P1io* p1io, fw::Fw* host, const uint8_t* buf, int len) {
    int quads = len / 4;

    if (quads < 1 || !host)
        return 0;

    uint32_t subop = get_le32(buf);
    uint32_t count = quads > 1 ? get_le32(buf + 4) : 0;
    uint32_t dest = quads > 2 ? get_le32(buf + 8) : 0;

    iris_debug(p1io, "FSCI subop {} count {:x} dest {:08x}", subop, count, dest);

    if (subop == 0) {
        p1io->fsci_stream_offset = 0;

        build_fsci_mac_frames(p1io);

        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_FSCI, 0);

        return 1;
    }

    if (subop == 2 && count && dest && p1io->io_mode != IO_MODE_POPN) {
        uint32_t available = p1io->fsci_stream_offset < P1IO_FSCI_MAC_STREAM_SIZE
            ? P1IO_FSCI_MAC_STREAM_SIZE - p1io->fsci_stream_offset
            : 0;

        uint32_t bytes = count;

        if (bytes > P1IO_FSCI_MAX_READ)
            bytes = P1IO_FSCI_MAX_READ;

        if (bytes > available)
            bytes = available;

        if (bytes) {
            fw::write_iop_memory(host, dest, p1io->fsci_mac_stream + p1io->fsci_stream_offset, bytes);

            p1io->fsci_stream_offset += bytes;
        }

        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_FSCI, byteswap32(bytes));

        return 1;
    }

    fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_FSCI, 0);

    return 1;
}

static int cmd_bootrom(P1io* p1io, fw::Fw* host, const uint8_t* buf, int len) {
    int quads = len / 4;

    if (quads < 4 || !host)
        return 0;

    uint32_t subop = get_le32(buf);
    uint32_t offset = get_le32(buf + 4);
    uint32_t count = get_le32(buf + 8);
    uint32_t dest = get_le32(buf + 12);

    if (offset > P1IO_BOOTROM_SIZE || count > P1IO_BOOTROM_SIZE - offset) {
        iris_debug(p1io, "Boot ROM command out of range offset {:x} count {:x}", offset, count);

        fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_BOOTROM, P1IO_BOOTROM_STATUS_OK);

        return 1;
    }

    if (subop == 0 && count && dest) {
        iris_debug(p1io, "Boot ROM read {:x} count {:x} -> iop {:08x}", offset, count, dest);

        for (uint32_t done = 0; done < count; done += P1IO_DMA_CHUNK) {
            uint32_t chunk = count - done;

            if (chunk > P1IO_DMA_CHUNK)
                chunk = P1IO_DMA_CHUNK;

            fw::queue_remote_write_bytes(host, fw::OFFSET_HIGH_IOP_DMA, dest + done, p1io->bootrom + offset + done, chunk);
        }
    }

    if (subop == 1 && count && dest) {
        iris_debug(p1io, "Boot ROM write {:x} count {:x} <- iop {:08x}", offset, count, dest);

        fw::read_iop_memory(host, dest, p1io->bootrom + offset, count);
    }

    fw::queue_remote_write_quad(host, P1IO_REGION_HIGH, P1IO_BOOTROM, P1IO_BOOTROM_STATUS_OK);

    return 1;
}

static int region_write(P1io* p1io, uint32_t reg, const uint8_t* buf, int len) {
    if (reg >= P1IO_BBSRAM && reg < P1IO_BBSRAM + P1IO_BBSRAM_SIZE) {
        uint32_t start = reg - P1IO_BBSRAM;

        if (start + len > P1IO_BBSRAM_SIZE)
            return fw::device::RESP_ADDRESS_ERROR;

        memcpy(p1io->bbsram + start, buf, len);

        flush_bbsram(p1io);

        return fw::device::RESP_COMPLETE;
    }

    iris_debug(p1io, "Unhandled write to {:06x} ({} bytes)", reg, len);

    return fw::device::RESP_COMPLETE;
}

static int region_write_dev(Device* dev, uint32_t reg, const uint8_t* buf, int len) {
    P1io* p1io = from_device(dev);

    if (reg < P1IO_BBSRAM || reg >= P1IO_BBSRAM + P1IO_BBSRAM_SIZE) {
        uint32_t command = reg & 0xfff;

        int handled = -1;

        switch (command) {
            case P1IO_CMD_CF:
            case P1IO_CMD_ATA: {
                handled = cmd_storage(p1io, dev->host, command, buf, len);
            } break;

            case P1IO_CMD_BOOTROM: {
                handled = cmd_bootrom(p1io, dev->host, buf, len);
            } break;

            case P1IO_CMD_DALLAS: {
                handled = cmd_dallas(p1io, dev->host, buf, len);
            } break;

            case P1IO_CMD_ADPCM: {
                handled = cmd_adpcm(p1io, dev->host, buf, len);
            } break;

            case P1IO_CMD_BBSRAM: {
                handled = cmd_bbsram(p1io, dev->host, buf, len);
            } break;

            case P1IO_CMD_UART: {
                handled = cmd_uart(p1io, dev->host, buf, len);
            } break;

            case P1IO_CMD_FSCI: {
                handled = cmd_fsci(p1io, dev->host, buf, len);
            } break;

            case P1IO_CMD_JAMMA_INIT: {
                handled = cmd_jamma_init(p1io, dev->host, buf, len);
            } break;

            case P1IO_CMD_JAMMA_OUTPUT: {
                handled = cmd_jamma_output(p1io, dev->host, buf, len);
            } break;
        }

        if (handled >= 0) {
            return handled ? fw::device::RESP_COMPLETE : fw::device::RESP_ADDRESS_ERROR;
        }
    }

    return region_write(p1io, reg, buf, len);
}

static void put_quadlet(uint8_t* buf, uint32_t value) {
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)value;
}

static int read_config_rom(P1io* p1io, uint32_t start, uint8_t* buf, int len) {
    if (start + len > P1IO_CONFIG_ROM_SIZE)
        return fw::device::RESP_ADDRESS_ERROR;

    if (!p1io->config_rom_loaded)
        return read_default_config_rom(start, buf, len);

    memcpy(buf, p1io->config_rom + start, len);

    return fw::device::RESP_COMPLETE;
}

static int read(Device* dev, uint64_t offset, uint8_t* buf, int len) {
    P1io* p1io = from_device(dev);

    if (offset == P1IO_BOOT_READY && len == 4) {
        put_quadlet(buf, 0x01000000);

        return fw::device::RESP_COMPLETE;
    }

    if (offset == P1IO_RUNTIME_READY && len == 4) {
        put_quadlet(buf, 0);

        return fw::device::RESP_COMPLETE;
    }

    if ((offset & ~0x3ffull) == P1IO_CROM_BASE)
        return read_config_rom(p1io, (uint32_t)(offset & 0x3ff), buf, len);

    if ((offset >> 32) == P1IO_REGION_HIGH)
        return region_read(p1io, (uint32_t)offset, buf, len);

    if (offset >= P1IO_CSR_BASE)
        return region_read(p1io, (uint32_t)(offset - P1IO_CSR_BASE), buf, len);

    // iris_debug(p1io, "Unhandled read from {:012x} ({} bytes)", offset, len);

    return fw::device::RESP_ADDRESS_ERROR;
}

static int write(Device* dev, uint64_t offset, const uint8_t* buf, int len) {
    P1io* p1io = from_device(dev);

    if ((offset >> 32) == P1IO_REGION_HIGH)
        return region_write_dev(dev, (uint32_t)offset, buf, len);

    if (offset >= P1IO_CSR_BASE)
        return region_write_dev(dev, (uint32_t)(offset - P1IO_CSR_BASE), buf, len);

    return region_write_dev(dev, (uint32_t)offset, buf, len);
}

static void reset_device(Device* dev) {
    P1io* p1io = from_device(dev);

    p1io->jamma = 0;
    p1io->jamma_dest = P1IO_JAMMA_DEST_INVALID;

    memset(p1io->coins, 0, sizeof(p1io->coins));
}

static void free_device(Device* dev) {
    P1io* p1io = from_device(dev);

    flush_bbsram(p1io);

    if (p1io->cf_file) {
        fs::close_handle(p1io->cf_fs, p1io->cf_file);
    }

    if (p1io->cf_fs) {
        fs::close(p1io->cf_fs);
    }

    if (p1io->cf_blk) {
        fs::blk::close(p1io->cf_blk);
    }

    if (p1io->cf) {
        ata::disc::ata_close(p1io->cf);
    }

    if (p1io->hdd) {
        ata::disc::ata_close(p1io->hdd);
    }

    delete[] p1io->bootrom;
    delete p1io;
}

static const fw::device::Ops ops = {
    .read = read,
    .write = write,
    .reset = reset_device,
    .free = free_device,
};

void create(Device* dev) {
    P1io* p1io = new P1io();

    p1io->logger = dev->logger;
    p1io->logger_id = logger::register_source(dev->logger, "p1io");

    p1io->bootrom = new uint8_t[P1IO_BOOTROM_SIZE]();

    dev->connected = 1;
    dev->node_id = 1;
    dev->guid = 0;
    dev->ops = &ops;
    dev->priv = p1io;

    reset_device(dev);
}

}
