/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "hardware.hpp"

#include "Granite/util/thread_id.hpp"

#include <filesystem>
#include <fstream>

namespace iris::gs::renderer::hardware {

static void load_pipeline_cache(state* ctx, const char* path) {
    std::vector <uint8_t> data;

    if (path && *path) {
        ctx->pipeline_cache.path = path;

        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (file) {
            data.resize(file.tellg());

            file.seekg(0);

            if (!file.read((char*)data.data(), data.size())) {
                data.clear();
            }
        }
    }

    if (!ctx->granite_device.init_pipeline_cache(data.data(), data.size(), false)) {
        iris_warning(ctx->gs, "Couldn't load pipeline cache, starting with an empty one");

        ctx->granite_device.init_pipeline_cache(nullptr, 0, false);
    } else if (data.size()) {
        iris_info(ctx->gs, "Loaded pipeline cache (%zu bytes)", data.size());
    }

    ctx->pipeline_cache.device = &ctx->granite_device;
}

PipelineCacheFile::~PipelineCacheFile() {
    if (!device || path.empty())
        return;

    size_t size = device->get_pipeline_cache_size();

    if (!size)
        return;

    std::vector <uint8_t> data(size);

    if (!device->get_pipeline_cache_data(data.data(), size))
        return;

    std::string tmp_path = path + ".tmp";

    {
        std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);

        if (!file.write((const char*)data.data(), data.size()))
            return;
    }

    std::error_code ec;

    std::filesystem::rename(tmp_path, path, ec);

    if (ec) {
        std::filesystem::remove(tmp_path, ec);
    }
}

static thread_local bool thread_index_registered = false;

static inline void register_calling_thread() {
    if (thread_index_registered) {
        return;
    }

    Util::register_thread_index(0);

    thread_index_registered = true;
}

void* create() {
    return new state();
}

static int get_analog_system(state* ctx) {
    if (ctx->config.analog_system == gs::VIDEO_MODE_AUTO) {
        return gs::get_video_mode(ctx->gs);
    }

    return ctx->config.analog_system;
}

static void init_analog_video(state* ctx) {
    AnalogVideoFilter::Options analog_video_filter_opts;

    ctx->analog_system = get_analog_system(ctx);

    analog_video_filter_opts.cable = static_cast<ParallelGS::AnalogVideoFilter::Cable>(ctx->config.analog_cable);
    analog_video_filter_opts.system = static_cast<ParallelGS::AnalogVideoFilter::System>(ctx->analog_system);

    ctx->analog_video_filter.init(ctx->granite_device, analog_video_filter_opts);
    ctx->crt_filter.init(ctx->granite_device);
}

static SuperSampling get_super_sampling(state* ctx) {
    if (ctx->config.enable_analog_video) {
        return SuperSampling::X1;
    }

    switch (ctx->config.internal_resolution) {
        case 1: return ctx->config.ssaa ? SuperSampling::X16 : SuperSampling::X4;
        case 2: return SuperSampling::X16;
    }

    return ctx->config.ssaa ? SuperSampling::X4 : SuperSampling::X1;
}

static void apply_super_sampling(state* ctx) {
    ctx->iface.set_super_sampling_rate(get_super_sampling(ctx), true, true, ctx->config.upscale_2d);
}

static void apply_color_depth(state* ctx) {
    switch (ctx->config.color_depth) {
        case 1: ctx->iface.set_color_depth(ColorDepth::NoDither); return;
        case 2: ctx->iface.set_color_depth(ColorDepth::Force32); return;
    }

    ctx->iface.set_color_depth(ColorDepth::Accurate);
}

