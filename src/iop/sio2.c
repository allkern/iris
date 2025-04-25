#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sio2.h"

struct ps2_sio2* ps2_sio2_create(void) {
    return malloc(sizeof(struct ps2_sio2));
}

void sio2_send_irq(void* udata, int overshoot) {
    struct ps2_sio2* sio2 = (struct ps2_sio2*)udata;

    sio2->istat |= 1;

    ps2_iop_intc_irq(sio2->intc, IOP_INTC_SIO2);
}

void sio2_dma_reset(struct ps2_sio2* sio2) {
    queue_clear(sio2->in);
}

void ps2_sio2_init(struct ps2_sio2* sio2, struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct sched_state* sched) {
    memset(sio2, 0, sizeof(struct ps2_sio2));

    sio2->intc = intc;
    sio2->dma = dma;

    sio2->in = queue_create();
    sio2->out = queue_create();

    queue_init(sio2->in);
    queue_init(sio2->out);

    sio2->sched = sched;
}

void ps2_sio2_destroy(struct ps2_sio2* sio2) {
    queue_destroy(sio2->in);
    queue_destroy(sio2->out);

    free(sio2);
}

static inline void sio2_handle_ctrl_write(struct ps2_sio2* sio2, uint32_t data) {
    sio2->ctrl = data & ~1;

    if (data & 0xc) {
        // printf("sio2: Controller reset\n");

        queue_clear(sio2->in);
        queue_clear(sio2->out);
    }

    if ((data & 1) == 0)
        return;

    sio2->istat |= 1;

    ps2_iop_intc_irq(sio2->intc, IOP_INTC_SIO2);

    // for (int i = 0; i < 16; i++) {
        // if (!sio2->send3[i])
        //     continue;

        int devid = sio2->in->buf[0];
        int cmd = sio2->in->buf[1];
        int p = sio2->send3[0] & 3;
        int len = (sio2->send3[0] >> 18) & 0xff;

        // printf("sio2: Sending command %02x to port %d with devid %02x (in: %d)\n",
        //     cmd, p, devid, queue_size(sio2->in)
        // );

        if ((devid != SIO2_DEV_PAD) || (p != 0)) {
            sio2->recv1 = 0x1d100;

            return;
        }

        // if (devid != SIO2_DEV_PAD) {
        //     sio2->recv1 = 0x1d100;

        //     queue_clear(sio2->in);

        //     return;
        // }

        // // Ports 2 and 3 are memory card ports
        // if ((p >= 2) && (devid != SIO2_DEV_MCD)) {
        //     printf("sio2: Trying to access non-mcd device with devid %02x in mcd port %d\n", devid);

        //     sio2->recv1 = 0x1d100;

        //     queue_clear(sio2->in);

        //     return;
        // }

        // // Port disconnected
        // if (!sio2->port[p].handle_command) {
        //     sio2->recv1 = 0x1d100;

        //     queue_clear(sio2->in);

        //     return;
        // }

        struct sched_event event;

        event.callback = sio2_send_irq;
        event.cycles = 1000;
        event.name = "SIO2 Command IRQ";
        event.udata = sio2;

        sched_schedule(sio2->sched, event);

        // Device connected, send command
        sio2->recv1 = 0x1100;

        // printf("sio2: Sending command to port %d with devid %02x\n", n, devid);

        sio2->port[p].handle_command(sio2, sio2->port[p].udata);

        // printf("sio2: Dequeing %d bytes from input FIFO (size=%d)\n", len, queue_size(sio2->in));

        // for (int j = 0; j < len; j++)
        //     queue_pop(sio2->in);

        // iop_dma_handle_sio2_out_transfer(sio2->dma);
    // }
}

uint64_t ps2_sio2_read8(struct ps2_sio2* sio2, uint32_t addr) {
    switch (addr) {
        // case 0x1F808260: /* printf("8-bit SIO2_FIFOIN read\n"); */ return 0;
        case 0x1F808264: /* printf("sio2: FIFOOUT read %02x\n", data); */ return queue_pop(sio2->out);
        // case 0x1F808268: /* printf("8-bit SIO2_CTRL read\n"); */ return 0; 
        // case 0x1F80826C: /* printf("8-bit SIO2_RECV1 read\n"); */ return 0;
        // case 0x1F808270: /* printf("8-bit SIO2_RECV2 read\n"); */ return 0;
        // case 0x1F808274: /* printf("8-bit SIO2_RECV3 read\n"); */ return 0;
        // case 0x1F808280: /* printf("8-bit SIO2_ISTAT read\n"); */ return 0;
        // default: {
        //     if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
        // Memcard or non-controller access
        //         // printf("8-bit SIO2_SEND3 read\n");
        //     }
        //     if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
        //         // printf("8-bit SIO2_SEND%d read\n", (addr & 4) ? 2 : 1);
        //     }
        // } break;
    }

    // printf("sio2: Unhandled 8-bit read from address %08x\n", addr); exit(1);

    return 0;
}

