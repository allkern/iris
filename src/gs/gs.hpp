#pragma once



#include "u128.h"
#include "scheduler.hpp"
#include "ee/timers.hpp"
#include "iop/timers.hpp"
#include "logger.hpp"

namespace iris::gs {

inline constexpr auto PRIM = 0x00;
inline constexpr auto RGBAQ = 0x01;
inline constexpr auto ST = 0x02;
inline constexpr auto UV = 0x03;
inline constexpr auto XYZF2 = 0x04;
inline constexpr auto XYZ2 = 0x05;
inline constexpr auto TEX0_1 = 0x06;
inline constexpr auto TEX0_2 = 0x07;
inline constexpr auto CLAMP_1 = 0x08;
inline constexpr auto CLAMP_2 = 0x09;
inline constexpr auto FOG = 0x0A;
inline constexpr auto XYZF3 = 0x0C;
inline constexpr auto XYZ3 = 0x0D;
inline constexpr auto TEX1_1 = 0x14;
inline constexpr auto TEX1_2 = 0x15;
inline constexpr auto TEX2_1 = 0x16;
inline constexpr auto TEX2_2 = 0x17;
inline constexpr auto XYOFFSET_1 = 0x18;
inline constexpr auto XYOFFSET_2 = 0x19;
inline constexpr auto PRMODECONT = 0x1A;
inline constexpr auto PRMODE = 0x1B;
inline constexpr auto TEXCLUT = 0x1C;
inline constexpr auto SCANMSK = 0x22;
inline constexpr auto MIPTBP1_1 = 0x34;
inline constexpr auto MIPTBP1_2 = 0x35;
inline constexpr auto MIPTBP2_1 = 0x36;
inline constexpr auto MIPTBP2_2 = 0x37;
inline constexpr auto TEXA = 0x3B;
inline constexpr auto FOGCOL = 0x3D;
inline constexpr auto TEXFLUSH = 0x3F;
inline constexpr auto SCISSOR_1 = 0x40;
inline constexpr auto SCISSOR_2 = 0x41;
inline constexpr auto ALPHA_1 = 0x42;
inline constexpr auto ALPHA_2 = 0x43;
inline constexpr auto DIMX = 0x44;
inline constexpr auto DTHE = 0x45;
inline constexpr auto COLCLAMP = 0x46;
inline constexpr auto TEST_1 = 0x47;
inline constexpr auto TEST_2 = 0x48;
inline constexpr auto PABE = 0x49;
inline constexpr auto FBA_1 = 0x4A;
inline constexpr auto FBA_2 = 0x4B;
inline constexpr auto FRAME_1 = 0x4C;
inline constexpr auto FRAME_2 = 0x4D;
inline constexpr auto ZBUF_1 = 0x4E;
inline constexpr auto ZBUF_2 = 0x4F;
inline constexpr auto BITBLTBUF = 0x50;
inline constexpr auto TRXPOS = 0x51;
inline constexpr auto TRXREG = 0x52;
inline constexpr auto TRXDIR = 0x53;
inline constexpr auto HWREG = 0x54;
inline constexpr auto SIGNAL = 0x60;
inline constexpr auto FINISH = 0x61;
inline constexpr auto LABEL = 0x62;

// TCC
inline constexpr auto RGB = 0;
inline constexpr auto RGBA = 1;

inline constexpr auto IIP = (1 << 3);// int0:1:0 Shading Method
inline constexpr auto TME = (1 << 4);// int0:1:0 Texture Mapping
inline constexpr auto FGE = (1 << 5);// int0:1:0 Fogging
inline constexpr auto ABE = (1 << 6);// int0:1:0 Alpha Blending
inline constexpr auto AA1 = (1 << 7);// int0:1:0 1 Pass Antialiasing (*1)
inline constexpr auto FST = (1 << 8);// int0:1:0 Method of Specifying Texture Coordinates (*2)
inline constexpr auto CTXT = (1 << 9);// int0:1:0 Context
inline constexpr auto FIX = (1 << 10);// int0:1:0 Fragment Value Control (RGBAFSTQ Change by DDA)

// Framebuffer/Pixel formats
inline constexpr auto PSMCT32 = 0x00;
inline constexpr auto PSMCT24 = 0x01;
inline constexpr auto PSMCT16 = 0x02;
inline constexpr auto PSMCT16S = 0x0a;
inline constexpr auto PSMZ32 = 0x30;
inline constexpr auto PSMZ24 = 0x31;
inline constexpr auto PSMZ16 = 0x32;
inline constexpr auto PSMZ16S = 0x3a;
inline constexpr auto PSMT8 = 0x13;
inline constexpr auto PSMT4 = 0x14;
inline constexpr auto PSMT8H = 0x1b;
inline constexpr auto PSMT4HL = 0x24;
inline constexpr auto PSMT4HH = 0x2c;

// Z buffer formats
inline constexpr auto ZSMZ32 = 0x00;
inline constexpr auto ZSMZ24 = 0x01;
inline constexpr auto ZSMZ16 = 0x02;
inline constexpr auto ZSMZ16S = 0x0a;

// Texture function
inline constexpr auto MODULATE = 0;
inline constexpr auto DECAL = 1;
inline constexpr auto HIGHLIGHT = 2;
inline constexpr auto HIGHLIGHT2 = 3;

// Timings
inline constexpr auto FRAME_SCANS_NTSC = 240;
inline constexpr auto VBLANK_SCANS_NTSC = 22;
inline constexpr auto SCANLINE_NTSC = 9370;
inline constexpr auto FRAME_SCANS_PAL = 286;
inline constexpr auto VBLANK_SCANS_PAL = 26;
inline constexpr auto SCANLINE_PAL = 9476;

// EE clock: 294.912 MHz, 294912000 clocks/s
// 294912000/60=4915200 clocks/frame

// #define FRAME_NTSC (4497600) // (240 * 9370)
// #define VBLANK_NTSC (417600) // (22 * 9370)
inline constexpr auto EE_CLOCK = 294912000;
#define EE_CLOCKS_PER_FRAME_NTSC (EE_CLOCK / 60)
#define EE_CLOCKS_PER_SCAN_NTSC (EE_CLOCKS_PER_FRAME_NTSC / 262)
inline constexpr auto EE_CLOCKS_PER_VFRAME = (EE_CLOCKS_PER_SCAN_NTSC * 240);
inline constexpr auto EE_CLOCKS_PER_VBLANK = (EE_CLOCKS_PER_SCAN_NTSC * 22);
inline constexpr auto FRAME_NTSC = 4502400;// (240 * 9370)
inline constexpr auto VBLANK_NTSC = 412720;// (22 * 9370)
inline constexpr auto PMODE_EN1 = 1;
inline constexpr auto PMODE_EN2 = 2;
inline constexpr auto FRAME_PAL = (286 * 9476);
inline constexpr auto VBLANK_PAL = (26 * 9476);

struct Gs;

struct Vertex {
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
    int32_t x;
    int32_t y;
    uint32_t z;
    uint32_t u;
    uint32_t v;
    float s;
    float t;
    float q;
};

struct Callback {
    void (*func)(void*);
    void* udata;
};

struct Context {
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

inline constexpr auto EVENT_VBLANK = 0;
inline constexpr auto EVENT_SCISSOR = 1;

struct Gs {
    struct {
        scheduler::Scheduler* sched;
        ee::intc::Intc* ee_intc;
        iop::intc::Intc* iop_intc;
        ee::timers::Timers* ee_timers;
        iop::timers::Timers* iop_timers;
    } hw;

