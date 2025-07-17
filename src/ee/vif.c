#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "vif.h"

// #define printf(fmt, ...)(0)

struct ps2_vif* ps2_vif_create(void) {
    return malloc(sizeof(struct ps2_vif));
}

void ps2_vif_init(struct ps2_vif* vif, int id, struct vu_state* vu, struct ps2_gif* gif, struct ps2_intc* intc, struct sched_state* sched, struct ee_bus* bus) {
    memset(vif, 0, sizeof(struct ps2_vif));

    vif->sched = sched;
    vif->intc = intc;
    vif->gif = gif;
    vif->bus = bus;
    vif->vu = vu;
    vif->id = id;
}

void ps2_vif_destroy(struct ps2_vif* vif) {
    free(vif);
}

static inline void vif_write_vu_mem(struct ps2_vif* vif, uint128_t data) {
    // Process mask
    if (vif->unpack_mask) {
        int cycle = (vif->unpack_cycle > 3) ? 3 : vif->unpack_cycle;
        int m[4], shift = (cycle & 3) * 8;
        uint32_t mask = (vif->mask >> shift) & 0xff;
        m[0] = (mask >> 0) & 3;
        m[1] = (mask >> 2) & 3;
        m[2] = (mask >> 4) & 3;
        m[3] = (mask >> 6) & 3;

        // Note: Mode 3 is undocumented, it sets the row registers
        //       to the value of the unpacked data, without changing
        //       the unpacked data itself.
        for (int i = 0; i < 4; i++) {
            if (m[i] == 0) {
                // Normal mode, m==0 -> write value as is
                if (vif->mode == 0) {
                    continue;
                } else if (vif->mode == 1) {
                    // Addition decompression
                    data.u32[i] = vif->r[i] + data.u32[i];
                } else if (vif->mode == 2) {
                    // Subtraction decompression
                    data.u32[i] = vif->r[i] + data.u32[i];
                    vif->r[i] = data.u32[i];
                } else if (vif->mode == 3) {
                    vif->r[i] = data.u32[i];
                }
            } else if (m[i] == 1) {
                data.u32[i] = vif->r[i];
            } else if (m[i] == 2) {
                data.u32[i] = vif->c[cycle];
            } else {
                // m=3 masks this fields' write, so we fetch
                // the value from VU mem instead
                data.u32[i] = vif->vu->vu_mem[vif->addr & 0x3ff].u32[i];
            }
        }
    } else {
        // Do mode processing only
        for (int i = 0; i < 4; i++) {
            if (vif->mode == 0) {
                continue;
            } else if (vif->mode == 1) {
                // Offset decompression
                data.u32[i] = vif->r[i] + data.u32[i];
            } else if (vif->mode == 2) {
                // Difference decompression
                data.u32[i] = vif->r[i] + data.u32[i];
                vif->r[i] = data.u32[i];
            } else if (vif->mode == 3) {
                vif->r[i] = data.u32[i];
            }
        }
    }

    if (vif->unpack_cl == vif->unpack_wl) {
        // Write data normally
        vif->vu->vu_mem[(vif->addr++) & 0x3ff] = data;
    } else if (vif->unpack_cl > vif->unpack_wl) {
        // Write data until unpack_wl is reached, then skip unpack_skip
        vif->vu->vu_mem[(vif->addr++) & 0x3ff] = data;
    } else {
        printf("vif%d: Unpack error: unpack_cl (%d) < unpack_wl (%d)\n", vif->id, vif->unpack_cl, vif->unpack_wl);
        exit(1);
    }

    vif->unpack_cycle++;

    if (vif->unpack_cycle == vif->unpack_wl) {
        vif->addr += vif->unpack_skip;
        vif->unpack_cycle = 0;
    }
}

void vif0_send_irq(void* udata, int overshoot) {
    struct ps2_vif* vif = (struct ps2_vif*)udata;

    ps2_intc_irq(vif->intc, EE_INTC_VIF0);
}

void vif1_send_irq(void* udata, int overshoot) {
    struct ps2_vif* vif = (struct ps2_vif*)udata;

    ps2_intc_irq(vif->intc, EE_INTC_VIF1);
}

