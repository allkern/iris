/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <new>
#include "ds.hpp"

namespace iris::dev::ds {

static inline uint8_t get_model_byte(Ds* ds) {
    switch (ds->mode) {
        case 0: return 0x41;
        case 1: return 0x73;
        default: return 0x79;
    }
}
static inline void cmd_set_vref_param(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_set_vref_param");

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xf3);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x02);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x5a);
}
static inline void cmd_query_masked(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_query_masked");

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xf3);
    queue::push(sio2->out, 0x5a);

    if (!ds->mode) {
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
    } else {
        queue::push(sio2->out, ds->mask[0]);
        queue::push(sio2->out, ds->mask[1]);
        queue::push(sio2->out, 0x03);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x5a);
    }
}
static inline void push_buttons(sio2::Sio2* sio2, Ds* ds) {
    uint16_t buttons = ds->buttons & ~ds->held;

    queue::push(sio2->out, buttons & 0xff);
    queue::push(sio2->out, buttons >> 8);
}
static inline uint8_t axis_value(Ds* ds, int axis, uint8_t value) {
    return ds->axis_override[axis] < 0 ? value : (uint8_t)ds->axis_override[axis];
}
static inline void push_axes(sio2::Sio2* sio2, Ds* ds) {
    queue::push(sio2->out, axis_value(ds, RIGHT_H, ds->ax_right_x));
    queue::push(sio2->out, axis_value(ds, RIGHT_V, ds->ax_right_y));
    queue::push(sio2->out, axis_value(ds, LEFT_H, ds->ax_left_x));
    queue::push(sio2->out, axis_value(ds, LEFT_V, ds->ax_left_y));
}
static inline void cmd_read_data(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_read_data({:04x})", ds->buttons);

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, get_model_byte(ds));
    queue::push(sio2->out, 0x5a);

    push_buttons(sio2, ds);

    if (ds->mode) {
        push_axes(sio2, ds);

        // Push pressure bytes (only in DualShock 2 mode)
        // Note: Some games (e.g. OutRun 2 SP/2006) won't register inputs
        //       if the pressure values are 0, so we push the max value
        //       instead
        if (ds->mode == 2) {
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
            queue::push(sio2->out, 0xff);
        }
    }
}
static inline void cmd_config_mode(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_config_mode({:02x})", queue::at(sio2->in, 3));

    // Same as read_data, but without pressure data in DualShock 2 mode
    if (!ds->config_mode) {
        queue::push(sio2->out, 0xff);

        // We don't use the model byte here because
        // config_mode returns the same data as analog (DS1)
        // when not in config mode regardless of the model
        queue::push(sio2->out, ds->mode ? 0x73 : 0x41);
        queue::push(sio2->out, 0x5a);

        push_buttons(sio2, ds);

        if (ds->mode) {
            push_axes(sio2, ds);
        }
    } else {
        queue::push(sio2->out, 0xff);
        queue::push(sio2->out, 0xf3);
        queue::push(sio2->out, 0x5a);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
    }

    ds->config_mode = queue::at(sio2->in, 3);
}
static inline void cmd_set_mode(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_set_mode({:02x}, {:02x})", queue::at(sio2->in, 3), queue::at(sio2->in, 4));

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xf3);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);

    int mode = queue::at(sio2->in, 3);
    int lock = queue::at(sio2->in, 4);

    if (mode < 2 && !ds->lock) {
        ds->mode = mode ? 1 : 0;
    }

    ds->lock = lock == 3;
}
static inline void cmd_query_model(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_query_model");

    queue::push(sio2->out, 0xff); // Header
    queue::push(sio2->out, 0xf3); // Mode (F3=Config)
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x03); // Model (01=Dualshock/Digital 03=Dualshock 2)
    queue::push(sio2->out, 0x02);
    queue::push(sio2->out, !!ds->mode); // Analog (00=no 01=yes)
    queue::push(sio2->out, 0x02);
    queue::push(sio2->out, 0x01);
    queue::push(sio2->out, 0x00);
}
static inline void cmd_query_act(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_query_act({:02x})", queue::at(sio2->in, 3));

    int index = queue::at(sio2->in, 3);

    if (!index) {
        queue::push(sio2->out, 0xff);
        queue::push(sio2->out, 0xf3);
        queue::push(sio2->out, 0x5a);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x01);
        queue::push(sio2->out, 0x02);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x0a);
    } else {
        queue::push(sio2->out, 0xff);
        queue::push(sio2->out, 0xf3);
        queue::push(sio2->out, 0x5a);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x00);
        queue::push(sio2->out, 0x01);
        queue::push(sio2->out, 0x01);
        queue::push(sio2->out, 0x01);
        queue::push(sio2->out, 0x14);
    }
}
static inline void cmd_query_comb(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_query_comb");

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xf3);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x02);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x01);
    queue::push(sio2->out, 0x00);
}
static inline void cmd_query_mode(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_query_mode");

    int index = queue::at(sio2->in, 3);

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xf3);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, index ? 7 : 4);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
}
static inline void cmd_vibration_toggle(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_vibration_toggle");

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xf3);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, ds->vibration[0]);
    queue::push(sio2->out, ds->vibration[1]);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);

    ds->vibration[0] = queue::at(sio2->in, 3);
    ds->vibration[1] = queue::at(sio2->in, 4);
}
static inline void cmd_set_native_mode(sio2::Sio2* sio2, Ds* ds) {
    iris_debug(ds, "cmd_set_native_mode({:02x}, {:02x}, {:02x}, {:02x})", queue::at(sio2->in, 3),
        queue::at(sio2->in, 4),
        queue::at(sio2->in, 5),
        queue::at(sio2->in, 6));

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xf3);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x5a);

    ds->mask[0] = queue::at(sio2->in, 3);
    ds->mask[1] = queue::at(sio2->in, 4);

    int value = queue::at(sio2->in, 5);

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

