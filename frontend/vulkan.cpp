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

static inline uint32_t find_memory_type(iris::instance* iris, uint32_t filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(iris->physical_device, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
        if ((filter & (1 << i)) && (mp.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return 0;
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

    iris->cubic_supported = is_device_extension_supported(iris, VK_EXT_FILTER_CUBIC_EXTENSION_NAME);

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

void enumerate_physical_devices(iris::instance* iris) {
    uint32_t count = 0;

    vkEnumeratePhysicalDevices(iris->instance, &count, nullptr);

    if (!count) {
        return;
    }

    std::vector <VkPhysicalDevice> devices(count);

    vkEnumeratePhysicalDevices(iris->instance, &count, devices.data());

    iris->vulkan_gpus.clear();

    for (const VkPhysicalDevice& device : devices) {
        VkPhysicalDeviceProperties properties;

        vkGetPhysicalDeviceProperties(device, &properties);

        iris::vulkan_gpu gpu;

        gpu.device = device;
        gpu.type = properties.deviceType;
        gpu.name = properties.deviceName;
        gpu.api_version = properties.apiVersion;

        iris->vulkan_gpus.push_back(gpu);
    }
}

VkPhysicalDevice find_suitable_physical_device(iris::instance* iris) {
    if (!iris->vulkan_gpus.size())
        return VK_NULL_HANDLE;

    for (int i = 0; i < iris->vulkan_gpus.size(); i++) {
        auto& dev = iris->vulkan_gpus[i];

        if (dev.type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            iris->vulkan_selected_device_index = i;

            return dev.device;
        }
    }

    iris->vulkan_selected_device_index = 0;

    // Just pick the first device for now
    return iris->vulkan_gpus[0].device;
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

texture upload_texture(iris::instance* iris, void* pixels, int width, int height, int stride) {
    texture tex = {};

    tex.width = width;
    tex.height = height;
    tex.stride = stride;
    tex.image_size = width * height * 4;

    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
    VkBuffer staging_buffer = create_buffer(
        iris,
        tex.image_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer_memory
    );

    if (staging_buffer == VK_NULL_HANDLE)
        return {};

    load_buffer(iris, staging_buffer_memory, pixels, tex.image_size);

    // To-do: Transition image layout and copy buffer to image
    // Create the Vulkan image.
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.extent.width = width;
        info.extent.height = height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        if (vkCreateImage(iris->device, &info, VK_NULL_HANDLE, &tex.image) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to create image\n");

            return {};
        }

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(iris->device, tex.image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(iris->physical_device, &memory_properties);

        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
            if ((req.memoryTypeBits & (1 << i)) &&
                (memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                alloc_info.memoryTypeIndex = i;
                break;
            }
        }
        if (vkAllocateMemory(iris->device, &alloc_info, VK_NULL_HANDLE, &tex.image_memory) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to allocate image memory\n");

            return {};
        }

        if (vkBindImageMemory(iris->device, tex.image, tex.image_memory, 0) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to bind image memory\n");

            return {};
        }
    }

    // Create the Image View
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = tex.image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(iris->device, &info, VK_NULL_HANDLE, &tex.image_view) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to create image view\n");

            return {};
        }
    }

    // Create Sampler
    {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.minLod = -1000;
        sampler_info.maxLod = 1000;
        sampler_info.maxAnisotropy = 1.0f;
        if (vkCreateSampler(iris->device, &sampler_info, VK_NULL_HANDLE, &tex.sampler) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to create sampler\n");

            return {};
        }
    }

    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = iris->descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &iris->descriptor_set_layout;
        if (vkAllocateDescriptorSets(iris->device, &alloc_info, &tex.descriptor_set) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to allocate descriptor sets\n");

            return {};
        }
    }

    // Update the Descriptor Set:
    {
        VkDescriptorImageInfo desc_image[1] = {};
        desc_image[0].sampler = tex.sampler;
        desc_image[0].imageView = tex.image_view;
        desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write_desc[1] = {};
        write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_desc[0].dstSet = tex.descriptor_set;
        write_desc[0].descriptorCount = 1;
        write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write_desc[0].pImageInfo = desc_image;
        vkUpdateDescriptorSets(iris->device, 1, write_desc, 0, nullptr);
    }

    VkCommandPool command_pool = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = iris->queue_family;

    if (vkCreateCommandPool(iris->device, &info, VK_NULL_HANDLE, &command_pool) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to create command pool\n");
    
        return {};
    }

    VkCommandBuffer command_buffer;

    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(iris->device, &alloc_info, &command_buffer) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to allocate command buffers\n");

            return {};
        }

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            printf("vulkan: Failed to begin command buffer\n");

            return {};
        }
    }

    // Copy to Image
    {
        VkImageMemoryBarrier copy_barrier[1] = {};
        copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].image = tex.image;
        copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier[0].subresourceRange.levelCount = 1;
        copy_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(command_buffer, staging_buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier use_barrier[1] = {};
        use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].image = tex.image;
        use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier[0].subresourceRange.levelCount = 1;
        use_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
    }

    // End command buffer
    {
        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to end command buffer\n");

            return {};
        }


        if (vkQueueSubmit(iris->queue, 1, &end_info, VK_NULL_HANDLE) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to submit queue\n");

            return {};
        } 

        
        if (vkDeviceWaitIdle(iris->device) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to wait device idle\n");

            return {};
        } 
    }

    vkDestroyCommandPool(iris->device, command_pool, nullptr);
    vkDestroyBuffer(iris->device, staging_buffer, nullptr);
    vkFreeMemory(iris->device, staging_buffer_memory, nullptr);

    return tex;
}

