#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::mcd {

inline constexpr uint32_t SIZE_8MB  = 0x4000;
inline constexpr uint32_t SIZE_16MB = 0x8000;
inline constexpr uint32_t SIZE_32MB = 0x10000;
inline constexpr uint32_t SIZE_64MB = 0x20000;

inline constexpr uint32_t SECTOR_SIZE = 512 + 16;

struct Mcd {
    int port = 0;
    uint8_t term = 0;
    uint16_t buttons = 0;
    uint8_t ax_right_y = 0;
    uint8_t ax_right_x = 0;
    uint8_t ax_left_y = 0;
    uint8_t ax_left_x = 0;
    int config_mode = 0;
    int act_index = 0;
    int mode_index = 0;
    uint32_t size = 0;
    uint8_t checksum = 0;
    uint32_t addr = 0;
    uint32_t buf_size = 0;
    uint8_t* buf = nullptr;

    FILE* file = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Mcd* attach(logger::Logger* logger, sio2::Sio2* sio2, int port, const char* path);
void detach(void* udata);

}
