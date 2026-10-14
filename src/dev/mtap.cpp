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
    dev.id = sio2::DEV_MTAP;

    sio2::attach_device(sio2, dev, port);

    return mtap;
}

void detach(void* udata) {
    Mtap* mtap = (Mtap*)udata;

    delete mtap;
}

}
