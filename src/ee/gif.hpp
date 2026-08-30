#pragma once

#include "u128.h"
#include "queue.hpp"
#include "logger.hpp"

namespace iris::gs { struct Gs; }

namespace iris::vu { struct Vu; }

namespace iris::ee::dmac { struct Dmac; }

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

struct Gif {
    struct {
        ee::dmac::Dmac* dmac;
        gs::Gs* gs;
        vu::Vu* vu1;
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
    uint8_t* p3_defer_buf;
    size_t p3_defer_size;
    size_t p3_defer_cap;

    // From ST(Q) to RGBA(Q)
    uint64_t q;

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
uint128_t fifo_read(Gif* gif);
void set_backend(Gif* gif, void* udata, void (*transfer)(void*, int, const void*, size_t), void (*readback)(void*, void*, size_t));
void set_dump_tap(Gif* gif, void* udata, void (*tap)(void*, int, const void*, size_t));

void set_path3_mask(Gif* gif, int mask);
int get_path3_mask(Gif* gif);

}
