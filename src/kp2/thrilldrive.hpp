#pragma once

#include <cstdint>

#include "acio.hpp"

namespace iris::kp2::thrilldrive {

struct Handle {
    int8_t force_feedback;
    int8_t force_feedback_aux;

    int calibrating;
};

struct Belt {
    int fastened;
};

void init_handle(Handle* handle);
void init_belt(Belt* belt);

bool handle_packet(void* udata, const acio::Request* request, acio::Response* response);
bool belt_packet(void* udata, const acio::Request* request, acio::Response* response);

}