void handle_command(sio2::Sio2* sio2, void* udata, int cmd) {
    Ds* ds = (Ds*)udata;

    switch (cmd) {
        case 0x40: cmd_set_vref_param(sio2, ds); return;
        case 0x41: cmd_query_masked(sio2, ds); return;
        case 0x42: cmd_read_data(sio2, ds); return;
        case 0x43: cmd_config_mode(sio2, ds); return;
        case 0x44: cmd_set_mode(sio2, ds); return;
        case 0x45: cmd_query_model(sio2, ds); return;
        case 0x46: cmd_query_act(sio2, ds); return;
        case 0x47: cmd_query_comb(sio2, ds); return;
        case 0x4C: cmd_query_mode(sio2, ds); return;
        case 0x4D: cmd_vibration_toggle(sio2, ds); return;
        case 0x4F: cmd_set_native_mode(sio2, ds); return;
    }

    iris_error(ds, "Unhandled command {:02x}", cmd);
}

Ds* attach(logger::Logger* logger, sio2::Sio2* sio2, int port) {
    Ds* ds = new Ds();

    ds->logger = logger;
    ds->logger_id = logger::register_source(logger, "ds");
    sio2::Device dev;

    dev.detach = detach;
    dev.reset = reset;
    dev.handle_command = handle_command;
    dev.udata = ds;
    dev.id = sio2::DEV_PAD;

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

    sio2::attach_device(sio2, dev, port);

    return ds;
}

void reset(void* udata) {
    Ds* ds = (Ds*)udata;

    ds->config_mode = 0;
    ds->act_index = 0;
    ds->mode_index = 0;
    ds->mode = 0;
    ds->lock = 0;

    ds->vibration[0] = 0xff;
    ds->vibration[1] = 0xff;
    ds->mask[0] = 0xff;
    ds->mask[1] = 0xff;
}

void button_press(Ds* ds, uint32_t mask) {
    if (mask == ANALOG) {
        if (!ds->lock)
            ds->mode = ds->mode ? 0 : 1;

        return;
    }

    ds->buttons &= ~mask;
}

void button_release(Ds* ds, uint32_t mask) {
    ds->buttons |= mask;
}

void analog_change(Ds* ds, int axis, uint8_t value) {
    switch (axis) {
        case 0: ds->ax_right_y = value; break;
        case 1: ds->ax_right_x = value; break;
        case 2: ds->ax_left_y = value; break;
        case 3: ds->ax_left_x = value; break;
    }
}

void detach(void* udata) {
    Ds* ds = (Ds*)udata;

    delete ds;
}

}
