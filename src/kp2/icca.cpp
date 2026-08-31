#include <cstring>

#include "icca.hpp"

namespace iris::kp2::icca {

inline constexpr auto CODE_GET_INFO = 0x0002;
inline constexpr auto CODE_CLEAR = 0x0000;
inline constexpr auto CODE_RESET = 0x0003;
inline constexpr auto CODE_UNKNOWN_0116 = 0x0116;
inline constexpr auto CODE_UNKNOWN_0120 = 0x0120;
inline constexpr auto CODE_QUEUE_LOOP_START = 0x0130;
inline constexpr auto CODE_ENGAGE = 0x0131;
inline constexpr auto CODE_POLL = 0x0134;
inline constexpr auto CODE_SET_SLOT_STATE = 0x0135;
inline constexpr auto CODE_DEVICE_CONTROL = 0x013a;
inline constexpr auto CODE_KEY_EXCHANGE = 0x0160;
inline constexpr auto CODE_POLL_FELICA = 0x0161;
inline constexpr auto CODE_POLL_ENCRYPTED = 0x0164;

inline constexpr auto STATUS_IDLE = 0x01;
inline constexpr auto STATUS_CARD = 0x02;

inline constexpr auto SENSOR_FRONT = 0x10;
inline constexpr auto SENSOR_BACK = 0x20;

inline constexpr auto KEYPAD_STARTED = 0x03;

inline constexpr auto SLOT_STATE_OFFSET = 1;

static const uint8_t info[INFO_SIZE] = {
    0x03, 0x00, 0x00, 0x00,
    0x00,
    0x01, 0x04, 0x00,
    'I', 'C', 'C', 'A',
    'O', 'c', 't', ' ', '2', '6', ' ', '2', '0', '0', '5', 0, 0, 0, 0, 0,
    '1', '3', ' ', ':', ' ', '5', '5', ' ', ':', ' ', '0', '3', 0, 0, 0, 0
};

static const uint8_t default_uid[UID_SIZE] = {
    0xe0, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01
};

static void respond(acio::Response* response, const uint8_t* data, int size) {
    memcpy(response->payload, data, size);

    response->length = (uint8_t)size;
}

static void respond_status_byte(acio::Response* response) {
    response->payload[0] = 0;
    response->length = 1;
}

void init(Icca* icca) {
    memcpy(icca->uid, default_uid, UID_SIZE);

    icca->card_present = 0;
    icca->card_switch = 0;
    icca->accepting = 0;
    icca->holding = 0;

    icca->keypad = 0;
    icca->keypad_last = 0;

    icca->key_event = 0;
    icca->key_counter = 8;
}

void set_uid(Icca* icca, const uint8_t* uid) {
    memcpy(icca->uid, uid, UID_SIZE);
}

void set_card_switch(Icca* icca, int pressed) {
    if (pressed && !icca->card_switch) {
        icca->card_present = 1;
    }

    icca->card_switch = pressed;
}

void press_key(Icca* icca, uint16_t mask) {
    icca->keypad |= mask;
}

void release_key(Icca* icca, uint16_t mask) {
    icca->keypad &= ~mask;
}

static void update_keypad(Icca* icca) {
    uint16_t rising = ~icca->keypad_last & icca->keypad;

    icca->key_event = 0;

    for (int i = 0; i < 16; i++) {
        if (!(rising & (1 << i)))
            continue;

        icca->key_event = (icca->key_counter << 4) | i;

        icca->key_counter = (icca->key_counter + 1) | 8;

        break;
    }

    icca->keypad_last = icca->keypad;
}

static void build_status(Icca* icca, uint8_t* status) {
    update_keypad(icca);

    if (icca->accepting && icca->card_present) {
        icca->holding = 1;
    }

    memset(status, 0, STATUS_SIZE);

    if (icca->card_present) {
        status[0] = STATUS_CARD;
        status[1] = icca->holding ? (SENSOR_FRONT | SENSOR_BACK) : SENSOR_FRONT;

        memcpy(status + 2, icca->uid, UID_SIZE);
    } else {
        status[0] = STATUS_IDLE;
    }

    status[11] = KEYPAD_STARTED;
    status[12] = icca->key_event;
    status[14] = icca->keypad >> 8;
    status[15] = icca->keypad & 0xff;
}

static void set_slot(Icca* icca, int state) {
    switch (state) {
        case SLOT_CLOSE: {
            icca->accepting = 0;
        } break;

        case SLOT_OPEN: {
            icca->accepting = 1;
        } break;

        case SLOT_EJECT: {
            icca->accepting = 0;
            icca->holding = 0;
            icca->card_present = 0;
        } break;
    }
}

bool handle_packet(void* udata, const acio::Request* request, acio::Response* response) {
    Icca* icca = (Icca*)udata;

    switch (request->code) {
        case CODE_GET_INFO: {
            respond(response, info, INFO_SIZE);
        } break;

        case CODE_CLEAR:
        case CODE_RESET:
        case CODE_UNKNOWN_0116:
        case CODE_UNKNOWN_0120:
        case CODE_QUEUE_LOOP_START:
        case CODE_DEVICE_CONTROL: {
            respond_status_byte(response);
        } break;

        case CODE_ENGAGE: {
            build_status(icca, response->payload);

            response->payload[0] = STATUS_IDLE;
            response->length = STATUS_SIZE;
        } break;

        case CODE_POLL: {
            build_status(icca, response->payload);

            response->length = STATUS_SIZE;
        } break;

        case CODE_SET_SLOT_STATE: {
            if (request->length > SLOT_STATE_OFFSET) {
                set_slot(icca, request->payload[SLOT_STATE_OFFSET]);
            }

            build_status(icca, response->payload);

            response->length = STATUS_SIZE;
        } break;

        case CODE_KEY_EXCHANGE:
        case CODE_POLL_FELICA:
        case CODE_POLL_ENCRYPTED: {
            respond_status_byte(response);
        } break;

        default: return false;
    }

    return true;
}

}
