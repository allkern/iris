#include <new>
#include "guncon.hpp"

namespace iris::dev::guncon {

// 
static inline uint8_t get_model_byte(Guncon* guncon) {
    return 0x63;
}
static inline void cmd_set_vref_param(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_set_vref_param");

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
static inline void cmd_query_masked(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_query_masked");

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
static inline void cmd_read_data(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_read_data({:04x})", guncon->buttons);

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, get_model_byte(guncon));
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, guncon->buttons & 0xff);
    queue::push(sio2->out, guncon->buttons >> 8);
    queue::push(sio2->out, guncon->x & 0xff);
    queue::push(sio2->out, guncon->x >> 8);
    queue::push(sio2->out, guncon->y & 0xff);
    queue::push(sio2->out, guncon->y >> 8);
}
static inline void cmd_config_mode(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_config_mode({:02x})", queue::at(sio2->in, 3));

    // Same as read_data, but without pressure data in DualShock 2 mode
    if (!guncon->config_mode) {
        queue::push(sio2->out, 0xff);

        // We don't use the model byte here because
        // config_mode returns the same data as analog (GUNCON1)
        // when not in config mode regardless of the model
        queue::push(sio2->out, 0x63);
        queue::push(sio2->out, 0x5a);
        queue::push(sio2->out, guncon->buttons & 0xff);
        queue::push(sio2->out, guncon->buttons >> 8);
        queue::push(sio2->out, guncon->x & 0xff);
        queue::push(sio2->out, guncon->x >> 8);
        queue::push(sio2->out, guncon->y & 0xff);
        queue::push(sio2->out, guncon->y >> 8);
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

    guncon->config_mode = queue::at(sio2->in, 3);
}
static inline void cmd_set_mode(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_set_mode({:02x}, {:02x})", queue::at(sio2->in, 3), queue::at(sio2->in, 4));

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
static inline void cmd_query_model(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_query_model");

    queue::push(sio2->out, 0xff); // Header
    queue::push(sio2->out, 0xf3); // Mode (F3=Config)
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x03); // Model (01=Dualshock/Digital 03=Dualshock 2)
    queue::push(sio2->out, 0x02);
    queue::push(sio2->out, 0x00); // Analog (00=no 01=yes)
    queue::push(sio2->out, 0x02);
    queue::push(sio2->out, 0x01);
    queue::push(sio2->out, 0x00);
}
static inline void cmd_query_act(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_query_act({:02x})", queue::at(sio2->in, 3));

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
static inline void cmd_query_comb(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_query_comb");

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
static inline void cmd_query_mode(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_query_mode");

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
static inline void cmd_vibration_toggle(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_vibration_toggle");

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xf3);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);
}
static inline void cmd_set_native_mode(sio2::Sio2* sio2, Guncon* guncon) {
    iris_debug(guncon, "cmd_set_native_mode({:02x}, {:02x}, {:02x}, {:02x})", queue::at(sio2->in, 3),
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
}

void handle_command(sio2::Sio2* sio2, void* udata, int cmd) {
    Guncon* guncon = (Guncon*)udata;

    switch (cmd) {
        case 0x40: cmd_set_vref_param(sio2, guncon); return;
        case 0x41: cmd_query_masked(sio2, guncon); return;
        case 0x42: cmd_read_data(sio2, guncon); return;
        case 0x43: cmd_config_mode(sio2, guncon); return;
        case 0x44: cmd_set_mode(sio2, guncon); return;
        case 0x45: cmd_query_model(sio2, guncon); return;
        case 0x46: cmd_query_act(sio2, guncon); return;
        case 0x47: cmd_query_comb(sio2, guncon); return;
        case 0x4C: cmd_query_mode(sio2, guncon); return;
        case 0x4D: cmd_vibration_toggle(sio2, guncon); return;
        case 0x4F: cmd_set_native_mode(sio2, guncon); return;
    }

    iris_fatal_error(guncon, "Unhandled command {:02x}", cmd);
}

Guncon* attach(logger::Logger* logger, sio2::Sio2* sio2, int port) {
    Guncon* guncon = new Guncon();

    guncon->logger = logger;
    guncon->logger_id = logger::register_source(logger, "guncon");
    sio2::Device dev;

    dev.detach = detach;
    dev.handle_command = handle_command;
    dev.udata = guncon;

    guncon->port = port;
    guncon->config_mode = 0;
    guncon->x = 0x10d;
    guncon->y = 0x88;
    guncon->buttons = 0xffff;

    sio2::attach_device(sio2, dev, port);

    return guncon;
}

void button_press(Guncon* guncon, uint16_t mask) {
    guncon->buttons &= ~mask;
}

void button_release(Guncon* guncon, uint16_t mask) {
    guncon->buttons |= mask;
}

void analog_change(Guncon* guncon, int axis, uint8_t value) {
    switch (axis) {
        case 0: guncon->x = value; break;
        case 1: guncon->y = value; break;
    }
}

void detach(void* udata) {
    Guncon* guncon = (Guncon*)udata;

    delete guncon;
}

}
