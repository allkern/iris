#include <cstdio>
#include <cstring>

#include <fmt/format.h>

#include "p1io.hpp"

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

P1io* from_device(Device* dev) {
    return (P1io*)dev->priv;
}

void set_io_mode(P1io* p1io, int mode) {
    p1io->io_mode = mode;

    iris_info(p1io, "I/O mode {}", mode);
}

int load_config_rom(P1io* p1io, const char* path) {
    p1io->config_rom_loaded = load_blob(p1io, path, p1io->config_rom, P1IO_CONFIG_ROM_SIZE, "config ROM");

    return p1io->config_rom_loaded;
}

int load_bootrom(P1io* p1io, const char* path) {
    p1io->bootrom_loaded = load_blob(p1io, path, p1io->bootrom, P1IO_BOOTROM_SIZE, "boot ROM");

    return p1io->bootrom_loaded;
}

int load_dongle(P1io* p1io, int which, const char* path) {
    if (which < 0 || which >= DONGLE_COUNT)
        return 0;

    p1io->dongle_loaded[which] = load_blob(p1io, path, p1io->dongle[which], DONGLE_SIZE, "dongle");

    return p1io->dongle_loaded[which];
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

    if (reg >= P1IO_CONFIG_ROM && reg < P1IO_CONFIG_ROM + P1IO_CONFIG_ROM_SIZE) {
        uint32_t start = reg - P1IO_CONFIG_ROM;

        if (!p1io->config_rom_loaded || start + len > P1IO_CONFIG_ROM_SIZE)
            return fw::device::RESP_ADDRESS_ERROR;

        memcpy(buf, p1io->config_rom + start, len);

        return fw::device::RESP_COMPLETE;
    }

    iris_debug(p1io, "Unhandled read from {:06x} ({} bytes)", reg, len);

    memset(buf, 0, len);

    return fw::device::RESP_COMPLETE;
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

static int read(Device* dev, uint64_t offset, uint8_t* buf, int len) {
    P1io* p1io = from_device(dev);

    if (offset < P1IO_INITIAL_REGISTER_SPACE)
        return fw::device::RESP_ADDRESS_ERROR;

    return region_read(p1io, (uint32_t)(offset - P1IO_INITIAL_REGISTER_SPACE), buf, len);
}

static int write(Device* dev, uint64_t offset, const uint8_t* buf, int len) {
    P1io* p1io = from_device(dev);

    if (offset < P1IO_INITIAL_REGISTER_SPACE)
        return fw::device::RESP_ADDRESS_ERROR;

    return region_write(p1io, (uint32_t)(offset - P1IO_INITIAL_REGISTER_SPACE), buf, len);
}

static void reset_device(Device* dev) {
    P1io* p1io = from_device(dev);

    p1io->jamma = 0;

    memset(p1io->coins, 0, sizeof(p1io->coins));
}

static void free_device(Device* dev) {
    P1io* p1io = from_device(dev);

    flush_bbsram(p1io);

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
