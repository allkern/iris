#pragma once

#include <cstddef>
#include <cstdint>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::guncon {

inline constexpr uint16_t BT_START  = 0x0008;
inline constexpr uint16_t BT_CIRCLE = 0x2000;
inline constexpr uint16_t BT_CROSS  = 0x4000;

inline constexpr int AX_X = 0;
inline constexpr int AX_Y = 1;

struct Guncon {
    int port = 0;
    uint16_t buttons = 0;
    uint16_t x = 0;
    uint16_t y = 0;
    int config_mode = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Guncon* attach(logger::Logger* logger, sio2::Sio2* sio2, int port);
void button_press(Guncon* guncon, uint16_t mask);
void button_release(Guncon* guncon, uint16_t mask);
void analog_change(Guncon* guncon, int axis, uint8_t value);
void detach(void* udata);

}
