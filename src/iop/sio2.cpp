#include <new>
#include <cstring>

#include "sio2.hpp"

namespace iris::sio2 {

static inline void reset_transfer(Sio2* sio2) {
    queue::clear(sio2->in);

    sio2->send3_index = 0;
}

Sio2* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched) {
    Sio2* sio2 = new Sio2();

    sio2->logger = logger;
    sio2->logger_id = logger::register_source(logger, "sio2");

    sio2->hw.intc = intc;
    sio2->hw.sched = sched;

    sio2->in = queue::create();
    sio2->out = queue::create();

    sio2->recv1 = 0x1d100;

    return sio2;
}

void send_irq(void* udata, int overshoot) {
    Sio2* sio2 = (Sio2*)udata;

    sio2->istat |= 1;

    iop::intc::irq(sio2->hw.intc, iop::intc::SIO2);
}

void dma_reset(Sio2* sio2) {
    queue::clear(sio2->in);
}

void connect(Sio2* sio2, iop::dma::Dma* dma) {
    sio2->hw.dma = dma;
}

void reset(Sio2* sio2) {
    reset_transfer(sio2);

    queue::clear(sio2->out);

    sio2->ctrl = 0;
    sio2->recv1 = 0x1d100;
    sio2->recv2 = 0;
    sio2->recv3 = 0;
    sio2->istat = 0;

    memset(sio2->send1, 0, sizeof(sio2->send1));
    memset(sio2->send2, 0, sizeof(sio2->send2));
    memset(sio2->send3, 0, sizeof(sio2->send3));

    for (int i = 0; i < 4; i++) {
        Device* dev = &sio2->port[i];

        if (dev->reset)
            dev->reset(dev->udata);
    }
}

void destroy(Sio2* sio2) {
    // Detach all devices
    for (int i = 0; i < 4; i++)
        detach_device(sio2, i);

    queue::destroy(sio2->in);
    queue::destroy(sio2->out);

    delete sio2;
}

static inline int handle_command(Sio2* sio2, int idx) {
    int devid = queue::at(sio2->in, 0);
    int cmd = queue::at(sio2->in, 1);
    int p = sio2->send3[idx] & 3;
    int len = (sio2->send3[idx] >> 18) & 0xff;

    // iris_debug(sio2, "Trying command {:02x} in SEND3[{}] port {} devid {:02x} (in: {} ({}), len: {})", cmd,
    //     idx, p, devid, queue::size(sio2->in), queue::size(sio2->in) + 2, len
    //);

    // If we're sending a pad command, make sure it's only for ports
    // 0-1, and if it's a memory card command, make sure it's only
    // for ports 2-3
    int pad = devid == DEV_PAD;
    int mcd = devid == DEV_MCD;
    int mtap = devid == DEV_MTAP;

    if (!(pad || mcd))
        return 0;

    // Check if the port is actually connected
    if (!sio2->port[p].handle_command)
        return 0;

    // iris_debug(sio2, "Sending command {:02x} to port {} with devid {:02x} (in: {} ({}), len: {})", //     cmd, p, devid, queue::size(sio2->in), queue::size(sio2->in) + 2, len
    //);

    // Send command
    sio2->port[p].handle_command(sio2, sio2->port[p].udata, cmd);

    // Clear current command parameters
    for (int i = 0; i < len; i++)
        queue::pop(sio2->in);

    // Weird behavior, DMA MCD commands are aligned to a 36-word boundary
    if (mcd) {
        while (queue::size(sio2->out) % 0x90)
            queue::push(sio2->out, 0);

        if (!queue::is_empty(sio2->in)) {
            // Remove input padding
            while (sio2->in->index % 0x90)
                queue::pop(sio2->in);
        }
    }

    // iris_debug(sio2, "FIFOOUT size: {} ({:x})", sio2->out->size, sio2->out->size);

    // if (cmd == 0x81) exit(1);

    return 1;
}

static inline void write_ctrl(Sio2* sio2, uint32_t data) {
    sio2->ctrl = data & ~1;

    if (data & 0xc) {
        // iris_debug(sio2, "Controller reset");

        queue::clear(sio2->out);
    }

    // Send command bit
    if ((data & 1) == 0)
        return;

    // Disconnected by default
    sio2->recv1 = 0x1d100;

    // Send IRQ no matter what
    sio2->istat |= 1;

    iop::intc::irq(sio2->hw.intc, iop::intc::SIO2);

    // iris_debug(sio2, "Sending command {:02x} to port {} with devid {:02x} (in: {}, len: {})", //     cmd, p, devid, queue::size(sio2->in), len
    //);

    for (int i = 0; i < 16; i++) {
        // Break when we find a null command
        if (!sio2->send3[i])
            break;

        // If any of the commands were handled, set RECV1 to 0x1100
        if (handle_command(sio2, i)) {
            if (sio2->recv1 & 0x2000) {
                sio2->recv1 = 0x1d100;
            } else {
                sio2->recv1 = 0x1100;
            }
        }
    }

    // Complete SIO2_out transfer after all commands have executed
    // For commands that use DMA (mostly MCD commands), this will handle
    // reading from the output FIFO. Will do nothing on commands
    // that don't use DMA, because the IOP needs to start a transfer
    // before sending a DMA command, and if a command executed, but
    // didn't actually put anything in the FIFO, the DMA request
    // will be cleared. (e.g. when a memory card isn't connected)
    iop::dma::end_sio2_out_transfer(sio2->hw.dma);
}

