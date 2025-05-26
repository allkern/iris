#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "gs/gs.h"
#include "software_thread.hpp"

#include <SDL.h>

#include "GL/gl3w.h"

static uint32_t* buf;

int psmct32_block[] = {
    0 , 1 , 4 , 5 , 16, 17, 20, 21,
    2 , 3 , 6 , 7 , 18, 19, 22, 23,
    8 , 9 , 12, 13, 24, 25, 28, 29,
    10, 11, 14, 15, 26, 27, 30, 31
};

int psmct32_column[] = {
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    2 , 3 , 6 , 7 , 10, 11, 14, 15
};

static inline int psmct32_addr(int base, int width, int x, int y) {
    // base expressed in blocks
    // 1 page = 64x32 = 8 KiB = 2048 words
    // 1 block = 8x8 = 256 B = 64 words
    // 1 column = 8x2 = 64 B = 16 words

    int page = (x >> 6) + ((y >> 5) * width);
    int blkx = (x >> 3) & 7;
    int blky = (y >> 3) & 3;
    int blk = blkx + (blky * 8);
    int col = (y >> 1) & 3;
    int idx = (x & 7) + ((y & 1) * 8);

    // Unswizzle block
    blk = psmct32_block[blk];

    // Unswizzle column
    idx = psmct32_column[idx];

    // printf("(%x, %d, %d, %d) -> p=%d b=%d c=%d addr=%08x\n",
    //     base, width, x, y,
    //     page, blk, cl,
    //     (page * 2048) + (base * 64) + (blk * 64) + cl
    // );

    return (page * 2048) + ((base + blk) * 64) + (col * 16) + idx;
}

int psmct16_block[] = {
    0 , 2 , 8 , 10,
    1 , 3 , 9 , 11,
    4 , 6 , 12, 14,
    5 , 7 , 13, 15,
    16, 18, 24, 26,
    17, 19, 25, 27,
    20, 22, 28, 30,
    21, 23, 29, 31
};

int psmct16_column[] = {
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15
};

int psmct16_shift[] = {
    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
    1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 ,
    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
    1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 
};

static inline int psmct16_addr(int base, int width, int x, int y) {
    // page            block          column
    // 64 x 64 pixels  16 x 8 pixels  16 x 2 pixels
    // base expressed in blocks
    // 1 page = 64x32 = 8 KiB = 2048 words
    // 1 block = 8x8 = 256 B = 64 words
    // 1 column = 8x2 = 64 B = 16 words

    int page = (x >> 6) + ((y >> 6) * width);
    int blkx = (x >> 4) & 3;
    int blky = (y >> 3) & 7;
    int blk = blkx + (blky * 4);
    int col = (y >> 1) & 3;
    int idx = (x & 15) + ((y & 1) * 16);

    // Unswizzle block
    blk = psmct16_block[blk];

    // Unswizzle column
    idx = psmct16_column[idx];

    // printf("(%x, %d, %d, %d) -> p=%d b=%d c=%d addr=%08x\n",
    //     base, width, x, y,
    //     page, blk, cl,
    //     (page * 2048) + (base * 64) + (blk * 64) + cl
    // );

    return (page * 2048) + ((base + blk) * 64) + (col * 16) + idx;
}

int psmct16s_block[] = {
    0 , 2 , 16, 18,
    1 , 3 , 17, 19,
    8 , 10, 24, 26,
    9 , 11, 25, 27,
    4 , 6 , 20, 22,
    5 , 7 , 21, 23,
    12, 14, 28, 30,
    13, 15, 29, 31
};

static inline int psmct16s_addr(int base, int width, int x, int y) {
    // page            block          column
    // 64 x 64 pixels  16 x 8 pixels  16 x 2 pixels
    // base expressed in blocks
    // 1 page = 64x32 = 8 KiB = 2048 words
    // 1 block = 8x8 = 256 B = 64 words
    // 1 column = 8x2 = 64 B = 16 words

    int page = (x >> 6) + ((y >> 6) * width);
    int blkx = (x >> 4) & 3;
    int blky = (y >> 3) & 7;
    int blk = blkx + (blky * 4);
    int col = (y >> 1) & 3;
    int idx = (x & 15) + ((y & 1) * 16);

    // Unswizzle block
    blk = psmct16s_block[blk];

    // Unswizzle column
    idx = psmct16_column[idx];

    // printf("(%x, %d, %d, %d) -> p=%d b=%d c=%d addr=%08x\n",
    //     base, width, x, y,
    //     page, blk, cl,
    //     (page * 2048) + (base * 64) + (blk * 64) + cl
    // );

    return (page * 2048) + ((base + blk) * 64) + (col * 16) + idx;
}

int psmt8_column_02[] = {
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7
};

int psmt8_column_13[] = {
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15
};

int psmt8_shift[] = {
    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
    2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 ,
    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
    2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 ,
    1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 ,
    3 , 3 , 3 , 3 , 3 , 3 , 3 , 3 ,
    1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 ,
    3 , 3 , 3 , 3 , 3 , 3 , 3 , 3
};

static inline int psmt8_addr(int base, int width, int x, int y) {
    // page            block          column
    // 128 x 64 pixels 16 x 16 pixels 16 x 4 pixels
    // base expressed in blocks
    // 1 page = 128x64 = 8 KiB = 2048 words
    // 1 block = 16x16 = 256 B = 64 words
    // 1 column = 16x4 = 64 B = 16 words
    // 1 page = 128/4x64 = 32x64 words
    // 1 block = 16/4x16 = 4x16 words
    // 1 column = 16/4x4 = 4x4 words

    int page = (x >> 7) + ((y >> 6) * (width >> 1));
    int blkx = (x >> 4) & 7;
    int blky = (y >> 4) & 3;
    int blk = blkx + (blky * 8);
    int col = (y >> 2) & 3;
    int idx = (x & 15) + ((y & 3) * 16);
    // int shift = (x & 3) * 16;

    // Unswizzle block
    blk = psmct32_block[blk];

    // Unswizzle column
    idx = (col & 1) ? psmt8_column_13[idx] : psmt8_column_02[idx];

    // printf("(%x, %d, %d, %d) -> p=%d b=%d c=%d addr=%08x\n",
    //     base, width, x, y,
    //     page, blk, cl,
    //     (page * 2048) + (base * 64) + (blk * 64) + cl
    // );

    return (page * 2048) + ((base + blk) * 64) + (col * 16) + idx;
}

int psmt4_block[] = {
    0 , 2 , 8 , 10, 1 , 3 , 9 , 11,
    4 , 6 , 12, 14, 5 , 7 , 13, 15,
    16, 18, 24, 26, 17, 19, 25, 27,
    20, 22, 28, 30, 21, 23, 29, 31
};

int psmt4_column_02[] = {
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7
};

int psmt4_column_13[] = {
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    8 , 9 , 12, 13, 0 , 1 , 4 , 5 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    10, 11, 14, 15, 2 , 3 , 6 , 7 ,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    0 , 1 , 4 , 5 , 8 , 9 , 12, 13,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15,
    2 , 3 , 6 , 7 , 10, 11, 14, 15
};

int psmt4_shift[] = {
    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
    8 , 8 , 8 , 8 , 8 , 8 , 8 , 8 ,
    16, 16, 16, 16, 16, 16, 16, 16,
    24, 24, 24, 24, 24, 24, 24, 24,
    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
    8 , 8 , 8 , 8 , 8 , 8 , 8 , 8 ,
    16, 16, 16, 16, 16, 16, 16, 16,
    24, 24, 24, 24, 24, 24, 24, 24,
    4 , 4 , 4 , 4 , 4 , 4 , 4 , 4 ,
    12, 12, 12, 12, 12, 12, 12, 12,
    20, 20, 20, 20, 20, 20, 20, 20,
    28, 28, 28, 28, 28, 28, 28, 28,
    4 , 4 , 4 , 4 , 4 , 4 , 4 , 4 ,
    12, 12, 12, 12, 12, 12, 12, 12,
    20, 20, 20, 20, 20, 20, 20, 20,
    28, 28, 28, 28, 28, 28, 28, 28
};

static inline int psmt4_addr(int base, int width, int x, int y) {
    // page             block          column
    // 128 x 128 pixels 32 x 16 pixels 32 x 4 pixels
    int page = (x >> 7) + ((y >> 7) * (width >> 1));
    int blkx = (x >> 5) & 3;
    int blky = (y >> 4) & 7;
    int blk = blkx + (blky * 4);
    int col = (y >> 2) & 3;
    int idx = (x & 31) + ((y & 3) * 32);
    // int shift = (x & 3) * 16;

    // Unswizzle block
    blk = psmt4_block[blk];

    // Unswizzle column
    idx = (col & 1) ? psmt4_column_13[idx] : psmt4_column_02[idx];

    // printf("(%x, %d, %d, %d) -> p=%d b=%d c=%d addr=%08x\n",
    //     base, width, x, y,
    //     page, blk, cl,
    //     (page * 2048) + (base * 64) + (blk * 64) + cl
    // );

    return (page * 2048) + ((base + blk) * 64) + (col * 16) + idx;
}

static const char* default_vert_shader =
    "#version 330 core\n"
    "layout (location = 0) in vec3 pos;\n"
    "layout (location = 1) in vec2 uv;\n"
    "uniform vec2 screen_size;\n"
    "out vec2 FragCoord;\n"
    "out vec2 FragTexCoord;\n"
    "void main() {\n"
    "    FragCoord = (pos.xy + 1.0) / 2.0;\n"
    "    FragTexCoord = uv;\n"
    "    FragCoord.y = 1.0 - FragCoord.y;\n"
    "    gl_Position = vec4(pos.xy, 0.0, 1.0);\n"
    "}";

static const char* default_frag_shader =
    "#version 330 core\n"
    "in vec2 FragCoord;\n"
    "in vec2 FragTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D input_texture;\n"
    "uniform vec2 screen_size;\n"
    "uniform int frame;\n"
    "void main() {\n"
    "    vec2 uv = vec2(FragTexCoord.x, 1.0 - FragTexCoord.y);\n"
    "    FragColor = texture(input_texture, uv);\n"
    "}";

static const char* frag_header =
    "#version 330 core\n"
    "in vec2 FragCoord;\n"
    "in vec2 FragTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D input_texture;\n"
    "uniform vec2 screen_size;\n"
    "uniform int frame;\n";

