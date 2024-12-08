#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "ee/gs.h"
#include "software.hpp"

#include <SDL.h>

static const int psmct32_clut_block[] = {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    0x40, 0x41, 0x42, 0x43,
    0x44, 0x45, 0x46, 0x47,
    0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f,
    0x48, 0x49, 0x4a, 0x4b,
    0x4c, 0x4d, 0x4e, 0x4f
};

void software_set_size(software_state* ctx, int width, int height) {
    if (ctx->texture)
        SDL_DestroyTexture(ctx->texture);

    int format = 0;

    uint32_t dfbp1 = (ctx->gs->dispfb1 & 0x1ff) << 11;
    uint32_t dfbw1 = ((ctx->gs->dispfb1 >> 9) & 0x3f) << 6;
    uint32_t dfbpsm1 = (ctx->gs->dispfb1 >> 15) & 0x1f;

    switch (dfbpsm1) {
        case GS_PSMCT32: format = SDL_PIXELFORMAT_ABGR8888; break;
        case GS_PSMCT24: format = SDL_PIXELFORMAT_ABGR8888; break;
        case GS_PSMCT16:
        case GS_PSMCT16S:
            format = SDL_PIXELFORMAT_ABGR1555;
        break;
    }

    uint64_t display = ctx->gs->display1 ? ctx->gs->display1 : ctx->gs->display2;

    int sw = (ctx->gs->ctx->scax1 - ctx->gs->ctx->scax0) + 1;
    int sh = (ctx->gs->ctx->scay1 - ctx->gs->ctx->scay0) + 1;
    int dh = (display >> 44) & 0x7ff;

    ctx->texture = SDL_CreateTexture(
        ctx->renderer,
        format,
        SDL_TEXTUREACCESS_STREAMING,
        sw,
        (sh > dh) ? sh : dh
    );

    SDL_SetTextureScaleMode(ctx->texture, SDL_ScaleModeLinear);

    // printf("software: width=%d height=%d format=%d\n",
    //     (ctx->gs->ctx->scax1 - ctx->gs->ctx->scax0) + 1,
    //     (ctx->gs->ctx->scay1 - ctx->gs->ctx->scay0) + 1,
    //     format
    // );
}

#define CLAMP(v, l, u) (((v) > (u)) ? (u) : (((v) < (l)) ? (l) : (v)))

static inline uint32_t gs_apply_function(struct ps2_gs* gs, uint32_t t, uint32_t f) {
    uint32_t pr, pg, pb, pa;
    uint32_t tr = t & 0xff;
    uint32_t tg = (t >> 8) & 0xff;
    uint32_t tb = (t >> 16) & 0xff;
    uint32_t ta = (t >> 24) & 0xff;
    uint32_t fr = f & 0xff;
    uint32_t fg = (f >> 8) & 0xff;
    uint32_t fb = (f >> 16) & 0xff;
    uint32_t fa = (f >> 24) & 0xff;

    switch (gs->ctx->tfx) {
        case GS_MODULATE: {
            pr = CLAMP((tr * fr) >> 7, 0, 255);
            pg = CLAMP((tg * fg) >> 7, 0, 255);
            pb = CLAMP((tb * fb) >> 7, 0, 255);
            pa = ta;

            // if (gs->ctx->tcc) {
            //     pa = CLAMP((ta * fa) >> 7, 0, 255);
            // } else {
            //     pa = fa;
            // }
        } break;
        case GS_DECAL: {
            pr = tr;
            pg = tg;
            pb = tb;
            pa = ta;
        } break;
        case GS_HIGHLIGHT: {
            pa = (f >> 24) & 0xff;
            pr = CLAMP(((tr * fr) >> 7) + pa, 0, 255);
            pg = CLAMP(((tg * fg) >> 7) + pa, 0, 255);
            pb = CLAMP(((tb * fb) >> 7) + pa, 0, 255);
        } break;
        case GS_HIGHLIGHT2: {
            pa = (f >> 24) & 0xff;
            pr = CLAMP(((tr * fr) >> 7) + pa, 0, 255);
            pg = CLAMP(((tg * fg) >> 7) + pa, 0, 255);
            pb = CLAMP(((tb * fb) >> 7) + pa, 0, 255);
        } break;
    }

    return pr | (pg << 8) | (pb << 16) | (pa << 24);
}

