#include <new>

#include "gs.hpp"
#include "ee/intc.hpp"
#include "iop/intc.hpp"

namespace iris::gs {

Gs* create(logger::Logger* logger, iop::intc::Intc* iop_intc, iop::timers::Timers* iop_timers, scheduler::Scheduler* sched) {
    Gs* gs = new Gs();

    gs->logger = logger;
    gs->logger_id = logger::register_source(logger, "gs");

    gs->hw.iop_intc = iop_intc;
    gs->hw.iop_timers = iop_timers;
    gs->hw.sched = sched;

    gs->vram = (uint32_t*)malloc(0x400000); // 4 MB

    reset(gs);

    return gs;
}

static inline void test_gs_irq(Gs* gs) {
    uint32_t mask = (gs->imr >> 8) & 0x1f;
    uint32_t stat = gs->csr & 0x1f;

    if (stat & (~mask)) {
        ee::intc::irq(gs->hw.ee_intc, ee::intc::GS);
    }
}

static inline int assert_vblank(Gs* gs) {
    if ((gs->csr & 8) == 0) {
        gs->csr |= 8;

        return ((gs->imr >> 8) & 8) == 0;
    }

    return 0;
}

static inline int assert_hblank(Gs* gs) {
    if ((gs->csr & 4) == 0) {
        if (gs->csr_enable & 4)
            gs->csr |= 4;

        // iris_debug(gs, "Asserting Hblank imr.hsync={}", (gs->imr >> 8) & 4);

        return ((gs->imr >> 8) & 4) == 0;
    }

    return 0;
}

void handle_vblank_in(void* udata, int overshoot);
void handle_hblank(void* udata, int overshoot);

void handle_vblank_out(void* udata, int overshoot) {
    Gs* gs = (Gs*)udata;

    scheduler::Event vblank_in_event;

    vblank_in_event.callback = handle_vblank_in;
    vblank_in_event.cycles = gs->frame_cycles;
    vblank_in_event.name = "Vblank in event";
    vblank_in_event.udata = gs;

    scheduler::schedule(gs->hw.sched, vblank_in_event);

    // Send Vblank IRQs through INTC
    ee::intc::irq(gs->hw.ee_intc, ee::intc::VBLANK_OUT);
    iop::intc::irq(gs->hw.iop_intc, iop::intc::VBLANK_OUT);

    gs->vblank = 0;

    ee::timers::handle_vblank_out(gs->hw.ee_timers);
}

void flip_field(void* udata, int overshoot) {
    Gs* gs = (Gs*)udata;

    // Toggle field
    gs->csr ^= (1 << 13) | (1 << 14);
}

void handle_vblank_in(void* udata, int overshoot) {
    Gs* gs = (Gs*)udata;

    scheduler::Event vblank_out_event;

    vblank_out_event.callback = handle_vblank_out;
    vblank_out_event.cycles = gs->vblank_cycles;
    vblank_out_event.name = "Vblank out event";
    vblank_out_event.udata = gs;

    scheduler::Event field_flip_event;

    field_flip_event.callback = flip_field;
    field_flip_event.cycles = 65622;
    field_flip_event.name = "Field flip event";
    field_flip_event.udata = gs;

    // Set Vblank and Hblank flag
    if (assert_vblank(gs)) {
        ee::intc::irq(gs->hw.ee_intc, ee::intc::GS);
    }

    // Send Vblank IRQ through INTC
    ee::intc::irq(gs->hw.ee_intc, ee::intc::VBLANK_IN);
    iop::intc::irq(gs->hw.iop_intc, iop::intc::VBLANK_IN);

    // uint32_t mask = (gs->imr >> 8) & 0x1f;
    // uint32_t stat = gs->csr & 0x1f;

    // if (stat & (~mask)) {
    //     ee::intc::irq(gs->hw.ee_intc, ee::intc::GS);
    // }

    scheduler::schedule(gs->hw.sched, vblank_out_event);
    scheduler::schedule(gs->hw.sched, field_flip_event);

    gs->vblank = 1;

    ee::timers::handle_vblank_in(gs->hw.ee_timers);
}

void handle_hblank(void* udata, int overshoot) {
    Gs* gs = (Gs*)udata;

    scheduler::Event hblank_event;

    hblank_event.callback = handle_hblank;
    hblank_event.cycles = gs->scanline_cycles * 2;
    hblank_event.name = "Hblank event";
    hblank_event.udata = gs;

    if (assert_hblank(gs)) {
        ee::intc::irq(gs->hw.ee_intc, ee::intc::GS);
    }

    ee::timers::handle_hblank(gs->hw.ee_timers);

    // gs->csr ^= 1 << 13 | 1 << 14;

    scheduler::schedule(gs->hw.sched, hblank_event);
}

void connect(Gs* gs, ee::intc::Intc* ee_intc, ee::timers::Timers* ee_timers) {
    gs->hw.ee_intc = ee_intc;
    gs->hw.ee_timers = ee_timers;
}

// Note: System 256/Super 256 games detect the jumper setting/system
//       by detecting EE frequency. These systems have higher clocks.
void set_ee_clock(Gs* gs, int hz) {
    gs->frame_cycles = (int)(((int64_t)FRAME_NTSC * hz) / EE_CLOCK);
    gs->vblank_cycles = (int)(((int64_t)VBLANK_NTSC * hz) / EE_CLOCK);
    gs->scanline_cycles = (int)(((int64_t)SCANLINE_NTSC * hz) / EE_CLOCK);
}

void reset(Gs* gs) {
    gs->ctx = &gs->context[0];
    gs->csr |= 2;
    gs->imr = 0x00007f00;

    // Note: Dokapon Kingdom relies on this, don't ask me why
    gs->stall_sigid = 0xffffffff;

    // Schedule Vblank event
    scheduler::Event vblank_event;
    vblank_event.callback = handle_vblank_in;
    vblank_event.cycles = gs->frame_cycles;
    vblank_event.name = "Vblank in event";
    vblank_event.udata = gs;

    scheduler::schedule(gs->hw.sched, vblank_event);

    scheduler::Event hblank_event;
    hblank_event.callback = handle_hblank;
    hblank_event.cycles = gs->scanline_cycles * 2;
    hblank_event.name = "Hblank event";
    hblank_event.udata = gs;

    scheduler::schedule(gs->hw.sched, hblank_event);

    memset(gs->vram, 0, 0x400000);
}

// void switch_context(Gs* gs, int c) {
//     gs->ctx = &gs->context[c];
// }

void destroy(Gs* gs) {
    free(gs->vram);
    delete gs;
}

// void start_primitive(Gs* gs) {
//     if (gs->prim & ~0x7ff) {
//         // iris_debug(gs, "Invalid prim value {:016x}", gs->prim);

//         // exit(1);
//     }

//     gs->vqi = 0;
// }

// static inline void unpack_vertex(Gs* gs, Vertex* v) {
//     v->x = v->xyz & 0xffff;
//     v->y = (v->xyz >> 16) & 0xffff;
//     v->z = v->xyz >> 32;
//     v->r = v->rgbaq & 0xff;
//     v->g = (v->rgbaq >> 8) & 0xff;
//     v->b = (v->rgbaq >> 16) & 0xff;
//     v->a = (v->rgbaq >> 24) & 0xff;

//     union {
//         uint32_t u32;
//         float f;
//     } s, t, q;

//     s.u32 = v->st & 0xffffffff;
//     t.u32 = v->st >> 32;
//     q.u32 = v->rgbaq >> 32;

//     v->s = s.f;
//     v->t = t.f;
//     v->q = q.f;
//     v->u = v->uv & 0x3fff;
//     v->v = (v->uv >> 16) & 0x3fff;
// }

// void write_vertex(Gs* gs, uint64_t data, int discard) {
//     gs->vq[gs->vqi].xyz = data;
//     gs->vq[gs->vqi].st = gs->st;
//     gs->vq[gs->vqi].uv = gs->uv;
//     gs->vq[gs->vqi].rgbaq = gs->rgbaq;

//     gs->attr = (gs->prmodecont & 1) ? gs->prim : gs->prmode;

//     // Cache PRIM/PRMODE fields
//     gs->iip = (gs->attr >> 3) & 1;
//     gs->tme = (gs->attr >> 4) & 1;
//     gs->fge = (gs->attr >> 5) & 1;
//     gs->abe = (gs->attr >> 6) & 1;
//     gs->aa1 = (gs->attr >> 7) & 1;
//     gs->fst = (gs->attr >> 8) & 1;
//     gs->ctxt = (gs->attr >> 9) & 1;
//     gs->fix = (gs->attr >> 10) & 1;

//     unpack_vertex(gs, &gs->vq[gs->vqi]);

//     // iris_debug(gs, "Pushing vertex ({:04x},{:04x}) to VQ[{}] discard={}", //     //     gs->vq[gs->vqi].x, gs->vq[gs->vqi].y, gs->vqi, discard
//     //);

//     gs->vqi++;

//     // for (int c = 0; c < 2; c++) {
//     //     uint32_t fbp = (gs->context[c].frame & 0x1ff) << 11;
//     //     uint32_t fbw = ((gs->context[c].frame >> 16) & 0x3f) << 6;
//     //     uint32_t xoff = (gs->context[c].xyoffset & 0xffff);
//     //     uint32_t yoff = ((gs->context[c].xyoffset >> 32) & 0xffff);
//     //     int scax0 = gs->context[c].scissor & 0x3ff;
//     //     int scay0 = (gs->context[c].scissor >> 32) & 0x3ff;
//     //     int scax1 = (gs->context[c].scissor >> 16) & 0x3ff;
//     //     int scay1 = (gs->context[c].scissor >> 48) & 0x3ff;

//     //     iris_debug(gs, "context {}: fbp={:08x} fbw={} xyoffset=({},{}) scissor=({},{}-{},{}) prmodecont={:016x} prmode={:016x} prim={:016x}", //     //         c,
//     //         fbp, fbw,
//     //         xoff, yoff,
//     //         scax0, scay0,
//     //         scax1, scay1,
//     //         gs->prmodecont,
//     //         gs->prmode,
//     //         gs->prim
//     //);
//     // }

//     switch_context(gs, (gs->attr & CTXT) ? 1 : 0);

//     switch (gs->prim & 7) {
//         case 0: if (gs->vqi == 1) { gs->backend.render_point(gs, gs->backend.udata); gs->vqi = 0; } break;
//         case 1: if (gs->vqi == 2) { gs->backend.render_line(gs, gs->backend.udata); gs->vqi = 0; } break;
//         case 2: {
//             if (gs->vqi == 2) {
//                 if (!discard)
//                     gs->backend.render_line(gs, gs->backend.udata);
//             } else if (gs->vqi == 3) {
//                 gs->vq[0] = gs->vq[1];
//                 gs->vq[1] = gs->vq[2];

//                 if (!discard)
//                     gs->backend.render_line(gs, gs->backend.udata);

//                 gs->vqi = 2;
//             }
//         } break;
//         case 3: if (gs->vqi == 3) { if (!discard) gs->backend.render_triangle(gs, gs->backend.udata); gs->vqi = 0; } break;
//         case 4: {
//             if (gs->vqi == 3) {
//                 if (!discard) gs->backend.render_triangle(gs, gs->backend.udata);
//             } else if (gs->vqi == 4) {
//                 gs->vq[0] = gs->vq[1];
//                 gs->vq[1] = gs->vq[2];
//                 gs->vq[2] = gs->vq[3];

//                 if (!discard) gs->backend.render_triangle(gs, gs->backend.udata);

//                 gs->vqi = 3;
//             }
//         } break;
//         case 5: {
//             if (gs->vqi == 3) {
//                 if (!discard) gs->backend.render_triangle(gs, gs->backend.udata);
//             } else if (gs->vqi == 4) {
//                 gs->vq[1] = gs->vq[2];
//                 gs->vq[2] = gs->vq[3];

//                 if (!discard) gs->backend.render_triangle(gs, gs->backend.udata);

//                 gs->vqi = 3;
//             }
//         } break;
//         case 6: if (gs->vqi == 2) { if (!discard) gs->backend.render_sprite(gs, gs->backend.udata); gs->vqi = 0; } break;
//         case 7: if (gs->vqi == 2) { if (!discard) gs->backend.render_sprite(gs, gs->backend.udata); gs->vqi = 0; } break;
//         default: {
//             iris_debug(gs, "Reserved primitive {}", gs->prim & 7);
//         } break;
//     }
// }

// void write_vertex_fog(Gs* gs, uint64_t data, int discard) {
//     gs->vq[gs->vqi].fog = data >> 56;

//     write_vertex(gs, data & 0xffffffffffffffull, discard);
// }

// void write_vertex_no_fog(Gs* gs, uint64_t data, int discard) {
//     gs->vq[gs->vqi].fog = gs->fog;

//     write_vertex(gs, data, discard);
// }

uint64_t read64(Gs* gs, uint32_t addr) {
    // Hack toggle between FIFO empty and FIFO "Neither Empty nor Almost Full"
    gs->csr ^= 0x4000;

    addr = (addr & 0xfffff000) | (addr & 0x3ff);

    switch (addr) {
        case 0x12000000:
        case 0x12000010:
        case 0x12000020:
        case 0x12000030:
        case 0x12000040:
        case 0x12000050:
        case 0x12000060:
        case 0x12000070:
        case 0x12000080:
        case 0x12000090:
        case 0x120000A0:
        case 0x120000B0:
        case 0x120000C0:
        case 0x120000D0:
        case 0x120000E0:
        case 0x12001000:
        case 0x12001010:
        case 0x12001040: {
            return gs->csr | 0x551b0000;
        }

        case 0x12001080: return gs->siglblid;

        case 0x12001001: return (gs->csr >> 8) & 0xff;
    }

    iris_debug(gs, "Unhandled read from {:08x}", addr);

    return 0;
}

static inline void unpack_dispfb1(Gs* gs) {
    gs->dfbp1 = (gs->dispfb1 & 0x1ff) << 5;
    gs->dfbw1 = (gs->dispfb1 >> 9) & 0x3f;
    gs->dfbpsm1 = (gs->dispfb1 >> 15) & 0x1f;
}

static inline void unpack_dispfb2(Gs* gs) {
    gs->dfbp2 = (gs->dispfb2 & 0x1ff) << 5;
    gs->dfbw2 = (gs->dispfb2 >> 9) & 0x3f;
    gs->dfbpsm2 = (gs->dispfb2 >> 15) & 0x1f;
}

void write64(Gs* gs, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x12000000: gs->pmode = data; return;
        case 0x12000010: gs->smode1 = data; return;
        case 0x12000020: gs->smode2 = data; return;
        case 0x12000030: gs->srfsh = data; return;
        case 0x12000040: gs->synch1 = data; return;
        case 0x12000050: gs->synch2 = data; return;
        case 0x12000060: gs->syncv = data; return;
        case 0x12000070: gs->dispfb1 = data; unpack_dispfb1(gs); return;
        case 0x12000080: gs->display1 = data; return;
        case 0x12000090: gs->dispfb2 = data; unpack_dispfb2(gs); return;
        case 0x120000A0: gs->display2 = data; return;
        case 0x120000B0: gs->extbuf = data; return;
        case 0x120000C0: gs->extdata = data; return;
        case 0x120000D0: gs->extwrite = data; return;
        case 0x120000E0: gs->bgcolor = data; return;
        case 0x12001000: {
            if (data & 8) {
                // Game is requesting vsync
                // gs->vblank |= 1;
            }

            gs->csr = (gs->csr & 0xfffffe00) | (gs->csr & ~(data & 0xf));
            gs->csr_enable = data;

            if (data & 1) {
                if (gs->signal_pending) {
                    gs->siglblid &= ~0xffffffffull;
                    gs->siglblid |= gs->stall_sigid;
                }
            }
        } return;
        case 0x12001010: {
            int prev_signal = (gs->imr >> 8) & 1;
            int new_signal = (data >> 8) & 1;

            gs->imr = data;

            if (gs->signal_pending && (prev_signal && !new_signal)) {
                gs->signal_pending--;

                ee::intc::irq(gs->hw.ee_intc, ee::intc::GS);
            }
        } return;
        case 0x12001040: gs->busdir = data; return;
        case 0x12001080: gs->siglblid = data; return;
    }

