#include "hardware.hpp"

// INCBIN stuff
#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include "incbin.h"

#define HW_MAX_VERTICES 1024

#ifdef __APPLE__
#define SHADER_FORMAT SDL_GPU_SHADERFORMAT_MSL
#define SHADER_ENTRYPOINT "main0"
INCBIN(hw_vertex_shader, "../shaders/vertex.msl");
INCBIN(hw_fragment_shader, "../shaders/fragment.msl");
INCBIN_EXTERN(hw_vertex_shader);
INCBIN_EXTERN(hw_fragment_shader);
#else
#define SHADER_FORMAT SDL_GPU_SHADERFORMAT_SPIRV
#define SHADER_ENTRYPOINT "main"
INCBIN(hw_vertex_shader, "../shaders/vertex.spv");
INCBIN(hw_fragment_shader, "../shaders/fragment.spv");
INCBIN_EXTERN(hw_vertex_shader);
INCBIN_EXTERN(hw_fragment_shader);
#endif

#include <SDL3/SDL.h>

void hardware_init(void* udata, struct ps2_gs* gs, SDL_Window* window, SDL_GPUDevice* device) {
    hardware_state* ctx = (hardware_state*)udata;
    
    ctx->window = window;
    ctx->device = device;
    ctx->gs = gs;

    // create the vertex shader
    SDL_GPUShaderCreateInfo vsci = {
        .code_size = g_hw_vertex_shader_size,
        .code = g_hw_vertex_shader_data,
        .entrypoint = SHADER_ENTRYPOINT,
        .format = SHADER_FORMAT,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 0,
    };

    // create the fragment shader
    SDL_GPUShaderCreateInfo fsci = {
        .code_size = g_hw_fragment_shader_size,
        .code = g_hw_fragment_shader_data,
        .entrypoint = SHADER_ENTRYPOINT,
        .format = SHADER_FORMAT,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 0,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 0,
    };
    
    SDL_GPUShader* vertex_shader = SDL_CreateGPUShader(device, &vsci);
    SDL_GPUShader* fragment_shader = SDL_CreateGPUShader(device, &fsci);

    // Create vertex buffer description and attributes
    SDL_GPUVertexBufferDescription vbd[1] = {{
        .slot = 0,
        .pitch = sizeof(hw_vertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0,
    }};

    SDL_GPUVertexAttribute va[] = {{
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT4,
        .offset = 0,
    }, {
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT2,
        .offset = sizeof(uint32_t) * 4,
    }, {
        .location = 2,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT3,
        .offset = sizeof(uint32_t) * 6,
    }, {
        .location = 3,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = sizeof(uint32_t) * 9,
    }};

    SDL_GPUColorTargetDescription ctd[1] = {{
        .format = SDL_GetGPUSwapchainTextureFormat(device, window),
        .blend_state = {
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .color_write_mask = SDL_GPU_COLORCOMPONENT_A,
            .enable_blend = false,
            .enable_color_write_mask = false,
        },
    }};

    // Create graphics pipeline
    SDL_GPUGraphicsPipelineCreateInfo gpci = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = {
            .vertex_buffer_descriptions = vbd,
            .num_vertex_buffers = 1,
            .vertex_attributes = va,
            .num_vertex_attributes = 4,
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info = {
            .color_target_descriptions = ctd,
            .num_color_targets = 1,
        },
    };

    gpci.depth_stencil_state.enable_depth_test = true;
    gpci.depth_stencil_state.enable_depth_write = false;
    gpci.depth_stencil_state.enable_stencil_test = false;
    gpci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    gpci.depth_stencil_state.write_mask = 0xFF;

    ctx->pipeline = SDL_CreateGPUGraphicsPipeline(device, &gpci);

    SDL_assert(ctx->pipeline);

    // we don't need to store the shaders after creating the pipeline
    SDL_ReleaseGPUShader(device, vertex_shader);
    SDL_ReleaseGPUShader(device, fragment_shader);

    ctx->vertex_buffer = nullptr;
}

void hardware_destroy(void* udata) {
    hardware_state* ctx = (hardware_state*)udata;
}
void hardware_set_size(void* udata, int width, int height) {
    hardware_state* ctx = (hardware_state*)udata;

    int en1 = ctx->gs->pmode & 1;
    int en2 = (ctx->gs->pmode >> 1) & 1;

    uint64_t display = 0, dispfb = 0;

    if (en1) {
        display = ctx->gs->display1;
        dispfb = ctx->gs->dispfb1;
    } else if (en2) {
        display = ctx->gs->display2;
        dispfb = ctx->gs->dispfb2;
    }

    uint32_t tex_fmt = (dispfb >> 15) & 0x1f;

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

    ctx->tex_w = tex_w;
    ctx->tex_h = tex_h;

    if ((ctx->gs->smode2 & 3) == 3)
        ctx->tex_h /= 2;
}
void hardware_set_scale(void* udata, float scale) {}
void hardware_set_aspect_mode(void* udata, int aspect_mode) {}
void hardware_set_integer_scaling(void* udata, bool integer_scaling) {}
void hardware_set_bilinear(void* udata, bool bilinear) {}
void hardware_get_viewport_size(void* udata, int* w, int* h) {
    *w = 0;
    *h = 0;
}
void hardware_get_display_size(void* udata, int* w, int* h) {
    *w = 0;
    *h = 0;
}
void hardware_get_display_format(void* udata, int* fmt) {
    *fmt = 0;
}
void hardware_get_interlace_mode(void* udata, int* mode) {
    *mode = 0;
}
void hardware_set_window_rect(void* udata, int x, int y, int w, int h) {}
void* hardware_get_buffer_data(void* udata, int* w, int* h, int* bpp) { *w = 0; *h = 0; *bpp = 0; return nullptr; }
const char* hardware_get_name(void* udata) { return "Hardware"; }

extern "C" void hardware_render_point(struct ps2_gs* gs, void* udata) {}
extern "C" void hardware_render_line(struct ps2_gs* gs, void* udata) {}
extern "C" void hardware_render_triangle(struct ps2_gs* gs, void* udata) {
    hardware_state* ctx = (hardware_state*)udata;

    // printf("triangle: v0=%08x v1=%08x v2=%08x\n",
    //     gs->vq[0].z, gs->vq[1].z, gs->vq[2].z
    // );

    int interlace = ((gs->smode2 & 3) == 3) ? 2 : 1;

    for (int i = 0; i < 3; i++) {
        hw_vertex hw_v;

        hw_v.r = gs->vq[i].r;
        hw_v.g = gs->vq[i].g;
        hw_v.b = gs->vq[i].b;
        hw_v.a = gs->vq[i].a;
        hw_v.u = gs->vq[i].u;
        hw_v.v = gs->vq[i].v;
        hw_v.x = gs->vq[i].x - ctx->gs->ctx->ofx;
        hw_v.y = (ctx->tex_h - (gs->vq[i].y - ctx->gs->ctx->ofy)) * interlace;
        hw_v.z = gs->vq[i].z;
        hw_v.s = gs->vq[i].s;
        hw_v.t = gs->vq[i].t;
        hw_v.q = gs->vq[i].q;

        ctx->vertices.push_back(hw_v);
    }
}
extern "C" void hardware_render_sprite(struct ps2_gs* gs, void* udata) {
    hardware_state* ctx = (hardware_state*)udata;

    struct gs_vertex v0 = gs->vq[0];
    struct gs_vertex v1 = gs->vq[1];

    int interlace = ((gs->smode2 & 3) == 3) ? 2 : 1;

    uint32_t v0_x = v0.x - gs->ctx->ofx;
    uint32_t v0_y = (ctx->tex_h - (v0.y - gs->ctx->ofy)) * interlace;
    uint32_t v1_x = v1.x - gs->ctx->ofx;
    uint32_t v1_y = (ctx->tex_h - (v1.y - gs->ctx->ofy)) * interlace;

    hw_vertex hw_v[6];

    hw_v[0].x = v0_x; // Top-left
    hw_v[0].y = v0_y;
    hw_v[1].x = v1_x; // Top-right
    hw_v[1].y = v0_y;
    hw_v[2].x = v0_x; // Bottom-left
    hw_v[2].y = v1_y;
    hw_v[3].x = v1_x; // Top-right
    hw_v[3].y = v0_y;
    hw_v[4].x = v1_x; // Bottom-right
    hw_v[4].y = v1_y;
    hw_v[5].x = v0_x; // Bottom-left
    hw_v[5].y = v1_y;

    for (int i = 0; i < 6; i++) {
        hw_v[i].r = v1.r;
        hw_v[i].g = v1.g;
        hw_v[i].b = v1.b;
        hw_v[i].a = v1.a;

        ctx->vertices.push_back(hw_v[i]);
    }
    
    // printf("sprite: v0=(%04x, %04x) v1=(%04x, %04x)\n",
    //     v0.x, v0.y,
    //     v1.x, v1.y
    // );
}
extern "C" void hardware_transfer_start(struct ps2_gs* gs, void* udata) {
    hardware_state* ctx = (hardware_state*)udata;

    uint32_t dbp = (gs->bitbltbuf >> 32) & 0x3fff;
    uint32_t dbw = (gs->bitbltbuf >> 48) & 0x3f;
    uint32_t dpsm = (gs->bitbltbuf >> 56) & 0x3f;
    uint32_t sbp = (gs->bitbltbuf >> 0) & 0x3fff;
    uint32_t sbw = (gs->bitbltbuf >> 16) & 0x3f;
    uint32_t spsm = (gs->bitbltbuf >> 24) & 0x3f;
    uint32_t dsax = (gs->trxpos >> 32) & 0x7ff;
    uint32_t dsay = (gs->trxpos >> 48) & 0x7ff;
    uint32_t ssax = (gs->trxpos >> 0) & 0x7ff;
    uint32_t ssay = (gs->trxpos >> 16) & 0x7ff;
    uint32_t dir = (gs->trxpos >> 59) & 3;
    uint32_t rrw = (gs->trxreg >> 0) & 0xfff;
    uint32_t rrh = (gs->trxreg >> 32) & 0xfff;
    uint32_t xdir = gs->trxdir & 3;

    if (xdir) {
        printf("gs: Unsupported local/host or local/local transfer\n");
    }

    printf("gs: dbp=%04x dbw=%02x dpsm=%02x dsa=(%04x, %04x) sbp=%04x sbw=%02x spsm=%02x ssa=(%04x, %04x) dir=%d rr=(%d, %d) xdir=%d\n",
        dbp, dbw, dpsm, dsax, dsay,
        sbp, sbw, spsm, ssax, ssay,
        dir, rrw, rrh, xdir
    );
}
extern "C" void hardware_transfer_write(struct ps2_gs* gs, void* udata) {}
extern "C" void hardware_transfer_read(struct ps2_gs* gs, void* udata) {}

void hardware_begin_render(void* udata, SDL_GPUCommandBuffer* command_buffer) {
    hardware_state* ctx = (hardware_state*)udata;

    if (ctx->vertices.size() == 0)
        return;

    size_t bytes = ctx->vertices.size() * sizeof(hw_vertex);

    // create the vertex buffer
    SDL_GPUBufferCreateInfo vbci = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = bytes,
    };

    ctx->vertex_buffer = SDL_CreateGPUBuffer(ctx->device, &vbci);

    SDL_GPUTransferBufferCreateInfo tbci = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = bytes,
    };

    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(ctx->device, &tbci);

    void* ptr = SDL_MapGPUTransferBuffer(ctx->device, tb, false);

    SDL_memcpy(ptr, ctx->vertices.data(), bytes);

    SDL_UnmapGPUTransferBuffer(ctx->device, tb);

    // start a copy pass
    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(command_buffer);

    // Upload vertex buffer (at offset 0)
    SDL_GPUTransferBufferLocation tbl = {
        .transfer_buffer = tb,
        .offset = 0,
    };

    SDL_GPUBufferRegion br = {
        .buffer = ctx->vertex_buffer,
        .offset = 0,
        .size = bytes,
    };

    SDL_UploadToGPUBuffer(cp, &tbl, &br, false);

    SDL_EndGPUCopyPass(cp);

    SDL_ReleaseGPUTransferBuffer(ctx->device, tb);
}

void hardware_render(void* udata, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass) {
    hardware_state* ctx = (hardware_state*)udata;

    if (ctx->vertices.size() == 0)
        return;

    SDL_BindGPUGraphicsPipeline(render_pass, ctx->pipeline);

    // bind the vertex buffer
    SDL_GPUBufferBinding bb[1]{};
    bb[0].buffer = ctx->vertex_buffer;
    bb[0].offset = 0;

    SDL_BindGPUVertexBuffers(render_pass, 0, bb, 1);

    // issue a draw call
    SDL_DrawGPUPrimitives(render_pass, ctx->vertices.size(), 1, 0, 0);
}

void hardware_end_render(void* udata, SDL_GPUCommandBuffer* command_buffer) {
    hardware_state* ctx = (hardware_state*)udata;

    ctx->vertices.clear();

    if (ctx->vertex_buffer) {
        SDL_ReleaseGPUBuffer(ctx->device, ctx->vertex_buffer);

        ctx->vertex_buffer = nullptr;
    }
}