enum : int {
    TR_FAIL,
    TR_PASS,
    TR_FB_ONLY,
    TR_ZB_ONLY,
    TR_RGB_ONLY
};

static inline uint32_t gs_read_fb(struct ps2_gs* gs, int x, int y) {
    switch (gs->ctx->fbpsm) {
        case GS_PSMCT32:
            return gs->vram[gs->ctx->fbp + x + (y * gs->ctx->fbw)];
        case GS_PSMCT24:
            return gs->vram[gs->ctx->fbp + x + (y * gs->ctx->fbw)] & 0xffffff;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            int shift = (x & 1) << 4;
            uint32_t mask = 0xffff << shift;
            uint32_t data = gs->vram[gs->ctx->fbp + (x >> 1) + (y * (gs->ctx->fbw >> 1))];

            return (data & mask) >> shift;
        }
    }

    return 0;
}

static inline uint32_t gs_read_zb(struct ps2_gs* gs, int x, int y) {
    switch (gs->ctx->zbpsm) {
        case GS_PSMZ32:
            return gs->vram[gs->ctx->zbp + x + (y * gs->ctx->fbw)];
        case GS_PSMZ24:
            return gs->vram[gs->ctx->zbp + x + (y * gs->ctx->fbw)] & 0xffffff;
        case GS_PSMZ16:
        case GS_PSMZ16S: {
            int shift = (x & 1) << 4;
            uint32_t mask = 0xffff << shift;
            uint32_t data = gs->vram[gs->ctx->zbp + (x >> 1) + (y * (gs->ctx->fbw >> 1))];

            return (data & mask) >> shift;
        }
    }

    return 0;
}

static inline uint32_t gs_read_cb(struct ps2_gs* gs, int i) {
    // CSM2 should be the same?
    // not sure if the different arrangement actually
    // changes how address calculation is performed
    // maybe it only applies to the CLUT cache
    // - Edit: CSM2 uses the TEXCLUT reg, and it's
    // basically just like a normal buffer

    switch (gs->ctx->cbpsm) {
        case GS_PSMCT32: {
            int b = i >> 5;
            int p = psmct32_clut_block[i & 0x1f] + b;

            return gs->vram[gs->ctx->cbp + p];
        } break;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            int shift = (i & 1) << 4;

            uint32_t data = gs->vram[gs->ctx->cbp + (i >> 1)];

            return (data & (0xffff << shift)) >> shift;
        } break;
    }

    return 0;
}

static inline uint32_t gs_to_rgba32(struct ps2_gs* gs, uint32_t c, int fmt) {
    switch (fmt) {
        case GS_PSMCT32:
            return c;
        case GS_PSMCT24:
            return c & 0xffffff;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            return ((c & 0x001f) << 3) |
                   ((c & 0x03e0) << 6) |
                   ((c & 0x7c00) << 9) |
                   ((c & 0x8000) ? 0xff000000 : 0);
        } break;
        case GS_PSMT8:
        case GS_PSMT8H:
        case GS_PSMT4:
        case GS_PSMT4HL:
        case GS_PSMT4HH: {
            return gs_to_rgba32(gs, c, gs->ctx->cbpsm);
        } break;
    }

    return 0;
}

