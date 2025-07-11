#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gif.h"

// Burnout games need the FQC field on GIF_STAT to change on
// GIF DMA transfers, otherwise they'll hang on the initial
// loading screen.

#define printf(fmt, ...)(0)

static inline const char* gif_get_reg_name(uint8_t r) {
    switch (r) {
        case 0x00: return "PRIM";
        case 0x01: return "RGBAQ";
        case 0x02: return "ST";
        case 0x03: return "UV";
        case 0x04: return "XYZF2";
        case 0x05: return "XYZ2";
        case 0x06: return "TEX0_1";
        case 0x07: return "TEX0_2";
        case 0x08: return "CLAMP_1";
        case 0x09: return "CLAMP_2";
        case 0x0A: return "FOG";
        case 0x0C: return "XYZF3";
        case 0x0D: return "XYZ3";
        case 0x14: return "TEX1_1";
        case 0x15: return "TEX1_2";
        case 0x16: return "TEX2_1";
        case 0x17: return "TEX2_2";
        case 0x18: return "XYOFFSET_1";
        case 0x19: return "XYOFFSET_2";
        case 0x1A: return "PRMODECONT";
        case 0x1B: return "PRMODE";
        case 0x1C: return "TEXCLUT";
        case 0x22: return "SCANMSK";
        case 0x34: return "MIPTBP1_1";
        case 0x35: return "MIPTBP1_2";
        case 0x36: return "MIPTBP2_1";
        case 0x37: return "MIPTBP2_2";
        case 0x3B: return "TEXA";
        case 0x3D: return "FOGCOL";
        case 0x3F: return "TEXFLUSH";
        case 0x40: return "SCISSOR_1";
        case 0x41: return "SCISSOR_2";
        case 0x42: return "ALPHA_1";
        case 0x43: return "ALPHA_2";
        case 0x44: return "DIMX";
        case 0x45: return "DTHE";
        case 0x46: return "COLCLAMP";
        case 0x47: return "TEST_1";
        case 0x48: return "TEST_2";
        case 0x49: return "PABE";
        case 0x4A: return "FBA_1";
        case 0x4B: return "FBA_2";
        case 0x4C: return "FRAME_1";
        case 0x4D: return "FRAME_2";
        case 0x4E: return "ZBUF_1";
        case 0x4F: return "ZBUF_2";
        case 0x50: return "BITBLTBUF";
        case 0x51: return "TRXPOS";
        case 0x52: return "TRXREG";
        case 0x53: return "TRXDIR";
        case 0x54: return "HWREG";
        case 0x60: return "SIGNAL";
        case 0x61: return "FINISH";
        case 0x62: return "LABEL";
    }

    return "<unknown>";
}
struct ps2_gif* ps2_gif_create(void) {
    return malloc(sizeof(struct ps2_gif));
}

void ps2_gif_init(struct ps2_gif* gif, struct vu_state* vu1, struct ps2_gs* gs) {
    memset(gif, 0, sizeof(struct ps2_gif));

    gif->gs = gs;
    gif->vu1 = vu1;
}

void ps2_gif_destroy(struct ps2_gif* gif) {
    free(gif);
}

uint64_t ps2_gif_read32(struct ps2_gif* gif, uint32_t addr) {
    switch (addr) {
        case 0x10003020: {
            // Clear FQC when reading GIF_STAT
            uint32_t v = gif->stat;

            gif->stat &= ~0x1f000000;

            return v;
        } break;
        case 0x10003040: return gif->tag0;
        case 0x10003050: return gif->tag1;
        case 0x10003060: return gif->tag2;
        case 0x10003070: return gif->tag3;
        case 0x10003080: return gif->cnt;
        case 0x10003090: return gif->p3cnt;
        case 0x100030A0: return gif->p3tag;
    }

    return 0;
}

void ps2_gif_write32(struct ps2_gif* gif, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x10003000: gif->ctrl = data; return;
        case 0x10003010: gif->mode = data; return;
    }
}

void gif_write_rgbaq(struct ps2_gif* gif, uint128_t data) {
    uint64_t r = data.u64[0] & 0xff;
    uint64_t g = (data.u64[0] >> 32) & 0xff;
    uint64_t b = data.u64[1] & 0xff;
    uint64_t a = (data.u64[1] >> 32) & 0xff;
    uint64_t v = r | (g << 8) | (b << 16) | (a << 24) | (gif->q << 32);

    ps2_gs_write_internal(gif->gs, GS_RGBAQ, v);
}

void gif_write_stq(struct ps2_gif* gif, uint128_t data) {
    gif->q = data.u64[1] & 0xffffffff;

    ps2_gs_write_internal(gif->gs, GS_ST, data.u64[0]);
}