static const int psmt8_clut_block[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

void render_point(struct ps2_gs* gs, void* udata);
void render_line(struct ps2_gs* gs, void* udata);
void render_triangle(struct ps2_gs* gs, void* udata);
void render_sprite(struct ps2_gs* gs, void* udata);
void transfer_start(struct ps2_gs* gs, void* udata);
void transfer_write(struct ps2_gs* gs, void* udata);
void transfer_read(struct ps2_gs* gs, void* udata);

void software_thread_render_thread(software_thread_state* ctx) {
    while (!ctx->end_signal) {
        while (true) {
            ctx->queue_mtx.lock();

            if (ctx->render_queue.empty()) {
                ctx->queue_mtx.unlock();

                break;
            }

            // Explicitly copy data from the queue
            render_data rdata = render_data(ctx->render_queue.front());

            // Assign context pointer to the copied context
            rdata.gs.ctx = &rdata.gs.context[(rdata.gs.attr & GS_CTXT) ? 1 : 0];

            ctx->render_queue.pop();
            ctx->queue_mtx.unlock();

            ctx->render_mtx.lock();

            switch (rdata.prim) {
                case 0: render_point(&rdata.gs, nullptr); break;
                case 1: render_line(&rdata.gs, nullptr); break;
                case 2: render_triangle(&rdata.gs, nullptr); break;
                case 3: render_sprite(&rdata.gs, nullptr); break;
            }

            ctx->render_mtx.unlock();
        }

        std::this_thread::yield();
    }
}

void software_thread_destroy(void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    // Send end signal to rendering thread
    ctx->end_signal = true;

    // Clear rendering queue
    ctx->queue_mtx.lock();

    while (!ctx->render_queue.empty())
        ctx->render_queue.pop();

    ctx->queue_mtx.unlock();

    for (GLuint p : ctx->programs)
        if (p) glDeleteProgram(p);

    if (ctx->default_program) glDeleteProgram(ctx->default_program);
    if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
    if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
    if (ctx->ebo) glDeleteBuffers(1, &ctx->ebo);
    if (ctx->tex) glDeleteTextures(1, &ctx->tex);
    if (ctx->fbo) glDeleteFramebuffers(1, &ctx->fbo);
    if (ctx->fb_vao) glDeleteVertexArrays(1, &ctx->fb_vao);
    if (ctx->fb_vbo) glDeleteBuffers(1, &ctx->fb_vbo);
    if (ctx->fb_ebo) glDeleteBuffers(1, &ctx->fb_ebo);
    if (ctx->fb_in_tex) glDeleteTextures(1, &ctx->fb_in_tex);
    if (ctx->fb_out_tex) glDeleteTextures(1, &ctx->fb_out_tex);

    // Should call destructors for our mutex, thread and queue
    delete ctx;
}

void software_thread_set_size(void* udata, int width, int height) {
    software_thread_state* ctx = (software_thread_state*)udata;

    int en1 = ctx->gs->pmode & 1;
    int en2 = (ctx->gs->pmode >> 1) & 1;

    uint64_t display, dispfb;

    if (en1) {
        display = ctx->gs->display1;
        dispfb = ctx->gs->dispfb1;
    } else if (en2) {
        display = ctx->gs->display2;
        dispfb = ctx->gs->dispfb2;
    }

    ctx->disp_fmt = (dispfb >> 15) & 0x1f;

    int magh = ((display >> 23) & 0xf) + 1;
    int magv = ((display >> 27) & 3) + 1;

    if (((display >> 32) & 0xfff) == 0) {
        ctx->tex_w = 0;
        ctx->tex_h = 0;
        
        return;
    }

    if (((display >> 44) & 0x7ff) == 0) {
        ctx->tex_w = 0;
        ctx->tex_h = 0;
        
        return;
    }

    uint32_t tex_w = (((display >> 32) & 0xfff) / magh) + 1;
    uint32_t tex_h = (((display >> 44) & 0x7ff) / magv) + 1;

    // SMODE2 INT=1 FFMD=1
    // if ((ctx->gs->smode2 & 3) == 3) {
    //     tex_h /= 2;
    // }

    // Do nothing if the size hasn't changed
    if (tex_w == ctx->tex_w && tex_h == ctx->tex_h) {
        return;
    }

    ctx->tex_w = tex_w;
    ctx->tex_h = tex_h;

    if (buf) free(buf);

    buf = (uint32_t*)malloc((ctx->tex_w * sizeof(uint32_t)) * ctx->tex_h);

    if (ctx->tex) {
        glDeleteTextures(1, &ctx->tex);
    }

    glGenTextures(1, &ctx->tex);
    glBindTexture(GL_TEXTURE_2D, ctx->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ctx->tex_w, ctx->tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // if ((ctx->gs->smode2 & 3) != 3)
    //     return;

    // if (ctx->buf)
    //     free(ctx->buf);

    // int bpp = 0;

    // switch (ctx->disp_fmt) {
    //     case GS_PSMCT32:
    //     case GS_PSMCT24: {
    //         bpp = 4;
    //     } break;
    //     case GS_PSMCT16:
    //     case GS_PSMCT16S: {
    //         bpp = 2;
    //     } break;
    // }

    // ctx->buf = (uint32_t*)malloc((ctx->tex_w * bpp) * ctx->tex_h);

    // printf("software: width=%d height=%d format=%d display=%08x\n",
    //     ((display >> 32) & 0xfff),
    //     ((display >> 44) & 0x7ff),
    //     format, display
    // );
}

void software_thread_set_scale(void* udata, float scale) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->scale = scale;
}

void software_thread_set_aspect_mode(void* udata, int aspect_mode) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->aspect_mode = aspect_mode;
}

void software_thread_set_integer_scaling(void* udata, bool integer_scaling) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->integer_scaling = integer_scaling;
}

void software_thread_set_bilinear(void* udata, bool bilinear) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->bilinear = bilinear;
}

void software_thread_get_viewport_size(void* udata, int* w, int* h) {
    software_thread_state* ctx = (software_thread_state*)udata;

    *w = ctx->tex_w;
    *h = ctx->tex_h;
}

void software_thread_get_display_size(void* udata, int* w, int* h) {
    software_thread_state* ctx = (software_thread_state*)udata;

    *w = ctx->disp_w;
    *h = ctx->disp_h;
}

void software_thread_get_display_format(void* udata, int* fmt) {
    software_thread_state* ctx = (software_thread_state*)udata;

    *fmt = ctx->disp_fmt;
}

void software_thread_set_window_rect(void* udata, int x, int y, int w, int h) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->window_x = x;
    ctx->window_y = y;
    ctx->window_w = w;
    ctx->window_h = h;
}

void software_thread_get_interlace_mode(void* udata, int* interlace) {
    software_thread_state* ctx = (software_thread_state*)udata;

    // *interlace = ctx->gs->
}

const char* software_thread_get_name(void* udata) {
    return "Software (Threaded)";
}

#define CLAMP(v, l, u) (((v) > (u)) ? (u) : (((v) < (l)) ? (l) : (v)))

static inline uint32_t gs_apply_function(struct ps2_gs* gs, uint32_t t, uint32_t f) {
    int pr, pg, pb, pa;
    int tr = t & 0xff;
    int tg = (t >> 8) & 0xff;
    int tb = (t >> 16) & 0xff;
    int ta = (t >> 24) & 0xff;
    int fr = f & 0xff;
    int fg = (f >> 8) & 0xff;
    int fb = (f >> 16) & 0xff;
    int fa = (f >> 24) & 0xff;

    switch (gs->ctx->tfx) {
        case GS_MODULATE: {
            pr = CLAMP((tr * fr) >> 7, 0, 255);
            pg = CLAMP((tg * fg) >> 7, 0, 255);
            pb = CLAMP((tb * fb) >> 7, 0, 255);

            if (gs->ctx->tcc) {
                pa = CLAMP((ta * fa) >> 7, 0, 255);
            } else {
                pa = fa;
            }
        } break;
        case GS_DECAL: {
            pr = tr;
            pg = tg;
            pb = tb;

            if (gs->ctx->tcc) {
                pa = ta;
            } else {
                pa = fa;
            }
        } break;
        case GS_HIGHLIGHT: {
            pr = CLAMP(((tr * fr) >> 7) + fa, 0, 255);
            pg = CLAMP(((tg * fg) >> 7) + fa, 0, 255);
            pb = CLAMP(((tb * fb) >> 7) + fa, 0, 255);

            if (gs->ctx->tcc) {
                pa = CLAMP(fa + ta, 0, 255);
            } else {
                pa = fa;
            }
        } break;
        case GS_HIGHLIGHT2: {
            pr = CLAMP(((tr * fr) >> 7) + fa, 0, 255);
            pg = CLAMP(((tg * fg) >> 7) + fa, 0, 255);
            pb = CLAMP(((tb * fb) >> 7) + fa, 0, 255);

            if (gs->ctx->tcc) {
                pa = ta;
            } else {
                pa = fa;
            }
        } break;
    }

    return (pr & 0xff) | ((pg & 0xff) << 8) | ((pb & 0xff) << 16) | ((pa & 0xff) << 24);
}

enum : int {
    TR_FAIL,
    TR_PASS,
    TR_FB_ONLY,
    TR_ZB_ONLY,
    TR_RGB_ONLY
};

static inline uint32_t gs_read_fb(struct ps2_gs* gs, int x, int y) {
    switch (gs->ctx->fbpsm) {
        case GS_PSMCT32: {
            uint32_t addr = psmct32_addr(gs->ctx->fbp >> 6, gs->ctx->fbw >> 6, x, y);

            return gs->vram[addr];
        }
            // return gs->vram[gs->ctx->fbp + x + (y * gs->ctx->fbw)];
        case GS_PSMCT24: {
            uint32_t addr = psmct32_addr(gs->ctx->fbp >> 6, gs->ctx->fbw >> 6, x, y);

            return gs->vram[addr] & 0xffffff;
        }
        case GS_PSMCT16: {
            uint32_t addr = psmct16_addr(gs->ctx->fbp >> 6, gs->ctx->fbw >> 6, x, y);
            uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

            int idx = (x & 15) + ((y & 1) * 16);

            return vram[psmct16_shift[idx]];
        } break;
        case GS_PSMCT16S: {
            uint32_t addr = psmct16s_addr(gs->ctx->fbp >> 6, gs->ctx->fbw >> 6, x, y);
            uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

            int idx = (x & 15) + ((y & 1) * 16);

            return vram[psmct16_shift[idx]];
        } break;
    }

    return 0;
}

static inline uint32_t gs_read_dispfb(struct ps2_gs* gs, int x, int y, int dfb) {
    uint32_t dfbp = dfb ? gs->dfbp2 : gs->dfbp1;
    uint32_t dfbw = dfb ? gs->dfbw2 : gs->dfbw1;
    uint32_t dfbpsm = dfb ? gs->dfbpsm2 : gs->dfbpsm1;

    switch (dfbpsm) {
        case GS_PSMCT32: {
            uint32_t addr = psmct32_addr(dfbp, dfbw, x, y);

            return gs->vram[addr];
        }
        case GS_PSMCT24: {
            uint32_t addr = psmct32_addr(dfbp, dfbw, x, y);

            return gs->vram[addr] & 0xffffff;
        }
        case GS_PSMCT16: {
            uint32_t addr = psmct16_addr(dfbp, dfbw, x, y);
            uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

            int idx = (x & 15) + ((y & 1) * 16);

            return vram[psmct16_shift[idx]];
        } break;
        case GS_PSMCT16S: {
            uint32_t addr = psmct16s_addr(dfbp, dfbw, x, y);
            uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

            int idx = (x & 15) + ((y & 1) * 16);

            return vram[psmct16_shift[idx]];
        } break;
    }

    return 0;
}

static inline uint32_t gs_read_zb(struct ps2_gs* gs, int x, int y) {
    switch (gs->ctx->zbpsm) {
        case GS_ZSMZ32:
            return gs->vram[(gs->ctx->zbp + x + (y * gs->ctx->fbw)) & 0xfffff];
        case GS_ZSMZ24:
            return gs->vram[gs->ctx->zbp + x + (y * gs->ctx->fbw)] & 0xffffff;
        case GS_ZSMZ16:
        case GS_ZSMZ16S: {
            int shift = (x & 1) << 4;
            uint32_t mask = 0xffff << shift;
            uint32_t data = gs->vram[gs->ctx->zbp + (x >> 1) + (y * (gs->ctx->fbw >> 1))];

            return (data & mask) >> shift;
        }
    }

    return 0;
}