    iris_debug(gs, "Unhandled write to {:08x} with data {:016x}", addr, data);
}

// static inline void load_clut_cache(Gs* gs, int i) {
//     // iris_debug(gs, "tbpsm={:x} cbpsm={:x} csm={} csa={:x} ({}) cld={}", //     //     gs->context[i].tbpsm,
//     //     gs->context[i].cbpsm,
//     //     gs->context[i].csm,
//     //     gs->context[i].csa,
//     //     gs->context[i].csa,
//     //     gs->context[i].cld
//     //);

//     switch (gs->context[i].cld) {
//         case 1: break;
//         case 2: gs->cbp0 = gs->context[i].cbp; break;
//         case 3: gs->cbp1 = gs->context[i].cbp; break;
//         case 4: if (gs->context[i].cbp == gs->cbp0) return; break;
//         case 5: if (gs->context[i].cbp == gs->cbp1) return; break;
//         default: return;
//     }

//     switch (gs->context[i].tbpsm) {
//         case PSMT8H:
//         case PSMT8: {
//             switch (gs->context[i].cbpsm) {
//                 case PSMCT32: {
//                     for (int y = 0; y < 16; y++) {
//                         for (int x = 0; x < 16; x++) {
//                             uint32_t cache_addr = (gs->context[i].csa * 16) + (x + (y * 16));
//                             uint32_t vram_addr = gs->context[i].cbp + (x + (y * 64));

