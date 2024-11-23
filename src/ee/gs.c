#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gs.h"
#include "intc.h"
#include "iop/intc.h"

struct ps2_gs* ps2_gs_create(void) {
    return malloc(sizeof(struct ps2_gs));
}

static inline void gs_invoke_event_handler(struct ps2_gs* gs, int event) {
    if (gs->events[event].func)
        gs->events[event].func(gs->events[event].udata);
}

void gs_handle_vblank_in(void* udata, int overshoot);

void gs_handle_vblank_out(void* udata, int overshoot) {
    struct ps2_gs* gs = (struct ps2_gs*)udata;

    struct sched_event vblank_event;

    vblank_event.callback = gs_handle_vblank_in;
    vblank_event.cycles = 4489019;
    vblank_event.name = "Vblank in event";
    vblank_event.udata = gs;

    sched_schedule(gs->sched, vblank_event);

    // Tell backend to render scene
    if (gs->backend.render)
        gs->backend.render(gs, gs->backend.udata);

    gs_invoke_event_handler(gs, GS_EVENT_VBLANK);

    // Set Vblank flag?
    gs->csr ^= 8;
    gs->csr ^= 1 << 13;

    // Send Vblank IRQ through INTC
    ps2_intc_irq(gs->ee_intc, EE_INTC_VBLANK_OUT);
    ps2_iop_intc_irq(gs->iop_intc, IOP_INTC_VBLANK_OUT);
}

void gs_handle_vblank_in(void* udata, int overshoot) {
    struct ps2_gs* gs = (struct ps2_gs*)udata;

    struct sched_event vblank_out_event;

    vblank_out_event.callback = gs_handle_vblank_out;
    vblank_out_event.cycles = 4310; 
    vblank_out_event.name = "Vblank out event";
    vblank_out_event.udata = gs;

    // Set Vblank flag?
    gs->csr ^= 8;

    // Send Vblank IRQ through INTC
    ps2_intc_irq(gs->ee_intc, EE_INTC_VBLANK_IN);
    ps2_iop_intc_irq(gs->iop_intc, IOP_INTC_VBLANK_IN);

    sched_schedule(gs->sched, vblank_out_event);
}

void ps2_gs_init(struct ps2_gs* gs, struct ps2_intc* ee_intc, struct ps2_iop_intc* iop_intc, struct sched_state* sched) {
    memset(gs, 0, sizeof(struct ps2_gs));

    gs->sched = sched;
    gs->ee_intc = ee_intc;
    gs->iop_intc = iop_intc;
    gs->vram = malloc(0x400000); // 4 MB

    // Schedule Vblank event
    struct sched_event vblank_event;

    vblank_event.callback = gs_handle_vblank_in;
    vblank_event.cycles = 4489019;
    vblank_event.name = "Vblank in event";
    vblank_event.udata = gs;

    sched_schedule(gs->sched, vblank_event);
}

void gs_switch_context(struct ps2_gs* gs, int c) {
    gs->ctx = &gs->context[c];
}

void ps2_gs_destroy(struct ps2_gs* gs) {
    free(gs);
}

void gs_start_primitive(struct ps2_gs* gs) {
    gs->vqi = 0;
}

