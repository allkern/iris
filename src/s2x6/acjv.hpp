#pragma once

#include "scheduler.hpp"
#include "logger.hpp"

namespace iris::s2x6::acjv {

inline constexpr auto BASE_ADDR = 0x12400000;
inline constexpr auto ADDR_SIZE = 0x8000;
inline constexpr auto FRAME_SIZE = 0x300;
inline constexpr auto RD_BASE = 0x12404000;
inline constexpr auto WR_BASE = 0x12404600;
inline constexpr auto WR_LAST = WR_BASE + ((FRAME_SIZE - 1) * 2);
inline constexpr auto FRAME_MAGIC = 0x3e6f;
inline constexpr auto REQ_MAGIC = 0x00;
inline constexpr auto REQ_ROOT_PACKET_ID = 0x10;
inline constexpr auto REQ_PACKET_ID = 0x18;
inline constexpr auto REQ_TOKEN = 0x1a;
inline constexpr auto REQ_JVS = 0x22;
inline constexpr auto REQ_JVS_AUX = 0x122;
inline constexpr auto RES_MAGIC = 0x00;
inline constexpr auto RES_FIRMWARE_VERSION = 0x02;
inline constexpr auto RES_ROOT_PACKET_ID = 0x28;
inline constexpr auto RES_LOADER_PACE = 0x2a;
inline constexpr auto RES_DIP_SWITCHES = 0x30;
inline constexpr auto RES_PACKET_ID = 0x40;
inline constexpr auto RES_TOKEN = 0x42;
inline constexpr auto RES_FRAME_COUNT = 0x57;
inline constexpr auto RES_JVS = 0x5a;
inline constexpr auto RES_JVS_AUX = 0x15a;
inline constexpr auto FIRMWARE_VERSION = 0x208;
inline constexpr auto LOADER_PACE = 0x5210;
inline constexpr auto JVS_SYNC = 0xe0;
inline constexpr auto JVS_MASTER_ADDRESS = 0x00;
inline constexpr auto JVS_STATUS_OK = 0x01;
inline constexpr auto JVS_STATUS_UNKNOWN_COMMAND = 0x02;
inline constexpr auto JVS_REPORT_OK = 0x01;
inline constexpr auto JVS_CMD_REVISION = 0x13;
inline constexpr auto JVS_REVISION = 0x30;
inline constexpr auto JVS_COMM_VERSION = 0x10;
inline constexpr auto PLAYER_COUNT = 2;

enum {
    JVS_SET_ADDRESS = 0xf1,
    JVS_RESET = 0xf0,
    JVS_READ_ID = 0x10,
    JVS_GET_CMD_REVISION = 0x11,
    JVS_GET_JVS_REVISION = 0x12,
    JVS_GET_COMM_VERSION = 0x13,
    JVS_GET_FEATURES = 0x14,
    JVS_SET_MAIN_ID = 0x15,
    JVS_READ_SWITCHES = 0x20,
    JVS_READ_COINS = 0x21,
    JVS_READ_ANALOG = 0x22,
    JVS_READ_SCREEN_POSITION = 0x25,
    JVS_DECREASE_COINS = 0x30,
    JVS_GENERAL_OUTPUT = 0x32,
    JVS_INCREASE_COINS = 0x35,
    JVS_NAMCO_VENDOR = 0x70
};

enum {
    FEATURE_END = 0x00,
    FEATURE_SWITCH_INPUT = 0x01,
    FEATURE_COIN_INPUT = 0x02,
    FEATURE_ANALOG_INPUT = 0x03,
    FEATURE_SCREEN_POSITION = 0x06,
    FEATURE_GENERAL_OUTPUT = 0x12
};

inline constexpr auto SWITCHES_PER_PLAYER = 16;

inline constexpr auto GUN_BITS = 0x10;
inline constexpr auto GUN_RANGE = 0xffff;
inline constexpr auto GUN_OFF_SCREEN = 0x0000;
inline constexpr auto GUN_ANALOG_CHANNELS = 2;
inline constexpr auto GENERAL_OUTPUT_SLOTS = 0x10;

inline constexpr auto GUN_DEFAULT_TRIGGER = 0x0001;
inline constexpr auto GUN_DEFAULT_PEDAL = 0x8000;

inline constexpr auto GUN_LOST = 0xffff;
inline constexpr auto GUN_TWO_TIER_WIDTH = 640.0f;
inline constexpr auto GUN_TWO_TIER_HEIGHT = 448.0f;
inline constexpr auto GUN_TWO_TIER_LOST = 0.35f;
inline constexpr auto GUN_EDGE_MARGIN = 0.01f;
inline constexpr auto GUN_CAMERA_VISIBLE_X = 230.0f / 320.0f;
inline constexpr auto GUN_CAMERA_VISIBLE_Y = 140.0f / 224.0f;
inline constexpr auto GUN_CAMERA_SAMPLER_GAP = 0x7fff;
inline constexpr auto TOUCH_MAX = 0xfffe;
inline constexpr auto TOUCH_CHANNELS = 1;

inline constexpr auto BOARD_ID_RAYS = "namco ltd.;RAYS PCB;";
inline constexpr auto BOARD_ID_MIU = "namco ltd.;MIU-I/O;Ver2.05;JPN,GUN-EXTENTION";
inline constexpr auto BOARD_ID_TSS = "namco ltd.;TSS-I/O;Ver2.11;GUN-EXTENTION";

enum {
    GUN_BOARD_CLASSIC,
    GUN_BOARD_TWO_TIER,
    GUN_BOARD_SIDE_SWITCH,
    GUN_BOARD_CAMERA
};

struct Gun {
    float x;
    float y;
    int aimed_away;
    int touching;
};

enum {
    MODE_DEFAULT,
    MODE_DRIVE,
    MODE_FCA,
    MODE_LIGHTGUN,
    MODE_TOUCH
};

inline constexpr auto FCA_HEARTBEAT = 0x0e;
inline constexpr auto FCA_BUTTONS = 0x40;
inline constexpr auto FCA_STEER = 0x80;
inline constexpr auto FCA_GAS = 0x82;
inline constexpr auto FCA_BRAKE = 0x84;
inline constexpr auto FCA_COIN = 0xc0;
inline constexpr auto FCA_TEST = 0xe2;
inline constexpr auto FCA_STEER_RANGE = 0x6400;
inline constexpr auto FCA_PEDAL_MAX = 0x5800;
inline constexpr auto FCA_INTERVAL = 4915200;
inline constexpr auto WHEEL_CHANNELS = 3;
inline constexpr auto WHEEL_BITS = 16;

enum {
    WHEEL_STANDARD,
    WHEEL_WANGAN
};

inline constexpr auto WHEEL_CENTER = 0x8000;
inline constexpr auto WHEEL_WANGAN_RANGE = 0x7e00;
inline constexpr auto WHEEL_WANGAN_PEDAL_MAX = 0x7fff;

enum {
    AXIS_STEER_LEFT,
    AXIS_STEER_RIGHT,
    AXIS_GAS,
    AXIS_BRAKE,
    AXIS_COUNT
};

enum {
    DIP_TEST = 0x80,
    DIP_VIDEO_VOLTAGE = 0x40,
    DIP_MONITOR_FREQUENCY = 0x20,
    DIP_VIDEO_SYNC = 0x10
};

inline constexpr auto DIP_DEFAULT = DIP_VIDEO_VOLTAGE | DIP_MONITOR_FREQUENCY | DIP_VIDEO_SYNC;

enum {
    BTN_START = 0x0080,
    BTN_SERVICE = 0x0040,
    BTN_UP = 0x0020,
    BTN_DOWN = 0x0010,
    BTN_LEFT = 0x0008,
    BTN_RIGHT = 0x0004,
    BTN_1 = 0x0002,
    BTN_2 = 0x0001,
    BTN_3 = 0x8000,
    BTN_4 = 0x4000,
    BTN_5 = 0x2000,
    BTN_6 = 0x1000
};

struct Acjv {
    uint8_t request[FRAME_SIZE];
    uint8_t response[FRAME_SIZE];