//                             gs->clut_cache[cache_addr] = gs->vram[vram_addr];
//                         }
//                     }
//                 } break;

//                 case PSMCT16:
//                 case PSMCT16S: {
//                     iris_debug(gs, "16bpp 8-bit CLUT");

//                     exit(1);
//                 } break;
//             }
//         } break;

//         case PSMT4HH:
//         case PSMT4HL:
//         case PSMT4: {
//             switch (gs->context[i].cbpsm) {
//                 case PSMCT32: {
//                     for (int y = 0; y < 2; y++) {
//                         for (int x = 0; x < 8; x++) {
//                             uint32_t cache_addr = (gs->context[i].csa * 16) + (x + (y * 8));
//                             uint32_t vram_addr = gs->context[i].cbp + (x + (y * 64));

//                             gs->clut_cache[cache_addr] = gs->vram[vram_addr];
//                         }
//                     }
//                 } break;

//                 case PSMCT16:
//                 case PSMCT16S: {
//                     iris_debug(gs, "16bpp 4-bit CLUT");

//                     exit(1);
//                 } break;
//             }
//         } break;
//     }
// }

// static inline void unpack_tex0(Gs* gs, int i) {
//     gs->context[i].tbp0 = gs->context[i].tex0 & 0x3fff;
//     gs->context[i].tbw = (gs->context[i].tex0 >> 14) & 0x3f;
//     gs->context[i].tbpsm = (gs->context[i].tex0 >> 20) & 0x3f;
//     gs->context[i].usize = 1 << ((gs->context[i].tex0 >> 26) & 0xf); // tw
//     gs->context[i].vsize = 1 << ((gs->context[i].tex0 >> 30) & 0xf); // th
//     gs->context[i].tcc = (gs->context[i].tex0 >> 34) & 1;
//     gs->context[i].tfx = (gs->context[i].tex0 >> 35) & 3;
//     gs->context[i].cbp = (gs->context[i].tex0 >> 37) & 0x3fff;
//     gs->context[i].cbpsm = (gs->context[i].tex0 >> 51) & 0xf;
//     gs->context[i].csm = (gs->context[i].tex0 >> 55) & 1;
//     gs->context[i].csa = (gs->context[i].tex0 >> 56) & 0x1f;
//     gs->context[i].cld = (gs->context[i].tex0 >> 61) & 7;