static inline uint32_t gs_read_cb_csm2(struct ps2_gs* gs, int i) {
    int x = i + gs->cou;

    uint32_t addr = psmct16_addr(gs->ctx->cbp, gs->cbw, x, gs->cov);
    uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

    int idx = (x & 15) + ((gs->cov & 1) * 16);

    return vram[psmct16_shift[idx]];

    // switch (gs->ctx->tbpsm) {
    //     case GS_PSMT8H:
    //     case GS_PSMT8: {
    //         uint32_t addr = psmct16_addr(gs->ctx->cbp, gs->cbw, x, gs->cov);
    //         uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

    //         int idx = (x & 15) + ((gs->cov & 1) * 16);

    //         return gs->vram[psmct16_shift[idx]];
    //     } break;

    //     case GS_PSMT4HL:
    //     case GS_PSMT4HH:
    //     case GS_PSMT4: {
    //         uint32_t addr = psmct16_addr(gs->ctx->cbp, gs->ctx->tbw, i, 0);
    //         uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

    //         return gs->vram[psmct16_shift[i & 15]];
    //     } break;
    // }
}

static inline uint32_t gs_read_cb(struct ps2_gs* gs, int i) {
    // Note: CSM2 should be the same?
    // not sure if the different arrangement actually
    // changes how address calculation is performed
    // maybe it only applies to the CLUT cache
    // - Edit: CSM2 uses the TEXCLUT reg, and it's
    // basically just like a normal buffer

    // Note: CLUT buffers are always 64 pixels wide

    if (gs->ctx->csm == 1)
        return gs_read_cb_csm2(gs, i);

    switch (gs->ctx->tbpsm) {
        case GS_PSMT8H:
        case GS_PSMT8: {
            int p = psmt8_clut_block[i];
            int x = p & 0xf;
            int y = p >> 4;

            switch (gs->ctx->cbpsm) {
                case GS_PSMCT32: {
                    return gs->vram[psmct32_addr(gs->ctx->cbp, 1, x, y)];
                } break;

                case GS_PSMCT16:
                case GS_PSMCT16S: {
                    uint32_t addr = psmct16_addr(gs->ctx->cbp, 2, x, y);
                    uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

                    int idx = (x & 15) + ((y & 1) * 16);

                    return vram[psmct16_shift[idx]];
                } break;
            }
        } break;

        case GS_PSMT4HH:
        case GS_PSMT4HL:
        case GS_PSMT4: {
            int x = i & 0x7;
            int y = i >> 3;

            switch (gs->ctx->cbpsm) {
                case GS_PSMCT32: {
                    // return gs->clut_cache[gs->ctx->csa + x + (y * 8)];
                    return gs->vram[psmct32_addr(gs->ctx->cbp, 1, x, y)];
                } break;

                case GS_PSMCT16:
                case GS_PSMCT16S: {
                    uint32_t addr = psmct16_addr(gs->ctx->cbp, 2, x, y);
                    uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

                    int idx = (x & 15) + ((y & 1) * 16);

                    return vram[psmct16_shift[idx]];
                } break;
            }
        } break;
    }

    return 0;
}

static inline uint32_t gs_to_rgba32(struct ps2_gs* gs, uint32_t c, int fmt) {
    switch (fmt) {
        case GS_PSMCT32:
            return c;
        case GS_PSMCT24: {
            uint32_t a = 0;

            if (gs->aem) {
                if (c & 0xffffff) {
                    a = gs->ta0;
                } else {
                    a = 0;
                }
            } else {
                a = gs->ta0;
            }

            return c | (a << 24);
        }
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            int ia = c & 0x8000;
            uint32_t oa = 0;

            if (ia) {
                oa = gs->ta1;
            } else {
                if (gs->aem) {
                    if (c & 0x7fff) {
                        oa = gs->ta0;
                    } else {
                        oa = 0;
                    }
                } else {
                    oa = gs->ta0;
                }
            }

            return ((c & 0x001f) << 3) |
                   ((c & 0x03e0) << 6) |
                   ((c & 0x7c00) << 9) |
                   (oa << 24);
        } break;
        case GS_PSMT8:
        case GS_PSMT8H:
        case GS_PSMT4:
        case GS_PSMT4HL:
        case GS_PSMT4HH: {
            return gs_to_rgba32(gs, c, gs->ctx->cbpsm);
        } break;
    }

    return 0;
}

static inline uint32_t gs_from_rgba32(struct ps2_gs* gs, uint32_t c, int fmt) {
    switch (fmt) {
        case GS_PSMCT32:
            return c;
        case GS_PSMCT24:
            // To-do: Use TEXA
            return c & 0xffffff;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            // To-do: Use TEXA
            return ((c & 0x0000f8) >> 3) |
                   ((c & 0x00f800) >> 6) |
                   ((c & 0xf80000) >> 9) |
                   ((c & 0x80000000) ? 0x8000 : 0);
        } break;
        case GS_PSMT8:
        case GS_PSMT8H:
        case GS_PSMT4:
        case GS_PSMT4HL:
        case GS_PSMT4HH: {
            return gs_from_rgba32(gs, c, gs->ctx->cbpsm);
        } break;
    }

    return 0;
}

static inline uint32_t gs_read_tb_impl(struct ps2_gs* gs, int u, int v) {
    switch (gs->ctx->tbpsm) {
        case GS_PSMCT32:
            return gs->vram[psmct32_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v)];
        case GS_PSMCT24:
            return gs->vram[psmct32_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v)];
        case GS_PSMCT16: {
            uint32_t addr = psmct16_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v);
            uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

            int idx = (u & 15) + ((v & 1) * 16);

            return vram[psmct16_shift[idx]];
        } break;
        case GS_PSMCT16S: {
            uint32_t addr = psmct16s_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v);
            uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

            int idx = (u & 15) + ((v & 1) * 16);

            return vram[psmct16_shift[idx]];
        } break;
        case GS_PSMT8: {
            uint32_t addr = psmt8_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v);
            uint8_t* vram = (uint8_t*)(&gs->vram[addr]);

            int idx = (u & 15) + ((v & 3) * 16);

            return gs_read_cb(gs, vram[psmt8_shift[idx]]);
        } break;
        case GS_PSMT8H: {
            uint32_t data = gs->vram[psmct32_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v)];

            return gs_read_cb(gs, data >> 24);
        } break;
        case GS_PSMT4: {
            uint32_t addr = psmt4_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v);

            int idx = (u & 31) + ((v & 3) * 32);
            int shift = psmt4_shift[idx];

            uint32_t mask = 0xful << shift;

            return gs_read_cb(gs, (gs->vram[addr] & mask) >> shift);
        } break;
        case GS_PSMT4HL: {
            uint32_t data = gs->vram[psmct32_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v)];

            return gs_read_cb(gs, (data >> 24) & 0xf);
        } break;
        case GS_PSMT4HH: {
            uint32_t data = gs->vram[psmct32_addr(gs->ctx->tbp0, gs->ctx->tbw, u, v)];

            return gs_read_cb(gs, data >> 28);
        } break;
    }

    return 0;
}

static inline uint32_t gs_read_tb(struct ps2_gs* gs, int u, int v) {
    if ((gs->ctx->xyoffset >> 32) & 0xf)
        v += 0x10;

    // return gs_read_tb_impl(gs, u >> 4, v >> 4);

    if (gs->ctx->mmag) {
        float a = (u & 0xf) * 0.0625;
        float b = (v & 0xf) * 0.0625;

        int iu0 = u >> 4;
        int iv0 = v >> 4;
        int iu1 = iu0 + 1;
        int iv1 = iv0 + 1;

        uint32_t s0 = gs_read_tb_impl(gs, iu0, iv0);
        uint32_t s1 = gs_read_tb_impl(gs, iu1, iv0);
        uint32_t s2 = gs_read_tb_impl(gs, iu0, iv1);
        uint32_t s3 = gs_read_tb_impl(gs, iu1, iv1);

        s0 = gs_to_rgba32(gs, s0, gs->ctx->tbpsm);
        s1 = gs_to_rgba32(gs, s1, gs->ctx->tbpsm);
        s2 = gs_to_rgba32(gs, s2, gs->ctx->tbpsm);
        s3 = gs_to_rgba32(gs, s3, gs->ctx->tbpsm);

        int r0 = s0 & 0xff;
        int g0 = (s0 >> 8) & 0xff;
        int b0 = (s0 >> 16) & 0xff;
        int a0 = (s0 >> 24) & 0xff;
        int r1 = s1 & 0xff;
        int g1 = (s1 >> 8) & 0xff;
        int b1 = (s1 >> 16) & 0xff;
        int a1 = (s1 >> 24) & 0xff;
        int r2 = s2 & 0xff;
        int g2 = (s2 >> 8) & 0xff;
        int b2 = (s2 >> 16) & 0xff;
        int a2 = (s2 >> 24) & 0xff;
        int r3 = s3 & 0xff;
        int g3 = (s3 >> 8) & 0xff;
        int b3 = (s3 >> 16) & 0xff;
        int a3 = (s3 >> 24) & 0xff;

        uint32_t rr = ((1.0-a) * (1.0-b) * r0) + (a * (1.0-b) * r1) + ((1.0-a) * b * r2) + (a * b * r3);
        uint32_t gg = ((1.0-a) * (1.0-b) * g0) + (a * (1.0-b) * g1) + ((1.0-a) * b * g2) + (a * b * g3);
        uint32_t bb = ((1.0-a) * (1.0-b) * b0) + (a * (1.0-b) * b1) + ((1.0-a) * b * b2) + (a * b * b3);
        uint32_t aa = ((1.0-a) * (1.0-b) * a0) + (a * (1.0-b) * a1) + ((1.0-a) * b * a2) + (a * b * a3);

        rr = CLAMP(rr, 0, 255);
        gg = CLAMP(gg, 0, 255);
        bb = CLAMP(bb, 0, 255);
        aa = CLAMP(aa, 0, 255);

        return gs_from_rgba32(gs, rr | (gg << 8) | (bb << 16) | (aa << 24), gs->ctx->tbpsm);
    } else {
        return gs_read_tb_impl(gs, u >> 4, v >> 4);
    }
}

static inline void gs_write_fb(struct ps2_gs* gs, int x, int y, uint32_t c) {
    uint32_t f = gs_from_rgba32(gs, c, gs->ctx->fbpsm);

    // To-do: Implement FBMSK
    switch (gs->ctx->fbpsm) {
        case GS_PSMCT32: {
            uint32_t addr = psmct32_addr(gs->ctx->fbp >> 6, gs->ctx->fbw >> 6, x, y);

            gs->vram[addr] = f;
            // gs->vram[(gs->ctx->fbp + x + (y * gs->ctx->fbw)) & 0xfffff] = f;
        } break;

        case GS_PSMCT24: {
            uint32_t addr = psmct32_addr(gs->ctx->fbp >> 6, gs->ctx->fbw >> 6, x, y);
            uint32_t v = gs->vram[addr];

            gs->vram[addr] = (v & 0xff000000) | (f & 0xffffff);
            // gs->vram[(gs->ctx->fbp + x + (y * gs->ctx->fbw)) & 0xfffff] = f;
        } break;
        case GS_PSMCT16: {
            uint32_t addr = psmct16_addr(gs->ctx->fbp >> 6, gs->ctx->fbw >> 6, x, y);
            uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

            int idx = (x & 15) + ((y & 1) * 16);

            vram[psmct16_shift[idx]] = f;
        } break;
        case GS_PSMCT16S: {
            uint32_t addr = psmct16s_addr(gs->ctx->fbp >> 6, gs->ctx->fbw >> 6, x, y);
            uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

            int idx = (x & 15) + ((y & 1) * 16);

            vram[psmct16_shift[idx]] = f;
        } break;
    }
}