static inline uint32_t gs_from_rgba32(struct ps2_gs* gs, uint32_t c, int fmt) {
    switch (fmt) {
        case GS_PSMCT32:
            return c;
        case GS_PSMCT24:
            // To-do: Use TEXA
            return c & 0xffffff;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            // To-do: Use TEXA
            return ((c & 0x0000ff) >> 3) |
                   ((c & 0x00ff00) >> 6) |
                   ((c & 0xff0000) >> 9) |
                   ((c & 0x80000000) ? 0x8000 : 0);
        } break;
        case GS_PSMT8:
        case GS_PSMT8H:
        case GS_PSMT4:
        case GS_PSMT4HL:
        case GS_PSMT4HH: {
            return gs_from_rgba32(gs, c, gs->ctx->cbpsm);
        } break;
    }

    return 0;
}

static inline uint32_t gs_read_tb(struct ps2_gs* gs, int u, int v) {
    switch (gs->ctx->tbpsm) {
        case GS_PSMCT32:
            return gs->vram[gs->ctx->tbp0 + u + (v * gs->ctx->tbw)];
        case GS_PSMCT24:
            return gs->vram[gs->ctx->tbp0 + u + (v * gs->ctx->tbw)] & 0xffffff;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            int shift = (u & 1) << 4;

            uint32_t data = gs->vram[gs->ctx->tbp0 + (u >> 1) + (v * (gs->ctx->tbw >> 1))];

            return (data & (0xffff << shift)) >> shift;
        } break;
        case GS_PSMT8: {
            int shift = (u & 3) << 3;

            uint32_t data = gs->vram[gs->ctx->tbp0 + (u >> 2) + (v * (gs->ctx->tbw >> 2))];

            int index = (data & (0xff << shift)) >> shift;

            return gs_read_cb(gs, index);
        } break;
        case GS_PSMT8H: {
            uint32_t data = gs->vram[gs->ctx->tbp0 + u + (v * gs->ctx->tbw)];

            return gs_read_cb(gs, data >> 24);
        } break;
        case GS_PSMT4: {
            int shift = (u & 7) << 2;

            uint32_t data = gs->vram[gs->ctx->tbp0 + (u >> 3) + (v * (gs->ctx->tbw >> 3))];

            int index = (data & (0xf << shift)) >> shift;

            return gs_read_cb(gs, index);
        } break;
        case GS_PSMT4HL: {
            uint32_t data = gs->vram[gs->ctx->tbp0 + u + (v * gs->ctx->tbw)];

            return gs_read_cb(gs, (data >> 24) & 0xf);
        } break;
        case GS_PSMT4HH: {
            uint32_t data = gs->vram[gs->ctx->tbp0 + u + (v * gs->ctx->tbw)];

            return gs_read_cb(gs, data >> 28);
        } break;
    }

    return 0;
}

static inline void gs_write_fb(struct ps2_gs* gs, int x, int y, uint32_t c) {
    uint32_t f = gs_from_rgba32(gs, c, gs->ctx->fbpsm);

    // To-do: Implement FBMSK
    switch (gs->ctx->fbpsm) {
        case GS_PSMCT32: {
            gs->vram[gs->ctx->fbp + x + (y * gs->ctx->fbw)] = f | 0xff000000;
        } break;

        case GS_PSMCT24: {
            gs->vram[gs->ctx->fbp + x + (y * gs->ctx->fbw)] = f;
        } break;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            int shift = (x & 1) << 4;
            uint32_t mask = 0xffff << shift;
            uint32_t addr = gs->ctx->fbp + (x >> 1) + (y * (gs->ctx->fbw >> 1));
            uint32_t data = gs->vram[addr] & ~mask;

            gs->vram[addr] = (f & mask) | data;
        } break;
    }
}

