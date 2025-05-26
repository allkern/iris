#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gs.h"
#include "ee/intc.h"
#include "iop/intc.h"

struct ps2_gs* ps2_gs_create(void) {
    return malloc(sizeof(struct ps2_gs));
}

static inline void gs_invoke_event_handler(struct ps2_gs* gs, int event) {
    if (gs->events[event].func)
        gs->events[event].func(gs->events[event].udata);
}

void gs_handle_vblank_in(void* udata, int overshoot);
void gs_handle_hblank(void* udata, int overshoot);

void gs_handle_vblank_out(void* udata, int overshoot) {
    struct ps2_gs* gs = (struct ps2_gs*)udata;

    struct sched_event vblank_in_event;

    vblank_in_event.callback = gs_handle_vblank_in;
    vblank_in_event.cycles = GS_FRAME_NTSC;
    vblank_in_event.name = "Vblank in event";
    vblank_in_event.udata = gs;

    sched_schedule(gs->sched, vblank_in_event);

    // Send Vblank IRQ through INTC
    ps2_intc_irq(gs->ee_intc, EE_INTC_VBLANK_OUT);
    ps2_iop_intc_irq(gs->iop_intc, IOP_INTC_VBLANK_OUT);
}

void gs_flip_field(void* udata, int overshoot) {
    struct ps2_gs* gs = (struct ps2_gs*)udata;

    // Toggle field
    gs->csr ^= 1 << 13;
}

void gs_handle_vblank_in(void* udata, int overshoot) {
    struct ps2_gs* gs = (struct ps2_gs*)udata;

    struct sched_event vblank_out_event;

    vblank_out_event.callback = gs_handle_vblank_out;
    vblank_out_event.cycles = GS_VBLANK_NTSC; 
    vblank_out_event.name = "Vblank out event";
    vblank_out_event.udata = gs;

    struct sched_event field_flip_event;

    field_flip_event.callback = gs_flip_field;
    field_flip_event.cycles = 30000; 
    field_flip_event.name = "Field flip event";
    field_flip_event.udata = gs;

    // Set Vblank and Hblank flag
    gs->csr |= 0xf;

    // Tell backend to render scene
    gs_invoke_event_handler(gs, GS_EVENT_VBLANK);

    // Send Vblank IRQ through INTC
    ps2_intc_irq(gs->ee_intc, EE_INTC_VBLANK_IN);

    uint32_t mask = (gs->imr >> 8) & 0x1f;
    uint32_t stat = gs->csr & 0x1f;

    if (stat & (~mask)) {
        ps2_intc_irq(gs->ee_intc, EE_INTC_GS);
    }

    ps2_iop_intc_irq(gs->iop_intc, IOP_INTC_VBLANK_IN);

    sched_schedule(gs->sched, vblank_out_event);
    sched_schedule(gs->sched, field_flip_event);
}

void gs_handle_hblank(void* udata, int overshoot) {
    struct ps2_gs* gs = (struct ps2_gs*)udata;

    struct sched_event hblank_event;

    hblank_event.callback = gs_handle_hblank;
    hblank_event.cycles = GS_SCANLINE_NTSC;
    hblank_event.name = "Hblank event";
    hblank_event.udata = gs;

    sched_schedule(gs->sched, hblank_event);
}

void ps2_gs_init(struct ps2_gs* gs, struct ps2_intc* ee_intc, struct ps2_iop_intc* iop_intc, struct ps2_ee_timers* ee_timers, struct ps2_iop_timers* iop_timers, struct sched_state* sched) {
    memset(gs, 0, sizeof(struct ps2_gs));

    gs->sched = sched;
    gs->ee_intc = ee_intc;
    gs->iop_intc = iop_intc;
    gs->ee_timers = ee_timers;
    gs->iop_timers = iop_timers;
    gs->vram = malloc(0x400000); // 4 MB

    // Schedule Vblank event
    struct sched_event vblank_event;
    vblank_event.callback = gs_handle_vblank_in;
    vblank_event.cycles = GS_FRAME_NTSC;
    vblank_event.name = "Vblank in event";
    vblank_event.udata = gs;

    sched_schedule(gs->sched, vblank_event);

    // struct sched_event hblank_event;
    // hblank_event.callback = gs_handle_hblank;
    // hblank_event.cycles = GS_SCANLINE_NTSC;
    // hblank_event.name = "Hblank event";
    // hblank_event.udata = gs;

    // sched_schedule(gs->sched, hblank_event);

    gs->ctx = &gs->context[0];

    gs->csr |= 2;
}