static inline void gs_write_fb_no_alpha(struct ps2_gs* gs, int x, int y, uint32_t c) {
    uint32_t f = gs_from_rgba32(gs, c, gs->ctx->fbpsm);

    // To-do: Implement FBMSK
    switch (gs->ctx->fbpsm) {
        case GS_PSMCT32: {
            uint32_t p = gs->vram[(gs->ctx->fbp + x + (y * gs->ctx->fbw)) & 0xfffff];

            gs->vram[(gs->ctx->fbp + x + (y * gs->ctx->fbw)) & 0xfffff] = (f & 0xffffff) | (p & 0xff000000);
        } break;

        case GS_PSMCT24: {
            uint32_t p = gs->vram[(gs->ctx->fbp + x + (y * gs->ctx->fbw)) & 0xfffff];

            gs->vram[gs->ctx->fbp + x + (y * gs->ctx->fbw)] = (f & 0xffffff) | (p & 0xff000000);
        } break;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            uint16_t* ptr = ((uint16_t*)&gs->vram[gs->ctx->fbp]) + x + (y * gs->ctx->fbw);

            *ptr = (c & 0x7fff) | (*ptr & 0x8000);
        } break;
    }
}

static inline void gs_write_zb(struct ps2_gs* gs, int x, int y, uint32_t z) {
    if (gs->ctx->zbmsk || !gs->ctx->zte)
        return;

    switch (gs->ctx->zbpsm) {
        case 0x01: z = std::min(z, 0xffffffu); break;
        case 0x02: z = std::min(z, 0xffffu); break;
        case 0x0A: z = std::min(z, 0xffffu); break;
        case 0x31: z = std::min(z, 0xffffffu); break;
        case 0x32: z = std::min(z, 0xffffu); break;
        case 0x3A: z = std::min(z, 0xffffu); break;
    }

    switch (gs->ctx->zbpsm) {
        case GS_ZSMZ32: {
            gs->vram[gs->ctx->zbp + x + (y * gs->ctx->fbw)] = z;
        } break;

        case GS_ZSMZ24: {
            gs->vram[gs->ctx->zbp + x + (y * gs->ctx->fbw)] = z;
        } break;
        case GS_ZSMZ16:
        case GS_ZSMZ16S: {
            uint16_t* ptr = (uint16_t*)&gs->vram[gs->ctx->zbp];

            *(ptr + x + (y * gs->ctx->fbw)) = z;
        } break;
    }
}

static inline int gs_test_scissor(struct ps2_gs* gs, int x, int y) {
    if (x < (int)gs->ctx->scax0 || y < (int)gs->ctx->scay0 ||
        x > (int)gs->ctx->scax1 || y > (int)gs->ctx->scay1) {
        return TR_FAIL;
    }

    return TR_PASS;
}

static inline int gs_test_pixel(struct ps2_gs* gs, int x, int y, uint32_t z, uint8_t a) {
    int tr = TR_PASS;

    // Alpha test
    if (gs->ctx->ate) {
        switch (gs->ctx->atst) {
            case 0: tr = TR_FAIL; break;
            case 2: tr = a < gs->ctx->aref; break;
            case 3: tr = a <= gs->ctx->aref; break;
            case 4: tr = a == gs->ctx->aref; break;
            case 5: tr = a >= gs->ctx->aref; break;
            case 6: tr = a > gs->ctx->aref; break;
            case 7: tr = a != gs->ctx->aref; break;
        }

        if (tr == TR_FAIL) {
            switch (gs->ctx->afail) {
                case 0: return TR_FAIL;
                case 1: tr = TR_FB_ONLY; break;
                case 2: tr = TR_ZB_ONLY; break;
                case 3: tr = TR_RGB_ONLY; break;
            }
        }
    }

    // Destination alpha test
    if (gs->ctx->date) {
        uint32_t s = gs_read_fb(gs, x, y);

        switch (gs->ctx->fbpsm) {
            case GS_PSMCT16:
            case GS_PSMCT16S:
                if (((s >> 15) & 1) != gs->ctx->datm) return TR_FAIL;
            break;
            case GS_PSMCT32:
                if (((s >> 31) & 1) != gs->ctx->datm) return TR_FAIL;
            break;
        }
    }

    // Depth test
    if (gs->ctx->zte) {
        uint32_t zb = gs_read_zb(gs, x, y);

        switch (gs->ctx->zbpsm) {
            case 0x01: z = std::min(z, 0xffffffu); break;
            case 0x02: z = std::min(z, 0xffffu); break;
            case 0x0A: z = std::min(z, 0xffffu); break;
            case 0x31: z = std::min(z, 0xffffffu); break;
            case 0x32: z = std::min(z, 0xffffu); break;
            case 0x3A: z = std::min(z, 0xffffu); break;
        }

        switch (gs->ctx->ztst) {
            case 0: return TR_FAIL;
            case 2: {
                if (z < zb) return TR_FAIL;
            } break;
            case 3: {
                if (z <= zb) return TR_FAIL;
            } break;
        }
    }

    return tr;
}

static inline int gs_clamp_u(struct ps2_gs* gs, int u) {
    int iu = u >> 4;

    switch (gs->ctx->wms) {
        case 0: {
            iu %= gs->ctx->usize;
        } break;

        case 1: {
            iu = (iu < 0) ? 0 : ((iu > (int)gs->ctx->usize) ? gs->ctx->usize : iu);
        } break;

        case 2: {
            iu = (iu < (int)gs->ctx->minu) ? gs->ctx->minu : ((iu > (int)gs->ctx->maxu) ? gs->ctx->maxu : iu);
        } break;

        case 3: {
            int umsk = gs->ctx->minu;
            int ufix = gs->ctx->maxu;

            iu = (iu & umsk) | ufix;
        } break;
    }

    return (iu << 4) | (u & 0xf);
}

static inline int gs_clamp_v(struct ps2_gs* gs, int v) {
    int iv = v >> 4;

    switch (gs->ctx->wmt) {
        case 0: {
            iv %= gs->ctx->vsize;
        } break;

        case 1: {
            iv = (iv < 0) ? 0 : ((iv > (int)gs->ctx->vsize) ? gs->ctx->vsize : iv);
        } break;

        case 2: {
            iv = (iv < (int)gs->ctx->minv) ? gs->ctx->minv : ((iv > (int)gs->ctx->maxv) ? gs->ctx->maxv : iv);
        } break;

        case 3: {
            int vmsk = gs->ctx->minv;
            int vfix = gs->ctx->maxv;

            iv = (iv & vmsk) | vfix;
        } break;
    }

    return (iv << 4) | (v & 0xf);
}

static inline uint32_t gs_alpha_blend(struct ps2_gs* gs, int x, int y, uint32_t s) {
    uint32_t d = gs_read_fb(gs, x, y);

    switch (gs->ctx->fbpsm) {
        case GS_PSMCT32: break;
        case GS_PSMCT24: d |= 0x80000000; break;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            int a = d & 0x8000;

            d = ((d & 0x001f) << 3) |
                ((d & 0x03e0) << 6) |
                ((d & 0x7c00) << 9);

            d |= a ? 0x80000000 : 0;
        } break;
    }

    uint32_t av = (gs->ctx->a == 0) ? s : ((gs->ctx->a == 1) ? d : 0);
    uint32_t bv = (gs->ctx->b == 0) ? s : ((gs->ctx->b == 1) ? d : 0);
    uint32_t cv = (gs->ctx->c == 0) ? (s >> 24) : ((gs->ctx->c == 1) ? (d >> 24) : gs->ctx->fix);
    uint32_t dv = (gs->ctx->d == 0) ? s : ((gs->ctx->d == 1) ? d : 0);
    
    // cv = CLAMP(cv, 0, 127);

    // if (!gs->tme)
    //     printf("Cd=%08x Cs=%08x a=%08x (%d) b=%08x (%d) c=%08x (%d) d=%08x (%d)\n", d, s,
    //     av, gs->ctx->a,
    //     bv, gs->ctx->b,
    //     cv, gs->ctx->c,
    //     dv, gs->ctx->d
    // );

    int ar = (av >> 0 ) & 0xff;
    int ag = (av >> 8 ) & 0xff;
    int ab = (av >> 16) & 0xff;
    // uint32_t aa = (av >> 24) & 0xff;
    int br = (bv >> 0 ) & 0xff;
    int bg = (bv >> 8 ) & 0xff;
    int bb = (bv >> 16) & 0xff;
    // uint32_t ba = (bv >> 24) & 0xff;
    int dr = (dv >> 0 ) & 0xff;
    int dg = (dv >> 8 ) & 0xff;
    int db = (dv >> 16) & 0xff;
    // uint32_t da = (dv >> 24) & 0xff;

    int rr = ar - br;
    int rg = ag - bg;
    int rb = ab - bb;

    rr = ((rr * (int)cv) >> 7) + dr;
    rg = ((rg * (int)cv) >> 7) + dg;
    rb = ((rb * (int)cv) >> 7) + db;
    // uint32_t ra = (((aa - ba) * cv) >> 7) + da;

    rr = CLAMP(rr, 0, 255);
    rg = CLAMP(rg, 0, 255);
    rb = CLAMP(rb, 0, 255);

    return (rr & 0xff) | ((rg & 0xff) << 8) | ((rb & 0xff) << 16) | (d & 0xff000000);
}

static inline float lerpf(int32_t x, float u1, int32_t x1, float u2, int32_t x2) {
    float b = u1 * (x2 - x);

    b += u2 * (x - x1);

    if (!(x2 - x1))
        return u1;

    return b / (x2 - x1);
}

static inline int32_t lerp(int32_t x, int32_t u1, int32_t x1, int32_t u2, int32_t x2) {
    int64_t b = (int64_t)u1 * (x2 - x);

    b += (int64_t)u2 * (x - x1);

    if (!(x2 - x1))
        return u1;

    return b / (x2 - x1);
}

int32_t stepsize(int32_t u1, int32_t x1, int32_t u2, int32_t x2, int64_t mult) {
    if (!(x2 - x1))
        return ((u2 - u1) * mult);

    return ((u2 - u1) * mult)/(x2 - x1);
}

static inline void gs_draw_pixel(struct ps2_gs* gs, int x, int y, uint32_t z, uint32_t c) {
    int a = c >> 24;

    int tr = gs_test_pixel(gs, x, y, z, a);

    if (tr == TR_FAIL)
        return;

    if (gs->abe) c = gs_alpha_blend(gs, x, y, c);

    switch (tr) {
        case TR_FB_ONLY: gs_write_fb(gs, x, y, c); break;
        case TR_ZB_ONLY: gs_write_zb(gs, x, y, z); break;
        case TR_RGB_ONLY: gs_write_fb_no_alpha(gs, x, y, c); break;
        case TR_PASS: {
            gs_write_zb(gs, x, y, z);
            gs_write_fb(gs, x, y, c);
        } break;
    }
}

int software_thread_compile_shader(const char* src, GLint type) {
    unsigned int shader = glCreateShader(type);

    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    int success;
    char infolog[512];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infolog);

        printf("software: Shader compilation failed\n%s", infolog);

        exit(1);

        return 0;
    }

    return shader;
}

char* software_thread_read_file(const char* path) {
    FILE* file = fopen(path, "rb");

    unsigned int size;

    fseek(file, 0, SEEK_END);

    size = ftell(file);

    fseek(file, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);

    buf[size] = '\0';

    if (!fread(buf, 1, size, file)) {
        printf("software: Couldn't read shader source\n");

        return buf;
    }

    return buf;
}