uint64_t read8(Sio2* sio2, uint32_t addr) {
    switch (addr) {
        // case 0x1F808260: /* iris_debug(sio2, "8-bit FIFOIN read"); */ return 0;
        case 0x1F808264: {
            if (queue::is_empty(sio2->out)) {
                // iris_debug(sio2, "FIFOOUT read {:02x}", 0x00);
                return 0x00;
            }

            uint8_t b = queue::pop(sio2->out);

            // iris_debug(sio2, "FIFOOUT read {:02x}", b);

            return b;
        } break;
        // case 0x1F808268: /* iris_debug(sio2, "8-bit CTRL read"); */ return 0; 
        // case 0x1F80826C: /* iris_debug(sio2, "8-bit RECV1 read"); */ return 0;
        // case 0x1F808270: /* iris_debug(sio2, "8-bit RECV2 read"); */ return 0;
        // case 0x1F808274: /* iris_debug(sio2, "8-bit RECV3 read"); */ return 0;
        // case 0x1F808280: /* iris_debug(sio2, "8-bit ISTAT read"); */ return 0;
        // default: {
        //     if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
        // Memcard or non-controller access
        //         // iris_debug(sio2, "8-bit SEND3 read");
        //     }
        //     if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
        //         // iris_debug(sio2, "8-bit SEND{} read", (addr & 4) ? 2 : 1);
        //     }
        // } break;
    }

    iris_fatal_error(sio2, "Unhandled 8-bit read from address {:08x}", addr);

    return 0;
}

uint64_t read32(Sio2* sio2, uint32_t addr) {
    switch (addr) {
        // case 0x1F808260: /* iris_debug(sio2, "32-bit FIFOIN read"); */ return 0;
        // case 0x1F808264: /* iris_debug(sio2, "32-bit FIFOOUT read"); */ return 0;
        case 0x1F808268: /* iris_debug(sio2, "32-bit CTRL read"); */ return 0;
        case 0x1F80826C: /* iris_debug(sio2, "32-bit RECV1 read"); */ return sio2->recv1;
        case 0x1F808270: /* iris_debug(sio2, "32-bit RECV2 read"); */ return 0xf;
        case 0x1F808274: /* iris_debug(sio2, "32-bit RECV3 read"); */ return 0;
        case 0x1F808280: /* iris_debug(sio2, "32-bit ISTAT read"); */ return sio2->istat;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                // iris_debug(sio2, "32-bit SEND3 read");

                return sio2->send3[(addr & 0x3f) >> 2];
            }
            // if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
            //     // iris_debug(sio2, "32-bit SEND{} read", (addr & 4) ? 2 : 1);
            // }
        } break;
    }

    iris_fatal_error(sio2, "Unhandled 32-bit read from address {:08x}", addr);

    return 0;
}

void write8(Sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1F808260: /* iris_debug(sio2, "FIFOIN write {:02x}", data); */ queue::push(sio2->in, data); return;
        // case 0x1F808264: /* iris_debug(sio2, "8-bit FIFOOUT write {:02x}", data); */ return;
        case 0x1F808268: write_ctrl(sio2, data); return;
        // case 0x1F80826C: /* iris_debug(sio2, "8-bit RECV1 write {:02x}", data); */ return;
        // case 0x1F808270: /* iris_debug(sio2, "8-bit RECV2 write {:02x}", data); */ return;
        // case 0x1F808274: /* iris_debug(sio2, "8-bit RECV3 write {:02x}", data); */ return;
        // case 0x1F808280: /* iris_debug(sio2, "8-bit ISTAT write {:02x}", data); */ return;
        // default: {
        //     if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
        //         // iris_debug(sio2, "8-bit SEND3 write {:02x}", data);
        //     }
        //     if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
        //         // iris_debug(sio2, "8-bit SEND{} write {:02x}", (addr & 4) ? 2 : 1, data);
        //     }
        // } break;
    }

    iris_fatal_error(sio2, "Unhandled 8-bit write to address {:08x}", addr);
}

void write32(Sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        // case 0x1F808260: /* iris_debug(sio2, "32-bit FIFOIN write {:08x}", data); */ return;
        // case 0x1F808264: /* iris_debug(sio2, "32-bit FIFOOUT write {:08x}", data); */ return;
        case 0x1F808268: write_ctrl(sio2, data); return;
        // case 0x1F80826C: /* iris_debug(sio2, "32-bit RECV1 write {:08x}", data); */ return;
        // case 0x1F808270: /* iris_debug(sio2, "32-bit RECV2 write {:08x}", data); */ return;
        // case 0x1F808274: /* iris_debug(sio2, "32-bit RECV3 write {:08x}", data); */ return;
        case 0x1F808280: /* iris_debug(sio2, "32-bit ISTAT write {:08x}", data); */ sio2->istat &= ~(data & 3); return;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                int index = (addr & 0x3f) >> 2;

                sio2->send3[index] = data;

                if (!index) reset_transfer(sio2);

                // iris_debug(sio2, "32-bit SEND3 write {:08x}", data);

                return;
            }
            if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
                // iris_debug(sio2, "32-bit SEND{} write {:08x}", (addr & 4) ? 2 : 1, data);

                return;
            }
        } break;
    }

    iris_debug(sio2, "Unhandled 32-bit write to address {:08x} ({:08x})", addr, data); // exit(1);
}

void attach_device(Sio2* sio2, Device dev, int port) {
    sio2->port[port] = dev;
}

void detach_device(Sio2* sio2, int port) {
    Device* dev = &sio2->port[port];

    if (dev->detach)
        dev->detach(dev->udata);

    dev->handle_command = 0;
    dev->detach = 0;
    dev->reset = 0;
    dev->udata = NULL;
}

}
