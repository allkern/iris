#ifndef GS_H
#define GS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"
#include "sched.h"

#define GS_PRIM       0x00
#define GS_RGBAQ      0x01
#define GS_ST         0x02
#define GS_UV         0x03
#define GS_XYZF2      0x04
#define GS_XYZ2       0x05
#define GS_TEX0_1     0x06
#define GS_TEX0_2     0x07
#define GS_CLAMP_1    0x08
#define GS_CLAMP_2    0x09
#define GS_FOG        0x0A
#define GS_XYZF3      0x0C
#define GS_XYZ3       0x0D
#define GS_TEX1_1     0x14
#define GS_TEX1_2     0x15
#define GS_TEX2_1     0x16
#define GS_TEX2_2     0x17
#define GS_XYOFFSET_1 0x18
#define GS_XYOFFSET_2 0x19
#define GS_PRMODECONT 0x1A
#define GS_PRMODE     0x1B
#define GS_TEXCLUT    0x1C
#define GS_SCANMSK    0x22
#define GS_MIPTBP1_1  0x34
#define GS_MIPTBP1_2  0x35
#define GS_MIPTBP2_1  0x36
#define GS_MIPTBP2_2  0x37
#define GS_TEXA       0x3B
#define GS_FOGCOL     0x3D
#define GS_TEXFLUSH   0x3F
#define GS_SCISSOR_1  0x40
#define GS_SCISSOR_2  0x41
#define GS_ALPHA_1    0x42
#define GS_ALPHA_2    0x43
#define GS_DIMX       0x44
#define GS_DTHE       0x45
#define GS_COLCLAMP   0x46
#define GS_TEST_1     0x47
#define GS_TEST_2     0x48
#define GS_PABE       0x49
#define GS_FBA_1      0x4A
#define GS_FBA_2      0x4B
#define GS_FRAME_1    0x4C
#define GS_FRAME_2    0x4D
#define GS_ZBUF_1     0x4E
#define GS_ZBUF_2     0x4F
#define GS_BITBLTBUF  0x50
#define GS_TRXPOS     0x51
#define GS_TRXREG     0x52
#define GS_TRXDIR     0x53
#define GS_HWREG      0x54
#define GS_SIGNAL     0x60
#define GS_FINISH     0x61
#define GS_LABEL      0x62

struct ps2_gs;

struct gs_renderer {
    void (*render_point)(struct ps2_gs*, void*);
    void (*render_line)(struct ps2_gs*, void*);
    void (*render_line_strip)(struct ps2_gs*, void*);
    void (*render_triangle)(struct ps2_gs*, void*);
    void (*render_triangle_strip)(struct ps2_gs*, void*);
    void (*render_triangle_fan)(struct ps2_gs*, void*);
    void (*render_sprite)(struct ps2_gs*, void*);
    void (*render)(struct ps2_gs*, void*);
    // void (*transfer_start)(struct ps2_gs*, void*);
    // void (*transfer_write)(struct ps2_gs*, void*);
    // void (*transfer_read)(struct ps2_gs*, void*);
    void* udata;
};

struct gs_vertex {
    uint64_t rgbaq;
    uint64_t xyz2;
};

struct gs_callback {
    void (*func)(void*);
    void* udata;
};

#define GS_EVENT_VBLANK 0
#define GS_EVENT_SCISSOR 1

struct ps2_gs {
    struct gs_renderer backend;

    uint32_t* vram;

    uint64_t pmode;
    uint64_t smode1;
    uint64_t smode2;
    uint64_t srfsh;
    uint64_t synch1;
    uint64_t synch2;
    uint64_t syncv;
    uint64_t dispfb1;
    uint64_t display1;
    uint64_t dispfb2;
    uint64_t display2;
    uint64_t extbuf;
    uint64_t extdata;
    uint64_t extwrite;
    uint64_t bgcolor;
    uint64_t csr;
    uint64_t imr;
    uint64_t busdir;
    uint64_t siglblid;

    // Internal registers
    uint64_t prim;
    uint64_t rgbaq;
    uint64_t st;
    uint64_t uv;
    uint64_t xyzf2;
    uint64_t xyz2;
    uint64_t tex0_1;
    uint64_t tex0_2;
    uint64_t clamp_1;
    uint64_t clamp_2;
    uint64_t fog;
    uint64_t xyzf3;
    uint64_t xyz3;
    uint64_t tex1_1;
    uint64_t tex1_2;
    uint64_t tex2_1;
    uint64_t tex2_2;
    uint64_t xyoffset_1;
    uint64_t xyoffset_2;
    uint64_t prmodecont;
    uint64_t prmode;
    uint64_t texclut;
    uint64_t scanmsk;
    uint64_t miptbp1_1;
    uint64_t miptbp1_2;
    uint64_t miptbp2_1;
    uint64_t miptbp2_2;
    uint64_t texa;
    uint64_t fogcol;
    uint64_t texflush;
    uint64_t scissor_1;
    uint64_t scissor_2;
    uint64_t alpha_1;
    uint64_t alpha_2;
    uint64_t dimx;
    uint64_t dthe;
    uint64_t colclamp;
    uint64_t test_1;
    uint64_t test_2;
    uint64_t pabe;
    uint64_t fba_1;
    uint64_t fba_2;
    uint64_t frame_1;
    uint64_t frame_2;
    uint64_t zbuf_1;
    uint64_t zbuf_2;
    uint64_t bitbltbuf;
    uint64_t trxpos;
    uint64_t trxreg;
    uint64_t trxdir;
    uint64_t hwreg;
    uint64_t signal;
    uint64_t finish;
    uint64_t label;

    // Ongoing transfer
    unsigned int sbp, dbp;
    unsigned int sbw, dbw;
    unsigned int spsm, dpsm;
    unsigned int ssax, ssay, dsax, dsay;
    unsigned int rrh, rrw;
    unsigned int dir, xdir;
    unsigned int sx, sy;
    unsigned int dx, dy;

    // Drawing data
    struct gs_vertex vq[4];
    unsigned int vqi;

    struct gs_callback events[2];

    struct sched_state* sched;
    struct ps2_intc* ee_intc;
    struct ps2_iop_intc* iop_intc;
};

struct ps2_gs* ps2_gs_create(void);
void ps2_gs_init(struct ps2_gs* gs, struct ps2_intc* ee_intc, struct ps2_iop_intc* iop_intc, struct sched_state* sched);
void ps2_gs_init_renderer(struct ps2_gs* gs, struct gs_renderer renderer);
void ps2_gs_destroy(struct ps2_gs* gs);
uint64_t ps2_gs_read64(struct ps2_gs* gs, uint32_t addr);
void ps2_gs_write64(struct ps2_gs* gs, uint32_t addr, uint64_t data);
void ps2_gs_write_internal(struct ps2_gs* gs, int reg, uint64_t data);
uint64_t ps2_gs_read_internal(struct ps2_gs* gs, int reg);
void ps2_gs_init_callback(struct ps2_gs* gs, int event, void (*func)(void*), void* udata);

void gs_write_vertex(struct ps2_gs* gs, uint64_t data, int discard);

#ifdef __cplusplus
}
#endif

#endif