void gif_write_uv(struct ps2_gif* gif, uint128_t data) {
    ps2_gs_write_internal(gif->gs, GS_UV, (data.u64[0] & 0x3fff) | (data.u64[0] >> 16));
}

void gif_write_xyzf23(struct ps2_gif* gif, uint128_t data) {
    uint64_t x = data.u64[0] & 0xffff;
    uint64_t y = (data.u64[0] >> 32) & 0xffff;
    uint64_t z = (data.u64[1] >> 4) & 0xffffff;
    uint64_t f = (data.u64[1] >> 36) & 0xff;
    uint64_t v = x | (y << 16) | (z << 32) | (f << 56);
    uint64_t adc = data.u64[1] & 0x800000000000ul;

    ps2_gs_write_internal(gif->gs, adc ? GS_XYZF3 : GS_XYZF2, v);
}

void gif_write_xyz23(struct ps2_gif* gif, uint128_t data) {
    uint64_t x = data.u64[0] & 0xffff;
    uint64_t y = (data.u64[0] >> 32) & 0xffff;
    uint64_t z = data.u64[1] & 0xffffffff;
    uint64_t v = x | (y << 16) | (z << 32);
    uint64_t adc = data.u64[1] & 0x800000000000ul;

    ps2_gs_write_internal(gif->gs, adc ? GS_XYZ3 : GS_XYZ2, v);
}

void gif_write_fog(struct ps2_gif* gif, uint128_t data) {
    ps2_gs_write_internal(gif->gs, GS_FOG, data.u64[1] << 20);
}

void gif_handle_tag(struct ps2_gif* gif, uint128_t data) {
    // 1.0f
    gif->q = 0x3f800000;

    gif->tag.nloop = data.u64[0] & 0x7fff;
    gif->tag.prim = (data.u64[0] >> 47) & 0x3ff;
    gif->tag.eop = !!(data.u64[0] & 0x8000);
    gif->tag.pre = !!(data.u64[0] & 0x400000000000ull);
    gif->tag.fmt = (data.u64[0] >> 58) & 3;
    gif->tag.nregs = (data.u64[0] >> 60) & 0xf;
    gif->tag.reg = data.u64[1];
    gif->tag.index = 0;

    switch (gif->tag.fmt) {
        case 0:
        case 1: {
            gif->tag.remaining = gif->tag.nregs * gif->tag.nloop;
        } break;
        case 2:
        case 3: {
            gif->tag.remaining = gif->tag.nloop;
        } break;
    }

    // printf("giftag: nloop=%04lx eop=%d prim=%d fmt=%d nregs=%d reg=%016lx\n",
    //     gif->tag.nloop, gif->tag.eop, gif->tag.prim, gif->tag.fmt, gif->tag.nregs, gif->tag.reg
    // );

    if (gif->tag.pre) {
        ps2_gs_write_internal(gif->gs, GS_PRIM, gif->tag.prim);
    }

    if (gif->tag.remaining) {
        gif->state = GIF_STATE_PROCESSING;
    }
}

void gif_handle_packed(struct ps2_gif* gif, uint128_t data) {
    int index = (gif->tag.index++) % gif->tag.nregs;
    int r = (gif->tag.reg >> (index * 4)) & 0xf;

    switch (r) {
        case 0x00: printf("gif: PRIM <- %016lx\n", data.u64[0]); ps2_gs_write_internal(gif->gs, GS_PRIM, data.u64[0] & 0x3ff); break;
        case 0x01: printf("gif: RGBAQ <- %016lx\n", data.u64[0]); gif_write_rgbaq(gif, data); break;
        case 0x02: printf("gif: STQ <- %016lx\n", data.u64[0]); gif_write_stq(gif, data); break;
        case 0x03: printf("gif: UV <- %016lx\n", data.u64[0]); gif_write_uv(gif, data); break;
        case 0x04: printf("gif: XYZF23 <- %016lx\n", data.u64[0]); gif_write_xyzf23(gif, data); break;
        case 0x05: printf("gif: XYZ23 <- %016lx\n", data.u64[0]); gif_write_xyz23(gif, data); break;
        case 0x06: printf("gif: TEX0_1 <- %016lx\n", data.u64[0]); ps2_gs_write_internal(gif->gs, GS_TEX0_1, data.u64[0]); break;
        case 0x07: printf("gif: TEX0_2 <- %016lx\n", data.u64[0]); ps2_gs_write_internal(gif->gs, GS_TEX0_2, data.u64[0]); break;
        case 0x08: printf("gif: CLAMP_1 <- %016lx\n", data.u64[0]); ps2_gs_write_internal(gif->gs, GS_CLAMP_1, data.u64[0]); break;
        case 0x09: printf("gif: CLAMP_2 <- %016lx\n", data.u64[0]); ps2_gs_write_internal(gif->gs, GS_CLAMP_2, data.u64[0]); break;
        case 0x0a: printf("gif: FOG <- %016lx\n", data.u64[0]); gif_write_fog(gif, data); break;
        case 0x0c: printf("gif: XYZF3 <- %016lx\n", data.u64[0]); ps2_gs_write_internal(gif->gs, GS_XYZF3, data.u64[0]); break;
        case 0x0d: printf("gif: XYZ3 <- %016lx\n", data.u64[0]); ps2_gs_write_internal(gif->gs, GS_XYZ3, data.u64[0]); break;

        // A+D
        case 0x0e: {
            printf("gif: write %s (A+D)\n", gif_get_reg_name(data.u64[1]));
            ps2_gs_write_internal(gif->gs, data.u64[1], data.u64[0]); 
        } break;

        // NOP
        case 0x0f: break;

        default: printf("gif: PACKED format for reg %d unimplemented\n", r); exit(1); break;
    }

    // Note: This handles odd NREGS*NLOOP case
    if (gif->tag.index == gif->tag.remaining) {
        gif->state = GIF_STATE_RECV_TAG;

        return;
    }
}

