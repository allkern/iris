/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <volk.h>

namespace iris {

struct Instance;

struct Vertex {
    struct {
        float x, y;
    } pos, uv;

    static constexpr const VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription binding_description = {};

        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return binding_description;
    }

    static constexpr const std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions = {};

        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[0].offset = offsetof(Vertex, pos);

        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[1].offset = offsetof(Vertex, uv);

        return attribute_descriptions;
    }
};

struct Texture {
    int width = 0, height = 0, stride = 0;
    VkDeviceSize image_size = 0;
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

struct VulkanGpu {
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    VkPhysicalDevice device = VK_NULL_HANDLE;
    std::string name = "";
    uint32_t api_version = 0;
};

struct PushConstants {
    float resolution[2];
    int frame;
    int pad;

    // FSR stuff
    uint32_t con0[4];
    uint32_t con1[4];
    uint32_t con2[4];
    uint32_t con3[4];
};

namespace vulkan {

void use_bundled_drivers(Instance* iris);
bool init(Instance* iris, bool enable_validation = false);
void cleanup(Instance* iris);
Texture upload_texture(Instance* iris, void* pixels, int width, int height, int stride);
void free_texture(Instance* iris, Texture& tex);
void free_textures(Instance* iris, std::vector <Texture>& textures);
void* read_image(Instance* iris, VkImage image, VkFormat format, int width, int height);
void wait_idle(Instance* iris);
void dump_device_fault(Instance* iris);
Texture load_texture_from_memory(Instance* iris, const void* data, size_t size);
Texture load_texture_from_file(Instance* iris, std::string path);

}

}