static inline void vif_handle_fifo_write(struct ps2_vif* vif, uint32_t data) {
    if (vif->state == VIF_IDLE) {
        vif->cmd = (data >> 24) & 0xff;

        if (vif->cmd & 0x80) {
            struct sched_event event;

            event.callback = vif->id ? vif1_send_irq : vif0_send_irq;
            event.cycles = 1000;
            event.name = vif->id ? "VIF1 Interrupt" : "VIF0 Interrupt";
            event.udata = vif;

            sched_schedule(vif->sched, event);

            // printf("vif%d: Requested IRQ\n", vif->id);
        }

        switch ((data >> 24) & 0x7f) {
            case VIF_CMD_NOP: {
                // printf("vif%d: NOP\n", vif->id);
            } break;
            case VIF_CMD_STCYCL: {
                // printf("vif%d: STCYCL(%04x)\n", vif->id, data & 0xffff);

                vif->cycle = data & 0xffff;
            } break;
            case VIF_CMD_OFFSET: {
                // printf("vif%d: OFFSET(%04x)\n", vif->id, data & 0xffff);

                // Set DBF to 0
                vif->stat &= ~0x80;

                // Set TOPS to BASE
                vif->tops = vif->base;

                vif->ofst = data & 0x3ff;
            } break;
            case VIF_CMD_BASE: {
                // printf("vif%d: BASE(%04x)\n", vif->id, data & 0xffff);

                vif->base = data & 0x3ff;
            } break;
            case VIF_CMD_ITOP: {
                // printf("vif%d: ITOP(%04x)\n", vif->id, data & 0xffff);

                vif->itops = data & 0x3ff;
            } break;
            case VIF_CMD_STMOD: {
                // printf("vif%d: STMOD(%04x)\n", vif->id, data & 0xffff);

                vif->mode = data & 3;
            } break;
            case VIF_CMD_MSKPATH3: {
                // printf("vif%d: MSKPATH3(%04x)\n", vif->id, data & 0xffff);

                if (data & 0x8000) {
                    vif->gif->stat |= 2;
                } else {
                    vif->gif->stat &= ~2;
                }
            } break;
            case VIF_CMD_MARK: {
                // printf("vif%d: MARK(%04x)\n", vif->id, data & 0xffff);

                vif->mark = data & 0xffff;
            } break;
            case VIF_CMD_FLUSHE: {
                // printf("vif%d: FLUSHE\n", vif->id);
            } break;
            case VIF_CMD_FLUSH: {
                // printf("vif%d: FLUSH\n", vif->id);
            } break;
            case VIF_CMD_FLUSHA: {
                // printf("vif%d: FLUSHA\n", vif->id);
            } break;
            case VIF_CMD_MSCAL: {
                // printf("vif%d: MSCAL(%04x)\n", vif->id, data & 0xffff);

                vif->top = vif->tops;

                // Toggle DBF
                vif->stat ^= 0x80;
                vif->tops = vif->base;
                vif->itop = vif->itops;

                if (vif->stat & 0x80) {
                    vif->tops += vif->ofst;
                }

                vu_execute_program(vif->vu, data & 0xffff);
            } break;
            case VIF_CMD_MSCALF: {
                // printf("vif%d: MSCALF(%04x)\n", vif->id, data & 0xffff);

                vif->top = vif->tops;

                // Toggle DBF
                vif->stat ^= 0x80;
                vif->tops = vif->base;
                vif->itop = vif->itops;

                if (vif->stat & 0x80) {
                    vif->tops += vif->ofst;
                }

                vu_execute_program(vif->vu, data & 0xffff);
            } break;
            case VIF_CMD_MSCNT: {
                // printf("vif%d: MSCNT(%08x)\n", vif->id, vif->vu->tpc);

                vif->top = vif->tops;

                // Toggle DBF
                vif->stat ^= 0x80;
                vif->tops = vif->base;
                vif->itop = vif->itops;

                if (vif->stat & 0x80) {
                    vif->tops += vif->ofst;
                }

                vu_execute_program(vif->vu, vif->vu->tpc);
            } break;
            case VIF_CMD_STMASK: {
                // printf("vif%d: STMASK(%04x)\n", vif->id, data & 0xffff);

                vif->state = VIF_RECV_DATA;
                vif->pending_words = 1;
            } break;
            case VIF_CMD_STROW: {
                // printf("vif%d: STROW(%04x)\n", vif->id, data & 0xffff);

                vif->state = VIF_RECV_DATA;
                vif->pending_words = 4;
            } break;
            case VIF_CMD_STCOL: {
                // printf("vif%d: STCOL(%04x)\n", vif->id, data & 0xffff);

                vif->state = VIF_RECV_DATA;
                vif->pending_words = 4;
            } break;
            case VIF_CMD_MPG: {
                // printf("vif%d: MPG(%04x, %04x)\n", vif->id, (data >> 16) & 0xff, data & 0xffff);

                int num = (data >> 16) & 0xff;

                if (!num) num = 256;

                vif->addr = data & 0xffff;
                vif->state = VIF_RECV_DATA;
                vif->pending_words = num * 2;
                vif->shift = 0;
            } break;
            case VIF_CMD_DIRECT: {
                // printf("vif%d: DIRECT(%04x)\n", vif->id, data & 0xffff);

                vif->state = VIF_RECV_DATA;
                vif->pending_words = (data & 0xffff) * 4;
                vif->shift = 0;
            } break;
            case VIF_CMD_DIRECTHL: {
                // printf("vif%d: DIRECTHL(%04x)\n", vif->id, data & 0xffff);

                vif->state = VIF_RECV_DATA;
                vif->pending_words = (data & 0xffff) * 4;
                vif->shift = 0;
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
                vif->unpack_fmt = (data >> 24) & 0xf;
                vif->unpack_usn = (data >> 14) & 1;
                vif->unpack_num = (data >> 16) & 0xff;
                vif->unpack_cl = vif->cycle & 0xff;
                vif->unpack_wl = (vif->cycle >> 8) & 0xff;
                vif->unpack_mask = (data >> 28) & 1;
                vif->unpack_cycle = 0;

                int vl = (data >> 24) & 3;
                int vn = (data >> 26) & 3;
                int flg = (data >> 15) & 1;
                int addr = data & 0x3ff;
                int filling = vif->unpack_cl < vif->unpack_wl;

                if (!vif->unpack_num) vif->unpack_num = 256;
                if (flg) addr += vif->tops;

                if (filling) {
                    printf("vif%d: Filling mode unimplemented\n", vif->id);

                    return;
                    // exit(1);
                }

                // To-do: Handle for filling
                vif->unpack_skip = vif->unpack_cl - vif->unpack_wl;
                vif->unpack_wl_count = 0;

                uint32_t pack_size = 16;

                if ((vl == 3 && vn == 3) == 0)
                    pack_size = (32 >> vl) * (vn + 1);

                vif->pending_words = pack_size * vif->unpack_num;
                vif->pending_words = (vif->pending_words + 0x1F) & ~0x1F;
                vif->pending_words /= 32;

                vif->unpack_shift = 0;
                vif->state = VIF_RECV_DATA;
                vif->shift = 0;
                vif->addr = addr;

                // printf("vif%d: UNPACK %02x fmt=%02x flg=%d num=%02x addr=%08x tops=%08x usn=%d wr=%d mode=%d\n", vif->id, data >> 24, vif->unpack_fmt, flg, vif->unpack_num, addr, vif->tops, vif->unpack_usn, vif->pending_words, vif->mode);
            } break;
            default: {
                // printf("vif%d: Unhandled command %02x\n", vif->id, vif->cmd);

                // exit(1);
            } break;
        }
    } else {
        switch (vif->cmd) {
            case VIF_CMD_STMASK: {
                vif->mask = data;
                vif->state = VIF_IDLE;
            } break;
            case VIF_CMD_STROW: {
                vif->r[4 - (vif->pending_words--)] = data;

                if (!vif->pending_words) {
                    vif->state = VIF_IDLE;
                }
            } break;
            case VIF_CMD_STCOL: {
                vif->c[4 - (vif->pending_words--)] = data;

                if (!vif->pending_words) {
                    vif->state = VIF_IDLE;
                }
            } break;
            case VIF_CMD_MPG: {
                // printf("vif%d: Writing %08x to MicroMem\n", vif->id, data);

                if (!vif->shift) {
                    vif->data.u32[vif->shift++] = data;
                } else {
                    vif->data.u32[1] = data;
                    vif->vu->micro_mem[(vif->addr++) & 0x7ff] = vif->data.u64[0];
                    vif->shift = 0;
                }

                if (!(--vif->pending_words)) {
                    vif->state = VIF_IDLE;
                }
            } break;
            case VIF_CMD_DIRECTHL:
            case VIF_CMD_DIRECT: {
                vif->data.u32[vif->shift++] = data;

                if (vif->shift == 4) {
                    ee_bus_write128(vif->bus, 0x10006000, vif->data);

                    vif->shift = 0;
                }

                if (!(--vif->pending_words)) {
                    vif->state = VIF_IDLE;
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
                switch (vif->unpack_fmt) {
                    // S-32
                    case 0x00: {
                        vif->data.u32[0] = data;
                        vif->data.u32[1] = data;
                        vif->data.u32[2] = data;
                        vif->data.u32[3] = data;

                        vif_write_vu_mem(vif, vif->data);
                    } break;

                    // S-16
                    case 0x01: {
                        for (int i = 0; i < 2; i++) {
                            uint128_t q = { 0 };

                            q.u32[0] = (data >> (i * 16)) & 0xffff;

                            if (!vif->unpack_usn) {
                                q.u32[0] = (int32_t)((int16_t)q.u32[0]);
                            }

                            q.u32[1] = q.u32[0];
                            q.u32[2] = q.u32[0];
                            q.u32[3] = q.u32[0];

                            vif_write_vu_mem(vif, q);

                            vif->unpack_num--;

                            if (!vif->unpack_num)
                                break;
                        }
                    } break;

                    // S-8
                    case 0x02: {
                        for (int i = 0; i < 4; i++) {
                            uint128_t q = { 0 };

                            q.u32[0] = (data >> (i * 8)) & 0xff;

                            if (!vif->unpack_usn) {
                                q.u32[0] = (int32_t)((int8_t)q.u32[0]);
                            }

                            q.u32[1] = q.u32[0];
                            q.u32[2] = q.u32[0];
                            q.u32[3] = q.u32[0];

                            vif_write_vu_mem(vif, q);

                            vif->unpack_num--;

                            if (!vif->unpack_num)
                                break;
                        }
                    } break;

                    // V2-32
                    case 0x04: {
                        vif->unpack_buf[vif->shift++] = data;

                        if (vif->shift == 2) {
                            uint128_t q = { 0 };

                            q.u32[0] = vif->unpack_buf[0];
                            q.u32[1] = vif->unpack_buf[1];

                            vif_write_vu_mem(vif, q);

                            vif->shift = 0;

                            vif->unpack_num--;

                            if (!vif->unpack_num)
                                break;
                        }
                    } break;

                    // V2-16
                    case 0x05: {
                        uint128_t q = { 0 };

                        q.u32[0] = data & 0xffff;
                        q.u32[1] = data >> 16;

                        if (!vif->unpack_usn) {
                            q.u32[0] = (int32_t)((int16_t)q.u32[0]);
                            q.u32[1] = (int32_t)((int16_t)q.u32[1]);
                        }

                        vif_write_vu_mem(vif, q);

                        vif->unpack_num--;

                        if (!vif->unpack_num)
                            break;
                    } break;

                    // V2-8
                    case 0x06: {
                        for (int i = 0; i < 2; i++) {
                            uint128_t q = { 0 };
                            uint16_t d = data >> (i * 16);

                            q.u32[0] = d & 0xff;
                            q.u32[1] = d >> 8;

                            if (!vif->unpack_usn) {
                                q.u32[0] = (int32_t)((int8_t)q.u32[0]);
                                q.u32[1] = (int32_t)((int8_t)q.u32[1]);
                            }

                            vif_write_vu_mem(vif, q);

                            vif->unpack_num--;

                            if (!vif->unpack_num)
                                break;
                        }
                    } break;

                    // V3-32
                    case 0x08: {
                        vif->unpack_buf[vif->shift++] = data;

                        if (vif->shift == 3) {
                            uint128_t q = { 0 };

                            q.u32[0] = vif->unpack_buf[0];
                            q.u32[1] = vif->unpack_buf[1];
                            q.u32[2] = vif->unpack_buf[2];

                            vif_write_vu_mem(vif, q);

                            vif->shift = 0;
                            vif->unpack_num--;

                            if (!vif->unpack_num)
                                break;
                        }
                    } break;

                    // V3-16
                    case 0x09: {
                        vif->unpack_buf[vif->shift++] = data;

                        if (vif->shift == (vif->unpack_shift ? 1 : 2)) {
                            uint128_t q = { 0 };

                            if (!vif->unpack_shift) {
                                q.u32[0] = vif->unpack_buf[0] & 0xffff;
                                q.u32[1] = (vif->unpack_buf[0] >> 16) & 0xffff;
                                q.u32[2] = vif->unpack_buf[1] & 0xffff;
                            } else {
                                q.u32[0] = vif->unpack_data;
                                q.u32[1] = vif->unpack_buf[0] & 0xffff;
                                q.u32[2] = vif->unpack_buf[0] >> 16;
                            }

                            if (!vif->unpack_usn) {
                                q.u32[0] = (int32_t)((int16_t)q.u32[0]);
                                q.u32[1] = (int32_t)((int16_t)q.u32[1]);
                                q.u32[2] = (int32_t)((int16_t)q.u32[2]);
                            }

                            vif_write_vu_mem(vif, q);

                            vif->shift = 0;
                            vif->unpack_num--;
                            vif->unpack_shift ^= 1;
                            vif->unpack_data = vif->unpack_buf[1] >> 16;

                            if (!vif->unpack_num)
                                break;
                        }
                    } break;

                    // V3-8 (disgusting)
                    case 0x0a: {
                        uint128_t q = { 0 };

                        switch (vif->unpack_shift) {
                            case 0: {
                                q.u32[0] = data & 0xff;
                                q.u32[1] = (data >> 8) & 0xff;
                                q.u32[2] = (data >> 16) & 0xff;

                                vif->unpack_data = data >> 24;
                                vif->unpack_shift++;

                                if (!vif->unpack_usn) {
                                    q.u32[0] = (int32_t)((int8_t)q.u32[0]);
                                    q.u32[1] = (int32_t)((int8_t)q.u32[1]);
                                    q.u32[2] = (int32_t)((int8_t)q.u32[2]);
                                }

                                vif_write_vu_mem(vif, q);

                                vif->unpack_num--;

                                if (!vif->unpack_num)
                                    break;
                            } break;

                            case 1: {
                                q.u32[0] = vif->unpack_data;
                                q.u32[1] = data & 0xff;
                                q.u32[2] = (data >> 8) & 0xff;

                                vif->unpack_data = data >> 16;
                                vif->unpack_shift++;

                                if (!vif->unpack_usn) {
                                    q.u32[0] = (int32_t)((int8_t)q.u32[0]);
                                    q.u32[1] = (int32_t)((int8_t)q.u32[1]);
                                    q.u32[2] = (int32_t)((int8_t)q.u32[2]);
                                }

                                vif_write_vu_mem(vif, q);

                                vif->unpack_num--;

                                if (!vif->unpack_num)
                                    break;
                            } break;

                            case 2: {
                                q.u32[0] = vif->unpack_data & 0xff;
                                q.u32[1] = (vif->unpack_data >> 8) & 0xff;
                                q.u32[2] = data & 0xff;

                                vif->unpack_data = (data >> 8) & 0xffffff;
                                vif->unpack_shift++;

                                if (!vif->unpack_usn) {
                                    q.u32[0] = (int32_t)((int8_t)q.u32[0]);
                                    q.u32[1] = (int32_t)((int8_t)q.u32[1]);
                                    q.u32[2] = (int32_t)((int8_t)q.u32[2]);
                                }

                                vif_write_vu_mem(vif, q);

                                vif->unpack_num--;

                                if (!vif->unpack_num)
                                    break;

                                q.u32[0] = (data >> 8) & 0xff;
                                q.u32[1] = (data >> 16) & 0xff;
                                q.u32[2] = (data >> 24) & 0xff;

                                vif->unpack_shift = 0;

                                if (!vif->unpack_usn) {
                                    q.u32[0] = (int32_t)((int8_t)q.u32[0]);
                                    q.u32[1] = (int32_t)((int8_t)q.u32[1]);
                                    q.u32[2] = (int32_t)((int8_t)q.u32[2]);
                                }

                                vif_write_vu_mem(vif, q);

                                vif->unpack_num--;

                                if (!vif->unpack_num)
                                    break;
                            } break;
                        }
                    } break;

                    // V4-32
                    case 0x0c: {
                        vif->unpack_buf[vif->shift++] = data;

                        if (vif->shift == 4) {
                            uint128_t q = { 0 };

                            q.u32[0] = vif->unpack_buf[0];
                            q.u32[1] = vif->unpack_buf[1];
                            q.u32[2] = vif->unpack_buf[2];
                            q.u32[3] = vif->unpack_buf[3];

                            vif_write_vu_mem(vif, q);

                            vif->shift = 0;
                        }
                    } break;

                    // V4-16
                    case 0x0d: {
                        vif->unpack_buf[vif->shift++] = data;

                        if (vif->shift == 2) {
                            uint128_t q = { 0 };

                            q.u32[0] = vif->unpack_buf[0] & 0xffff;
                            q.u32[1] = vif->unpack_buf[0] >> 16;
                            q.u32[2] = vif->unpack_buf[1] & 0xffff;
                            q.u32[3] = vif->unpack_buf[1] >> 16;

                            if (!vif->unpack_usn) {
                                q.u32[0] = (int32_t)((int16_t)q.u32[0]);
                                q.u32[1] = (int32_t)((int16_t)q.u32[1]);
                                q.u32[2] = (int32_t)((int16_t)q.u32[2]);
                                q.u32[3] = (int32_t)((int16_t)q.u32[3]);
                            }

                            vif_write_vu_mem(vif, q);

                            vif->shift = 0;
                        }
                    } break;

                    // V4-8
                    case 0x0e: {
                        uint128_t q = { 0 };

                        q.u32[0] = data & 0xff;
                        q.u32[1] = (data >> 8) & 0xff;
                        q.u32[2] = (data >> 16) & 0xff;
                        q.u32[3] = (data >> 24) & 0xff;

                        if (!vif->unpack_usn) {
                            q.u32[0] = (int32_t)((int8_t)q.u32[0]);
                            q.u32[1] = (int32_t)((int8_t)q.u32[1]);
                            q.u32[2] = (int32_t)((int8_t)q.u32[2]);
                            q.u32[3] = (int32_t)((int8_t)q.u32[3]);
                        }

                        vif_write_vu_mem(vif, q);
                    } break;

                    // V4-5
                    case 0x0f: {
                        uint128_t q = { 0 };

                        for (int i = 0; i < 2; i++) {
                            uint16_t c = (data >> (i * 16)) & 0xffff;

                            q.u32[0] = ((c >> 0) & 0x1f) << 3;
                            q.u32[1] = ((c >> 5) & 0x1f) << 3;
                            q.u32[2] = ((c >> 10) & 0x1f) << 3;
                            q.u32[3] = ((c >> 15) & 1) << 7;

                            vif_write_vu_mem(vif, q);

                            vif->unpack_num--;

                            if (!vif->unpack_num)
                                break;
                        }
                    } break;

                    default: {
                        printf("vif%d: Unimplemented unpack format %02x\n", vif->id, vif->unpack_fmt);

                        exit(1);
                    } break;
                }

                if (!(--vif->pending_words)) {
                    vif->state = VIF_IDLE;
                }
            } break;
        }
    }
}

uint64_t ps2_vif_read32(struct ps2_vif* vif, uint32_t addr) {
    switch (addr) {
        // VIF0 registers
        case 0x10003800: return vif->stat;
        // case 0x10003810: return vif->fbrst;
        case 0x10003820: return vif->err;
        case 0x10003830: return vif->mark;
        case 0x10003840: return vif->cycle;
        case 0x10003850: return vif->mode;
        case 0x10003860: return vif->num;
        case 0x10003870: return vif->mask;
        case 0x10003880: return vif->code;
        case 0x10003890: return vif->itops;
        case 0x100038d0: return vif->itop;
        case 0x10003900: return vif->r[0];
        case 0x10003910: return vif->r[1];
        case 0x10003920: return vif->r[2];
        case 0x10003930: return vif->r[3];
        case 0x10003940: return vif->c[0];
        case 0x10003950: return vif->c[1];
        case 0x10003960: return vif->c[2];
        case 0x10003970: return vif->c[3];

        // VIF1 registers
        case 0x10003c00: return vif->stat;
        case 0x10003c10: return vif->fbrst;
        case 0x10003c20: return vif->err;
        case 0x10003c30: return vif->mark;
        case 0x10003c40: return vif->cycle;
        case 0x10003c50: return vif->mode;
        case 0x10003c60: return vif->num;
        case 0x10003c70: return vif->mask;
        case 0x10003c80: return vif->code;
        case 0x10003c90: return vif->itops;
        case 0x10003ca0: return vif->base;
        case 0x10003cb0: return vif->ofst;
        case 0x10003cc0: return vif->tops;
        case 0x10003cd0: return vif->itop;
        case 0x10003ce0: return vif->top;
        case 0x10003d00: return vif->r[0];
        case 0x10003d10: return vif->r[1];
        case 0x10003d20: return vif->r[2];
        case 0x10003d30: return vif->r[3];
        case 0x10003d40: return vif->c[0];
        case 0x10003d50: return vif->c[1];
        case 0x10003d60: return vif->c[2];
        case 0x10003d70: return vif->c[3];

        // VIF FIFOs
        case 0x10004000: // printf("vif%d: 32-bit FIFO read\n", vif->id); exit(1); break;
        case 0x10005000: // printf("vif%d: 32-bit FIFO read\n", vif->id); exit(1); break;

        default: {
            printf("vif%d: Unhandled 32-bit read to %08x\n", vif->id, addr);

            exit(1);
        } break;
    }

    return 0;
}

void ps2_vif_write32(struct ps2_vif* vif, uint32_t addr, uint64_t data) {
    switch (addr) {
        // VIF0 registers
        case 0x10003810: {
            vif->fbrst = data;
            vif->state = VIF_IDLE;
            vif->pending_words = 0;
            vif->unpack_shift = 0;
            vif->shift = 0;
        } break;

        case 0x10003820: vif->err = data; break;
        case 0x10003830: vif->mark = data; break;

        // VIF1 registers
        case 0x10003c00: vif->stat &= 0x800000; vif->stat |= data & 0x800000; break;
        case 0x10003c10: {
            vif->fbrst = data;
            vif->state = VIF_IDLE;
            vif->pending_words = 0;
            vif->unpack_shift = 0;
            vif->shift = 0;
        } break;

        case 0x10003c20: vif->err = data; break;
        case 0x10003c30: vif->mark = data; break;

        // VIF FIFOs
        case 0x10004000: vif_handle_fifo_write(vif, data); break;
        case 0x10005000: vif_handle_fifo_write(vif, data); break;

        default: {
            printf("vif%d: Unhandled 32-bit write to %08x\n", vif->id, addr);

            exit(1);
        } break;
    }
}

uint128_t ps2_vif_read128(struct ps2_vif* vif, uint32_t addr) {
    switch (addr) {
        case 0x10004000: // printf("vif%d: 128-bit FIFO read\n", vif->id); exit(1); break;
        case 0x10005000: // printf("vif%d: 128-bit FIFO read\n", vif->id); exit(1); break;

        default: {
            printf("vif%d: Unhandled 128-bit read to %08x\n", vif->id, addr);

            exit(1);
        } break;
    }

    return (uint128_t){ .u64[0] = 0, .u64[1] = 0 };
}

void ps2_vif_write128(struct ps2_vif* vif, uint32_t addr, uint128_t data) {
    switch (addr) {
        case 0x10004000: {
            vif_handle_fifo_write(vif, data.u32[0]);
            vif_handle_fifo_write(vif, data.u32[1]);
            vif_handle_fifo_write(vif, data.u32[2]);
            vif_handle_fifo_write(vif, data.u32[3]);
        } break;

        case 0x10005000: {
            vif_handle_fifo_write(vif, data.u32[0]);
            vif_handle_fifo_write(vif, data.u32[1]);
            vif_handle_fifo_write(vif, data.u32[2]);
            vif_handle_fifo_write(vif, data.u32[3]);
        } break;

        default: {
            printf("vif%d: Unhandled 128-bit write to %08x\n", vif->id, addr);

            exit(1);
        } break;
    }
}

#undef printf