//     gs->context[i].usize = (gs->context[i].usize > 1024) ? 1024 : gs->context[i].usize;
//     gs->context[i].vsize = (gs->context[i].vsize > 1024) ? 1024 : gs->context[i].vsize;

//     if (gs->context[i].cld) {
//         // iris_debug(gs, "CLUT cache load (mode {}, dbp={:08x})", gs->context[i].cld, gs->context[i].cbp);
//         // load_clut_cache(gs, i);
//     }
// }

// static inline void unpack_clamp(Gs* gs, int i) {
//     gs->context[i].wms = gs->context[i].clamp & 3;
//     gs->context[i].wmt = (gs->context[i].clamp >> 2) & 3;
//     gs->context[i].minu = (gs->context[i].clamp >> 4) & 0x3ff;
//     gs->context[i].maxu = (gs->context[i].clamp >> 14) & 0x3ff;
//     gs->context[i].minv = (gs->context[i].clamp >> 24) & 0x3ff;
//     gs->context[i].maxv = (gs->context[i].clamp >> 34) & 0x3ff;
// }

// static inline void unpack_tex1(Gs* gs, int i) {
//     gs->context[i].lcm = gs->context[i].tex1 & 1;
//     gs->context[i].mxl = (gs->context[i].tex1 >> 2) & 7;
//     gs->context[i].mmag = (gs->context[i].tex1 >> 5) & 1;
//     gs->context[i].mmin = (gs->context[i].tex1 >> 6) & 7;
//     gs->context[i].mtba = (gs->context[i].tex1 >> 9) & 1;
//     gs->context[i].l = (gs->context[i].tex1 >> 19) & 3;
//     gs->context[i].k = gs->context[i].tex1 >> 32;
// }