bool init(void* udata, const CreateInfo& info) {
    state* ctx = static_cast<state*>(udata);

    if (!Vulkan::Context::init_loader(nullptr))
        return false;

    ctx->gs = info.gs;
    ctx->gif = info.gif;
    ctx->config = *(HardwareConfig*)info.config;

    ctx->instance = new ExternallyManagedInstance(info.instance, info.instance_create_info);
    ctx->device = new ExternallyManagedDevice(info.device, info.device_create_info);
    ctx->signal_handler = new RendererSignalHandler(ctx->gs);

    ctx->granite_ctx.set_instance_factory(ctx->instance);
    ctx->granite_ctx.set_device_factory(ctx->device);
    ctx->granite_ctx.set_num_thread_indices(1);

    // We don't need to pass any extensions or layers because the
    // VkInstance is managed/created externally.
    if (!ctx->granite_ctx.init_instance(nullptr, 0)) {
        iris_error(info.gs, "Failed to initialize the Granite instance");

        return false;
    }

    if (!ctx->granite_ctx.init_device(info.physical_device, VK_NULL_HANDLE, nullptr, 0)) {
        iris_error(info.gs, "Failed to initialize the Granite device");

        return false;
    }

    ctx->granite_device.set_context(ctx->granite_ctx);
    ctx->granite_device.init_frame_contexts(4);
    ctx->granite_device.set_queue_lock(lock_queue, unlock_queue);

    // Use our own pipeline cache file
    load_pipeline_cache(ctx, info.pipeline_cache_path);

    GSOptions opts = {};
    opts.super_sampling = get_super_sampling(ctx);
    opts.ordered_super_sampling = true;
    opts.super_sampled_textures = true;
    opts.super_sampled_quads = ctx->config.upscale_2d;
    opts.dynamic_super_sampling = true;

    if (!ctx->iface.init(&ctx->granite_device, opts)) {
        return false;
    }

    ctx->iface.reset_context_state();
    ctx->iface.set_signal_interface(ctx->signal_handler);

    Hacks hacks = {};
    hacks.allow_blend_demote = ctx->config.allow_blend_demote;
    hacks.backbuffer_promotion = ctx->config.backbuffer_promotion;
    hacks.disable_mipmaps = ctx->config.disable_mipmaps;
    hacks.unsynced_readbacks = ctx->config.unsynced_readbacks;

    ctx->iface.set_hacks(hacks);

    apply_super_sampling(ctx);
    apply_color_depth(ctx);

    if (ctx->config.enable_analog_video) {
        init_analog_video(ctx);
    }

    return true;
}

void reset(void* udata) {
    state* ctx = static_cast<state*>(udata);

    // ctx->iface.flush();
    ctx->iface.reset_context_state();

    // Clear VRAM
    void* ptr = ctx->iface.map_vram_write(0, 0x400000);

    memset(ptr, 0, 0x400000);

    ctx->iface.end_vram_write(0, 0x400000);
}

void destroy(void* udata) {
    state* ctx = static_cast<state*>(udata);

    delete ctx->instance;
    delete ctx->device;
    delete ctx->signal_handler;

    delete ctx;
}

static Image scanout(state* ctx, const gs::PrivilegedState& state) {
    if (!state.pmode) {
        ctx->iface.flush();

        Image image = {};
        image.image = VK_NULL_HANDLE;
        image.view = VK_NULL_HANDLE;

        return image;
    }

    auto& priv = ctx->iface.get_priv_register_state();

    *((uint64_t*)&priv.pmode) = state.pmode;
    *((uint64_t*)&priv.smode1) = state.smode1;
    *((uint64_t*)&priv.smode2) = state.smode2;
    *((uint64_t*)&priv.srfsh) = state.srfsh;
    *((uint64_t*)&priv.synch1) = state.synch1;
    *((uint64_t*)&priv.synch2) = state.synch2;
    *((uint64_t*)&priv.syncv) = state.syncv;
    *((uint64_t*)&priv.dispfb1) = state.dispfb1;
    *((uint64_t*)&priv.display1) = state.display1;
    *((uint64_t*)&priv.dispfb2) = state.dispfb2;
    *((uint64_t*)&priv.display2) = state.display2;
    *((uint64_t*)&priv.extbuf) = state.extbuf;
    *((uint64_t*)&priv.extdata) = state.extdata;
    *((uint64_t*)&priv.extwrite) = state.extwrite;
    *((uint64_t*)&priv.bgcolor) = state.bgcolor;
    *((uint64_t*)&priv.csr) = state.csr;
    *((uint64_t*)&priv.imr) = state.imr;
    *((uint64_t*)&priv.busdir) = state.busdir;
    *((uint64_t*)&priv.siglblid) = state.siglblid;

    ctx->iface.flush();

    VSyncInfo info = {};

    info.phase = state.csr & (1 << 13) ? 0 : 1;

    if (ctx->config.invert_fields) {
        info.phase ^= 1;
    }

    if (get_super_sampling(ctx) != SuperSampling::X1) {
        info.raw_circuit_scanout = true;
        info.high_resolution_scanout = ctx->config.internal_resolution != 0;
        info.quad_resolution_scanout = ctx->config.internal_resolution == 2;
        info.anti_blur = true;
    } else {
        info.raw_circuit_scanout = false;
        info.high_resolution_scanout = false;
        info.anti_blur = false;
    }

    info.force_progressive = ctx->config.force_progressive;
    info.overscan = ctx->config.overscan;
    info.crtc_offsets = ctx->config.crtc_offsets;
    info.dst_access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    info.dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    info.dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    info.adapt_to_internal_horizontal_resolution = true;

    ScanoutResult scanout = ctx->iface.vsync(info);

    ctx->current_scanout = scanout.image;

    Vulkan::Image* granite_image = scanout.image.get();

    Image image;

    image.image = granite_image->get_image();
    image.width = granite_image->get_width();
    image.height = granite_image->get_height();
    image.format = granite_image->get_format();
    image.view = granite_image->get_view().get_view().view;

    if (ctx->config.enable_analog_video) {
        if (ctx->analog_system != get_analog_system(ctx)) {
            init_analog_video(ctx);
        }

        auto cmd = ctx->granite_device.request_command_buffer();

        AnalogVideoFilter::FilterOptions filter_options = {};
        filter_options.dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        filter_options.dst_access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        filter_options.dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        filter_options.phase = scanout.interlaced ? scanout.interlace_phase : 0;

        filter_options.line_comb = ctx->config.line_comb;
        filter_options.skip_notch = ctx->config.skip_notch;

        filter_options.input_sampling_rate_mhz = 13.5f * float(scanout.image->get_width()) / 640.0f;

        ctx->analog_video_filter.run_filter(*cmd, granite_image->get_view(), filter_options);

        image.view = ctx->analog_video_filter.get_output().get_view().view;

        ctx->granite_device.submit(cmd);
        ctx->granite_device.flush_frame();
    }

    return image;
}