void software_thread_init_default_shader(software_thread_state* ctx) {
    ctx->vert_shader = software_thread_compile_shader(default_vert_shader, GL_VERTEX_SHADER);
    GLuint frag_shader = software_thread_compile_shader(default_frag_shader, GL_FRAGMENT_SHADER);

    char infolog[512];
    int success;

    ctx->default_program = glCreateProgram();

    glAttachShader(ctx->default_program, ctx->vert_shader);
    glAttachShader(ctx->default_program, frag_shader);
    glLinkProgram(ctx->default_program);
    glGetProgramiv(ctx->default_program, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(ctx->default_program, 512, NULL, infolog);

        printf("opengl: Program linking failed\n%s", infolog);

        exit(1);
    }
}

void software_thread_push_shader(software_thread_state* ctx, const char* path) {
    char* src = software_thread_read_file(path);
    char* hdr = (char*)malloc(strlen(src) + strlen(frag_header) + 1);

    strcpy(hdr, frag_header);
    strcat(hdr, src);

    int shader = software_thread_compile_shader(hdr, GL_FRAGMENT_SHADER);

    char infolog[512];
    int success;

    GLuint program = glCreateProgram();

    glAttachShader(program, ctx->vert_shader);
    glAttachShader(program, shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infolog);

        printf("opengl: Program linking failed\n%s", infolog);

        exit(1);
    }

    ctx->programs.push_back(program);

    free(src);
    free(hdr);
}

void software_thread_init(void* udata, struct ps2_gs* gs, SDL_Window* window) {
    software_thread_state* ctx = (software_thread_state*)udata;

    gl3wInit();

    ctx->window = window;
    ctx->gs = gs;
    ctx->scale = 1.5f;

    // Initialize default shaders
    software_thread_init_default_shader(ctx);

    // software_thread_push_shader(ctx, "shaders/flip.frag");
    // software_thread_push_shader(ctx, "shaders/encoder.frag");
    // software_thread_push_shader(ctx, "shaders/decoder.frag");
    // software_thread_push_shader(ctx, "shaders/smooth.frag");

    // Initialize framebuffer VAO
    static const GLfloat fb_vertices[] = {
        // positions         // texture coords
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,   // top right
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,   // bottom right
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,   // bottom left
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f    // top left 
    };

    static const GLuint fb_indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    glGenVertexArrays(1, &ctx->fb_vao);
    glGenBuffers(1, &ctx->fb_vbo);
    glGenBuffers(1, &ctx->fb_ebo);

    glBindVertexArray(ctx->fb_vao);

    glBindBuffer(GL_ARRAY_BUFFER, ctx->fb_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fb_vertices), fb_vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->fb_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(fb_indices), fb_indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &ctx->vao);
    glGenBuffers(1, &ctx->vbo);
    glGenBuffers(1, &ctx->ebo);

    ctx->end_signal = false;
    ctx->render_thr = std::thread(software_thread_render_thread, ctx);
    ctx->render_thr.detach();
}

static inline void software_thread_vram_blit(struct ps2_gs* gs, software_thread_state* ctx) {
    // printf("dbp=%x (%x) dbw=%d (%d) dpsm=%02x dsa=(%d,%d) sbp=%x (%x) sbw=%d (%d) spsm=%02x ssa=(%d,%d) rr=(%d,%d) xdir=%d\n",
    //     ctx->dbp, ctx->dbp,
    //     ctx->dbw, ctx->dbw,
    //     ctx->dpsm,
    //     ctx->dsax,
    //     ctx->dsay,
    //     ctx->sbp, ctx->sbp,
    //     ctx->sbw, ctx->sbw,
    //     ctx->spsm,
    //     ctx->ssax,
    //     ctx->ssay,
    //     ctx->rrw,
    //     ctx->rrh,
    //     ctx->xdir
    // );

    for (int y = 0; y < (int)ctx->rrh; y++) {
        uint32_t src = ctx->sbp + ctx->ssax + (ctx->ssay * ctx->rrw) + (y * ctx->rrw);
        uint32_t dst = ctx->dbp + ctx->dsax + (ctx->dsay * ctx->rrw) + (y * ctx->rrw);

        memcpy(gs->vram + dst, gs->vram + src, ctx->rrw * sizeof(uint32_t));
    }
}

void render_point(struct ps2_gs* gs, void* udata) {
    struct gs_vertex vert = gs->vq[0];

    vert.x -= gs->ctx->ofx;
    vert.y -= gs->ctx->ofy;

    if (!gs_test_scissor(gs, vert.x, vert.y))
        return;

    gs_draw_pixel(gs, vert.x, vert.y, vert.z, vert.rgbaq & 0xffffffff);
}

void render_line(struct ps2_gs* gs, void* udata) {
    struct gs_vertex v0 = gs->vq[0];
    struct gs_vertex v1 = gs->vq[1];

    // printf("v0=(%d,%d) v1=(%d,%d)\n", v0.x, v0.y, v1.x, v1.y);

    v0.x -= gs->ctx->ofx;
    v0.y -= gs->ctx->ofy;
    v1.x -= gs->ctx->ofx;
    v1.y -= gs->ctx->ofy;

    int dx = abs(v1.x - v0.x);
    int sx = v0.x < v1.x ? 1 : -1;
    int dy = -abs(v1.y - v0.y);
    int sy = v0.y < v1.y ? 1 : -1;
    int error = dx + dy;

    while (1) {
        if (gs_test_scissor(gs, v0.x, v0.y))
            gs_draw_pixel(gs, v0.x, v0.y, v0.z, v1.rgbaq & 0xffffffff);

        int e2 = error << 1;
    
        if (e2 >= dy) {
            if (v0.x == v1.x) break;

            error = error + dy;
            v0.x = v0.x + sx;
        }

        if (e2 <= dx) {
            if (v0.y == v1.y) break;

            error = error + dx;
            v0.y = v0.y + sy;
        }
    }
}

void gs_draw_wireframe(struct ps2_gs* gs, struct gs_vertex v0, struct gs_vertex v1) {
    v0.x -= gs->ctx->ofx;
    v0.y -= gs->ctx->ofy;
    v1.x -= gs->ctx->ofx;
    v1.y -= gs->ctx->ofy;

    int dx = abs(v1.x - v0.x);
    int sx = v0.x < v1.x ? 1 : -1;
    int dy = -abs(v1.y - v0.y);
    int sy = v0.y < v1.y ? 1 : -1;
    int error = dx + dy;

    while (1) {
        if (gs_test_scissor(gs, v0.x, v0.y))
            gs_write_fb(gs, v0.x, v0.y, 0xff0000ff);

        int e2 = error << 1;
    
        if (e2 >= dy) {
            if (v0.x == v1.x) break;

            error = error + dy;
            v0.x = v0.x + sx;
        }

        if (e2 <= dx) {
            if (v0.y == v1.y) break;

            error = error + dx;
            v0.y = v0.y + sy;
        }
    }
}

#define EDGE(a, b, c) ((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))
#define MIN2(a, b) ((((int)a) < ((int)b)) ? ((int)a) : ((int)b))
#define MIN3(a, b, c) (MIN2(MIN2(a, b), c))
#define MAX2(a, b) ((((int)a) > ((int)b)) ? ((int)a) : ((int)b))
#define MAX3(a, b, c) (MAX2(MAX2(a, b), c))

#define IS_TOPLEFT(a, b) ((b.y > a.y) || ((a.y == b.y) && (b.x < a.x)))

