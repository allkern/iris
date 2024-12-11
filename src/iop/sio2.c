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

uint64_t ps2_sio2_read8(struct ps2_sio2* sio2, uint32_t addr) {

    switch (addr) {
        case 0x1F808260: printf("8-bit SIO2_FIFOIN read\n"); return 0;
        case 0x1F808264: printf("8-bit SIO2_FIFOOUT read\n"); return 0;
        case 0x1F808268: printf("8-bit SIO2_CTRL read\n"); return 0; 
        case 0x1F80826C: printf("8-bit SIO2_RECV1 read\n"); return 0;
        case 0x1F808270: printf("8-bit SIO2_RECV2 read\n"); return 0;
        case 0x1F808274: printf("8-bit SIO2_RECV3 read\n"); return 0;
        case 0x1F808280: printf("8-bit SIO2_ISTAT read\n"); return 0;
    }
    return 0;
}

uint64_t ps2_sio2_read32(struct ps2_sio2* sio2, uint32_t addr) {
    // case 0x1F808200-1F80823Fh SIO2_SEND3 - Command Parameters
    // case 0x1F808240-1F80825Fh SIO2_SEND1/SEND2 - Port1/2 Control?

    switch (addr) {
        case 0x1F808260: printf("32-bit SIO2_FIFOIN read\n"); return 0;
        case 0x1F808264: printf("32-bit SIO2_FIFOOUT read\n"); return 0;
        case 0x1F808268: return sio2->ctrl;
        case 0x1F80826C: printf("32-bit SIO2_RECV1 read\n"); return 0;
        case 0x1F808270: printf("32-bit SIO2_RECV2 read\n"); return 0;
        case 0x1F808274: printf("32-bit SIO2_RECV3 read\n"); return 0;
        case 0x1F808280: printf("32-bit SIO2_ISTAT read\n"); return 0;
    }
}

void ps2_sio2_write8(struct ps2_sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1F808260: ps2_iop_intc_irq(sio2->intc, IOP_INTC_SIO2); return;
        case 0x1F808264: printf("8-bit SIO2_FIFOOUT write %02x\n", data); return;
        case 0x1F808268: printf("8-bit SIO2_CTRL write %02x\n", data); return;
        case 0x1F80826C: printf("8-bit SIO2_RECV1 write %02x\n", data); return;
        case 0x1F808270: printf("8-bit SIO2_RECV2 write %02x\n", data); return;
        case 0x1F808274: printf("8-bit SIO2_RECV3 write %02x\n", data); return;
        case 0x1F808280: printf("8-bit SIO2_ISTAT write %02x\n", data); return;
    }
}

void ps2_sio2_write32(struct ps2_sio2* sio2, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1F808260: printf("32-bit SIO2_FIFOIN write %08x\n", data); return;
        case 0x1F808264: printf("32-bit SIO2_FIFOOUT write %08x\n", data); return;
        case 0x1F808268: printf("32-bit SIO2_CTRL write %08x\n", data); exit(1); return;
        case 0x1F80826C: printf("32-bit SIO2_RECV1 write %08x\n", data); return;
        case 0x1F808270: printf("32-bit SIO2_RECV2 write %08x\n", data); return;
        case 0x1F808274: printf("32-bit SIO2_RECV3 write %08x\n", data); return;
        case 0x1F808280: printf("32-bit SIO2_ISTAT write %08x\n", data); return;
    }
}
