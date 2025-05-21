#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sio2.h"

static inline void sio2_reset(struct ps2_sio2* sio2) {
    queue_clear(sio2->in);

    sio2->send3_index = 0;
}

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
    sio2->recv1 = 0x1d100;
}

void ps2_sio2_destroy(struct ps2_sio2* sio2) {
    queue_destroy(sio2->in);
    queue_destroy(sio2->out);

    free(sio2);
}

static inline int sio2_handle_command(struct ps2_sio2* sio2, int idx) {
    int devid = queue_at(sio2->in, 0);
    int cmd = queue_at(sio2->in, 1);
    int p = sio2->send3[idx] & 3;
    int len = (sio2->send3[idx] >> 18) & 0xff;

    // printf("sio2: Trying command %02x in SEND3[%d] port %d devid %02x (in: %d (%d), len: %d)\n", cmd,
    //     idx, p, devid, queue_size(sio2->in), queue_size(sio2->in) + 2, len
    // );

    // If we're sending a pad command, make sure it's only for ports
    // 0-1, and if it's a memory card command, make sure it's only
    // for ports 2-3
    int pad = devid == SIO2_DEV_PAD;
    int mcd = devid == SIO2_DEV_MCD;

    if (!(pad || mcd))
        return 0;

    // Check if the port is actually connected
    if (!sio2->port[p].handle_command)
        return 0;

    // printf("sio2: Sending command %02x to port %d with devid %02x (in: %d (%d), len: %d)\n",
    //     cmd, p, devid, queue_size(sio2->in), queue_size(sio2->in) + 2, len
    // );

    // Send command
    sio2->port[p].handle_command(sio2, sio2->port[p].udata, cmd);

    // Clear current command parameters
    for (int i = 0; i < len; i++)
        queue_pop(sio2->in);

    // Weird behavior, DMA MCD commands are aligned to a 36-word boundary
    if (mcd) {
        while (sio2->out->size % 0x90)
            queue_push(sio2->out, 0);

        if (!queue_is_empty(sio2->in)) {
            // Remove input padding
            while (sio2->in->index % 0x90)
                queue_pop(sio2->in);
        }
    }

    // printf("sio2: FIFOOUT size: %d (%x)\n", sio2->out->size, sio2->out->size);

    // if (cmd == 0x81) exit(1);

    return 1;
}

static inline void sio2_write_ctrl(struct ps2_sio2* sio2, uint32_t data) {
    sio2->ctrl = data & ~1;

    if (data & 0xc) {
        // printf("sio2: Controller reset\n");

        queue_clear(sio2->out);
    }

    // Send command bit
    if ((data & 1) == 0)
        return;

    // Disconnected by default
    sio2->recv1 = 0x1d100;

    // Send IRQ no matter what
    sio2->istat |= 1;

    ps2_iop_intc_irq(sio2->intc, IOP_INTC_SIO2);

    // printf("sio2: Sending command %02x to port %d with devid %02x (in: %d, len: %d)\n",
    //     cmd, p, devid, queue_size(sio2->in), len
    // );

    for (int i = 0; i < 16; i++) {
        // Break when we find a null command
        if (!sio2->send3[i])
            break;

        // If any of the commands were handled, set RECV1 to 0x1100
        if (sio2_handle_command(sio2, i)) {
            sio2->recv1 = 0x1100;
        }
    }

    // Complete SIO2_out transfer after all commands have executed
    // For commands that use DMA (mostly MCD commands), this will handle
    // reading from the output FIFO. Will do nothing on commands
    // that don't use DMA, because the IOP needs to start a transfer
    // before sending a DMA command, and if a command executed, but
    // didn't actually put anything in the FIFO, the DMA request
    // will be cleared. (e.g. when a memory card isn't connected)
    iop_dma_end_sio2_out_transfer(sio2->dma);
}

uint64_t ps2_sio2_read8(struct ps2_sio2* sio2, uint32_t addr) {
    switch (addr) {
        // case 0x1F808260: /* printf("8-bit SIO2_FIFOIN read\n"); */ return 0;
        case 0x1F808264: {
            if (queue_is_empty(sio2->out)) {
                // printf("sio2: SIO2_FIFOOUT read %02x\n", 0x00);
                return 0x00;
            }

            uint8_t b = queue_pop(sio2->out);

            // printf("sio2: SIO2_FIFOOUT read %02x\n", b);

            return b;
        } break;
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

    printf("sio2: Unhandled 8-bit read from address %08x\n", addr); exit(1);

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

    printf("sio2: Unhandled 32-bit read from address %08x\n", addr); exit(1);

    return 0;
}

void ps2_sio2_write8(struct ps2_sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1F808260: /* printf("sio2: FIFOIN write %02x\n", data); */ queue_push(sio2->in, data); return;
        // case 0x1F808264: /* printf("8-bit SIO2_FIFOOUT write %02x\n", data); */ return;
        case 0x1F808268: sio2_write_ctrl(sio2, data); return;
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

    printf("sio2: Unhandled 8-bit write to address %08x\n", addr); exit(1);
}

void ps2_sio2_write32(struct ps2_sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        // case 0x1F808260: /* printf("32-bit SIO2_FIFOIN write %08x\n", data); */ return;
        // case 0x1F808264: /* printf("32-bit SIO2_FIFOOUT write %08x\n", data); */ return;
        case 0x1F808268: sio2_write_ctrl(sio2, data); return;
        // case 0x1F80826C: /* printf("32-bit SIO2_RECV1 write %08x\n", data); */ return;
        // case 0x1F808270: /* printf("32-bit SIO2_RECV2 write %08x\n", data); */ return;
        // case 0x1F808274: /* printf("32-bit SIO2_RECV3 write %08x\n", data); */ return;
        case 0x1F808280: /* printf("32-bit SIO2_ISTAT write %08x\n", data); */ sio2->istat &= ~(data & 3); return;
        default: {
            if (addr >= 0x1F808200 && addr <= 0x1F80823F) {
                int index = (addr & 0x3f) >> 2;

                sio2->send3[index] = data;

                if (!index) sio2_reset(sio2);

                // printf("sio2: 32-bit SEND3 write %08x\n", data);

                return;
            }
            if (addr >= 0x1F808240 && addr <= 0x1F80825F) {
                // printf("32-bit SIO2_SEND%d write %08x\n", (addr & 4) ? 2 : 1, data);

                return;
            }
        } break;
    }

    printf("sio2: Unhandled 32-bit write to address %08x (%08x)\n", addr, data); // exit(1);
}

void ps2_sio2_attach_device(struct ps2_sio2* sio2, struct sio2_device dev, int port) {
    sio2->port[port] = dev;
}

void ps2_sio2_detach_device(struct ps2_sio2* sio2, int port) {
    struct sio2_device* dev = &sio2->port[port];

    if (dev->detach)
        dev->detach(dev->udata);

    dev->handle_command = 0;
    dev->detach = 0;
    dev->udata = NULL;
}