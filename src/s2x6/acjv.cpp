#include <string>
#include <new>

#include "acjv.hpp"

namespace iris::s2x6::acjv {

static void set16(uint8_t* frame, int offset, uint16_t value);

static void fca_event(void* udata, int overshoot);

static void schedule_fca(Acjv* acjv) {
    scheduler::Event event;

    event.callback = fca_event;
    event.cycles = FCA_INTERVAL;
    event.name = "FCA-1 input frame";
    event.udata = acjv;

    scheduler::schedule(acjv->sched, event);
}

static float clamp_axis(float value, float low, float high);

static void fca_event(void* udata, int overshoot) {
    Acjv* acjv = (Acjv*)udata;

    schedule_fca(acjv);

    if (acjv->mode != MODE_FCA)
        return;

    uint8_t* frame = acjv->response;

    set16(frame, FCA_HEARTBEAT, ++acjv->fca_counter);

    float steer = clamp_axis(acjv->axis[AXIS_STEER_RIGHT] - acjv->axis[AXIS_STEER_LEFT], -1.0f, 1.0f);

    set16(frame, FCA_STEER, (uint16_t)(WHEEL_CENTER + (int)(steer * FCA_STEER_RANGE)));
    set16(frame, FCA_GAS, (uint16_t)(clamp_axis(acjv->axis[AXIS_GAS], 0.0f, 1.0f) * FCA_PEDAL_MAX));
    set16(frame, FCA_BRAKE, (uint16_t)(clamp_axis(acjv->axis[AXIS_BRAKE], 0.0f, 1.0f) * FCA_PEDAL_MAX));

    uint16_t buttons = acjv->buttons[0];
    uint8_t shift = 0;
    uint8_t panel = 0;

    if (buttons & BTN_3) shift |= 0x80;
    if (buttons & BTN_4) shift |= 0x40;
    if (buttons & BTN_2) panel |= 0x01;
    if (buttons & (BTN_START | BTN_1)) panel |= 0x02;
    if (buttons & BTN_UP) panel |= 0x20;
    if (buttons & BTN_DOWN) panel |= 0x10;
    if (buttons & BTN_SERVICE) panel |= 0x40;

    frame[FCA_BUTTONS] = shift;
    frame[FCA_BUTTONS + 1] = panel;

    frame[FCA_TEST] = (acjv->dip_switches & DIP_TEST) ? 0x80 : 0;
    frame[FCA_COIN] = acjv->coins[0] & 0xff;
}

void reset(Acjv* acjv) {
    schedule_fca(acjv);
}

Acjv* create(logger::Logger* logger, scheduler::Scheduler* sched) {
    Acjv* acjv = new Acjv();

    acjv->logger = logger;
    acjv->logger_id = logger::register_source(logger, "acjv");

    acjv->sched = sched;

    acjv->dip_switches = DIP_DEFAULT;
    acjv->board_id = "namco ltd.;RAYS PCB;";

    schedule_fca(acjv);

    return acjv;
}

void destroy(Acjv* acjv) {
    delete acjv;
}

void press_switch(Acjv* acjv, int player, uint16_t mask) {
    if (player < 0 || player >= PLAYER_COUNT)
        return;

    acjv->buttons[player] |= mask;
}

void release_switch(Acjv* acjv, int player, uint16_t mask) {
    if (player < 0 || player >= PLAYER_COUNT)
        return;

    acjv->buttons[player] &= ~mask;
}

void set_coin_switch(Acjv* acjv, int slot, int pressed) {
    if (slot < 0 || slot >= PLAYER_COUNT)
        return;

    if (pressed && !acjv->coin_held[slot])
        acjv->coins[slot]++;

    acjv->coin_held[slot] = pressed != 0;
}

void set_test_switch(Acjv* acjv, int pressed) {
    if (pressed && !acjv->test_held)
        acjv->dip_switches ^= DIP_TEST;

    acjv->test_held = pressed != 0;
}

void set_mode(Acjv* acjv, int mode, int wheel_style) {
    acjv->mode = mode;
    acjv->wheel_style = wheel_style;
}

void set_axis(Acjv* acjv, int axis, float value) {
    if (axis < 0 || axis >= AXIS_COUNT)
        return;

    acjv->axis[axis] = value;
}

static float clamp_axis(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;

    return value;
}

static void read_wheel(Acjv* acjv, uint16_t* channels) {
    float steer = clamp_axis(acjv->axis[AXIS_STEER_RIGHT] - acjv->axis[AXIS_STEER_LEFT], -1.0f, 1.0f);
    float pedal_max = 0xffff;

    if (acjv->wheel_style == WHEEL_WANGAN) {
        channels[0] = (uint16_t)(WHEEL_CENTER + (int)(steer * WHEEL_WANGAN_RANGE));

        pedal_max = WHEEL_WANGAN_PEDAL_MAX;
    } else {
        channels[0] = (uint16_t)(((steer * 0.5f) + 0.5f) * 0xffff);
    }

    channels[1] = (uint16_t)(clamp_axis(acjv->axis[AXIS_GAS], 0.0f, 1.0f) * pedal_max);
    channels[2] = (uint16_t)(clamp_axis(acjv->axis[AXIS_BRAKE], 0.0f, 1.0f) * pedal_max);
}

static uint16_t get16(const uint8_t* frame, int offset) {
    return frame[offset] | (frame[offset + 1] << 8);
}

static void set16(uint8_t* frame, int offset, uint16_t value) {
    frame[offset] = value & 0xff;
    frame[offset + 1] = value >> 8;
}

void start(Acjv* acjv) {
    iris_debug(acjv, "Board started, mode {}", acjv->mode);

    set16(acjv->response, RES_FIRMWARE_VERSION, FIRMWARE_VERSION);
}

struct Reply {
    uint8_t* cursor;
    uint8_t* end;
    uint8_t* size;
};

static void report(Reply* reply, uint8_t value) {
    if (reply->cursor == reply->end)
        return;

    *reply->cursor++ = value;

    (*reply->size)++;
}

static void handle_jvs_packet(Acjv* acjv, const uint8_t* packet, uint8_t* out) {
    const uint8_t* cursor = packet + 3;

    int remaining = packet[2] - 1;

    out[0] = JVS_SYNC;
    out[1] = JVS_MASTER_ADDRESS;
    out[2] = 1;
    out[3] = JVS_STATUS_OK;

    Reply reply = { out + 4, acjv->response + FRAME_SIZE, out + 2 };

    while (remaining > 0) {
        int cmd = *cursor++;

        remaining--;

        if (!acjv->seen_commands[cmd]) {
            acjv->seen_commands[cmd] = 1;

            iris_debug(acjv, "JVS command {:02x}", cmd);
        }

        switch (cmd) {
            case JVS_RESET: {
                cursor++;
                remaining--;
            } break;

            case JVS_SET_ADDRESS: {
                cursor++;
                remaining--;

                report(&reply, JVS_REPORT_OK);
            } break;

            case JVS_READ_ID: {
                report(&reply, JVS_REPORT_OK);

                for (const char* c = acjv->board_id; *c; c++)
                    report(&reply, *c);

                report(&reply, 0);
            } break;

            case JVS_GET_CMD_REVISION: {
                report(&reply, JVS_REPORT_OK);
                report(&reply, JVS_CMD_REVISION);
            } break;

            case JVS_GET_JVS_REVISION: {
                report(&reply, JVS_REPORT_OK);
                report(&reply, JVS_REVISION);
            } break;

            case JVS_GET_COMM_VERSION: {
                report(&reply, JVS_REPORT_OK);
                report(&reply, JVS_COMM_VERSION);
            } break;

            case JVS_GET_FEATURES: {
                report(&reply, JVS_REPORT_OK);

                report(&reply, FEATURE_COIN_INPUT);
                report(&reply, PLAYER_COUNT);
                report(&reply, 0);
                report(&reply, 0);

                report(&reply, FEATURE_SWITCH_INPUT);
                report(&reply, PLAYER_COUNT);
                report(&reply, SWITCHES_PER_PLAYER);
                report(&reply, 0);

                if (acjv->mode == MODE_DRIVE) {
                    report(&reply, FEATURE_ANALOG_INPUT);
                    report(&reply, WHEEL_CHANNELS);
                    report(&reply, WHEEL_BITS);
                    report(&reply, 0);
                }

                report(&reply, FEATURE_END);
            } break;

            case JVS_SET_MAIN_ID: {
                while (remaining > 0) {
                    int c = *cursor++;

                    remaining--;

                    if (!c)
                        break;
                }

                report(&reply, JVS_REPORT_OK);
            } break;

            case JVS_NAMCO_VENDOR: {
                int sub = *cursor++;

                remaining--;

                int args = sub == 0x60 ? 1 : sub == 0x62 ? 2 : sub == 0x18 ? 4 : remaining;

                cursor += args;
                remaining -= args;

                report(&reply, JVS_REPORT_OK);
                report(&reply, 2);
                report(&reply, sub);
                report(&reply, 0x01);
            } break;

            case JVS_READ_SWITCHES: {
                int players = *cursor++;

                cursor++;

                remaining -= 2;

                report(&reply, JVS_REPORT_OK);
                report(&reply, acjv->dip_switches & DIP_TEST);

                for (int i = 0; i < players && i < PLAYER_COUNT; i++) {
                    report(&reply, acjv->buttons[i] & 0xff);
                    report(&reply, acjv->buttons[i] >> 8);
                }
            } break;

            case JVS_READ_COINS: {
                int slots = *cursor++;

                remaining--;

                report(&reply, JVS_REPORT_OK);

                for (int i = 0; i < slots && i < PLAYER_COUNT; i++) {
                    report(&reply, (acjv->coins[i] >> 8) & 0x3f);
                    report(&reply, acjv->coins[i] & 0xff);
                }
            } break;

            case JVS_READ_ANALOG: {
                int channels = *cursor++;

                remaining--;

                uint16_t wheel[WHEEL_CHANNELS] = {};

                if (acjv->mode == MODE_DRIVE)
                    read_wheel(acjv, wheel);

                report(&reply, JVS_REPORT_OK);

                for (int i = 0; i < channels; i++) {
                    uint16_t value = i < WHEEL_CHANNELS ? wheel[i] : WHEEL_CENTER;

                    report(&reply, value >> 8);
                    report(&reply, value & 0xff);
                }
            } break;

            case JVS_INCREASE_COINS:
            case JVS_DECREASE_COINS: {
                int slot = *cursor++;
                int amount = (cursor[0] << 8) | cursor[1];

                cursor += 2;
                remaining -= 3;

                if (slot >= 1 && slot <= PLAYER_COUNT) {
                    if (cmd == JVS_INCREASE_COINS) {
                        acjv->coins[slot - 1] += amount;
                    } else {
                        acjv->coins[slot - 1] -= amount;
                    }
                }

                report(&reply, JVS_REPORT_OK);
            } break;

            case JVS_GENERAL_OUTPUT: {
                int bytes = *cursor++;

                cursor += bytes;
                remaining -= bytes + 1;

                report(&reply, JVS_REPORT_OK);
            } break;

            default: {
                if (cmd != acjv->last_unknown_command) {
                    acjv->last_unknown_command = cmd;

                    iris_error(acjv, "Unhandled JVS command {:02x}", cmd);
                }

                out[3] = JVS_STATUS_UNKNOWN_COMMAND;

                return;
            }
        }
    }
}

// Enough to cover the board's identification exchange without following it
// into the per-frame input polling
inline constexpr auto LOGGED_FRAMES_MAX = 24;

static std::string hex_packet(const uint8_t* packet) {
    std::string out;

    int size = packet[2] + 3;

    if (size > 64)
        size = 64;

    for (int i = 0; i < size; i++) {
        char byte[4];

        snprintf(byte, sizeof(byte), "%02x ", packet[i]);

        out += byte;
    }

    return out;
}

static void handle_frame(Acjv* acjv) {
    uint8_t* request = acjv->request;
    uint8_t* response = acjv->response;

    uint16_t magic = get16(request, REQ_MAGIC);

    set16(response, RES_MAGIC, magic);

    if (magic != FRAME_MAGIC) {
        if (!acjv->reported_bad_magic) {
            acjv->reported_bad_magic = 1;

            iris_warning(acjv, "Bad frame magic {:04x}", magic);
        }

        return;
    }

    set16(response, RES_FIRMWARE_VERSION, FIRMWARE_VERSION);
    set16(response, RES_ROOT_PACKET_ID, get16(request, REQ_ROOT_PACKET_ID));
    set16(response, RES_TOKEN, get16(request, REQ_TOKEN));

    for (int i = 0; i < 3; i++)
        set16(response, RES_LOADER_PACE + (i * 2), LOADER_PACE);

    response[RES_DIP_SWITCHES] = acjv->dip_switches;
    response[RES_FRAME_COUNT] = ++acjv->frame_count;

    uint16_t packet_id = get16(request, REQ_PACKET_ID);

    if (acjv->frame_count == 1)
        iris_debug(acjv, "First frame, packet id {:04x}", packet_id);

    if (!packet_id)
        return;

    int request_offset = request[REQ_JVS_AUX] == JVS_SYNC ? REQ_JVS_AUX : REQ_JVS;
    int response_offset = request_offset == REQ_JVS_AUX ? RES_JVS_AUX : RES_JVS;

    handle_jvs_packet(acjv, request + request_offset, response + response_offset);

    if (acjv->logged_frames < LOGGED_FRAMES_MAX) {
        acjv->logged_frames++;

        iris_debug(acjv, "{} -> {}",
            hex_packet(request + request_offset),
            hex_packet(response + response_offset));
    }

    bool decrement_pending = request[REQ_JVS_AUX] == JVS_SYNC &&
                             request[REQ_JVS] == JVS_SYNC &&
                             request[REQ_JVS + 1] != 0;

    if (decrement_pending && packet_id != acjv->last_coin_packet_id) {
        handle_jvs_packet(acjv, request + REQ_JVS, response + RES_JVS);

        acjv->last_coin_packet_id = packet_id;
    }

    set16(response, RES_PACKET_ID, packet_id);
}

uint64_t read16(Acjv* acjv, uint32_t addr) {
    if (addr >= RD_BASE && addr < RD_BASE + (FRAME_SIZE * 2))
        return acjv->response[(addr - RD_BASE) >> 1];

    return 0;
}

void write16(Acjv* acjv, uint32_t addr, uint64_t data) {
    if (addr >= WR_BASE && addr < WR_BASE + (FRAME_SIZE * 2)) {
        acjv->request[(addr - WR_BASE) >> 1] = data & 0xff;

        if (addr == WR_LAST)
            handle_frame(acjv);

        return;
    }
}

}
