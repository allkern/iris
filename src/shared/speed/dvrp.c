#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dvrp.h"

struct ps2_dvrp* ps2_dvrp_create(void) {
    return malloc(sizeof(struct ps2_dvrp));
}

void ps2_dvrp_init(struct ps2_dvrp* dvrp, struct ps2_speed* speed) {
    memset(dvrp, 0, sizeof(struct ps2_dvrp));

    dvrp->speed = speed;

    // bit 5 - IOMAN task
    // bit 4 - DVR task
    // bit 3 - AV task
    // bit 2 - DVR/MISC task
    // bit 1 - Busy
    dvrp->status = 0x3E;
}

void ps2_dvrp_destroy(struct ps2_dvrp* dvrp) {
    free(dvrp);
}

void dvrp_send_irq(struct ps2_dvrp* dvrp, uint16_t irq) {
    dvrp->intr_stat = irq;
    dvrp->intr_cause = dvrp->cmd;

    if (dvrp->intr_stat & dvrp->intr_mask) {
        ps2_speed_send_irq(dvrp->speed, SPD_INTR_DVR);
    }
}

void dvrp_send_intr_cmd_ack(void* udata, int overshoot) {
    struct ps2_dvrp* dvrp = (struct ps2_dvrp*)udata;

    dvrp_send_irq(dvrp, DVRP_INTR_CMD_ACK);
}

void dvrp_send_intr_cmd_comp(void* udata, int overshoot) {
    struct ps2_dvrp* dvrp = (struct ps2_dvrp*)udata;

    dvrp_send_irq(dvrp, DVRP_INTR_CMD_COMP);
}

void dvrp_handle_command(struct ps2_dvrp* dvrp, uint16_t cmd) {
    // printf("dvrp: Handle command %04x params=%d\n", cmd, dvrp->param_index);

    dvrp->cmd = cmd;
    dvrp->param_index = 0;

    struct sched_event event;

    event.callback = dvrp_send_intr_cmd_ack;
    event.udata = dvrp;
    event.cycles = 10000;
    event.name = "dvrp cmd ack";

    sched_schedule(dvrp->speed->sched, event);

    // 210e - dvrioctl2_rec_prohibit
    // 3109 - avioctl2_set_d_audio_sel
}

uint64_t ps2_dvrp_read(struct ps2_dvrp* dvrp, uint32_t addr) {
    // printf("dvrp: read16 %08x\n", addr);

    switch (addr) {
        case 0x4200: return dvrp->intr_stat;
        case 0x4208: return dvrp->intr_mask;
        case 0x4210: return dvrp->cmd;
        case 0x4214: return dvrp->params[dvrp->param_index];
        case 0x4218: return dvrp->status2;
        case 0x4220: return dvrp->intr_cause;
        case 0x4228: return 1; // ?
        case 0x4230: return dvrp->status;

        // ??
        case 0x4234: return 1;
        case 0x4238: return dvrp->status;
        case 0x423c: return dvrp->status;
    }

    return 0;
}

void dvrp_clear_speed_dvr_intr(struct ps2_dvrp* dvrp) {
    if ((dvrp->intr_stat & dvrp->intr_mask) == 0) {
        dvrp->speed->intr_stat &= ~SPD_INTR_DVR;
    }
}

void ps2_dvrp_write(struct ps2_dvrp* dvrp, uint32_t addr, uint64_t data) {
    // printf("dvrp: write16 %08x %08x\n", addr, data);

    switch (addr) {
        case 0x4204: dvrp->intr_stat &= ~data; dvrp_clear_speed_dvr_intr(dvrp); break;
        case 0x4208: dvrp->intr_mask = data; dvrp_clear_speed_dvr_intr(dvrp); break;
        case 0x4210: dvrp_handle_command(dvrp, data); break;
        case 0x4214: dvrp->params[dvrp->param_index++] = data; break;
        // case 0x4218: dvrp->status2 = data; break;
        case 0x4230: dvrp->status = data; break;
    }
}