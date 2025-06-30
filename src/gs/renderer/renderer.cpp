#include "renderer.hpp"

#include "null.hpp"
#include "software.hpp"
#include "software_thread.hpp"

renderer_state* renderer_create(void) {
    return new renderer_state;
}

void renderer_init_null(renderer_state* renderer, struct ps2_gs* gs, SDL_Window* window, SDL_GPUDevice* device) {
    renderer->udata = NULL;
    renderer->init = null_init;
    renderer->destroy = null_destroy;
    renderer->set_size = null_set_size;
    renderer->set_scale = null_set_scale;
    renderer->set_aspect_mode = null_set_aspect_mode;
    renderer->set_integer_scaling = null_set_integer_scaling;
    renderer->set_bilinear = null_set_bilinear;
    renderer->get_viewport_size = null_get_viewport_size;
    renderer->get_display_size = null_get_display_size;
    renderer->get_display_format = null_get_display_format;
    renderer->get_interlace_mode = null_get_interlace_mode;
    renderer->set_window_rect = null_set_window_rect;
    renderer->get_buffer_data = null_get_buffer_data;
    renderer->get_name = null_get_name;
    renderer->begin_render = software_thread_begin_render;
    renderer->render = software_thread_render;
    renderer->end_render = software_thread_end_render;

    gs->backend.render_point = null_render_point;
    gs->backend.render_line = null_render_line;
    gs->backend.render_triangle = null_render_triangle;
    gs->backend.render_sprite = null_render_sprite;
    gs->backend.transfer_start = null_transfer_start;
    gs->backend.transfer_write = null_transfer_write;
    gs->backend.transfer_read = null_transfer_read;
    gs->backend.udata = renderer->udata;
}

void renderer_init_software(renderer_state* renderer, struct ps2_gs* gs, SDL_Window* window, SDL_GPUDevice* device) {
    renderer->udata = new software_state;
    renderer->init = software_init;
    renderer->destroy = software_destroy;
    renderer->set_size = software_set_size;
    renderer->set_scale = software_set_scale;
    renderer->set_aspect_mode = software_set_aspect_mode;
    renderer->set_integer_scaling = software_set_integer_scaling;
    renderer->set_bilinear = software_set_bilinear;
    renderer->get_viewport_size = software_get_viewport_size;
    renderer->get_display_size = software_get_display_size;
    renderer->get_display_format = software_get_display_format;
    renderer->get_interlace_mode = software_get_interlace_mode;
    renderer->set_window_rect = software_set_window_rect;
    renderer->get_buffer_data = software_get_buffer_data;
    renderer->get_name = software_get_name;
    renderer->begin_render = software_thread_begin_render;
    renderer->render = software_thread_render;
    renderer->end_render = software_thread_end_render;

    gs->backend.render_point = software_render_point;
    gs->backend.render_line = software_render_line;
    gs->backend.render_triangle = software_render_triangle;
    gs->backend.render_sprite = software_render_sprite;
    gs->backend.transfer_start = software_transfer_start;
    gs->backend.transfer_write = software_transfer_write;
    gs->backend.transfer_read = software_transfer_read;
    gs->backend.udata = renderer->udata;
}

void renderer_init_software_thread(renderer_state* renderer, struct ps2_gs* gs, SDL_Window* window, SDL_GPUDevice* device) {
    renderer->udata = new software_thread_state;
    renderer->init = software_thread_init;
    renderer->destroy = software_thread_destroy;
    renderer->set_size = software_thread_set_size;
    renderer->set_scale = software_thread_set_scale;
    renderer->set_aspect_mode = software_thread_set_aspect_mode;
    renderer->set_integer_scaling = software_thread_set_integer_scaling;
    renderer->set_bilinear = software_thread_set_bilinear;
    renderer->get_viewport_size = software_thread_get_viewport_size;
    renderer->get_display_size = software_thread_get_display_size;
    renderer->get_display_format = software_thread_get_display_format;
    renderer->get_interlace_mode = software_thread_get_interlace_mode;
    renderer->set_window_rect = software_thread_set_window_rect;
    renderer->get_buffer_data = software_thread_get_buffer_data;
    renderer->get_name = software_thread_get_name;
    renderer->begin_render = software_thread_begin_render;
    renderer->render = software_thread_render;
    renderer->end_render = software_thread_end_render;

    gs->backend.render_point = software_thread_render_point;
    gs->backend.render_line = software_thread_render_line;
    gs->backend.render_triangle = software_thread_render_triangle;
    gs->backend.render_sprite = software_thread_render_sprite;
    gs->backend.transfer_start = software_thread_transfer_start;
    gs->backend.transfer_write = software_thread_transfer_write;
    gs->backend.transfer_read = software_thread_transfer_read;
    gs->backend.udata = renderer->udata;
}