// static inline void unpack_tex2(Gs* gs, int i) {
//     gs->context[i].tbpsm = (gs->context[i].tex2 >> 20) & 0x3f;
//     gs->context[i].cbp = (gs->context[i].tex2 >> 37) & 0x3fff;
//     gs->context[i].cbpsm = (gs->context[i].tex2 >> 51) & 0xf;
//     gs->context[i].csm = (gs->context[i].tex2 >> 55) & 1;
//     gs->context[i].csa = (gs->context[i].tex2 >> 56) & 0x1f;
//     gs->context[i].cld = (gs->context[i].tex2 >> 61) & 7;

//     if (gs->context[i].cld) {
//         // iris_debug(gs, "CLUT cache load (mode {}, dbp={:08x})", gs->context[i].cld, gs->context[i].cbp);
//         // load_clut_cache(gs, i);
//     }

//     // if (gs->context[i].cld) {
//     //     load_clut_cache(gs, i);
//     // }
// }

// static inline void unpack_xyoffset(Gs* gs, int i) {
//     gs->context[i].ofx = gs->context[i].xyoffset & 0xffff;
//     gs->context[i].ofy = (gs->context[i].xyoffset >> 32) & 0xffff;
// }

// static inline void unpack_miptbp1(Gs* gs, int i) {
//     gs->context[i].mmtbp[0] = gs->context[i].miptbp1 & 0x3fff;
//     gs->context[i].mmtbw[0] = (gs->context[i].miptbp1 >> 14) & 0x3f;
//     gs->context[i].mmtbp[1] = (gs->context[i].miptbp1 >> 20) & 0x3fff;
//     gs->context[i].mmtbw[1] = (gs->context[i].miptbp1 >> 34) & 0x3f;
//     gs->context[i].mmtbp[2] = (gs->context[i].miptbp1 >> 40) & 0x3fff;
//     gs->context[i].mmtbw[2] = (gs->context[i].miptbp1 >> 54) & 0x3f;
// }

// static inline void unpack_miptbp2(Gs* gs, int i) {
//     gs->context[i].mmtbp[3] = gs->context[i].miptbp2 & 0x3fff;
//     gs->context[i].mmtbw[3] = (gs->context[i].miptbp2 >> 14) & 0x3f;
//     gs->context[i].mmtbp[4] = (gs->context[i].miptbp2 >> 20) & 0x3fff;
//     gs->context[i].mmtbw[4] = (gs->context[i].miptbp2 >> 34) & 0x3f;
//     gs->context[i].mmtbp[5] = (gs->context[i].miptbp2 >> 40) & 0x3fff;
//     gs->context[i].mmtbw[5] = (gs->context[i].miptbp2 >> 54) & 0x3f;
// }

// static inline void unpack_scissor(Gs* gs, int i) {
//     gs->context[i].scax0 = gs->context[i].scissor & 0x3ff;
//     gs->context[i].scay0 = (gs->context[i].scissor >> 32) & 0x3ff;
//     gs->context[i].scax1 = (gs->context[i].scissor >> 16) & 0x3ff;
//     gs->context[i].scay1 = (gs->context[i].scissor >> 48) & 0x3ff;
// }

// static inline void unpack_alpha(Gs* gs, int i) {
//     gs->context[i].a = gs->context[i].alpha & 3;
//     gs->context[i].b = (gs->context[i].alpha >> 2) & 3;
//     gs->context[i].c = (gs->context[i].alpha >> 4) & 3;
//     gs->context[i].d = (gs->context[i].alpha >> 6) & 3;
//     gs->context[i].fix = (gs->context[i].alpha >> 32) & 0xff;
// }

// static inline void unpack_test(Gs* gs, int i) {
//     gs->context[i].ate = gs->context[i].test & 1;
//     gs->context[i].atst = (gs->context[i].test >> 1) & 7;
//     gs->context[i].aref = (gs->context[i].test >> 4) & 0xff;
//     gs->context[i].afail = (gs->context[i].test >> 12) & 3;
//     gs->context[i].date = (gs->context[i].test >> 14) & 1;
//     gs->context[i].datm = (gs->context[i].test >> 15) & 1;
//     gs->context[i].zte = (gs->context[i].test >> 16) & 1;
//     gs->context[i].ztst = (gs->context[i].test >> 17) & 3;
// }

// static inline void unpack_frame(Gs* gs, int i) {
//     gs->context[i].fbp = (gs->context[i].frame & 0x1ff) << 11;
//     gs->context[i].fbw = ((gs->context[i].frame >> 16) & 0x3f) << 6;
//     gs->context[i].fbpsm = (gs->context[i].frame >> 24) & 0x3f;
//     gs->context[i].fbmsk = gs->context[i].frame >> 32;
// }

// static inline void unpack_zbuf(Gs* gs, int i) {
//     gs->context[i].zbp = (gs->context[i].zbuf & 0x1ff) << 11;
//     gs->context[i].zbpsm = (gs->context[i].zbuf >> 24) & 0xf;
//     gs->context[i].zbmsk = (gs->context[i].zbuf >> 32) & 1;
// }

// static inline void unpack_texclut(Gs* gs) {
//     gs->cbw = gs->texclut & 0x3f;
//     gs->cou = ((gs->texclut >> 6) & 0x3f) << 4;
//     gs->cov = (gs->texclut >> 12) & 0x3ff;
// }

// static inline void unpack_texa(Gs* gs) {
//     gs->ta0 = gs->texa & 0xff;
//     gs->aem = (gs->texa >> 15) & 1;
//     gs->ta1 = (gs->texa >> 32) & 0xff;
// }

