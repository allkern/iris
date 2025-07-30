#ifndef GIF_H
#define GIF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "gs/gs.h"
#include "u128.h"
#include "vu.h"

#define GIF_STATE_RECV_TAG 0
#define GIF_STATE_PROCESSING 1

struct gif_tag {
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

struct ps2_gif {
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

    struct ps2_gs* gs;
    struct vu_state* vu1;

    int state;
    struct gif_tag tag;

    // From ST(Q) to RGBA(Q)
    uint64_t q;
};

struct ps2_gif* ps2_gif_create(void);
void ps2_gif_init(struct ps2_gif* gif, struct vu_state* vu1, struct ps2_gs* gs);
void ps2_gif_destroy(struct ps2_gif* gif);
uint64_t ps2_gif_read32(struct ps2_gif* gif, uint32_t addr);
void ps2_gif_write32(struct ps2_gif* gif, uint32_t addr, uint64_t data);
void ps2_gif_write128(struct ps2_gif* gif, uint32_t addr, uint128_t data);

#ifdef __cplusplus
}
#endif

#endif