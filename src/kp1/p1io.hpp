#pragma once

#include <cstdint>

#include "iop/fw/device.hpp"
#include "logger.hpp"

namespace iris::kp1::p1io {

inline constexpr uint64_t P1IO_INITIAL_REGISTER_SPACE = 0xffff'f000'0000ull;
inline constexpr uint32_t P1IO_CONFIG_ROM = 0x000400;
inline constexpr uint32_t P1IO_CONFIG_ROM_SIZE = 0x000400;
inline constexpr uint32_t P1IO_NET_CMD = 0x000180;
inline constexpr uint32_t P1IO_NET_CMD_STRIDE = 0x000020;
inline constexpr uint32_t P1IO_NET_RSP = 0x0b0000;
inline constexpr uint32_t P1IO_NET_RSP_STRIDE = 0x001000;
inline constexpr int P1IO_NET_CHANNELS = 8;
inline constexpr uint32_t P1IO_CMD_CF = 0x000390;
inline constexpr uint32_t P1IO_CMD_ATA = 0x0003a0;
inline constexpr uint32_t P1IO_CMD_SIZE = 0x000010;
inline constexpr uint32_t P1IO_DALLAS = 0x010000;
inline constexpr uint32_t P1IO_UART = 0x030000;
inline constexpr uint32_t P1IO_CF = 0x050000;
inline constexpr uint32_t P1IO_ATA = 0x060000;
inline constexpr uint32_t P1IO_ADPCM = 0x070000;
inline constexpr uint32_t P1IO_BBSRAM = 0x080000;
inline constexpr uint32_t P1IO_BOOTROM = 0x090000;
inline constexpr uint32_t P1IO_FSCI = 0x0a0000;
inline constexpr uint32_t P1IO_REGION_SIZE = 0x010000;
inline constexpr uint32_t P1IO_BBSRAM_SIZE = 0x002000;
inline constexpr uint32_t P1IO_BOOTROM_SIZE = 0x080000;
inline constexpr auto P1IO_DONGLE_SIZE = 40;

enum {
    DONGLE_INTERNAL = 0,
    DONGLE_EXTERNAL,
    DONGLE_COUNT
};

enum {
    IO_MODE_JVS = 0,
    IO_MODE_EXTIO,
    IO_MODE_POPN,
    IO_MODE_PPOOL,
    IO_MODE_B22,
    IO_MODE_DOGSTATIONDX
};

enum {
    JAMMA_P1_START = 0x00000100,
    JAMMA_P1_UP = 0x00000200,
    JAMMA_P1_DOWN = 0x00000400,
    JAMMA_P1_LEFT = 0x00000800,
    JAMMA_P1_RIGHT = 0x00001000,
    JAMMA_P1_BUTTON1 = 0x00002000,
    JAMMA_P1_BUTTON2 = 0x00004000,
    JAMMA_P1_BUTTON3 = 0x00008000,

    JAMMA_P2_START = 0x00010000,
    JAMMA_P2_UP = 0x00020000,
    JAMMA_P2_DOWN = 0x00040000,
    JAMMA_P2_LEFT = 0x00080000,
    JAMMA_P2_RIGHT = 0x00100000,
    JAMMA_P2_BUTTON1 = 0x00200000,
    JAMMA_P2_BUTTON2 = 0x00400000,
    JAMMA_P2_BUTTON3 = 0x00800000,

    JAMMA_TEST = 0x01000000,
    JAMMA_SERVICE = 0x02000000,
    JAMMA_COIN1 = 0x04000000,
    JAMMA_COIN2 = 0x08000000
};

struct P1io {
    int io_mode;

    uint8_t dongle[DONGLE_COUNT][DONGLE_SIZE];
    int dongle_loaded[DONGLE_COUNT];

    uint8_t config_rom[regs::CONFIG_ROM_SIZE];
    int config_rom_loaded;

    uint8_t* bootrom;
    int bootrom_loaded;

    uint8_t bbsram[regs::BBSRAM_SIZE];
    char bbsram_path[512];

    uint32_t jamma;
    uint16_t coins[2];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

void create(fw::device::Device* dev);

P1io* from_device(fw::device::Device* dev);

void set_io_mode(P1io* p1io, int mode);

int load_config_rom(P1io* p1io, const char* path);
int load_bootrom(P1io* p1io, const char* path);
int load_dongle(P1io* p1io, int which, const char* path);
int load_bbsram(P1io* p1io, const char* path);

void press_switch(P1io* p1io, uint32_t mask);
void release_switch(P1io* p1io, uint32_t mask);
void insert_coin(P1io* p1io, int slot);

}