void ps2_gs_reset(struct ps2_gs* gs) {
    gs->ctx = &gs->context[0];
    gs->csr |= 2;

    memset(gs->vram, 0, 0x400000);
}

void gs_switch_context(struct ps2_gs* gs, int c) {
    gs->ctx = &gs->context[c];
}

void ps2_gs_destroy(struct ps2_gs* gs) {
    free(gs->vram);
    free(gs);
}

void gs_start_primitive(struct ps2_gs* gs) {
    if (gs->prim & ~0x7ff) {
        // printf("gs: Invalid prim value %016x\n", gs->prim);

        // exit(1);
    }

    gs->vqi = 0;
}

static inline void gs_unpack_vertex(struct ps2_gs* gs, struct gs_vertex* v) {
    v->x = (v->xyz & 0xffff) >> 4;
    v->y = ((v->xyz >> 16) & 0xffff) >> 4;
    v->z = v->xyz >> 32;
    v->r = v->rgbaq & 0xff;
    v->g = (v->rgbaq >> 8) & 0xff;
    v->b = (v->rgbaq >> 16) & 0xff;
    v->a = (v->rgbaq >> 24) & 0xff;

    union {
        uint32_t u32;
        float f;
    } s, t, q;

    s.u32 = v->st & 0xffffffff;
    t.u32 = v->st >> 32;
    q.u32 = v->rgbaq >> 32;

    v->s = s.f;
    v->t = t.f;
    v->q = q.f;
    v->u = v->uv & 0x3fff;
    v->v = (v->uv >> 16) & 0x3fff;
}

void gs_write_vertex(struct ps2_gs* gs, uint64_t data, int discard) {
    gs->vq[gs->vqi].xyz = data;
    gs->vq[gs->vqi].st = gs->st;
    gs->vq[gs->vqi].uv = gs->uv;
    gs->vq[gs->vqi].rgbaq = gs->rgbaq;
    gs->attr = (gs->prmodecont & 1) ? gs->prim : gs->prmode;

    // Cache PRIM/PRMODE fields
    gs->iip = (gs->attr >> 3) & 1;
    gs->tme = (gs->attr >> 4) & 1;
    gs->fge = (gs->attr >> 5) & 1;
    gs->abe = (gs->attr >> 6) & 1;
    gs->aa1 = (gs->attr >> 7) & 1;
    gs->fst = (gs->attr >> 8) & 1;
    gs->ctxt = (gs->attr >> 9) & 1;
    gs->fix = (gs->attr >> 10) & 1;

    gs_unpack_vertex(gs, &gs->vq[gs->vqi]);

    gs->vqi++;

    // for (int c = 0; c < 2; c++) {
    //     uint32_t fbp = (gs->context[c].frame & 0x1ff) << 11;
    //     uint32_t fbw = ((gs->context[c].frame >> 16) & 0x3f) << 6;
    //     uint32_t xoff = (gs->context[c].xyoffset & 0xffff) >> 4;
    //     uint32_t yoff = ((gs->context[c].xyoffset >> 32) & 0xffff) >> 4;
    //     int scax0 = gs->context[c].scissor & 0x3ff;
    //     int scay0 = (gs->context[c].scissor >> 32) & 0x3ff;
    //     int scax1 = (gs->context[c].scissor >> 16) & 0x3ff;
    //     int scay1 = (gs->context[c].scissor >> 48) & 0x3ff;

    //     printf("context %d: fbp=%08x fbw=%d xyoffset=(%d,%d) scissor=(%d,%d-%d,%d) prmodecont=%016lx prmode=%016lx prim=%016lx\n",
    //         c,
    //         fbp, fbw,
    //         xoff, yoff,
    //         scax0, scay0,
    //         scax1, scay1,
    //         gs->prmodecont,
    //         gs->prmode,
    //         gs->prim
    //     );
    // }

    gs_switch_context(gs, (gs->attr & GS_CTXT) ? 1 : 0);

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
        case 7: if (gs->vqi == 2) { if (!discard) gs->backend.render_sprite(gs, gs->backend.udata); gs->vqi = 0; } break;
        default: {
            printf("gs: Reserved primitive %ld\n", gs->prim & 7);
        } break;
    }
}