Image get_frame(void* udata) {
    state* ctx = static_cast<state*>(udata);

    gs::PrivilegedState state;

    gs::get_privileged_state(ctx->gs, &state);

    return scanout(ctx, state);
}

static void publish_scanout(state* ctx, const ScanoutRequest& request) {
    Image image = scanout(ctx, request.state);

    std::lock_guard <std::mutex> lock(ctx->scanout_mutex);

    ctx->scanout_history[ctx->scanout_next] = ctx->current_scanout;
    ctx->scanout_ids[ctx->scanout_next] = request.id;
    ctx->scanout_images[ctx->scanout_next] = image;
    ctx->scanout_next = (ctx->scanout_next + 1) % 8;
    ctx->scanout_image = image;
    ctx->scanout_id = request.id;

    ctx->scanout_cv.notify_all();
}

bool get_scanout(void* udata, uint64_t id, Image* image, bool wait) {
    state* ctx = static_cast<state*>(udata);

    std::unique_lock <std::mutex> lock(ctx->scanout_mutex);

    if (wait) {
        if (!ctx->scanout_cv.wait_for(lock, std::chrono::milliseconds(500), [&] { return ctx->scanout_id >= id; })) {
            return false;
        }
    } else if (ctx->scanout_id < id) {
        return false;
    }

    for (int i = 0; i < 8; i++) {
        if (ctx->scanout_ids[i] == id) {
            *image = ctx->scanout_images[i];

            return true;
        }
    }

    *image = ctx->scanout_image;

    return true;
}

void transfer(void* udata, int path, const void* data, size_t size) {
    state* ctx = static_cast<state*>(udata);

    register_calling_thread();

    if (path == PATH_SCANOUT) {
        ScanoutRequest request;

        memcpy(&request, data, sizeof(request));

        publish_scanout(ctx, request);

        return;
    }

    ctx->iface.gif_transfer(path, data, size);
}

void readback(void* udata, void* data, size_t size) {
    state* ctx = static_cast<state*>(udata);

    register_calling_thread();

    ctx->iface.read_transfer_fifo((void*)data, size / 16);
}

void read_vram(void* udata, void* dst, size_t size) {
    state* ctx = static_cast<state*>(udata);

    // Flush pending GS work so local memory reflects everything submitted so far
    ctx->iface.flush();

    const void* src = ctx->iface.map_vram_read(0, size);

    memcpy(dst, src, size);
}

void set_config(void* udata, void* config) {
    state* ctx = (state*)udata;

    ctx->config = *(HardwareConfig*)config;

    Hacks hacks = {};
    hacks.allow_blend_demote = ctx->config.allow_blend_demote;
    hacks.backbuffer_promotion = ctx->config.backbuffer_promotion;
    hacks.disable_mipmaps = ctx->config.disable_mipmaps;
    hacks.unsynced_readbacks = ctx->config.unsynced_readbacks;

    ctx->iface.set_hacks(hacks);

    apply_super_sampling(ctx);
    apply_color_depth(ctx);

    if (ctx->config.enable_analog_video) {
        init_analog_video(ctx);
    }
}

}