// static inline void unpack_dimx(Gs* gs) {
//     gs->dither[0][0] = ((int32_t)(((gs->dimx >> 0 ) & 7) << 29)) >> 29;
//     gs->dither[0][1] = ((int32_t)(((gs->dimx >> 4 ) & 7) << 29)) >> 29;
//     gs->dither[0][2] = ((int32_t)(((gs->dimx >> 8 ) & 7) << 29)) >> 29;
//     gs->dither[0][3] = ((int32_t)(((gs->dimx >> 12) & 7) << 29)) >> 29;
//     gs->dither[1][0] = ((int32_t)(((gs->dimx >> 16) & 7) << 29)) >> 29;
//     gs->dither[1][1] = ((int32_t)(((gs->dimx >> 20) & 7) << 29)) >> 29;
//     gs->dither[1][2] = ((int32_t)(((gs->dimx >> 24) & 7) << 29)) >> 29;
//     gs->dither[1][3] = ((int32_t)(((gs->dimx >> 28) & 7) << 29)) >> 29;
//     gs->dither[2][0] = ((int32_t)(((gs->dimx >> 32) & 7) << 29)) >> 29;
//     gs->dither[2][1] = ((int32_t)(((gs->dimx >> 36) & 7) << 29)) >> 29;
//     gs->dither[2][2] = ((int32_t)(((gs->dimx >> 40) & 7) << 29)) >> 29;
//     gs->dither[2][3] = ((int32_t)(((gs->dimx >> 44) & 7) << 29)) >> 29;
//     gs->dither[3][0] = ((int32_t)(((gs->dimx >> 48) & 7) << 29)) >> 29;
//     gs->dither[3][1] = ((int32_t)(((gs->dimx >> 52) & 7) << 29)) >> 29;
//     gs->dither[3][2] = ((int32_t)(((gs->dimx >> 56) & 7) << 29)) >> 29;
//     gs->dither[3][3] = ((int32_t)(((gs->dimx >> 60) & 7) << 29)) >> 29;
// }

