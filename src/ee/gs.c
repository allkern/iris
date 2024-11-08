#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gs.h"
#include "intc.h"
#include "iop/intc.h"

#include "gs_software.h"
#include "gs_opengl.h"

struct ps2_gs* ps2_gs_create(void) {
    return malloc(sizeof(struct ps2_gs));
}

void gs_handle_vblank_in(void* udata, int overshoot);

void gs_handle_vblank_out(void* udata, int overshoot) {
    struct ps2_gs* gs = (struct ps2_gs*)udata;

    struct sched_event vblank_event;

    vblank_event.callback = gs_handle_vblank_in;
    vblank_event.cycles = 4489019;
    vblank_event.name = "Vblank event";
    vblank_event.udata = gs;

    if (gs->vblank_callback)
        gs->vblank_callback(gs->vblank_udata);

    // Tell backend to render scene
    gs->backend.render(gs, gs->backend.udata);

    // Set Vblank flag?
    gs->csr &= !8;
    gs->csr ^= 1 << 13;

    // Send Vblank IRQ through INTC
    ps2_intc_irq(gs->ee_intc, EE_INTC_VBLANK_OUT);
    ps2_iop_intc_irq(gs->iop_intc, IOP_INTC_VBLANK_OUT);

    sched_schedule(gs->sched, vblank_event);
}

void gs_handle_vblank_in(void* udata, int overshoot) {
    struct ps2_gs* gs = (struct ps2_gs*)udata;

    struct sched_event vblank_out_event;

    vblank_out_event.callback = gs_handle_vblank_out;
    vblank_out_event.cycles = 431096; 
    vblank_out_event.name = "Vblank out event";
    vblank_out_event.udata = gs;

    // Set Vblank flag?
    gs->csr |= 8;

    // Send Vblank IRQ through INTC
    ps2_intc_irq(gs->ee_intc, EE_INTC_VBLANK_IN);
    ps2_iop_intc_irq(gs->iop_intc, IOP_INTC_VBLANK_IN);

    printf("serving vblankin\n");

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

    struct gsr_opengl* opengl_ctx = malloc(sizeof(struct gsr_opengl));

    gs->backend.render_point = gsr_sw_render_point;
    gs->backend.render_line = gsr_sw_render_line;
    gs->backend.render_line_strip = gsr_sw_render_line_strip;
    gs->backend.render_triangle = gsr_sw_render_triangle;
    gs->backend.render_triangle_strip = gsr_sw_render_triangle_strip;
    gs->backend.render_triangle_fan = gsr_sw_render_triangle_fan;
    gs->backend.render_sprite = gsr_sw_render_sprite;
    gs->backend.render = gsr_sw_render;
    gs->backend.udata = &opengl_ctx;

    gsr_gl_init(opengl_ctx);
}

void ps2_gs_destroy(struct ps2_gs* gs) {
    free(gs);
}

static inline void gs_vram_blit(struct ps2_gs* gs) {
    for (int y = 0; y < gs->rrh; y++) {
        uint32_t src = gs->sbp + gs->ssax + (gs->ssay * gs->sbw) + (y * gs->sbw);
        uint32_t dst = gs->dbp + gs->dsax + (gs->dsay * gs->dbw) + (y * gs->dbw);

        // printf("gs: ssa=(%d, %d) -> dsa=(%d, %d) rr=(%d, %d) sbw=%d dbw=%d sbp=%08x dbp=%08x\n",
        //     gs->ssax, gs->ssay,
        //     gs->dsax, gs->dsay,
        //     gs->rrw, gs->rrh,
        //     gs->sbw, gs->dbw,
        //     gs->sbp, gs->dbp
        // );
        
        memcpy(gs->vram + dst, gs->vram + src, gs->rrw * sizeof(uint32_t));
    }
}

