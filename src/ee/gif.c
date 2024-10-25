#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gif.h"

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
}
struct ps2_gif* ps2_gif_create(void) {
    return malloc(sizeof(struct ps2_gif));
}

void ps2_gif_init(struct ps2_gif* gif) {
    memset(gif, 0, sizeof(struct ps2_gif));
}

void ps2_gif_destroy(struct ps2_gif* gif) {
    free(gif);
}

uint64_t ps2_gif_read32(struct ps2_gif* gif, uint32_t addr) {
    switch (addr) {
        case 0x10003020: return gif->stat;
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

void ps2_gif_write128(struct ps2_gif* gif, uint32_t addr, uint128_t data) {
    if (gif->state == GIF_STATE_RECV_TAG) {
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

        printf("giftag: nloop=%04lx eop=%d prim=%d fmt=%d nregs=%d reg=%016lx\n",
            gif->tag.nloop, gif->tag.eop, gif->tag.prim, gif->tag.fmt, gif->tag.nregs, gif->tag.reg
        );
    
        if (gif->tag.pre) {
            printf("GS PRIM <- %08x\n", gif->tag.prim);
        }

        if (gif->tag.remaining) {
            gif->state = GIF_STATE_PROCESSING;
        }

        return;
    }

    if (gif->tag.index != gif->tag.remaining) {
        if (gif->tag.fmt == 2) {
            // printf("reg=HWREG data=%016lx\n", data.u64[0]);
            // printf("reg=HWREG data=%016lx\n", data.u64[1]);

            ++gif->tag.index;

            if (gif->tag.index == gif->tag.remaining) {
                gif->state = GIF_STATE_RECV_TAG;
            }

            return;
        }

        int index = (gif->tag.index++) % gif->tag.nregs;
        int r = (gif->tag.reg >> (index * 4)) & 0xf;

        switch (r) {
            case 0x00: printf("reg=PRIM data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x01: printf("reg=RGBAQ data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x02: printf("reg=ST data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x03: printf("reg=UV data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x04: printf("reg=XYZF2 data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x05: printf("reg=XYZ2 data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x06: printf("reg=TEX0_1 data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x07: printf("reg=TEX0_2 data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x08: printf("reg=CLAMP_1 data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x09: printf("reg=CLAMP_2 data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x0a: printf("reg=FOG data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x0c: printf("reg=XYZF3 data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x0d: printf("reg=XYZ3 data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
            case 0x0e: printf("reg=%s data=%016lx\n", gif_get_reg_name(data.u64[1]), data.u64[0]); break;
            case 0x0f: printf("reg=NOP data=%016lx%016lx\n", data.u64[1], data.u64[0]); break;
        }

        if (gif->tag.index == gif->tag.remaining)
            gif->state = GIF_STATE_RECV_TAG;
    }

    /*
        GIFtag Format

        0-14    NLOOP - Data per register to transfer
        15      EOP - End of packet
        16-45   Unused
        46      Enable PRIM field
        47-57   Data to be sent to GS PRIM register if GIFtag.46 == 1
        58-59   Data format
                0=PACKED
                1=REGLIST
                2=IMAGE
                3=IMAGE
        60-63   NREGS - Number of registers
                0=16 registers
        64-127  Register field, 4 bits each
    */
}