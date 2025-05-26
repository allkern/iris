#ifndef GS_H
#define GS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"
#include "sched.h"
#include "ee/timers.h"
#include "iop/timers.h"

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

// TCC
#define GS_RGB 0
#define GS_RGBA 1

#define GS_IIP (1 << 3) // int0:1:0 Shading Method
#define GS_TME (1 << 4) // int0:1:0 Texture Mapping
#define GS_FGE (1 << 5) // int0:1:0 Fogging
#define GS_ABE (1 << 6) // int0:1:0 Alpha Blending
#define GS_AA1 (1 << 7) // int0:1:0 1 Pass Antialiasing (*1)
#define GS_FST (1 << 8) // int0:1:0 Method of Specifying Texture Coordinates (*2)
#define GS_CTXT (1 << 9) // int0:1:0 Context
#define GS_FIX (1 << 10) // int0:1:0 Fragment Value Control (RGBAFSTQ Change by DDA)

// Framebuffer/Pixel formats
#define GS_PSMCT32 0x00
#define GS_PSMCT24 0x01
#define GS_PSMCT16 0x02
#define GS_PSMCT16S 0x0a
#define GS_PSMZ32 0x30
#define GS_PSMZ24 0x31
#define GS_PSMZ16 0x32
#define GS_PSMZ16S 0x3a
#define GS_PSMT8 0x13
#define GS_PSMT4 0x14
#define GS_PSMT8H 0x1b
#define GS_PSMT4HL 0x24
#define GS_PSMT4HH 0x2c

// Z buffer formats
#define GS_ZSMZ32 0x00
#define GS_ZSMZ24 0x01
#define GS_ZSMZ16 0x02
#define GS_ZSMZ16S 0x0a

// Texture function
#define GS_MODULATE 0
#define GS_DECAL 1
#define GS_HIGHLIGHT 2
#define GS_HIGHLIGHT2 3

// Timings
#define GS_FRAME_SCANS_NTSC 240
#define GS_VBLANK_SCANS_NTSC 22
#define GS_SCANLINE_NTSC 9370
#define GS_FRAME_SCANS_PAL 286
#define GS_VBLANK_SCANS_PAL 26
#define GS_SCANLINE_PAL 9476

// EE clock: 294.912 MHz, 294912000 clocks/s
// 294912000/60=4915200 clocks/frame

#define GS_FRAME_NTSC (240 * 9370)
#define GS_VBLANK_NTSC (22 * 9370)
#define GS_FRAME_PAL (286 * 9476)
#define GS_VBLANK_PAL (26 * 9476)

struct ps2_gs;

struct gs_renderer {
    void (*render_point)(struct ps2_gs*, void*);
    void (*render_line)(struct ps2_gs*, void*);
    void (*render_triangle)(struct ps2_gs*, void*);
    void (*render_sprite)(struct ps2_gs*, void*);
    void (*render)(struct ps2_gs*, void*);
    void (*transfer_start)(struct ps2_gs*, void*);
    void (*transfer_write)(struct ps2_gs*, void*);
    void (*transfer_read)(struct ps2_gs*, void*);
    void* udata;
};

struct gs_vertex {
    uint64_t rgbaq;
    uint64_t xyz;
    uint64_t st;
    uint64_t uv;
    uint64_t fog;

    // Cached fields
    uint32_t r;
    uint32_t g;
    uint32_t b;
    uint32_t a;
    int16_t x;
    int16_t y;
    uint32_t z;
    uint32_t u;
    uint32_t v;
    float s;
    float t;
    float q;
};

struct gs_callback {
    void (*func)(void*);
    void* udata;
};

struct gs_context {
    uint64_t frame; // (FRAME_1, FRAME_2)
    uint64_t zbuf; // (ZBUF_1, ZBUF_2)
    uint64_t tex0; // (TEX0_1, TEX0_2)
    uint64_t tex1; // (TEX1_1, TEX1_2)
    uint64_t tex2; // (TEX2_1, TEX2_2)
    uint64_t miptbp1; // (MIPTBP1_1, MIPTBP1_2)
    uint64_t miptbp2; // (MIPTBP2_1, MIPTBP2_2)
    uint64_t clamp; // (CLAMP_1, CLAMP_2)
    uint64_t test; // (TEST_1, TEST_2)
    uint64_t alpha; // (ALPHA_1, ALPHA_2)
    uint64_t xyoffset; // (XYOFFSET_1, XYOFFSET_2)
    uint64_t scissor; // (SCISSOR_1, SCISSOR_2)
    uint64_t fba; // (FBA_1, FBA_2)

    // Cached fields
    // FRAME
    uint32_t fbp;
    uint32_t fbw;
    uint32_t fbpsm;
    uint32_t fbmsk;