static inline void gs_write_zb(struct ps2_gs* gs, int x, int y, float z) {
    // To-do: Implement ZBMSK
    switch (gs->ctx->zbpsm) {
        case GS_PSMZ32: {
            gs->vram[gs->ctx->zbp + x + (y * gs->ctx->fbw)] = z;
        } break;

        case GS_PSMZ24: {
            gs->vram[gs->ctx->zbp + x + (y * gs->ctx->fbw)] = z;
        } break;
        case GS_PSMZ16:
        case GS_PSMZ16S: {
            int shift = (x & 1) << 4;
            uint32_t mask = 0xffff << shift;
            uint32_t addr = gs->ctx->fbp + (x >> 1) + (y * (gs->ctx->fbw >> 1));
            uint32_t data = gs->vram[addr] & ~mask;

            gs->vram[addr] = ((uint32_t)z & mask) | data;
        } break;
    }
}

static inline int gs_test_scissor(struct ps2_gs* gs, int x, int y) {
    if (x < gs->ctx->scax0 || y < gs->ctx->scay0 ||
        x > gs->ctx->scax1 || y > gs->ctx->scay1) {
        return TR_FAIL;
    }

    return TR_PASS;
}

static inline int gs_test_pixel(struct ps2_gs* gs, int x, int y, int z, int a) {
    struct gs_vertex v = gs->vq[0];

    int tr = TR_PASS;

    // Alpha test
    if (gs->ctx->ate) {
        switch (gs->ctx->atst) {
            case 0: tr = TR_FAIL; break;
            case 2: tr = a < gs->ctx->aref; break;
            case 3: tr = a <= gs->ctx->aref; break;
            case 4: tr = a == gs->ctx->aref; break;
            case 5: tr = a >= gs->ctx->aref; break;
            case 6: tr = a > gs->ctx->aref; break;
            case 7: tr = a != gs->ctx->aref; break;
        }

        if (tr == TR_FAIL) {
            switch (gs->ctx->afail) {
                case 0: return TR_FAIL;
                case 1: tr = TR_FB_ONLY; break;
                case 2: tr = TR_ZB_ONLY; break;
                case 3: tr = TR_RGB_ONLY; break;
            }
        }
    }

    // Destination alpha test
    if (gs->ctx->date) {
        uint32_t s = gs_read_fb(gs, x, y);

        switch (gs->ctx->fbpsm) {
            case GS_PSMCT16:
            case GS_PSMCT16S:
                if (((s >> 15) & 1) != gs->ctx->datm) return TR_FAIL;
            case GS_PSMCT32:
                if (((s >> 31) & 1) != gs->ctx->datm) return TR_FAIL;
        }
    }

    // Depth test
    if (gs->ctx->zte) {
        uint32_t zb = gs_read_zb(gs, x, y);

        switch (gs->ctx->ztst) {
            case 0: return TR_FAIL;
            case 2: {
                if (z < zb) return TR_FAIL;
            } break;
            case 3: {
                if (z <= zb) return TR_FAIL;
            } break;
        }
    }

    return tr;
}

void software_init(software_state* ctx, struct ps2_gs* gs, SDL_Window* window, SDL_Renderer* renderer) {
    ctx->window = window;
    ctx->renderer = renderer;
    ctx->gs = gs;
}

static inline void software_vram_blit(struct ps2_gs* gs, software_state* ctx) {
    for (int y = 0; y < ctx->rrh; y++) {
        uint32_t src = ctx->sbp + ctx->ssax + (ctx->ssay * ctx->sbw) + (y * ctx->sbw);
        uint32_t dst = ctx->dbp + ctx->dsax + (ctx->dsay * ctx->dbw) + (y * ctx->dbw);

        memcpy(gs->vram + dst, gs->vram + src, ctx->rrw * sizeof(uint32_t));
    }
}

