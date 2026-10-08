#include <new>

#include "rm.hpp"

namespace iris::dev::rm {

static inline void push_reply(sio2::Sio2* sio2, uint8_t id, uint8_t status, uint32_t code) {
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, id);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, status);
    queue::push(sio2->out, code & 0xff);
    queue::push(sio2->out, (code >> 8) & 0xff);
    queue::push(sio2->out, (code >> 16) & 0xff);
}

static inline void cmd_find(sio2::Sio2* sio2, Rm* rm) {
    iris_debug(rm, "cmd_find");

    push_reply(sio2, rm->initialized ? 0x12 : 0x1f, 0x00, 0);
}

static inline void cmd_init(sio2::Sio2* sio2, Rm* rm) {
    rm->power_mode = queue::at(sio2->in, 2);
    rm->initialized = 1;

    iris_debug(rm, "cmd_init({:02x})", rm->power_mode);

    push_reply(sio2, 0x12, 0x00, 0);
}

static inline void cmd_poll(sio2::Sio2* sio2, Rm* rm) {
    uint32_t code = next_code(rm->key.load(), rm->phase);

    rm->polls++;

    push_reply(sio2, 0x12, code ? 0x14 : 0x00, code);
}

void handle_command(sio2::Sio2* sio2, void* udata, int cmd) {
    Rm* rm = (Rm*)udata;

    switch (cmd) {
        case 0x04: cmd_poll(sio2, rm); return;
        case 0x06: cmd_init(sio2, rm); return;
        case 0x0f: cmd_find(sio2, rm); return;
    }

    iris_error(rm, "Unhandled command {:02x}", cmd);

    push_reply(sio2, 0xff, 0xff, 0xffffff);
}

uint32_t next_code(Key key, int& phase) {
    if (!key.code[1]) {
        phase = 0;

        return key.code[0];
    }

    uint32_t code = key.code[phase];

    phase ^= 1;

    return code;
}

Rm* attach(logger::Logger* logger, sio2::Sio2* sio2, int port) {
    Rm* rm = new Rm();

    rm->logger = logger;
    rm->logger_id = logger::register_source(logger, "rm");
    sio2::Device dev;

    dev.detach = detach;
    dev.reset = reset;
    dev.handle_command = handle_command;
    dev.udata = rm;
    dev.id = sio2::DEV_IR;

    rm->port = port;

    sio2::attach_device(sio2, dev, port);

    return rm;
}

void set_key(Rm* rm, Key key) {
    rm->key.store(key);
}

void release(Rm* rm) {
    rm->key.store(Key {});
}

void reset(void* udata) {
    Rm* rm = (Rm*)udata;

    rm->initialized = 0;
    rm->power_mode = 0;
    rm->phase = 0;
}

void detach(void* udata) {
    Rm* rm = (Rm*)udata;

    delete rm;
}

}