    // ZBUF
    uint32_t zbp;
    uint32_t zbpsm;
    uint32_t zbmsk;

    // TEX0
    uint32_t tbp0;
    uint32_t tbw;
    uint32_t tbpsm;
    uint32_t usize;
    uint32_t vsize;
    uint32_t tcc;
    uint32_t tfx;
    uint32_t cbp;
    uint32_t cbpsm;
    uint32_t csm;
    uint32_t csa;
    uint32_t cld;

    // TEX1
    uint32_t lcm;
    uint32_t mxl;
    uint32_t mmag;
    uint32_t mmin;
    uint32_t mtba;
    uint32_t l;
    uint32_t k;

    // MIPTBP1/2
    uint32_t mmtbp[6];
    uint32_t mmtbw[6];

    // CLAMP
    uint32_t wms;
    uint32_t wmt;
    uint32_t minu;
    uint32_t maxu;
    uint32_t minv;
    uint32_t maxv;

    // TEST
    uint32_t ate;
    uint32_t atst;
    uint32_t aref;
    uint32_t afail;
    uint32_t date;
    uint32_t datm;
    uint32_t zte;
    uint32_t ztst;

    // ALPHA
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t fix;

    // XYOFFSET
    uint32_t ofx;
    uint32_t ofy;

    // SCISSOR
    uint32_t scax0;
    uint32_t scax1;
    uint32_t scay0;
    uint32_t scay1;
};

#define GS_EVENT_VBLANK 0
#define GS_EVENT_SCISSOR 1

struct ps2_gs {
    struct gs_renderer backend;

    uint32_t* vram;

    // 1KB CLUT cache
    uint32_t clut_cache[0x100];
    uint32_t cbp0;
    uint32_t cbp1;

    uint32_t attr;
    struct gs_context context[2];
    struct gs_context* ctx;

    // Privileged registers
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
    uint64_t fog;
    uint64_t xyzf3;
    uint64_t xyz3;
    uint64_t prmodecont;
    uint64_t prmode;
    uint64_t texclut;
    uint64_t scanmsk;
    uint64_t texa;
    uint64_t fogcol;
    uint64_t texflush;
    uint64_t dimx;
    uint64_t dthe;
    uint64_t colclamp;
    uint64_t pabe;
    uint64_t bitbltbuf;
    uint64_t trxpos;
    uint64_t trxreg;
    uint64_t trxdir;
    uint64_t hwreg;
    uint64_t signal;
    uint64_t finish;
    uint64_t label;

    // Drawing data
    struct gs_vertex vq[4];
    unsigned int vqi;

    // Cached fields
    int iip;
    int tme;
    int fge;
    int abe;
    int aa1;
    int fst;
    int ctxt;
    int fix;

    // TEXCLUT
    uint32_t cbw;
    uint32_t cou;
    uint32_t cov;

    // TEXA
    int aem;
    uint32_t ta0;
    uint32_t ta1;

    // DISPFB1/2
    uint32_t dfbp1;
    uint32_t dfbw1;
    uint32_t dfbpsm1;
    uint32_t dfbp2;
    uint32_t dfbw2;
    uint32_t dfbpsm2;

    struct gs_callback events[2];

    struct sched_state* sched;
    struct ps2_intc* ee_intc;
    struct ps2_iop_intc* iop_intc;
    struct ps2_ee_timers* ee_timers;
    struct ps2_iop_timers* iop_timers;
};

struct ps2_gs* ps2_gs_create(void);
void ps2_gs_init(struct ps2_gs* gs, struct ps2_intc* ee_intc, struct ps2_iop_intc* iop_intc, struct ps2_ee_timers* ee_timers, struct ps2_iop_timers* iop_timers, struct sched_state* sched);
void ps2_gs_reset(struct ps2_gs* gs);
void ps2_gs_init_renderer(struct ps2_gs* gs, struct gs_renderer renderer);
void ps2_gs_destroy(struct ps2_gs* gs);
uint64_t ps2_gs_read64(struct ps2_gs* gs, uint32_t addr);
void ps2_gs_write64(struct ps2_gs* gs, uint32_t addr, uint64_t data);
void ps2_gs_write_internal(struct ps2_gs* gs, int reg, uint64_t data);
uint64_t ps2_gs_read_internal(struct ps2_gs* gs, int reg);
void ps2_gs_init_callback(struct ps2_gs* gs, int event, void (*func)(void*), void* udata);
struct gs_callback* ps2_gs_get_callback(struct ps2_gs* gs, int event);
void ps2_gs_remove_callback(struct ps2_gs* gs, int event);

void gs_write_vertex(struct ps2_gs* gs, uint64_t data, int discard);

#ifdef __cplusplus
}
#endif

#endif