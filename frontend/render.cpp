#include <cmath>

#include "iris.hpp"

#define RENDER_MAX_SHADER_PASSES 16

// INCBIN stuff
#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include "incbin.h"

INCBIN(default_vertex_shader, "../shaders/shader.spv");

namespace iris::render {

static int frame = 0;

bool create_image(iris::instance* iris, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage& image, VkImageView& view, VkDeviceMemory& memory) {
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

    if (vkCreateImage(iris->device, &image_info, nullptr, &image) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(iris->device, image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(iris->physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((memory_requirements.memoryTypeBits & (1 << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            alloc_info.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(iris->device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to allocate image memory\n");

        vkDestroyImage(iris->device, image, nullptr);

        return false;
    }

    vkBindImageMemory(iris->device, image, memory, 0);

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

    if (vkCreateImageView(iris->device, &image_view_info, nullptr, &view) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool rebuild_framebuffers(iris::instance* iris) {
    if (!iris->shader_passes.size())
        return true;

    for (auto& fb : iris->shader_framebuffers) {
        if (fb.framebuffer) vkDestroyFramebuffer(iris->device, fb.framebuffer, nullptr);
        if (fb.view) vkDestroyImageView(iris->device, fb.view, nullptr);
        if (fb.image) vkDestroyImage(iris->device, fb.image, nullptr);
        if (fb.memory) vkFreeMemory(iris->device, fb.memory, nullptr);

        bool res = create_image(iris,
            iris->image.width,
            iris->image.height,
            iris->image.format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            fb.image,
            fb.view,
            fb.memory
        );

        if (!res) {
            fprintf(stderr, "render: Failed to create shader pass framebuffer image\n");

            return false;
        }

        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

        // This field is misleading, it makes it seem like we would only ever be able
        // to use this framebuffer in the specified renderpass, but in reality we can
        // use this framebuffer in any renderpass that's **compatible** with the one
        // we're specifying here. "compatible" as defined here:
        // https://docs.vulkan.org/spec/latest/chapters/renderpass.get_html()#renderpass-compatibility
        framebuffer_info.renderPass = iris->shader_passes[0].get_render_pass();
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &fb.view;
        framebuffer_info.width = iris->image.width;
        framebuffer_info.height = iris->image.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(iris->device, &framebuffer_info, nullptr, &fb.framebuffer) != VK_SUCCESS) {
            fprintf(stderr, "render: Failed to create shader pass framebuffer\n");

            return false;
        }
    }

    return true;
}

bool init(iris::instance* iris) {
    iris->shader_passes.reserve(RENDER_MAX_SHADER_PASSES);

    // Initialize our renderer
    iris->renderer = renderer_create();

    renderer_create_info info = {};

    info.backend = iris->renderer_backend;
    info.gif = iris->ps2->gif;
    info.gs = iris->ps2->gs;
    info.instance = iris->instance;
    info.device = iris->device;
    info.physical_device = iris->physical_device;
    info.instance_create_info = iris->instance_create_info;
    info.device_create_info = iris->device_create_info;

    switch (info.backend) {
        case RENDERER_BACKEND_HARDWARE: {
            info.config = &iris->hardware_backend_config;
        } break;
    }

    if (!renderer_init(iris->renderer, info)) {
        fprintf(stderr, "render: Failed to initialize renderer backend\n");

        return false;
    }

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.minLod = -1000;
    sampler_info.maxLod = 1000;
    sampler_info.maxAnisotropy = 1.0f;

    if (vkCreateSampler(iris->device, &sampler_info, VK_NULL_HANDLE, &iris->sampler) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create texture sampler\n");

        return false;
    }

    VkShaderModuleCreateInfo vert_shader_create_info = {};
    vert_shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_shader_create_info.pCode = (const uint32_t*)g_default_vertex_shader_data;
    vert_shader_create_info.codeSize = g_default_vertex_shader_size;

    if (vkCreateShaderModule(iris->device, &vert_shader_create_info, nullptr, &iris->default_vert_shader) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create default vertex shader module\n");

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

    if (vkCreateDescriptorSetLayout(iris->device, &layout_info, nullptr, &iris->shader_descriptor_set_layout) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create descriptor set layout\n");

        return false;
    }

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = iris->descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &iris->shader_descriptor_set_layout;

    if (vkAllocateDescriptorSets(iris->device, &alloc_info, &iris->shader_descriptor_set) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to allocate descriptor sets\n");

        return false;
    }

    return true;
}

static inline void update_vertex_buffer(iris::instance* iris, VkCommandBuffer command_buffer) {
    SDL_Rect size, rect, display;

    SDL_GetWindowSize(iris->window, &size.w, &size.h);

    display.w = size.w;
    display.h = size.h;
    display.x = 0;
    display.y = 0;
    
    if (!iris->fullscreen) {
        display.h -= iris->menubar_height;
        display.y += iris->menubar_height;

        if (iris->show_status_bar) {
            display.h -= iris->menubar_height;
        }
    }

    rect.w = iris->image.width;
    rect.h = iris->image.height;

    float scale = iris->integer_scaling ? floorf(iris->scale) : iris->scale;

    switch (iris->aspect_mode) {
        case RENDER_ASPECT_NATIVE: {
            rect.w *= scale;
            rect.h *= scale;
        } break;

        case RENDER_ASPECT_4_3: {
            rect.w *= scale;
            rect.h = (float)rect.w * (3.0f / 4.0f);
        } break;

        case RENDER_ASPECT_16_9: {
            rect.w *= scale;
            rect.h = (float)rect.w * (9.0f / 16.0f);
        } break;

        case RENDER_ASPECT_5_4: {
            rect.w *= scale;
            rect.h = (float)rect.w * (4.0f / 5.0f);
        } break;

        case RENDER_ASPECT_STRETCH: {
            rect.w = display.w;
            rect.h = display.h;
        } break;

        case RENDER_ASPECT_AUTO:
        case RENDER_ASPECT_STRETCH_KEEP: {
            rect.h = display.h;
            rect.w = (float)rect.h * (4.0f / 3.0f);

            // Scale vertically if the rect ends up being bigger
            // than our display area
            if (rect.w > display.w) {
                rect.w = display.w;
                rect.h = (float)rect.w * (3.0f / 4.0f);
            }
        } break;
    }

    iris->render_width = rect.w;
    iris->render_height = rect.h;

    rect.x = display.x + ((display.w / 2) - (rect.w / 2));
    rect.y = display.y + ((display.h / 2) - (rect.h / 2));

    float x0 = (rect.x / ((float)size.w / 2.0f)) - 1.0f;
    float y0 = (rect.y / ((float)size.h / 2.0f)) - 1.0f;
    float x1 = ((rect.x + rect.w) / ((float)size.w / 2.0f)) - 1.0f;
    float y1 = ((rect.y + rect.h) / ((float)size.h / 2.0f)) - 1.0f;

    iris->vertices[0] = vertex{ { x0, y0 }, {0.0f, 1.0f} };
    iris->vertices[1] = vertex{ { x1, y0 }, {1.0f, 1.0f} };
    iris->vertices[2] = vertex{ { x1, y1 }, {1.0f, 0.0f} };
    iris->vertices[3] = vertex{ { x0, y1 }, {0.0f, 0.0f} };

    void* ptr;

    vkMapMemory(iris->device, iris->vertex_staging_buffer_memory, 0, iris->vertex_buffer_size, 0, &ptr);
    memcpy(ptr, iris->vertices.data(), (size_t)iris->vertex_buffer_size);
    vkUnmapMemory(iris->device, iris->vertex_staging_buffer_memory);

    static VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = iris->vertex_buffer_size;

    vkCmdCopyBuffer(
        command_buffer,
        iris->vertex_staging_buffer,
        iris->vertex_buffer,
        1,
        &region
    );
}

static inline void update_descriptor_set(iris::instance* iris, VkImageView view, VkSampler sampler) {
    VkDescriptorImageInfo image_info = {};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = view;
    image_info.sampler = sampler;

    VkWriteDescriptorSet descriptor_write = {};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = iris->descriptor_set;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(iris->device, 1, &descriptor_write, 0, nullptr);
}

void render_shader_passes(iris::instance* iris, VkCommandBuffer command_buffer, VkImageView& output_view, VkImage& output_image) {
    if (!iris->shader_passes.size())
        return;

    if (iris->shader_framebuffers[0].framebuffer == VK_NULL_HANDLE ||
        iris->shader_framebuffers[1].framebuffer == VK_NULL_HANDLE) {
        rebuild_framebuffers(iris);
    }

    int i = 0;

    for (auto& pass : iris->shader_passes) {
        if (pass.bypass || !pass.ready())
            continue;

        const int fb = i & 1;
        const VkImageView input_view = i == 0 ? iris->image.view : iris->shader_framebuffers[fb ^ 1].view;

        output_view = iris->shader_framebuffers[fb].view;
        output_image = iris->shader_framebuffers[fb].image;

        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = pass.get_render_pass();
        info.framebuffer = iris->shader_framebuffers[fb].framebuffer;
        info.renderArea.extent.width = iris->image.width;
        info.renderArea.extent.height = iris->image.height;
        info.clearValueCount = 1;
        info.pClearValues = &iris->clear_value;

        VkDescriptorImageInfo image_info = {};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = input_view;
        image_info.sampler = iris->sampler;

        VkWriteDescriptorSet descriptor_write = {};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = iris->shader_descriptor_set;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(iris->device, 1, &descriptor_write, 0, nullptr);

        vkCmdBeginRenderPass(command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.get_pipeline());

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(command_buffer, iris->index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.get_pipeline_layout(), 0, 1, &iris->shader_descriptor_set, 0, nullptr);

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = iris->image.width;
        viewport.height = iris->image.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = { (uint32_t)iris->image.width, (uint32_t)iris->image.height };
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        push_constants constants = {
            .resolution = { (float)iris->image.width, (float)iris->image.height },
            .frame = frame
        };

        vkCmdPushConstants(command_buffer, pass.get_pipeline_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants), &constants);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        i++;
    }
}

bool render_frame(iris::instance* iris, VkCommandBuffer command_buffer, VkFramebuffer framebuffer) {
    renderer_image image;

    if (iris->pause) {
        image = iris->image;
    } else {
        image = renderer_get_frame(iris->renderer);
    }

    bool need_rebuild = image.width != iris->image.width ||
                        image.height != iris->image.height ||
                        image.format != iris->image.format;

    iris->image = image;

    if (need_rebuild && image.view != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(iris->device);
        vkQueueWaitIdle(iris->queue);

        for (auto& pass : iris->shader_passes) {
            pass.rebuild();
        }
    }

    // Process shader passes here
    iris->output_image = iris->image;

    if (iris->output_image.view != VK_NULL_HANDLE) {
        render_shader_passes(iris, command_buffer, iris->output_image.view, iris->output_image.image);
    }

    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = iris->render_pass;
    info.framebuffer = framebuffer;
    info.renderArea.extent.width = iris->main_window_data.Width;
    info.renderArea.extent.height = iris->main_window_data.Height;
    info.clearValueCount = 1;
    info.pClearValues = &iris->clear_value;

    if (iris->output_image.view != VK_NULL_HANDLE) {
        update_vertex_buffer(iris, command_buffer);
        update_descriptor_set(iris, iris->output_image.view, iris->sampler);
    }

    vkCmdBeginRenderPass(command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, iris->pipeline);

    if (iris->output_image.view != VK_NULL_HANDLE) {
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &iris->vertex_buffer, offsets);
        vkCmdBindIndexBuffer(command_buffer, iris->index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, iris->pipeline_layout, 0, 1, &iris->descriptor_set, 0, nullptr);
    }

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = iris->main_window_data.Width;
    viewport.height = iris->main_window_data.Height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = { (uint32_t)iris->main_window_data.Width, (uint32_t)iris->main_window_data.Height };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    if (iris->output_image.view != VK_NULL_HANDLE) {
        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);

    frame++;

    return true;
}

void switch_backend(iris::instance* iris, int backend) {
    if (iris->renderer_backend == backend)
        return;

    renderer_destroy(iris->renderer);

    iris->renderer = renderer_create();

    renderer_create_info info = {};

    info.backend = backend;
    info.gif = iris->ps2->gif;
    info.gs = iris->ps2->gs;
    info.instance = iris->instance;
    info.device = iris->device;
    info.physical_device = iris->physical_device;
    info.instance_create_info = iris->instance_create_info;
    info.device_create_info = iris->device_create_info;

    switch (info.backend) {
        case RENDERER_BACKEND_HARDWARE: {
            info.config = &iris->hardware_backend_config;
        } break;
    }

    if (!renderer_init(iris->renderer, info)) {
        fprintf(stderr, "render: Failed to initialize renderer backend\n");
    } else {
        iris->renderer_backend = backend;
    }
}

void refresh(iris::instance* iris) {
    switch (iris->renderer_backend) {
        case RENDERER_BACKEND_HARDWARE: {
            renderer_set_config(iris->renderer, &iris->hardware_backend_config);
        } break;
    }

    iris->image = renderer_get_frame(iris->renderer);

    if (iris->image.view == VK_NULL_HANDLE)
        return;

    if (iris->shader_passes.size() == 0)
        return;

    vkDeviceWaitIdle(iris->device);
    vkQueueWaitIdle(iris->queue);

    for (auto& pass : iris->shader_passes) {
        pass.rebuild();
    }

    rebuild_framebuffers(iris);
}

void destroy(iris::instance* iris) {
    vkDeviceWaitIdle(iris->device);
    vkQueueWaitIdle(iris->queue);

    for (auto& fb : iris->shader_framebuffers) {
        if (fb.framebuffer) vkDestroyFramebuffer(iris->device, fb.framebuffer, nullptr);
        if (fb.view) vkDestroyImageView(iris->device, fb.view, nullptr);
        if (fb.image) vkDestroyImage(iris->device, fb.image, nullptr);
        if (fb.memory) vkFreeMemory(iris->device, fb.memory, nullptr);
    }

    if (iris->shader_descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(iris->device, iris->shader_descriptor_set_layout, nullptr);
    }

    if (iris->default_vert_shader) {
        vkDestroyShaderModule(iris->device, iris->default_vert_shader, nullptr);
    }

    iris->shader_passes.clear();

    renderer_destroy(iris->renderer);
}

}