void gs_write_vertex_fog(struct ps2_gs* gs, uint64_t data, int discard) {
    gs->vq[gs->vqi].fog = data >> 56;

    gs_write_vertex(gs, data & 0xffffffffffffffull, discard);
}

void gs_write_vertex_no_fog(struct ps2_gs* gs, uint64_t data, int discard) {
    gs->vq[gs->vqi].fog = gs->fog;

    gs_write_vertex(gs, data, discard);
}

uint64_t ps2_gs_read64(struct ps2_gs* gs, uint32_t addr) {
    switch (addr) {
        case 0x12000000: return gs->csr | 0x551b0000;
        case 0x12000010: return gs->csr | 0x551b0000;
        case 0x12000020: return gs->csr | 0x551b0000;
        case 0x12000030: return gs->csr | 0x551b0000;
        case 0x12000040: return gs->csr | 0x551b0000;
        case 0x12000050: return gs->csr | 0x551b0000;
        case 0x12000060: return gs->csr | 0x551b0000;
        case 0x12000070: return gs->csr | 0x551b0000;
        case 0x12000080: return gs->csr | 0x551b0000;
        case 0x12000090: return gs->csr | 0x551b0000;
        case 0x120000A0: return gs->csr | 0x551b0000;
        case 0x120000B0: return gs->csr | 0x551b0000;
        case 0x120000C0: return gs->csr | 0x551b0000;
        case 0x120000D0: return gs->csr | 0x551b0000;
        case 0x120000E0: return gs->csr | 0x551b0000;
        case 0x12001000: return gs->csr | 0x551b0000;
        case 0x12001010: return gs->csr | 0x551b0000;
        case 0x12001040: return gs->csr | 0x551b0000;
        case 0x12001080: return gs->siglblid;
    }

    return 0;
}

static inline void gs_unpack_dispfb1(struct ps2_gs* gs) {
    gs->dfbp1 = (gs->dispfb1 & 0x1ff) << 5;
    gs->dfbw1 = (gs->dispfb1 >> 9) & 0x3f;
    gs->dfbpsm1 = (gs->dispfb1 >> 15) & 0x1f;
}

static inline void gs_unpack_dispfb2(struct ps2_gs* gs) {
    gs->dfbp2 = (gs->dispfb2 & 0x1ff) << 5;
    gs->dfbw2 = (gs->dispfb2 >> 9) & 0x3f;
    gs->dfbpsm2 = (gs->dispfb2 >> 15) & 0x1f;
}

void ps2_gs_write64(struct ps2_gs* gs, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x12000000: gs->pmode = data; gs_invoke_event_handler(gs, GS_EVENT_SCISSOR); return;
        case 0x12000010: gs->smode1 = data; return;
        case 0x12000020: gs->smode2 = data; return;
        case 0x12000030: gs->srfsh = data; return;
        case 0x12000040: gs->synch1 = data; return;
        case 0x12000050: gs->synch2 = data; return;
        case 0x12000060: gs->syncv = data; return;
        case 0x12000070: gs->dispfb1 = data; gs_unpack_dispfb1(gs); gs_invoke_event_handler(gs, GS_EVENT_SCISSOR); return;
        case 0x12000080: gs->display1 = data; gs_invoke_event_handler(gs, GS_EVENT_SCISSOR); return;
        case 0x12000090: gs->dispfb2 = data; gs_unpack_dispfb2(gs); gs_invoke_event_handler(gs, GS_EVENT_SCISSOR); return;
        case 0x120000A0: gs->display2 = data; gs_invoke_event_handler(gs, GS_EVENT_SCISSOR); return;
        case 0x120000B0: gs->extbuf = data; return;
        case 0x120000C0: gs->extdata = data; return;
        case 0x120000D0: gs->extwrite = data; return;
        case 0x120000E0: gs->bgcolor = data; return;
        case 0x12001000: gs->csr = (gs->csr & 0xfffffe00) | (gs->csr & ~(data & 0xf)); return;
        case 0x12001010: gs->imr = data; return;
        case 0x12001040: gs->busdir = data; return;
        case 0x12001080: gs->siglblid = data; return;
    }
}