extern "C" void software_render_point(struct ps2_gs* gs, void* udata) {
    software_state* ctx = (software_state*)udata;

    struct gs_vertex vert = gs->vq[0];

    vert.x -= gs->ctx->ofx;
    vert.y -= gs->ctx->ofy;

    int tr = gs_test_pixel(gs, vert.x, vert.y, vert.z, vert.a);

    if (tr == TR_FAIL)
        return;

    gs_write_fb(gs, vert.x, vert.y, vert.rgbaq & 0xffffffff);

    return;

    if (gs->tme) {
        int u = vert.u;
        int v = vert.v;

        if (!gs->fst) {
            u = gs->ctx->usize * (vert.s / vert.q);
            v = gs->ctx->vsize * (vert.t / vert.q);
        }

        switch (gs->ctx->wms) {
            case 0: {
                u %= gs->ctx->usize;
            } break;

            case 1: {
                u = (u < 0) ? 0 : ((u > gs->ctx->usize) ? gs->ctx->usize : u);
            } break;

            case 2: {
                u = (u < gs->ctx->minu) ? gs->ctx->minu : ((u > gs->ctx->maxu) ? gs->ctx->maxu : u);
            } break;

            case 3: {
                int umsk = gs->ctx->minu;
                int ufix = gs->ctx->maxu;

                u = (u & umsk) | ufix;
            } break;
        }

        switch (gs->ctx->wmt) {
            case 0: {
                v %= gs->ctx->vsize;
            } break;

            case 1: {
                v = (v < 0) ? 0 : ((v > gs->ctx->vsize) ? gs->ctx->vsize : u);
            } break;

            case 2: {
                v = (v < gs->ctx->minv) ? gs->ctx->minv : ((u > gs->ctx->maxv) ? gs->ctx->maxv : v);
            } break;

            case 3: {
                int vmsk = gs->ctx->minv;
                int vfix = gs->ctx->maxv;

                v = (v & vmsk) | vfix;
            } break;
        }

        uint32_t c = gs_read_tb(gs, u, v);
    }
}

extern "C" void software_render_line(struct ps2_gs* gs, void* udata) {

}

#define EDGE(a, b, c) ((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))
#define MIN2(a, b) (((a) < (b)) ? (a) : (b))
#define MIN3(a, b, c) (MIN2(MIN2(a, b), c))
#define MAX2(a, b) (((a) > (b)) ? (a) : (b))
#define MAX3(a, b, c) (MAX2(MAX2(a, b), c))

#define IS_TOPLEFT(a, b) ((b.y > a.y) || ((a.y == b.y) && (b.x < a.x)))

