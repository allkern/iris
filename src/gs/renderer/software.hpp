#pragma once

#include <vector>
#include <cstdio>

#include <SDL.h>

#include "GL/gl3w.h"

#include "renderer.hpp"

struct software_state {
    unsigned int sbp, dbp;
    unsigned int sbw, dbw;
    unsigned int spsm, dpsm;
    unsigned int ssax, ssay, dsax, dsay;
    unsigned int rrh, rrw;
    unsigned int dir, xdir;
    unsigned int sx, sy;
    unsigned int dx, dy, px;

    uint32_t psmct24_data;
    uint32_t psmct24_shift;

    SDL_Window* window = nullptr;
    struct ps2_gs* gs;

    int tex_w = 0;
    int tex_h = 0;
    int disp_w = 0;
    int disp_h = 0;
    int disp_fmt = 0;
    int aspect_mode = 0; // Native
    bool integer_scaling = false;
    float scale = 1.5;
    bool bilinear = true;

    GLuint vert_shader = 0;

    std::vector <GLuint> programs;

    GLuint default_program;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint tex = 0;
    GLuint fbo = 0;
    GLuint fb_vao = 0;
    GLuint fb_vbo = 0;
    GLuint fb_ebo = 0;
    GLuint fb_in_tex = 0;
    GLuint fb_out_tex = 0;

    unsigned int frame = 0;
};

void software_init(void* ctx, struct ps2_gs* gs, SDL_Window* window);
void software_destroy(void* ctx);
void software_set_size(void* ctx, int width, int height);
void software_set_scale(void* ctx, float scale);
void software_set_aspect_mode(void* ctx, int aspect_mode);
void software_set_integer_scaling(void* ctx, bool integer_scaling);
void software_set_bilinear(void* ctx, bool bilinear);
void software_get_viewport_size(void* ctx, int* w, int* h);
void software_get_display_size(void* ctx, int* w, int* h);
void software_get_display_format(void* ctx, int* fmt);
void software_set_window_rect(void* udata, int x, int y, int w, int h);
const char* software_get_name(void* ctx);
// void software_push_shader(software_state* ctx, const char* path);
// void software_pop_shader(software_state* ctx);

extern "C" {
void software_render_point(struct ps2_gs* gs, void* udata);
void software_render_line(struct ps2_gs* gs, void* udata);
void software_render_triangle(struct ps2_gs* gs, void* udata);
void software_render_sprite(struct ps2_gs* gs, void* udata);
void software_render(struct ps2_gs* gs, void* udata);
void software_transfer_start(struct ps2_gs* gs, void* udata);
void software_transfer_write(struct ps2_gs* gs, void* udata);
void software_transfer_read(struct ps2_gs* gs, void* udata);
}