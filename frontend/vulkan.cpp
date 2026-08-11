#include <algorithm>

#include "config.hpp"
#include "iris.hpp"

#include "stb_image.h"

#include <SDL3/SDL_vulkan.h>

#include <volk.h>

namespace iris::vulkan {

std::vector <VkExtensionProperties> get_instance_extensions(Instance* iris) {
    std::vector <VkExtensionProperties> extensions;

    uint32_t count;

    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    extensions.resize(count);

    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to enumerate instance extensions");

        return {};
    }

    return extensions;
}

std::vector <VkLayerProperties> get_instance_layers(Instance* iris) {
    std::vector <VkLayerProperties> layers;

    uint32_t count;

    vkEnumerateInstanceLayerProperties(&count, nullptr);

    layers.resize(count);

    if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to enumerate instance layers");

        return {};
    }

    return layers;
}

bool is_instance_extension_supported(Instance* iris, const char* name) {
    return std::find_if(
        iris->vk.instance_extensions.begin(),
        iris->vk.instance_extensions.end(),
        [name](const VkExtensionProperties& ext) {
            return strncmp(ext.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
        }
    ) != iris->vk.instance_extensions.end();
}

bool is_instance_layer_supported(Instance* iris, const char* name) {
    return std::find_if(
        iris->vk.instance_layers.begin(),
        iris->vk.instance_layers.end(),
        [name](const VkLayerProperties& layer) {
            return strncmp(layer.layerName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
        }
    ) != iris->vk.instance_layers.end();
}

bool is_device_extension_supported(Instance* iris, const char* name) {
    return std::find_if(
        iris->vk.device_extensions.begin(),
        iris->vk.device_extensions.end(),
        [name](const VkExtensionProperties& ext) {
            return strncmp(ext.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
        }
    ) != iris->vk.device_extensions.end();
}

bool is_device_layer_supported(Instance* iris, const char* name) {
    return std::find_if(
        iris->vk.device_layers.begin(),
        iris->vk.device_layers.end(),
        [name](const VkLayerProperties& layer) {
            return strncmp(layer.layerName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
        }
    ) != iris->vk.device_layers.end();
}

std::vector <VkExtensionProperties> get_device_extensions(Instance* iris) {
    std::vector <VkExtensionProperties> extensions;

    uint32_t count;

    vkEnumerateDeviceExtensionProperties(iris->vk.physical_device, nullptr, &count, nullptr);

    extensions.resize(count);

    if (vkEnumerateDeviceExtensionProperties(iris->vk.physical_device, nullptr, &count, extensions.data()) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to enumerate device extensions");

        return {};
    }

    return extensions;
}

std::vector <VkLayerProperties> get_device_layers(Instance* iris) {
    std::vector <VkLayerProperties> layers;

    uint32_t count;

    vkEnumerateDeviceLayerProperties(iris->vk.physical_device, &count, nullptr);

    layers.resize(count);

    if (vkEnumerateDeviceLayerProperties(iris->vk.physical_device, &count, layers.data()) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to enumerate device layers");

        return {};
    }

    return layers;
}

struct instance_create_info {
    std::vector <const char*> enabled_extensions;
    std::vector <const char*> enabled_layers;
    VkInstanceCreateFlags flags = 0;
};

VkInstance create_instance(Instance* iris, const instance_create_info& info) {
    VkInstance vk_instance = VK_NULL_HANDLE;

    for (const char* ext : info.enabled_extensions) {
        if (!is_instance_extension_supported(iris, ext)) {
            iris_warning(&iris->log.vulkan, "Requested instance extension not supported: {}", ext);

            continue;
        }

        iris->vk.enabled_instance_extensions.push_back(ext);
    }

    for (const char* layer : info.enabled_layers) {
        if (!is_instance_layer_supported(iris, layer)) {
            iris_warning(&iris->log.vulkan, "Requested instance layer not supported: {}", layer);

            continue;
        }

        iris->vk.enabled_instance_layers.push_back(layer);
    }

    iris->vk.app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    iris->vk.app_info.pApplicationName = IRIS_TITLE;
    iris->vk.app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    iris->vk.app_info.pEngineName = "Vulkan";
    iris->vk.app_info.engineVersion = VK_MAKE_VERSION(1, 1, 0);
    iris->vk.app_info.apiVersion = IRIS_VULKAN_API_VERSION;
    iris->vk.app_info.pNext = VK_NULL_HANDLE;

    iris->vk.instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    iris->vk.instance_create_info.pApplicationInfo = &iris->vk.app_info;
    iris->vk.instance_create_info.enabledExtensionCount = iris->vk.enabled_instance_extensions.size();
    iris->vk.instance_create_info.ppEnabledExtensionNames = iris->vk.enabled_instance_extensions.data();

    iris->vk.instance_create_info.enabledLayerCount = iris->vk.enabled_instance_layers.size();
    iris->vk.instance_create_info.ppEnabledLayerNames = iris->vk.enabled_instance_layers.data();
    iris->vk.instance_create_info.flags = info.flags;

    if (vkCreateInstance(&iris->vk.instance_create_info, nullptr, &vk_instance) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return vk_instance;
}

static inline uint32_t find_memory_type(Instance* iris, uint32_t filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(iris->vk.physical_device, &mp);

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

VkDevice create_device(Instance* iris, const device_create_info& info) {
    VkDevice device = VK_NULL_HANDLE;

    for (const char* ext : info.enabled_extensions) {
        if (!is_device_extension_supported(iris, ext)) {
            iris_warning(&iris->log.vulkan, "Requested device extension not supported: {}", ext);

            continue;
        }

        iris->vk.enabled_device_extensions.push_back(ext);
    }

    iris->vk.cubic_supported = is_device_extension_supported(iris, VK_EXT_FILTER_CUBIC_EXTENSION_NAME);

    for (const char* layer : info.enabled_layers) {
        if (!is_device_layer_supported(iris, layer)) {
            iris_warning(&iris->log.vulkan, "Requested device layer not supported: {}", layer);

            continue;
        }

        iris->vk.enabled_device_layers.push_back(layer);
    }

    iris->vk.device_features = {};
    iris->vk.device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    iris->vk.device_features.pNext = info.data;

    VkPhysicalDeviceFeatures2 supported_features = {};
    supported_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported_features.pNext = nullptr;

    vkGetPhysicalDeviceFeatures2(iris->vk.physical_device, &supported_features);

#define SET_FEATURE(f) \
    iris->vk.device_features.features.f = (supported_features.features.f && info.enabled_features.f)

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

    iris->vk.queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    iris->vk.queue_create_info.queueFamilyIndex = iris->vk.queue_family;
    iris->vk.queue_create_info.queueCount = 1;
    iris->vk.queue_create_info.pQueuePriorities = queue_priority;

    iris->vk.device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    iris->vk.device_create_info.queueCreateInfoCount = 1;
    iris->vk.device_create_info.pQueueCreateInfos = &iris->vk.queue_create_info;
    iris->vk.device_create_info.enabledExtensionCount = iris->vk.enabled_device_extensions.size();
    iris->vk.device_create_info.ppEnabledExtensionNames = iris->vk.enabled_device_extensions.data();
    iris->vk.device_create_info.enabledLayerCount = iris->vk.enabled_device_layers.size();
    iris->vk.device_create_info.ppEnabledLayerNames = iris->vk.enabled_device_layers.data();
    iris->vk.device_create_info.pEnabledFeatures = VK_NULL_HANDLE;
    iris->vk.device_create_info.pNext = &iris->vk.device_features;

    if (vkCreateDevice(iris->vk.physical_device, &iris->vk.device_create_info, nullptr, &device) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return device;
}

void enumerate_physical_devices(Instance* iris) {
    uint32_t count = 0;

    vkEnumeratePhysicalDevices(iris->vk.instance, &count, nullptr);

    if (!count) {
        return;
    }

    std::vector <VkPhysicalDevice> devices(count);

    vkEnumeratePhysicalDevices(iris->vk.instance, &count, devices.data());

    iris->vk.vulkan_gpus.clear();

    for (const VkPhysicalDevice& device : devices) {
        VkPhysicalDeviceProperties properties;

        vkGetPhysicalDeviceProperties(device, &properties);

        VulkanGpu gpu;

        gpu.device = device;
        gpu.type = properties.deviceType;
        gpu.name = properties.deviceName;
        gpu.api_version = properties.apiVersion;

        iris->vk.vulkan_gpus.push_back(gpu);
    }
}

VkPhysicalDevice find_suitable_physical_device(Instance* iris) {
    if (!iris->vk.vulkan_gpus.size())
        return VK_NULL_HANDLE;

    for (int i = 0; i < iris->vk.vulkan_gpus.size(); i++) {
        auto& dev = iris->vk.vulkan_gpus[i];

        if (dev.type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            iris->vk.vulkan_selected_device_index = i;

            return dev.device;
        }
    }

    iris->vk.vulkan_selected_device_index = 0;

    // Just pick the first device for now
    return iris->vk.vulkan_gpus[0].device;
}

int find_graphics_queue_family_index(Instance* iris) {
    uint32_t count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(iris->vk.physical_device, &count, nullptr);

    if (!count) {
        return -1;
    }

    std::vector <VkQueueFamilyProperties> queue_families(count);

    vkGetPhysicalDeviceQueueFamilyProperties(iris->vk.physical_device, &count, queue_families.data());

    // Just return the first graphics-capable queue family, we should
    // actually be looking for dedicated compute/transfer queues
    for (uint32_t i = 0; i < count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }

    return -1;
}

VkBuffer create_buffer(Instance* iris, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory& buffer_memory) {
    VkBuffer buffer = VK_NULL_HANDLE;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(iris->vk.device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to create buffer");

        return VK_NULL_HANDLE;
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(iris->vk.device, buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(iris->vk.physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((memory_requirements.memoryTypeBits & (1 << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            alloc_info.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(iris->vk.device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to allocate buffer memory");

        vkDestroyBuffer(iris->vk.device, buffer, nullptr);

        return VK_NULL_HANDLE;
    }

    vkBindBufferMemory(iris->vk.device, buffer, buffer_memory, 0);

    return buffer;
}

void load_buffer(Instance* iris, VkDeviceMemory buffer_memory, void* data, VkDeviceSize size) {
    void* ptr;

    vkMapMemory(iris->vk.device, buffer_memory, 0, size, 0, &ptr);
    memcpy(ptr, data, (size_t)size);
    vkUnmapMemory(iris->vk.device, buffer_memory);
}

bool copy_buffer(Instance* iris, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandPool command_pool = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = iris->vk.queue_family;

    if (vkCreateCommandPool(iris->vk.device, &info, VK_NULL_HANDLE, &command_pool) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to create command pool");
    
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(iris->vk.device, &alloc_info, &command_buffer);

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

    vkQueueSubmit(iris->vk.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(iris->vk.queue);

    vkFreeCommandBuffers(iris->vk.device, command_pool, 1, &command_buffer);
    vkDestroyCommandPool(iris->vk.device, command_pool, VK_NULL_HANDLE);

    return true;
}

bool create_descriptor_pool(Instance* iris) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 0;

    for (VkDescriptorPoolSize& pool_size : pool_sizes)
        pool_info.maxSets += pool_size.descriptorCount;

    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(iris->vk.device, &pool_info, VK_NULL_HANDLE, &iris->vk.descriptor_pool) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to create descriptor pool");

        return false;
    }

    return true;
}

Texture upload_texture(Instance* iris, void* pixels, int width, int height, int stride) {
    Texture tex = {};

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
        
        if (vkCreateImage(iris->vk.device, &info, VK_NULL_HANDLE, &tex.image) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to create image");

            return {};
        }

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(iris->vk.device, tex.image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(iris->vk.physical_device, &memory_properties);

        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
            if ((req.memoryTypeBits & (1 << i)) &&
                (memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                alloc_info.memoryTypeIndex = i;
                break;
            }
        }
        if (vkAllocateMemory(iris->vk.device, &alloc_info, VK_NULL_HANDLE, &tex.image_memory) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to allocate image memory");

            return {};
        }

        if (vkBindImageMemory(iris->vk.device, tex.image, tex.image_memory, 0) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to bind image memory");

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
        if (vkCreateImageView(iris->vk.device, &info, VK_NULL_HANDLE, &tex.image_view) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to create image view");

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
        if (vkCreateSampler(iris->vk.device, &sampler_info, VK_NULL_HANDLE, &tex.sampler) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to create sampler");

            return {};
        }
    }

    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = iris->vk.descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &iris->vk.descriptor_set_layout;
        if (vkAllocateDescriptorSets(iris->vk.device, &alloc_info, &tex.descriptor_set) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to allocate descriptor sets");

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
        vkUpdateDescriptorSets(iris->vk.device, 1, write_desc, 0, nullptr);
    }

    VkCommandPool command_pool = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = iris->vk.queue_family;

    if (vkCreateCommandPool(iris->vk.device, &info, VK_NULL_HANDLE, &command_pool) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to create command pool");
    
        return {};
    }

    VkCommandBuffer command_buffer;

    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(iris->vk.device, &alloc_info, &command_buffer) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to allocate command buffers");

            return {};
        }

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to begin command buffer");

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
            iris_error(&iris->log.vulkan, "Failed to end command buffer");

            return {};
        }


        if (vkQueueSubmit(iris->vk.queue, 1, &end_info, VK_NULL_HANDLE) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to submit queue");

            return {};
        } 

        
        if (vkDeviceWaitIdle(iris->vk.device) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to wait device idle");

            return {};
        } 
    }

    vkDestroyCommandPool(iris->vk.device, command_pool, nullptr);
    vkDestroyBuffer(iris->vk.device, staging_buffer, nullptr);
    vkFreeMemory(iris->vk.device, staging_buffer_memory, nullptr);

    return tex;
}

void free_texture(Instance* iris, Texture& tex) {
    if (!iris->vk.device)
        return;

    // Textures are now released while the app is running, not only at shutdown,
    // so any frame still referencing this descriptor set has to retire first
    vkDeviceWaitIdle(iris->vk.device);

    if (tex.descriptor_set) vkFreeDescriptorSets(iris->vk.device, iris->vk.descriptor_pool, 1, &tex.descriptor_set);
    if (tex.sampler) vkDestroySampler(iris->vk.device, tex.sampler, nullptr);
    if (tex.image_view) vkDestroyImageView(iris->vk.device, tex.image_view, nullptr);
    if (tex.image) vkDestroyImage(iris->vk.device, tex.image, nullptr);
    if (tex.image_memory) vkFreeMemory(iris->vk.device, tex.image_memory, nullptr);

    tex = Texture();
}

bool init(Instance* iris, bool enable_validation) {
    if (volkInitialize() != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to initialize volk loader");

        return false;
    }

    iris->vk.instance_extensions = get_instance_extensions(iris);
    iris->vk.instance_layers = get_instance_layers(iris);

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
#if defined(__APPLE__) && defined(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    instance_create_info instance_info = {};
    instance_info.enabled_extensions = extensions;
    instance_info.enabled_layers = layers;
    instance_info.flags = flags;

    iris->vk.instance = create_instance(iris, instance_info);

    if (!iris->vk.instance) {
        iris_error(&iris->log.vulkan, "Failed to create Vulkan instance");

        return false;
    }

    volkLoadInstance(iris->vk.instance);

    // Find a suitable Vulkan physical device (GPU)
    enumerate_physical_devices(iris);

    iris->vk.vulkan_selected_device_index = 0;

    if (iris->vk.vulkan_physical_device < 0) {
        iris->vk.physical_device = find_suitable_physical_device(iris);
    } else {
        if (iris->vk.vulkan_physical_device >= iris->vk.vulkan_gpus.size()) {
            iris->vk.physical_device = find_suitable_physical_device(iris);
            iris->vk.vulkan_physical_device = iris->vk.vulkan_selected_device_index;
        } else {
            iris->vk.physical_device = iris->vk.vulkan_gpus[iris->vk.vulkan_physical_device].device;
            iris->vk.vulkan_selected_device_index = iris->vk.vulkan_physical_device;
        }
    }

    if (!iris->vk.physical_device) {
        iris_error(&iris->log.vulkan, "Failed to find a suitable Vulkan device");

        return false;
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(iris->vk.physical_device, &properties);

    iris_info(&iris->log.vulkan, "Using Vulkan device \"{}\". API version {}.{}.{}.{} Driver version {:x}",
        properties.deviceName,
        VK_API_VERSION_MAJOR(properties.apiVersion),
        VK_API_VERSION_MINOR(properties.apiVersion),
        VK_API_VERSION_PATCH(properties.apiVersion),
        VK_API_VERSION_VARIANT(properties.apiVersion),
        properties.driverVersion
    );

    iris->vk.device_extensions = get_device_extensions(iris);
    iris->vk.device_layers = get_device_layers(iris);

    // Find a graphics-capable queue family
    int queue_family = find_graphics_queue_family_index(iris);

    if (queue_family == -1) {
        iris_error(&iris->log.vulkan, "Failed to find a graphics-capable queue family");

        return false;
    }

    iris->vk.queue_family = queue_family;

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

    iris->vk.vulkan_11_features.pNext = &iris->vk.vulkan_12_features;
    iris->vk.vulkan_12_features.pNext = &iris->vk.subgroup_size_control_features;
    iris->vk.subgroup_size_control_features.pNext = &iris->vk.synchronization2_features;
    iris->vk.synchronization2_features.pNext = VK_NULL_HANDLE;

    iris->vk.vulkan_11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    iris->vk.vulkan_12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    iris->vk.subgroup_size_control_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES;
    iris->vk.synchronization2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;

    // Parallel-GS uses synchronization2
    iris->vk.synchronization2_features.synchronization2 = VK_TRUE;

    iris->vk.vulkan_11_features.storageBuffer16BitAccess = VK_TRUE;
    iris->vk.vulkan_11_features.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    iris->vk.vulkan_12_features.descriptorIndexing = VK_TRUE;
    iris->vk.vulkan_12_features.descriptorBindingPartiallyBound = VK_TRUE;
    iris->vk.vulkan_12_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    iris->vk.vulkan_12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    iris->vk.vulkan_12_features.runtimeDescriptorArray = VK_TRUE;
    iris->vk.vulkan_12_features.timelineSemaphore = VK_TRUE;
    iris->vk.vulkan_12_features.bufferDeviceAddress = VK_TRUE;
    iris->vk.vulkan_12_features.scalarBlockLayout = VK_TRUE;
    iris->vk.vulkan_12_features.storageBuffer8BitAccess = VK_TRUE;
    iris->vk.vulkan_12_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    
    iris->vk.subgroup_size_control_features.subgroupSizeControl = VK_TRUE;
    iris->vk.subgroup_size_control_features.computeFullSubgroups = VK_TRUE;

    if (is_device_extension_supported(iris, VK_EXT_DEVICE_FAULT_EXTENSION_NAME)) {
        device_info.enabled_extensions.push_back(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);

        iris->vk.fault_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;
        iris->vk.fault_features.deviceFault = VK_TRUE;
        iris->vk.fault_features.pNext = VK_NULL_HANDLE;

        iris->vk.synchronization2_features.pNext = &iris->vk.fault_features;
        iris->vk.device_fault_supported = true;

        iris_info(&iris->log.vulkan, "VK_EXT_device_fault enabled (GPU fault reporting available)");
    }

    // Chain in all feature structs
    device_info.data = &iris->vk.vulkan_11_features;

    iris->vk.device = create_device(iris, device_info);

    if (!iris->vk.device) {
        iris_error(&iris->log.vulkan, "Failed to create Vulkan device");

        return false;
    }

    vkGetDeviceQueue(iris->vk.device, iris->vk.queue_family, 0, &iris->vk.queue);

    iris->vk.indices = { 0, 1, 2, 2, 3, 0 };

    iris->vk.vertex_buffer_size = sizeof(Vertex) * iris->vk.vertices.size();

    // Create vertex and index buffers
    // Create and populate index buffer immediately by creating
    // a staging buffer, filling it, and then copying it over to
    // the device local index buffer.
    // The vertex buffer will be updated dynamically each frame.
    VkDeviceMemory index_staging_buffer_memory;
    VkDeviceSize index_buffer_size = sizeof(uint16_t) * iris->vk.indices.size();

    iris->vk.index_buffer = create_buffer(
        iris,
        sizeof(uint16_t) * iris->vk.indices.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        iris->vk.index_buffer_memory
    );

    VkBuffer index_staging_buffer = create_buffer(
        iris,
        index_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        index_staging_buffer_memory
    );

    load_buffer(iris, index_staging_buffer_memory, iris->vk.indices.data(), index_buffer_size);
    copy_buffer(iris, index_staging_buffer, iris->vk.index_buffer, index_buffer_size);

    iris->vk.vertex_buffer = create_buffer(
        iris,
        iris->vk.vertex_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        iris->vk.vertex_buffer_memory
    );

    iris->vk.vertex_staging_buffer = create_buffer(
        iris,
        iris->vk.vertex_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        iris->vk.vertex_staging_buffer_memory
    );

    // We don't need the staging buffer anymore
    vkFreeMemory(iris->vk.device, index_staging_buffer_memory, nullptr);
    vkDestroyBuffer(iris->vk.device, index_staging_buffer, nullptr);

    create_descriptor_pool(iris);

    return true;
}

void cleanup(Instance* iris) {
    vulkan::wait_idle(iris);

    if (iris->vk.descriptor_set_layout) vkDestroyDescriptorSetLayout(iris->vk.device, iris->vk.descriptor_set_layout, nullptr);
    if (iris->vk.descriptor_pool) vkDestroyDescriptorPool(iris->vk.device, iris->vk.descriptor_pool, nullptr);

    for (int i = 0; i < 3; i++)
        if (iris->vk.sampler[i]) vkDestroySampler(iris->vk.device, iris->vk.sampler[i], nullptr);

    if (iris->vk.vertex_buffer) vkDestroyBuffer(iris->vk.device, iris->vk.vertex_buffer, nullptr);
    if (iris->vk.vertex_staging_buffer) vkDestroyBuffer(iris->vk.device, iris->vk.vertex_staging_buffer, nullptr);
    if (iris->vk.index_buffer) vkDestroyBuffer(iris->vk.device, iris->vk.index_buffer, nullptr);
    if (iris->vk.vertex_staging_buffer_memory) vkFreeMemory(iris->vk.device, iris->vk.vertex_staging_buffer_memory, nullptr);
    if (iris->vk.vertex_buffer_memory) vkFreeMemory(iris->vk.device, iris->vk.vertex_buffer_memory, nullptr);
    if (iris->vk.index_buffer_memory) vkFreeMemory(iris->vk.device, iris->vk.index_buffer_memory, nullptr);
    if (iris->vk.pipeline) vkDestroyPipeline(iris->vk.device, iris->vk.pipeline, nullptr);
    // ImGui takes care of this apparently (probably shouldn't)
    // if (iris->vk.surface) vkDestroySurfaceKHR(iris->vk.instance, iris->vk.surface, nullptr);
    if (iris->vk.render_pass) vkDestroyRenderPass(iris->vk.device, iris->vk.render_pass, nullptr);
    if (iris->vk.pipeline_layout) vkDestroyPipelineLayout(iris->vk.device, iris->vk.pipeline_layout, nullptr);
    if (iris->vk.device) vkDestroyDevice(iris->vk.device, nullptr);
    if (iris->vk.instance) vkDestroyInstance(iris->vk.instance, nullptr);
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

void* read_image(Instance* iris, VkImage src_image, VkFormat format, int width, int height) {
    if (src_image == VK_NULL_HANDLE) {
        iris_error(&iris->log.vulkan, "Source image is null");

        return nullptr;
    }

    if (!width || !height) {
        iris_error(&iris->log.vulkan, "Invalid image dimensions for readback ({}x{})", width, height);

        return nullptr;
    }

    bool supports_blit = true;

    // Check blit support for source and destination
    VkFormatProperties format_props;

    // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
    vkGetPhysicalDeviceFormatProperties(iris->vk.physical_device, format, &format_props);

    if (!(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        iris_warning(&iris->log.vulkan, "Device does not support blitting from optimal tiled images, using copy instead of blit!");

        supports_blit = false;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(iris->vk.physical_device, VK_FORMAT_R8G8B8A8_UNORM, &format_props);

    if (!(format_props.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        iris_warning(&iris->log.vulkan, "Device does not support blitting to linear tiled images, using copy instead of blit!");

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

    if (vkCreateImage(iris->vk.device, &image_create_info, nullptr, &dst_image) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to create image for readback");

        return nullptr;
    }

    VkDeviceMemory dst_image_memory;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(iris->vk.device, dst_image, &req);
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = find_memory_type(iris, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(iris->vk.device, &alloc_info, VK_NULL_HANDLE, &dst_image_memory) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to allocate image memory for readback");

        return {};
    }

    if (vkBindImageMemory(iris->vk.device, dst_image, dst_image_memory, 0) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to bind image memory for readback");

        return {};
    }

    // Do the actual blit from the swapchain image to our host visible destination image
    VkCommandPool command_pool = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = iris->vk.queue_family;

    if (vkCreateCommandPool(iris->vk.device, &info, VK_NULL_HANDLE, &command_pool) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to create command pool for readback");
    
        return {};
    }

    VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandPool = command_pool;
    cmd_buffer_alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(iris->vk.device, &cmd_buffer_alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "Failed to begin command buffer for readback");

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
            iris_error(&iris->log.vulkan, "Failed to end command buffer");

            return {};
        }


        if (vkQueueSubmit(iris->vk.queue, 1, &end_info, VK_NULL_HANDLE) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to submit queue");

            return {};
        } 

        
        if (vkDeviceWaitIdle(iris->vk.device) != VK_SUCCESS) {
            iris_error(&iris->log.vulkan, "Failed to wait device idle");

            return {};
        } 
    }

    vkDestroyCommandPool(iris->vk.device, command_pool, nullptr);

    // Get layout of the image (including row pitch)
    VkImageSubresource subresource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subresource_layout;
    vkGetImageSubresourceLayout(iris->vk.device, dst_image, &subresource, &subresource_layout);

    // Map image memory so we can start copying from it
    const char* data;
    vkMapMemory(iris->vk.device, dst_image_memory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
    data += subresource_layout.offset;

    void* buf = malloc(width * height * 4);

    memcpy(buf, data, width * height * 4);

    // Clean up resources
    vkUnmapMemory(iris->vk.device, dst_image_memory);
    vkFreeMemory(iris->vk.device, dst_image_memory, nullptr);
    vkDestroyImage(iris->vk.device, dst_image, nullptr);

    return buf;
}

void wait_idle(Instance* iris) {
    if (iris->vk.device) {
        vkDeviceWaitIdle(iris->vk.device);
    } else if (iris->vk.queue) {
        vkQueueWaitIdle(iris->vk.queue);
    }
}

// Dump GPU fault info after a VK_ERROR_DEVICE_LOST. Requires VK_EXT_device_fault
// (enabled in init() when supported). Safe to call on an already-lost device.
void dump_device_fault(Instance* iris) {
    // Only report once - after a loss every frame hits a device-lost path.
    if (iris->vk.device_fault_dumped)
        return;

    iris->vk.device_fault_dumped = true;

    if (!iris->vk.device_fault_supported) {
        iris_warning(&iris->log.vulkan, "GPU device lost, but VK_EXT_device_fault is not enabled - no fault details available");

        return;
    }

    if (!iris->vk.device)
        return;

    auto get_fault_info = (PFN_vkGetDeviceFaultInfoEXT)vkGetDeviceProcAddr(iris->vk.device, "vkGetDeviceFaultInfoEXT");

    if (!get_fault_info) {
        iris_error(&iris->log.vulkan, "vkGetDeviceFaultInfoEXT not loaded");

        return;
    }

    VkDeviceFaultCountsEXT counts = {};
    counts.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT;

    VkResult res = get_fault_info(iris->vk.device, &counts, nullptr);

    if (res != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "vkGetDeviceFaultInfoEXT (counts) failed: {}", (int)res);

        return;
    }

    std::vector <VkDeviceFaultAddressInfoEXT> address_infos(counts.addressInfoCount);
    std::vector <VkDeviceFaultVendorInfoEXT> vendor_infos(counts.vendorInfoCount);

    VkDeviceFaultInfoEXT fault = {};
    fault.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT;
    fault.pAddressInfos = address_infos.data();
    fault.pVendorInfos = vendor_infos.data();

    res = get_fault_info(iris->vk.device, &counts, &fault);

    if (res != VK_SUCCESS) {
        iris_error(&iris->log.vulkan, "vkGetDeviceFaultInfoEXT (data) failed: {}", (int)res);

        return;
    }

    iris_error(&iris->log.vulkan, "==== GPU DEVICE FAULT ==== ({} address, {} vendor)", counts.addressInfoCount, counts.vendorInfoCount);
    iris_error(&iris->log.vulkan, "{}", fault.description);

    for (uint32_t i = 0; i < counts.addressInfoCount; i++) {
        const VkDeviceFaultAddressInfoEXT& a = address_infos[i];

        iris_error(&iris->log.vulkan, "Address fault: type={} reported=0x{:x} precision=0x{:x}",
            (int)a.addressType,
            (unsigned long long)a.reportedAddress,
            (unsigned long long)a.addressPrecision
        );
    }

    for (uint32_t i = 0; i < counts.vendorInfoCount; i++) {
        const VkDeviceFaultVendorInfoEXT& v = vendor_infos[i];

        iris_error(&iris->log.vulkan, "Vendor fault: \"{}\" code=0x{:x} data=0x{:x}",
            v.description,
            (unsigned long long)v.vendorFaultCode,
            (unsigned long long)v.vendorFaultData
        );
    }
}

Texture load_texture_from_memory(Instance* iris, const void* data, size_t size) {
    int x, y, c;

    stbi_uc* buf = stbi_load_from_memory((const stbi_uc*)data, size, &x, &y, &c, 4);

    auto tex = vulkan::upload_texture(iris, buf, x, y, c);

    stbi_image_free(buf);

    return tex;
}

Texture load_texture_from_file(Instance* iris, std::string path) {
    int x, y, c;

    stbi_uc* buf = stbi_load(path.c_str(), &x, &y, &c, 4);

    auto tex = vulkan::upload_texture(iris, buf, x, y, c);

    stbi_image_free(buf);

    return tex;
}

}