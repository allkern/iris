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

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "modchip.hpp"

namespace iris::modchip {

// First 24 words of the stage 0 sprintf patch, we use this to
// find modchip patches in BIOS dumps
inline constexpr uint32_t CHIP_STUB_SIGNATURE[] = {
    0x03e0382d, 0x3c080010, 0x0411001d, 0x3404a000,
    0x3c090001, 0x240a002d, 0x0000682d, 0x04110001,
    0x3404a103, 0x27ec0004, 0x04110015, 0x00892025,
    0x004a1826, 0xa1030000, 0x394a00a7, 0x0148502d,
    0x25080001, 0x31040400, 0x14800003, 0x01a26826,
    0x01800008, 0x3404a100, 0x04110009, 0x3404a001
};

// Our own version of the chip patch (so we do modchip emulation on
// BIOSes without modchip patches)
inline constexpr uint32_t IRIS_STUB[] = {
    0x03e0382d, 0x3c080010, 0x3c090001, 0x240a002d,
    0x0000682d, 0x04110017, 0x3404a000, 0x3524a103,
    0x04110014, 0x00000000, 0x004a1826, 0xa1030000,
    0x394a00a7, 0x0148502d, 0x25080001, 0x31030400,
    0x01a26826, 0x1060fff6, 0x3524a100, 0x04110009,
    0x3404a001, 0x15a0ffeb, 0x00000000, 0x0c000ab0,
    0x00000000, 0x0c000aa0, 0x00000000, 0x0c040008,
    0x00000000, 0x00043402, 0x3c011fc0, 0x2402004d,
    0xa0220000, 0x00041202, 0xa0220000, 0xa0240000,
    0x942506fa, 0xdc2206f8, 0x10c00005, 0x0002243e,
    0x10850003, 0x00000000, 0x1000fff4, 0x3404a300,
    0x03e00008, 0x00021602
};

// Used to find sprintf on BIOS dumps
inline constexpr uint32_t SPRINTF_PROLOGUE[] = {
    0x27bdfe80, 0xffb600f0, 0xffb500e0, 0x0080b02d,
    0xffb400d0, 0x0000a82d, 0xffb300c0, 0x27b40150,
    0xffbf0100, 0x00a0982d, 0xffb200b0, 0xffb100a0,
    0xffb00090, 0xffa60150, 0xffa70158, 0xffa80160,
    0xffa90168, 0xffaa0170, 0xffab0178, 0xe7ac0140,
    0xe7ae0144, 0xe7b00148, 0xe7b2014c, 0x82660000,
    0x10c0014a, 0x24020025, 0x14c2011e, 0x02d51021,
    0x3c028002, 0x27a40080, 0x24470000, 0x68e30007,
    0x6ce30000, 0x8ce50008, 0xb3a30087, 0xb7a30080,
    0xafa50088, 0x240d002d, 0x24050001, 0x240c002b,
    0x240b0002, 0x240a0020, 0x24090023, 0x24080004,
    0x24070030, 0x24030008, 0x26730001, 0x82660000,
    0x14cd0004, 0xdfa20080, 0x00451025, 0x1000fffa,
    0xffa20080, 0x14cc0003, 0x004b1025, 0x1000fff6,
    0xffa20080, 0x14ca0003
};

inline constexpr size_t SPRINTF_INIT_ADDIU = 30;
inline constexpr size_t SPRINTF_INIT_LDL = 31;
inline constexpr size_t SPRINTF_INIT_LDR = 32;
inline constexpr size_t SPRINTF_INIT_LW = 33;

inline constexpr uint32_t MOVE_V1_ZERO = 0x0000182d;
inline constexpr uint32_t MOVE_A1_ZERO = 0x0000282d;
inline constexpr uint32_t NOP = 0x00000000;

struct InitAddress {
    const char* version;
    const char* date;
    uint16_t low;
};

inline constexpr InitAddress SPRINTF_INIT_ADDRESSES[] = {
    { "0100", "20000117", 0x0d10 },
    { "0100", "20000306", 0x1198 },
    { "0101", "20000217", 0x0d10 },
    { "0110", "20000727", 0x1458 },
    { "0120", "20000902", 0x14d8 },
    { "0120", "20001027", 0x14d8 },
    { "0150", "20001228", 0x3398 },
    { "0150", "20010118", 0x3398 },
    { "0160", "20010427", 0x3458 },
    { "0160", "20010704", 0x3f18 },
    { "0160", "20010730", 0x3f18 },
    { "0160", "20011004", 0x3f18 },
    { "0160", "20020207", 0x3e18 },
    { "0160", "20020319", 0x3c18 },
    { "0160", "20020426", 0x3c68 },
    { "0170", "20030206", 0x3fe8 },
    { "0170", "20030227", 0x3fe8 },
    { "0170", "20030325", 0x3fe8 },
    { "0180", "20031028", 0x53a8 },
    { "0190", "20030623", 0x3fe8 },
    { "0190", "20030822", 0x3fe8 },
    { "0200", "20040614", 0x3fe8 },
    { "0210", "20040917", 0x53a8 },
    { "0220", "20050620", 0x45a8 },
    { "0220", "20060210", 0x45a8 },
    { "0220", "20060905", 0x45a8 },
    { "0230", "20080220", 0x45a8 }
};

inline constexpr size_t ROMDIR_ENTRY_SIZE = 16;
inline constexpr size_t ROMDIR_NAME_SIZE = 10;
inline constexpr size_t ROMVER_SIZE = 14;

static const uint8_t* find_romver(const uint8_t* rom, size_t size) {
    static const char reset[] = "RESET\0\0\0\0\0";

    const uint8_t* entry = nullptr;

    for (size_t i = 0; i + ROMDIR_ENTRY_SIZE <= size; i += ROMDIR_ENTRY_SIZE) {
        if (!memcmp(rom + i, reset, ROMDIR_NAME_SIZE)) {
            entry = rom + i;

            break;
        }
    }

    if (!entry) {
        return nullptr;
    }

    size_t offset = 0;

    while (entry + ROMDIR_ENTRY_SIZE <= rom + size && entry[0]) {
        uint32_t entry_size;

        memcpy(&entry_size, entry + 12, sizeof(entry_size));

        if (!memcmp(entry, "ROMVER\0\0\0\0", ROMDIR_NAME_SIZE)) {
            if (offset + ROMVER_SIZE > size) {
                return nullptr;
            }

            return rom + offset;
        }

        offset += (entry_size + 15) & ~15u;
        entry += ROMDIR_ENTRY_SIZE;
    }

    return nullptr;
}

static const InitAddress* find_init_address(const uint8_t* romver) {
    if (!romver) {
        return nullptr;
    }

    for (const InitAddress& address : SPRINTF_INIT_ADDRESSES) {
        bool version_matches = !memcmp(romver, address.version, 4);
        bool date_matches = !memcmp(romver + 6, address.date, 8);

        if (version_matches && date_matches) {
            return &address;
        }
    }

    return nullptr;
}

enum class Site {
    NONE,
    SPRINTF,
    CHIP_STUB,
    IRIS_STUB
};

static void restore_sprintf(uint8_t* stub, const InitAddress* address) {
    uint32_t prologue[sizeof(SPRINTF_PROLOGUE) / sizeof(uint32_t)];

    memcpy(prologue, SPRINTF_PROLOGUE, sizeof(prologue));

    if (address) {
        prologue[SPRINTF_INIT_ADDIU] |= address->low;
    } else {
        prologue[SPRINTF_INIT_LDL] = MOVE_V1_ZERO;
        prologue[SPRINTF_INIT_LDR] = NOP;
        prologue[SPRINTF_INIT_LW] = MOVE_A1_ZERO;
    }

    memcpy(stub, prologue, sizeof(prologue));
}

static bool matches_sprintf(const uint8_t* code) {
    uint32_t words[sizeof(SPRINTF_PROLOGUE) / sizeof(uint32_t)];

    memcpy(words, code, sizeof(words));

    words[SPRINTF_INIT_ADDIU] &= 0xffff0000;

    return !memcmp(words, SPRINTF_PROLOGUE, sizeof(words));
}

static Site classify(const uint8_t* code) {
    if (!memcmp(code, CHIP_STUB_SIGNATURE, sizeof(CHIP_STUB_SIGNATURE))) {
        return Site::CHIP_STUB;
    }

    if (!memcmp(code, IRIS_STUB, sizeof(IRIS_STUB))) {
        return Site::IRIS_STUB;
    }

    if (matches_sprintf(code)) {
        return Site::SPRINTF;
    }

    return Site::NONE;
}

static size_t find_site(const bios::Bios* bios, Site* site) {
    size_t size = bios->size + 1;

    for (size_t i = 0; i + sizeof(SPRINTF_PROLOGUE) <= size; i += 4) {
        Site found = classify(bios->buf + i);

        if (found != Site::NONE) {
            *site = found;

            return i;
        }
    }

    *site = Site::NONE;

    return 0;
}

bool unpatch_bios(bios::Bios* bios) {
    size_t size = bios->size + 1;

    const uint8_t* romver = find_romver(bios->buf, size);
    const InitAddress* address = find_init_address(romver);

    bool patched = false;

    for (size_t i = 0; i + sizeof(SPRINTF_PROLOGUE) <= size; i += 4) {
        Site site = classify(bios->buf + i);

        if (site != Site::CHIP_STUB && site != Site::IRIS_STUB) {
            continue;
        }

        restore_sprintf(bios->buf + i, address);

        // if (site == Site::IRIS_STUB) {
        //     iris_debug(bios, "Removed the modchip patch at {:06x}", i);
        // } else if (address) {
        //     iris_warning(bios, "restored sprintf at {:06x}", i);
        // } else {
        //     iris_warning(bios, "restored sprintf at {:06x} (unknown ROMVER, using zero init)", i);
        // }

        patched = true;
    }

    return patched;
}

bool install_stub(bios::Bios* bios) {
    if (!find_romver(bios->buf, bios->size + 1)) {
        return false;
    }

    Site site;

    size_t offset = find_site(bios, &site);

    if (site == Site::NONE) {
        iris_warning(bios, "Couldn't find the kernel's sprintf");

        return false;
    }

    memcpy(bios->buf + offset, IRIS_STUB, sizeof(IRIS_STUB));

    iris_info(bios, "Patched sprintf at {:06x}", offset);

    return true;
}

inline constexpr uint8_t COMMAND_MAGIC = 'M';
inline constexpr uint8_t CHIP_VERSION = 0x10;
inline constexpr uint8_t MISO_IDLE = 0xff;
inline constexpr uint8_t REPLY_SELECTED = 0x00;
inline constexpr uint8_t REPLY_DESELECTED = 0xff;
inline constexpr uint8_t REPLY_ACK = 0x00;

inline constexpr uint8_t GROUP_SELECT = 0xa0;
inline constexpr uint8_t GROUP_TRANSFER = 0xa1;
inline constexpr uint8_t GROUP_WRITE = 0xa2;
inline constexpr uint8_t GROUP_SEND = 0xa9;
inline constexpr uint8_t GROUP_REPEAT = 0xa3;
inline constexpr uint8_t GROUP_VERSION = 0xa4;
inline constexpr uint8_t GROUP_MODE = 0xae;
inline constexpr uint8_t GROUP_STREAM_RESET = 0xa5;
inline constexpr uint8_t GROUP_STREAM_READ = 0xa6;

inline constexpr uint8_t CLOCK_SINGLE = 0xa7;
inline constexpr uint8_t CLOCK_DOUBLE = 0xac;

inline constexpr uint8_t STREAM_SEED[STREAM_ORDER] = {
    0x8c, 0x8f, 0x0d, 0xc3, 0xde, 0x43, 0x18, 0xd3,
    0x85, 0x49, 0xf1, 0xa6, 0x46, 0x5f, 0x81, 0xfa,
    0x51, 0x8c, 0x1a, 0x8a, 0x49, 0x1b
};

inline constexpr int STREAM_TAPS[] = {
    1, 2, 3, 6, 7, 8, 9, 13, 14, 16, 17, 18, 19, 22
};

inline constexpr uint8_t SELECT_ASSERT = 0x00;

inline constexpr uint8_t SPI_WRITE_STATUS = 0x01;
inline constexpr uint8_t SPI_PAGE_PROGRAM = 0x02;
inline constexpr uint8_t SPI_READ = 0x03;
inline constexpr uint8_t SPI_WRITE_DISABLE = 0x04;
inline constexpr uint8_t SPI_READ_STATUS = 0x05;
inline constexpr uint8_t SPI_WRITE_ENABLE = 0x06;
inline constexpr uint8_t SPI_FAST_READ = 0x0b;
inline constexpr uint8_t SPI_SECTOR_ERASE = 0x20;
inline constexpr uint8_t SPI_CHIP_ERASE = 0x60;
inline constexpr uint8_t SPI_PAGE_ERASE = 0x81;
inline constexpr uint8_t SPI_READ_ID = 0xab;
inline constexpr uint8_t SPI_CHIP_ERASE_ALT = 0xc7;
inline constexpr uint8_t SPI_SECTOR_ERASE_ALT = 0xd7;
inline constexpr uint8_t SPI_BLOCK_ERASE = 0xd8;
inline constexpr int SPI_ADDRESS_BYTES = 3;
inline constexpr int SPI_ID_DUMMY_BYTES = 3;

inline constexpr uint8_t STATUS_WRITE_ENABLED = 0x02;
inline constexpr uint8_t STATUS_WRITABLE_BITS = 0x9c;

inline constexpr uint32_t FLASH_PAGE_SIZE = 0x100;
inline constexpr uint32_t FLASH_SECTOR_SIZE = 0x1000;
inline constexpr uint32_t FLASH_BLOCK_SIZE = 0x10000;
inline constexpr uint8_t FLASH_ERASED = 0xff;

inline constexpr uint8_t FLASH_MANUFACTURER_ID = 0x9d;
inline constexpr uint8_t FLASH_DEVICE_ID_64K = 0x7b;
inline constexpr uint8_t FLASH_CONTINUATION_ID = 0x7f;

inline constexpr uint32_t REPLY_WINDOW = 0x6f8;
inline constexpr int REPLY_DATA_SHIFT = 24;
inline constexpr uint64_t REPLY_VALID_BIT = 1ull << 48;

inline constexpr long MAX_FLASH_SIZE = 0x1000000;

enum {
    PARSE_IDLE,
    PARSE_HI,
    PARSE_LO
};

Modchip* create(logger::Logger* logger, bios::Bios* bios) {
    Modchip* chip = new Modchip();

    chip->hw.bios = bios;
    chip->logger = logger;
    chip->logger_id = logger::register_source(logger, "modchip");

    return chip;
}

bool load_flash(Modchip* chip, const char* path) {
    FILE* file = fopen(path, "rb");

    if (!file) {
        iris_error(chip, "Couldn't open flash image '{}'", path);

        return false;
    }

    fseek(file, 0, SEEK_END);

    long size = ftell(file);

    fseek(file, 0, SEEK_SET);

    if (size <= 0 || size > MAX_FLASH_SIZE) {
        iris_error(chip, "'{}' is likely not a flash image ({} bytes)", path, size);

        fclose(file);

        return false;
    }

    uint8_t* flash = new uint8_t[size];

    bool ok = fread(flash, 1, size, file) == (size_t)size;

    fclose(file);

    if (!ok) {
        iris_error(chip, "Couldn't read flash image '{}'", path);

        delete[] flash;

        return false;
    }

    unload_flash(chip);

    size_t path_size = strlen(path) + 1;

    chip->flash = flash;
    chip->flash_size = size;
    chip->flash_path = new char[path_size];

    memcpy(chip->flash_path, path, path_size);

    iris_info(chip, "Loaded {} KiB flash image '{}'", size / 1024, path);

    return true;
}

void unload_flash(Modchip* chip) {
    delete[] chip->flash;
    delete[] chip->flash_path;

    chip->flash = nullptr;
    chip->flash_size = 0;
    chip->flash_path = nullptr;
    chip->flash_status = 0;
}

bool is_enabled(const Modchip* chip) {
    return chip && chip->flash;
}

void reset(Modchip* chip) {
    auto hw = chip->hw;
    uint8_t* flash = chip->flash;
    size_t flash_size = chip->flash_size;
    char* flash_path = chip->flash_path;
    uint8_t flash_status = chip->flash_status;
    logger::Logger* logger = chip->logger;
    size_t logger_id = chip->logger_id;

    new (chip) Modchip();

    chip->hw = hw;
    chip->flash = flash;
    chip->flash_size = flash_size;
    chip->flash_path = flash_path;
    chip->flash_status = flash_status;
    chip->logger = logger;
    chip->logger_id = logger_id;
}

void destroy(Modchip* chip) {
    unload_flash(chip);

    delete chip;
}

static uint8_t read_flash(Modchip* chip) {
    uint8_t data = chip->flash[chip->spi_address % chip->flash_size];

    chip->spi_address++;

    return data;
}

static uint8_t device_id(const Modchip* chip) {
    uint8_t id = FLASH_DEVICE_ID_64K;

    for (size_t size = FLASH_BLOCK_SIZE; size < chip->flash_size; size <<= 1) {
        id++;
    }

    return id;
}

static uint8_t read_id(const Modchip* chip, int index) {
    switch (index % 3) {
        case 0: return FLASH_MANUFACTURER_ID;
        case 1: return device_id(chip);
    }

    return FLASH_CONTINUATION_ID;
}

static void save_flash(Modchip* chip) {
    chip->flash_dirty = false;

    FILE* file = fopen(chip->flash_path, "wb");

    if (!file) {
        iris_error(chip, "Couldn't open flash image '{}' for writing", chip->flash_path);

        return;
    }

    bool ok = fwrite(chip->flash, 1, chip->flash_size, file) == chip->flash_size;

    fclose(file);

    if (!ok) {
        iris_error(chip, "Couldn't write flash image '{}'", chip->flash_path);

        return;
    }

    iris_info(chip, "Saved flash image '{}'", chip->flash_path);
}

static void erase(Modchip* chip, size_t size) {
    if (!chip->write_enabled) {
        return;
    }

    size_t start = (chip->spi_address & ~(size - 1)) % chip->flash_size;
    size_t length = std::min(size, chip->flash_size - start);

    memset(chip->flash + start, FLASH_ERASED, length);

    chip->flash_dirty = true;
}

static void program(Modchip* chip, int offset, uint8_t data) {
    if (!chip->write_enabled) {
        return;
    }

    uint32_t page = chip->spi_address & ~(FLASH_PAGE_SIZE - 1);
    uint32_t address = page | ((chip->spi_address + offset) & (FLASH_PAGE_SIZE - 1));

    chip->flash[address % chip->flash_size] &= data;
    chip->flash_dirty = true;
}

static bool is_write_command(uint8_t command) {
    switch (command) {
        case SPI_WRITE_STATUS:
        case SPI_PAGE_PROGRAM:
        case SPI_SECTOR_ERASE:
        case SPI_CHIP_ERASE:
        case SPI_PAGE_ERASE:
        case SPI_CHIP_ERASE_ALT:
        case SPI_SECTOR_ERASE_ALT:
        case SPI_BLOCK_ERASE: {
            return true;
        }
    }

    return false;
}

static size_t erase_size(uint8_t command) {
    switch (command) {
        case SPI_PAGE_ERASE: {
            return FLASH_PAGE_SIZE;
        }

        case SPI_SECTOR_ERASE:
        case SPI_SECTOR_ERASE_ALT: {
            return FLASH_SECTOR_SIZE;
        }
    }

    return FLASH_BLOCK_SIZE;
}

static void begin_command(Modchip* chip) {
    switch (chip->spi_command) {
        case SPI_WRITE_ENABLE: {
            chip->write_enabled = true;
        } break;

        case SPI_WRITE_DISABLE: {
            chip->write_enabled = false;
        } break;

        case SPI_CHIP_ERASE:
        case SPI_CHIP_ERASE_ALT: {
            chip->spi_address = 0;

            erase(chip, chip->flash_size);
        } break;
    }
}

static void end_command(Modchip* chip) {
    if (chip->spi_count && is_write_command(chip->spi_command)) {
        chip->write_enabled = false;
    }

    if (chip->flash_dirty) {
        save_flash(chip);
    }
}

static uint32_t shift_address(Modchip* chip, uint8_t data) {
    chip->spi_address = (chip->spi_address << 8) | data;

    return chip->spi_address;
}

static uint8_t transfer(Modchip* chip, uint8_t data) {
    if (!chip->selected) {
        return MISO_IDLE;
    }

    chip->spi_count++;

    if (chip->spi_count == 1) {
        chip->spi_command = data;
        chip->spi_address = 0;

        begin_command(chip);

        return MISO_IDLE;
    }

    int payload = chip->spi_count - 2;

    switch (chip->spi_command) {
        case SPI_READ:
        case SPI_FAST_READ: {
            if (payload < SPI_ADDRESS_BYTES) {
                shift_address(chip, data);

                return MISO_IDLE;
            }

            if (chip->spi_command == SPI_FAST_READ && payload == SPI_ADDRESS_BYTES) {
                return MISO_IDLE;
            }

            // iris_debug(chip, "spi {} {}", chip->spi_command == SPI_READ ? "read" : "fast read", chip->spi_address);

            return read_flash(chip);
        }

        case SPI_READ_STATUS: {
            // iris_debug(chip, "read status");

            return chip->flash_status | (chip->write_enabled ? STATUS_WRITE_ENABLED : 0);
        }

        case SPI_WRITE_STATUS: {
            // iris_debug(chip, "write status data={}", data);

            if (payload == 0 && chip->write_enabled) {
                chip->flash_status = data & STATUS_WRITABLE_BITS;
            }

            return MISO_IDLE;
        }

        case SPI_PAGE_PROGRAM: {
            if (payload < SPI_ADDRESS_BYTES) {
                shift_address(chip, data);

                return MISO_IDLE;
            }

            // iris_debug(chip, "page program addr={}", chip->spi_address);

            program(chip, payload - SPI_ADDRESS_BYTES, data);

            return MISO_IDLE;
        }

        case SPI_PAGE_ERASE:
        case SPI_SECTOR_ERASE:
        case SPI_SECTOR_ERASE_ALT:
        case SPI_BLOCK_ERASE: {
            if (payload < SPI_ADDRESS_BYTES) {
                shift_address(chip, data);
            }

            if (payload == SPI_ADDRESS_BYTES - 1) {
                // iris_debug(chip, "erase addr={}", chip->spi_address);

                erase(chip, erase_size(chip->spi_command));
            }

            return MISO_IDLE;
        }

        case SPI_READ_ID: {
            if (payload < SPI_ID_DUMMY_BYTES) {
                return MISO_IDLE;
            }

            // iris_debug(chip, "read id");

            return read_id(chip, payload - SPI_ID_DUMMY_BYTES);
        }
    }

    return MISO_IDLE;
}

static void step_stream(uint8_t* state) {
    uint8_t next = 0;

    for (int tap : STREAM_TAPS) {
        next ^= state[STREAM_ORDER - tap];
    }

    memmove(state, state + 1, STREAM_ORDER - 1);

    state[STREAM_ORDER - 1] = next;
}

static void reset_stream(Modchip* chip) {
    memcpy(chip->stream_state, STREAM_SEED, STREAM_ORDER);

    chip->stream_block_valid = false;
    chip->stream_index = 0;
}

static void clock_stream(Modchip* chip) {
    step_stream(chip->stream_state);

    chip->stream_block_valid = false;
    chip->stream_index = 0;
}

static uint8_t read_stream(Modchip* chip) {
    if (!chip->stream_block_valid) {
        uint8_t state[STREAM_ORDER];

        memcpy(state, chip->stream_state, STREAM_ORDER);

        for (int i = 0; i < STREAM_BLOCK_SIZE; i++) {
            chip->stream_block[i] = state[0];

            step_stream(state);
        }

        chip->stream_block_valid = true;
    }

    uint8_t data = chip->stream_block[chip->stream_index];

    chip->stream_index = (chip->stream_index + 1) % STREAM_BLOCK_SIZE;

    return data;
}

static void execute(Modchip* chip, uint16_t command) {
    uint8_t group = command >> 8;
    uint8_t operand = command & 0xff;

    switch (group) {
        case GROUP_SELECT: {
            if (operand == SELECT_ASSERT) {
                chip->selected = true;
                chip->spi_count = 0;
                chip->reply = REPLY_SELECTED;
            } else {
                if (chip->selected) {
                    end_command(chip);
                }

                chip->selected = false;
                chip->reply = REPLY_DESELECTED;
            }
        } break;

        case GROUP_TRANSFER:
        case GROUP_WRITE:
        case GROUP_SEND: {
            chip->reply = transfer(chip, operand);
        } break;

        case GROUP_REPEAT: {
        } break;

        case GROUP_VERSION: {
            chip->reply = CHIP_VERSION;
        } break;

        case GROUP_MODE: {
            chip->mode = operand;
            chip->reply = REPLY_ACK;

            // iris_debug(chip, "Mode {:02x}", operand);
        } break;

        case GROUP_STREAM_RESET: {
            reset_stream(chip);

            chip->reply = REPLY_ACK;
        } break;

        case GROUP_STREAM_READ: {
            chip->reply = read_stream(chip);
        } break;

        default: {
            chip->reply = REPLY_ACK;

            iris_debug(chip, "Unknown command {:04x}", command);
        } break;
    }

    chip->reply_pending = true;
}

static void receive(Modchip* chip, uint8_t data) {
    chip->clock_armed = false;

    switch (chip->parse_state) {
        case PARSE_IDLE: {
            if (data == COMMAND_MAGIC) {
                chip->parse_state = PARSE_HI;
            }
        } break;

        case PARSE_HI: {
            if (data == CLOCK_SINGLE || data == CLOCK_DOUBLE) {
                chip->clock_armed = true;
                chip->parse_state = PARSE_IDLE;

                break;
            }

            chip->command_hi = data;
            chip->parse_state = PARSE_LO;
        } break;

        case PARSE_LO: {
            chip->parse_state = PARSE_IDLE;

            execute(chip, (chip->command_hi << 8) | data);
        } break;
    }
}

static void receive_words(Modchip* chip, const uint8_t* data, int words) {
    for (int i = 0; i < words; i++) {
        receive(chip, data[i * 4 + 3]);
    }
}

static void observe_read(Modchip* chip) {
    if (chip->clock_armed) {
        clock_stream(chip);
    }
}

uint64_t read8(Modchip* chip, uint32_t addr) {
    observe_read(chip);

    return bios::read8(chip->hw.bios, addr);
}

uint64_t read16(Modchip* chip, uint32_t addr) {
    observe_read(chip);

    return bios::read16(chip->hw.bios, addr);
}

uint64_t read32(Modchip* chip, uint32_t addr) {
    observe_read(chip);

    return bios::read32(chip->hw.bios, addr);
}

uint64_t read64(Modchip* chip, uint32_t addr) {
    observe_read(chip);

    uint64_t value = bios::read64(chip->hw.bios, addr);

    if (!is_enabled(chip) || !chip->reply_pending || addr != REPLY_WINDOW) {
        return value;
    }

    uint64_t pulled_low = (uint64_t)(uint8_t)~chip->reply << REPLY_DATA_SHIFT;

    value &= ~pulled_low;
    value &= ~REPLY_VALID_BIT;

    chip->reply_pending = false;

    return value;
}

uint128_t read128(Modchip* chip, uint32_t addr) {
    observe_read(chip);

    return bios::read128(chip->hw.bios, addr);
}

void write8(Modchip* chip, uint32_t addr, uint64_t data) {
    if (!is_enabled(chip)) {
        return;
    }

    receive(chip, data & 0xff);
}

void write16(Modchip* chip, uint32_t addr, uint64_t data) {
    if (!is_enabled(chip)) {
        return;
    }

    receive(chip, (data >> 8) & 0xff);
}

void write32(Modchip* chip, uint32_t addr, uint64_t data) {
    if (!is_enabled(chip)) {
        return;
    }

    receive(chip, (data >> 24) & 0xff);
}

void write64(Modchip* chip, uint32_t addr, uint64_t data) {
    if (!is_enabled(chip)) {
        return;
    }

    uint8_t bytes[8];

    memcpy(bytes, &data, sizeof(bytes));

    receive_words(chip, bytes, 2);
}

void write128(Modchip* chip, uint32_t addr, uint128_t data) {
    if (!is_enabled(chip)) {
        return;
    }

    receive_words(chip, data.u8, 4);
}

}
