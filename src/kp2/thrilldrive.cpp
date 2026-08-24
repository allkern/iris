#include <cstring>

#include "thrilldrive.hpp"

namespace iris::kp2::thrilldrive {

inline constexpr auto CODE_GET_INFO = 0x0002;
inline constexpr auto CODE_RESET = 0x0100;
inline constexpr auto CODE_STATUS = 0x0102;
inline constexpr auto CODE_STATUS_ALT = 0x0110;
inline constexpr auto CODE_STATUS_EXT = 0x0111;
inline constexpr auto CODE_BELT_STATE = 0x0113;
inline constexpr auto CODE_FORCE_FEEDBACK = 0x0120;

inline constexpr auto FFB_CALIBRATION_MARKER = -2;

inline constexpr auto FFB_OFFSET = 6;
inline constexpr auto FFB_AUX_OFFSET = 9;

inline constexpr auto BELT_FASTENED = 0x00;
inline constexpr auto BELT_RELEASED = 0xff;

static const uint8_t handle_info[48] = {
    0x03, 0x00, 0x00, 0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    'H', 'N', 'D', 'L',
    'O', 'c', 't', ' ', '2', '6', ' ', '2', '0', '0', '5', 0, 0, 0, 0, 0,
    '1', '3', ' ', ':', ' ', '5', '5', ' ', ':', ' ', '0', '3', 0, 0, 0, 0
};

static const uint8_t belt_info[48] = {
    0x03, 0x00, 0x00, 0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    'B', 'E', 'L', 'T',
    'O', 'c', 't', ' ', '2', '6', ' ', '2', '0', '0', '5', 0, 0, 0, 0, 0,
    '1', '3', ' ', ':', ' ', '5', '5', ' ', ':', ' ', '0', '3', 0, 0, 0, 0
};

static void respond(acio::Response* response, const uint8_t* data, int size) {
    memcpy(response->payload, data, size);

    response->length = (uint8_t)size;
}

static void respond_zeroes(acio::Response* response, int size) {
    memset(response->payload, 0, size);

    response->length = (uint8_t)size;
}

void init_handle(Handle* handle) {
    handle->force_feedback = 0;
    handle->force_feedback_aux = 0;
    handle->calibrating = 0;
}

void init_belt(Belt* belt) {
    belt->fastened = 1;
}

bool handle_packet(void* udata, const acio::Request* request, acio::Response* response) {
    Handle* handle = (Handle*)udata;

    switch (request->code) {
        case CODE_GET_INFO: {
            respond(response, handle_info, sizeof(handle_info));
        } break;

        case CODE_RESET: {
            respond_zeroes(response, 1);
        } break;

        case CODE_FORCE_FEEDBACK: {
            if (request->length > FFB_AUX_OFFSET) {
                handle->force_feedback = (int8_t)request->payload[FFB_OFFSET];
                handle->force_feedback_aux = (int8_t)request->payload[FFB_AUX_OFFSET];

                handle->calibrating = handle->force_feedback_aux == FFB_CALIBRATION_MARKER;
            }

            respond_zeroes(response, 1);
        } break;

        default: return false;
    }

    return true;
}

bool belt_packet(void* udata, const acio::Request* request, acio::Response* response) {
    Belt* belt = (Belt*)udata;

    switch (request->code) {
        case CODE_GET_INFO: {
            respond(response, belt_info, sizeof(belt_info));
        } break;

        case CODE_RESET:
        case CODE_STATUS:
        case CODE_STATUS_ALT: {
            respond_zeroes(response, 1);
        } break;

        case CODE_STATUS_EXT: {
            respond_zeroes(response, 4);
        } break;

        case CODE_BELT_STATE: {
            respond_zeroes(response, 3);

            response->payload[2] = belt->fastened ? BELT_FASTENED : BELT_RELEASED;
        } break;

        default: return false;
    }

    return true;
}

}