void gs_start_transfer(struct ps2_gs* gs) {
    gs->dbp = (gs->bitbltbuf >> 32) & 0x3fff;
    gs->dbw = (gs->bitbltbuf >> 48) & 0x3f;
    gs->dpsm = (gs->bitbltbuf >> 56) & 0x3f;
    gs->sbp = (gs->bitbltbuf >> 0) & 0x3fff;
    gs->sbw = (gs->bitbltbuf >> 16) & 0x3f;
    gs->spsm = (gs->bitbltbuf >> 24) & 0x3f;
    gs->dsax = (gs->trxpos >> 32) & 0x7ff;
    gs->dsay = (gs->trxpos >> 48) & 0x7ff;
    gs->ssax = (gs->trxpos >> 0) & 0x7ff;
    gs->ssay = (gs->trxpos >> 16) & 0x7ff;
    gs->dir = (gs->trxpos >> 59) & 3;
    gs->rrw = (gs->trxreg >> 0) & 0xfff;
    gs->rrh = (gs->trxreg >> 32) & 0xfff;
    gs->xdir = gs->trxdir & 3;

    gs->dbp <<= 6;
    gs->dbw <<= 6;
    gs->sbp <<= 6;
    gs->sbw <<= 6;

    if (gs->xdir == 2)
        gs_vram_blit(gs);
}

void gs_transfer_write(struct ps2_gs* gs) {
    uint32_t addr = gs->dbp + gs->dsax + (gs->dsay * gs->dbw);

    addr += gs->dx + (gs->dy * gs->dbw);

    gs->vram[addr + 0] = gs->hwreg & 0xffffffff;
    gs->vram[addr + 1] = gs->hwreg >> 32;

    gs->dx += 2;

    if (gs->dx >= gs->rrw) {
        gs->dx = 0;
        ++gs->dy;
    }
}

void gs_draw_point(struct ps2_gs* gs) {

}

void gs_start_primitive(struct ps2_gs* gs) {
    gs->vqi = 0;
}

