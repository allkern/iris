#include <cmath>
#include <vector>

#include "iris.hpp"

#include "gs/gs_dump.hpp"
#include "ps2.hpp"

#define RENDER_MAX_SHADER_PASSES 16

constexpr unsigned char g_default_vertex_shader_data[] = {
#embed "../shaders/shader.spv"
};
constexpr unsigned int g_default_vertex_shader_size = sizeof(g_default_vertex_shader_data);

namespace iris::render {

static int frame = 0;
static constexpr uint32_t DESCRIPTOR_SET_RING_SIZE = 8;

bool create_image(Instance* iris, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage& image, VkImageView& view, VkDeviceMemory& memory) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(iris->vk.device, &image_info, nullptr, &image) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(iris->vk.device, image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(iris->vk.physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((memory_requirements.memoryTypeBits & (1 << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            alloc_info.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(iris->vk.device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        iris_error(&iris->log.render, "Failed to allocate image memory");

        vkDestroyImage(iris->vk.device, image, nullptr);

        return false;
    }

    vkBindImageMemory(iris->vk.device, image, memory, 0);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = format;
    image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(iris->vk.device, &image_view_info, nullptr, &view) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool rebuild_framebuffers(Instance* iris) {
    if (!shaders::count(iris))
        return true;

    vulkan::wait_idle(iris);

    for (auto& pass_framebuffers : iris->vk.shader_pass_framebuffers) {
        for (VkFramebuffer& framebuffer : pass_framebuffers) {
            if (framebuffer) {
                vkDestroyFramebuffer(iris->vk.device, framebuffer, nullptr);
                framebuffer = VK_NULL_HANDLE;
            }
        }
    }

    iris->vk.shader_pass_framebuffers.clear();

    for (auto& fb : iris->vk.shader_framebuffers) {
        if (fb.view) vkDestroyImageView(iris->vk.device, fb.view, nullptr);
        if (fb.image) vkDestroyImage(iris->vk.device, fb.image, nullptr);
        if (fb.memory) vkFreeMemory(iris->vk.device, fb.memory, nullptr);

        bool res = create_image(iris,
            iris->vk.image.width,
            iris->vk.image.height,
            iris->vk.image.format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            fb.image,
            fb.view,
            fb.memory
        );

        if (!res) {
            iris_error(&iris->log.render, "Failed to create shader pass framebuffer image");

            return false;
        }
    }

    const size_t pass_count = shaders::count(iris);

    iris->vk.shader_pass_framebuffers.resize(pass_count);

    for (size_t pass_index = 0; pass_index < pass_count; pass_index++) {
        auto* pass = shaders::at(iris, (int)pass_index);

        if (!pass || !pass->ready()) {
            iris->vk.shader_pass_framebuffers[pass_index] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

            continue;
        }

        for (int framebuffer_index = 0; framebuffer_index < 2; framebuffer_index++) {
            VkFramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = pass->get_render_pass();
            framebuffer_info.attachmentCount = 1;
            framebuffer_info.pAttachments = &iris->vk.shader_framebuffers[framebuffer_index].view;
            framebuffer_info.width = iris->vk.image.width;
            framebuffer_info.height = iris->vk.image.height;
            framebuffer_info.layers = 1;

            if (vkCreateFramebuffer(iris->vk.device, &framebuffer_info, nullptr, &iris->vk.shader_pass_framebuffers[pass_index][framebuffer_index]) != VK_SUCCESS) {
                iris_error(&iris->log.render, "Failed to create shader pass framebuffer");

                return false;
            }
        }
    }

    return true;
}

bool init(Instance* iris) {
    // Initialize our renderer
    iris->renderer = gs::renderer::create();

    gs::renderer::CreateInfo info = {};

    info.backend = iris->renderer_backend;
    info.gif = iris->ps2->gif;
    info.gs = iris->ps2->gs;
    info.instance = iris->vk.instance;
    info.device = iris->vk.device;
    info.physical_device = iris->vk.physical_device;
    info.instance_create_info = iris->vk.instance_create_info;
    info.device_create_info = iris->vk.device_create_info;

    switch (info.backend) {
        case gs::renderer::BACKEND_HARDWARE: {
            info.config = &iris->hardware_backend_config;
        } break;
    }

    if (!gs::renderer::init(iris->renderer, info)) {
        iris_error(&iris->log.render, "Failed to initialize renderer backend");

        return false;
    }

    VkSamplerCreateInfo nearest_sampler_info = {};
    nearest_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    nearest_sampler_info.magFilter = VK_FILTER_NEAREST;
    nearest_sampler_info.minFilter = VK_FILTER_NEAREST;
    nearest_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    nearest_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    nearest_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    nearest_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    nearest_sampler_info.minLod = -1000;
    nearest_sampler_info.maxLod = 1000;
    nearest_sampler_info.maxAnisotropy = 1.0f;

    if (vkCreateSampler(iris->vk.device, &nearest_sampler_info, VK_NULL_HANDLE, &iris->vk.sampler[0]) != VK_SUCCESS) {
        iris_error(&iris->log.render, "Failed to create nearest texture sampler");

        return false;
    }

    VkSamplerCreateInfo bilinear_sampler_info = {};
    bilinear_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    bilinear_sampler_info.magFilter = VK_FILTER_LINEAR;
    bilinear_sampler_info.minFilter = VK_FILTER_LINEAR;
    bilinear_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    bilinear_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    bilinear_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    bilinear_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    bilinear_sampler_info.minLod = -1000;
    bilinear_sampler_info.maxLod = 1000;
    bilinear_sampler_info.maxAnisotropy = 1.0f;

    if (vkCreateSampler(iris->vk.device, &bilinear_sampler_info, VK_NULL_HANDLE, &iris->vk.sampler[1]) != VK_SUCCESS) {
        iris_error(&iris->log.render, "Failed to create bilinear texture sampler");

        return false;
    }

    if (iris->vk.cubic_supported) {
        VkSamplerCreateInfo cubic_sampler_info = {};
        cubic_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        cubic_sampler_info.magFilter = VK_FILTER_LINEAR;
        cubic_sampler_info.minFilter = VK_FILTER_LINEAR;
        cubic_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        cubic_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        cubic_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        cubic_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        cubic_sampler_info.minLod = -1000;
        cubic_sampler_info.maxLod = 1000;
        cubic_sampler_info.maxAnisotropy = 1.0f;

        if (vkCreateSampler(iris->vk.device, &cubic_sampler_info, VK_NULL_HANDLE, &iris->vk.sampler[2]) != VK_SUCCESS) {
            iris_error(&iris->log.render, "Failed to create cubic texture sampler");

            return false;
        }
    }

    VkShaderModuleCreateInfo vert_shader_create_info = {};
    vert_shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_shader_create_info.pCode = (const uint32_t*)g_default_vertex_shader_data;
    vert_shader_create_info.codeSize = g_default_vertex_shader_size;

    if (vkCreateShaderModule(iris->vk.device, &vert_shader_create_info, nullptr, &iris->vk.default_vert_shader) != VK_SUCCESS) {
        iris_error(&iris->log.render, "Failed to create default vertex shader module");

        return false;
    }

    // Create descriptor set
    VkDescriptorSetLayoutBinding sampler_layout_binding = {};
    sampler_layout_binding.binding = 0;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.pImmutableSamplers = nullptr;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &sampler_layout_binding;

    if (vkCreateDescriptorSetLayout(iris->vk.device, &layout_info, nullptr, &iris->vk.shader_descriptor_set_layout) != VK_SUCCESS) {
        iris_error(&iris->log.render, "Failed to create descriptor set layout");

        return false;
    }

    const uint32_t shader_descriptor_set_count = DESCRIPTOR_SET_RING_SIZE * RENDER_MAX_SHADER_PASSES;

    std::vector <VkDescriptorSetLayout> shader_layouts(shader_descriptor_set_count, iris->vk.shader_descriptor_set_layout);

    iris->vk.shader_descriptor_sets.resize(shader_descriptor_set_count, VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = iris->vk.descriptor_pool;
    alloc_info.descriptorSetCount = shader_descriptor_set_count;
    alloc_info.pSetLayouts = shader_layouts.data();

    if (vkAllocateDescriptorSets(iris->vk.device, &alloc_info, iris->vk.shader_descriptor_sets.data()) != VK_SUCCESS) {
        iris_error(&iris->log.render, "Failed to allocate descriptor sets");

        return false;
    }

    iris->vk.shader_descriptor_set = iris->vk.shader_descriptor_sets[0];

    return true;
}

static inline VkDescriptorSet get_frame_descriptor_set(Instance* iris) {
    if (!iris->vk.descriptor_sets.size()) {
        return iris->vk.descriptor_set;
    }

    const uint32_t frame_index = iris->vk.main_window_data.FrameIndex;

    return iris->vk.descriptor_sets[frame_index % iris->vk.descriptor_sets.size()];
}

static inline VkDescriptorSet get_frame_shader_descriptor_set(Instance* iris, uint32_t pass_index) {
    if (!iris->vk.shader_descriptor_sets.size()) {
        return iris->vk.shader_descriptor_set;
    }

    const uint32_t frame_index = iris->vk.main_window_data.FrameIndex % DESCRIPTOR_SET_RING_SIZE;
    const uint32_t slot = (frame_index * RENDER_MAX_SHADER_PASSES) + (pass_index % RENDER_MAX_SHADER_PASSES);

    return iris->vk.shader_descriptor_sets[slot % iris->vk.shader_descriptor_sets.size()];
}

static inline void update_vertex_buffer(Instance* iris, VkCommandBuffer command_buffer) {
    SDL_Rect size, rect, display;

    const int normalized_angle = ((iris->angle % 360) + 360) % 360;
    const bool swap_axes = normalized_angle == 90 || normalized_angle == 270;

    float aspect_ratio = 4.0 / 3.0;

    if (swap_axes) {
        aspect_ratio = 3.0 / 4.0;
    }

    float aspect_ratio_inv = 1.0f / aspect_ratio;

    SDL_GetWindowSize(iris->window, &size.w, &size.h);

    display.w = size.w;
    display.h = size.h;
    display.x = 0;
    display.y = 0;
    
    int inset = get_menubar_inset(iris);

    display.h -= inset;
    display.y += inset;

    if (!iris->fullscreen && !iris->no_decorations && iris->ui.show_status_bar) {
        display.h -= iris->ui.menubar_height;
    }

    rect.w = iris->vk.image.width;
    rect.h = iris->vk.image.height;

    float scale = iris->integer_scaling ? floorf(iris->scale) : iris->scale;

    switch (iris->aspect_mode) {
        case render::NATIVE: {
            rect.w *= scale;
            rect.h *= scale;
        } break;

        case render::FORCE_4_3: {
            rect.w *= scale;
            rect.h = (float)rect.w * (3.0f / 4.0f);
        } break;

        case render::FORCE_16_9: {
            rect.w *= scale;
            rect.h = (float)rect.w * (9.0f / 16.0f);
        } break;

        case render::FORCE_5_4: {
            rect.w *= scale;
            rect.h = (float)rect.w * (4.0f / 5.0f);
        } break;

        case render::STRETCH: {
            rect.w = display.w;
            rect.h = display.h;
        } break;

        case render::AUTO:
        case render::STRETCH_KEEP: {
            if (swap_axes) {
                std::swap(rect.w, rect.h);
            }

            rect.h = display.h;
            rect.w = (float)rect.h * aspect_ratio;

            // Scale vertically if the rect ends up being bigger
            // than our display area
            if (rect.w > display.w) {
                rect.w = display.w;
                rect.h = (float)rect.w * aspect_ratio_inv;
            }
        } break;
    }

    if (iris->aspect_mode != render::AUTO && iris->aspect_mode != render::STRETCH_KEEP) {
        if (swap_axes) {
            std::swap(rect.w, rect.h);
        }
    }

    iris->render_width = rect.w;
    iris->render_height = rect.h;

    rect.x = display.x + ((display.w / 2) - (rect.w / 2));
    rect.y = display.y + ((display.h / 2) - (rect.h / 2));

    iris->render_x = rect.x;
    iris->render_y = rect.y;

    float x0 = (rect.x / ((float)size.w / 2.0f)) - 1.0f;
    float y0 = (rect.y / ((float)size.h / 2.0f)) - 1.0f;
    float x1 = ((rect.x + rect.w) / ((float)size.w / 2.0f)) - 1.0f;
    float y1 = ((rect.y + rect.h) / ((float)size.h / 2.0f)) - 1.0f;

    float uvs[4][2] = {
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 0.0f}
    };

    for (int i = 0; i < 4; i++) {
        float u = uvs[i][0];
        float v = uvs[i][1];

        if (iris->flip_x) {
            u = 1.0f - u;
        }

        if (iris->flip_y) {
            v = 1.0f - v;
        }

        switch (normalized_angle) {
            case 90: {
                // Rotate clockwise in UV space.
                const float prev_u = u;
                u = v;
                v = 1.0f - prev_u;
            } break;

            case 180: {
                u = 1.0f - u;
                v = 1.0f - v;
            } break;

            case 270: {
                // Rotate counter-clockwise in UV space.
                const float prev_u = u;
                u = 1.0f - v;
                v = prev_u;
            } break;
        }

        uvs[i][0] = u;
        uvs[i][1] = v;
    }

    iris->vk.vertices[0] = Vertex{ { x0, y0 }, {uvs[0][0], uvs[0][1]} };
    iris->vk.vertices[1] = Vertex{ { x1, y0 }, {uvs[1][0], uvs[1][1]} };
    iris->vk.vertices[2] = Vertex{ { x1, y1 }, {uvs[2][0], uvs[2][1]} };
    iris->vk.vertices[3] = Vertex{ { x0, y1 }, {uvs[3][0], uvs[3][1]} };

    void* ptr;

    vkMapMemory(iris->vk.device, iris->vk.vertex_staging_buffer_memory, 0, iris->vk.vertex_buffer_size, 0, &ptr);
    memcpy(ptr, iris->vk.vertices.data(), (size_t)iris->vk.vertex_buffer_size);
    vkUnmapMemory(iris->vk.device, iris->vk.vertex_staging_buffer_memory);

    static VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = iris->vk.vertex_buffer_size;

    vkCmdCopyBuffer(
        command_buffer,
        iris->vk.vertex_staging_buffer,
        iris->vk.vertex_buffer,
        1,
        &region
    );
}

static inline void update_descriptor_set(Instance* iris, VkDescriptorSet set, VkImageView view, VkSampler sampler) {
    VkDescriptorImageInfo image_info = {};
    image_info.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    image_info.imageView = view;
    image_info.sampler = sampler;

    VkWriteDescriptorSet descriptor_write = {};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = set;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(iris->vk.device, 1, &descriptor_write, 0, nullptr);
}

void render_shader_passes(Instance* iris, VkCommandBuffer command_buffer, VkImageView& output_view, VkImage& output_image) {
    if (!shaders::count(iris))
        return;

    if (iris->vk.shader_pass_framebuffers.size() != shaders::count(iris)) {
        if (!rebuild_framebuffers(iris)) {
            iris_error(&iris->log.render, "Failed to rebuild shader framebuffers");

            return;
        }
    }

    int i = 0;

    for (size_t pass_index = 0; pass_index < shaders::vector(iris).size(); pass_index++) {
        auto& pass = shaders::vector(iris)[pass_index];

        if (pass->bypass || !pass->ready())
            continue;

        const int fb = i & 1;
        const VkImageView input_view = i == 0 ? iris->vk.image.view : iris->vk.shader_framebuffers[fb ^ 1].view;
        VkFramebuffer framebuffer = iris->vk.shader_pass_framebuffers[pass_index][fb];

        if (framebuffer == VK_NULL_HANDLE) {
            if (!rebuild_framebuffers(iris)) {
                iris_error(&iris->log.render, "Failed to rebuild shader framebuffers");

                return;
            }

            framebuffer = iris->vk.shader_pass_framebuffers[pass_index][fb];

            if (framebuffer == VK_NULL_HANDLE) {
                iris_error(&iris->log.render, "Shader framebuffer is null after rebuild");

                return;
            }
        }

        output_view = iris->vk.shader_framebuffers[fb].view;
        output_image = iris->vk.shader_framebuffers[fb].image;

        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = pass->get_render_pass();
        info.framebuffer = framebuffer;
        info.renderArea.extent.width = iris->vk.image.width;
        info.renderArea.extent.height = iris->vk.image.height;
        info.clearValueCount = 1;
        info.pClearValues = &iris->vk.clear_value;

        VkDescriptorImageInfo image_info = {};
        image_info.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        image_info.imageView = input_view;
        image_info.sampler = iris->vk.sampler[iris->filter];

        VkWriteDescriptorSet descriptor_write = {};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        const VkDescriptorSet shader_descriptor_set = get_frame_shader_descriptor_set(iris, i);
        descriptor_write.dstSet = shader_descriptor_set;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(iris->vk.device, 1, &descriptor_write, 0, nullptr);

        vkCmdBeginRenderPass(command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->get_pipeline());

        vkCmdBindIndexBuffer(command_buffer, iris->vk.index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->get_pipeline_layout(), 0, 1, &shader_descriptor_set, 0, nullptr);

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = iris->vk.image.width;
        viewport.height = iris->vk.image.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = { (uint32_t)iris->vk.image.width, (uint32_t)iris->vk.image.height };
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        PushConstants constants = {
            .resolution = { (float)iris->vk.image.width, (float)iris->vk.image.height },
            .frame = frame
        };

        vkCmdPushConstants(command_buffer, pass->get_pipeline_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &constants);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        i++;
    }
}

VkClearValue background(Instance* iris) {
    if (!iris->no_decorations)
        return iris->vk.clear_value;

    VkClearValue black = {};

    black.color.float32[3] = 1.0f;

    return black;
}

bool render_frame(Instance* iris, VkCommandBuffer command_buffer, VkFramebuffer framebuffer) {
    gs::renderer::Image image;

    if (iris->debug.pause) {
        image = iris->vk.image;
    } else {
        image = gs::renderer::get_frame(iris->renderer);
    }

    bool need_rebuild = image.width != iris->vk.image.width ||
                        image.height != iris->vk.image.height ||
                        image.format != iris->vk.image.format;

    iris->vk.image = image;

    if (need_rebuild && image.view != VK_NULL_HANDLE) {
        vulkan::wait_idle(iris);

        for (auto& pass : shaders::vector(iris)) {
            if (!pass->rebuild()) {
                iris_error(&iris->log.render, "Failed to rebuild shader pass");

                return false;
            }
        }

        if (!rebuild_framebuffers(iris)) {
            iris_error(&iris->log.render, "Failed to rebuild shader pass framebuffers");

            return false;
        }
    }

    // Process shader passes here
    iris->vk.output_image = iris->vk.image;

    if (iris->enable_shaders && iris->vk.output_image.view != VK_NULL_HANDLE) {
        render_shader_passes(iris, command_buffer, iris->vk.output_image.view, iris->vk.output_image.image);
    }

    VkClearValue clear = background(iris);

    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = iris->vk.main_window_data.RenderPass;
    info.framebuffer = framebuffer;
    info.renderArea.extent.width = iris->vk.main_window_data.Width;
    info.renderArea.extent.height = iris->vk.main_window_data.Height;
    info.clearValueCount = 1;
    info.pClearValues = &clear;

    if (iris->vk.output_image.view != VK_NULL_HANDLE) {
        const VkDescriptorSet descriptor_set = get_frame_descriptor_set(iris);

        update_vertex_buffer(iris, command_buffer);
        update_descriptor_set(iris, descriptor_set, iris->vk.output_image.view, iris->vk.sampler[iris->filter]);
    }

    vkCmdBeginRenderPass(command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);

    VkClearAttachment clear_attachment = {};
    clear_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachment.colorAttachment = 0;
    clear_attachment.clearValue = clear;

    VkClearRect clear_rect = {};
    clear_rect.rect.offset = { 0, 0 };
    clear_rect.rect.extent = { (uint32_t)iris->vk.main_window_data.Width, (uint32_t)iris->vk.main_window_data.Height };
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    vkCmdClearAttachments(command_buffer, 1, &clear_attachment, 1, &clear_rect);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, iris->vk.pipeline);

    if (iris->vk.output_image.view != VK_NULL_HANDLE) {
        VkDeviceSize offsets[] = { 0 };
        const VkDescriptorSet descriptor_set = get_frame_descriptor_set(iris);

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &iris->vk.vertex_buffer, offsets);
        vkCmdBindIndexBuffer(command_buffer, iris->vk.index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, iris->vk.pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    }

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = iris->vk.main_window_data.Width;
    viewport.height = iris->vk.main_window_data.Height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = { (uint32_t)iris->vk.main_window_data.Width, (uint32_t)iris->vk.main_window_data.Height };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    if (iris->vk.output_image.view != VK_NULL_HANDLE) {
        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);

    if (!iris->debug.pause)
        frame++;

    return true;
}

void switch_backend(Instance* iris, int backend) {
    if (iris->renderer_backend == backend)
        return;

    vulkan::wait_idle(iris);

    gs::renderer::destroy(iris->renderer);

    iris->renderer = gs::renderer::create();

    gs::renderer::CreateInfo info = {};

    info.backend = backend;
    info.gif = iris->ps2->gif;
    info.gs = iris->ps2->gs;
    info.instance = iris->vk.instance;
    info.device = iris->vk.device;
    info.physical_device = iris->vk.physical_device;
    info.instance_create_info = iris->vk.instance_create_info;
    info.device_create_info = iris->vk.device_create_info;

    switch (info.backend) {
        case gs::renderer::BACKEND_HARDWARE: {
            info.config = &iris->hardware_backend_config;
        } break;
    }

    if (!gs::renderer::init(iris->renderer, info)) {
        iris_error(&iris->log.render, "Failed to initialize renderer backend");
    } else {
        iris->renderer_backend = backend;
    }
}

static void gsdump_tap(void* udata, int path, const void* data, size_t size) {
    gs::dump::transfer((gs::dump::Dump*)udata, path, data, size);
}

void gs_dump_start(Instance* iris, std::string path, int frames, int delay, std::string serial) {
    if (!iris->debug.gsdump)
        iris->debug.gsdump = gs::dump::create();

    iris->debug.gsdump_path = path;
    iris->debug.gsdump_serial = serial;
    iris->debug.gsdump_delay_remaining = delay < 0 ? 0 : delay;
    iris->debug.gsdump_frames_remaining = frames < 1 ? 1 : frames;
    iris->debug.gsdump_armed = true;
}

void gs_dump_tick(Instance* iris) {
    if (!iris->debug.gsdump)
        return;

    if (iris->debug.gsdump_armed) {
        if (iris->debug.gsdump_delay_remaining > 0) {
            iris->debug.gsdump_delay_remaining--;

            return;
        }

        iris->debug.gsdump_armed = false;

        std::vector<uint8_t> vram(0x400000);

        gs::renderer::read_vram(iris->renderer, vram.data(), vram.size());

        gif::set_dump_tap(iris->ps2->gif, iris->debug.gsdump, gsdump_tap);

        const char* serial = iris->debug.gsdump_serial.empty() ? nullptr : iris->debug.gsdump_serial.c_str();

        if (!gs::dump::begin(iris->debug.gsdump, iris->debug.gsdump_path.c_str(),
                iris->ps2->gs, iris->ps2->gif,
                vram.data(), (uint32_t)vram.size(), serial, 0)) {
            gif::set_dump_tap(iris->ps2->gif, nullptr, nullptr);
            iris->debug.gsdump_frames_remaining = 0;

            iris_error(&iris->log.render, "Failed to open GS dump '{}'", iris->debug.gsdump_path.c_str());
        }

        return;
    }

    if (gs::dump::is_active(iris->debug.gsdump)) {
        gs::dump::vsync(iris->debug.gsdump, iris->ps2->gs);

        if (--iris->debug.gsdump_frames_remaining <= 0) {
            gs::dump::end(iris->debug.gsdump);

            gif::set_dump_tap(iris->ps2->gif, nullptr, nullptr);
        }
    }
}

void refresh(Instance* iris) {
    switch (iris->renderer_backend) {
        case gs::renderer::BACKEND_HARDWARE: {
            gs::renderer::set_config(iris->renderer, &iris->hardware_backend_config);
        } break;
    }

    iris->vk.image = gs::renderer::get_frame(iris->renderer);

    if (iris->vk.image.view == VK_NULL_HANDLE)
        return;

    if (shaders::count(iris) == 0)
        return;

    vulkan::wait_idle(iris);

    for (auto& pass : shaders::vector(iris)) {
        pass->rebuild();
    }

    rebuild_framebuffers(iris);
}

void destroy(Instance* iris) {
    if (!iris->window)
        return;

    vulkan::wait_idle(iris);

    for (auto& pass_framebuffers : iris->vk.shader_pass_framebuffers) {
        for (VkFramebuffer& framebuffer : pass_framebuffers) {
            if (framebuffer) {
                vkDestroyFramebuffer(iris->vk.device, framebuffer, nullptr);
            }
        }
    }

    iris->vk.shader_pass_framebuffers.clear();

    for (auto& fb : iris->vk.shader_framebuffers) {
        if (fb.view) vkDestroyImageView(iris->vk.device, fb.view, nullptr);
        if (fb.image) vkDestroyImage(iris->vk.device, fb.image, nullptr);
        if (fb.memory) vkFreeMemory(iris->vk.device, fb.memory, nullptr);
    }

    if (iris->vk.shader_descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(iris->vk.device, iris->vk.shader_descriptor_set_layout, nullptr);
    }

    if (iris->vk.default_vert_shader) {
        vkDestroyShaderModule(iris->vk.device, iris->vk.default_vert_shader, nullptr);
    }

    shaders::clear(iris);

    if (iris->renderer) gs::renderer::destroy(iris->renderer);

    if (iris->debug.gsdump) {
        gs::dump::destroy(iris->debug.gsdump);

        iris->debug.gsdump = nullptr;
    }
}

}