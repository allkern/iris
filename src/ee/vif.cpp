#include <new>
#include <math.h>

#include "vif.hpp"
#include "gif.hpp"

namespace iris::vif {

Vif* create(logger::Logger* logger, int id, scheduler::Scheduler* sched, ee::bus::Bus* bus) {
    Vif* vif = new Vif();

    vif->logger = logger;
    vif->logger_id = logger::register_source(logger, "vif");

    vif->hw.sched = sched;
    vif->hw.bus = bus;
    vif->id = id;

    reset(vif);

    return vif;
}

void connect(Vif* vif, vu::Vu* vu, gif::Gif* gif, ee::intc::Intc* intc, ee::dmac::Dmac* dmac) {
    vif->hw.vu = vu;
    vif->hw.gif = gif;
    vif->hw.intc = intc;
    vif->hw.dmac = dmac;
}

void reset(Vif* vif) {
    auto hw = vif->hw;
    int id = vif->id;

    new (vif) Vif();

    vif->hw = hw;
    vif->id = id;

    vif->dreq = 1;
}

void destroy(Vif* vif) {
    delete vif;
}

static inline void vif_emit_vu_mem(Vif* vif, uint128_t data, int is_fill) {
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
                if (is_fill) {
                    data.u32[i] = vif->r[i];
                } else if (vif->mode == 0) {
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
                data.u32[i] = vu::get_vu_mem_ptr(vif->hw.vu, vif->addr)->u32[i];
            }
        }
    } else if (is_fill) {
        for (int i = 0; i < 4; i++)
            data.u32[i] = vif->r[i];
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

    *vu::get_vu_mem_ptr(vif->hw.vu, vif->addr++) = data;
}

static inline void vif_write_vu_mem(Vif* vif, uint128_t data) {
    vif_emit_vu_mem(vif, data, 0);

    vif->unpack_cycle++;
    vif->unpack_wcount--;

    if (vif->unpack_cl >= vif->unpack_wl) {
        if (vif->unpack_cycle == (int)vif->unpack_wl) {
            vif->addr += vif->unpack_skip;
            vif->unpack_cycle = 0;
        }
    } else {
        if (vif->unpack_cycle == (int)vif->unpack_cl) {
            while (vif->unpack_cycle < (int)vif->unpack_wl && vif->unpack_wcount > 0) {
                vif_emit_vu_mem(vif, (uint128_t){ 0 }, 1);

                vif->unpack_cycle++;
                vif->unpack_wcount--;
            }

            vif->unpack_cycle = 0;
        }
    }
}

static inline void vif_unpack_flush_fills(Vif* vif) {
    while (vif->unpack_wcount > 0) {
        vif_emit_vu_mem(vif, (uint128_t){ 0 }, 1);

        vif->unpack_cycle++;
        vif->unpack_wcount--;

        if (vif->unpack_cycle == (int)vif->unpack_wl)
            vif->unpack_cycle = 0;
    }

    vif->state = VIF_IDLE;
}

void vif0_send_irq(void* udata, int overshoot) {
    Vif* vif = (Vif*)udata;

    ee::intc::irq(vif->hw.intc, ee::intc::VIF0);
}

void vif1_send_irq(void* udata, int overshoot) {
    Vif* vif = (Vif*)udata;

    ee::intc::irq(vif->hw.intc, ee::intc::VIF1);
}

