#pragma once

#include <vector>
#include <cstdio>

#include <SDL3/SDL.h>

#include "renderer.hpp"

#include "Granite/vulkan/device.hpp"
#include "Granite/vulkan/context.hpp"
#include "analog/analog_video.hpp"
#include "gs_renderer.hpp"
#include "gs_interface.hpp"

using namespace Vulkan;
using namespace ParallelGS;

namespace iris::gs::renderer::hardware {

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
    gs::Gs* m_gs;

public:
    virtual ~RendererSignalHandler() override = default;
    RendererSignalHandler(gs::Gs* gs) : m_gs(gs) {}

    virtual bool on_signal(uint64_t payload) override {
        return gs::write_signal(m_gs, payload);
    }

    virtual bool on_finish(uint64_t payload) override {
        return gs::write_finish(m_gs, payload);
    }

    virtual bool on_label(uint64_t payload) override {
        return gs::write_label(m_gs, payload);
    }
};

struct state {
    Vulkan::Context granite_ctx;
    Vulkan::Device granite_device;
    Vulkan::ImageHandle current_scanout;
    GSInterface iface;
    ExternallyManagedDevice* device;
    ExternallyManagedInstance* instance;
    RendererSignalHandler* signal_handler;
    HardwareConfig config;
    AnalogVideoFilter analog_video_filter;
    CRTFilter crt_filter;

    gs::Gs* gs;
    gif::Gif* gif;
};

void* create();
bool init(void* udata, const CreateInfo& info);
void reset(void* udata);
void destroy(void* udata);
void set_config(void* udata, void* config);
Image get_frame(void* udata);

void transfer(void* udata, int path, const void* data, size_t size);
void readback(void* udata, void* data, size_t size);
void read_vram(void* udata, void* dst, size_t size);

}
