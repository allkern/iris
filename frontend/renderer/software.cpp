#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "ee/gs.h"
#include "software.hpp"

static inline void software_vram_blit(struct ps2_gs* gs, software_state* ctx) {
    for (int y = 0; y < ctx->rrh; y++) {
        uint32_t src = ctx->sbp + ctx->ssax + (ctx->ssay * ctx->sbw) + (y * ctx->sbw);
        uint32_t dst = ctx->dbp + ctx->dsax + (ctx->dsay * ctx->dbw) + (y * ctx->dbw);

        memcpy(gs->vram + dst, gs->vram + src, ctx->rrw * sizeof(uint32_t));
    }
}

extern "C" void software_render_point(struct ps2_gs* gs, void* udata) {
    software_state* ctx = (software_state*)udata;

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

    int wx = x - xoff;
    int wy = y - yoff;

    // Scissor test
    if (wx < scax0 || wy < scay0 || wx > scax1 || wy > scay1) {
        return;
    }

    // Alpha test
    int pass = 0;

    int ate = gs->ctx->test & 1;

    if (ate) {
        pass = 1;
    } else {
        int atst = (gs->ctx->test >> 1) & 7;
        int aref = (gs->ctx->test >> 4) & 0xff;
        int a = (v.rgbaq >> 24) & 0xff;

        switch (atst) {
            case 0: pass = 0; break;
            case 1: pass = 1; break;
            case 2: pass = a < aref; break;
            case 3: pass = a <= aref; break;
            case 4: pass = a == aref; break;
            case 5: pass = a >= aref; break;
            case 6: pass = a > aref; break;
            case 7: pass = a != aref; break;
        }
    }

    int update = 0;

    if (!pass) {
        int afail = (gs->ctx->test >> 12) & 3;

        if (!afail)
            return;

        update = afail;
    }

    uint32_t off = (x + (y * fbw));

    // To-do: Destination Alpha Test

    // Depth test
    int zte = (gs->ctx->test >> 16) & 1;

    if (zte) {
        // uint32_t zaddr = gs->z
        int ztst = (gs->ctx->test >> 17) & 3;

        switch (ztst) {
            case 0: pass = 0; break;
            case 1: pass = 1; break;
            // case 2: pass = 
        }
    }

    
}

extern "C" void software_render_line(struct ps2_gs* gs, void* udata) {

}

extern "C" void software_render_triangle(struct ps2_gs* gs, void* udata) {

}

extern "C" void software_render_sprite(struct ps2_gs* gs, void* udata) {

}

extern "C" void software_render(struct ps2_gs* gs, void* udata) {

}

extern "C" void transfer_start(struct ps2_gs* gs, void* udata) {
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

extern "C" void transfer_write(struct ps2_gs* gs, void* udata) {
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

extern "C" void transfer_read(struct ps2_gs* gs, void* udata) {
    gs->hwreg = 0;
}