// static inline void gs_load_clut_cache(struct ps2_gs* gs, int i) {
//     // printf("tbpsm=%x cbpsm=%x csm=%d csa=%x (%d) cld=%d\n",
//     //     gs->context[i].tbpsm,
//     //     gs->context[i].cbpsm,
//     //     gs->context[i].csm,
//     //     gs->context[i].csa,
//     //     gs->context[i].csa,
//     //     gs->context[i].cld
//     // );

//     switch (gs->context[i].cld) {
//         case 1: break;
//         case 2: gs->cbp0 = gs->context[i].cbp; break;
//         case 3: gs->cbp1 = gs->context[i].cbp; break;
//         case 4: if (gs->context[i].cbp == gs->cbp0) return; break;
//         case 5: if (gs->context[i].cbp == gs->cbp1) return; break;
//         default: return;
//     }

//     switch (gs->context[i].tbpsm) {
//         case GS_PSMT8H:
//         case GS_PSMT8: {
//             switch (gs->context[i].cbpsm) {
//                 case GS_PSMCT32: {
//                     for (int y = 0; y < 16; y++) {
//                         for (int x = 0; x < 16; x++) {
//                             uint32_t cache_addr = (gs->context[i].csa * 16) + (x + (y * 16));
//                             uint32_t vram_addr = gs->context[i].cbp + (x + (y * 64));

//                             gs->clut_cache[cache_addr] = gs->vram[vram_addr];
//                         }
//                     }
//                 } break;

//                 case GS_PSMCT16:
//                 case GS_PSMCT16S: {
//                     printf("16bpp 8-bit CLUT\n");

//                     exit(1);
//                 } break;
//             }
//         } break;

//         case GS_PSMT4HH:
//         case GS_PSMT4HL:
//         case GS_PSMT4: {
//             switch (gs->context[i].cbpsm) {
//                 case GS_PSMCT32: {
//                     for (int y = 0; y < 2; y++) {
//                         for (int x = 0; x < 8; x++) {
//                             uint32_t cache_addr = (gs->context[i].csa * 16) + (x + (y * 8));
//                             uint32_t vram_addr = gs->context[i].cbp + (x + (y * 64));

//                             gs->clut_cache[cache_addr] = gs->vram[vram_addr];
//                         }
//                     }
//                 } break;

//                 case GS_PSMCT16:
//                 case GS_PSMCT16S: {
//                     printf("16bpp 4-bit CLUT\n");

//                     exit(1);
//                 } break;
//             }
//         } break;
//     }
// }

static inline void gs_unpack_tex0(struct ps2_gs* gs, int i) {
    gs->context[i].tbp0 = gs->context[i].tex0 & 0x3fff;
    gs->context[i].tbw = (gs->context[i].tex0 >> 14) & 0x3f;
    gs->context[i].tbpsm = (gs->context[i].tex0 >> 20) & 0x3f;
    gs->context[i].usize = 1 << ((gs->context[i].tex0 >> 26) & 0xf); // tw
    gs->context[i].vsize = 1 << ((gs->context[i].tex0 >> 30) & 0xf); // th
    gs->context[i].tcc = (gs->context[i].tex0 >> 34) & 1;
    gs->context[i].tfx = (gs->context[i].tex0 >> 35) & 3;
    gs->context[i].cbp = (gs->context[i].tex0 >> 37) & 0x3fff;
    gs->context[i].cbpsm = (gs->context[i].tex0 >> 51) & 0xf;
    gs->context[i].csm = (gs->context[i].tex0 >> 55) & 1;
    gs->context[i].csa = (gs->context[i].tex0 >> 56) & 0x1f;
    gs->context[i].cld = (gs->context[i].tex0 >> 61) & 7;

    gs->context[i].usize = (gs->context[i].usize > 1024) ? 1024 : gs->context[i].usize;
    gs->context[i].vsize = (gs->context[i].vsize > 1024) ? 1024 : gs->context[i].vsize;

    if (gs->context[i].cld) {
        // printf("gs: CLUT cache load (mode %d, dbp=%08x)\n", gs->context[i].cld, gs->context[i].cbp);
        // gs_load_clut_cache(gs, i);
    }
}

