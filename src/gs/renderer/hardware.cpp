#include "hardware.hpp"

void* hardware_create() {
    return new hardware_state();
}

bool hardware_init(void* udata, const renderer_create_info& info) {
    hardware_state* ctx = static_cast<hardware_state*>(udata);

    if (!Context::init_loader(nullptr))
		return false;

    ctx->gs = info.gs;
    ctx->gif = info.gif;

	ctx->instance = new ExternallyManagedInstance(info.instance, info.instance_create_info);
	ctx->device = new ExternallyManagedDevice(info.device, info.device_create_info);
	ctx->signal_handler = new RendererSignalHandler(ctx->gs);

	ctx->granite_ctx.set_instance_factory(ctx->instance);
	ctx->granite_ctx.set_device_factory(ctx->device);
	ctx->granite_ctx.set_num_thread_indices(1);

	// We don't need to pass any extensions or layers because the
	// VkInstance is managed/created externally.
	if (!ctx->granite_ctx.init_instance(nullptr, 0)) {
		fprintf(stderr, "renderer: Failed to initialize Granite instance\n");

		return false;
	}

	if (!ctx->granite_ctx.init_device(info.physical_device, VK_NULL_HANDLE, nullptr, 0)) {
		fprintf(stderr, "renderer: Failed to initialize Granite device\n");

		return false;
	}

	ctx->granite_device.set_context(ctx->granite_ctx);
	ctx->granite_device.init_frame_contexts(4);

	GSOptions opts = {};
	// opts.super_sampling = SuperSampling::X4;
	// opts.ordered_super_sampling = true;
	// opts.super_sampled_textures = true;

	if (!ctx->interface.init(&ctx->granite_device, opts)) {
		return false;
    }

	ctx->interface.reset_context_state();
	ctx->interface.set_signal_interface(ctx->signal_handler);

    return true;
}

void hardware_reset(void* udata) {
	hardware_state* ctx = static_cast<hardware_state*>(udata);

	ctx->interface.flush();
	ctx->interface.reset_context_state();

	// Clear VRAM
	void* ptr = ctx->interface.map_vram_write(0, 0x400000);

	memset(ptr, 0, 0x400000);

	ctx->interface.end_vram_write(0, 0x400000);
}

void hardware_destroy(void* udata) {
    hardware_state* ctx = static_cast<hardware_state*>(udata);

    ctx->granite_device.wait_idle();

	delete ctx->instance;
	delete ctx->device;
	delete ctx->signal_handler;

    delete ctx;
}

static int phase = 0;

renderer_image hardware_get_frame(void* udata) {
    hardware_state* ctx = static_cast<hardware_state*>(udata);

	ctx->interface.flush();

	auto& priv = ctx->interface.get_priv_register_state();

    struct gs_privileged_state state;
    gs_get_privileged_state(ctx->gs, &state);

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

	if (!state.pmode) {
		// No display enabled.
		renderer_image image = {};
		image.image = VK_NULL_HANDLE;
		image.view = VK_NULL_HANDLE;

		return image;
	}

	VSyncInfo info = {};

	info.phase = phase;
	info.anti_blur = true;
	info.force_progressive = false;
	info.overscan = false;
	info.crtc_offsets = false;
	info.dst_access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	info.dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	info.dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info.adapt_to_internal_horizontal_resolution = false;
	info.raw_circuit_scanout = true;
	info.high_resolution_scanout = false;

	ScanoutResult scanout = ctx->interface.vsync(info);

	Image* granite_image = scanout.image.get();

	renderer_image image;

	image.image = granite_image->get_image();
	image.width = granite_image->get_width();
	image.height = granite_image->get_height();
	image.format = granite_image->get_format();
	image.view = granite_image->get_view().get_view().view;

	phase ^= 1;

	return image;
}

extern "C" void hardware_transfer(void* udata, int path, const void* data, size_t size) {
    hardware_state* ctx = static_cast<hardware_state*>(udata);

	ctx->interface.gif_transfer(path, data, size);
}