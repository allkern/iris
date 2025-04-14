#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "vif.h"

struct ps2_vif* ps2_vif_create(void) {
    return malloc(sizeof(struct ps2_vif));
}

void ps2_vif_init(struct ps2_vif* vif, struct ps2_intc* intc, struct sched_state* sched, struct ee_bus* bus) {
    memset(vif, 0, sizeof(struct ps2_vif));

    vif->sched = sched;
    vif->intc = intc;
    vif->bus = bus;
}

void ps2_vif_destroy(struct ps2_vif* vif) {
    free(vif);
}

void vif1_send_irq(void* udata, int overshoot) {
    struct ps2_vif* vif = (struct ps2_vif*)udata;

    ps2_intc_irq(vif->intc, EE_INTC_VIF1);
}

void vif1_handle_fifo_write(struct ps2_vif* vif, uint32_t data) {
    if (vif->vif1_state == VIF_IDLE) {
        vif->vif1_cmd = (data >> 24) & 0xff;
        
        // if (vif->vif1_cmd & 0x80) {
        //     struct sched_event event;

        //     event.callback = vif1_send_irq;
        //     event.cycles = 1000;
        //     event.name = "VIF1 Interrupt";
        //     event.udata = vif;

        //     printf("vif1: Requested IRQ\n");

        //     exit(1);
        // }

        switch ((data >> 24) & 0x7f) {
            case VIF_CMD_NOP: {
                // printf("vif1: NOP\n");
            } break;
            case VIF_CMD_STCYCL: {
                // printf("vif1: STCYCL(%04x)\n", data & 0xffff);

                vif->vif1_cycle = data & 0xffff;
            } break;
            case VIF_CMD_OFFSET: {
                // printf("vif1: OFFSET(%04x)\n", data & 0xffff);

                vif->vif1_ofst = data & 0xffff;
            } break;
            case VIF_CMD_BASE: {
                // printf("vif1: BASE(%04x)\n", data & 0xffff);

                vif->vif1_base = data & 0x3f;
            } break;
            case VIF_CMD_ITOP: {
                // printf("vif1: ITOP(%04x)\n", data & 0xffff);

                vif->vif1_itop = data & 0x3f;
            } break;
            case VIF_CMD_STMOD: {
                // printf("vif1: STMOD(%04x)\n", data & 0xffff);

                vif->vif1_mode = data & 3;
            } break;
            case VIF_CMD_MSKPATH3: {
                // printf("vif1: MSKPATH3(%04x)\n", data & 0xffff);
            } break;
            case VIF_CMD_MARK: {
                // printf("vif1: MARK(%04x)\n", data & 0xffff);

                vif->vif1_mark = data & 0xffff;
            } break;
            case VIF_CMD_FLUSHE: {
                // printf("vif1: FLUSHE\n");
            } break;
            case VIF_CMD_FLUSH: {
                // printf("vif1: FLUSH\n");
            } break;
            case VIF_CMD_FLUSHA: {
                // printf("vif1: FLUSHA\n");
            } break;
            case VIF_CMD_MSCAL: {
                // printf("vif1: MSCAL(%04x)\n", data & 0xffff);
            } break;
            case VIF_CMD_MSCALF: {
                // printf("vif1: MSCALF(%04x)\n", data & 0xffff);
            } break;
            case VIF_CMD_MSCNT: {
                // printf("vif1: MSCNT\n");
            } break;
            case VIF_CMD_STMASK: {
                // printf("vif1: STMASK(%04x)\n", data & 0xffff);

                vif->vif1_state = VIF_RECV_DATA;
                vif->vif1_pending_words = 1;
            } break;
            case VIF_CMD_STROW: {
                // printf("vif1: STROW(%04x)\n", data & 0xffff);

                vif->vif1_state = VIF_RECV_DATA;
                vif->vif1_pending_words = 4;
            } break;
            case VIF_CMD_STCOL: {
                // printf("vif1: STCOL(%04x)\n", data & 0xffff);

                vif->vif1_state = VIF_RECV_DATA;
                vif->vif1_pending_words = 4;
            } break;
            case VIF_CMD_MPG: {
                // printf("vif1: MPG(%04x, %04x)\n", (data >> 16) & 0xff, data & 0xffff);

                int num = (data >> 16) & 0xff;

                if (!num) num = 256;

                vif->vif1_state = VIF_RECV_DATA;
                vif->vif1_pending_words = (num * 8) / 4;
            } break;
            case VIF_CMD_DIRECT: {
                // printf("vif1: DIRECT(%04x)\n", data & 0xffff);

                vif->vif1_state = VIF_RECV_DATA;
                vif->vif1_pending_words = (data & 0xffff) * 4;
                vif->vif1_shift = 0;
            } break;
            case VIF_CMD_DIRECTHL: {
                // printf("vif1: DIRECTHL(%04x)\n", data & 0xffff);

                vif->vif1_state = VIF_RECV_DATA;
                vif->vif1_pending_words = (data & 0xffff) * 4;
                vif->vif1_shift = 0;
            } break;

            // UNPACK commands
            case 0x60: case 0x61: case 0x62: case 0x63:
            case 0x64: case 0x65: case 0x66: case 0x67:
            case 0x68: case 0x69: case 0x6a: case 0x6b:
            case 0x6c: case 0x6d: case 0x6e: case 0x6f:
            case 0x70: case 0x71: case 0x72: case 0x73:
            case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7a: case 0x7b:
            case 0x7c: case 0x7d: case 0x7e: case 0x7f: {
                int fmt = (data >> 24) & 0xf;
                int cl = vif->vif1_cycle & 0xff;
                int wl = (vif->vif1_cycle >> 8) & 0xff;
                int vl = (data >> 24) & 3;
                int vn = (data >> 26) & 3;
                int num = (data >> 16) & 0xff;
                int flg = (data >> 15) & 1;
                int usn = (data >> 14) & 1;
                int addr = data & 0x3ff;
                int filling = cl < wl;

                if (!num) num = 256;
                if (flg) addr += vif->vif1_tops;

                // printf("vif: UNPACK %02x cl=%02x wl=%02x vl=%d vn=%d num=%02x\n", data >> 24, cl, wl, vl, vn, num);

                assert(!filling);

                vif->vif1_pending_words = (((32>>vl) * (vn+1)) * num) / 32;
                vif->vif1_state = VIF_RECV_DATA;

                // printf("vif: %d pending words\n", vif->vif1_pending_words);

                // switch (fmt) {
                //     case UNPACK_V2_16: break;
                //     case UNPACK_V4_32: break;
                //     case UNPACK_V4_16: break;

                //     default: printf("vif1: Unhandled UNPACK format %d\n", fmt); exit(1);
                // }
            } break;
            default: {
                printf("vif1: Unhandled command %02x\n", vif->vif1_cmd);

                exit(1);
            } break;
        }
    } else {
        switch (vif->vif1_cmd) {
            case VIF_CMD_STMASK: {
                vif->vif1_mask = data;
                vif->vif1_state = VIF_IDLE;
            } break;
            case VIF_CMD_STROW: {
                vif->vif1_r[4 - (vif->vif1_pending_words--)] = data;

                if (!vif->vif1_pending_words) {
                    vif->vif1_state = VIF_IDLE;
                }
            } break;
            case VIF_CMD_STCOL: {
                vif->vif1_c[4 - (vif->vif1_pending_words--)] = data;

                if (!vif->vif1_pending_words) {
                    vif->vif1_state = VIF_IDLE;
                }
            } break;
            case VIF_CMD_MPG: {
                // printf("vif1: Writing %08x to VU mem\n", data);

                if (!(--vif->vif1_pending_words)) {
                    vif->vif1_state = VIF_IDLE;
                }
            } break;
            case VIF_CMD_DIRECTHL:
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

            case 0x60: case 0x61: case 0x62: case 0x63:
            case 0x64: case 0x65: case 0x66: case 0x67:
            case 0x68: case 0x69: case 0x6a: case 0x6b:
            case 0x6c: case 0x6d: case 0x6e: case 0x6f:
            case 0x70: case 0x71: case 0x72: case 0x73:
            case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7a: case 0x7b:
            case 0x7c: case 0x7d: case 0x7e: case 0x7f: {
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
        case 0x10004000: // printf("vif0: FIFO read\n"); break;
        case 0x10005000: // printf("vif1: FIFO read\n"); break;
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
        case 0x10004000: // printf("vif0: FIFO write\n"); break;
        case 0x10005000: vif1_handle_fifo_write(vif, data); break;
    }
}

uint128_t ps2_vif_read128(struct ps2_vif* vif, uint32_t addr) {
    switch (addr) {
        case 0x10004000: // printf("vif0: 128-bit FIFO read\n"); break;
        case 0x10005000: // printf("vif1: 128-bit FIFO read\n"); break;
    }

    return (uint128_t){ .u64[0] = 0, .u64[1] = 0 };;
}

void ps2_vif_write128(struct ps2_vif* vif, uint32_t addr, uint128_t data) {
    switch (addr) {
        case 0x10004000: // printf("vif0: 128-bit FIFO write\n"); break;
        case 0x10005000: {
            vif1_handle_fifo_write(vif, data.u32[0]);
            vif1_handle_fifo_write(vif, data.u32[1]);
            vif1_handle_fifo_write(vif, data.u32[2]);
            vif1_handle_fifo_write(vif, data.u32[3]);
        } break;
    }
}