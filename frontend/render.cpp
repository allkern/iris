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

bool render_frame(iris::instance* iris) {
    renderer_image image = renderer_get_frame(iris->renderer);

    if (image.image == VK_NULL_HANDLE) {
        // No frame to render
        ImGui::Text("Display not initialized");

        return true;
    }

    if (iris->descriptor_set != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(iris->descriptor_set);
    }

    iris->descriptor_set = ImGui_ImplVulkan_AddTexture(iris->sampler, image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    ImGui::Image((ImTextureID)iris->descriptor_set, ImVec2(image.width, image.height));

    return true;
}

void destroy(iris::instance* iris) {
    renderer_destroy(iris->renderer);
}

}