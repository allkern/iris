#include "ds.h"

#include <stdlib.h>
#include <string.h>

#define printf(fmt, ...)(0)

static inline uint8_t ds_get_model_byte(struct ds_state* ds) {
    switch (ds->mode) {
        case 0: return 0x41;
        case 1: return 0x73;
        case 2: return 0x79;
    }
}
static inline void ds_cmd_set_vref_param(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_set_vref_param\n");

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
    printf("ds: ds_cmd_query_masked\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);

    if (!ds->mode) {
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
    } else {
        queue_push(sio2->out, ds->mask[0]);
        queue_push(sio2->out, ds->mask[1]);
        queue_push(sio2->out, 0x03);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x00);
        queue_push(sio2->out, 0x5a);
    }
}
static inline void ds_cmd_read_data(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_read_data(%04x)\n", ds->buttons);

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, ds_get_model_byte(ds));
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, ds->buttons & 0xff);
    queue_push(sio2->out, ds->buttons >> 8);

    if (ds->mode) {
        queue_push(sio2->out, ds->ax_right_x);
        queue_push(sio2->out, ds->ax_right_y);
        queue_push(sio2->out, ds->ax_left_x);
        queue_push(sio2->out, ds->ax_left_y);

        // Push pressure bytes (only in DualShock 2 mode)
        // Note: Some games (e.g. OutRun 2 SP/2006) won't register inputs
        //       if the pressure values are 0, so we push the max value
        //       instead
        if (ds->mode == 2) {
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
            queue_push(sio2->out, 0xff);
        }
    }
}
static inline void ds_cmd_config_mode(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_config_mode(%02x)\n", queue_at(sio2->in, 3));

    // Same as read_data, but without pressure data in DualShock 2 mode
    if (!ds->config_mode) {
        queue_push(sio2->out, 0xff);

        // We don't use the model byte here because
        // config_mode returns the same data as analog (DS1)
        // when not in config mode regardless of the model
        queue_push(sio2->out, ds->mode ? 0x73 : 0x41);
        queue_push(sio2->out, 0x5a);
        queue_push(sio2->out, ds->buttons & 0xff);
        queue_push(sio2->out, ds->buttons >> 8);

        if (ds->mode) {
            queue_push(sio2->out, ds->ax_right_x);
            queue_push(sio2->out, ds->ax_right_y);
            queue_push(sio2->out, ds->ax_left_x);
            queue_push(sio2->out, ds->ax_left_y);
        }
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

    ds->config_mode = queue_at(sio2->in, 3);
}
static inline void ds_cmd_set_mode(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_set_mode(%02x, %02x)\n", queue_at(sio2->in, 3), queue_at(sio2->in, 4));

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);

    int mode = queue_at(sio2->in, 3);
    int lock = queue_at(sio2->in, 4);

    if (mode < 2 && !ds->lock) {
        ds->mode = mode ? 1 : 0;
    }

    ds->lock = lock == 3;
}
static inline void ds_cmd_query_model(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_query_model\n");

    queue_push(sio2->out, 0xff); // Header
    queue_push(sio2->out, 0xf3); // Mode (F3=Config)
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x03); // Model (01=Dualshock/Digital 03=Dualshock 2)
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, !!ds->mode); // Analog (00=no 01=yes)
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, 0x01);
    queue_push(sio2->out, 0x00);
}
static inline void ds_cmd_query_act(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_query_act(%02x)\n", queue_at(sio2->in, 3));

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
static inline void ds_cmd_query_comb(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_query_comb\n");

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
    printf("ds: ds_cmd_query_mode\n");

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
static inline void ds_cmd_vibration_toggle(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_vibration_toggle\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xf3);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, ds->vibration[0]);
    queue_push(sio2->out, ds->vibration[1]);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);

    ds->vibration[0] = queue_at(sio2->in, 3);
    ds->vibration[1] = queue_at(sio2->in, 4);
}
static inline void ds_cmd_set_native_mode(struct ps2_sio2* sio2, struct ds_state* ds) {
    printf("ds: ds_cmd_set_native_mode(%02x, %02x, %02x, %02x)\n",
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

    ds->mask[0] = queue_at(sio2->in, 3);
    ds->mask[1] = queue_at(sio2->in, 4);

    int value = queue_at(sio2->in, 5);

    if ((value & 1) == 0) {
        // Digital mode
        ds->mode = 0;
    } else if ((value & 2) == 0) {
        // Analog mode
        ds->mode = 1;
    } else {
        // DualShock 2 mode
        ds->mode = 2;
    }
}

void ds_handle_command(struct ps2_sio2* sio2, void* udata, int cmd) {
    struct ds_state* ds = (struct ds_state*)udata;

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

    fprintf(stderr, "ds: Unhandled command %02x\n", cmd);

    // exit(1);
}

struct ds_state* ds_attach(struct ps2_sio2* sio2, int port) {
    struct ds_state* ds = malloc(sizeof(struct ds_state));
    struct sio2_device dev;

    dev.detach = ds_detach;
    dev.handle_command = ds_handle_command;
    dev.udata = ds;

    memset(ds, 0, sizeof(struct ds_state));

    ds->port = port;
    ds->ax_right_y = 0x7f;
    ds->ax_right_x = 0x7f;
    ds->ax_left_y = 0x7f;
    ds->ax_left_x = 0x7f;
    ds->buttons = 0xffff;
    ds->vibration[0] = 0xff;
    ds->vibration[1] = 0xff;
    ds->mask[0] = 0xff;
    ds->mask[1] = 0xff;

    // Start in digital mode
    ds->mode = 0;
    ds->lock = 0;

    ps2_sio2_attach_device(sio2, dev, port);

    return ds;
}

void ds_button_press(struct ds_state* ds, uint32_t mask) {
    if (mask == DS_BT_ANALOG) {
        if (!ds->lock)
            ds->mode = ds->mode ? 0 : 1;

        return;
    }

    ds->buttons &= ~mask;
}

void ds_button_release(struct ds_state* ds, uint32_t mask) {
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

void ds_detach(void* udata) {
    struct ds_state* ds = (struct ds_state*)udata;

    free(ds);
}