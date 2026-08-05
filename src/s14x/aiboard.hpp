#pragma once

#include "link.hpp"
#include "logger.hpp"

namespace iris::s14x::aiboard {

struct Aiboard {
    uint16_t version;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Aiboard* create(logger::Logger* logger);
void destroy(Aiboard* aiboard);

void handle_packet(void* udata, link::Packet* in, link::Packet* out);

}