// void write_internal(Gs* gs, int reg, uint64_t data) {
//     switch (reg) {
//         case 0x00: /* iris_debug(gs, "PRIM <- {:016x}", data); */ gs->prim = data; start_primitive(gs); return;
//         case 0x01: /* iris_debug(gs, "RGBAQ <- {:016x}", data); */ gs->rgbaq = data; return;
//         case 0x02: /* iris_debug(gs, "ST <- {:016x}", data); */ gs->st = data; return;
//         case 0x03: /* iris_debug(gs, "UV <- {:016x}", data); */ gs->uv = data; return;
//         case 0x04: /* iris_debug(gs, "XYZF2 <- {:016x}", data); */ gs->xyzf2 = data; write_vertex_fog(gs, gs->xyzf2, 0); return;
//         case 0x05: /* iris_debug(gs, "XYZ2 <- {:016x}", data); */ gs->xyz2 = data; write_vertex_no_fog(gs, gs->xyz2, 0); return;
//         case 0x06: /* iris_debug(gs, "TEX0_1 <- {:016x}", data); */ gs->context[0].tex0 = data; unpack_tex0(gs, 0); return;
//         case 0x07: /* iris_debug(gs, "TEX0_2 <- {:016x}", data); */ gs->context[1].tex0 = data; unpack_tex0(gs, 1); return;
//         case 0x08: /* iris_debug(gs, "CLAMP_1 <- {:016x}", data); */ gs->context[0].clamp = data; unpack_clamp(gs, 0); return;
//         case 0x09: /* iris_debug(gs, "CLAMP_2 <- {:016x}", data); */ gs->context[1].clamp = data; unpack_clamp(gs, 1); return;
//         case 0x0A: /* iris_debug(gs, "FOG <- {:016x}", data); */ gs->fog = data; return;
//         case 0x0C: /* iris_debug(gs, "XYZF3 <- {:016x}", data); */ gs->xyzf3 = data; write_vertex_fog(gs, gs->xyzf3, 1); return;
//         case 0x0D: /* iris_debug(gs, "XYZ3 <- {:016x}", data); */ gs->xyz3 = data; write_vertex_no_fog(gs, gs->xyz3, 1); return;
//         case 0x14: /* iris_debug(gs, "TEX1_1 <- {:016x}", data); */ gs->context[0].tex1 = data; unpack_tex1(gs, 0); return;
//         case 0x15: /* iris_debug(gs, "TEX1_2 <- {:016x}", data); */ gs->context[1].tex1 = data; unpack_tex1(gs, 1); return;
//         case 0x16: /* iris_debug(gs, "TEX2_1 <- {:016x}", data); */ gs->context[0].tex2 = data; unpack_tex2(gs, 0); return;
//         case 0x17: /* iris_debug(gs, "TEX2_2 <- {:016x}", data); */ gs->context[1].tex2 = data; unpack_tex2(gs, 1); return;
//         case 0x18: /* iris_debug(gs, "XYOFFSET_1 <- {:016x}", data); */ gs->context[0].xyoffset = data; unpack_xyoffset(gs, 0); return;
//         case 0x19: /* iris_debug(gs, "XYOFFSET_2 <- {:016x}", data); */ gs->context[1].xyoffset = data; unpack_xyoffset(gs, 1); return;
//         case 0x1A: /* iris_debug(gs, "PRMODECONT <- {:016x}", data); */ gs->prmodecont = data; return;
//         case 0x1B: /* iris_debug(gs, "PRMODE <- {:016x}", data); */ gs->prmode = data; return;
//         case 0x1C: /* iris_debug(gs, "TEXCLUT <- {:016x}", data); */ gs->texclut = data; unpack_texclut(gs); return;
//         case 0x22: /* iris_debug(gs, "SCANMSK <- {:016x}", data); */ gs->scanmsk = data; return;
//         case 0x34: /* iris_debug(gs, "MIPTBP1_1 <- {:016x}", data); */ gs->context[0].miptbp1 = data; unpack_miptbp1(gs, 0); return;
//         case 0x35: /* iris_debug(gs, "MIPTBP1_2 <- {:016x}", data); */ gs->context[1].miptbp1 = data; unpack_miptbp1(gs, 1); return;
//         case 0x36: /* iris_debug(gs, "MIPTBP2_1 <- {:016x}", data); */ gs->context[0].miptbp2 = data; unpack_miptbp2(gs, 0); return;
//         case 0x37: /* iris_debug(gs, "MIPTBP2_2 <- {:016x}", data); */ gs->context[1].miptbp2 = data; unpack_miptbp2(gs, 1); return;
//         case 0x3B: /* iris_debug(gs, "TEXA <- {:016x}", data); */ gs->texa = data; unpack_texa(gs); return;
//         case 0x3D: /* iris_debug(gs, "FOGCOL <- {:016x}", data); */ gs->fogcol = data; return;
//         case 0x3F: /* iris_debug(gs, "TEXFLUSH <- {:016x}", data); */ gs->texflush = data; return;
//         case 0x40: /* iris_debug(gs, "SCISSOR_1 <- {:016x}", data); */ gs->context[0].scissor = data; unpack_scissor(gs, 0); invoke_event_handler(gs, EVENT_SCISSOR); return;
//         case 0x41: /* iris_debug(gs, "SCISSOR_2 <- {:016x}", data); */ gs->context[1].scissor = data; unpack_scissor(gs, 1); invoke_event_handler(gs, EVENT_SCISSOR); return;
//         case 0x42: /* iris_debug(gs, "ALPHA_1 <- {:016x}", data); */ gs->context[0].alpha = data; unpack_alpha(gs, 0); return;
//         case 0x43: /* iris_debug(gs, "ALPHA_2 <- {:016x}", data); */ gs->context[1].alpha = data; unpack_alpha(gs, 1); return;
//         case 0x44: /* iris_debug(gs, "DIMX <- {:016x}", data); */ gs->dimx = data; unpack_dimx(gs); return;
//         case 0x45: /* iris_debug(gs, "DTHE <- {:016x}", data); */ gs->dthe = data; return;
//         case 0x46: /* iris_debug(gs, "COLCLAMP <- {:016x}", data); */ gs->colclamp = data; return;
//         case 0x47: /* iris_debug(gs, "TEST_1 <- {:016x}", data); */ gs->context[0].test = data; unpack_test(gs, 0); return;
//         case 0x48: /* iris_debug(gs, "TEST_2 <- {:016x}", data); */ gs->context[1].test = data; unpack_test(gs, 1); return;
//         case 0x49: /* iris_debug(gs, "PABE <- {:016x}", data); */ gs->pabe = data; return;
//         case 0x4A: /* iris_debug(gs, "FBA_1 <- {:016x}", data); */ gs->context[0].fba = data; return;
//         case 0x4B: /* iris_debug(gs, "FBA_2 <- {:016x}", data); */ gs->context[1].fba = data; return;
//         case 0x4C: /* iris_debug(gs, "FRAME_1 <- {:016x}", data); */ gs->context[0].frame = data; unpack_frame(gs, 0); return;
//         case 0x4D: /* iris_debug(gs, "FRAME_2 <- {:016x}", data); */ gs->context[1].frame = data; unpack_frame(gs, 1); return;
//         case 0x4E: /* iris_debug(gs, "ZBUF_1 <- {:016x}", data); */ gs->context[0].zbuf = data; unpack_zbuf(gs, 0); return;
//         case 0x4F: /* iris_debug(gs, "ZBUF_2 <- {:016x}", data); */ gs->context[1].zbuf = data; unpack_zbuf(gs, 1); return;
//         case 0x50: /* iris_debug(gs, "BITBLTBUF <- {:016x}", data); */ gs->bitbltbuf = data; return;
//         case 0x51: /* iris_debug(gs, "TRXPOS <- {:016x}", data); */ gs->trxpos = data; return;
//         case 0x52: /* iris_debug(gs, "TRXREG <- {:016x}", data); */ gs->trxreg = data; return;
//         case 0x53: /* iris_debug(gs, "TRXDIR <- {:016x}", data); */ gs->trxdir = data; gs->backend.transfer_start(gs, gs->backend.udata); return;
//         case 0x54: gs->hwreg = data; gs->backend.transfer_write(gs, gs->backend.udata); return;
//         case 0x60: /* iris_debug(gs, "SIGNAL <- {:016x}", data); */ {
//             uint64_t mask = data >> 32;
//             uint64_t value = data & mask;

//             if (gs->csr & 1) {
//                 gs->signal_pending++;

//                 gs->stall_sigid = gs->siglblid & 0xffffffff;
//                 gs->stall_sigid &= ~mask;
//                 gs->stall_sigid |= value;

//                 return;
//             }

//             gs->signal_pending++;
//             gs->signal = data;

//             gs->csr |= 1;
//             gs->siglblid &= ~mask;
//             gs->siglblid |= value;

