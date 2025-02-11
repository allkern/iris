#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vif.h"

struct ps2_vif* ps2_vif_create(void) {
    return malloc(sizeof(struct ps2_vif));
}

void ps2_vif_init(struct ps2_vif* vif, struct ee_bus* bus) {
    memset(vif, 0, sizeof(struct ps2_vif));

    vif->bus = bus;
}

void ps2_vif_destroy(struct ps2_vif* vif) {
    free(vif);
}

void vif1_handle_fifo_write(struct ps2_vif* vif, uint32_t data) {
    if (vif->vif1_state == VIF_IDLE) {
        vif->vif1_cmd = (data >> 24) & 0x7f;

        switch ((data >> 24) & 0x7f) {
            case VIF_CMD_NOP: {
                // printf("vif: NOP\n");
            } break;
            case VIF_CMD_STCYCL: {
                // printf("vif: STCYCL(%04x)\n", data & 0xffff);

                vif->vif1_cycle = data & 0xffff;
            } break;
            case VIF_CMD_OFFSET: {
                // printf("vif: OFFSET(%04x)\n", data & 0xffff);

                vif->vif1_ofst = data & 0xffff;
            } break;
            case VIF_CMD_BASE: {
                // printf("vif: BASE(%04x)\n", data & 0xffff);

                vif->vif1_base = data & 0xffff;
            } break;
            case VIF_CMD_STMOD: {
                // printf("vif: STMOD(%04x)\n", data & 0xffff);

                vif->vif1_mode = data & 3;
            } break;
            case VIF_CMD_MSKPATH3: {
                // printf("vif: MSKPATH3(%04x)\n", data & 0xffff);
            } break;
            case VIF_CMD_MARK: {
                // printf("vif: MARK(%04x)\n", data & 0xffff);

                vif->vif1_mark = data & 0xffff;
            } break;
            case VIF_CMD_FLUSH: {
                // printf("vif: FLUSH\n");
            } break;
            case VIF_CMD_MSCAL: {
                // printf("vif: MSCAL(%04x)\n", data & 0xffff);
            } break;
            case VIF_CMD_MPG: {
                // printf("vif: MPG(%04x, %04x)\n", (data >> 16) & 0xff, data & 0xffff);

                vif->vif1_state = VIF_RECV_DATA;
                vif->vif1_pending_words = (((data >> 16) & 0xff) * 8) / 4;

                // printf("pending words: %08x (%d)\n",
                //     vif->vif1_pending_words,
                //     vif->vif1_pending_words
                // );
            } break;
            case VIF_CMD_DIRECT: {
                // printf("vif: DIRECT(%04x)\n", data & 0xffff);

                vif->vif1_state = VIF_RECV_DATA;
                vif->vif1_pending_words = (data & 0xffff) * 4;
            } break;
            default: {
                // printf("vif: Unhandled command %02x\n", vif->vif1_cmd);
            } break;
        }
    } else {
        switch (vif->vif1_cmd) {
            case VIF_CMD_MPG: {
                // printf("vif: Writing %08x to VU mem\n", data);

                if (!(--vif->vif1_pending_words)) {
                    vif->vif1_state = VIF_IDLE;
                }
            } break;
            case VIF_CMD_DIRECT: {
                vif->vif1_data.u32[vif->vif1_shift++] = data;

                if (vif->vif1_shift == 4) {
                    ee_bus_write128(vif->bus, 0x10006000, vif->vif1_data);

                    vif->vif1_shift = 0;
                }

                if (!(--vif->vif1_pending_words)) {
                    vif->vif1_state = VIF_IDLE;
                }
            } break;
        }
    }
}

uint64_t ps2_vif_read32(struct ps2_vif* vif, uint32_t addr) {
    switch (addr) {
        case 0x10003800: return vif->vif0_stat;
        case 0x10003810: return vif->vif0_fbrst;
        case 0x10003820: return vif->vif0_err;
        case 0x10003830: return vif->vif0_mark;
        case 0x10003840: return vif->vif0_cycle;
        case 0x10003850: return vif->vif0_mode;
        case 0x10003860: return vif->vif0_num;
        case 0x10003870: return vif->vif0_mask;
        case 0x10003880: return vif->vif0_code;
        case 0x10003890: return vif->vif0_itops;
        case 0x100038d0: return vif->vif0_itop;
        case 0x10003900: return vif->vif0_r[0];
        case 0x10003910: return vif->vif0_r[1];
        case 0x10003920: return vif->vif0_r[2];
        case 0x10003930: return vif->vif0_r[3];
        case 0x10003940: return vif->vif0_c[0];
        case 0x10003950: return vif->vif0_c[1];
        case 0x10003960: return vif->vif0_c[2];
        case 0x10003970: return vif->vif0_c[3];
        case 0x10003c00: return vif->vif1_stat;
        case 0x10003c10: return vif->vif1_fbrst;
        case 0x10003c20: return vif->vif1_err;
        case 0x10003c30: return vif->vif1_mark;
        case 0x10003c40: return vif->vif1_cycle;
        case 0x10003c50: return vif->vif1_mode;
        case 0x10003c60: return vif->vif1_num;
        case 0x10003c70: return vif->vif1_mask;
        case 0x10003c80: return vif->vif1_code;
        case 0x10003c90: return vif->vif1_itops;
        case 0x10003ca0: return vif->vif1_base;
        case 0x10003cb0: return vif->vif1_ofst;
        case 0x10003cc0: return vif->vif1_tops;
        case 0x10003cd0: return vif->vif1_itop;
        case 0x10003ce0: return vif->vif1_top;
        case 0x10003d00: return vif->vif1_r[0];
        case 0x10003d10: return vif->vif1_r[1];
        case 0x10003d20: return vif->vif1_r[2];
        case 0x10003d30: return vif->vif1_r[3];
        case 0x10003d40: return vif->vif1_c[0];
        case 0x10003d50: return vif->vif1_c[1];
        case 0x10003d60: return vif->vif1_c[2];
        case 0x10003d70: return vif->vif1_c[3];
        case 0x10004000: printf("vif0: FIFO read\n"); break;
        case 0x10005000: printf("vif1: FIFO read\n"); break;
    }

    return 0;
}

