#include "guncon.h"

#include <stdlib.h>
#include <string.h>

// #define printf(fmt, ...)(0)

static inline uint8_t guncon_get_model_byte(struct guncon_state* guncon) {
    return 0x63;
}
static inline void guncon_cmd_set_vref_param(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_set_vref_param\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x5a);
}
static inline void guncon_cmd_query_masked(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_query_masked\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
}
static inline void guncon_cmd_read_data(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_read_data(%04x)\n", guncon->buttons);

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, guncon_get_model_byte(guncon));
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, guncon->buttons & 0xff);
    queue_push(sio2->out, guncon->buttons >> 8);
    queue_push(sio2->out, guncon->x & 0xff);
    queue_push(sio2->out, guncon->x >> 8);
    queue_push(sio2->out, guncon->y & 0xff);
    queue_push(sio2->out, guncon->y >> 8);
}
static inline void guncon_cmd_config_mode(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_config_mode(%02x)\n", queue_at(sio2->in, 3));

    // Same as read_data, but without pressure data in DualShock 2 mode
    if (!guncon->config_mode) {
        queue_push(sio2->out, 0xff);

        // We don't use the model byte here because
        // config_mode returns the same data as analog (GUNCON1)
        // when not in config mode regardless of the model
        queue_push(sio2->out, 0x63);
        queue_push(sio2->out, 0x5a);
        queue_push(sio2->out, guncon->buttons & 0xff);
        queue_push(sio2->out, guncon->buttons >> 8);
        queue_push(sio2->out, guncon->x & 0xff);
        queue_push(sio2->out, guncon->x >> 8);
        queue_push(sio2->out, guncon->y & 0xff);
        queue_push(sio2->out, guncon->y >> 8);
    } else {
        queue_push(sio2->out, 0xff);
        queue_push(sio2->out, 0xf3);
        queue_push(sio2->out, 0x5a);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
    }

    guncon->config_mode = queue_at(sio2->in, 3);
}
static inline void guncon_cmd_set_mode(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_set_mode(%02x, %02x)\n", queue_at(sio2->in, 3), queue_at(sio2->in, 4));

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
}
static inline void guncon_cmd_query_model(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_query_model\n");

    queue_push(sio2->out, 0xff); // Header
    queue_push(sio2->out, 0xf3); // Mode (F3=Config)
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x03); // Model (01=Dualshock/Digital 03=Dualshock 2)
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, 0x00); // Analog (00=no 01=yes)
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, 0x01);
    queue_push(sio2->out, 0x00);
}
static inline void guncon_cmd_query_act(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_query_act(%02x)\n", queue_at(sio2->in, 3));

    int index = queue_at(sio2->in, 3);

    if (!index) {
        queue_push(sio2->out, 0xff);
        queue_push(sio2->out, 0xf3);
        queue_push(sio2->out, 0x5a);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x01);
        queue_push(sio2->out, 0x02);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x0a);
    } else {
        queue_push(sio2->out, 0xff);
        queue_push(sio2->out, 0xf3);
        queue_push(sio2->out, 0x5a);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x01);
        queue_push(sio2->out, 0x01);
        queue_push(sio2->out, 0x01);
        queue_push(sio2->out, 0x14);
    }
}
static inline void guncon_cmd_query_comb(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_query_comb\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x01);
    queue_push(sio2->out, 0x00);
}
static inline void guncon_cmd_query_mode(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_query_mode\n");

    int index = queue_at(sio2->in, 3);

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, index ? 7 : 4);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
}
static inline void guncon_cmd_vibration_toggle(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_vibration_toggle\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
}
static inline void guncon_cmd_set_native_mode(struct ps2_sio2* sio2, struct guncon_state* guncon) {
    printf("guncon: guncon_cmd_set_native_mode(%02x, %02x, %02x, %02x)\n",
        queue_at(sio2->in, 3),
        queue_at(sio2->in, 4),
        queue_at(sio2->in, 5),
        queue_at(sio2->in, 6)
    );

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x5a);
}

void guncon_handle_command(struct ps2_sio2* sio2, void* udata, int cmd) {
    struct guncon_state* guncon = (struct guncon_state*)udata;

    switch (cmd) {
        case 0x40: guncon_cmd_set_vref_param(sio2, guncon); return;
        case 0x41: guncon_cmd_query_masked(sio2, guncon); return;
        case 0x42: guncon_cmd_read_data(sio2, guncon); return;
        case 0x43: guncon_cmd_config_mode(sio2, guncon); return;
        case 0x44: guncon_cmd_set_mode(sio2, guncon); return;
        case 0x45: guncon_cmd_query_model(sio2, guncon); return;
        case 0x46: guncon_cmd_query_act(sio2, guncon); return;
        case 0x47: guncon_cmd_query_comb(sio2, guncon); return;
        case 0x4C: guncon_cmd_query_mode(sio2, guncon); return;
        case 0x4D: guncon_cmd_vibration_toggle(sio2, guncon); return;
        case 0x4F: guncon_cmd_set_native_mode(sio2, guncon); return;
    }

    printf("guncon: Unhandled command %02x\n", cmd);

    exit(1);
}

struct guncon_state* guncon_attach(struct ps2_sio2* sio2, int port) {
    struct guncon_state* guncon = malloc(sizeof(struct guncon_state));
    struct sio2_device dev;

    dev.detach = guncon_detach;
    dev.handle_command = guncon_handle_command;
    dev.udata = guncon;

    memset(guncon, 0, sizeof(struct guncon_state));

    guncon->port = port;
    guncon->config_mode = 0;
    guncon->x = 0x10d;
    guncon->y = 0x88;
    guncon->buttons = 0xffff;

    ps2_sio2_attach_device(sio2, dev, port);

    return guncon;
}

void guncon_button_press(struct guncon_state* guncon, uint16_t mask) {
    guncon->buttons &= ~mask;
}

void guncon_button_release(struct guncon_state* guncon, uint16_t mask) {
    guncon->buttons |= mask;
}

void guncon_analog_change(struct guncon_state* guncon, int axis, uint8_t value) {
    switch (axis) {
        case 0: guncon->x = value; break;
        case 1: guncon->y = value; break;
    }
}

void guncon_detach(void* udata) {
    struct guncon_state* guncon = (struct guncon_state*)udata;

    free(guncon);
}