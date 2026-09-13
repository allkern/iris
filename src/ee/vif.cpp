#include <new>
#include <math.h>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "vif.hpp"
#include "gif.hpp"

#include "profile_counters.hpp"

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

    logger::Logger* logger = vif->logger;
    size_t logger_id = vif->logger_id;

    new (vif) Vif();

    vif->logger = logger;
    vif->logger_id = logger_id;

    vif->hw = hw;
    vif->id = id;

    vif->dreq = 1;
}

void destroy(Vif* vif) {
    delete vif;
}

static inline void vif_emit_vu_mem(Vif* vif, uint128_t data, int is_fill) {
    // Fastpath the most common case
    if (!vif->unpack_mask && !is_fill && vif->mode == 0) {
        *vu::get_vu_mem_ptr(vif->hw.vu, vif->addr++) = data;

        return;
    }

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

static const char* vif_get_cmd_name(uint8_t cmd) {
    switch (cmd & 0x7f) {
        case CMD_NOP: return "NOP";
        case CMD_STCYCL: return "STCYCL";
        case CMD_OFFSET: return "OFFSET";
        case CMD_BASE: return "BASE";
        case CMD_ITOP: return "ITOP";
        case CMD_STMOD: return "STMOD";
        case CMD_MSKPATH3: return "MSKPATH3";
        case CMD_MARK: return "MARK";
        case CMD_FLUSHE: return "FLUSHE";
        case CMD_FLUSH: return "FLUSH";
        case CMD_FLUSHA: return "FLUSHA";
        case CMD_MSCAL: return "MSCAL";
        case CMD_MSCALF: return "MSCALF";
        case CMD_MSCNT: return "MSCNT";
        case CMD_STMASK: return "STMASK";
        case CMD_STROW: return "STROW";
        case CMD_STCOL: return "STCOL";
        case CMD_MPG: return "MPG";
        case CMD_DIRECT: return "DIRECT";
        case CMD_DIRECTHL: return "DIRECTHL";
    }

    if ((cmd & 0x60) == 0x60)
        return "UNPACK";

    return "?";
}

static inline bool vif_is_fifo(uint32_t addr) {
    uint32_t page = addr & 0xfffff000;

    return page == VIF0_FIFO_BASE || page == VIF1_FIFO_BASE;
}

static inline void count_vif_word(Vif* vif) {
    if (!vif->id) {
        profile::count(profile::VIF0_WORDS);

        return;
    }

    if (vif->state == VIF_IDLE) {
        profile::count(profile::VIF1_COMMAND_WORDS);

        return;
    }

    int command = vif->cmd & 0x7f;

    if (command == CMD_DIRECT || command == CMD_DIRECTHL) {
        profile::count(profile::VIF1_DIRECT_WORDS);
    } else if (command == CMD_MPG) {
        profile::count(profile::VIF1_MPG_WORDS);
    } else if ((command & 0x60) == 0x60) {
        profile::count(profile::VIF1_UNPACK_WORDS);
    } else {
        profile::count(profile::VIF1_REGISTER_WORDS);
    }
}

static inline void vif_handle_fifo_write(Vif* vif, uint32_t data) {
    count_vif_word(vif);

    if (vif->state == VIF_IDLE) {
        vif->cmd = (data >> 24) & 0xff;

        // iris_info(vif, "vif{}: Received command {:02x}, irq={} raw={:08x} ({})", vif->id, vif->cmd, (vif->cmd & 0x80) != 0, data, vif_get_cmd_name(vif->cmd));

        int mark = vif->cmd == CMD_MARK;

        if ((vif->cmd & 0x80) && !(vif->err & ERR_MII)) {
            ee::intc::irq(vif->hw.intc, vif->id ? ee::intc::VIF1 : ee::intc::VIF0);

            // iris_debug(vif, "vif{}: Requested IRQ command={:02x}", vif->id, vif->cmd);

            // Note: MARK commands trigger the IRQ but don't stall the VIF.
            if (!mark) {
                vif->stat |= STAT_VIS | STAT_INT;
                vif->code = data;
                vif->dreq = 0;
            }
        }

        // iris_info(vif, "vif{}: {} code={:08x} num={:02x} imm={:04x}",
        //     vif->id, vif_get_cmd_name(vif->cmd), data, (data >> 16) & 0xff, data & 0xffff);

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

                vu::end_micro_upload(vif->hw.vu);
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

                vu::end_micro_upload(vif->hw.vu);
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

                vu::end_micro_upload(vif->hw.vu);
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

                vu::begin_micro_upload(vif->hw.vu);
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
                iris_warning(vif, "vif{}: Unhandled command {:02x} (code={:08x})",
                    vif->id, vif->cmd & 0x7f, data);
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

                    vu::upload_micro_word(vif->hw.vu, vif->addr++, vif->data.u64[0]);

                    vif->shift = 0;
                }

                if (!(--vif->pending_words)) {
                    vu::end_micro_upload(vif->hw.vu);

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

        default: {
            if (vif_is_fifo(addr))
                break;

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
        // Only FDR is writable, the rest of the status is owned by the VIF
        case 0x10003c00: vif->stat = (vif->stat & ~STAT_FDR) | (data & STAT_FDR); break;
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

        default: {
            if (vif_is_fifo(addr)) {
                vif_handle_fifo_write(vif, data);

                break;
            }

            iris_debug(vif, "vif{}: Unhandled 32-bit write to {:08x}", vif->id, addr);
        } break;
    }
}

uint128_t read128(Vif* vif, uint32_t addr) {
    if (!vif_is_fifo(addr))
        iris_fatal_error(vif, "vif{}: Unhandled 128-bit read to {:08x}", vif->id, addr);

    return uint128_t{};
}

void write128(Vif* vif, uint32_t addr, uint128_t data) {
    if (!vif_is_fifo(addr)) {
        iris_debug(vif, "vif{}: Unhandled 128-bit write to {:08x}", vif->id, addr);

        return;
    }

    for (int i = 0; i < 4; i++) {
        vif_handle_fifo_write(vif, data.u32[i]);
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

void consume_direct_qwords(Vif* vif, uint32_t qwords, uint128_t last) {
    profile::count(profile::VIF1_DIRECT_WORDS, (uint64_t)qwords * 4);

    vif->data = last;
    vif->pending_words -= (int)(qwords * 4);

    if (!vif->pending_words) {
        vif->state = VIF_IDLE;
    }
}

static bool vif_bulk_check_enabled() {
    static const char* setting = getenv("IRIS_VIF_BULK_CHECK");
    static const bool enabled = setting && setting[0] == '1';

    return enabled;
}

static inline uint32_t vif_unpack_vertex_words(uint32_t fmt) {
    switch (fmt) {
        case UNPACK_S_32: return 1;
        case UNPACK_V2_32: return 2;
        case UNPACK_V2_16: return 1;
        case UNPACK_V3_32: return 3;
        case UNPACK_V4_32: return 4;
        case UNPACK_V4_16: return 2;
        case UNPACK_V4_8: return 1;
    }

    return 0;
}

static inline bool vif_unpack_counts_vertices(uint32_t fmt) {
    return fmt == UNPACK_V2_32 || fmt == UNPACK_V2_16 || fmt == UNPACK_V3_32;
}

static inline bool vif_unpack_is_simple(const Vif* vif) {
    if (vif->unpack_mask || vif->mode != 0) {
        return false;
    }

    if (vif->unpack_cl != vif->unpack_wl || vif->unpack_wl == 0) {
        return false;
    }

    return vif_unpack_vertex_words(vif->unpack_fmt) != 0;
}

static inline uint32_t vif_extend16(uint32_t value, bool sign_extend) {
    if (!sign_extend) {
        return value;
    }

    return (uint32_t)(int32_t)(int16_t)value;
}

static inline uint32_t vif_extend8(uint32_t value, bool sign_extend) {
    if (!sign_extend) {
        return value;
    }

    return (uint32_t)(int32_t)(int8_t)value;
}

static inline uint128_t vif_decode_vertex(uint32_t fmt, const uint8_t* source, bool sign_extend) {
    uint128_t q = { 0 };

    switch (fmt) {
        case UNPACK_S_32: {
            uint32_t word;

            memcpy(&word, source, sizeof(word));

            q.u32[0] = word;
            q.u32[1] = word;
            q.u32[2] = word;
            q.u32[3] = word;
        } break;

        case UNPACK_V2_32: {
            memcpy(&q.u32[0], source, 8);
        } break;

        case UNPACK_V3_32: {
            memcpy(&q.u32[0], source, 12);
        } break;

        case UNPACK_V4_32: {
            memcpy(&q.u32[0], source, 16);
        } break;

        case UNPACK_V2_16: {
            uint32_t word;

            memcpy(&word, source, sizeof(word));

            q.u32[0] = vif_extend16(word & 0xffff, sign_extend);
            q.u32[1] = vif_extend16(word >> 16, sign_extend);
        } break;

        case UNPACK_V4_16: {
            uint32_t words[2];

            memcpy(words, source, sizeof(words));

            q.u32[0] = vif_extend16(words[0] & 0xffff, sign_extend);
            q.u32[1] = vif_extend16(words[0] >> 16, sign_extend);
            q.u32[2] = vif_extend16(words[1] & 0xffff, sign_extend);
            q.u32[3] = vif_extend16(words[1] >> 16, sign_extend);
        } break;

        case UNPACK_V4_8: {
            q.u32[0] = vif_extend8(source[0], sign_extend);
            q.u32[1] = vif_extend8(source[1], sign_extend);
            q.u32[2] = vif_extend8(source[2], sign_extend);
            q.u32[3] = vif_extend8(source[3], sign_extend);
        } break;
    }

    return q;
}

static void vif_unpack_finish_span(Vif* vif, const uint8_t* data, uint32_t vertices, const uint128_t& last) {
    uint32_t fmt = vif->unpack_fmt;
    uint32_t per_vertex = vif_unpack_vertex_words(fmt);
    uint32_t consumed = vertices * per_vertex;

    if (per_vertex > 1) {
        memcpy(vif->unpack_buf, data + (size_t)(consumed - per_vertex) * 4, (size_t)per_vertex * 4);
    }

    if (fmt == UNPACK_S_32) {
        vif->data = last;
    }

    if (vif_unpack_counts_vertices(fmt)) {
        vif->unpack_num -= vertices;
    }

    vif->pending_words -= (int)consumed;

    if (!vif->pending_words) {
        vif->state = VIF_IDLE;
    }
}

static uint32_t vif_unpack_simple(Vif* vif, const uint8_t* data, uint32_t words) {
    uint32_t per_vertex = vif_unpack_vertex_words(vif->unpack_fmt);
    uint32_t vertices = words / per_vertex;

    if (!vertices) {
        return 0;
    }

    uint128_t* vu_mem = vu::get_vu_mem_ptr(vif->hw.vu, 0);
    uint32_t mask = vu::get_vu_mem_size(vif->hw.vu);
    uint32_t addr = vif->addr;
    uint32_t fmt = vif->unpack_fmt;
    bool sign_extend = !vif->unpack_usn;

    uint128_t last = { 0 };

    for (uint32_t vertex = 0; vertex < vertices; vertex++) {
        const uint8_t* source = data + (size_t)vertex * per_vertex * 4;

        last = vif_decode_vertex(fmt, source, sign_extend);

        vu_mem[addr & mask] = last;

        addr++;
    }

    vif->addr = addr;
    vif->unpack_cycle = (int)(((uint32_t)vif->unpack_cycle + vertices) % vif->unpack_wl);
    vif->unpack_wcount -= (int)vertices;

    vif_unpack_finish_span(vif, data, vertices, last);

    return vertices * per_vertex;
}

static uint32_t vif_unpack_general(Vif* vif, const uint8_t* data, uint32_t words) {
    uint32_t per_vertex = vif_unpack_vertex_words(vif->unpack_fmt);
    uint32_t vertices = words / per_vertex;

    if (!vertices) {
        return 0;
    }

    uint32_t fmt = vif->unpack_fmt;
    bool sign_extend = !vif->unpack_usn;

    uint128_t last = { 0 };

    for (uint32_t vertex = 0; vertex < vertices; vertex++) {
        const uint8_t* source = data + (size_t)vertex * per_vertex * 4;

        last = vif_decode_vertex(fmt, source, sign_extend);

        vif_write_vu_mem(vif, last);
    }

    vif_unpack_finish_span(vif, data, vertices, last);

    return vertices * per_vertex;
}

static void vif_unpack_span(Vif* vif, const uint8_t* data, uint32_t count) {
    bool bulk = vif_unpack_vertex_words(vif->unpack_fmt) != 0;
    bool simple = vif_unpack_is_simple(vif);

    uint32_t index = 0;

    while (index < count) {
        if (bulk && !vif->shift) {
            const uint8_t* source = data + (size_t)index * 4;

            uint32_t remaining = count - index;
            uint32_t consumed = 0;

            if (simple) {
                consumed = vif_unpack_simple(vif, source, remaining);
            } else {
                consumed = vif_unpack_general(vif, source, remaining);
            }

            if (consumed) {
                profile::count(profile::VIF1_UNPACK_WORDS, consumed);

                index += consumed;

                continue;
            }
        }

        uint32_t word;

        memcpy(&word, data + (size_t)index * 4, sizeof(word));

        vif_handle_fifo_write(vif, word);

        index++;
    }
}

static void vif_unpack_span_by_word(Vif* vif, const uint8_t* data, uint32_t count) {
    for (uint32_t index = 0; index < count; index++) {
        uint32_t word;

        memcpy(&word, data + (size_t)index * 4, sizeof(word));

        vif_handle_fifo_write(vif, word);
    }
}

static bool vif_unpack_states_match(const Vif* a, const Vif* b) {
    if (a->addr != b->addr || a->pending_words != b->pending_words || a->state != b->state) {
        return false;
    }

    if (a->unpack_cycle != b->unpack_cycle || a->unpack_wcount != b->unpack_wcount || a->unpack_num != b->unpack_num) {
        return false;
    }

    if (a->shift != b->shift || a->unpack_shift != b->unpack_shift || a->unpack_data != b->unpack_data) {
        return false;
    }

    if (memcmp(a->unpack_buf, b->unpack_buf, sizeof(a->unpack_buf))) {
        return false;
    }

    if (memcmp(&a->data, &b->data, sizeof(a->data))) {
        return false;
    }

    return memcmp(a->r, b->r, sizeof(a->r)) == 0;
}

static void vif_unpack_span_checked(Vif* vif, const uint8_t* data, uint32_t count) {
    static std::vector <uint8_t> memory_before;
    static std::vector <uint8_t> memory_bulk;
    static int warnings = 0;

    uint128_t* memory = vu::get_vu_mem_ptr(vif->hw.vu, 0);
    size_t bytes = ((size_t)vu::get_vu_mem_size(vif->hw.vu) + 1) * sizeof(uint128_t);

    memory_before.resize(bytes);
    memory_bulk.resize(bytes);

    Vif before = *vif;

    memcpy(memory_before.data(), memory, bytes);

    vif_unpack_span(vif, data, count);

    Vif bulk = *vif;

    memcpy(memory_bulk.data(), memory, bytes);

    *vif = before;

    memcpy(memory, memory_before.data(), bytes);

    uint64_t counted = profile::counters[profile::VIF1_UNPACK_WORDS];

    vif_unpack_span_by_word(vif, data, count);

    profile::counters[profile::VIF1_UNPACK_WORDS] = counted;

    bool same_state = vif_unpack_states_match(&bulk, vif);
    bool same_memory = memcmp(memory_bulk.data(), memory, bytes) == 0;

    if (same_state && same_memory) {
        return;
    }

    profile::count(profile::VIF1_BULK_CHECK_MISMATCHES);

    if (warnings < 16) {
        warnings++;

        iris_warning(vif, "vif1: bulk unpack differs from the word path fmt={:x} words={} state_matches={} memory_matches={}",
            before.unpack_fmt, count, same_state, same_memory
        );
    }
}

void unpack_words(Vif* vif, const uint8_t* data, uint32_t count) {
    if (vif_bulk_check_enabled()) {
        vif_unpack_span_checked(vif, data, count);

        return;
    }

    vif_unpack_span(vif, data, count);
}

#undef printf

}