void render_triangle(struct ps2_gs* gs, void* udata) {
    struct gs_vertex v0 = gs->vq[0];
    struct gs_vertex v1 = gs->vq[1];
    struct gs_vertex v2 = gs->vq[2];

    if (EDGE(v0, v1, v2) < 0) {
        v1 = gs->vq[2];
        v2 = gs->vq[1];
    }

    v0.x -= gs->ctx->ofx;
    v1.x -= gs->ctx->ofx;
    v2.x -= gs->ctx->ofx;
    v0.y -= gs->ctx->ofy;
    v1.y -= gs->ctx->ofy;
    v2.y -= gs->ctx->ofy;
    // v0.x -= 1;
    // v1.x -= 1;
    // v2.x -= 1;
    // v0.y -= 1;
    // v1.y -= 1;
    // v2.y -= 1;

    int xmin = MIN3(v0.x, v1.x, v2.x);
    int ymin = MIN3(v0.y, v1.y, v2.y);
    int xmax = MAX3(v0.x, v1.x, v2.x);
    int ymax = MAX3(v0.y, v1.y, v2.y);

    xmin = MAX2(xmin, gs->ctx->scax0);
    ymin = MAX2(ymin, gs->ctx->scay0);
    xmax = MIN2(xmax, gs->ctx->scax1);
    ymax = MIN2(ymax, gs->ctx->scay1);

    int a01 = (int)v0.y - (int)v1.y, b01 = (int)v1.x - (int)v0.x;
    int a12 = (int)v1.y - (int)v2.y, b12 = (int)v2.x - (int)v1.x;
    int a20 = (int)v2.y - (int)v0.y, b20 = (int)v0.x - (int)v2.x;

    struct gs_vertex p;
    p.x = xmin;
    p.y = ymin;

    // int bias0 = IS_TOPLEFT(v1, v2) ? 0 : -1;
    // int bias1 = IS_TOPLEFT(v2, v0) ? 0 : -1;
    // int bias2 = IS_TOPLEFT(v0, v1) ? 0 : -1;
    int w0_row = EDGE(v1, v2, p); // + bias0;
    int w1_row = EDGE(v2, v0, p); // + bias1;
    int w2_row = EDGE(v0, v1, p); // + bias2;

    // printf("triangle: v0=(%d,%d,%08x) v1=(%d,%d,%08x) v2=(%d,%d,%08x) min=(%d,%d) max=(%d,%d) iip=%d tme=%d fst=%d abe=%d tfx=%d tcc=%d zte=%d\n",
    //     v0.x, v0.y, v0.z,
    //     v1.x, v1.y, v1.z,
    //     v2.x, v2.y, v2.z,
    //     xmin, ymin,
    //     xmax, ymax,
    //     gs->iip,
    //     gs->tme,
    //     gs->fst,
    //     gs->abe,
    //     gs->ctx->tfx,
    //     gs->ctx->tcc,
    //     gs->ctx->zte
    // );

    // if (gs->tme) {
    //     printf("gs: TB format=%d (0x%02x) tbp=%x tbw=%d uv=(%d,%d) TEX w=%d h=%d CB format=%d cbp=%x csm=%d tfx=%d rgba=%08lx abe=%d FB format=%d fbw=%d fba=%ld\n",
    //         gs->ctx->tbpsm,
    //         gs->ctx->tbpsm,
    //         gs->ctx->tbp0,
    //         gs->ctx->tbw,
    //         v0.u,
    //         v0.v,
    //         gs->ctx->usize,
    //         gs->ctx->vsize,
    //         gs->ctx->cbpsm,
    //         gs->ctx->cbp,
    //         gs->ctx->csm,
    //         gs->ctx->tfx,
    //         v1.rgbaq & 0xffffffff,
    //         gs->abe,
    //         gs->ctx->fbpsm,
    //         gs->ctx->fbw,
    //         gs->ctx->fba// ,
    //         // gs->ctx->a,
    //         // gs->ctx->b,
    //         // gs->ctx->c,
    //         // gs->ctx->d,
    //         // gs->ctx->tcc,
    //         // gs->ctx->fix
    //     );
    // }

    // printf("triangle: v0=(%d,%d,%d) v1=(%d,%d,%d) v2=(%d,%d,%d) min=(%d,%d) max=(%d,%d) sca0=(%d,%d) sca1=(%d,%d) offset=(%d,%d)\n",
    //     v0.x, v0.y, v0.z,
    //     v1.x, v1.y, v1.z,
    //     v2.x, v2.y, v2.z,
    //     xmin, ymin,
    //     xmax, ymax,
    //     gs->ctx->scax0, gs->ctx->scay0,
    //     gs->ctx->scax1, gs->ctx->scay1,
    //     gs->ctx->ofx, gs->ctx->ofy
    // );

    // printf("ZB format=%x zbp=%x zmsk=%d zte=%d ztst=%d\n",
    //     gs->ctx->zbpsm,
    //     gs->ctx->zbp,
    //     gs->ctx->zbmsk,
    //     gs->ctx->zte,
    //     gs->ctx->ztst
    // );

    int area = EDGE(v0, v1, v2);

    for (p.y = ymin; p.y <= ymax; p.y++) {
        // Barycentric coordinates at start of row
        int w0 = w0_row;
        int w1 = w1_row;
        int w2 = w2_row;

        for (p.x = xmin; p.x <= xmax; p.x++) {
            // If p is on or inside all edges, render pixel.
            if ((w0 | w1 | w2) < 0) {
                w0 += a12;
                w1 += a20;
                w2 += a01;

                continue;
            }

            // Calculate interpolation weights
            double iw0 = (double)w0 / (double)area;
            double iw1 = (double)w1 / (double)area;
            double iw2 = (double)w2 / (double)area;

            uint32_t fr, fg, fb, fa;

            if (gs->iip) {
                fr = v0.r * iw0 + v1.r * iw1 + v2.r * iw2;
                fg = v0.g * iw0 + v1.g * iw1 + v2.g * iw2;
                fb = v0.b * iw0 + v1.b * iw1 + v2.b * iw2;
                fa = v0.a * iw0 + v1.a * iw1 + v2.a * iw2;
            } else {
                fr = v2.r;
                fg = v2.g;
                fb = v2.b;
                fa = v2.a;
            }

            if (gs->tme) {
                // Fixed-point 12:4
                int u, v;

                if (gs->fst) {
                    u = v0.u * iw0 + v1.u * iw1 + v2.u * iw2;
                    v = v0.v * iw0 + v1.v * iw1 + v2.v * iw2;
                } else {
                    float s = v0.s * iw0 + v1.s * iw1 + v2.s * iw2;
                    float t = v0.t * iw0 + v1.t * iw1 + v2.t * iw2;
                    float q = v0.q * iw0 + v1.q * iw1 + v2.q * iw2;

                    float uf = (s / q) * gs->ctx->usize;
                    float vf = (t / q) * gs->ctx->vsize;

                    // Convert to 12:4 fixed-point
                    u = uf * (1 << 4);
                    v = vf * (1 << 4);
                }

                u = gs_clamp_u(gs, u);
                v = gs_clamp_v(gs, v);

                uint32_t f = fr | (fg << 8) | (fb << 16) | (fa << 24);
                uint32_t t = gs_read_tb(gs, u, v);
                t = gs_to_rgba32(gs, t, gs->ctx->tbpsm);
                t = gs_apply_function(gs, t, f);

                fr = t & 0xff;
                fg = (t >> 8) & 0xff;
                fb = (t >> 16) & 0xff;
                fa = t >> 24;
            }

            uint32_t fz = (uint32_t)floorf((double)v0.z * iw0 + (double)v1.z * iw1 + (double)v2.z * iw2);

            int tr = gs_test_pixel(gs, p.x, p.y, fz, fa);

            if (tr == TR_FAIL) {
                w0 += a12;
                w1 += a20;
                w2 += a01;

                continue;
            }

            uint32_t fc = fr | (fg << 8) | (fb << 16) | (fa << 24);

            gs_draw_pixel(gs, p.x, p.y, fz, fc);

            // One step to the right
            w0 += a12;
            w1 += a20;
            w2 += a01;
        }

        // One row step
        w0_row += b12;
        w1_row += b20;
        w2_row += b01;
    }

    // gs_draw_wireframe(gs, gs->vq[0], gs->vq[1]);
    // gs_draw_wireframe(gs, gs->vq[1], gs->vq[2]);
    // gs_draw_wireframe(gs, gs->vq[2], gs->vq[0]);
}

void render_sprite(struct ps2_gs* gs, void* udata) {
    // gs_draw_wireframe(gs, dv0, dv2);

    // if (gs->tme) {
    //     struct gs_vertex dv0 = gs->vq[0];
    //     struct gs_vertex dv1 = gs->vq[0];
    //     struct gs_vertex dv2 = gs->vq[1];
    //     struct gs_vertex dv3 = gs->vq[1];

    //     dv1.x = gs->vq[1].x;
    //     dv1.y = gs->vq[0].y;
    //     dv3.x = gs->vq[0].x;
    //     dv3.y = gs->vq[1].y;

    //     gs_draw_wireframe(gs, dv0, dv1);
    //     gs_draw_wireframe(gs, dv1, dv2);
    //     gs_draw_wireframe(gs, dv2, dv3);
    //     gs_draw_wireframe(gs, dv3, dv0);
    //     return;
    // }

    struct gs_vertex v0 = gs->vq[0];
    struct gs_vertex v1 = gs->vq[1];

    v0.x -= gs->ctx->ofx;
    v1.x -= gs->ctx->ofx;
    v0.y -= gs->ctx->ofy;
    v1.y -= gs->ctx->ofy;

    int xmin = (int32_t)std::min(v0.x, v1.x);
    int ymin = (int32_t)std::min(v0.y, v1.y);
    int xmax = (int32_t)std::max(v0.x, v1.x);
    int ymax = (int32_t)std::max(v0.y, v1.y);

    int z = v1.z;
    int a = v1.a;

    // printf("gs: Sprite at (%d,%d-%d,%d) min=(%d,%d) max=(%d,%d) fbp=%x fbpsm=%x tme=%d tbp=%x rgba=%08lx scissor=(%d,%d-%d,%d) ctx=%d\n",
    //     v0.x, v0.y,
    //     v1.x, v1.y,
    //     xmin, ymin,
    //     xmax, ymax,
    //     gs->ctx->fbp,
    //     gs->ctx->fbpsm,
    //     gs->tme,
    //     gs->ctx->tbp0,
    //     v1.rgbaq & 0xffffffff,
    //     gs->ctx->scax0, gs->ctx->scay0,
    //     gs->ctx->scax1, gs->ctx->scay1,
    //     gs->ctxt
    // );

    // if (gs->tme && gs->abe) {
    //     printf("gs: Sprite at (%d,%d-%d,%d) tcc=%d tfx=%d rgba=%08x a=%d b=%d c=%d d=%d fix=%02x tbpsm=%02x\n",
    //         v0.x, v0.y,
    //         v1.x, v1.y,
    //         gs->ctx->tcc,
    //         gs->ctx->tfx,
    //         v1.rgbaq & 0xffffffff,
    //         gs->ctx->a,
    //         gs->ctx->b,
    //         gs->ctx->c,
    //         gs->ctx->d,
    //         gs->ctx->fix,
    //         gs->ctx->tbpsm
    //     );
    // }
    // if (gs->tme) {
    //     printf("gs: TB format=%d (0x%02x) tbp=%x tbw=%d uv=(%d,%d) TEX w=%d h=%d CB format=%d cbp=%x csm=%d tfx=%d rgba=%08lx abe=%d FB format=%d fbw=%d fba=%ld\n",
    //         gs->ctx->tbpsm,
    //         gs->ctx->tbpsm,
    //         gs->ctx->tbp0,
    //         gs->ctx->tbw,
    //         v0.u,
    //         v0.v,
    //         gs->ctx->usize,
    //         gs->ctx->vsize,
    //         gs->ctx->cbpsm,
    //         gs->ctx->cbp,
    //         gs->ctx->csm,
    //         gs->ctx->tfx,
    //         v1.rgbaq & 0xffffffff,
    //         gs->abe,
    //         gs->ctx->fbpsm,
    //         gs->ctx->fbw,
    //         gs->ctx->fba// ,
    //         // gs->ctx->a,
    //         // gs->ctx->b,
    //         // gs->ctx->c,
    //         // gs->ctx->d,
    //         // gs->ctx->tcc,
    //         // gs->ctx->fix
    //     );
    // }

    // if (gs->tme) {
    //     printf("gs: (%d,%d)-(%d,%d) (%d,%d)-(%d,%d)\n",
    //         v0.x, v0.y,
    //         v1.x, v1.y,
    //         v0.u, v0.v,
    //         v1.u, v1.v
    //     );
    // }

    // if (!gs->fst) {
    //     printf("gs: stq0=(%f,%f,%f) stq1=(%f,%f,%f)\n",
    //         v0.s, v0.t, v0.q,
    //         v1.s, v1.t, v1.q
    //     );
    // } else {
    //     printf("gs: uv0=(%d,%d) uv1=(%d,%d)\n",
    //         v0.u, v0.v,
    //         v1.u, v1.v
    //     );
    // }

    // if (!gs->tme)
    //     return;

    float u = v0.u;
    float v = v0.v;

    for (int y = ymin; y < ymax; y++) {
        for (int x = xmin; x < xmax; x++) {
            if (!gs_test_scissor(gs, x, y))
                continue;

            uint32_t c = v1.rgbaq & 0xffffffff;

            if (gs->tme) {
                float tx = (float)(x - v0.x) / (float)(v1.x - v0.x);
                float ty = (float)(y - v0.y) / (float)(v1.y - v0.y);

                if (gs->fst) {
                    u = v0.u + (v1.u - v0.u) * tx;
                    v = v0.v + (v1.v - v0.v) * ty;
                } else {
                    u = v0.s + (v1.s - v0.s) * tx;
                    v = v0.t + (v1.t - v0.t) * ty;
                    
                    u *= gs->ctx->usize;
                    v *= gs->ctx->vsize;
                }

                u = gs_clamp_u(gs, u);
                v = gs_clamp_v(gs, v);

                c = gs_read_tb(gs, u, v);
                c = gs_to_rgba32(gs, c, gs->ctx->tbpsm);
                c = gs_apply_function(gs, c, v1.rgbaq & 0xffffffff);

                a = c >> 24;
            }

            gs_draw_pixel(gs, x, y, z, c);
        }
    }

    // struct gs_vertex dv0 = gs->vq[0];
    // struct gs_vertex dv1 = gs->vq[0];
    // struct gs_vertex dv2 = gs->vq[1];
    // struct gs_vertex dv3 = gs->vq[1];

    // dv1.x = gs->vq[1].x;
    // dv1.y = gs->vq[0].y;
    // dv3.x = gs->vq[0].x;
    // dv3.y = gs->vq[1].y;

    // gs_draw_wireframe(gs, dv0, dv1);
    // gs_draw_wireframe(gs, dv1, dv2);
    // gs_draw_wireframe(gs, dv2, dv3);
    // gs_draw_wireframe(gs, dv3, dv0);
}

