#include <new>

#include "mtap.hpp"

namespace iris::dev::mtap {

void cmd_probe(sio2::Sio2* sio2, Mtap* mtap) {
    iris_debug(mtap, "cmd_probe");

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0x80);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x04);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x5a);
}

void handle_command(sio2::Sio2* sio2, void* udata, int cmd) {
    Mtap* mtap = (Mtap*)udata;

    switch (cmd) {
        case 0x12: cmd_probe(sio2, mtap); return;
    }

    iris_fatal_error(mtap, "Unhandled command {:02x}", cmd);
}

Mtap* attach(logger::Logger* logger, sio2::Sio2* sio2, int port) {
    Mtap* mtap = new Mtap();

    mtap->logger = logger;
    mtap->logger_id = logger::register_source(logger, "mtap");
    sio2::Device dev;

    dev.detach = detach;
    dev.handle_command = handle_command;
    dev.udata = mtap;

    sio2::attach_device(sio2, dev, port);

    return mtap;
}

void detach(void* udata) {
    Mtap* mtap = (Mtap*)udata;

    delete mtap;
}

}