extern "C" void software_render_triangle(struct ps2_gs* gs, void* udata) {
    struct gs_vertex v0 = gs->vq[0];
    struct gs_vertex v1 = gs->vq[1];
    struct gs_vertex v2 = gs->vq[2];

    if (EDGE(v0, v1, v2) < 0) {
        v1 = gs->vq[2];
        v2 = gs->vq[1];
    }

    v0.x -= gs->ctx->ofx;
    v1.x -= gs->ctx->ofx;
    v2.x -= gs->ctx->ofx;
    v0.y -= gs->ctx->ofy;
    v1.y -= gs->ctx->ofy;
    v2.y -= gs->ctx->ofy;

    int xmin = MIN3(v0.x, v1.x, v2.x);
    int ymin = MIN3(v0.y, v1.y, v2.y);
    int xmax = MAX3(v0.x, v1.x, v2.x);
    int ymax = MAX3(v0.y, v1.y, v2.y);

    xmin = MAX2(xmin, gs->ctx->scax0);
    ymin = MAX2(ymin, gs->ctx->scay0);
    xmax = MIN2(xmax, gs->ctx->scax1);
    ymax = MIN2(ymax, gs->ctx->scay1);

    int a01 = (int)v0.y - (int)v1.y, b01 = (int)v1.x - (int)v0.x;
    int a12 = (int)v1.y - (int)v2.y, b12 = (int)v2.x - (int)v1.x;
    int a20 = (int)v2.y - (int)v0.y, b20 = (int)v0.x - (int)v2.x;

    struct gs_vertex p;
    p.x = xmin;
    p.y = ymin;

    int bias0 = IS_TOPLEFT(v1, v2) ? 0 : -1;
    int bias1 = IS_TOPLEFT(v2, v0) ? 0 : -1;
    int bias2 = IS_TOPLEFT(v0, v1) ? 0 : -1;
    int w0_row = EDGE(v1, v2, p) + bias0;
    int w1_row = EDGE(v2, v0, p) + bias1;
    int w2_row = EDGE(v0, v1, p) + bias2;

    // printf("triangle: v0=(%d,%d,%d) v1=(%d,%d,%d) v2=(%d,%d,%d) iip=%d tme=%d fst=%d abe=%d tfx=%d tcc=%d zte=%d\n",
    //     v0.x, v0.y, v0.z,
    //     v1.x, v1.y, v1.z,
    //     v2.x, v2.y, v2.z,
    //     gs->iip,
    //     gs->tme,
    //     gs->fst,
    //     gs->abe,
    //     gs->ctx->tfx,
    //     gs->ctx->tcc,
    //     gs->ctx->zte
    // );

    int area = EDGE(v0, v1, v2);

    for (p.y = ymin; p.y <= ymax; p.y++) {
        // Barycentric coordinates at start of row
        int w0 = w0_row;
        int w1 = w1_row;
        int w2 = w2_row;

        for (p.x = xmin; p.x <= xmax; p.x++) {
            // If p is on or inside all edges, render pixel.
            if ((w0 | w1 | w2) < 0) {
                w0 += a12;
                w1 += a20;
                w2 += a01;

                continue;
            }

            // Calculate interpolation weights
            float iw0 = (float)w0 / (float)area;
            float iw1 = (float)w1 / (float)area;
            float iw2 = (float)w2 / (float)area;

            uint32_t fr, fg, fb, fa;

            if (gs->iip) {
                fr = v0.r * iw0 + v1.r * iw1 + v2.r * iw2;
                fg = v0.g * iw0 + v1.g * iw1 + v2.g * iw2;
                fb = v0.b * iw0 + v1.b * iw1 + v2.b * iw2;
                fa = v0.a * iw0 + v1.a * iw1 + v2.a * iw2;
            } else {
                fr = v2.r;
                fg = v2.g;
                fb = v2.b;
                fa = v2.a;
            }

            if (gs->tme) {
                float u, v;

                if (gs->fst) {
                    u = v0.u * iw0 + v1.u * iw1 + v2.u * iw2;
                    v = v0.v * iw0 + v1.v * iw1 + v2.v * iw2;
                } else {
                    float s = v0.s * iw0 + v1.s * iw1 + v2.s * iw2;
                    float t = v0.t * iw0 + v1.t * iw1 + v2.t * iw2;
                    float q = v0.q * iw0 + v1.q * iw1 + v2.q * iw2;

                    u = (s / q) * gs->ctx->usize;
                    v = (t / q) * gs->ctx->vsize;
                }

                uint32_t f = fr | (fg << 8) | (fb << 16);
                uint32_t t = gs_read_tb(gs, u, v);
                t = gs_to_rgba32(gs, t, gs->ctx->tbpsm);
                t = gs_apply_function(gs, t, f);

                fr = t & 0xff;
                fg = (t >> 8) & 0xff;
                fb = (t >> 16) & 0xff;
                fa = t >> 24;
            }

            uint32_t fz = v0.z * iw0 + v1.z * iw1 + v2.z * iw2;

            int tr = gs_test_pixel(gs, p.x, p.y, fz, fa);

            if (tr == TR_FAIL) {
                w0 += a12;
                w1 += a20;
                w2 += a01;

                continue;
            }

            uint32_t fc = fr | (fg << 8) | (fb << 16) | (fa << 24);

            fc = gs_from_rgba32(gs, fc, gs->ctx->fbpsm);

            switch (tr) {
                case TR_FB_ONLY: gs_write_fb(gs, p.x, p.y, fc); break;
                case TR_ZB_ONLY: gs_write_zb(gs, p.x, p.y, fz); break;
                // To-do: case TR_RGB_ONLY: gs_write_fb(gs, x, y, c); break;
                case TR_PASS: {
                    gs_write_fb(gs, p.x, p.y, fc);
                    gs_write_zb(gs, p.x, p.y, fz);
                } break;
            }

            // One step to the right
            w0 += a12;
            w1 += a20;
            w2 += a01;
        }

        // One row step
        w0_row += b12;
        w1_row += b20;
        w2_row += b01;
    }
}