void free_texture(iris::instance* iris, texture& tex) {
    if (!iris->device)
        return;

    if (tex.sampler) vkDestroySampler(iris->device, tex.sampler, nullptr);
    if (tex.image_view) vkDestroyImageView(iris->device, tex.image_view, nullptr);
    if (tex.image) vkDestroyImage(iris->device, tex.image, nullptr);
    if (tex.image_memory) vkFreeMemory(iris->device, tex.image_memory, nullptr);
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
    enumerate_physical_devices(iris);

    iris->vulkan_selected_device_index = 0;

    if (iris->vulkan_physical_device < 0) {
        iris->physical_device = find_suitable_physical_device(iris);
    } else {
        if (iris->vulkan_physical_device > iris->vulkan_gpus.size()) {
            iris->physical_device = VK_NULL_HANDLE;
        } else {
            iris->physical_device = iris->vulkan_gpus[iris->vulkan_physical_device].device;
            iris->vulkan_selected_device_index = iris->vulkan_physical_device;
        }
    }

    if (!iris->physical_device) {
        fprintf(stderr, "vulkan: Failed to find a suitable Vulkan device\n");

        return false;
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(iris->physical_device, &properties);

    printf("vulkan: Using Vulkan device \"%s\". API version %d.%d.%d.%d Driver version %x\n",
        properties.deviceName,
        VK_API_VERSION_MAJOR(properties.apiVersion),
        VK_API_VERSION_MINOR(properties.apiVersion),
        VK_API_VERSION_PATCH(properties.apiVersion),
        VK_API_VERSION_VARIANT(properties.apiVersion),
        properties.driverVersion
    );

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
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_FILTER_CUBIC_EXTENSION_NAME
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

static int ic = 0;

void cleanup(iris::instance* iris) {
    vkQueueWaitIdle(iris->queue);
    vkDeviceWaitIdle(iris->device);

    if (iris->descriptor_set_layout) vkDestroyDescriptorSetLayout(iris->device, iris->descriptor_set_layout, nullptr);
    if (iris->descriptor_pool) vkDestroyDescriptorPool(iris->device, iris->descriptor_pool, nullptr);

    for (int i = 0; i < 3; i++)
        if (iris->sampler[i]) vkDestroySampler(iris->device, iris->sampler[i], nullptr);

    if (iris->vertex_buffer) vkDestroyBuffer(iris->device, iris->vertex_buffer, nullptr);
    if (iris->vertex_staging_buffer) vkDestroyBuffer(iris->device, iris->vertex_staging_buffer, nullptr);
    if (iris->index_buffer) vkDestroyBuffer(iris->device, iris->index_buffer, nullptr);
    if (iris->vertex_staging_buffer_memory) vkFreeMemory(iris->device, iris->vertex_staging_buffer_memory, nullptr);
    if (iris->vertex_buffer_memory) vkFreeMemory(iris->device, iris->vertex_buffer_memory, nullptr);
    if (iris->index_buffer_memory) vkFreeMemory(iris->device, iris->index_buffer_memory, nullptr);
    if (iris->pipeline) vkDestroyPipeline(iris->device, iris->pipeline, nullptr);
    // if (iris->surface) vkDestroySurfaceKHR(iris->instance, iris->surface, nullptr);
    if (iris->render_pass) vkDestroyRenderPass(iris->device, iris->render_pass, nullptr);
    if (iris->pipeline_layout) vkDestroyPipelineLayout(iris->device, iris->pipeline_layout, nullptr);
    if (iris->device) vkDestroyDevice(iris->device, nullptr);
    if (iris->instance) vkDestroyInstance(iris->instance, nullptr);
}

void insert_image_memory_barrier(
    VkCommandBuffer buffer,
    VkImage image,
    VkAccessFlags src_access_mask,
    VkAccessFlags dst_access_mask,
    VkImageLayout old_image_layout,
    VkImageLayout new_image_layout,
    VkPipelineStageFlags src_stage_mask,
    VkPipelineStageFlags dst_stage_mask,
    VkImageSubresourceRange subresource_range)
{
    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcAccessMask = src_access_mask;
    image_memory_barrier.dstAccessMask = dst_access_mask;
    image_memory_barrier.oldLayout = old_image_layout;
    image_memory_barrier.newLayout = new_image_layout;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(
        buffer,
        src_stage_mask,
        dst_stage_mask,
        0,
        0, nullptr,
        0, nullptr,
        1, &image_memory_barrier
    );
}

void* read_image(iris::instance* iris, VkImage src_image, VkFormat format, int width, int height) {
    if (src_image == VK_NULL_HANDLE) {
        printf("vulkan: Source image is null\n");

        return nullptr;
    }

    if (!width || !height) {
        printf("vulkan: Invalid image dimensions for readback (%dx%d)\n", width, height);

        return nullptr;
    }

    bool supports_blit = true;

    // Check blit support for source and destination
    VkFormatProperties format_props;

    // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
    vkGetPhysicalDeviceFormatProperties(iris->physical_device, format, &format_props);

    if (!(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        printf("Device does not support blitting from optimal tiled images, using copy instead of blit!\n");

        supports_blit = false;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(iris->physical_device, VK_FORMAT_R8G8B8A8_UNORM, &format_props);

    if (!(format_props.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        printf("Device does not support blitting to linear tiled images, using copy instead of blit!\n");

        supports_blit = false;
    }

    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo image_create_info = {};
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.extent.depth = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.mipLevels = 1;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Create the image
    VkImage dst_image;

    if (vkCreateImage(iris->device, &image_create_info, nullptr, &dst_image) != VK_SUCCESS) {
        printf("Failed to create image for readback\n");

        return nullptr;
    }

    VkDeviceMemory dst_image_memory;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(iris->device, dst_image, &req);
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = find_memory_type(iris, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(iris->device, &alloc_info, VK_NULL_HANDLE, &dst_image_memory) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to allocate image memory for readback\n");

        return {};
    }

    if (vkBindImageMemory(iris->device, dst_image, dst_image_memory, 0) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to bind image memory for readback\n");

        return {};
    }

    // Do the actual blit from the swapchain image to our host visible destination image
    VkCommandPool command_pool = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = iris->queue_family;

    if (vkCreateCommandPool(iris->device, &info, VK_NULL_HANDLE, &command_pool) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to create command pool for readback\n");
    
        return {};
    }

    VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandPool = command_pool;
    cmd_buffer_alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(iris->device, &cmd_buffer_alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        printf("vulkan: Failed to begin command buffer for readback\n");

        return {};
    }

    // Transition destination image to transfer destination layout
    insert_image_memory_barrier(
        command_buffer,
        dst_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    );

    // Transition swapchain image from present to transfer source layout
    insert_image_memory_barrier(
        command_buffer,
        src_image,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    );

    // If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
    if (supports_blit) {
        // Define the region to blit (we will blit the whole swapchain image)
        VkOffset3D blitSize;
        blitSize.x = width;
        blitSize.y = height;
        blitSize.z = 1;
        VkImageBlit imageBlitRegion{};
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcOffsets[1] = blitSize;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstOffsets[1] = blitSize;

        // Issue the blit command
        vkCmdBlitImage(
            command_buffer,
            src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageBlitRegion,
            VK_FILTER_NEAREST
        );
    } else {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy imageCopyRegion = {};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = width;
        imageCopyRegion.extent.height = height;
        imageCopyRegion.extent.depth = 1;

        // Issue the copy command
        vkCmdCopyImage(
            command_buffer,
            src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageCopyRegion
        );
    }

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    insert_image_memory_barrier(
        command_buffer,
        dst_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    );

    // Transition back the swap chain image after the blit is done
    insert_image_memory_barrier(
        command_buffer,
        src_image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    );

    // End command buffer
    {
        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to end command buffer\n");

            return {};
        }


        if (vkQueueSubmit(iris->queue, 1, &end_info, VK_NULL_HANDLE) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to submit queue\n");

            return {};
        } 

        
        if (vkDeviceWaitIdle(iris->device) != VK_SUCCESS) {
            fprintf(stderr, "vulkan: Failed to wait device idle\n");

            return {};
        } 
    }

    vkDestroyCommandPool(iris->device, command_pool, nullptr);

    // Get layout of the image (including row pitch)
    VkImageSubresource subresource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subresource_layout;
    vkGetImageSubresourceLayout(iris->device, dst_image, &subresource, &subresource_layout);

    // Map image memory so we can start copying from it
    const char* data;
    vkMapMemory(iris->device, dst_image_memory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
    data += subresource_layout.offset;

    void* buf = malloc(width * height * 4);

    memcpy(buf, data, width * height * 4);

    // Clean up resources
    vkUnmapMemory(iris->device, dst_image_memory);
    vkFreeMemory(iris->device, dst_image_memory, nullptr);
    vkDestroyImage(iris->device, dst_image, nullptr);

    return buf;
}

}