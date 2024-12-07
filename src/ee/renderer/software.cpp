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

    ctx->texture = SDL_CreateTexture(
        ctx->renderer,
        format,
        SDL_TEXTUREACCESS_STREAMING,
        (ctx->gs->ctx->scax1 - ctx->gs->ctx->scax0) + 1,
        (ctx->gs->ctx->scay1 - ctx->gs->ctx->scay0) + 1
    );

    SDL_SetTextureScaleMode(ctx->texture, SDL_ScaleModeLinear);

    // printf("software: width=%d height=%d format=%d\n",
    //     (ctx->gs->ctx->scax1 - ctx->gs->ctx->scax0) + 1,
    //     (ctx->gs->ctx->scay1 - ctx->gs->ctx->scay0) + 1,
    //     format
    // );
}

#define CLAMP(v, l, u) (((v) > (u)) ? (u) : (((v) < (l)) ? (l) : (v)))

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

extern "C" void software_render_triangle(struct ps2_gs* gs, void* udata) {
    struct gs_vertex v = gs->vq[0];

    // All of these only change on writes to their respective
    // registers, so we should definitely cache the fields

    // Base FB
    uint32_t fbp = (gs->ctx->frame & 0x1ff) << 11;
    uint32_t fbw = ((gs->ctx->frame >> 16) & 0x3f) << 6;

    // Window
    uint32_t xoff = (gs->ctx->xyoffset & 0xffff) >> 4;
    uint32_t yoff = ((gs->ctx->xyoffset >> 32) & 0xffff) >> 4;
    int scax0 = gs->ctx->scissor & 0x3ff;
    int scay0 = (gs->ctx->scissor >> 32) & 0x3ff;
    int scax1 = (gs->ctx->scissor >> 16) & 0x3ff;
    int scay1 = (gs->ctx->scissor >> 48) & 0x3ff;

    uint32_t x = (v.xyz & 0xffff) >> 4;
    uint32_t y = (v.xyz & 0xffff0000) >> 20;

    // printf("fbp=%08x fbw=%d xyoffset=(%d,%d) scissor=(%d,%d-%d,%d), xy=(%d,%d)\n",
    //     fbp, fbw,
    //     xoff, yoff,
    //     scax0, scay0,
    //     scax1, scay1,
    //     x, y
    // );

    // exit(1);
}

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

            if (gs->ctx->tcc) {
                pa = CLAMP((ta * fa) >> 7, 0, 255);
            } else {
                pa = fa;
            }
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

    pr &= 0xff;
    pg &= 0xff;
    pb &= 0xff;
    pa &= 0xff;

    uint32_t p = pr | (pg << 8) | (pb << 16) | (pa << 24);

    return p;
}

int loop = 0;

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