//             test_gs_irq(gs);
//         } return;
//         case 0x61: /* iris_debug(gs, "FINISH <- {:016x}", data); */ {
//             // Trigger FINISH event
//             gs->csr |= 2;

//             test_gs_irq(gs);
//         } return;
//         case 0x62: /* iris_debug(gs, "LABEL <- {:016x}", data); */ {
//             gs->label = data;

//             uint64_t mask = data >> 32;

//             gs->siglblid &= (~mask) << 32;
//             gs->siglblid |= (data & mask) << 32;
//         } break;
//         default: {
//             // iris_debug(gs, "Invalid privileged register {:02x} write {:016x}", reg, data);

//             return;
//         }
//     }
// }

// uint64_t read_internal(Gs* gs, int reg) {
//     switch (reg) {
//         case 0x00: return gs->prim;
//         case 0x01: return gs->rgbaq;
//         case 0x02: return gs->st;
//         case 0x03: return gs->uv;
//         case 0x04: return gs->xyzf2;
//         case 0x05: return gs->xyz2;
//         case 0x06: return gs->context[0].tex0;
//         case 0x07: return gs->context[1].tex0;
//         case 0x08: return gs->context[0].clamp;
//         case 0x09: return gs->context[1].clamp;
//         case 0x0A: return gs->fog;
//         case 0x0C: return gs->xyzf3;
//         case 0x0D: return gs->xyz3;
//         case 0x14: return gs->context[0].tex1;
//         case 0x15: return gs->context[1].tex1;
//         case 0x16: return gs->context[0].tex2;
//         case 0x17: return gs->context[1].tex2;
//         case 0x18: return gs->context[0].xyoffset;
//         case 0x19: return gs->context[1].xyoffset;
//         case 0x1A: return gs->prmodecont;
//         case 0x1B: return gs->prmode;
//         case 0x1C: return gs->texclut;
//         case 0x22: return gs->scanmsk;
//         case 0x34: return gs->context[0].miptbp1;
//         case 0x35: return gs->context[1].miptbp1;
//         case 0x36: return gs->context[0].miptbp2;
//         case 0x37: return gs->context[1].miptbp2;
//         case 0x3B: return gs->texa;
//         case 0x3D: return gs->fogcol;
//         case 0x3F: return gs->texflush;
//         case 0x40: return gs->context[0].scissor;
//         case 0x41: return gs->context[1].scissor;
//         case 0x42: return gs->context[0].alpha;
//         case 0x43: return gs->context[1].alpha;
//         case 0x44: return gs->dimx;
//         case 0x45: return gs->dthe;
//         case 0x46: return gs->colclamp;
//         case 0x47: return gs->context[0].test;
//         case 0x48: return gs->context[1].test;
//         case 0x49: return gs->pabe;
//         case 0x4A: return gs->context[0].fba;
//         case 0x4B: return gs->context[1].fba;
//         case 0x4C: return gs->context[0].frame;
//         case 0x4D: return gs->context[1].frame;
//         case 0x4E: return gs->context[0].zbuf;
//         case 0x4F: return gs->context[1].zbuf;
//         case 0x50: return gs->bitbltbuf;
//         case 0x51: return gs->trxpos;
//         case 0x52: return gs->trxreg;
//         case 0x53: return gs->trxdir;
//         case 0x54: gs->backend.transfer_read(gs, gs->backend.udata); return gs->hwreg;
//         case 0x60: return gs->signal;
//         case 0x61: return gs->finish;
//         case 0x62: return gs->label;
//         default: {
//             iris_debug(gs, "Invalid privileged register {:02x} read", reg);

//             exit(1);
//         }
//     }

//     return 0;
// }

void get_privileged_state(Gs* gs, PrivilegedState* state) {
    state->pmode = gs->pmode;
    state->smode1 = gs->smode1;
    state->smode2 = gs->smode2;
    state->srfsh = gs->srfsh;
    state->synch1 = gs->synch1;
    state->synch2 = gs->synch2;
    state->syncv = gs->syncv;
    state->dispfb1 = gs->dispfb1;
    state->display1 = gs->display1;
    state->dispfb2 = gs->dispfb2;
    state->display2 = gs->display2;
    state->extbuf = gs->extbuf;
    state->extdata = gs->extdata;
    state->extwrite = gs->extwrite;
    state->bgcolor = gs->bgcolor;
    state->csr = gs->csr;
    state->imr = gs->imr;
    state->busdir = gs->busdir;
    state->siglblid = gs->siglblid;
}

int is_vblank(Gs* gs) {
    return gs->vblank;
}

int write_signal(Gs* gs, uint64_t data) {
    uint64_t mask = data >> 32;
    uint64_t value = data & mask;

    if (gs->csr & 1) {
        gs->signal_pending++;

        gs->stall_sigid = gs->siglblid & 0xffffffff;
        gs->stall_sigid &= ~mask;
        gs->stall_sigid |= value;

        return 1;
    }

    gs->signal_pending++;
    gs->signal = data;

    gs->csr |= 1;
    gs->siglblid &= ~mask;
    gs->siglblid |= value;

    test_gs_irq(gs);

    return 1;
}

int write_finish(Gs* gs, uint64_t data) {
    // Trigger FINISH event
    gs->csr |= 2;

    test_gs_irq(gs);

    return 1;
}

int write_label(Gs* gs, uint64_t data) {
    gs->label = data;

    uint64_t mask = data >> 32;

    gs->siglblid &= (~mask) << 32;
    gs->siglblid |= (data & mask) << 32;

    return 1;
}

}
