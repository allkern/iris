#pragma once

#include <atomic>

#include "u128.h"
#include "queue.hpp"
#include "logger.hpp"

namespace iris::gs { struct Gs; }

namespace iris::vu { struct Vu; }

namespace iris::ee::dmac { struct Dmac; }

namespace iris::mtvu { struct Mtvu; }

namespace iris::gif {

enum State {
    RECV_TAG,
    PROCESSING
};

enum PathId : int {
    PATH1,
    PATH2,
    PATH3
};

struct Tag {
    uint64_t nloop;
    uint32_t prim;
    int eop;
    int pre;
    int fmt;
    int nregs;
    uint64_t reg;
    uint64_t qwc;

    int index;
    int remaining;
};

inline constexpr int FRONT_SCAN_EVENTS = 8;

struct FrontScanEvent {
    int type;
    uint64_t data;
};

struct FrontScan {
    int enabled;
    int state;
    int fmt;
    int scannable;
    int nregs;
    int index;
    uint64_t regs;
    uint64_t qwc;
    int path;

    int events;
    FrontScanEvent event[FRONT_SCAN_EVENTS];
};

struct Gif {
    struct {
        ee::dmac::Dmac* dmac;
        gs::Gs* gs;
        vu::Vu* vu1;
        mtvu::Mtvu* mtvu;
    } hw;

    uint64_t ctrl;
    uint64_t mode;
    uint64_t stat;
    uint64_t tag0;
    uint64_t tag1;
    uint64_t tag2;
    uint64_t tag3;
    uint64_t cnt;
    uint64_t p3cnt;
    uint64_t p3tag;

    // Renderer state
    void* udata;
    void (*transfer)(void*, int, const void*, size_t);
    void (*readback)(void*, void*, size_t);
    queue::Queue* queue[3];

    // GS dump stuff
    void* dump_udata;
    void (*dump_transfer)(void*, int, const void*, size_t);

    int state;
    Tag tag;

    int mask_m3r;
    int mask_m3p;
    int path3_mask_enable;

    int p3_stall_enable;
    int p3_resuming;
    int p3_refuse;
    uint8_t p3_fifo[16 * 16];
    uint32_t p3_fifo_qwords;
    int p3_draining;

    uint64_t trx_bitbltbuf;
    uint64_t trx_trxreg;
    uint32_t download_qwords;
    int download_notify;

    uint8_t* p3_defer_buf;
    size_t p3_defer_size;
    size_t p3_defer_cap;

    // From ST(Q) to RGBA(Q)
    uint64_t q;

    bool report_fifo_activity;
    std::atomic <uint32_t> fifo_activity;

    FrontScan scan;
    int flushing_deferred_path3;

    uint64_t p3_left;
    int p3_eop;
    uint64_t p2_left;
    int p2_eop;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Gif* create(logger::Logger* logger);
void connect(Gif* gif, ee::dmac::Dmac* dmac, vu::Vu* vu1, gs::Gs* gs);
void reset(Gif* gif);
void destroy(Gif* gif);
uint64_t read32(Gif* gif, uint32_t addr);
void write32(Gif* gif, uint32_t addr, uint64_t data);
void write128(Gif* gif, uint32_t addr, uint128_t data);
void fifo_write(Gif* gif, uint128_t data, int path);
void fifo_write_qwords(Gif* gif, const uint8_t* data, uint32_t count, int path);
uint128_t fifo_read(Gif* gif);
void set_backend(Gif* gif, void* udata, void (*transfer)(void*, int, const void*, size_t), void (*readback)(void*, void*, size_t));
void set_dump_tap(Gif* gif, void* udata, void (*tap)(void*, int, const void*, size_t));
void sync_backend(Gif* gif);
void enable_front_scan(Gif* gif);
void scan_front_qwords(Gif* gif, int path, const uint8_t* data, uint32_t qwords);
int flushing_deferred_path3(Gif* gif);
void set_path3_mask(Gif* gif, int mask);
int get_path3_mask(Gif* gif);
int can_accept(Gif* gif, int path);
int path3_stall_enabled(Gif* gif);
bool path3_packet_open(Gif* gif);
bool path2_packet_open(Gif* gif);
uint64_t path3_packet_qwords(Gif* gif);
int path3_refusal(Gif* gif);
uint32_t path3_fifo_space(Gif* gif);
uint32_t download_remaining(Gif* gif);
uint32_t read_download(Gif* gif, void* dst, uint32_t qwords);
bool path3_fifo_holding(Gif* gif);
bool path3_packet_started(Gif* gif);
void path3_fifo_push(Gif* gif, const uint8_t* data, uint32_t qwords);

}