    uint32_t* vram;

    int vblank;

    // SIGNAL stuff
    int signal_pending;
    int signal_stall;
    uint32_t stall_sigid;

    // 1KB CLUT cache
    uint32_t clut_cache[0x100];
    uint32_t cbp0;
    uint32_t cbp1;

    uint32_t attr;
    Context context[2];
    Context* ctx;

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
    uint64_t csr_enable;

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
    Vertex vq[4];
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

    // DIMX
    int dither[4][4];


    int frame_cycles = FRAME_NTSC;
    int vblank_cycles = VBLANK_NTSC;
    int scanline_cycles = SCANLINE_NTSC;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Gs* create(logger::Logger* logger, iop::intc::Intc* iop_intc, iop::timers::Timers* iop_timers, scheduler::Scheduler* sched);
void connect(Gs* gs, ee::intc::Intc* ee_intc, ee::timers::Timers* ee_timers);
void reset(Gs* gs);
void set_ee_clock(Gs* gs, int hz);
void destroy(Gs* gs);
uint64_t read64(Gs* gs, uint32_t addr);
void write64(Gs* gs, uint32_t addr, uint64_t data);
int is_vblank(Gs* gs);
int is_display_enabled(const Gs* gs);

struct PrivilegedState {
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
};

void get_privileged_state(Gs* gs, PrivilegedState* state);

int write_signal(Gs* gs, uint64_t data);
int write_finish(Gs* gs, uint64_t data);
int write_label(Gs* gs, uint64_t data);

}
