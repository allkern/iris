#pragma once

#include <vector>
#include <cstdio>
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include <mutex>

#include <SDL3/SDL.h>

#include "gs/gs.h"

#include "renderer.hpp"

struct render_data {
    int prim;
    struct ps2_gs gs;
};

struct gpu_vertex {
    float x, y, u, v;
};

struct software_thread_state {
    std::queue <render_data> render_queue;
    std::thread render_thr;
    std::mutex queue_mtx;
    std::mutex render_mtx;

    std::atomic <bool> end_signal;

    unsigned int sbp = 0, dbp = 0;
    unsigned int sbw = 0, dbw = 0;
    unsigned int spsm = 0, dpsm = 0;
    unsigned int ssax = 0, ssay = 0, dsax = 0, dsay = 0;
    unsigned int rrh = 0, rrw = 0;
    unsigned int dir = 0, xdir = 0;
    unsigned int sx = 0, sy = 0;
    unsigned int dx = 0, dy = 0, px = 0;

    uint32_t psmct24_data = 0;
    uint32_t psmct24_shift = 0;

    // SDL stuff
    SDL_Window* window = nullptr;
    SDL_GPUDevice* device = nullptr;
    SDL_GPUBuffer* vertex_buffer = nullptr;
    SDL_GPUBuffer* index_buffer = nullptr;
    SDL_GPUTexture* texture = nullptr;
    SDL_GPUSampler* sampler[2] = { nullptr };
    SDL_GPUGraphicsPipeline* pipeline = nullptr;

    // Context
    SDL_GPUCommandBuffer* cb = nullptr;
    SDL_GPURenderPass* rp = nullptr;

    struct ps2_gs* gs = nullptr;

    unsigned int window_x = 0, window_y = 0;
    unsigned int window_w = 0, window_h = 0;

    int tex_w = 0;
    int tex_h = 0;
    int disp_w = 0;
    int disp_h = 0;
    int disp_fmt = 0;
    int aspect_mode = 0; // Native
    bool integer_scaling = false;
    float scale = 1.5;
    bool bilinear = true;

    unsigned int frame = 0;

    uint32_t* buf = nullptr;
};

void software_thread_init(void* udata, struct ps2_gs* gs, SDL_Window* window, SDL_GPUDevice* device);
void software_thread_destroy(void* udata);
void software_thread_set_size(void* udata, int width, int height);
void software_thread_set_scale(void* udata, float scale);
void software_thread_set_aspect_mode(void* udata, int aspect_mode);
void software_thread_set_integer_scaling(void* udata, bool integer_scaling);
void software_thread_set_bilinear(void* udata, bool bilinear);
void software_thread_get_viewport_size(void* udata, int* w, int* h);
void software_thread_get_display_size(void* udata, int* w, int* h);
void software_thread_get_display_format(void* udata, int* fmt);
void software_thread_get_interlace_mode(void* udata, int* mode);
void software_thread_set_window_rect(void* udata, int x, int y, int w, int h);
void* software_thread_get_buffer_data(void* udata, int* w, int* h, int* bpp);
const char* software_thread_get_name(void* udata);
void software_thread_begin_render(void* udata, SDL_GPUCommandBuffer* command_buffer);
void software_thread_render(void* udata, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass);
void software_thread_end_render(void* udata, SDL_GPUCommandBuffer* command_buffer);

extern "C" {
void software_thread_render_point(struct ps2_gs* gs, void* udata);
void software_thread_render_line(struct ps2_gs* gs, void* udata);
void software_thread_render_triangle(struct ps2_gs* gs, void* udata);
void software_thread_render_sprite(struct ps2_gs* gs, void* udata);
void software_thread_transfer_start(struct ps2_gs* gs, void* udata);
void software_thread_transfer_write(struct ps2_gs* gs, void* udata);
void software_thread_transfer_read(struct ps2_gs* gs, void* udata);
}