#include "renderer.hpp"

#include "null.hpp"
#include "hardware.hpp"
#include "ee/gif.hpp"

namespace iris::gs::renderer {

Renderer* create() {
    return new Renderer;
}

static void sync_emulation(Renderer* renderer) {
    if (renderer->active_backend == BACKEND_NULL) {
        return;
    }

    if (!renderer->info.gif) {
        return;
    }

    gif::sync_backend(renderer->info.gif);
}

void init_callbacks(Renderer* renderer, int backend) {
    renderer->active_backend = backend;

    switch (backend) {
        case BACKEND_NULL: {
            renderer->create = null::create;
            renderer->init = null::init;
            renderer->reset = null::reset;
            renderer->destroy = null::destroy;
            renderer->get_frame = null::get_frame;
            renderer->set_config = null::set_config;
            renderer->transfer = null::transfer;
            renderer->readback = null::readback;
            renderer->read_vram = null::read_vram;
        } break;

        case BACKEND_SOFTWARE: {
            // To-do: Software renderer
            renderer->create = null::create;
            renderer->init = null::init;
            renderer->reset = null::reset;
            renderer->destroy = null::destroy;
            renderer->get_frame = null::get_frame;
            renderer->set_config = null::set_config;
            renderer->transfer = null::transfer;
            renderer->readback = null::readback;
            renderer->read_vram = null::read_vram;
        } break;

        case BACKEND_HARDWARE: {
            renderer->create = hardware::create;
            renderer->init = hardware::init;
            renderer->reset = hardware::reset;
            renderer->destroy = hardware::destroy;
            renderer->get_frame = hardware::get_frame;
            renderer->set_config = hardware::set_config;
            renderer->transfer = hardware::transfer;
            renderer->readback = hardware::readback;
            renderer->read_vram = hardware::read_vram;
        } break;
    }
}

bool init(Renderer* renderer, const CreateInfo& info) {
    renderer->info = info;

    init_callbacks(renderer, info.backend);

    renderer->udata = renderer->create();

    gif::set_backend(info.gif, renderer->udata, renderer->transfer, renderer->readback);

    return renderer->init(renderer->udata, info);
}

bool set_backend(Renderer* renderer, int backend, void* config) {
    if (backend == renderer->info.backend)
        return true;

    sync_emulation(renderer);

    renderer->destroy(renderer->udata);

    CreateInfo info = renderer->info;
    info.backend = backend;
    info.config = config;

    return init(renderer, info);
}

void hotswap(Renderer* renderer, int backend) {
    sync_emulation(renderer);

    init_callbacks(renderer, backend);

    // Re-point the GIF backend at the newly-selected callbacks. Without this the
    // core keeps feeding the previous backend (e.g. hardware) even after a swap
    // to NULL, so GIF transfers would still reach parallel-gs during loading.
    if (renderer->info.gif)
        gif::set_backend(renderer->info.gif, renderer->udata, renderer->transfer, renderer->readback);
}

void destroy(Renderer* renderer) {
    sync_emulation(renderer);

    renderer->destroy(renderer->udata);

    delete renderer;
}

void reset(Renderer* renderer) {
    sync_emulation(renderer);

    renderer->reset(renderer->udata);
}

Image get_frame(Renderer* renderer) {
    sync_emulation(renderer);

    return renderer->get_frame(renderer->udata);
}

void read_vram(Renderer* renderer, void* dst, size_t size) {
    sync_emulation(renderer);

    renderer->read_vram(renderer->udata, dst, size);
}

void set_config(Renderer* renderer, void* config) {
    sync_emulation(renderer);

    renderer->set_config(renderer->udata, config);
}

}
