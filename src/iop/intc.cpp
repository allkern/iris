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

#include "intc.hpp"

namespace iris::iop::intc {

Intc* create(logger::Logger* logger, iop::Iop* iop) {
    Intc* intc = new Intc();

    intc->logger = logger;
    intc->logger_id = logger::register_source(logger, "iop_intc");

    intc->hw.iop = iop;

    reset(intc);

    return intc;
}

void reset(Intc* intc) {
    auto hw = intc->hw;

    logger::Logger* logger = intc->logger;
    size_t logger_id = intc->logger_id;

    new (intc) Intc();

    intc->logger = logger;
    intc->logger_id = logger_id;

    intc->hw = hw;

    intc->ctrl = 1;
}

void irq(Intc* intc, int dev) {
    intc->stat |= dev;

    iop::set_irq_pending(intc->hw.iop, intc->ctrl && (intc->stat & intc->mask));
}

void destroy(Intc* intc) {
    delete intc;
}

uint64_t read8(Intc* intc, uint32_t addr) {
    iris_fatal_error(intc, "IOP intc 8-bit read from address {:08x}", addr);

    return 0;
}

uint64_t read16(Intc* intc, uint32_t addr) {
    iris_fatal_error(intc, "IOP intc 16-bit read from address {:08x}", addr);

    return 0;
}

uint64_t read32(Intc* intc, uint32_t addr) {
    uint32_t ctrl = intc->ctrl;

    switch (addr) {
        case 0x1f801070: return intc->stat;
        case 0x1f801074: return intc->mask;
        case 0x1f801078: intc->ctrl = 0; break;
    }

    iop::set_irq_pending(intc->hw.iop, intc->ctrl && (intc->stat & intc->mask));

    return ctrl;
}

void write8(Intc* intc, uint32_t addr, uint64_t data) {
    iris_fatal_error(intc, "iop: IOP INTC 8-bit write to address {:08x} ({:02x})", addr, data);
}

void write16(Intc* intc, uint32_t addr, uint64_t data) {
    iris_fatal_error(intc, "iop: IOP INTC 8-bit write to address {:08x} ({:04x})", addr, data);
}

void write32(Intc* intc, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1f801070: intc->stat &= data; break;
        case 0x1f801074: intc->mask = data; break;
        case 0x1f801078: intc->ctrl = data; break;
    }

    iop::set_irq_pending(intc->hw.iop, intc->ctrl && (intc->stat & intc->mask));
}

}