void render(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    int en1 = ctx->gs->pmode & 1;
    int en2 = (ctx->gs->pmode >> 1) & 1;

    uint32_t dfbp = 0;
    int dfb = 0;

    if (en1) {
        dfb = 0;
        dfbp = ctx->gs->dispfb1;
    } else if (en2) {
        dfb = 1;
        dfbp = ctx->gs->dispfb2;
    }

    uint32_t* ptr = &gs->vram[dfbp];

    // printf("fbp=%x fbw=%d fbpsm=%d stride=%d dy=%d\n", dfbp, dfbw, dfbpsm, stride, dy);

    if (!ctx->tex_w)
        return;

    if ((ctx->gs->smode2 & 3) == 3) {
        // Need to deinterlace
        int bpp = 0;
        int odd = ((ctx->gs->csr >> 13) & 1) == 0;

        switch (ctx->disp_fmt) {
            case GS_PSMCT32:
            case GS_PSMCT24: {
                bpp = 4;

                for (int y = 0; y < ctx->tex_h / 2; y++) {
                    for (int x = 0; x < ctx->tex_w; x++) {
                        uint32_t* dst = buf + x + (((y * 2) + odd) * ctx->tex_w);

                        *dst = gs_read_dispfb(gs, x, y, dfb);
                    }
                }
            } break;
            case GS_PSMCT16:
            case GS_PSMCT16S: {
                for (int y = 0; y < ctx->tex_h / 2; y++) {
                    for (int x = 0; x < ctx->tex_w; x++) {
                        uint16_t* dst = ((uint16_t*)buf) + x + (((y * 2) + odd) * ctx->tex_w);

                        *dst = gs_read_dispfb(gs, x, y, dfb);
                    }
                }
            } break;
        }
    } else {
        switch (ctx->disp_fmt) {
            case GS_PSMCT32:
            case GS_PSMCT24: {
                for (int y = 0; y < ctx->tex_h; y++) {
                    for (int x = 0; x < ctx->tex_w; x++) {
                        uint32_t* dst = buf + x + (y * ctx->tex_w);

                        *dst = gs_read_dispfb(gs, x, y, dfb);
                    }
                }
            } break;
            case GS_PSMCT16:
            case GS_PSMCT16S: {
                for (int y = 0; y < ctx->tex_h; y++) {
                    for (int x = 0; x < ctx->tex_w; x++) {
                        uint16_t* dst = ((uint16_t*)buf) + x + (y * ctx->tex_w);

                        *dst = gs_read_dispfb(gs, x, y, dfb);
                    }
                }
            } break;
        }
    }

    ptr = buf;

    SDL_Rect size, rect;

    rect.w = ctx->tex_w;
    rect.h = ctx->tex_h;

    SDL_GetWindowSize(ctx->window, &size.w, &size.h);

    float scale = ctx->integer_scaling ? floorf(ctx->scale) : ctx->scale;

    switch (ctx->aspect_mode) {
        case RENDERER_ASPECT_NATIVE: {
            rect.w *= scale;
            rect.h *= scale;
        } break;

        case RENDERER_ASPECT_4_3: {
            rect.w *= scale;
            rect.h = (float)rect.w * (3.0f / 4.0f);
        } break;

        case RENDERER_ASPECT_16_9: {
            rect.w *= scale;
            rect.h = (float)rect.w * (9.0f / 16.0f);
        } break;

        case RENDERER_ASPECT_5_4: {
            rect.w *= scale;
            rect.h = (float)rect.w * (4.0f / 5.0f);
        } break;

        case RENDERER_ASPECT_STRETCH: {
            rect.w = size.w;
            rect.h = size.h;
        } break;

        case RENDERER_ASPECT_AUTO:
        case RENDERER_ASPECT_STRETCH_KEEP: {
            rect.h = size.h;
            rect.w = (float)rect.h * (4.0f / 3.0f);
        } break;
    }

    ctx->disp_w = rect.w;
    ctx->disp_h = rect.h;

    rect.x = (size.w / 2) - (rect.w / 2);
    rect.y = (size.h / 2) - (rect.h / 2);

    float x0 = (rect.x / ((float)size.w / 2.0f)) - 1.0f;
    float y0 = (rect.y / ((float)size.h / 2.0f)) - 1.0f;
    float x1 = ((rect.x + rect.w) / ((float)size.w / 2.0f)) - 1.0f;
    float y1 = ((rect.y + rect.h) / ((float)size.h / 2.0f)) - 1.0f;

    GLfloat vertices[] = {
        // positions   // texture coords
        x1, y1, 0.0f,  1.0f, 1.0f,   // top right
        x1, y0, 0.0f,  1.0f, 0.0f,   // bottom right
        x0, y0, 0.0f,  0.0f, 0.0f,   // bottom left
        x0, y1, 0.0f,  0.0f, 1.0f    // top left 
    };

    GLuint indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    glBindVertexArray(ctx->vao);

    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    int internal_format, format;

    switch (ctx->disp_fmt) {
        case GS_PSMCT32:
        case GS_PSMCT24: {
            internal_format = GL_RGBA;
            format = GL_UNSIGNED_BYTE;
        } break;
        case GS_PSMCT16:
        case GS_PSMCT16S: {
            internal_format = GL_RGBA;
            format = GL_UNSIGNED_SHORT_1_5_5_5_REV;
        } break;
    }

    // Update screen texture
    glBindTexture(GL_TEXTURE_2D, ctx->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, ctx->tex_w, ctx->tex_h, 0, internal_format, format, ptr);

    if (!ctx->programs.size()) {
        // No shaders present, use default shader
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ctx->tex);

        glViewport(0, 0, size.w, size.h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(ctx->default_program);

        glUniform1i(glGetUniformLocation(ctx->default_program, "input_texture"), 0);
        glUniform2f(glGetUniformLocation(ctx->default_program, "screen_size"), (float)rect.w, (float)rect.h);
        glUniform1i(glGetUniformLocation(ctx->default_program, "frame"), (ctx->gs->csr >> 12) & 1);

        glBindVertexArray(ctx->vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);

        return;
    } else if (ctx->programs.size() == 1) {
        // Only 1 shader present, use default framebuffer
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ctx->tex);

        glViewport(0, 0, size.w, size.h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(ctx->programs[0]);

        glUniform1i(glGetUniformLocation(ctx->programs[0], "input_texture"), 0);
        glUniform2f(glGetUniformLocation(ctx->programs[0], "screen_size"), (float)rect.w, (float)rect.h);
        glUniform1i(glGetUniformLocation(ctx->programs[0], "frame"), (ctx->gs->csr >> 12) & 1);

        glBindVertexArray(ctx->vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);

        return;
    }

    // More than 1 shader present, create framebuffer and do render graph
    if (ctx->fb_out_tex) {
        glDeleteTextures(1, &ctx->fb_out_tex);
    }

    glGenTextures(1, &ctx->fb_out_tex);
    glBindTexture(GL_TEXTURE_2D, ctx->fb_out_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, rect.w, rect.h, 0, internal_format, format, NULL);

    if (ctx->fb_in_tex) {
        glDeleteTextures(1, &ctx->fb_in_tex);
    }

    glGenTextures(1, &ctx->fb_in_tex);
    glBindTexture(GL_TEXTURE_2D, ctx->fb_in_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, rect.w, rect.h, 0, internal_format, format, NULL);

    if (ctx->fbo) {
        glDeleteFramebuffers(1, &ctx->fbo);
    }

    glGenFramebuffers(1, &ctx->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->fb_out_tex, 0); 

    // first pass
    // Bind screen texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->tex);

    glViewport(0, 0, rect.w, rect.h);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
    glUseProgram(ctx->programs[0]);

    glUniform1i(glGetUniformLocation(ctx->programs[0], "input_texture"), 0);
    glUniform2f(glGetUniformLocation(ctx->programs[0], "screen_size"), (float)ctx->tex_w, (float)ctx->tex_h);
    glUniform1i(glGetUniformLocation(ctx->programs[0], "frame"), (ctx->gs->csr >> 12) & 1);

    glBindVertexArray(ctx->fb_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    int output_tex, input_tex;

    // "middle" passes
    for (int i = 1; i < (int)ctx->programs.size() - 1; i++) {
        // Bind framebuffer texture for middle and last passes
        input_tex = (i & 1) ? ctx->fb_out_tex : ctx->fb_in_tex;
        output_tex = (i & 1) ? ctx->fb_in_tex : ctx->fb_out_tex;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_tex, 0);
    
        glUseProgram(ctx->programs[i]);

        glUniform1i(glGetUniformLocation(ctx->programs[i], "input_texture"), 0);
        glUniform2f(glGetUniformLocation(ctx->programs[i], "screen_size"), (float)rect.w, (float)rect.h);
        glUniform1i(glGetUniformLocation(ctx->programs[i], "frame"), (ctx->gs->csr >> 12) & 1);

        glBindVertexArray(ctx->fb_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, output_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ctx->bilinear ? GL_LINEAR : GL_NEAREST);

    // last pass
    glViewport(0, 0, size.w, size.h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(ctx->programs.back());

    glUniform1i(glGetUniformLocation(ctx->programs.back(), "input_texture"), 0);
    glUniform2f(glGetUniformLocation(ctx->programs.back(), "screen_size"), (float)rect.w, (float)rect.h);
    glUniform1i(glGetUniformLocation(ctx->programs.back(), "frame"), (ctx->gs->csr >> 12) & 1);

    glBindVertexArray(ctx->vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

static inline int gs_pixels_to_size(int fmt, int px) {
    switch (fmt) {
        case GS_PSMCT32:
        case GS_PSMCT24:
        case GS_PSMT8H:
        case GS_PSMT4HL:
        case GS_PSMT4HH:
            return px;
        case GS_PSMCT16:
        case GS_PSMCT16S:
            return px >> 1;
        case GS_PSMT8:
            return px >> 2;
        case GS_PSMT4:
            return px >> 3;
    }
    return px;
}

void transfer_start(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->dbp = (gs->bitbltbuf >> 32) & 0x3fff;
    ctx->dbw = (gs->bitbltbuf >> 48) & 0x3f;
    ctx->dpsm = (gs->bitbltbuf >> 56) & 0x3f;
    ctx->sbp = (gs->bitbltbuf >> 0) & 0x3fff;
    ctx->sbw = (gs->bitbltbuf >> 16) & 0x3f;
    ctx->spsm = (gs->bitbltbuf >> 24) & 0x3f;
    ctx->dsax = (gs->trxpos >> 32) & 0x7ff;
    ctx->dsay = (gs->trxpos >> 48) & 0x7ff;
    ctx->ssax = (gs->trxpos >> 0) & 0x7ff;
    ctx->ssay = (gs->trxpos >> 16) & 0x7ff;
    ctx->dir = (gs->trxpos >> 59) & 3;
    ctx->rrw = (gs->trxreg >> 0) & 0xfff;
    ctx->rrh = (gs->trxreg >> 32) & 0xfff;
    ctx->xdir = gs->trxdir & 3;
    ctx->dx = ctx->dsax;
    ctx->dy = ctx->dsay;
    ctx->sx = ctx->ssax;
    ctx->sy = ctx->ssay;
    ctx->px = 0;

    // uint32_t dbp = ctx->dbp;

    // ctx->dbp <<= 6;
    // ctx->dbw <<= 6;
    // ctx->sbp <<= 6;
    // ctx->sbw <<= 6;

    // uint32_t dbw = ctx->dbw;

    // ctx->dbw = gs_pixels_to_size(ctx->dpsm, ctx->dbw);
    // ctx->sbw = gs_pixels_to_size(ctx->spsm, ctx->sbw);

    ctx->psmct24_data = 0;
    ctx->psmct24_shift = 0;

    // FILE* file = fopen("gs.dump", "a");
    // printf("dbp=%x (%x) dbw=%d (%d) dpsm=%02x dsa=(%d,%d) rr=(%d,%d) xdir=%d\n",
    //     ctx->dbp, ctx->dbp,
    //     ctx->dbw, ctx->dbw,
    //     ctx->dpsm,
    //     ctx->dsax,
    //     ctx->dsay,
    //     ctx->rrw,
    //     ctx->rrh,
    //     ctx->xdir
    // );
    // fclose(file);

    if (ctx->xdir == 2) {
        software_thread_vram_blit(gs, ctx);
    }
}

static inline void gs_write_psmct32(struct ps2_gs* gs, software_thread_state* ctx, uint32_t data) {
    uint32_t addr = psmct32_addr(ctx->dbp, ctx->dbw, ctx->dx, ctx->dy);

    ctx->dx++;

    gs->vram[addr & 0xfffff] = data;

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_write_psmt4hh(struct ps2_gs* gs, software_thread_state* ctx, uint32_t data) {
    uint32_t addr = psmct32_addr(ctx->dbp, ctx->dbw, ctx->dx, ctx->dy);

    ctx->dx++;

    gs->vram[addr] = (gs->vram[addr] & 0x0fffffff) | (data << 28);

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_write_psmt4hl(struct ps2_gs* gs, software_thread_state* ctx, uint32_t data) {
    uint32_t addr = psmct32_addr(ctx->dbp, ctx->dbw, ctx->dx, ctx->dy);

    ctx->dx++;

    gs->vram[addr] = (gs->vram[addr] & 0xf0ffffff) | (data << 24);

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_write_psmt4(struct ps2_gs* gs, software_thread_state* ctx, uint32_t index) {
    uint32_t addr = psmt4_addr(ctx->dbp, ctx->dbw, ctx->dx, ctx->dy) & 0xfffff;

    int idx = (ctx->dx & 31) + ((ctx->dy & 3) * 32);
    int shift = psmt4_shift[idx];

    uint32_t mask = 0xful << shift;

    gs->vram[addr] = (gs->vram[addr] & ~mask) | (index << shift);

    ctx->dx++;

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_write_psmt8(struct ps2_gs* gs, software_thread_state* ctx, uint32_t index) {
    uint32_t addr = psmt8_addr(ctx->dbp, ctx->dbw, ctx->dx, ctx->dy) & 0xfffff;
    uint8_t* vram = (uint8_t*)(&gs->vram[addr]);

    int idx = (ctx->dx & 15) + ((ctx->dy & 3) * 16);

    vram[psmt8_shift[idx]] = index;

    ctx->dx++;

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_write_psmt8h(struct ps2_gs* gs, software_thread_state* ctx, uint32_t data) {
    uint32_t addr = psmct32_addr(ctx->dbp, ctx->dbw, ctx->dx, ctx->dy);

    ctx->dx++;

    gs->vram[addr] = (gs->vram[addr] & 0x00ffffff) | (data << 24);

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_store_hwreg_psmt4(struct ps2_gs* gs, software_thread_state* ctx) {
    for (int i = 0; i < 16; i++) {
        uint64_t index = (gs->hwreg >> (i * 4)) & 0xf;

        gs_write_psmt4(gs, ctx, index);
    }
}

static inline void gs_store_hwreg_psmt4hh(struct ps2_gs* gs, software_thread_state* ctx) {
    for (int i = 0; i < 16; i++) {
        uint64_t index = (gs->hwreg >> (i * 4)) & 0xf;

        gs_write_psmt4hh(gs, ctx, index);
    }
}

static inline void gs_store_hwreg_psmt4hl(struct ps2_gs* gs, software_thread_state* ctx) {
    for (int i = 0; i < 16; i++) {
        uint64_t index = (gs->hwreg >> (i * 4)) & 0xf;

        gs_write_psmt4hl(gs, ctx, index);
    }
}

static inline void gs_store_hwreg_psmt8(struct ps2_gs* gs, software_thread_state* ctx) {
    for (int i = 0; i < 8; i++) {
        uint64_t index = (gs->hwreg >> (i * 8)) & 0xff;

        gs_write_psmt8(gs, ctx, index);
    }
}

static inline void gs_store_hwreg_psmt8h(struct ps2_gs* gs, software_thread_state* ctx) {
    for (int i = 0; i < 8; i++) {
        uint64_t index = (gs->hwreg >> (i * 8)) & 0xff;

        gs_write_psmt8h(gs, ctx, index);
    }
}

static inline void gs_store_hwreg_psmct32(struct ps2_gs* gs, software_thread_state* ctx) {
    uint64_t data[2] = {
        gs->hwreg & 0xffffffff,
        gs->hwreg >> 32
    };

    for (int i = 0; i < 2; i++) {
        gs_write_psmct32(gs, ctx, data[i]);
    }
}

static inline void gs_write_psmct24(struct ps2_gs* gs, software_thread_state* ctx, uint32_t data) {
    uint32_t addr = psmct32_addr(ctx->dbp, ctx->dbw, ctx->dx++, ctx->dy);

    gs->vram[addr] = (gs->vram[addr] & 0xff000000) | (data & 0xffffff);

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_store_hwreg_psmct24(struct ps2_gs* gs, software_thread_state* ctx) {
    switch (ctx->psmct24_shift++) {
        case 0: {
            gs_write_psmct24(gs, ctx, gs->hwreg);
            gs_write_psmct24(gs, ctx, gs->hwreg >> 24);
            
            ctx->psmct24_data = gs->hwreg >> 48;
        } break;

        case 1: {
            gs_write_psmct24(gs, ctx, ctx->psmct24_data | ((gs->hwreg & 0xff) << 16));
            gs_write_psmct24(gs, ctx, gs->hwreg >> 8);
            gs_write_psmct24(gs, ctx, gs->hwreg >> 32);

            ctx->psmct24_data = gs->hwreg >> 56;
        } break;

        case 2: {
            gs_write_psmct24(gs, ctx, ctx->psmct24_data | ((gs->hwreg & 0xffff) << 8));
            gs_write_psmct24(gs, ctx, gs->hwreg >> 16);
            gs_write_psmct24(gs, ctx, gs->hwreg >> 40);

            ctx->psmct24_shift = 0;
        } break;
    }
}

static inline void gs_write_psmct16(struct ps2_gs* gs, software_thread_state* ctx, uint32_t index) {
    uint32_t addr = psmct16_addr(ctx->dbp, ctx->dbw, ctx->dx, ctx->dy) & 0xfffff;
    uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

    int idx = (ctx->dx & 15) + ((ctx->dy & 1) * 16);

    vram[psmct16_shift[idx]] = index;

    ctx->dx++;

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_write_psmct16s(struct ps2_gs* gs, software_thread_state* ctx, uint32_t index) {
    uint32_t addr = psmct16s_addr(ctx->dbp, ctx->dbw, ctx->dx, ctx->dy) & 0xfffff;
    uint16_t* vram = (uint16_t*)(&gs->vram[addr]);

    int idx = (ctx->dx & 15) + ((ctx->dy & 1) * 16);

    vram[psmct16_shift[idx]] = index;

    ctx->dx++;

    if (ctx->dx == (ctx->rrw + ctx->dsax)) {
        ctx->dx = ctx->dsax;
        ctx->dy++;
    }
}

static inline void gs_store_hwreg_psmct16(struct ps2_gs* gs, software_thread_state* ctx) {
    for (int i = 0; i < 4; i++) {
        uint64_t p = (gs->hwreg >> (i * 16)) & 0xffff;

        gs_write_psmct16(gs, ctx, p);
    }
}

static inline void gs_store_hwreg_psmct16s(struct ps2_gs* gs, software_thread_state* ctx) {
    for (int i = 0; i < 4; i++) {
        uint64_t p = (gs->hwreg >> (i * 16)) & 0xffff;

        gs_write_psmct16s(gs, ctx, p);
    }
}

void transfer_write(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    switch (ctx->dpsm) {
        case GS_PSMCT32: {
            gs_store_hwreg_psmct32(gs, ctx);
        } break;

        case GS_PSMCT24: {
            gs_store_hwreg_psmct24(gs, ctx);
        } break;

        case GS_PSMCT16: {
            gs_store_hwreg_psmct16(gs, ctx);
        } break;

        case GS_PSMCT16S: {
            gs_store_hwreg_psmct16s(gs, ctx);
        } break;

        case GS_PSMT8: {
            gs_store_hwreg_psmt8(gs, ctx);
        } break;

        case GS_PSMT8H: {
            gs_store_hwreg_psmt8h(gs, ctx);
        } break;

        case GS_PSMT4: {
            gs_store_hwreg_psmt4(gs, ctx);
        } break;

        case GS_PSMT4HH: {
            gs_store_hwreg_psmt4hh(gs, ctx);
        } break;

        case GS_PSMT4HL: {
            gs_store_hwreg_psmt4hl(gs, ctx);
        } break;

        default: {
            gs_store_hwreg_psmct32(gs, ctx);
        } break;
    }
}

void transfer_read(struct ps2_gs* gs, void* udata) {
    gs->hwreg = 0;
}

extern "C" void software_thread_render_point(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->queue_mtx.lock();
    ctx->render_queue.push(render_data());

    render_data& rdata = ctx->render_queue.back();

    rdata.gs = *gs;
    rdata.prim = 0;
    ctx->queue_mtx.unlock();
}

extern "C" void software_thread_render_line(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->queue_mtx.lock();
    ctx->render_queue.push(render_data());

    render_data& rdata = ctx->render_queue.back();

    rdata.gs = *gs;
    rdata.prim = 1;
    ctx->queue_mtx.unlock();
}

extern "C" void software_thread_render_triangle(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->queue_mtx.lock();
    ctx->render_queue.push(render_data());

    render_data& rdata = ctx->render_queue.back();

    rdata.gs = *gs;
    rdata.prim = 2;
    ctx->queue_mtx.unlock();
}

extern "C" void software_thread_render_sprite(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    ctx->queue_mtx.lock();
    ctx->render_queue.push(render_data());

    render_data& rdata = ctx->render_queue.back();

    rdata.gs = *gs;
    rdata.prim = 3;
    ctx->queue_mtx.unlock();
}

extern "C" void software_thread_render(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    // Block until queue is empty
    while (true) {
        ctx->queue_mtx.lock();

        if (ctx->render_queue.empty()) {
            ctx->queue_mtx.unlock();

            break;
        }

        ctx->queue_mtx.unlock();
    }

    ctx->render_mtx.lock();

    render(gs, ctx);

    ctx->render_mtx.unlock();
}

extern "C" void software_thread_transfer_start(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    // Block until queue is empty
    while (true) {
        ctx->queue_mtx.lock();

        if (ctx->render_queue.empty()) {
            ctx->queue_mtx.unlock();

            break;
        }

        ctx->queue_mtx.unlock();
    }

    ctx->render_mtx.lock();

    transfer_start(gs, ctx);

    ctx->render_mtx.unlock();
}

extern "C" void software_thread_transfer_write(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    // Block until queue is empty
    while (true) {
        ctx->queue_mtx.lock();

        if (ctx->render_queue.empty()) {
            ctx->queue_mtx.unlock();

            break;
        }

        ctx->queue_mtx.unlock();
    }

    ctx->render_mtx.lock();

    transfer_write(gs, ctx);

    ctx->render_mtx.unlock();
}

extern "C" void software_thread_transfer_read(struct ps2_gs* gs, void* udata) {
    software_thread_state* ctx = (software_thread_state*)udata;

    // Block until queue is empty
    while (true) {
        ctx->queue_mtx.lock();

        if (ctx->render_queue.empty()) {
            ctx->queue_mtx.unlock();

            break;
        }

        ctx->queue_mtx.unlock();
    }

    ctx->render_mtx.lock();

    transfer_read(gs, ctx);

    ctx->render_mtx.unlock();
}