static inline void gs_unpack_clamp(struct ps2_gs* gs, int i) {
    gs->context[i].wms = gs->context[i].clamp & 3;
    gs->context[i].wmt = (gs->context[i].clamp >> 2) & 3;
    gs->context[i].minu = (gs->context[i].clamp >> 4) & 0x3ff;
    gs->context[i].maxu = (gs->context[i].clamp >> 14) & 0x3ff;
    gs->context[i].minv = (gs->context[i].clamp >> 24) & 0x3ff;
    gs->context[i].maxv = (gs->context[i].clamp >> 34) & 0x3ff;
}

static inline void gs_unpack_tex1(struct ps2_gs* gs, int i) {
    gs->context[i].lcm = gs->context[i].tex1 & 1;
    gs->context[i].mxl = (gs->context[i].tex1 >> 2) & 7;
    gs->context[i].mmag = (gs->context[i].tex1 >> 5) & 1;
    gs->context[i].mmin = (gs->context[i].tex1 >> 6) & 7;
    gs->context[i].mtba = (gs->context[i].tex1 >> 9) & 1;
    gs->context[i].l = (gs->context[i].tex1 >> 19) & 3;
    gs->context[i].k = gs->context[i].tex1 >> 32;
}

static inline void gs_unpack_tex2(struct ps2_gs* gs, int i) {
    gs->context[i].tbpsm = (gs->context[i].tex2 >> 20) & 0x3f;
    gs->context[i].cbp = (gs->context[i].tex2 >> 37) & 0x3fff;
    gs->context[i].cbpsm = (gs->context[i].tex2 >> 51) & 0xf;
    gs->context[i].csm = (gs->context[i].tex2 >> 55) & 1;
    gs->context[i].csa = (gs->context[i].tex2 >> 56) & 0x1f;
    gs->context[i].cld = (gs->context[i].tex2 >> 61) & 7;

    if (gs->context[i].cld) {
        // printf("gs: CLUT cache load (mode %d, dbp=%08x)\n", gs->context[i].cld, gs->context[i].cbp);
        // gs_load_clut_cache(gs, i);
    }

    // if (gs->context[i].cld) {
    //     gs_load_clut_cache(gs, i);
    // }
}

static inline void gs_unpack_xyoffset(struct ps2_gs* gs, int i) {
    gs->context[i].ofx = (gs->context[i].xyoffset & 0xffff) >> 4;
    gs->context[i].ofy = ((gs->context[i].xyoffset >> 32) & 0xffff) >> 4;
}

static inline void gs_unpack_miptbp1(struct ps2_gs* gs, int i) {
    gs->context[i].mmtbp[0] = gs->context[i].miptbp1 & 0x3fff;
    gs->context[i].mmtbw[0] = (gs->context[i].miptbp1 >> 14) & 0x3f;
    gs->context[i].mmtbp[1] = (gs->context[i].miptbp1 >> 20) & 0x3fff;
    gs->context[i].mmtbw[1] = (gs->context[i].miptbp1 >> 34) & 0x3f;
    gs->context[i].mmtbp[2] = (gs->context[i].miptbp1 >> 40) & 0x3fff;
    gs->context[i].mmtbw[2] = (gs->context[i].miptbp1 >> 54) & 0x3f;
}

static inline void gs_unpack_miptbp2(struct ps2_gs* gs, int i) {
    gs->context[i].mmtbp[3] = gs->context[i].miptbp2 & 0x3fff;
    gs->context[i].mmtbw[3] = (gs->context[i].miptbp2 >> 14) & 0x3f;
    gs->context[i].mmtbp[4] = (gs->context[i].miptbp2 >> 20) & 0x3fff;
    gs->context[i].mmtbw[4] = (gs->context[i].miptbp2 >> 34) & 0x3f;
    gs->context[i].mmtbp[5] = (gs->context[i].miptbp2 >> 40) & 0x3fff;
    gs->context[i].mmtbw[5] = (gs->context[i].miptbp2 >> 54) & 0x3f;
}

