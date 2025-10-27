#include <cmath>

#include "iris.hpp"

namespace iris::render {

bool init(iris::instance* iris) {
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

    if (!renderer_init(iris->renderer, info)) {
        fprintf(stderr, "render: Failed to initialize renderer backend\n");

        return false;
    }

    VkSamplerCreateInfo sampler_info = {};
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

    if (vkCreateSampler(iris->device, &sampler_info, VK_NULL_HANDLE, &iris->sampler) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create texture sampler\n");

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
        } break;
    }

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

bool render_frame(iris::instance* iris, VkCommandBuffer command_buffer, VkFramebuffer framebuffer) {
    iris->image = renderer_get_frame(iris->renderer);

    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = iris->render_pass;
    info.framebuffer = framebuffer;
    info.renderArea.extent.width = iris->main_window_data.Width;
    info.renderArea.extent.height = iris->main_window_data.Height;
    info.clearValueCount = 1;
    info.pClearValues = &iris->clear_value;

    if (iris->image.image != VK_NULL_HANDLE) {
        update_vertex_buffer(iris, command_buffer);
        update_descriptor_set(iris, iris->image.view, iris->sampler);
    }

    vkCmdBeginRenderPass(command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, iris->pipeline);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &iris->vertex_buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, iris->index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, iris->pipeline_layout, 0, 1, &iris->descriptor_set, 0, nullptr);

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

    if (iris->image.image != VK_NULL_HANDLE) {
        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);

    return true;
}

void destroy(iris::instance* iris) {
    renderer_destroy(iris->renderer);
}

}