#include <string>
#include <new>

#include "acjv.hpp"

namespace iris::s2x6::acjv {

static void set16(uint8_t* frame, int offset, uint16_t value);

static void fca_event(void* udata, int overshoot);

static void update_gun_sensor(Acjv* acjv, int player);

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
    acjv->board_id = BOARD_ID_RAYS;

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

void set_dip_switches(Acjv* acjv, uint8_t value) {
    acjv->dip_switches = (acjv->dip_switches & DIP_TEST) | (value & DIP_CONFIGURABLE);
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

void set_gun_position(Acjv* acjv, int player, float x, float y) {
    if (player < 0 || player >= PLAYER_COUNT)
        return;

    acjv->gun[player].x = x;
    acjv->gun[player].y = y;
    acjv->gun[player].aimed_away = 0;

    update_gun_sensor(acjv, player);
}

void set_gun_off_screen(Acjv* acjv, int player) {
    if (player < 0 || player >= PLAYER_COUNT)
        return;

    acjv->gun[player].aimed_away = 1;
    acjv->gun[player].touching = 0;

    update_gun_sensor(acjv, player);
}

void set_touch_pressed(Acjv* acjv, int player, int pressed) {
    if (player < 0 || player >= PLAYER_COUNT)
        return;

    acjv->gun[player].touching = pressed;
}

void set_gun_board(Acjv* acjv, int board, uint16_t sensor, int sensor_active_high) {
    acjv->gun_board = board;
    acjv->gun_sensor = sensor;
    acjv->gun_sensor_active_high = sensor_active_high;

    switch (board) {
        case GUN_BOARD_TWO_TIER: acjv->board_id = BOARD_ID_MIU; break;
        case GUN_BOARD_CAMERA: acjv->board_id = BOARD_ID_TSS; break;

        default: acjv->board_id = BOARD_ID_RAYS; break;
    }
}

void set_gun_buttons(Acjv* acjv, uint16_t trigger, uint16_t pedal) {
    acjv->gun_trigger = trigger;
    acjv->gun_pedal = pedal;
}

void set_gun_trigger(Acjv* acjv, int player, int pressed) {
    if (!acjv->gun_trigger)
        return;

    if (pressed) {
        press_switch(acjv, player, acjv->gun_trigger);
    } else {
        release_switch(acjv, player, acjv->gun_trigger);
    }
}

void set_gun_pedal(Acjv* acjv, int player, int pressed) {
    if (!acjv->gun_pedal)
        return;

    if (pressed) {
        press_switch(acjv, player, acjv->gun_pedal);
    } else {
        release_switch(acjv, player, acjv->gun_pedal);
    }
}

static float clamp_axis(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;

    return value;
}

static uint16_t to_signed_position(float value) {
    if (value > 32767.0f)
        value = 32767.0f;

    if (value < -32767.0f)
        value = -32767.0f;

    return (uint16_t)(int16_t)value;
}

static void read_gun_classic(const Gun* gun, uint16_t* x, uint16_t* y) {
    int on_screen = gun->x >= 0.0f && gun->y >= 0.0f &&
                    gun->x < 1.0f - GUN_EDGE_MARGIN &&
                    gun->y < 1.0f - GUN_EDGE_MARGIN;

    if (!on_screen) {
        *x = GUN_OFF_SCREEN;
        *y = GUN_OFF_SCREEN;

        return;
    }

    *x = (uint16_t)(gun->x * GUN_RANGE);
    *y = (uint16_t)((1.0f - gun->y) * GUN_RANGE);

    if (*x == GUN_OFF_SCREEN)
        *x = 1;

    if (*y == GUN_OFF_SCREEN)
        *y = 1;
}

static void read_gun_two_tier(const Gun* gun, uint16_t* x, uint16_t* y) {
    float overshoot = 0.0f;

    if (-gun->x > overshoot) overshoot = -gun->x;
    if (gun->x - 1.0f > overshoot) overshoot = gun->x - 1.0f;
    if (-gun->y > overshoot) overshoot = -gun->y;
    if (gun->y - 1.0f > overshoot) overshoot = gun->y - 1.0f;

    if (overshoot > GUN_TWO_TIER_LOST) {
        *x = GUN_LOST;
        *y = GUN_LOST;

        return;
    }

    *x = to_signed_position(gun->x * GUN_TWO_TIER_WIDTH);
    *y = to_signed_position((1.0f - gun->y) * GUN_TWO_TIER_HEIGHT);
}

static void read_gun_touch(const Gun* gun, uint16_t* x, uint16_t* y) {
    if (!gun->touching) {
        *x = GUN_LOST;
        *y = GUN_LOST;

        return;
    }

    float across = clamp_axis(gun->x, 0.0f, 1.0f);
    float down = clamp_axis(gun->y, 0.0f, 1.0f);

    *x = (uint16_t)clamp_axis(across * GUN_RANGE, 0.0f, TOUCH_MAX);
    *y = (uint16_t)clamp_axis((1.0f - down) * GUN_RANGE, 0.0f, TOUCH_MAX);
}

static void read_gun_side_switch(const Gun* gun, uint16_t* x, uint16_t* y) {
    float across = clamp_axis(gun->x, 0.0f, 1.0f);
    float down = clamp_axis(gun->y, 0.0f, 1.0f);

    *x = (uint16_t)((1.0f - across) * GUN_RANGE);
    *y = (uint16_t)(down * GUN_RANGE);
}

static void read_gun_camera(const Gun* gun, uint16_t* x, uint16_t* y) {
    float across = 0.5f + (gun->x - 0.5f) * GUN_CAMERA_VISIBLE_X;
    float down = 0.5f + (0.5f - gun->y) * GUN_CAMERA_VISIBLE_Y;

    int in_aim_area = gun->x >= GUN_EDGE_MARGIN && gun->x <= 1.0f - GUN_EDGE_MARGIN &&
                      gun->y >= GUN_EDGE_MARGIN && gun->y <= 1.0f - GUN_EDGE_MARGIN;

    if (!in_aim_area || across < 0.0f || across > 1.0f || down < 0.0f || down > 1.0f) {
        *x = GUN_LOST;
        *y = GUN_LOST;

        return;
    }

    *x = (uint16_t)clamp_axis(across * GUN_RANGE, 1.0f, GUN_RANGE - 1.0f);
    *y = (uint16_t)clamp_axis(down * GUN_RANGE, 1.0f, GUN_RANGE - 1.0f);

    if (*x == GUN_CAMERA_SAMPLER_GAP)
        *x = GUN_CAMERA_SAMPLER_GAP - 1;

    if (*y == GUN_CAMERA_SAMPLER_GAP)
        *y = GUN_CAMERA_SAMPLER_GAP - 1;
}

static int gun_is_on_screen(const Gun* gun) {
    return !gun->aimed_away &&
           gun->x >= 0.0f && gun->x <= 1.0f &&
           gun->y >= 0.0f && gun->y <= 1.0f;
}

static void read_gun(Acjv* acjv, int player, uint16_t* x, uint16_t* y) {
    *x = GUN_OFF_SCREEN;
    *y = GUN_OFF_SCREEN;

    if (player < 0 || player >= PLAYER_COUNT)
        return;

    const Gun* gun = &acjv->gun[player];

    if (acjv->mode == MODE_TOUCH) {
        read_gun_touch(gun, x, y);

        return;
    }

    if (gun->aimed_away) {
        int sentinel = acjv->gun_board == GUN_BOARD_TWO_TIER ||
                       acjv->gun_board == GUN_BOARD_CAMERA;

        if (sentinel) {
            *x = GUN_LOST;
            *y = GUN_LOST;
        }

        return;
    }

    switch (acjv->gun_board) {
        case GUN_BOARD_TWO_TIER: read_gun_two_tier(gun, x, y); break;
        case GUN_BOARD_CAMERA: read_gun_camera(gun, x, y); break;

        default: read_gun_classic(gun, x, y); break;
    }
}

static void read_gun_analog(Acjv* acjv, int player, uint16_t* x, uint16_t* y) {
    *x = GUN_OFF_SCREEN;
    *y = GUN_OFF_SCREEN;

    if (player < 0 || player >= PLAYER_COUNT || acjv->gun[player].aimed_away)
        return;

    read_gun_side_switch(&acjv->gun[player], x, y);
}

static void update_gun_sensor(Acjv* acjv, int player) {
    if (!acjv->gun_sensor)
        return;

    int on_screen = gun_is_on_screen(&acjv->gun[player]);
    int pressed = acjv->gun_sensor_active_high ? on_screen : !on_screen;

    if (pressed) {
        press_switch(acjv, player, acjv->gun_sensor);
    } else {
        release_switch(acjv, player, acjv->gun_sensor);
    }
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

                if (acjv->mode == MODE_TOUCH) {
                    report(&reply, FEATURE_SCREEN_POSITION);
                    report(&reply, GUN_BITS);
                    report(&reply, GUN_BITS);
                    report(&reply, TOUCH_CHANNELS);
                }

                if (acjv->mode == MODE_LIGHTGUN) {
                    report(&reply, FEATURE_SCREEN_POSITION);
                    report(&reply, GUN_BITS);
                    report(&reply, GUN_BITS);
                    report(&reply, PLAYER_COUNT);

                    report(&reply, FEATURE_GENERAL_OUTPUT);
                    report(&reply, GENERAL_OUTPUT_SLOTS);
                    report(&reply, 0);
                    report(&reply, 0);

                    report(&reply, FEATURE_ANALOG_INPUT);
                    report(&reply, GUN_ANALOG_CHANNELS);
                    report(&reply, GUN_BITS);
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

                uint16_t value[WHEEL_CHANNELS] = {};
                int driven = 0;

                if (acjv->mode == MODE_DRIVE) {
                    read_wheel(acjv, value);

                    driven = WHEEL_CHANNELS;
                }

                if (acjv->mode == MODE_LIGHTGUN) {
                    read_gun_analog(acjv, 0, &value[0], &value[1]);

                    driven = GUN_ANALOG_CHANNELS;
                }

                report(&reply, JVS_REPORT_OK);

                for (int i = 0; i < channels; i++) {
                    uint16_t channel = i < driven ? value[i] : WHEEL_CENTER;

                    report(&reply, channel >> 8);
                    report(&reply, channel & 0xff);
                }
            } break;

            case JVS_READ_SCREEN_POSITION: {
                int gun = *cursor++;

                remaining--;

                uint16_t x = GUN_OFF_SCREEN;
                uint16_t y = GUN_OFF_SCREEN;

                read_gun(acjv, gun - 1, &x, &y);

                if (x != acjv->logged_gun_x || y != acjv->logged_gun_y) {
                    acjv->logged_gun_x = x;
                    acjv->logged_gun_y = y;

                    // iris_debug(acjv, "Gun {} aim {:.3f},{:.3f} reported {:04x},{:04x}",
                    //     gun, acjv->gun[0].x, acjv->gun[0].y, x, y);
                }

                report(&reply, JVS_REPORT_OK);
                report(&reply, x >> 8);
                report(&reply, x & 0xff);
                report(&reply, y >> 8);
                report(&reply, y & 0xff);
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