void renderer_init_backend(renderer_state* renderer, struct ps2_gs* gs, SDL_Window* window, SDL_GPUDevice* device, int id) {
    renderer->gs = gs;

    if (renderer->udata) {
        renderer->destroy(renderer->udata);

        renderer->udata = NULL;
    }

    switch (id) {
        case RENDERER_NULL: renderer_init_null(renderer, gs, window, device); break;
        case RENDERER_SOFTWARE: renderer_init_software(renderer, gs, window, device); break;
        case RENDERER_SOFTWARE_THREAD: renderer_init_software_thread(renderer, gs, window, device); break;
        default: {
            printf("renderer: Unknown backend %d\n", id);
        } break;
    }

    renderer->init(renderer->udata, gs, window, device);
}

void renderer_set_size(renderer_state* renderer, int w, int h) {
    renderer->set_size(renderer->udata, w, h);
}
void renderer_set_scale(renderer_state* renderer, float scale) {
    renderer->set_scale(renderer->udata, scale);
}
void renderer_set_aspect_mode(renderer_state* renderer, int mode) {
    renderer->set_aspect_mode(renderer->udata, mode);
}
void renderer_set_integer_scaling(renderer_state* renderer, bool integer_scaling) {
    renderer->set_integer_scaling(renderer->udata, integer_scaling);
}
void renderer_set_bilinear(renderer_state* renderer, bool bilinear) {
    renderer->set_bilinear(renderer->udata, bilinear);
}
void renderer_get_viewport_size(renderer_state* renderer, int* w, int* h) {
    renderer->get_viewport_size(renderer->udata, w, h);
}
void renderer_get_display_size(renderer_state* renderer, int* w, int* h) {
    renderer->get_display_size(renderer->udata, w, h);
}
void renderer_get_display_format(renderer_state* renderer, int* fmt) {
    renderer->get_display_format(renderer->udata, fmt);
}
void renderer_get_interlace_mode(renderer_state* renderer, int* mode) {
    renderer->get_interlace_mode(renderer->udata, mode);
}
void renderer_set_window_rect(renderer_state* renderer, int x, int y, int w, int h) {
    renderer->set_window_rect(renderer->udata, x, y, w, h);
}
void renderer_set_render_context(renderer_state* renderer, SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* rp) {
    renderer->set_render_context(renderer->udata, cb, rp);
}
void* renderer_get_buffer_data(renderer_state* renderer, int* w, int* h, int* bpp) {
    return renderer->get_buffer_data(renderer->udata, w, h, bpp);
}
const char* renderer_get_name(renderer_state* renderer) {
    return renderer->get_name(renderer->udata);
}

void renderer_destroy(renderer_state* renderer) {
    if (renderer->udata)
        renderer->destroy(renderer->udata);

    renderer->udata = nullptr;
    renderer->init = nullptr;
    renderer->destroy = nullptr;
    renderer->set_size = nullptr;
    renderer->set_scale = nullptr;
    renderer->set_aspect_mode = nullptr;
    renderer->set_integer_scaling = nullptr;
    renderer->set_bilinear = nullptr;
    renderer->get_viewport_size = nullptr;
    renderer->get_display_size = nullptr;
    renderer->get_display_format = nullptr;
    renderer->get_buffer_data = nullptr;
    renderer->get_name = nullptr;
    renderer->begin_render = nullptr;
    renderer->render = nullptr;
    renderer->end_render = nullptr;

    delete renderer;
}

void renderer_begin_render(renderer_state* renderer, SDL_GPUCommandBuffer* command_buffer) {
    renderer->begin_render(renderer->udata, command_buffer);
}

void renderer_render(renderer_state* renderer, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass) {
    renderer->render(renderer->udata, command_buffer, render_pass);
}

void renderer_end_render(renderer_state* renderer, SDL_GPUCommandBuffer* command_buffer) {
    renderer->end_render(renderer->udata, command_buffer);
}