static inline void vif_handle_fifo_write(Vif* vif, uint32_t data) {
    if (vif->state == VIF_IDLE) {
        vif->cmd = (data >> 24) & 0xff;

        int mark = vif->cmd == CMD_MARK;

        if (vif->cmd & 0x80) {
            ee::intc::irq(vif->hw.intc, vif->id ? ee::intc::VIF1 : ee::intc::VIF0);

            // iris_debug(vif, "vif{}: Requested IRQ command={:02x}", vif->id, vif->cmd);

            // Note: MARK commands trigger the IRQ but don't stall the VIF.
            if (!mark) {
                vif->stat |= 0x00000c00;
                // vif->stat ^= 0x0f000000;
                vif->code = data;
                vif->dreq = 0;
            }
        }

        switch ((data >> 24) & 0x7f) {
            case CMD_NOP: {
                // iris_debug(vif, "vif{}: NOP", vif->id);
            } break;
            case CMD_STCYCL: {
                // iris_debug(vif, "vif{}: STCYCL({:04x})", vif->id, data & 0xffff);

                vif->cycle = data & 0xffff;
            } break;
            case CMD_OFFSET: {
                // iris_debug(vif, "vif{}: OFFSET({:04x})", vif->id, data & 0xffff);

                // Set DBF to 0
                vif->stat &= ~0x80;

                // Set TOPS to BASE
                vif->tops = vif->base;

                vif->ofst = data & 0x3ff;
            } break;
            case CMD_BASE: {
                // iris_debug(vif, "vif{}: BASE({:04x})", vif->id, data & 0xffff);

                vif->base = data & 0x3ff;
            } break;
            case CMD_ITOP: {
                // iris_debug(vif, "vif{}: ITOP({:04x})", vif->id, data & 0xffff);

                vif->itops = data & 0x3ff;
            } break;
            case CMD_STMOD: {
                // iris_debug(vif, "vif{}: STMOD({:04x})", vif->id, data & 0xffff);

                vif->mode = data & 3;
            } break;
            case CMD_MSKPATH3: {
                // iris_debug(vif, "vif{}: MSKPATH3({:04x})", vif->id, data & 0xffff);

                gif::set_path3_mask(vif->hw.gif, (data & 0x8000) != 0);
            } break;
            case CMD_MARK: {
                // iris_debug(vif, "vif{}: MARK({:04x})", vif->id, data & 0xffff);

                vif->mark = data & 0xffff;
                vif->stat |= 0x40;
            } break;
            case CMD_FLUSHE: {
                // iris_debug(vif, "vif{}: FLUSHE", vif->id);
            } break;
            case CMD_FLUSH: {
                // Note: MASSIVE GRAN TURISMO HACK!
                //       GT3/4 expect IBT and stall bits to be set when a
                //       VIF IRQ occurs, CODE also needs to be set to the
                //       last command that caused a stall.
                //       This is admittedly a huge hack, but we can't really
                //       emulate any of this without properly implementing
                //       DMA timings.
                // iris_debug(vif, "vif{}: FLUSH", vif->id);
            } break;
            case CMD_FLUSHA: {
                // iris_debug(vif, "vif{}: FLUSHA", vif->id);
            } break;
            case CMD_MSCAL: {
                // iris_debug(vif, "vif{}: MSCAL({:04x})", vif->id, data & 0xffff);

                vif->top = vif->tops;

                // Toggle DBF
                vif->stat ^= 0x80;
                vif->tops = vif->base;
                vif->itop = vif->itops;

                if (vif->stat & 0x80) {
                    vif->tops += vif->ofst;
                }

                vu::execute_program(vif->hw.vu, data & 0xffff);
            } break;
            case CMD_MSCALF: {
                // iris_debug(vif, "vif{}: MSCALF({:04x})", vif->id, data & 0xffff);

                vif->top = vif->tops;

                // Toggle DBF
                vif->stat ^= 0x80;
                vif->tops = vif->base;
                vif->itop = vif->itops;

                if (vif->stat & 0x80) {
                    vif->tops += vif->ofst;
                }

                vu::execute_program(vif->hw.vu, data & 0xffff);
            } break;
            case CMD_MSCNT: {
                // iris_debug(vif, "vif{}: MSCNT({:08x})", vif->id, vu::get_tpc(vif->hw.vu));

                vif->top = vif->tops;

                // Toggle DBF
                vif->stat ^= 0x80;
                vif->tops = vif->base;
                vif->itop = vif->itops;

                if (vif->stat & 0x80) {
                    vif->tops += vif->ofst;
                }

                vu::execute_program_tpc(vif->hw.vu);
            } break;
            case CMD_STMASK: {
                // iris_debug(vif, "vif{}: STMASK({:04x})", vif->id, data & 0xffff);

                vif->state = VIF_RECV_DATA;
                vif->pending_words = 1;
            } break;
            case CMD_STROW: {
                // iris_debug(vif, "vif{}: STROW({:04x})", vif->id, data & 0xffff);

                vif->state = VIF_RECV_DATA;
                vif->pending_words = 4;
            } break;
            case CMD_STCOL: {
                // iris_debug(vif, "vif{}: STCOL({:04x})", vif->id, data & 0xffff);

                vif->state = VIF_RECV_DATA;
                vif->pending_words = 4;
            } break;
            case CMD_MPG: {
                // iris_debug(vif, "vif{}: MPG({:04x}, {:04x})", vif->id, (data >> 16) & 0xff, data & 0xffff);

                int num = (data >> 16) & 0xff;

                if (!num) num = 256;

                vif->addr = data & 0xffff;
                vif->state = VIF_RECV_DATA;
                vif->pending_words = num * 2;
                vif->shift = 0;

                vu::invalidate_range(vif->hw.vu, vif->addr << 3, num << 3);
            } break;
            case CMD_DIRECT: {
                // iris_debug(vif, "vif{}: DIRECT({:04x})", vif->id, data & 0xffff);

                int imm = data & 0xffff;

                if (imm == 0) {
                    imm = 0x10000;
                }

                vif->state = VIF_RECV_DATA;
                vif->pending_words = imm * 4;
                vif->shift = 0;
            } break;
            case CMD_DIRECTHL: {
                // iris_debug(vif, "vif{}: DIRECTHL({:04x})", vif->id, data & 0xffff);

                int imm = data & 0xffff;

                if (imm == 0) {
                    imm = 0x10000;
                }

                vif->state = VIF_RECV_DATA;
                vif->pending_words = imm * 4;
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

                if (!vif->unpack_num) vif->unpack_num = 256;
                if (flg) addr += vif->tops;

                int num = vif->unpack_num;
                int cl = vif->unpack_cl;
                int wl = vif->unpack_wl;
                int filling = cl < wl;

                vif->unpack_wcount = num;

                int read_num = num;

                if (filling) {
                    int rem = num % wl;

                    read_num = cl * (num / wl) + (rem > cl ? cl : rem);
                    vif->unpack_skip = 0;
                } else {
                    vif->unpack_skip = cl - wl;
                }

                uint32_t pack_size = 16;

                if ((vl == 3 && vn == 3) == 0)
                    pack_size = (32 >> vl) * (vn + 1);

                vif->pending_words = pack_size * read_num;
                vif->pending_words = (vif->pending_words + 0x1F) & ~0x1F;
                vif->pending_words /= 32;

                vif->unpack_num = read_num;

                vif->unpack_shift = 0;
                vif->shift = 0;
                vif->addr = addr;

                // iris_debug(vif, "vif{}: UNPACK {:02x} fmt={:02x} flg={} num={:02x} read={} addr={:08x} tops={:08x} usn={} wr={} cl={} wl={} mode={}", vif->id, data >> 24, vif->unpack_fmt, flg, num, read_num, addr, vif->tops, vif->unpack_usn, vif->pending_words, cl, wl, vif->mode);

                if (vif->pending_words == 0) {
                    vif_unpack_flush_fills(vif);
                } else {
                    vif->state = VIF_RECV_DATA;
                }
            } break;
            default: {
                // iris_debug(vif, "vif{}: Unhandled command {:02x}", vif->id, vif->cmd);

                // exit(1);
            } break;
        }
    } else {
        switch (vif->cmd) {
            case CMD_STMASK: {
                vif->mask = data;
                vif->state = VIF_IDLE;
            } break;
            case CMD_STROW: {
                vif->r[4 - (vif->pending_words--)] = data;

                if (!vif->pending_words) {
                    vif->state = VIF_IDLE;
                }
            } break;
            case CMD_STCOL: {
                vif->c[4 - (vif->pending_words--)] = data;

                if (!vif->pending_words) {
                    vif->state = VIF_IDLE;
                }
            } break;
            case CMD_MPG: {
                if (!vif->shift) {
                    vif->data.u32[vif->shift++] = data;
                } else {
                    vif->data.u32[1] = data;

                    // iris_debug(vif, "vif{}: Writing {:08x} {:08x} to MicroMem addr={:04x}", vif->id, vif->data.u32[0], vif->data.u32[1], vif->addr);

                    *vu::get_micro_mem_ptr(vif->hw.vu, vif->addr++) = vif->data.u64[0];

                    vif->shift = 0;
                }

                if (!(--vif->pending_words)) {
                    vif->state = VIF_IDLE;
                }
            } break;
            case CMD_DIRECTHL:
            case CMD_DIRECT: {
                vif->data.u32[vif->shift++] = data;

                vif->pending_words--;

                if (vif->shift == 4) {
                    // iris_debug(vif, "vif{}: Writing {:08x} {:08x} {:08x} {:08x} to GIF FIFO pending={}", vif->id, vif->data.u32[3], vif->data.u32[2], vif->data.u32[1], vif->data.u32[0], vif->pending_words);
                    gif::fifo_write(vif->hw.gif, vif->data, gif::PATH2);

                    vif->shift = 0;
                }

                if (!vif->pending_words) {
                    // iris_debug(vif, "vif{}: DIRECT complete", vif->id);

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
                        iris_fatal_error(vif, "vif{}: Unimplemented unpack format {:02x}", vif->id, vif->unpack_fmt);
                    } break;
                }

                if (!(--vif->pending_words)) {
                    vif->state = VIF_IDLE;
                }
            } break;
        }
    }
}

