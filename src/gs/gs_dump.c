#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gs_dump.h"

#define GS_DUMP_STATE_VERSION 9

#define GS_DUMP_REGSTATE_SIZE ( \
    4              /* version                     */ + \
    15 * 8         /* prim..trxreg + 1 dummy u64  */ + \
    2 * 12 * 8     /* two drawing contexts        */ + \
    8 + 8          /* rgbaq, st                   */ + \
    4 + 4          /* uv.words[0], fog.words[0]   */ + \
    8              /* dummy XYZ u64               */ + \
    4 * 4          /* dummy GIFReg + transfer X/Y */ + \
    61             /* version >= 9 trailing pad   */ )

#define GS_DUMP_PRIVREGS_SIZE 0x2000

enum {
    GS_DUMP_PACKET_TRANSFER = 0,
    GS_DUMP_PACKET_VSYNC = 1,
    GS_DUMP_PACKET_READFIFO = 2,
    GS_DUMP_PACKET_PRIVREGISTERS = 3
};

struct gs_dump_header {
    uint32_t version;
    uint32_t state_size;
    uint32_t serial_offset;
    uint32_t serial_size;
    uint32_t crc;
    uint32_t screenshot_width;
    uint32_t screenshot_height;
    uint32_t screenshot_offset;
    uint32_t screenshot_size;
};

struct gs_dump {
    FILE* file;
    int active;
};

struct gs_dump* gs_dump_create(void) {
    struct gs_dump* dump = malloc(sizeof(struct gs_dump));

    dump->file = NULL;
    dump->active = 0;

    return dump;
}

void gs_dump_destroy(struct gs_dump* dump) {
    if (!dump)
        return;

    if (dump->file)
        fclose(dump->file);

    free(dump);
}

static inline void write_data(struct gs_dump* dump, const void* data, size_t size) {
    fwrite(data, 1, size, dump->file);
}

static inline void write_u8(struct gs_dump* dump, uint8_t v) {
    fwrite(&v, sizeof(v), 1, dump->file);
}

static inline void write_u32(struct gs_dump* dump, uint32_t v) {
    fwrite(&v, sizeof(v), 1, dump->file);
}

static inline void write_u64(struct gs_dump* dump, uint64_t v) {
    fwrite(&v, sizeof(v), 1, dump->file);
}

static void write_register_state(struct gs_dump* dump, struct ps2_gs* gs) {
    write_u32(dump, GS_DUMP_STATE_VERSION);

    write_u64(dump, gs->prim);
    write_u64(dump, gs->prmodecont);
    write_u64(dump, gs->texclut);
    write_u64(dump, gs->scanmsk);
    write_u64(dump, gs->texa);
    write_u64(dump, gs->fogcol);
    write_u64(dump, gs->dimx);
    write_u64(dump, gs->dthe);
    write_u64(dump, gs->colclamp);
    write_u64(dump, gs->pabe);
    write_u64(dump, gs->bitbltbuf);
    write_u64(dump, gs->trxdir);
    write_u64(dump, gs->trxpos);
    write_u64(dump, gs->trxreg);
    write_u64(dump, 0); // dummy

    for (int i = 0; i < 2; i++) {
        struct gs_context* ctx = &gs->context[i];

        write_u64(dump, ctx->xyoffset);
        write_u64(dump, ctx->tex0);
        write_u64(dump, ctx->tex1);
        write_u64(dump, ctx->clamp);
        write_u64(dump, ctx->miptbp1);
        write_u64(dump, ctx->miptbp2);
        write_u64(dump, ctx->scissor);
        write_u64(dump, ctx->alpha);
        write_u64(dump, ctx->test);
        write_u64(dump, ctx->fba);
        write_u64(dump, ctx->frame);
        write_u64(dump, ctx->zbuf);
    }

    write_u64(dump, gs->rgbaq);
    write_u64(dump, gs->st);
    write_u32(dump, (uint32_t)gs->uv);
    write_u32(dump, (uint32_t)gs->fog);
    write_u64(dump, 0);
    write_u32(dump, 0);
    write_u32(dump, 0);
    write_u32(dump, 0);
    write_u32(dump, 0);

    uint8_t pad[61];
    memset(pad, 0, sizeof(pad));
    write_data(dump, pad, sizeof(pad));
}