void ps2_vif_write32(struct ps2_vif* vif, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x10003800: vif->vif0_stat = data; break;
        case 0x10003810: vif->vif0_fbrst = data; break;
        case 0x10003820: vif->vif0_err = data; break;
        case 0x10003830: vif->vif0_mark = data; break;
        case 0x10003840: vif->vif0_cycle = data; break;
        case 0x10003850: vif->vif0_mode = data; break;
        case 0x10003860: vif->vif0_num = data; break;
        case 0x10003870: vif->vif0_mask = data; break;
        case 0x10003880: vif->vif0_code = data; break;
        case 0x10003890: vif->vif0_itops = data; break;
        case 0x100038d0: vif->vif0_itop = data; break;
        case 0x10003900: vif->vif0_r[0] = data; break;
        case 0x10003910: vif->vif0_r[1] = data; break;
        case 0x10003920: vif->vif0_r[2] = data; break;
        case 0x10003930: vif->vif0_r[3] = data; break;
        case 0x10003940: vif->vif0_c[0] = data; break;
        case 0x10003950: vif->vif0_c[1] = data; break;
        case 0x10003960: vif->vif0_c[2] = data; break;
        case 0x10003970: vif->vif0_c[3] = data; break;
        case 0x10003c00: vif->vif1_stat = data; break;
        case 0x10003c10: vif->vif1_fbrst = data; break;
        case 0x10003c20: vif->vif1_err = data; break;
        case 0x10003c30: vif->vif1_mark = data; break;
        case 0x10003c40: vif->vif1_cycle = data; break;
        case 0x10003c50: vif->vif1_mode = data; break;
        case 0x10003c60: vif->vif1_num = data; break;
        case 0x10003c70: vif->vif1_mask = data; break;
        case 0x10003c80: vif->vif1_code = data; break;
        case 0x10003c90: vif->vif1_itops = data; break;
        case 0x10003ca0: vif->vif1_base = data; break;
        case 0x10003cb0: vif->vif1_ofst = data; break;
        case 0x10003cc0: vif->vif1_tops = data; break;
        case 0x10003cd0: vif->vif1_itop = data; break;
        case 0x10003ce0: vif->vif1_top = data; break;
        case 0x10003d00: vif->vif1_r[0] = data; break;
        case 0x10003d10: vif->vif1_r[1] = data; break;
        case 0x10003d20: vif->vif1_r[2] = data; break;
        case 0x10003d30: vif->vif1_r[3] = data; break;
        case 0x10003d40: vif->vif1_c[0] = data; break;
        case 0x10003d50: vif->vif1_c[1] = data; break;
        case 0x10003d60: vif->vif1_c[2] = data; break;
        case 0x10003d70: vif->vif1_c[3] = data; break;
        case 0x10004000: printf("vif0: FIFO write\n"); break;
        case 0x10005000: vif1_handle_fifo_write(vif, data); break;
    }
}

uint128_t ps2_vif_read128(struct ps2_vif* vif, uint32_t addr) {
    switch (addr) {
        case 0x10004000: printf("vif0: 128-bit FIFO read\n"); break;
        case 0x10005000: printf("vif1: 128-bit FIFO read\n"); break;
    }

    return (uint128_t){ .u64[0] = 0, .u64[1] = 0 };;
}

void ps2_vif_write128(struct ps2_vif* vif, uint32_t addr, uint128_t data) {
    switch (addr) {
        case 0x10004000: printf("vif0: 128-bit FIFO write\n"); break;
        case 0x10005000: {
            vif1_handle_fifo_write(vif, data.u32[0]);
            vif1_handle_fifo_write(vif, data.u32[1]);
            vif1_handle_fifo_write(vif, data.u32[2]);
            vif1_handle_fifo_write(vif, data.u32[3]);
        } break;
    }
}