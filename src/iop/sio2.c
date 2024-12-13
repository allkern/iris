#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sio2.h"

struct ps2_sio2* ps2_sio2_create(void) {
    return malloc(sizeof(struct ps2_sio2));
}

void ps2_sio2_init(struct ps2_sio2* sio2, struct ps2_iop_intc* intc) {
    memset(sio2, 0, sizeof(struct ps2_sio2));

    sio2->intc = intc;
    sio2->in = queue_create();
    sio2->out = queue_create();

    queue_init(sio2->in);
    queue_init(sio2->out);
}

void ps2_sio2_destroy(struct ps2_sio2* sio2) {
    queue_destroy(sio2->in);
    queue_destroy(sio2->out);

    free(sio2);
}

static inline void sio2_handle_ctrl_write(struct ps2_sio2* sio2, uint8_t data) {
    sio2->ctrl = data;

    if (data & 12) {
        // printf("sio2: Controller reset\n");
        queue_clear(sio2->in);
        queue_clear(sio2->out);
    }

    if (data & 1) {
        int n = sio2->send3[0] & 3;

        // printf("sio2: Sending command %02x to port %d\n", sio2->in->buf[1], n);

        sio2->port[n].handle_command(sio2, sio2->port[n].udata);

        ps2_iop_intc_irq(sio2->intc, IOP_INTC_SIO2);
    }
}

uint64_t ps2_sio2_read8(struct ps2_sio2* sio2, uint32_t addr) {
    switch (addr) {
        case 0x1F808260: /* printf("8-bit SIO2_FIFOIN read\n"); */ return 0;
        case 0x1F808264: /* printf("sio2: FIFOOUT read %02x\n", data); */ return queue_pop(sio2->out);;
        case 0x1F808268: /* printf("8-bit SIO2_CTRL read\n"); */ return 0; 
        case 0x1F80826C: /* printf("8-bit SIO2_RECV1 read\n"); */ return 0;
        case 0x1F808270: /* printf("8-bit SIO2_RECV2 read\n"); */ return 0;
        case 0x1F808274: /* printf("8-bit SIO2_RECV3 read\n"); */ return 0;
        case 0x1F808280: /* printf("8-bit SIO2_ISTAT read\n"); */ return 0;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                // printf("8-bit SIO2_SEND3 read\n");
            }
            if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
                // printf("8-bit SIO2_SEND%d read\n", (addr & 4) ? 2 : 1);
            }
        } break;
    }

    return 0;
}

uint64_t ps2_sio2_read32(struct ps2_sio2* sio2, uint32_t addr) {
    switch (addr) {
        case 0x1F808260: /* printf("32-bit SIO2_FIFOIN read\n"); */ return 0;
        case 0x1F808264: /* printf("32-bit SIO2_FIFOOUT read\n"); */ return 0;
        case 0x1F808268: /* printf("32-bit SIO2_CTRL read\n"); */ return sio2->ctrl;
        case 0x1F80826C: /* printf("32-bit SIO2_RECV1 read\n"); */ return 0x1100;
        case 0x1F808270: /* printf("32-bit SIO2_RECV2 read\n"); */ return 0xf;
        case 0x1F808274: /* printf("32-bit SIO2_RECV3 read\n"); */ return 0;
        case 0x1F808280: /* printf("32-bit SIO2_ISTAT read\n"); */ return 0;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                // printf("32-bit SIO2_SEND3 read\n");

                return sio2->send3[(addr & 0x3f) >> 2];
            }
            if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
                // printf("32-bit SIO2_SEND%d read\n", (addr & 4) ? 2 : 1);
            }
        } break;
    }

    return 0;
}

void ps2_sio2_write8(struct ps2_sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1F808260: /* printf("sio2: FIFOIN write %02x\n", data); */ queue_push(sio2->in, data); return;
        case 0x1F808264: /* printf("8-bit SIO2_FIFOOUT write %02x\n", data); */ return;
        case 0x1F808268: sio2_handle_ctrl_write(sio2, data); return;
        case 0x1F80826C: /* printf("8-bit SIO2_RECV1 write %02x\n", data); */ return;
        case 0x1F808270: /* printf("8-bit SIO2_RECV2 write %02x\n", data); */ return;
        case 0x1F808274: /* printf("8-bit SIO2_RECV3 write %02x\n", data); */ return;
        case 0x1F808280: /* printf("8-bit SIO2_ISTAT write %02x\n", data); */ return;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                // printf("8-bit SIO2_SEND3 write %02x\n", data);
            }
            if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
                // printf("8-bit SIO2_SEND%d write %02x\n", (addr & 4) ? 2 : 1, data);
            }
        } break;
    }
}

void ps2_sio2_write32(struct ps2_sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1F808260: /* printf("32-bit SIO2_FIFOIN write %08x\n", data); */ return;
        case 0x1F808264: /* printf("32-bit SIO2_FIFOOUT write %08x\n", data); */ return;
        case 0x1F808268: sio2_handle_ctrl_write(sio2, data); return;
        case 0x1F80826C: /* printf("32-bit SIO2_RECV1 write %08x\n", data); */ return;
        case 0x1F808270: /* printf("32-bit SIO2_RECV2 write %08x\n", data); */ return;
        case 0x1F808274: /* printf("32-bit SIO2_RECV3 write %08x\n", data); */ return;
        case 0x1F808280: /* printf("32-bit SIO2_ISTAT write %08x\n", data); */ return;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                sio2->send3[(addr & 0x3f) >> 2] = data;

                // printf("32-bit SIO2_SEND3 write %08x\n", data);
            }
            if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
                // printf("32-bit SIO2_SEND%d write %08x\n", (addr & 4) ? 2 : 1, data);
            }
        } break;
    }
}

void ps2_sio2_attach_device(struct ps2_sio2* sio2, struct sio2_device dev, int port) {
    sio2->port[port] = dev;
}

void ps2_sio2_detach_device(struct ps2_sio2* sio2, int port) {
    sio2->port[port].handle_command = 0;
    sio2->port[port].udata = NULL;
}