static inline void gs_unpack_scissor(struct ps2_gs* gs, int i) {
    gs->context[i].scax0 = gs->context[i].scissor & 0x3ff;
    gs->context[i].scay0 = (gs->context[i].scissor >> 32) & 0x3ff;
    gs->context[i].scax1 = (gs->context[i].scissor >> 16) & 0x3ff;
    gs->context[i].scay1 = (gs->context[i].scissor >> 48) & 0x3ff;
}

static inline void gs_unpack_alpha(struct ps2_gs* gs, int i) {
    gs->context[i].a = gs->context[i].alpha & 3;
    gs->context[i].b = (gs->context[i].alpha >> 2) & 3;
    gs->context[i].c = (gs->context[i].alpha >> 4) & 3;
    gs->context[i].d = (gs->context[i].alpha >> 6) & 3;
    gs->context[i].fix = (gs->context[i].alpha >> 32) & 0xff;
}

static inline void gs_unpack_test(struct ps2_gs* gs, int i) {
    gs->context[i].ate = gs->context[i].test & 1;
    gs->context[i].atst = (gs->context[i].test >> 1) & 7;
    gs->context[i].aref = (gs->context[i].test >> 4) & 0xff;
    gs->context[i].afail = (gs->context[i].test >> 12) & 3;
    gs->context[i].date = (gs->context[i].test >> 14) & 1;
    gs->context[i].datm = (gs->context[i].test >> 15) & 1;
    gs->context[i].zte = (gs->context[i].test >> 16) & 1;
    gs->context[i].ztst = (gs->context[i].test >> 17) & 3;
}

static inline void gs_unpack_frame(struct ps2_gs* gs, int i) {
    gs->context[i].fbp = (gs->context[i].frame & 0x1ff) << 11;
    gs->context[i].fbw = ((gs->context[i].frame >> 16) & 0x3f) << 6;
    gs->context[i].fbpsm = (gs->context[i].frame >> 24) & 0x3f;
    gs->context[i].fbmsk = gs->context[i].frame >> 32;
}

static inline void gs_unpack_zbuf(struct ps2_gs* gs, int i) {
    gs->context[i].zbp = (gs->context[i].zbuf & 0x1ff) << 11;
    gs->context[i].zbpsm = (gs->context[i].zbuf >> 24) & 0xf;
    gs->context[i].zbmsk = (gs->context[i].zbuf >> 32) & 1;
}

static inline void gs_unpack_texclut(struct ps2_gs* gs) {
    gs->cbw = gs->texclut & 0x3f;
    gs->cou = ((gs->texclut >> 6) & 0x3f) << 4;
    gs->cov = (gs->texclut >> 12) & 0x3ff;
}

static inline void gs_unpack_texa(struct ps2_gs* gs) {
    gs->ta0 = gs->texa & 0xff;
    gs->aem = (gs->texa >> 15) & 1;
    gs->ta1 = (gs->texa >> 32) & 0xff;
}

