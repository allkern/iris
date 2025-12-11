#include <algorithm>

#include "config.hpp"
#include "iris.hpp"

#include <SDL3/SDL_vulkan.h>

#include <volk.h>

namespace iris::vulkan {

std::vector <VkExtensionProperties> get_instance_extensions() {
    std::vector <VkExtensionProperties> extensions;

    uint32_t count;

    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    extensions.resize(count);

    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to enumerate instance extensions\n");

        return {};
    }

    return extensions;
}

std::vector <VkLayerProperties> get_instance_layers() {
    std::vector <VkLayerProperties> layers;

    uint32_t count;

    vkEnumerateInstanceLayerProperties(&count, nullptr);

    layers.resize(count);

    if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to enumerate instance layers\n");

        return {};
    }

    return layers;
}

bool is_instance_extension_supported(iris::instance* iris, const char* name) {
    return std::find_if(
        iris->instance_extensions.begin(),
        iris->instance_extensions.end(),
        [name](const VkExtensionProperties& ext) {
            return strncmp(ext.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
        }
    ) != iris->instance_extensions.end();
}

bool is_instance_layer_supported(iris::instance* iris, const char* name) {
    return std::find_if(
        iris->instance_layers.begin(),
        iris->instance_layers.end(),
        [name](const VkLayerProperties& layer) {
            return strncmp(layer.layerName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
        }
    ) != iris->instance_layers.end();
}

bool is_device_extension_supported(iris::instance* iris, const char* name) {
    return std::find_if(
        iris->device_extensions.begin(),
        iris->device_extensions.end(),
        [name](const VkExtensionProperties& ext) {
            return strncmp(ext.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
        }
    ) != iris->device_extensions.end();
}

bool is_device_layer_supported(iris::instance* iris, const char* name) {
    return std::find_if(
        iris->device_layers.begin(),
        iris->device_layers.end(),
        [name](const VkLayerProperties& layer) {
            return strncmp(layer.layerName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
        }
    ) != iris->device_layers.end();
}

std::vector <VkExtensionProperties> get_device_extensions(iris::instance* iris) {
    std::vector <VkExtensionProperties> extensions;

    uint32_t count;

    vkEnumerateDeviceExtensionProperties(iris->physical_device, nullptr, &count, nullptr);

    extensions.resize(count);

    if (vkEnumerateDeviceExtensionProperties(iris->physical_device, nullptr, &count, extensions.data()) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to enumerate device extensions\n");

        return {};
    }

    return extensions;
}

std::vector <VkLayerProperties> get_device_layers(iris::instance* iris) {
    std::vector <VkLayerProperties> layers;

    uint32_t count;

    vkEnumerateDeviceLayerProperties(iris->physical_device, &count, nullptr);

    layers.resize(count);

    if (vkEnumerateDeviceLayerProperties(iris->physical_device, &count, layers.data()) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to enumerate device layers\n");

        return {};
    }

    return layers;
}

struct instance_create_info {
    std::vector <const char*> enabled_extensions;
    std::vector <const char*> enabled_layers;
    VkInstanceCreateFlags flags = 0;
};

VkInstance create_instance(iris::instance* iris, const instance_create_info& info) {
    VkInstance instance = VK_NULL_HANDLE;

    for (const char* ext : info.enabled_extensions) {
        if (!is_instance_extension_supported(iris, ext)) {
            fprintf(stderr, "vulkan: Requested instance extension not supported: %s\n", ext);

            continue;
        }

        iris->enabled_instance_extensions.push_back(ext);
    }

    for (const char* layer : info.enabled_layers) {
        if (!is_instance_layer_supported(iris, layer)) {
            fprintf(stderr, "vulkan: Requested instance layer not supported: %s\n", layer);

            continue;
        }

        iris->enabled_instance_layers.push_back(layer);
    }

    iris->app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    iris->app_info.pApplicationName = IRIS_TITLE;
    iris->app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    iris->app_info.pEngineName = "Vulkan";
    iris->app_info.engineVersion = VK_MAKE_VERSION(1, 1, 0);
    iris->app_info.apiVersion = IRIS_VULKAN_API_VERSION;
    iris->app_info.pNext = VK_NULL_HANDLE;

    iris->instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    iris->instance_create_info.pApplicationInfo = &iris->app_info;
    iris->instance_create_info.enabledExtensionCount = iris->enabled_instance_extensions.size();
    iris->instance_create_info.ppEnabledExtensionNames = iris->enabled_instance_extensions.data();

    iris->instance_create_info.enabledLayerCount = iris->enabled_instance_layers.size();
    iris->instance_create_info.ppEnabledLayerNames = iris->enabled_instance_layers.data();
    iris->instance_create_info.flags = info.flags;

    if (vkCreateInstance(&iris->instance_create_info, nullptr, &instance) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return instance;
}

struct device_create_info {
    std::vector <const char*> enabled_extensions;
    std::vector <const char*> enabled_layers;
    VkPhysicalDeviceFeatures enabled_features = {};
    void* data;
};

VkDevice create_device(iris::instance* iris, const device_create_info& info) {
    VkDevice device = VK_NULL_HANDLE;

    for (const char* ext : info.enabled_extensions) {
        if (!is_device_extension_supported(iris, ext)) {
            fprintf(stderr, "vulkan: Requested device extension not supported: %s\n", ext);

            continue;
        }

        iris->enabled_device_extensions.push_back(ext);
    }

    for (const char* layer : info.enabled_layers) {
        if (!is_device_layer_supported(iris, layer)) {
            fprintf(stderr, "vulkan: Requested device layer not supported: %s\n", layer);

            continue;
        }

        iris->enabled_device_layers.push_back(layer);
    }

    iris->device_features = {};
    iris->device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    iris->device_features.pNext = info.data;

    VkPhysicalDeviceFeatures2 supported_features = {};
    supported_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported_features.pNext = nullptr;

    vkGetPhysicalDeviceFeatures2(iris->physical_device, &supported_features);

#define SET_FEATURE(f) \
    iris->device_features.features.f = (supported_features.features.f && info.enabled_features.f)

    SET_FEATURE(robustBufferAccess);
    SET_FEATURE(fullDrawIndexUint32);
    SET_FEATURE(imageCubeArray);
    SET_FEATURE(independentBlend);
    SET_FEATURE(geometryShader);
    SET_FEATURE(tessellationShader);
    SET_FEATURE(sampleRateShading);
    SET_FEATURE(dualSrcBlend);
    SET_FEATURE(logicOp);
    SET_FEATURE(multiDrawIndirect);
    SET_FEATURE(drawIndirectFirstInstance);
    SET_FEATURE(depthClamp);
    SET_FEATURE(depthBiasClamp);
    SET_FEATURE(fillModeNonSolid);
    SET_FEATURE(depthBounds);
    SET_FEATURE(wideLines);
    SET_FEATURE(largePoints);
    SET_FEATURE(alphaToOne);
    SET_FEATURE(multiViewport);
    SET_FEATURE(samplerAnisotropy);
    SET_FEATURE(textureCompressionETC2);
    SET_FEATURE(textureCompressionASTC_LDR);
    SET_FEATURE(textureCompressionBC);
    SET_FEATURE(occlusionQueryPrecise);
    SET_FEATURE(pipelineStatisticsQuery);
    SET_FEATURE(vertexPipelineStoresAndAtomics);
    SET_FEATURE(fragmentStoresAndAtomics);
    SET_FEATURE(shaderTessellationAndGeometryPointSize);
    SET_FEATURE(shaderImageGatherExtended);
    SET_FEATURE(shaderStorageImageExtendedFormats);
    SET_FEATURE(shaderStorageImageMultisample);
    SET_FEATURE(shaderStorageImageReadWithoutFormat);
    SET_FEATURE(shaderStorageImageWriteWithoutFormat);
    SET_FEATURE(shaderUniformBufferArrayDynamicIndexing);
    SET_FEATURE(shaderSampledImageArrayDynamicIndexing);
    SET_FEATURE(shaderStorageBufferArrayDynamicIndexing);
    SET_FEATURE(shaderStorageImageArrayDynamicIndexing);
    SET_FEATURE(shaderClipDistance);
    SET_FEATURE(shaderCullDistance);
    SET_FEATURE(shaderFloat64);
    SET_FEATURE(shaderInt64);
    SET_FEATURE(shaderInt16);
    SET_FEATURE(shaderResourceResidency);
    SET_FEATURE(shaderResourceMinLod);
    SET_FEATURE(sparseBinding);
    SET_FEATURE(sparseResidencyBuffer);
    SET_FEATURE(sparseResidencyImage2D);
    SET_FEATURE(sparseResidencyImage3D);
    SET_FEATURE(sparseResidency2Samples);
    SET_FEATURE(sparseResidency4Samples);
    SET_FEATURE(sparseResidency8Samples);
    SET_FEATURE(sparseResidency16Samples);
    SET_FEATURE(sparseResidencyAliased);
    SET_FEATURE(variableMultisampleRate);
    SET_FEATURE(inheritedQueries);

#undef SET_FEATURE

    const float queue_priority[] = { 1.0f };

    iris->queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    iris->queue_create_info.queueFamilyIndex = iris->queue_family;
    iris->queue_create_info.queueCount = 1;
    iris->queue_create_info.pQueuePriorities = queue_priority;

    iris->device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    iris->device_create_info.queueCreateInfoCount = 1;
    iris->device_create_info.pQueueCreateInfos = &iris->queue_create_info;
    iris->device_create_info.enabledExtensionCount = iris->enabled_device_extensions.size();
    iris->device_create_info.ppEnabledExtensionNames = iris->enabled_device_extensions.data();
    iris->device_create_info.enabledLayerCount = iris->enabled_device_layers.size();
    iris->device_create_info.ppEnabledLayerNames = iris->enabled_device_layers.data();
    iris->device_create_info.pEnabledFeatures = VK_NULL_HANDLE;
    iris->device_create_info.pNext = &iris->device_features;

    if (vkCreateDevice(iris->physical_device, &iris->device_create_info, nullptr, &device) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return device;
}

VkPhysicalDevice find_suitable_physical_device(iris::instance* iris) {
    uint32_t count = 0;

    vkEnumeratePhysicalDevices(iris->instance, &count, nullptr);

    if (!count) {
        return VK_NULL_HANDLE;
    }

    std::vector <VkPhysicalDevice> devices(count);

    vkEnumeratePhysicalDevices(iris->instance, &count, devices.data());

    for (const VkPhysicalDevice& device : devices) {
        VkPhysicalDeviceProperties properties;

        vkGetPhysicalDeviceProperties(device, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            return device;
        }
    }

    // Just pick the first device for now
    return devices[0];
}

int find_graphics_queue_family_index(iris::instance* iris) {
    uint32_t count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(iris->physical_device, &count, nullptr);

    if (!count) {
        return -1;
    }

    std::vector <VkQueueFamilyProperties> queue_families(count);

    vkGetPhysicalDeviceQueueFamilyProperties(iris->physical_device, &count, queue_families.data());

    // Just return the first graphics-capable queue family, we should
    // actually be looking for dedicated compute/transfer queues
    for (uint32_t i = 0; i < count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }

    return -1;
}

VkBuffer create_buffer(iris::instance* iris, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory& buffer_memory) {
    VkBuffer buffer = VK_NULL_HANDLE;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(iris->device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to create buffer\n");

        return VK_NULL_HANDLE;
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(iris->device, buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(iris->physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((memory_requirements.memoryTypeBits & (1 << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            alloc_info.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(iris->device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to allocate buffer memory\n");

        vkDestroyBuffer(iris->device, buffer, nullptr);

        return VK_NULL_HANDLE;
    }

    vkBindBufferMemory(iris->device, buffer, buffer_memory, 0);

    return buffer;
}

void load_buffer(iris::instance* iris, VkDeviceMemory buffer_memory, void* data, VkDeviceSize size) {
    void* ptr;

    vkMapMemory(iris->device, buffer_memory, 0, size, 0, &ptr);
    memcpy(ptr, data, (size_t)size);
    vkUnmapMemory(iris->device, buffer_memory);
}

bool copy_buffer(iris::instance* iris, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandPool command_pool = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = iris->queue_family;

    if (vkCreateCommandPool(iris->device, &info, VK_NULL_HANDLE, &command_pool) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to create command pool\n");
    
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(iris->device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy_region{};
    copy_region.size = size;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(iris->queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(iris->queue);

    vkFreeCommandBuffers(iris->device, command_pool, 1, &command_buffer);
    vkDestroyCommandPool(iris->device, command_pool, VK_NULL_HANDLE);

    return true;
}

bool create_descriptor_pool(iris::instance* iris) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_info.maxSets = 0;

    for (VkDescriptorPoolSize& pool_size : pool_sizes)
        pool_info.maxSets += pool_size.descriptorCount;

    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(iris->device, &pool_info, VK_NULL_HANDLE, &iris->descriptor_pool) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to create descriptor pool\n");

        return false;
    }

    return true;
}

bool upload_texture(iris::instance* iris, void* pixels, int width, int height, int stride) {
    VkDeviceSize image_size = width * height * stride;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;

    staging_buffer = create_buffer(
        iris,
        image_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer_memory
    );

    if (staging_buffer == VK_NULL_HANDLE) {
        return false;
    }

    load_buffer(iris, staging_buffer_memory, pixels, image_size);

    // To-do: Transition image layout and copy buffer to image

    vkDestroyBuffer(iris->device, staging_buffer, nullptr);
    vkFreeMemory(iris->device, staging_buffer_memory, nullptr);

    return true;
}

bool init(iris::instance* iris, bool enable_validation) {
    if (volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to initialize volk loader\n");

        return false;
    }

    iris->instance_extensions = get_instance_extensions();
    iris->instance_layers = get_instance_layers();

    std::vector <const char*> extensions = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    std::vector <const char*> layers;

    if (enable_validation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    // Push SDL extensions
    uint32_t sdl_extension_count;
    auto sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);

    for (uint32_t i = 0; i < sdl_extension_count; i++) {
        extensions.push_back(sdl_extensions[i]);
    }

    VkInstanceCreateFlags flags = 0;

    // Needed for MoltenVK on macOS
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    instance_create_info instance_info = {};
    instance_info.enabled_extensions = extensions;
    instance_info.enabled_layers = layers;
    instance_info.flags = flags;

    iris->instance = create_instance(iris, instance_info);

    if (!iris->instance) {
        fprintf(stderr, "vulkan: Failed to create Vulkan instance\n");

        return false;
    }

    volkLoadInstance(iris->instance);

    // Find a suitable Vulkan physical device (GPU)
    iris->physical_device = find_suitable_physical_device(iris);

    if (!iris->physical_device) {
        fprintf(stderr, "vulkan: Failed to find a suitable Vulkan device\n");

        return false;
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(iris->physical_device, &properties);

    printf("vulkan: Using Vulkan device \"%s\"\n", properties.deviceName);

    iris->device_extensions = get_device_extensions(iris);
    iris->device_layers = get_device_layers(iris);

    // Find a graphics-capable queue family
    int queue_family = find_graphics_queue_family_index(iris);

    if (queue_family == -1) {
        fprintf(stderr, "vulkan: Failed to find a graphics-capable queue family\n");

        return false;
    }

    iris->queue_family = queue_family;

    // To-do: Query required extensions/features from backends here.
    //        For now we'll just initialize a fixed set of extensions
    //        and features.

    device_create_info device_info = {};
    device_info.enabled_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
        // VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
        VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
        VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
        VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
        VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,
        VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
        // VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
        VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
        // VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
        VK_KHR_LOAD_STORE_OP_NONE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };
    device_info.enabled_layers = {};

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    device_info.enabled_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    device_info.enabled_features = {};
    device_info.enabled_features.shaderInt16 = VK_TRUE;

    iris->vulkan_11_features.pNext = &iris->vulkan_12_features;
    iris->vulkan_12_features.pNext = &iris->subgroup_size_control_features;
    iris->subgroup_size_control_features.pNext = VK_NULL_HANDLE;

    iris->vulkan_11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    iris->vulkan_12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    iris->subgroup_size_control_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES;

    iris->vulkan_11_features.storageBuffer16BitAccess = VK_TRUE;
    iris->vulkan_11_features.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    iris->vulkan_12_features.descriptorIndexing = VK_TRUE;
    iris->vulkan_12_features.descriptorBindingPartiallyBound = VK_TRUE;
    iris->vulkan_12_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    iris->vulkan_12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    iris->vulkan_12_features.runtimeDescriptorArray = VK_TRUE;
    iris->vulkan_12_features.timelineSemaphore = VK_TRUE;
    iris->vulkan_12_features.bufferDeviceAddress = VK_TRUE;
    iris->vulkan_12_features.scalarBlockLayout = VK_TRUE;
    iris->vulkan_12_features.storageBuffer8BitAccess = VK_TRUE;
    iris->vulkan_12_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    
    iris->subgroup_size_control_features.subgroupSizeControl = VK_TRUE;
    iris->subgroup_size_control_features.computeFullSubgroups = VK_TRUE;

    // Chain in all feature structs
    device_info.data = &iris->vulkan_11_features;

    iris->device = create_device(iris, device_info);

    if (!iris->device) {
        fprintf(stderr, "vulkan: Failed to create Vulkan device\n");

        return false;
    }

    vkGetDeviceQueue(iris->device, iris->queue_family, 0, &iris->queue);

    iris->indices = { 0, 1, 2, 2, 3, 0 };

    iris->vertex_buffer_size = sizeof(vertex) * iris->vertices.size();

    // Create vertex and index buffers
    // Create and populate index buffer immediately by creating
    // a staging buffer, filling it, and then copying it over to
    // the device local index buffer.
    // The vertex buffer will be updated dynamically each frame.
    VkDeviceMemory index_staging_buffer_memory;
    VkDeviceSize index_buffer_size = sizeof(uint16_t) * iris->indices.size();

    iris->index_buffer = create_buffer(
        iris,
        sizeof(uint16_t) * iris->indices.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        iris->index_buffer_memory
    );

    VkBuffer index_staging_buffer = create_buffer(
        iris,
        index_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        index_staging_buffer_memory
    );

    load_buffer(iris, index_staging_buffer_memory, iris->indices.data(), index_buffer_size);
    copy_buffer(iris, index_staging_buffer, iris->index_buffer, index_buffer_size);

    iris->vertex_buffer = create_buffer(
        iris,
        iris->vertex_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        iris->vertex_buffer_memory
    );

    iris->vertex_staging_buffer = create_buffer(
        iris,
        iris->vertex_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        iris->vertex_staging_buffer_memory
    );

    // We don't need the staging buffer anymore
    vkFreeMemory(iris->device, index_staging_buffer_memory, nullptr);
    vkDestroyBuffer(iris->device, index_staging_buffer, nullptr);

    create_descriptor_pool(iris);

    return true;
}

void cleanup(iris::instance* iris) {
    vkDestroyDescriptorSetLayout(iris->device, iris->descriptor_set_layout, nullptr);
    vkDestroyDescriptorPool(iris->device, iris->descriptor_pool, nullptr);
    vkDestroySampler(iris->device, iris->sampler, nullptr);
    vkDestroyBuffer(iris->device, iris->vertex_buffer, nullptr);
    vkDestroyBuffer(iris->device, iris->vertex_staging_buffer, nullptr);
    vkDestroyBuffer(iris->device, iris->index_buffer, nullptr);
    vkFreeMemory(iris->device, iris->vertex_staging_buffer_memory, nullptr);
    vkFreeMemory(iris->device, iris->vertex_buffer_memory, nullptr);
    vkFreeMemory(iris->device, iris->index_buffer_memory, nullptr);
    vkDestroyPipeline(iris->device, iris->pipeline, nullptr);
    vkDestroyRenderPass(iris->device, iris->render_pass, nullptr);
    vkDestroyPipelineLayout(iris->device, iris->pipeline_layout, nullptr);
    vkDestroyDevice(iris->device, nullptr);
    vkDestroyInstance(iris->instance, nullptr);
}

}