void gs_write_vertex(struct ps2_gs* gs, uint64_t data, int discard) {
    gs->vq[gs->vqi].xyz = data;
    gs->vq[gs->vqi].fog = gs->fog;
    gs->vq[gs->vqi].st = gs->st;
    gs->vq[gs->vqi].uv = gs->uv;
    gs->vq[gs->vqi++].rgbaq = gs->rgbaq;
    gs->attr = (gs->prmodecont & 1) ? gs->prmode : gs->prim;

    gs_switch_context(gs, (gs->attr & ATTR_CTXT) ? 1 : 0);

    switch (gs->prim & 7) {
        case 0: if (gs->vqi == 1) { gs->backend.render_point(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 1: if (gs->vqi == 2) { gs->backend.render_line(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 2: {
            if (gs->vqi == 2) {
                if (!discard)
                    gs->backend.render_line(gs, gs->backend.udata);
            } else if (gs->vqi == 3) {
                gs->vq[0] = gs->vq[1];
                gs->vq[1] = gs->vq[2];

                if (!discard)
                    gs->backend.render_line(gs, gs->backend.udata);

                gs->vqi = 2;
            }
        } break;
        case 3: if (gs->vqi == 3) { if (!discard) gs->backend.render_triangle(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 4: {
            if (gs->vqi == 3) {
                if (!discard) gs->backend.render_triangle(gs, gs->backend.udata);
            } else if (gs->vqi == 4) {
                gs->vq[0] = gs->vq[1];
                gs->vq[1] = gs->vq[2];
                gs->vq[2] = gs->vq[3];

                if (!discard) gs->backend.render_triangle(gs, gs->backend.udata);

                gs->vqi = 3;
            }
        } break;
        case 5: {
            if (gs->vqi == 3) {
                if (!discard) gs->backend.render_triangle(gs, gs->backend.udata);
            } else if (gs->vqi == 4) {
                gs->vq[1] = gs->vq[2];
                gs->vq[2] = gs->vq[3];

                if (!discard) gs->backend.render_triangle(gs, gs->backend.udata);

                gs->vqi = 3;
            }
        } break;
        case 6: if (gs->vqi == 2) { if (!discard) gs->backend.render_sprite(gs, gs->backend.udata); gs->vqi = 0; } break;
        default: {
            printf("gs: Reserved primitive %d\n", gs->prim & 7);
            exit(1);
        } break;
    }
}

uint64_t ps2_gs_read64(struct ps2_gs* gs, uint32_t addr) {
    switch (addr) {
        case 0x12000000: return gs->pmode;
        case 0x12000010: return gs->smode1;
        case 0x12000020: return gs->smode2;
        case 0x12000030: return gs->srfsh;
        case 0x12000040: return gs->synch1;
        case 0x12000050: return gs->synch2;
        case 0x12000060: return gs->syncv;
        case 0x12000070: return gs->dispfb1;
        case 0x12000080: return gs->display1;
        case 0x12000090: return gs->dispfb2;
        case 0x120000A0: return gs->display2;
        case 0x120000B0: return gs->extbuf;
        case 0x120000C0: return gs->extdata;
        case 0x120000D0: return gs->extwrite;
        case 0x120000E0: return gs->bgcolor;
        case 0x12001000: return gs->csr;
        case 0x12001010: return gs->imr;
        case 0x12001040: return gs->busdir;
        case 0x12001080: return gs->siglblid;
    }

    return 0;
}

void ps2_gs_write64(struct ps2_gs* gs, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x12000000: gs->pmode = data; return;
        case 0x12000010: gs->smode1 = data; return;
        case 0x12000020: gs->smode2 = data; return;
        case 0x12000030: gs->srfsh = data; return;
        case 0x12000040: gs->synch1 = data; return;
        case 0x12000050: gs->synch2 = data; return;
        case 0x12000060: gs->syncv = data; return;
        case 0x12000070: gs->dispfb1 = data; return;
        case 0x12000080: gs->display1 = data; return;
        case 0x12000090: gs->dispfb2 = data; return;
        case 0x120000A0: gs->display2 = data; return;
        case 0x120000B0: gs->extbuf = data; return;
        case 0x120000C0: gs->extdata = data; return;
        case 0x120000D0: gs->extwrite = data; return;
        case 0x120000E0: gs->bgcolor = data; return;
        case 0x12001000: gs->csr = data; return;
        case 0x12001010: gs->imr = data; return;
        case 0x12001040: gs->busdir = data; return;
        case 0x12001080: gs->siglblid = data; return;
    }
}

void ps2_gs_write_internal(struct ps2_gs* gs, int reg, uint64_t data) {
    switch (reg) {
        case 0x00: gs->prim = data; gs_start_primitive(gs); return;
        case 0x01: gs->rgbaq = data; return;
        case 0x02: gs->st = data; return;
        case 0x03: gs->uv = data; return;
        case 0x04: gs->xyzf2 = data; gs_write_vertex(gs, gs->xyzf2, 0); return;
        case 0x05: gs->xyz2 = data; gs_write_vertex(gs, gs->xyz2, 0); return;
        case 0x06: gs->context[0].tex0 = data; return;
        case 0x07: gs->context[1].tex0 = data; return;
        case 0x08: gs->context[0].clamp = data; return;
        case 0x09: gs->context[1].clamp = data; return;
        case 0x0A: gs->fog = data; return;
        case 0x0C: gs->xyzf3 = data; gs_write_vertex(gs, gs->xyzf3, 1); return;
        case 0x0D: gs->xyz3 = data; gs_write_vertex(gs, gs->xyz3, 1); return;
        case 0x14: gs->context[0].tex1 = data; return;
        case 0x15: gs->context[1].tex1 = data; return;
        case 0x16: gs->context[0].tex2 = data; return;
        case 0x17: gs->context[1].tex2 = data; return;
        case 0x18: gs->context[0].xyoffset = data; return;
        case 0x19: gs->context[1].xyoffset = data; return;
        case 0x1A: gs->prmodecont = data; return;
        case 0x1B: gs->prmode = data; return;
        case 0x1C: gs->texclut = data; return;
        case 0x22: gs->scanmsk = data; return;
        case 0x34: gs->context[0].miptbp1 = data; return;
        case 0x35: gs->context[1].miptbp1 = data; return;
        case 0x36: gs->context[0].miptbp2 = data; return;
        case 0x37: gs->context[1].miptbp2 = data; return;
        case 0x3B: gs->texa = data; return;
        case 0x3D: gs->fogcol = data; return;
        case 0x3F: gs->texflush = data; return;
        case 0x40: gs->context[0].scissor = data; gs_invoke_event_handler(gs, GS_EVENT_SCISSOR); return;
        case 0x41: gs->context[1].scissor = data; return;
        case 0x42: gs->context[0].alpha = data; return;
        case 0x43: gs->context[1].alpha = data; return;
        case 0x44: gs->dimx = data; return;
        case 0x45: gs->dthe = data; return;
        case 0x46: gs->colclamp = data; return;
        case 0x47: gs->context[0].test = data; return;
        case 0x48: gs->context[1].test = data; return;
        case 0x49: gs->pabe = data; return;
        case 0x4A: gs->context[0].fba = data; return;
        case 0x4B: gs->context[1].fba = data; return;
        case 0x4C: gs->context[0].frame = data; return;
        case 0x4D: gs->context[1].frame = data; return;
        case 0x4E: gs->context[0].zbuf = data; return;
        case 0x4F: gs->context[1].zbuf = data; return;
        case 0x50: gs->bitbltbuf = data; return;
        case 0x51: gs->trxpos = data; return;
        case 0x52: gs->trxreg = data; return;
        case 0x53: gs->trxdir = data; gs->backend.transfer_start(gs, gs->backend.udata); return; // gs_start_transfer(gs); return;
        case 0x54: gs->hwreg = data; gs->backend.transfer_write(gs, gs->backend.udata); return; // gs_transfer_write(gs); return;
        case 0x60: gs->signal = data; return;
        case 0x61: gs->finish = data; return;
        case 0x62: gs->label = data; return;
    }
}

uint64_t ps2_gs_read_internal(struct ps2_gs* gs, int reg) {
    switch (reg) {
        case 0x00: return gs->prim;
        case 0x01: return gs->rgbaq;
        case 0x02: return gs->st;
        case 0x03: return gs->uv;
        case 0x04: return gs->xyzf2;
        case 0x05: return gs->xyz2;
        case 0x06: return gs->context[0].tex0;
        case 0x07: return gs->context[1].tex0;
        case 0x08: return gs->context[0].clamp;
        case 0x09: return gs->context[1].clamp;
        case 0x0A: return gs->fog;
        case 0x0C: return gs->xyzf3;
        case 0x0D: return gs->xyz3;
        case 0x14: return gs->context[0].tex1;
        case 0x15: return gs->context[1].tex1;
        case 0x16: return gs->context[0].tex2;
        case 0x17: return gs->context[1].tex2;
        case 0x18: return gs->context[0].xyoffset;
        case 0x19: return gs->context[1].xyoffset;
        case 0x1A: return gs->prmodecont;
        case 0x1B: return gs->prmode;
        case 0x1C: return gs->texclut;
        case 0x22: return gs->scanmsk;
        case 0x34: return gs->context[0].miptbp1;
        case 0x35: return gs->context[1].miptbp1;
        case 0x36: return gs->context[0].miptbp2;
        case 0x37: return gs->context[1].miptbp2;
        case 0x3B: return gs->texa;
        case 0x3D: return gs->fogcol;
        case 0x3F: return gs->texflush;
        case 0x40: return gs->context[0].scissor;
        case 0x41: return gs->context[1].scissor;
        case 0x42: return gs->context[0].alpha;
        case 0x43: return gs->context[1].alpha;
        case 0x44: return gs->dimx;
        case 0x45: return gs->dthe;
        case 0x46: return gs->colclamp;
        case 0x47: return gs->context[0].test;
        case 0x48: return gs->context[1].test;
        case 0x49: return gs->pabe;
        case 0x4A: return gs->context[0].fba;
        case 0x4B: return gs->context[1].fba;
        case 0x4C: return gs->context[0].frame;
        case 0x4D: return gs->context[1].frame;
        case 0x4E: return gs->context[0].zbuf;
        case 0x4F: return gs->context[1].zbuf;
        case 0x50: return gs->bitbltbuf;
        case 0x51: return gs->trxpos;
        case 0x52: return gs->trxreg;
        case 0x53: return gs->trxdir;
        case 0x54: gs->backend.transfer_read(gs, gs->backend.udata); return gs->hwreg;
        case 0x60: return gs->signal;
        case 0x61: return gs->finish;
        case 0x62: return gs->label;
    }

    return 0;
}

void ps2_gs_init_callback(struct ps2_gs* gs, int event, void (*func)(void*), void* udata) {
    gs->events[event].func = func;
    gs->events[event].udata = udata;
}