void gif_handle_reglist(struct ps2_gif* gif, uint128_t data) {
    for (int i = 0; i < 2; i++) {
        int index = (gif->tag.index++) % gif->tag.nregs;
        int r = (gif->tag.reg >> (index * 4)) & 0xf;

        switch (r) {
            case 0x00: ps2_gs_write_internal(gif->gs, GS_PRIM, data.u64[i]); break;
            case 0x01: ps2_gs_write_internal(gif->gs, GS_RGBAQ, data.u64[i]); break;
            case 0x02: ps2_gs_write_internal(gif->gs, GS_ST, data.u64[i]); break;
            case 0x03: ps2_gs_write_internal(gif->gs, GS_UV, data.u64[i]); break;
            case 0x04: ps2_gs_write_internal(gif->gs, GS_XYZF2, data.u64[i]); break;
            case 0x05: ps2_gs_write_internal(gif->gs, GS_XYZ2, data.u64[i]); break;
            case 0x06: ps2_gs_write_internal(gif->gs, GS_TEX0_1, data.u64[i]); break;
            case 0x07: ps2_gs_write_internal(gif->gs, GS_TEX0_2, data.u64[i]); break;
            case 0x08: ps2_gs_write_internal(gif->gs, GS_CLAMP_1, data.u64[i]); break;
            case 0x09: ps2_gs_write_internal(gif->gs, GS_CLAMP_2, data.u64[i]); break;
            case 0x0a: ps2_gs_write_internal(gif->gs, GS_FOG, data.u64[i]); break;
            case 0x0c: ps2_gs_write_internal(gif->gs, GS_XYZF3, data.u64[i]); break;
            case 0x0d: ps2_gs_write_internal(gif->gs, GS_XYZ3, data.u64[i]); break;

            // A+D
            // NOP
            case 0x0e:
            case 0x0f: break;

            // default: printf("gif: REGLIST format for reg %d unimplemented\n", r); break;
        }

        // Note: This handles odd NREGS*NLOOP case
        if (gif->tag.index == gif->tag.remaining) {
            gif->state = GIF_STATE_RECV_TAG;

            return;
        }
    }

    if (gif->tag.index == gif->tag.remaining) {
        gif->state = GIF_STATE_RECV_TAG;

        return;
    }
}

void gif_handle_image(struct ps2_gif* gif, uint128_t data) {
    ps2_gs_write_internal(gif->gs, GS_HWREG, data.u64[0]);
    ps2_gs_write_internal(gif->gs, GS_HWREG, data.u64[1]);
    
    ++gif->tag.index;

    if (gif->tag.index == gif->tag.remaining) {
        gif->state = GIF_STATE_RECV_TAG;
    }

    return;
}

void ps2_gif_write128(struct ps2_gif* gif, uint32_t addr, uint128_t data) {
    // Set FQC when getting GIF FIFO writes
    gif->stat |= 0x1f000000;

    if (gif->state == GIF_STATE_RECV_TAG) {
        gif_handle_tag(gif, data);

        return;
    }

    if (gif->tag.index != gif->tag.remaining) {
        switch (gif->tag.fmt) {
            case 0: gif_handle_packed(gif, data); return;
            case 1: gif_handle_reglist(gif, data); return;
            case 2: gif_handle_image(gif, data); return;
            case 3: gif_handle_image(gif, data); return;
        }
    }
}

#undef printf