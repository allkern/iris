#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::ps1_mcd {

inline constexpr uint32_t SECTOR_SIZE = 128;
inline constexpr uint32_t SIZE = 0x20000;

struct Ps1Mcd {
    // 128 KiB
    uint8_t buf[SIZE] = {};
    uint8_t flag = 0;
    int type = 0;

    FILE* file = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Ps1Mcd* attach(logger::Logger* logger, sio2::Sio2* sio2, int port, const char* path);
void set_type(Ps1Mcd* mcd, int type);
void detach(void* udata);

}