extern "C" void software_render_sprite(struct ps2_gs* gs, void* udata) {
    struct gs_vertex v0 = gs->vq[0];
    struct gs_vertex v1 = gs->vq[1];

    v0.x -= gs->ctx->ofx;
    v1.x -= gs->ctx->ofx;
    v0.y -= gs->ctx->ofy;
    v1.y -= gs->ctx->ofy;

    int xmin = (int32_t)std::min(v0.x, v1.x);
    int ymin = (int32_t)std::min(v0.y, v1.y);
    int xmax = (int32_t)std::max(v0.x, v1.x);
    int ymax = (int32_t)std::max(v0.y, v1.y);

    // printf("gs: Sprite at (%d,%d-%d,%d) tme=%d abe=%d rgba=%08x\n",
    //     v0.x, v0.y,
    //     v1.x, v1.y,
    //     gs->tme,
    //     gs->abe,
    //     v1.rgbaq & 0xffffffff
    // );

    // printf("gs: TB format=%d tbp=%x fbw=%d CB format=%d cbp=%x csa=%d tfx=%d rgba=%08x abe=%d\n",
    //     gs->ctx->tbpsm,
    //     gs->ctx->tbp0,
    //     gs->ctx->tbw,
    //     gs->ctx->cbpsm,
    //     gs->ctx->cbp,
    //     gs->ctx->csa,
    //     gs->ctx->tfx,
    //     v1.rgbaq & 0xffffffff,
    //     gs->abe
    // );
    int z = v1.z;
    int a = v1.a;
    int r = v1.r;
    int g = v1.g;
    int b = v1.b;

    // if (!gs->fst) {
    //     printf("gs: stq0=(%f,%f,%f) stq1=(%f,%f,%f)\n",
    //         v0.s, v0.t, v0.q,
    //         v1.s, v1.t, v1.q
    //     );
    // } else {
    //     printf("gs: uv0=(%d,%d) uv1=(%d,%d)\n",
    //         v0.u, v0.v,
    //         v1.u, v1.v
    //     );
    // }

    float u = v0.u;
    float v = v0.v;

    for (int y = ymin; y < ymax; y++) {
        for (int x = xmin; x < xmax; x++) {
            if (!gs_test_scissor(gs, x, y))
                continue;

            uint32_t c = v1.rgbaq & 0xffffffff;

            if (gs->tme) {
                float tx = (float)(x - xmin) / (float)(xmax - xmin);
                float ty = (float)(y - ymin) / (float)(ymax - ymin);

                if (gs->fst) {
                    u = v0.u + (v1.u - v0.u) * tx;
                    v = v0.v + (v1.v - v0.v) * ty;
                } else {
                    u = v0.s + (v1.s - v0.s) * tx;
                    v = v0.t + (v1.t - v0.t) * ty;
                    
                    u *= gs->ctx->usize;
                    v *= gs->ctx->vsize;
                }

                uint32_t f = c;

                c = gs_read_tb(gs, u, v);
                c = gs_to_rgba32(gs, c, gs->ctx->tbpsm);
                c = gs_apply_function(gs, c, f);

                a = c >> 24;
            }

            int tr = gs_test_pixel(gs, x, y, z, a);

            if (tr == TR_FAIL)
                continue;

            switch (tr) {
                case TR_FB_ONLY: gs_write_fb(gs, x, y, c); break;
                case TR_ZB_ONLY: gs_write_zb(gs, x, y, z); break;
                // To-do: case TR_RGB_ONLY: gs_write_fb(gs, x, y, c); break;
                case TR_PASS: {
                    gs_write_fb(gs, x, y, c);
                    gs_write_zb(gs, x, y, z);
                } break;
            }
        }
    }
}

