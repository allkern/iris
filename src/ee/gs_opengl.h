#ifndef GS_OPENGL_H
#define GS_OPENGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <GL/gl.h>

#include "gs.h"

struct gsr_opengl {
    GLint program;
    GLuint vbo, vao, ebo;

    float v[512*3];
    int vi;
};

void gsr_gl_init(struct gsr_opengl* ctx);
void gsr_gl_render_point(struct ps2_gs* gs, void* udata);
void gsr_gl_render_line(struct ps2_gs* gs, void* udata);
void gsr_gl_render_line_strip(struct ps2_gs* gs, void* udata);
void gsr_gl_render_triangle(struct ps2_gs* gs, void* udata);
void gsr_gl_render_triangle_strip(struct ps2_gs* gs, void* udata);
void gsr_gl_render_triangle_fan(struct ps2_gs* gs, void* udata);
void gsr_gl_render_sprite(struct ps2_gs* gs, void* udata);
void gsr_gl_render(struct ps2_gs* gs, void* udata);

#ifdef __cplusplus
}
#endif

#endif