void ps2_gs_write_internal(struct ps2_gs* gs, int reg, uint64_t data) {
    switch (reg) {
        case 0x00: /* printf("gs: PRIM <- %016lx\n", data); */ gs->prim = data; gs_start_primitive(gs); return;
        case 0x01: /* printf("gs: RGBAQ <- %016lx\n", data); */ gs->rgbaq = data; return;
        case 0x02: /* printf("gs: ST <- %016lx\n", data); */ gs->st = data; return;
        case 0x03: /* printf("gs: UV <- %016lx\n", data); */ gs->uv = data; return;
        case 0x04: /* printf("gs: XYZF2 <- %016lx\n", data); */ gs->xyzf2 = data; gs_write_vertex_fog(gs, gs->xyzf2, 0); return;
        case 0x05: /* printf("gs: XYZ2 <- %016lx\n", data); */ gs->xyz2 = data; gs_write_vertex_no_fog(gs, gs->xyz2, 0); return;
        case 0x06: /* printf("gs: TEX0_1 <- %016lx\n", data); */ gs->context[0].tex0 = data; gs_unpack_tex0(gs, 0); return;
        case 0x07: /* printf("gs: TEX0_2 <- %016lx\n", data); */ gs->context[1].tex0 = data; gs_unpack_tex0(gs, 1); return;
        case 0x08: /* printf("gs: CLAMP_1 <- %016lx\n", data); */ gs->context[0].clamp = data; gs_unpack_clamp(gs, 0); return;
        case 0x09: /* printf("gs: CLAMP_2 <- %016lx\n", data); */ gs->context[1].clamp = data; gs_unpack_clamp(gs, 1); return;
        case 0x0A: /* printf("gs: FOG <- %016lx\n", data); */ gs->fog = data; return;
        case 0x0C: /* printf("gs: XYZF3 <- %016lx\n", data); */ gs->xyzf3 = data; gs_write_vertex_fog(gs, gs->xyzf3, 1); return;
        case 0x0D: /* printf("gs: XYZ3 <- %016lx\n", data); */ gs->xyz3 = data; gs_write_vertex_no_fog(gs, gs->xyz3, 1); return;
        case 0x14: /* printf("gs: TEX1_1 <- %016lx\n", data); */ gs->context[0].tex1 = data; gs_unpack_tex1(gs, 0); return;
        case 0x15: /* printf("gs: TEX1_2 <- %016lx\n", data); */ gs->context[1].tex1 = data; gs_unpack_tex1(gs, 1); return;
        case 0x16: /* printf("gs: TEX2_1 <- %016lx\n", data); */ gs->context[0].tex2 = data; gs_unpack_tex2(gs, 0); return;
        case 0x17: /* printf("gs: TEX2_2 <- %016lx\n", data); */ gs->context[1].tex2 = data; gs_unpack_tex2(gs, 1); return;
        case 0x18: /* printf("gs: XYOFFSET_1 <- %016lx\n", data); */ gs->context[0].xyoffset = data; gs_unpack_xyoffset(gs, 0); return;
        case 0x19: /* printf("gs: XYOFFSET_2 <- %016lx\n", data); */ gs->context[1].xyoffset = data; gs_unpack_xyoffset(gs, 1); return;
        case 0x1A: /* printf("gs: PRMODECONT <- %016lx\n", data); */ gs->prmodecont = data; return;
        case 0x1B: /* printf("gs: PRMODE <- %016lx\n", data); */ gs->prmode = data; return;
        case 0x1C: /* printf("gs: TEXCLUT <- %016lx\n", data); */ gs->texclut = data; gs_unpack_texclut(gs); return;
        case 0x22: /* printf("gs: SCANMSK <- %016lx\n", data); */ gs->scanmsk = data; return;
        case 0x34: /* printf("gs: MIPTBP1_1 <- %016lx\n", data); */ gs->context[0].miptbp1 = data; gs_unpack_miptbp1(gs, 0); return;
        case 0x35: /* printf("gs: MIPTBP1_2 <- %016lx\n", data); */ gs->context[1].miptbp1 = data; gs_unpack_miptbp1(gs, 1); return;
        case 0x36: /* printf("gs: MIPTBP2_1 <- %016lx\n", data); */ gs->context[0].miptbp2 = data; gs_unpack_miptbp2(gs, 0); return;
        case 0x37: /* printf("gs: MIPTBP2_2 <- %016lx\n", data); */ gs->context[1].miptbp2 = data; gs_unpack_miptbp2(gs, 1); return;
        case 0x3B: /* printf("gs: TEXA <- %016lx\n", data); */ gs->texa = data; gs_unpack_texa(gs); return;
        case 0x3D: /* printf("gs: FOGCOL <- %016lx\n", data); */ gs->fogcol = data; return;
        case 0x3F: /* printf("gs: TEXFLUSH <- %016lx\n", data); */ gs->texflush = data; return;
        case 0x40: /* printf("gs: SCISSOR_1 <- %016lx\n", data); */ gs->context[0].scissor = data; gs_unpack_scissor(gs, 0); gs_invoke_event_handler(gs, GS_EVENT_SCISSOR); return;
        case 0x41: /* printf("gs: SCISSOR_2 <- %016lx\n", data); */ gs->context[1].scissor = data; gs_unpack_scissor(gs, 1); gs_invoke_event_handler(gs, GS_EVENT_SCISSOR); return;
        case 0x42: /* printf("gs: ALPHA_1 <- %016lx\n", data); */ gs->context[0].alpha = data; gs_unpack_alpha(gs, 0); return;
        case 0x43: /* printf("gs: ALPHA_2 <- %016lx\n", data); */ gs->context[1].alpha = data; gs_unpack_alpha(gs, 1); return;
        case 0x44: /* printf("gs: DIMX <- %016lx\n", data); */ gs->dimx = data; return;
        case 0x45: /* printf("gs: DTHE <- %016lx\n", data); */ gs->dthe = data; return;
        case 0x46: /* printf("gs: COLCLAMP <- %016lx\n", data); */ gs->colclamp = data; return;
        case 0x47: /* printf("gs: TEST_1 <- %016lx\n", data); */ gs->context[0].test = data; gs_unpack_test(gs, 0); return;
        case 0x48: /* printf("gs: TEST_2 <- %016lx\n", data); */ gs->context[1].test = data; gs_unpack_test(gs, 1); return;
        case 0x49: /* printf("gs: PABE <- %016lx\n", data); */ gs->pabe = data; return;
        case 0x4A: /* printf("gs: FBA_1 <- %016lx\n", data); */ gs->context[0].fba = data; return;
        case 0x4B: /* printf("gs: FBA_2 <- %016lx\n", data); */ gs->context[1].fba = data; return;
        case 0x4C: /* printf("gs: FRAME_1 <- %016lx\n", data); */ gs->context[0].frame = data; gs_unpack_frame(gs, 0); return;
        case 0x4D: /* printf("gs: FRAME_2 <- %016lx\n", data); */ gs->context[1].frame = data; gs_unpack_frame(gs, 1); return;
        case 0x4E: /* printf("gs: ZBUF_1 <- %016lx\n", data); */ gs->context[0].zbuf = data; gs_unpack_zbuf(gs, 0); return;
        case 0x4F: /* printf("gs: ZBUF_2 <- %016lx\n", data); */ gs->context[1].zbuf = data; gs_unpack_zbuf(gs, 1); return;
        case 0x50: /* printf("gs: BITBLTBUF <- %016lx\n", data); */ gs->bitbltbuf = data; return;
        case 0x51: /* printf("gs: TRXPOS <- %016lx\n", data); */ gs->trxpos = data; return;
        case 0x52: /* printf("gs: TRXREG <- %016lx\n", data); */ gs->trxreg = data; return;
        case 0x53: /* printf("gs: TRXDIR <- %016lx\n", data); */ gs->trxdir = data; gs->backend.transfer_start(gs, gs->backend.udata); return; // gs_start_transfer(gs); return;
        case 0x54: gs->hwreg = data; gs->backend.transfer_write(gs, gs->backend.udata); return; // gs_transfer_write(gs); return;
        case 0x60: /* printf("gs: SIGNAL <- %016lx\n", data); */ gs->signal = data; return;
        case 0x61: {
            // Trigger FINISH event
            gs->csr |= 2;

            uint32_t mask = (gs->imr >> 8) & 0x1f;
            uint32_t stat = gs->csr & 0x1f;

            if (stat & (~mask)) {
                ps2_intc_irq(gs->ee_intc, EE_INTC_GS);
            }
        } return;
        case 0x62: /* printf("gs: LABEL <- %016lx\n", data); */ gs->label = data; return;
        default: {
            // printf("gs: Invalid privileged register %02x write %016lx\n", reg, data);

            return;

            exit(1);
        }
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
        default: {
            printf("gs: Invalid privileged register %02x read\n", reg);

            exit(1);
        }
    }

    return 0;
}

void ps2_gs_init_callback(struct ps2_gs* gs, int event, void (*func)(void*), void* udata) {
    gs->events[event].func = func;
    gs->events[event].udata = udata;
}

struct gs_callback* ps2_gs_get_callback(struct ps2_gs* gs, int event) {
    return &gs->events[event];
}

void ps2_gs_remove_callback(struct ps2_gs* gs, int event) {
    gs->events[event].func = NULL;
}