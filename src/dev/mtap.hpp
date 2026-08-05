#pragma once

#include <cstddef>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::mtap {

struct Mtap {
    sio2::Device port[8] = {};

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Mtap* attach(logger::Logger* logger, sio2::Sio2* sio2, int port);
void detach(void* udata);

}
