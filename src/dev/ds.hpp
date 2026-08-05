#pragma once

#include <cstddef>
#include <cstdint>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::ds {

inline constexpr uint32_t BT_SELECT   = 0x0001;
inline constexpr uint32_t BT_L3       = 0x0002;
inline constexpr uint32_t BT_R3       = 0x0004;
inline constexpr uint32_t BT_START    = 0x0008;
inline constexpr uint32_t BT_UP       = 0x0010;
inline constexpr uint32_t BT_RIGHT    = 0x0020;
inline constexpr uint32_t BT_DOWN     = 0x0040;
inline constexpr uint32_t BT_LEFT     = 0x0080;
inline constexpr uint32_t BT_L2       = 0x0100;
inline constexpr uint32_t BT_R2       = 0x0200;
inline constexpr uint32_t BT_L1       = 0x0400;
inline constexpr uint32_t BT_R1       = 0x0800;
inline constexpr uint32_t BT_TRIANGLE = 0x1000;
inline constexpr uint32_t BT_CIRCLE   = 0x2000;
inline constexpr uint32_t BT_CROSS    = 0x4000;
inline constexpr uint32_t BT_SQUARE   = 0x8000;
inline constexpr uint32_t BT_ANALOG   = 0x10000;

inline constexpr int AX_RIGHT_V = 0;
inline constexpr int AX_RIGHT_H = 1;
inline constexpr int AX_LEFT_V  = 2;
inline constexpr int AX_LEFT_H  = 3;

struct Ds {
    int port = 0;
    uint16_t buttons = 0;
    uint8_t ax_right_y = 0;
    uint8_t ax_right_x = 0;
    uint8_t ax_left_y = 0;
    uint8_t ax_left_x = 0;
    int config_mode = 0;
    int act_index = 0;
    int mode_index = 0;
    int mode = 0;
    int vibration[2] = {};
    int mask[2] = {};
    int lock = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Ds* attach(logger::Logger* logger, sio2::Sio2* sio2, int port);
void button_press(Ds* ds, uint32_t mask);
void button_release(Ds* ds, uint32_t mask);
void analog_change(Ds* ds, int axis, uint8_t value);
void detach(void* udata);

}
