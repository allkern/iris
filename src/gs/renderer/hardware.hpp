#pragma once

#include <vector>
#include <cstdio>

#include <SDL3/SDL.h>

#include "renderer.hpp"

#include "Granite/vulkan/device.hpp"
#include "Granite/vulkan/context.hpp"
#include "gs_renderer.hpp"
#include "gs_interface.hpp"

using namespace Vulkan;
using namespace ParallelGS;

class ExternallyManagedDevice : public DeviceFactory {
	VkDevice m_device = nullptr;
	VkDeviceCreateInfo m_create_info;

public:
	// Device is externally managed
	ExternallyManagedDevice(VkDevice device, VkDeviceCreateInfo create_info) :
		m_device(device),
		m_create_info(create_info) {}; 

	// Create a device ourselves (unused)
	ExternallyManagedDevice() = delete; 

	virtual ~ExternallyManagedDevice() override = default;

	VkDevice create_device(VkPhysicalDevice gpu, const VkDeviceCreateInfo *info) override {
		return m_device;
	}

	const VkDeviceCreateInfo *get_existing_create_info() override {
		return &m_create_info;
	}

	bool factory_owns_created_device() override {
		return true;
	}
};

class ExternallyManagedInstance : public InstanceFactory {
	VkInstanceCreateInfo m_create_info = {};
	VkInstance m_instance = nullptr;

public:
	// Instance is externally managed
	ExternallyManagedInstance(VkInstance instance, VkInstanceCreateInfo create_info) :
		m_instance(instance),
		m_create_info(create_info) {}; 

	// Create an instance ourselves (unused)
	ExternallyManagedInstance() = delete; 

	~ExternallyManagedInstance() override = default;

	VkInstance create_instance(const VkInstanceCreateInfo *info) override {
		if (m_instance) return m_instance;

		// Create a new instance, I won't implement this
		return m_instance;
	}

	const VkInstanceCreateInfo* get_existing_create_info() override {
		return &m_create_info;
	}

	bool factory_owns_created_instance() override {
		return true;
	}
};

class RendererSignalHandler : public SignalInterface {
	struct ps2_gs* m_gs;

public:
	virtual ~RendererSignalHandler() override = default;
	RendererSignalHandler(struct ps2_gs* gs) : m_gs(gs) {}

	virtual bool on_signal(uint64_t payload) override {
		return ps2_gs_write_signal(m_gs, payload);
	}

	virtual bool on_finish(uint64_t payload) override {
		return ps2_gs_write_finish(m_gs, payload);
	}

	virtual bool on_label(uint64_t payload) override {
		return ps2_gs_write_label(m_gs, payload);;
	}
};

struct hardware_state {
    Vulkan::Context granite_ctx;
    Vulkan::Device granite_device;
    GSInterface interface;
	ExternallyManagedDevice* device;
	ExternallyManagedInstance* instance;
	RendererSignalHandler* signal_handler;

    struct ps2_gs* gs;
    struct ps2_gif* gif;
};

void* hardware_create();
bool hardware_init(void* udata, const renderer_create_info& info);
void hardware_reset(void* udata);
void hardware_destroy(void* udata);
renderer_image hardware_get_frame(void* udata);

extern "C" {
void hardware_transfer(void* udata, int path, const void* data, size_t size);
}