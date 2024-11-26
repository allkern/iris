#pragma once

#include <vector>
#include <cstdio>

#include "GL/gl3w.h"

struct opengl_vertex {
    float x, y, z;
    float r, g, b;
};

struct opengl_primitive {
    int type;
    int offset;
    int count;
};

struct opengl_state {
    std::vector <opengl_vertex> verts;
    std::vector <opengl_primitive> primitives;

    GLuint program;
    GLuint vbo, vao;
    GLuint texture;

    int width, height;
};

void opengl_init(opengl_state* ctx);
void opengl_set_size(opengl_state* ctx, int width, int height, float scale);

extern "C" {
void opengl_render_point(struct ps2_gs* gs, void* udata);
void opengl_render_line(struct ps2_gs* gs, void* udata);
void opengl_render_line_strip(struct ps2_gs* gs, void* udata);
void opengl_render_triangle(struct ps2_gs* gs, void* udata);
void opengl_render_triangle_strip(struct ps2_gs* gs, void* udata);
void opengl_render_triangle_fan(struct ps2_gs* gs, void* udata);
void opengl_render_sprite(struct ps2_gs* gs, void* udata);
void opengl_render(struct ps2_gs* gs, void* udata);
void opengl_transfer_start(struct ps2_gs* gs, void* udata);
void opengl_transfer_write(struct ps2_gs* gs, void* udata);
void opengl_transfer_read(struct ps2_gs* gs, void* udata);
}