extern "C" void software_render(struct ps2_gs* gs, void* udata) {
    software_state* ctx = (software_state*)udata;

    int stride;

    uint32_t dispfb = ctx->gs->dispfb2 ? ctx->gs->dispfb2 : ctx->gs->dispfb2;

    uint32_t dfbp = (dispfb & 0x1ff) << 11;
    uint32_t* ptr = &gs->vram[dfbp];
    uint32_t dfbw = ((dispfb >> 9) & 0x3f) << 6;
    uint32_t dfbpsm = (dispfb >> 15) & 0x1f;

    switch (dfbpsm) {
        case GS_PSMCT32: stride = dfbw * 4; break;
        case GS_PSMCT24: stride = dfbw * 4; break;
        case GS_PSMCT16:
        case GS_PSMCT16S:
            stride = dfbw * 2;
        break;
    }

    if (!ctx->texture) {
        SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
        SDL_RenderClear(ctx->renderer);
    } else {
        SDL_UpdateTexture(ctx->texture, NULL, ptr, stride);
        SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);
    }

    SDL_RenderPresent(ctx->renderer);
}

extern "C" void software_transfer_start(struct ps2_gs* gs, void* udata) {
    software_state* ctx = (software_state*)udata;

    ctx->dbp = (gs->bitbltbuf >> 32) & 0x3fff;
    ctx->dbw = (gs->bitbltbuf >> 48) & 0x3f;
    ctx->dpsm = (gs->bitbltbuf >> 56) & 0x3f;
    ctx->sbp = (gs->bitbltbuf >> 0) & 0x3fff;
    ctx->sbw = (gs->bitbltbuf >> 16) & 0x3f;
    ctx->spsm = (gs->bitbltbuf >> 24) & 0x3f;
    ctx->dsax = (gs->trxpos >> 32) & 0x7ff;
    ctx->dsay = (gs->trxpos >> 48) & 0x7ff;
    ctx->ssax = (gs->trxpos >> 0) & 0x7ff;
    ctx->ssay = (gs->trxpos >> 16) & 0x7ff;
    ctx->dir = (gs->trxpos >> 59) & 3;
    ctx->rrw = (gs->trxreg >> 0) & 0xfff;
    ctx->rrh = (gs->trxreg >> 32) & 0xfff;
    ctx->xdir = gs->trxdir & 3;
    ctx->dx = 0;
    ctx->dy = 0;
    ctx->sx = 0;
    ctx->sy = 0;

    ctx->dbp <<= 6;
    ctx->dbw <<= 6;
    ctx->sbp <<= 6;
    ctx->sbw <<= 6;

    if (ctx->xdir == 2) {
        software_vram_blit(gs, ctx);
    }
}

extern "C" void software_transfer_write(struct ps2_gs* gs, void* udata) {
    software_state* ctx = (software_state*)udata;

    uint32_t addr = ctx->dbp + ctx->dsax + (ctx->dsay * ctx->dbw);

    addr += ctx->dx + (ctx->dy * ctx->dbw);

    gs->vram[addr + 0] = gs->hwreg & 0xffffffff;
    gs->vram[addr + 1] = gs->hwreg >> 32;

    ctx->dx += 2;

    if (ctx->dx >= ctx->rrw) {
        ctx->dx = 0;

        ++ctx->dy;
    }
}

extern "C" void software_transfer_read(struct ps2_gs* gs, void* udata) {
    gs->hwreg = 0;
}