    uint16_t buttons[PLAYER_COUNT];
    uint16_t coins[PLAYER_COUNT];

    uint8_t dip_switches;
    uint8_t frame_count;

    uint8_t coin_held[PLAYER_COUNT];
    uint8_t test_held;

    uint16_t last_coin_packet_id;

    int mode;
    int wheel_style;
    float axis[AXIS_COUNT];

    uint16_t fca_counter;

    Gun gun[PLAYER_COUNT];

    uint16_t gun_trigger = GUN_DEFAULT_TRIGGER;
    uint16_t gun_pedal = GUN_DEFAULT_PEDAL;
    uint16_t gun_sensor = 0;
    int gun_sensor_active_high = 0;
    int gun_board = GUN_BOARD_CLASSIC;
    uint16_t logged_gun_x = 0;
    uint16_t logged_gun_y = 0;

    uint8_t seen_commands[256];
    uint8_t reported_bad_magic;
    int logged_frames;
    int last_unknown_command;

    scheduler::Scheduler* sched;

    const char* board_id;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Acjv* create(logger::Logger* logger, scheduler::Scheduler* sched);
void destroy(Acjv* acjv);
void reset(Acjv* acjv);
void start(Acjv* acjv);
void press_switch(Acjv* acjv, int player, uint16_t mask);
void release_switch(Acjv* acjv, int player, uint16_t mask);
void set_coin_switch(Acjv* acjv, int slot, int pressed);
void set_test_switch(Acjv* acjv, int pressed);
void set_mode(Acjv* acjv, int mode, int wheel_style);
void set_axis(Acjv* acjv, int axis, float value);
void set_gun_position(Acjv* acjv, int player, float x, float y);
void set_gun_off_screen(Acjv* acjv, int player);
void set_touch_pressed(Acjv* acjv, int player, int pressed);
void set_gun_buttons(Acjv* acjv, uint16_t trigger, uint16_t pedal);
void set_gun_board(Acjv* acjv, int board, uint16_t sensor, int sensor_active_high);
void set_gun_trigger(Acjv* acjv, int player, int pressed);
void set_gun_pedal(Acjv* acjv, int player, int pressed);
uint64_t read16(Acjv* acjv, uint32_t addr);
void write16(Acjv* acjv, uint32_t addr, uint64_t data);
}