void gs_write_vertex(struct ps2_gs* gs, uint64_t data) {
    gs->vq[gs->vqi].xyz2 = data;
    gs->vq[gs->vqi++].rgbaq = gs->rgbaq;

    switch (gs->prim & 7) {
        case 0: if (gs->vqi == 1) { gs->backend.render_point(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 1: if (gs->vqi == 2) { gs->backend.render_line(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 2: if (gs->vqi == 2) { gs->backend.render_line_strip(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 3: if (gs->vqi == 3) { gs->backend.render_triangle(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 4: if (gs->vqi == 3) { gs->backend.render_triangle_strip(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 5: if (gs->vqi == 3) { gs->backend.render_triangle_fan(gs, gs->backend.udata); gs->vqi = 0; } break;
        case 6: if (gs->vqi == 2) { gs->backend.render_sprite(gs, gs->backend.udata); gs->vqi = 0; } break;
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
        case 0x04: gs->xyzf2 = data; gs_write_vertex(gs, gs->xyzf2); return;
        case 0x05: gs->xyz2 = data; gs_write_vertex(gs, gs->xyz2); return;
        case 0x06: gs->tex0_1 = data; return;
        case 0x07: gs->tex0_2 = data; return;
        case 0x08: gs->clamp_1 = data; return;
        case 0x09: gs->clamp_2 = data; return;
        case 0x0A: gs->fog = data; return;
        case 0x0C: gs->xyzf3 = data; return;
        case 0x0D: gs->xyz3 = data; gs_write_vertex(gs, gs->xyz3); return;
        case 0x14: gs->tex1_1 = data; return;
        case 0x15: gs->tex1_2 = data; return;
        case 0x16: gs->tex2_1 = data; return;
        case 0x17: gs->tex2_2 = data; return;
        case 0x18: gs->xyoffset_1 = data; return;
        case 0x19: gs->xyoffset_2 = data; return;
        case 0x1A: gs->prmodecont = data; return;
        case 0x1B: gs->prmode = data; return;
        case 0x1C: gs->texclut = data; return;
        case 0x22: gs->scanmsk = data; return;
        case 0x34: gs->miptbp1_1 = data; return;
        case 0x35: gs->miptbp1_2 = data; return;
        case 0x36: gs->miptbp2_1 = data; return;
        case 0x37: gs->miptbp2_2 = data; return;
        case 0x3B: gs->texa = data; return;
        case 0x3D: gs->fogcol = data; return;
        case 0x3F: gs->texflush = data; return;
        case 0x40: gs->scissor_1 = data; return;
        case 0x41: gs->scissor_2 = data; return;
        case 0x42: gs->alpha_1 = data; return;
        case 0x43: gs->alpha_2 = data; return;
        case 0x44: gs->dimx = data; return;
        case 0x45: gs->dthe = data; return;
        case 0x46: gs->colclamp = data; return;
        case 0x47: gs->test_1 = data; return;
        case 0x48: gs->test_2 = data; return;
        case 0x49: gs->pabe = data; return;
        case 0x4A: gs->fba_1 = data; return;
        case 0x4B: gs->fba_2 = data; return;
        case 0x4C: gs->frame_1 = data; return;
        case 0x4D: gs->frame_2 = data; return;
        case 0x4E: gs->zbuf_1 = data; return;
        case 0x4F: gs->zbuf_2 = data; return;
        case 0x50: gs->bitbltbuf = data; return;
        case 0x51: gs->trxpos = data; return;
        case 0x52: gs->trxreg = data; return;
        case 0x53: gs->trxdir = data; gs_start_transfer(gs); return;
        case 0x54: gs->hwreg = data; gs_transfer_write(gs); return;
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
        case 0x06: return gs->tex0_1;
        case 0x07: return gs->tex0_2;
        case 0x08: return gs->clamp_1;
        case 0x09: return gs->clamp_2;
        case 0x0A: return gs->fog;
        case 0x0C: return gs->xyzf3;
        case 0x0D: return gs->xyz3;
        case 0x14: return gs->tex1_1;
        case 0x15: return gs->tex1_2;
        case 0x16: return gs->tex2_1;
        case 0x17: return gs->tex2_2;
        case 0x18: return gs->xyoffset_1;
        case 0x19: return gs->xyoffset_2;
        case 0x1A: return gs->prmodecont;
        case 0x1B: return gs->prmode;
        case 0x1C: return gs->texclut;
        case 0x22: return gs->scanmsk;
        case 0x34: return gs->miptbp1_1;
        case 0x35: return gs->miptbp1_2;
        case 0x36: return gs->miptbp2_1;
        case 0x37: return gs->miptbp2_2;
        case 0x3B: return gs->texa;
        case 0x3D: return gs->fogcol;
        case 0x3F: return gs->texflush;
        case 0x40: return gs->scissor_1;
        case 0x41: return gs->scissor_2;
        case 0x42: return gs->alpha_1;
        case 0x43: return gs->alpha_2;
        case 0x44: return gs->dimx;
        case 0x45: return gs->dthe;
        case 0x46: return gs->colclamp;
        case 0x47: return gs->test_1;
        case 0x48: return gs->test_2;
        case 0x49: return gs->pabe;
        case 0x4A: return gs->fba_1;
        case 0x4B: return gs->fba_2;
        case 0x4C: return gs->frame_1;
        case 0x4D: return gs->frame_2;
        case 0x4E: return gs->zbuf_1;
        case 0x4F: return gs->zbuf_2;
        case 0x50: return gs->bitbltbuf;
        case 0x51: return gs->trxpos;
        case 0x52: return gs->trxreg;
        case 0x53: return gs->trxdir;
        case 0x54: return gs->hwreg;
        case 0x60: return gs->signal;
        case 0x61: return gs->finish;
        case 0x62: return gs->label;
    }

    return 0;
}

void ps2_gs_init_vblank_callback(struct ps2_gs* gs, void (*vblank_callback)(void*), void* udata) {
    gs->vblank_callback = vblank_callback;
    gs->vblank_udata = udata;
}