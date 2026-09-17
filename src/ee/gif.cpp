#include <new>

#include "gif.hpp"
#include "gs/gs.hpp"
#include "vu.hpp"
#include "dmac.hpp"
#include "mtvu.hpp"

#include "profile_counters.hpp"

namespace iris::gif {

// Burnout games need the FQC field on STAT to change on
// GIF DMA transfers, otherwise they'll hang on the initial
// loading screen.

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

constexpr uint64_t GIF_HASH_OFFSET_BASIS = 0xcbf29ce484222325ull;
constexpr uint64_t GIF_HASH_PRIME = 0x100000001b3ull;

static bool read_gif_hash_setting() {
    const char* setting = getenv("IRIS_GIF_HASH");

    return setting && setting[0];
}

static const bool gif_hash_enabled = read_gif_hash_setting();

static inline void gif_fold_transfer_hash(Gif* gif, int path, const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;

    uint64_t hash = gif->transfer_hash;

    hash = (hash ^ (uint64_t)path) * GIF_HASH_PRIME;
    hash = (hash ^ (uint64_t)size) * GIF_HASH_PRIME;

    for (size_t offset = 0; offset + 4 <= size; offset += 4) {
        uint32_t word;

        memcpy(&word, bytes + offset, sizeof(word));

        hash = (hash ^ word) * GIF_HASH_PRIME;
    }

    gif->transfer_hash = hash;
}

static inline void gif_send_transfer(Gif* gif, int path, const void* data, size_t size) {
    if (gif_hash_enabled) {
        gif_fold_transfer_hash(gif, path, data, size);
    }

    if (gif->transfer) {
        gif->transfer(gif->udata, path, data, size);
    }

    if (gif->dump_transfer) {
        gif->dump_transfer(gif->dump_udata, path, data, size);
    }
}

Gif* create(logger::Logger* logger) {
    Gif* gif = new Gif();

    gif->transfer_hash = GIF_HASH_OFFSET_BASIS;

    gif->logger = logger;
    gif->logger_id = logger::register_source(logger, "gif");

    const char* e = getenv("IRIS_PATH3_MASK");

    gif->path3_mask_enable = (e && e[0] == '1') ? 1 : 0;

    const char* st = getenv("IRIS_PATH3_STALL");

    gif->p3_stall_enable = (st && st[0] == '1') ? 1 : 0;

    // A queue for each PATH
    for (int i = 0; i < 3; i++)
        gif->queue[i] = queue::create();

    return gif;
}

void connect(Gif* gif, ee::dmac::Dmac* dmac, vu::Vu* vu1, gs::Gs* gs) {
    gif->hw.dmac = dmac;
    gif->hw.vu1 = vu1;
    gif->hw.gs = gs;
}

void reset(Gif* gif) {
    gif->ctrl = 0;
    gif->mode = 0;
    gif->stat = 0;
    gif->tag0 = 0;
    gif->tag1 = 0;
    gif->tag2 = 0;
    gif->tag3 = 0;
    gif->cnt = 0;
    gif->p3cnt = 0;
    gif->p3tag = 0;
    gif->state = 0;
    gif->q = 0;

    gif->mask_m3r = 0;
    gif->mask_m3p = 0;
    gif->p3_defer_size = 0;
    gif->stat &= ~3;

    gif->fifo_activity.store(0);

    gif->scan.state = State::RECV_TAG;
    gif->scan.qwc = 0;
    gif->scan.events = 0;

    memset(&gif->tag, 0, sizeof(Tag));

    for (int i = 0; i < 3; i++)
        queue::clear(gif->queue[i]);
}

void destroy(Gif* gif) {
    for (int i = 0; i < 3; i++)
        queue::destroy(gif->queue[i]);

    if (gif->p3_defer_buf)
        free(gif->p3_defer_buf);

    delete gif;
}

static inline int gif_path3_masked(Gif* gif) {
    return gif->path3_mask_enable && (gif->mask_m3r || gif->mask_m3p);
}

static void gif_defer_path3(Gif* gif, const void* buf, size_t size) {
    if (gif->p3_defer_size + size > gif->p3_defer_cap) {
        size_t cap = gif->p3_defer_cap ? gif->p3_defer_cap : 0x10000;

        while (gif->p3_defer_size + size > cap)
            cap *= 2;

        uint8_t* grown = (uint8_t *)realloc(gif->p3_defer_buf, cap);

        if (!grown) {
            iris_error(gif, "path3: cannot hold {} more bytes, dropping {} held", size, gif->p3_defer_size);

            gif->p3_defer_size = 0;

            return;
        }

        gif->p3_defer_buf = grown;
        gif->p3_defer_cap = cap;
    }

    memcpy(gif->p3_defer_buf + gif->p3_defer_size, buf, size);

    gif->p3_defer_size += size;
}

int can_accept(Gif* gif, int path) {
    if (path != PATH3 || !gif->p3_stall_enable) {
        return 1;
    }

    if (!gif_path3_masked(gif)) {
        return 1;
    }

    if (gif->state != State::RECV_TAG) {
        return 1;
    }

    return 0;
}

int path3_stall_enabled(Gif* gif) {
    return gif->p3_stall_enable;
}

static void gif_path3_lifted(Gif* gif);

static void gif_flush_path3(Gif* gif) {
    if (!gif->p3_defer_size)
        return;

    gif->flushing_deferred_path3 = 1;

    gif_send_transfer(gif, PATH3, gif->p3_defer_buf, gif->p3_defer_size);

    gif->flushing_deferred_path3 = 0;

    gif->p3_defer_size = 0;
}

static void gif_path3_lifted(Gif* gif) {
    if (!gif->p3_stall_enable) {
        gif_flush_path3(gif);

        return;
    }

    if (gif->p3_resuming) {
        return;
    }

    gif->p3_resuming = 1;

    ee::dmac::resume_gif(gif->hw.dmac);

    gif->p3_resuming = 0;
}

uint64_t read32(Gif* gif, uint32_t addr) {
    switch (addr) {
        case 0x10003020: {
            // Clear FQC when reading STAT
            uint32_t v = gif->stat;

            if (gif->hw.mtvu) {
                v |= mtvu::take_gif_fifo_activity(gif->hw.mtvu);
            }

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

void write32(Gif* gif, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x10003000: {
            if (data & 1) {
                reset(gif);
            }

            if (gif->hw.mtvu) {
                mtvu::push_gif_write32(gif->hw.mtvu, addr, (uint32_t)data);
            }
        } return;
        case 0x10003010: {
            gif->mode = data;

            int prev = gif_path3_masked(gif);

            gif->mask_m3r = data & 1;

            if (data & 1) {
                gif->stat |= 1;
            } else {
                gif->stat &= ~1;
            }

            if (gif->hw.mtvu) {
                mtvu::push_gif_write32(gif->hw.mtvu, addr, (uint32_t)data);

                return;
            }

            if (prev && !gif_path3_masked(gif)) {
                gif_path3_lifted(gif);
            }
        } return;
    }
}

// void gif_write_rgbaq(Gif* gif, uint128_t data) {
//     uint64_t r = data.u64[0] & 0xff;
//     uint64_t g = (data.u64[0] >> 32) & 0xff;
//     uint64_t b = data.u64[1] & 0xff;
//     uint64_t a = (data.u64[1] >> 32) & 0xff;
//     uint64_t v = r | (g << 8) | (b << 16) | (a << 24) | (gif->q << 32);

//     gs::write_internal(gif->hw.gs, gs::RGBAQ, v);
// }

// void gif_write_stq(Gif* gif, uint128_t data) {
//     gif->q = data.u64[1] & 0xffffffff;

//     gs::write_internal(gif->hw.gs, gs::ST, data.u64[0]);
// }

// void gif_write_uv(Gif* gif, uint128_t data) {
//     gs::write_internal(gif->hw.gs, gs::UV, (data.u64[0] & 0x3fff) | (data.u64[0] >> 16));
// }

// void gif_write_xyzf23(Gif* gif, uint128_t data) {
//     uint64_t x = data.u64[0] & 0xffff;
//     uint64_t y = (data.u64[0] >> 32) & 0xffff;
//     uint64_t z = (data.u64[1] >> 4) & 0xffffff;
//     uint64_t f = (data.u64[1] >> 36) & 0xff;
//     uint64_t v = x | (y << 16) | (z << 32) | (f << 56);
//     uint64_t adc = data.u64[1] & 0x800000000000ul;

//     gs::write_internal(gif->hw.gs, adc ? gs::XYZF3 : gs::XYZF2, v);
// }

// void gif_write_xyz23(Gif* gif, uint128_t data) {
//     uint64_t x = data.u64[0] & 0xffff;
//     uint64_t y = (data.u64[0] >> 32) & 0xffff;
//     uint64_t z = data.u64[1] & 0xffffffff;
//     uint64_t v = x | (y << 16) | (z << 32);
//     uint64_t adc = data.u64[1] & 0x800000000000ul;

//     gs::write_internal(gif->hw.gs, adc ? gs::XYZ3 : gs::XYZ2, v);
// }

// void gif_write_fog(Gif* gif, uint128_t data) {
//     gs::write_internal(gif->hw.gs, gs::FOG, data.u64[1] << 20);
// }

void gif_handle_tag(Gif* gif, uint128_t data) {
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

    if (gif->tag.nregs == 0)
        gif->tag.nregs = 16;

    switch (gif->tag.fmt) {
        case 0: {
            gif->tag.remaining = gif->tag.nregs * gif->tag.nloop;
            gif->tag.qwc = gif->tag.nloop * gif->tag.nregs;
        } break;
        case 1: {
            gif->tag.remaining = gif->tag.nregs * gif->tag.nloop;
            gif->tag.qwc = (gif->tag.nloop * gif->tag.nregs + 1) / 2;
        } break;
        case 2:
        case 3: {
            gif->tag.remaining = gif->tag.nloop;
            gif->tag.qwc = gif->tag.nloop;
        } break;
    }

    // iris_debug(gif, "giftag: nloop={:04x} eop={} prim={:04x} (pre={}) fmt={} nregs={} reg={:08x}{:08x} size={}", //     gif->tag.nloop, gif->tag.eop, gif->tag.prim, gif->tag.pre, gif->tag.fmt, gif->tag.nregs, gif->tag.reg >> 32, gif->tag.reg & 0xffffffff, gif->tag.qwc
    //);

    // if (gif->tag.pre) {
    //     gs::write_internal(gif->hw.gs, gs::PRIM, gif->tag.prim);
    // }

    if (gif->tag.remaining) {
        gif->state = State::PROCESSING;
    }
}

// void gif_handle_packed(Gif* gif, uint128_t data) {
//     int index = (gif->tag.index++) % gif->tag.nregs;
//     int r = (gif->tag.reg >> (index * 4)) & 0xf;

//     switch (r) {
//         case 0x00: /* iris_debug(gif, "PRIM <- {:016x}", data.u64[0]); */ gs::write_internal(gif->hw.gs, gs::PRIM, data.u64[0] & 0x3ff); break;
//         case 0x01: /* iris_debug(gif, "RGBAQ <- {:016x}", data.u64[0]); */ gif_write_rgbaq(gif, data); break;
//         case 0x02: /* iris_debug(gif, "STQ <- {:016x}", data.u64[0]); */ gif_write_stq(gif, data); break;
//         case 0x03: /* iris_debug(gif, "UV <- {:016x}", data.u64[0]); */ gif_write_uv(gif, data); break;
//         case 0x04: /* iris_debug(gif, "XYZF23 <- {:08x}{:08x} {:08x}{:08x}", data.u32[3], data.u32[2], data.u32[1], data.u32[0]); */ gif_write_xyzf23(gif, data); break;
//         case 0x05: /* iris_debug(gif, "XYZ23 <- {:016x}", data.u64[0]); */ gif_write_xyz23(gif, data); break;
//         case 0x06: /* iris_debug(gif, "TEX0_1 <- {:016x}", data.u64[0]); */ gs::write_internal(gif->hw.gs, gs::TEX0_1, data.u64[0]); break;
//         case 0x07: /* iris_debug(gif, "TEX0_2 <- {:016x}", data.u64[0]); */ gs::write_internal(gif->hw.gs, gs::TEX0_2, data.u64[0]); break;
//         case 0x08: /* iris_debug(gif, "CLAMP_1 <- {:016x}", data.u64[0]); */ gs::write_internal(gif->hw.gs, gs::CLAMP_1, data.u64[0]); break;
//         case 0x09: /* iris_debug(gif, "CLAMP_2 <- {:016x}", data.u64[0]); */ gs::write_internal(gif->hw.gs, gs::CLAMP_2, data.u64[0]); break;
//         case 0x0a: /* iris_debug(gif, "FOG <- {:016x}", data.u64[0]); */ gif_write_fog(gif, data); break;
//         case 0x0c: /* iris_debug(gif, "XYZF3 <- {:016x}", data.u64[0]); */ gs::write_internal(gif->hw.gs, gs::XYZF3, data.u64[0]); break;
//         case 0x0d: /* iris_debug(gif, "XYZ3 <- {:016x}", data.u64[0]); */ gs::write_internal(gif->hw.gs, gs::XYZ3, data.u64[0]); break;

//         // A+D
//         case 0x0e: {
//             // iris_debug(gif, "write {} (A+D)", gif_get_reg_name(data.u64[1]));
//             gs::write_internal(gif->hw.gs, data.u64[1], data.u64[0]); 
//         } break;

//         // NOP
//         case 0x0f: break;

//         default: /* iris_fatal_error(gif, "PACKED format for reg {} unimplemented", r); */ break;
//     }

//     gif->tag.qwc--;

//     if (gif->tag.qwc == 0) {
//         gif->state = STATE_RECV_TAG;

//         return;
//     }
// }

// void gif_handle_reglist(Gif* gif, uint128_t data) {
//     for (int i = 0; i < 2; i++) {
//         int index = (gif->tag.index++) % gif->tag.nregs;
//         int r = (gif->tag.reg >> (index * 4)) & 0xf;

//         switch (r) {
//             case 0x00: gs::write_internal(gif->hw.gs, gs::PRIM, data.u64[i]); break;
//             case 0x01: gs::write_internal(gif->hw.gs, gs::RGBAQ, data.u64[i]); break;
//             case 0x02: gs::write_internal(gif->hw.gs, gs::ST, data.u64[i]); break;
//             case 0x03: gs::write_internal(gif->hw.gs, gs::UV, data.u64[i]); break;
//             case 0x04: gs::write_internal(gif->hw.gs, gs::XYZF2, data.u64[i]); break;
//             case 0x05: gs::write_internal(gif->hw.gs, gs::XYZ2, data.u64[i]); break;
//             case 0x06: gs::write_internal(gif->hw.gs, gs::TEX0_1, data.u64[i]); break;
//             case 0x07: gs::write_internal(gif->hw.gs, gs::TEX0_2, data.u64[i]); break;
//             case 0x08: gs::write_internal(gif->hw.gs, gs::CLAMP_1, data.u64[i]); break;
//             case 0x09: gs::write_internal(gif->hw.gs, gs::CLAMP_2, data.u64[i]); break;
//             case 0x0a: gs::write_internal(gif->hw.gs, gs::FOG, data.u64[i]); break;
//             case 0x0c: gs::write_internal(gif->hw.gs, gs::XYZF3, data.u64[i]); break;
//             case 0x0d: gs::write_internal(gif->hw.gs, gs::XYZ3, data.u64[i]); break;

//             // A+D
//             // NOP
//             case 0x0e:
//             case 0x0f: break;

//             // default: iris_debug(gif, "REGLIST format for reg {} unimplemented", r); break;
//         }

//         // Note: This handles odd NREGS*NLOOP case
//         if (gif->tag.index == gif->tag.remaining)
//             break;
//     }

//     gif->tag.qwc--;

//     if (gif->tag.qwc == 0) {
//         gif->state = STATE_RECV_TAG;

//         return;
//     }
// }

// void gif_handle_image(Gif* gif, uint128_t data) {
//     gs::write_internal(gif->hw.gs, gs::HWREG, data.u64[0]);
//     gs::write_internal(gif->hw.gs, gs::HWREG, data.u64[1]);
    
//     gif->tag.qwc--;

//     if (gif->tag.qwc == 0) {
//         gif->state = STATE_RECV_TAG;
//     }
// }

void write128(Gif* gif, uint32_t addr, uint128_t data) {
    fifo_write(gif, data, PATH3);
}

constexpr uint64_t GIF_BULK_MIN_QWORDS = 8;

static inline void gif_write_qword(Gif* gif, uint128_t data, int path) {
    if (gif->state == State::RECV_TAG) {
        queue::push128(gif->queue[path], data);

        gif_handle_tag(gif, data);

        return;
    }

    if (gif->tag.qwc) {
        queue::Queue* queue = gif->queue[path];

        queue::push128(queue, data);

        gif->tag.qwc--;

        if (!gif->tag.qwc) {
            gif->state = State::RECV_TAG;

            size_t bytes = queue::size(queue) * sizeof(uint32_t);

            // While PATH3 is masked, hold completed PATH3 packets until the
            // mask's falling edge so PATH1/PATH2 draws that sample the target
            // region see the pre-upload contents (double-buffered texture
            // streaming in OutRun2 SP, SSX On Tour, etc).
            int deferred = path == PATH3 && gif_path3_masked(gif) && !gif->p3_stall_enable;

            if (deferred) {
                gif_defer_path3(gif, queue->buf.data(), bytes);
            } else {
                gif_send_transfer(gif, path, queue->buf.data(), bytes);
            }

            queue::clear(queue);
        }
    }
}

static inline void gif_note_fifo_activity(Gif* gif) {
    gif->stat |= 0x1f000000;

    if (!gif->report_fifo_activity) {
        return;
    }

    if (!gif->fifo_activity.load(std::memory_order_relaxed)) {
        gif->fifo_activity.store(0x1f000000, std::memory_order_relaxed);
    }
}

void fifo_write(Gif* gif, uint128_t data, int path) {
    if (gif->hw.mtvu) {
        gif->stat |= 0x1f000000;

        scan_front_qwords(gif, path, (const uint8_t*)&data, 1);

        mtvu::push_gif_qwords(gif->hw.mtvu, path, (const uint8_t*)&data, 1);

        return;
    }

    profile::count((profile::Counter)(profile::GIF_PATH1_QWORDS + path));

    gif_note_fifo_activity(gif);

    gif_write_qword(gif, data, path);
}

void fifo_write_qwords(Gif* gif, const uint8_t* data, uint32_t count, int path) {
    if (gif->hw.mtvu) {
        gif->stat |= 0x1f000000;

        scan_front_qwords(gif, path, data, count);

        mtvu::push_gif_qwords(gif->hw.mtvu, path, data, count);

        return;
    }

    profile::count((profile::Counter)(profile::GIF_PATH1_QWORDS + path), count);

    gif_note_fifo_activity(gif);

    uint32_t index = 0;

    while (index < count) {
        uint64_t remaining = count - index;

        bool bulk = gif->state == State::PROCESSING
                 && gif->tag.qwc > GIF_BULK_MIN_QWORDS
                 && remaining > GIF_BULK_MIN_QWORDS;

        if (bulk) {
            uint64_t run = gif->tag.qwc - 1;

            if (run > remaining) {
                run = remaining;
            }

            queue::push_words(gif->queue[path], data + (size_t)index * 16, (size_t)run * 4);

            gif->tag.qwc -= run;
            index += (uint32_t)run;

            continue;
        }

        uint128_t qword;

        memcpy(&qword, data + (size_t)index * 16, sizeof(qword));

        gif_write_qword(gif, qword, path);

        index++;
    }
}

void set_backend(Gif* gif, void* udata, void (*transfer)(void*, int, const void*, size_t), void (*readback)(void*, void*, size_t)) {
    gif->udata = udata;
    gif->transfer = transfer;
    gif->readback = readback;

    if (gif->hw.mtvu) {
        mtvu::update_gif_backend(gif->hw.mtvu);
    }
}

constexpr uint64_t GIF_AD_DESCRIPTOR = 0xe;
constexpr uint64_t GIF_REG_SIGNAL = 0x60;
constexpr uint64_t GIF_REG_FINISH = 0x61;
constexpr uint64_t GIF_REG_LABEL = 0x62;

static void scan_apply_events(Gif* gif) {
    int events = gif->scan.events;

    gif->scan.events = 0;

    if (gif->scan.path == PATH3 && gif_path3_masked(gif)) {
        return;
    }

    for (int index = 0; index < events; index++) {
        const FrontScanEvent& event = gif->scan.event[index];

        switch (event.type) {
            case gs::SIGNAL_EVENT_SIGNAL: {
                gs::apply_signal(gif->hw.gs, event.data);
            } break;

            case gs::SIGNAL_EVENT_FINISH: {
                gs::apply_finish(gif->hw.gs, event.data);
            } break;

            case gs::SIGNAL_EVENT_LABEL: {
                gs::apply_label(gif->hw.gs, event.data);
            } break;
        }
    }
}

static void scan_record_event(Gif* gif, int type, uint64_t data) {
    if (gif->scan.events == FRONT_SCAN_EVENTS) {
        scan_apply_events(gif);
    }

    gif->scan.event[gif->scan.events].type = type;
    gif->scan.event[gif->scan.events].data = data;

    gif->scan.events++;
}

static void scan_packed_qword(Gif* gif, const uint128_t& qword) {
    uint64_t descriptor = (gif->scan.regs >> (gif->scan.index * 4)) & 0xf;

    gif->scan.index++;

    if (gif->scan.index == gif->scan.nregs) {
        gif->scan.index = 0;
    }

    if (descriptor != GIF_AD_DESCRIPTOR) {
        return;
    }

    switch (qword.u64[1] & 0x7f) {
        case GIF_REG_SIGNAL: {
            scan_record_event(gif, gs::SIGNAL_EVENT_SIGNAL, qword.u64[0]);
        } break;

        case GIF_REG_FINISH: {
            scan_record_event(gif, gs::SIGNAL_EVENT_FINISH, qword.u64[0]);
        } break;

        case GIF_REG_LABEL: {
            scan_record_event(gif, gs::SIGNAL_EVENT_LABEL, qword.u64[0]);
        } break;
    }
}

static bool scan_tag_has_ad(uint64_t regs, int nregs) {
    for (int index = 0; index < nregs; index++) {
        if (((regs >> (index * 4)) & 0xf) == GIF_AD_DESCRIPTOR) {
            return true;
        }
    }

    return false;
}

static void scan_tag(Gif* gif, const uint128_t& qword, int path) {
    uint64_t nloop = qword.u64[0] & 0x7fff;
    uint64_t nregs = (qword.u64[0] >> 60) & 0xf;

    if (!nregs) {
        nregs = 16;
    }

    gif->scan.fmt = (int)((qword.u64[0] >> 58) & 3);
    gif->scan.nregs = (int)nregs;
    gif->scan.regs = qword.u64[1];
    gif->scan.index = 0;
    gif->scan.path = path;

    switch (gif->scan.fmt) {
        case 0: {
            gif->scan.qwc = nloop * nregs;
        } break;

        case 1: {
            gif->scan.qwc = (nloop * nregs + 1) / 2;
        } break;

        default: {
            gif->scan.qwc = nloop;
        } break;
    }

    gif->scan.scannable = !gif->scan.fmt && scan_tag_has_ad(gif->scan.regs, gif->scan.nregs);
    gif->scan.state = gif->scan.qwc ? State::PROCESSING : State::RECV_TAG;
}

void enable_front_scan(Gif* gif) {
    gif->scan.enabled = 1;
}

int flushing_deferred_path3(Gif* gif) {
    return gif->flushing_deferred_path3;
}

void scan_front_qwords(Gif* gif, int path, const uint8_t* data, uint32_t qwords) {
    if (!gif->scan.enabled) {
        return;
    }

    for (uint32_t index = 0; index < qwords; index++) {
        uint128_t qword;

        memcpy(&qword, data + (size_t)index * 16, sizeof(qword));

        if (gif->scan.state == State::RECV_TAG) {
            scan_tag(gif, qword, path);

            if (gif->scan.state == State::RECV_TAG) {
                scan_apply_events(gif);
            }

            continue;
        }

        if (!gif->scan.scannable) {
            uint64_t remaining = qwords - index;

            if (remaining > gif->scan.qwc) {
                remaining = gif->scan.qwc;
            }

            gif->scan.qwc -= remaining;
            index += (uint32_t)remaining - 1;
        } else {
            scan_packed_qword(gif, qword);

            gif->scan.qwc--;
        }

        if (!gif->scan.qwc) {
            gif->scan.state = State::RECV_TAG;

            scan_apply_events(gif);
        }
    }
}

void sync_backend(Gif* gif) {
    if (gif->hw.mtvu) {
        mtvu::sync(gif->hw.mtvu, mtvu::SYNC_OTHER);
    }
}

void set_dump_tap(Gif* gif, void* udata, void (*tap)(void*, int, const void*, size_t)) {
    gif->dump_udata = udata;
    gif->dump_transfer = tap;

    if (gif->hw.mtvu) {
        mtvu::update_gif_backend(gif->hw.mtvu);
    }
}

void set_path3_mask(Gif* gif, int mask) {
    int prev = gif_path3_masked(gif);

    gif->mask_m3p = mask ? 1 : 0;

    if (mask) {
        gif->stat |= 2;
    } else {
        gif->stat &= ~2;
    }

    if (gif->hw.mtvu) {
        return;
    }

    if (prev && !gif_path3_masked(gif)) {
        gif_path3_lifted(gif);
    }
}

uint64_t get_transfer_hash(Gif* gif) {
    if (gif->hw.mtvu) {
        return mtvu::get_gif_transfer_hash(gif->hw.mtvu);
    }

    return gif->transfer_hash;
}

int get_path3_mask(Gif* gif) {
    return gif_path3_masked(gif);
}

#undef printf

}
