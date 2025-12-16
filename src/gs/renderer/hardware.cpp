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
	opts.super_sampling = (SuperSampling)ctx->config.super_sampling;
	opts.ordered_super_sampling = true;
	opts.super_sampled_textures = true;
	opts.dynamic_super_sampling = true;

	if (!ctx->interface.init(&ctx->granite_device, opts)) {
		return false;
    }

	ctx->interface.reset_context_state();
	ctx->interface.set_signal_interface(ctx->signal_handler);

	ctx->config = *(hardware_config*)info.config;

	Hacks hacks = {};
	hacks.allow_blend_demote = ctx->config.allow_blend_demote;
	hacks.backbuffer_promotion = ctx->config.backbuffer_promotion;
	hacks.disable_mipmaps = ctx->config.disable_mipmaps;
	hacks.unsynced_readbacks = ctx->config.unsynced_readbacks;

	ctx->interface.set_hacks(hacks);

	if (ctx->config.super_sampling == 0) {
		ctx->interface.set_super_sampling_rate((SuperSampling)0, false, false);
	} else {
		SuperSampling super_sampling;

		switch (ctx->config.super_sampling) {
			case 1: super_sampling = SuperSampling::X2; break;
			case 2: super_sampling = SuperSampling::X4; break;
			case 3: super_sampling = SuperSampling::X8; break;
			case 4: super_sampling = SuperSampling::X16; break;
			default: super_sampling = (SuperSampling)0; break;
		}

		ctx->interface.set_super_sampling_rate(super_sampling, true, true);
	}

    return true;
}

void hardware_reset(void* udata) {
	hardware_state* ctx = static_cast<hardware_state*>(udata);

	// ctx->interface.flush();
	ctx->interface.reset_context_state();

	// Clear VRAM
	void* ptr = ctx->interface.map_vram_write(0, 0x400000);

	memset(ptr, 0, 0x400000);

	ctx->interface.end_vram_write(0, 0x400000);
}

void hardware_destroy(void* udata) {
    hardware_state* ctx = static_cast<hardware_state*>(udata);

	delete ctx->instance;
	delete ctx->device;
	delete ctx->signal_handler;

    delete ctx;
}

renderer_image hardware_get_frame(void* udata) {
    hardware_state* ctx = static_cast<hardware_state*>(udata);

    struct gs_privileged_state state;

    gs_get_privileged_state(ctx->gs, &state);

	if (!state.pmode) {
		// No display enabled.
		renderer_image image = {};
		image.image = VK_NULL_HANDLE;
		image.view = VK_NULL_HANDLE;

		return image;
	}

	auto& priv = ctx->interface.get_priv_register_state();

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

	ctx->interface.flush();

	VSyncInfo info = {};

	if (ctx->config.super_sampling) {
		info.phase = 0;
		info.raw_circuit_scanout = true;
		info.high_resolution_scanout = true;
		info.anti_blur = true;
	} else {
		info.phase = ctx->config.force_progressive ? 0 : (ctx->gs->csr & (1 << 13) ? 0 : 1);
		info.raw_circuit_scanout = false;
		info.high_resolution_scanout = false;
		info.anti_blur = false;
	}

	info.force_progressive = ctx->config.force_progressive;
	info.overscan = ctx->config.overscan;
	info.crtc_offsets = ctx->config.crtc_offsets;
	info.dst_access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	info.dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	info.dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info.adapt_to_internal_horizontal_resolution = false;

	ScanoutResult scanout = ctx->interface.vsync(info);

	Image* granite_image = scanout.image.get();

	renderer_image image;

	image.image = granite_image->get_image();
	image.width = granite_image->get_width();
	image.height = granite_image->get_height();
	image.format = granite_image->get_format();
	image.view = granite_image->get_view().get_view().view;

	return image;
}

extern "C" void hardware_transfer(void* udata, int path, const void* data, size_t size) {
    hardware_state* ctx = static_cast<hardware_state*>(udata);

	ctx->interface.gif_transfer(path, data, size);
}

void hardware_set_config(void* udata, void* config) {
	hardware_state* ctx = (hardware_state*)udata;

	ctx->config = *(hardware_config*)config;

	Hacks hacks = {};
	hacks.allow_blend_demote = ctx->config.allow_blend_demote;
	hacks.backbuffer_promotion = ctx->config.backbuffer_promotion;
	hacks.disable_mipmaps = ctx->config.disable_mipmaps;
	hacks.unsynced_readbacks = ctx->config.unsynced_readbacks;

	ctx->interface.set_hacks(hacks);

	if (ctx->config.super_sampling == 0) {
		ctx->interface.set_super_sampling_rate((SuperSampling)0, false, false);
	} else {
		SuperSampling super_sampling;

		switch (ctx->config.super_sampling) {
			case 1: super_sampling = SuperSampling::X2; break;
			case 2: super_sampling = SuperSampling::X4; break;
			case 3: super_sampling = SuperSampling::X8; break;
			case 4: super_sampling = SuperSampling::X16; break;
			default: super_sampling = (SuperSampling)0; break;
		}

		ctx->interface.set_super_sampling_rate(super_sampling, true, true);
	}
}