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

#include "accore.hpp"

namespace iris::s2x6::accore {

Accore* create(logger::Logger* logger, iop::intc::Intc* intc, acjv::Acjv* acjv) {
    Accore* accore = new Accore();

    accore->logger = logger;
    accore->logger_id = logger::register_source(logger, "accore");

    accore->intc = intc;
    accore->acjv = acjv;

    return accore;
}

void destroy(Accore* accore) {
    delete accore;
}

static uint16_t cause_bit(int source) {
    return source == IRQ_ATA ? CAUSE_ATA : CAUSE_UART;
}

static const char* source_name(int source) {
    return source == IRQ_ATA ? "ATA" : "UART";
}

void irq(Accore* accore, int source) {
    if (source != IRQ_ATA && source != IRQ_UART)
        return;

    uint16_t bit = cause_bit(source);

    if (accore->cause & bit) {
        accore->pending[source]++;

        iris_debug(accore, "Queue {} pending {}", source_name(source), accore->pending[source]);

        return;
    }

    accore->cause |= bit;

    iris_debug(accore, "Raise {} cause {:04x}", source_name(source), accore->cause);

    iop::intc::irq(accore->intc, iop::intc::DEV9);
}

static void ack(Accore* accore, int source) {
    uint16_t bit = cause_bit(source);

    accore->cause &= ~bit;

    if (!accore->pending[source]) {
        iris_debug(accore, "Ack {} cause {:04x}", source_name(source), accore->cause);

        return;
    }

    accore->pending[source]--;
    accore->cause |= bit;

    iris_debug(accore, "Ack {} re-raised, pending {}", source_name(source), accore->pending[source]);

    iop::intc::irq(accore->intc, iop::intc::DEV9);
}

static bool is_ignored(uint32_t addr) {
    for (uint32_t ignored : R_IGNORED)
        if (addr == ignored)
            return true;

    return false;
}

uint64_t read16(Accore* accore, uint32_t addr) {
    if (addr == R_CAUSE)
        return accore->cause;

    if (addr != accore->last_unhandled_read) {
        accore->last_unhandled_read = addr;

        iris_warning(accore, "Unhandled read {:08x}", addr);
    }

    return 0;
}

void write16(Accore* accore, uint32_t addr, uint64_t data) {
    switch (addr) {
        case R_JVS_START: {
            acjv::start(accore->acjv);
        } return;

        case R_JVS_STOP: return;

        case R_DISABLE_ATA_IRQ:
        case R_DISABLE_UART_IRQ: return;

        case R_FPGA_PROGRAM_BEGIN: {
            accore->cause |= CAUSE_FPGA_BUSY;
        } return;

        case R_FPGA_PROGRAM_END: {
            accore->cause &= ~CAUSE_FPGA_BUSY;
        } return;

        case R_ACK_ATA: {
            ack(accore, IRQ_ATA);
        } return;

        case R_ACK_UART: {
            ack(accore, IRQ_UART);
        } return;
    }

    if (is_ignored(addr))
        return;

    if (addr != accore->last_unhandled_write) {
        accore->last_unhandled_write = addr;

        iris_warning(accore, "Unhandled write {:08x} = {:04x}", addr, (uint16_t)data);
    }
}

}