uint64_t read32(Vif* vif, uint32_t addr) {
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
        case 0x10003c00: {
            uint32_t stat = vif->stat; vif->stat = 0;
            
            return stat; 
        } break;
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
        case 0x10004000: // iris_fatal_error(vif, "vif{}: 32-bit FIFO read", vif->id); break;
        case 0x10005000: // iris_fatal_error(vif, "vif{}: 32-bit FIFO read", vif->id); break;

        default: {
            iris_fatal_error(vif, "vif{}: Unhandled 32-bit read to {:08x}", vif->id, addr);
        } break;
    }

    return 0;
}

void write32(Vif* vif, uint32_t addr, uint64_t data) {
    switch (addr) {
        // VIF0 registers
        case 0x10003810: {
            vif->fbrst = data;
            vif->state = VIF_IDLE;
            vif->pending_words = 0;
            vif->unpack_shift = 0;
            vif->shift = 0;
            vif->dreq = 1;

            // Clear VSS, VFS, VIS, INT, ER0, ER1
            if (data & 8) {
                vif->stat &= ~0x3f00;
            }

            ee::dmac::handle_vif0_transfer(vif->hw.dmac);
        } break;

        case 0x10003820: vif->err = data; break;
        case 0x10003830: vif->stat &= ~0x40; break;

        // VIF1 registers
        case 0x10003c00: vif->stat &= 0x800000; vif->stat |= data & 0x800000; break;
        case 0x10003c10: {
            vif->fbrst = data;
            vif->state = VIF_IDLE;
            vif->pending_words = 0;
            vif->unpack_shift = 0;
            vif->shift = 0;
            vif->dreq = 1;

            // Clear VSS, VFS, VIS, INT, ER0, ER1
            if (data & 8) {
                vif->stat &= ~0x3f00;
            }

            ee::dmac::handle_vif1_transfer(vif->hw.dmac);
        } break;

        case 0x10003c20: vif->err = data; break;
        case 0x10003c30: vif->stat &= ~0x40; break;
        case 0x10003c80: /* Unknown */ break;

        // VIF FIFOs
        case 0x10004000: vif_handle_fifo_write(vif, data); break;
        case 0x10004010: vif_handle_fifo_write(vif, data); break;
        case 0x10005000: vif_handle_fifo_write(vif, data); break;
        case 0x10005010: vif_handle_fifo_write(vif, data); break;

        default: {
            iris_debug(vif, "vif{}: Unhandled 32-bit write to {:08x}", vif->id, addr);

            // exit(1);
        } break;
    }
}