uint64_t ps2_sio2_read32(struct ps2_sio2* sio2, uint32_t addr) {
    switch (addr) {
        // case 0x1F808260: /* printf("32-bit SIO2_FIFOIN read\n"); */ return 0;
        // case 0x1F808264: /* printf("32-bit SIO2_FIFOOUT read\n"); */ return 0;
        case 0x1F808268: /* printf("32-bit SIO2_CTRL read\n"); */ return 0;
        case 0x1F80826C: /* printf("32-bit SIO2_RECV1 read\n"); */ return sio2->recv1;
        case 0x1F808270: /* printf("32-bit SIO2_RECV2 read\n"); */ return 0xf;
        case 0x1F808274: /* printf("32-bit SIO2_RECV3 read\n"); */ return 0;
        case 0x1F808280: /* printf("32-bit SIO2_ISTAT read\n"); */ return sio2->istat;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                // printf("32-bit SIO2_SEND3 read\n");

                return sio2->send3[(addr & 0x3f) >> 2];
            }
            // if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
            //     // printf("32-bit SIO2_SEND%d read\n", (addr & 4) ? 2 : 1);
            // }
        } break;
    }

    // printf("sio2: Unhandled 32-bit read from address %08x\n", addr); exit(1);

    return 0;
}

void ps2_sio2_write8(struct ps2_sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1F808260: /* printf("sio2: FIFOIN write %02x\n", data); */ queue_push(sio2->in, data); return;
        // case 0x1F808264: /* printf("8-bit SIO2_FIFOOUT write %02x\n", data); */ return;
        case 0x1F808268: sio2_handle_ctrl_write(sio2, data); return;
        // case 0x1F80826C: /* printf("8-bit SIO2_RECV1 write %02x\n", data); */ return;
        // case 0x1F808270: /* printf("8-bit SIO2_RECV2 write %02x\n", data); */ return;
        // case 0x1F808274: /* printf("8-bit SIO2_RECV3 write %02x\n", data); */ return;
        // case 0x1F808280: /* printf("8-bit SIO2_ISTAT write %02x\n", data); */ return;
        // default: {
        //     if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
        //         // printf("8-bit SIO2_SEND3 write %02x\n", data);
        //     }
        //     if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
        //         // printf("8-bit SIO2_SEND%d write %02x\n", (addr & 4) ? 2 : 1, data);
        //     }
        // } break;
    }

    // printf("sio2: Unhandled 8-bit write to address %08x\n", addr); exit(1);
}

void ps2_sio2_write32(struct ps2_sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        // case 0x1F808260: /* printf("32-bit SIO2_FIFOIN write %08x\n", data); */ return;
        // case 0x1F808264: /* printf("32-bit SIO2_FIFOOUT write %08x\n", data); */ return;
        case 0x1F808268: sio2_handle_ctrl_write(sio2, data); return;
        // case 0x1F80826C: /* printf("32-bit SIO2_RECV1 write %08x\n", data); */ return;
        // case 0x1F808270: /* printf("32-bit SIO2_RECV2 write %08x\n", data); */ return;
        // case 0x1F808274: /* printf("32-bit SIO2_RECV3 write %08x\n", data); */ return;
        case 0x1F808280: /* printf("32-bit SIO2_ISTAT write %08x\n", data); */ sio2->istat &= data & 3; return;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                sio2->send3[(addr & 0x3f) >> 2] = data;

                // printf("sio2: 32-bit SEND3 write %08x\n", data);

                return;
            }
            if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
                // printf("32-bit SIO2_SEND%d write %08x\n", (addr & 4) ? 2 : 1, data);
            }
        } break;
    }

    // printf("sio2: Unhandled 32-bit write to address %08x (%08x)\n", addr, data); // exit(1);
}

void ps2_sio2_attach_device(struct ps2_sio2* sio2, struct sio2_device dev, int port) {
    sio2->port[port] = dev;
}

void ps2_sio2_detach_device(struct ps2_sio2* sio2, int port) {
    sio2->port[port].handle_command = 0;
    sio2->port[port].udata = NULL;
}