static void write_priv_registers(struct gs_dump* dump, struct ps2_gs* gs) {
    struct gs_privileged_state state;

    gs_get_privileged_state(gs, &state);

    uint8_t buf[GS_DUMP_PRIVREGS_SIZE];
    memset(buf, 0, sizeof(buf));

    uint64_t* lo = (uint64_t*)buf;
    uint64_t* hi = (uint64_t*)(buf + 0x1000);

    lo[0]  = state.pmode;
    lo[2]  = state.smode1;
    lo[4]  = state.smode2;
    lo[6]  = state.srfsh;
    lo[8]  = state.synch1;
    lo[10] = state.synch2;
    lo[12] = state.syncv;
    lo[14] = state.dispfb1;
    lo[16] = state.display1;
    lo[18] = state.dispfb2;
    lo[20] = state.display2;
    lo[22] = state.extbuf;
    lo[24] = state.extdata;
    lo[26] = state.extwrite;
    lo[28] = state.bgcolor;

    hi[0]  = state.csr;
    hi[2]  = state.imr;
    hi[8]  = state.busdir;
    hi[16] = state.siglblid;

    write_data(dump, buf, sizeof(buf));
}

int gs_dump_begin(struct gs_dump* dump, const char* path,
    struct ps2_gs* gs, struct ps2_gif* gif,
    const void* vram, uint32_t vram_size,
    const char* serial, uint32_t crc) {
    if (dump->file)
        gs_dump_end(dump);

    dump->file = fopen(path, "wb");

    if (!dump->file)
        return 0;

    uint32_t serial_size = serial ? (uint32_t)strlen(serial) : 0;

    struct gs_dump_header header;
    header.version = GS_DUMP_STATE_VERSION;
    header.state_size = GS_DUMP_REGSTATE_SIZE + vram_size + 4 * (16 + 4) + 4;
    header.serial_offset = sizeof(struct gs_dump_header);
    header.serial_size = serial_size;
    header.crc = crc;
    header.screenshot_width = 0;
    header.screenshot_height = 0;
    header.screenshot_offset = sizeof(struct gs_dump_header) + serial_size;
    header.screenshot_size = 0;

    uint32_t header_size = sizeof(struct gs_dump_header) + serial_size;

    write_u32(dump, 0xFFFFFFFF);
    write_u32(dump, header_size);
    write_data(dump, &header, sizeof(header));

    if (serial_size)
        write_data(dump, serial, serial_size);

    write_register_state(dump, gs);
    write_data(dump, vram, vram_size);

    for (int i = 0; i < 4; i++) {
        uint8_t tag[16];
        memset(tag, 0, sizeof(tag));
        write_data(dump, tag, sizeof(tag));
        write_u32(dump, 0); // reg
    }

    write_u32(dump, (uint32_t)gif->q);

    write_priv_registers(dump, gs);

    fflush(dump->file);

    dump->active = 1;

    return 1;
}

int gs_dump_is_active(struct gs_dump* dump) {
    return dump->active;
}

void gs_dump_transfer(struct gs_dump* dump, int path, const void* data, size_t size) {
    if (!dump->active)
        return;

    write_u8(dump, GS_DUMP_PACKET_TRANSFER);
    write_u8(dump, (uint8_t)path);
    write_u32(dump, (uint32_t)size);
    write_data(dump, data, size);
}

void gs_dump_vsync(struct gs_dump* dump, struct ps2_gs* gs) {
    if (!dump->active)
        return;

    write_u8(dump, GS_DUMP_PACKET_PRIVREGISTERS);
    write_priv_registers(dump, gs);

    write_u8(dump, GS_DUMP_PACKET_VSYNC);
    write_u8(dump, (gs->csr & (1 << 13)) ? 0 : 1);
}

void gs_dump_readfifo(struct gs_dump* dump, uint32_t size) {
    if (!dump->active)
        return;

    write_u8(dump, GS_DUMP_PACKET_READFIFO);
    write_u32(dump, size);
}

void gs_dump_end(struct gs_dump* dump) {
    dump->active = 0;

    if (dump->file) {
        fclose(dump->file);
        dump->file = NULL;
    }
}