uint128_t read128(Vif* vif, uint32_t addr) {
    switch (addr) {
        case 0x10004000: break; // iris_fatal_error(vif, "vif{}: 128-bit FIFO read", vif->id); break;
        case 0x10005000: break; // iris_fatal_error(vif, "vif{}: 128-bit FIFO read", vif->id); break;

        default: {
            iris_fatal_error(vif, "vif{}: Unhandled 128-bit read to {:08x}", vif->id, addr);
        } break;
    }

    return uint128_t{};
}

void write128(Vif* vif, uint32_t addr, uint128_t data) {
    switch (addr) {
        case 0x10004000:
        case 0x10004010: {
            vif_handle_fifo_write(vif, data.u32[0]);
            vif_handle_fifo_write(vif, data.u32[1]);
            vif_handle_fifo_write(vif, data.u32[2]);
            vif_handle_fifo_write(vif, data.u32[3]);
        } break;

        case 0x10005000:
        case 0x10005010: {
            vif_handle_fifo_write(vif, data.u32[0]);
            vif_handle_fifo_write(vif, data.u32[1]);
            vif_handle_fifo_write(vif, data.u32[2]);
            vif_handle_fifo_write(vif, data.u32[3]);
        } break;

        default: {
            // iris_debug(vif, "vif{}: Unhandled 128-bit write to {:08x}", vif->id, addr);

            // exit(1);
        } break;
    }
}

uint32_t fifo_read(Vif* vif) {
    // iris_debug(vif, "vif{}: 32-bit FIFO read", vif->id);

    return 0;
}

void fifo_write(Vif* vif, uint32_t data) {
    vif_handle_fifo_write(vif, data);
}

int get_dreq(Vif* vif) {
    return vif->dreq;
}

#undef printf

}
