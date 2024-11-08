#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include "GL/gl3w.h"

#include "gs_opengl.h"

const char* vert_shader_src =
    "#version 330 core\n"
    "layout (location = 0) in vec3 pos;\n"
    "void main() {\n"
    "   gl_Position = vec4(pos.x, pos.y, pos.z, 1.0);\n"
    "}";

const char* frag_shader_src = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}";

float vertices[] = {
     0.5f,  0.5f, 0.0f,  // top right
     0.5f, -0.5f, 0.0f,  // bottom right
    -0.5f,  0.5f, 0.0f,  // top left 
     0.5f, -0.5f, 0.0f,  // bottom right
    -0.5f, -0.5f, 0.0f,  // bottom left
    -0.5f,  0.5f, 0.0f   // top left
}; 

unsigned int indices[] = {
    0, 1, 3,
    1, 2, 3
};

int gl_compile_shader(const char* src, GLint type) {
    unsigned int shader = glCreateShader(type);

    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    int success;
    char infolog[512];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infolog);

        printf("gs_opengl: Shader compilation failed\n%s", infolog);

        exit(1);
    
        return 0;
    }

    return shader;
}

void gsr_gl_init(struct gsr_opengl* ctx) {
    gl3wInit();

    int vert_shader = gl_compile_shader(vert_shader_src, GL_VERTEX_SHADER);
    int frag_shader = gl_compile_shader(frag_shader_src, GL_FRAGMENT_SHADER);

    char infolog[512];
    int success;

    ctx->program = glCreateProgram();

    glAttachShader(ctx->program, vert_shader);
    glAttachShader(ctx->program, frag_shader);
    glLinkProgram(ctx->program);
    glGetProgramiv(ctx->program, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(ctx->program, 512, NULL, infolog);

        printf("gs_opengl: Program linking failed\n%s", infolog);

        exit(1);
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    glGenVertexArrays(1, &ctx->vao);
    glGenBuffers(1, &ctx->vbo);
    glGenBuffers(1, &ctx->ebo);

    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(ctx->vao);

    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ebo);
    // glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // note that this is allowed, the call to glVertexAttribPointer registered ctx.vbo as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // remember: do NOT unbind the ctx.ebo while a ctx.vao is active as the bound element buffer object IS stored in the ctx.vao; keep the ctx.ebo bound.
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // You can unbind the ctx.vao afterwards so other ctx.vao calls won't accidentally modify this ctx.vao, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0);

    ctx->vi = 0;
}

void gsr_gl_render_point(struct ps2_gs* gs, void* udata) {
    struct gsr_opengl* ctx = (struct gsr_opengl*)udata;

    float x = (gs->vq[0].xyz2 & 0xffff) >> 4;
    float y = (gs->vq[0].xyz2 & 0xffff0000) >> 20;
    float z = gs->vq[0].xyz2 >> 32;

    ctx->v[ctx->vi++] = 0.2;
    ctx->v[ctx->vi++] = 0.2;
    ctx->v[ctx->vi++] = z;
}
void gsr_gl_render_line(struct ps2_gs* gs, void* udata) {}
void gsr_gl_render_line_strip(struct ps2_gs* gs, void* udata) {}
void gsr_gl_render_triangle(struct ps2_gs* gs, void* udata) {}
void gsr_gl_render_triangle_strip(struct ps2_gs* gs, void* udata) {}
void gsr_gl_render_triangle_fan(struct ps2_gs* gs, void* udata) {}
void gsr_gl_render_sprite(struct ps2_gs* gs, void* udata) {}
void gsr_gl_render(struct ps2_gs* gs, void* udata) {
    struct gsr_opengl* ctx = (struct gsr_opengl*)udata;

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!ctx->vi)
        return;

    glBindVertexArray(ctx->vao);

    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, ctx->vi * sizeof(float), ctx->v, GL_STREAM_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // draw our first triangle
    glUseProgram(ctx->program);
    glBindVertexArray(ctx->vao);
    glDrawArrays(GL_POINTS, 0, ctx->vi);

    ctx->vi = 0;
}