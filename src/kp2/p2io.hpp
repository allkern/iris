#pragma once

#include <cstdint>

#include "iop/usb/device.hpp"
#include "acio.hpp"
#include "thrilldrive.hpp"
#include "logger.hpp"

namespace iris::kp2::p2io {

inline constexpr auto DONGLE_SIZE = 40;
inline constexpr auto DONGLE_SERIAL_SIZE = 8;
inline constexpr auto DONGLE_MEMORY_SIZE = 32;
inline constexpr auto DONGLE_CRC_POLY = 0x8c;

inline constexpr auto DONGLE_BLACK = 0;
inline constexpr auto DONGLE_WHITE = 1;
inline constexpr auto DONGLE_COUNT = 2;

inline constexpr auto PORT_COM1 = 0;
inline constexpr auto PORT_COM2 = 1;
inline constexpr auto PORT_COUNT = 2;

inline constexpr auto ANALOG_COUNT = 3;

inline constexpr auto ANALOG_STEER = 0;
inline constexpr auto ANALOG_GAS = 1;
inline constexpr auto ANALOG_BRAKE = 2;

inline constexpr auto ANALOG_CENTER = 0x8000;
inline constexpr auto ANALOG_MAX = 0xffff;

enum {
    AXIS_STEER_LEFT,
    AXIS_STEER_RIGHT,
    AXIS_GAS,
    AXIS_BRAKE,
    AXIS_COUNT
};

enum {
    INPUT_DRUMMANIA = 0,
    INPUT_GUITARFREAKS,
    INPUT_DDR,
    INPUT_TOYS_MARCH,
    INPUT_THRILL_DRIVE,
    INPUT_DANCE_864
};

enum {
    CMD_GET_VERSION = 0x01,
    CMD_RESEND = 0x02,
    CMD_FWRITEMODE = 0x03,
    CMD_SET_WATCHDOG = 0x05,
    CMD_SET_AV_MASK = 0x22,
    CMD_GET_AV_REPORT = 0x23,
    CMD_LAMP_OUT = 0x24,
    CMD_DALLAS = 0x25,
    CMD_SEND_IR = 0x26,
    CMD_READ_DIPSWITCH = 0x27,
    CMD_GET_JAMMA_POR = 0x28,
    CMD_PORT_READ = 0x29,
    CMD_PORT_READ_POR = 0x2a,
    CMD_JAMMA_START = 0x2f,
    CMD_COIN_STOCK = 0x31,
    CMD_COIN_COUNTER = 0x32,
    CMD_COIN_BLOCKER = 0x33,
    CMD_COIN_COUNTER_OUT = 0x34,
    CMD_SCI_SETUP = 0x38,
    CMD_SCI_WRITE = 0x3a,
    CMD_SCI_READ = 0x3b
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


inline constexpr auto RESPONSE_MAX = 256;
inline constexpr auto WIRE_MAX = (RESPONSE_MAX * 2) + 1;

struct P2io {
    uint8_t dongle[DONGLE_COUNT][DONGLE_SIZE];
    int dongle_loaded[DONGLE_COUNT];
    int dongle_warned[DONGLE_COUNT];
    int requested_dongle;

    acio::Port port[PORT_COUNT];

    int input_type;

    thrilldrive::Handle thrilldrive_handle;
    thrilldrive::Belt thrilldrive_belt;

    uint32_t jamma;
    uint16_t analog[ANALOG_COUNT];
    float axis[AXIS_COUNT];
    uint16_t coins[2];

    uint8_t dip_switches;
    int force_31khz;

    uint8_t watchdog;
    uint32_t watchdog_state;

    uint8_t response[RESPONSE_MAX];
    int response_size;

    uint8_t wire[WIRE_MAX];
    int wire_size;
    int wire_read;

    uint8_t ctrl_buf[256];
    int ctrl_len;
    int ctrl_offset;
    int ctrl_dir_in;
    int ctrl_set_address;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

void create(usb::device::Device* dev);

P2io* from_device(usb::device::Device* dev);

int load_dongle(P2io* p2io, int which, const char* path);
void set_input_type(P2io* p2io, int type);
void set_dip_switches(P2io* p2io, uint8_t value);
void set_force_31khz(P2io* p2io, int enabled);
void press_switch(P2io* p2io, uint32_t mask);
void release_switch(P2io* p2io, uint32_t mask);
void set_analog(P2io* p2io, int channel, uint16_t value);
void set_axis(P2io* p2io, int axis, float value);
void insert_coin(P2io* p2io, int slot);

}
