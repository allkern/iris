#include "ds.h"

#include <stdlib.h>
#include <string.h>

static inline void ds_cmd_set_vref_param(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_set_vref_param\n");

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
static inline void ds_cmd_query_masked(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_query_masked\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x5a);
}
static inline void ds_cmd_read_data(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_read_data(%04x)\n", ds->buttons);

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0x79);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, ds->buttons & 0xff);
    queue_push(sio2->out, ds->buttons >> 8);
    queue_push(sio2->out, ds->ax_right_y);
    queue_push(sio2->out, ds->ax_right_x);
    queue_push(sio2->out, ds->ax_left_y);
    queue_push(sio2->out, ds->ax_left_x);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
}
static inline void ds_cmd_config_mode(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_config_mode(%02x)\n", sio2->in->buf[3]);

    if (!ds->config_mode) {
        queue_push(sio2->out, 0xff);
        queue_push(sio2->out, 0x79);
        queue_push(sio2->out, 0x5a);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, ds->ax_right_y);
        queue_push(sio2->out, ds->ax_right_x);
        queue_push(sio2->out, ds->ax_left_y);
        queue_push(sio2->out, ds->ax_left_x);

        ds->config_mode = sio2->in->buf[3];
    } else {
        queue_push(sio2->out, 0xff);
        queue_push(sio2->out, 0x79);
        queue_push(sio2->out, 0x5a);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);

        ds->config_mode = 0;
    }
}
static inline void ds_cmd_set_mode(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_set_mode(%02x)\n", sio2->in->buf[3]);

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
static inline void ds_cmd_query_model(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_query_model\n");

    queue_push(sio2->out, 0xff); // Header
    queue_push(sio2->out, 0xf3); // Mode (F3=Config)
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x03); // Model (01=Dualshock/Digital 03=Dualshock 2)
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, 0x01); // Analog (00=no 01=yes)
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, 0x01);
    queue_push(sio2->out, 0x00);
}
static inline void ds_cmd_query_act(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_query_act\n");

    int index = sio2->in->buf[2];

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
static inline void ds_cmd_query_comb(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_query_comb unimplemented\n");
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
static inline void ds_cmd_query_mode(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_query_mode\n");

    int index = sio2->in->buf[2];

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
static inline void ds_cmd_vibration_toggle(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_vibration_toggle\n");

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
static inline void ds_cmd_set_native_mode(struct ps2_sio2* sio2, struct ds_state* ds) {
    // printf("ds: ds_cmd_set_native_mode\n");

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

void ds_handle_command(struct ps2_sio2* sio2, void* udata) {
    struct ds_state* ds = (struct ds_state*)udata;

    uint8_t cmd = sio2->in->buf[1];

    switch (cmd) {
        case 0x40: ds_cmd_set_vref_param(sio2, ds); return;
        case 0x41: ds_cmd_query_masked(sio2, ds); return;
        case 0x42: ds_cmd_read_data(sio2, ds); return;
        case 0x43: ds_cmd_config_mode(sio2, ds); return;
        case 0x44: ds_cmd_set_mode(sio2, ds); return;
        case 0x45: ds_cmd_query_model(sio2, ds); return;
        case 0x46: ds_cmd_query_act(sio2, ds); return;
        case 0x47: ds_cmd_query_comb(sio2, ds); return;
        case 0x4C: ds_cmd_query_mode(sio2, ds); return;
        case 0x4D: ds_cmd_vibration_toggle(sio2, ds); return;
        case 0x4F: ds_cmd_set_native_mode(sio2, ds); return;
    }

    printf("ds: Unhandled command %02x\n", cmd);

    // exit(1);
}

struct ds_state* ds_sio2_attach(struct ps2_sio2* sio2, int port) {
    struct ds_state* ds = malloc(sizeof(struct ds_state));
    struct sio2_device dev;

    dev.handle_command = ds_handle_command;
    dev.udata = ds;

    memset(ds, 0, sizeof(struct ds_state));

    ds->port = port;
    ds->ax_right_y = 0x80;
    ds->ax_right_x = 0x80;
    ds->ax_left_y = 0x80;
    ds->ax_left_x = 0x80;
    ds->buttons = 0xffff;

    ps2_sio2_attach_device(sio2, dev, port);

    return ds;
}

void ds_button_press(struct ds_state* ds, uint16_t mask) {
    ds->buttons &= ~mask;
}

void ds_button_release(struct ds_state* ds, uint16_t mask) {
    ds->buttons |= mask;
}

void ds_analog_change(struct ds_state* ds, int axis, uint8_t value) {
    switch (axis) {
        case 0: ds->ax_right_y = value; break;
        case 1: ds->ax_right_x = value; break;
        case 2: ds->ax_left_y = value; break;
        case 3: ds->ax_left_x = value; break;
    }
}