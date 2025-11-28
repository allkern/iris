#include <cmath>

#include "iris.hpp"

// INCBIN stuff
#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include "incbin.h"

INCBIN(default_vertex_shader, "../shaders/shader.spv");
INCBIN(encoder_frag_shader, "../shaders/encoder.spv");
INCBIN(decoder_frag_shader, "../shaders/decoder.spv");
INCBIN(sharpen_frag_shader, "../shaders/sharpen.spv");

#define RENDER_MAX_SHADER_PASSES 16

namespace iris::render {

static int frame = 0;

struct push_constants {
    float resolution[2];
    int frame;
};

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

bool rebuild_shaders(iris::instance* iris) {
    // To-do: Destroy old resources

    for (auto& pass : iris->shader_passes) {
        // Create pipeline layout
        VkPushConstantRange push_constant_range = {};
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(push_constants);
        push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &iris->shader_descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;

        if (vkCreatePipelineLayout(iris->device, &pipeline_layout_info, nullptr, &pass.pipeline_layout) != VK_SUCCESS) {
            fprintf(stderr, "render: Failed to create shader pipeline layout\n");

            return false;
        }

        // Create render pass
        VkAttachmentDescription color_attachment = {};
        color_attachment.format = iris->image.format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference color_attachment_ref = {};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPass render_pass = VK_NULL_HANDLE;

        VkRenderPassCreateInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        if (vkCreateRenderPass(iris->device, &render_pass_info, nullptr, &pass.render_pass) != VK_SUCCESS) {
            fprintf(stderr, "render: Failed to create shader render pass\n");

            return false;
        }

        // Create graphics pipeline
        VkPipelineShaderStageCreateInfo shader_stages[2] = {};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = iris->default_vert_shader;
        shader_stages[0].pName = "main";
        shader_stages[0].pNext = VK_NULL_HANDLE;
        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = pass.frag_shader;
        shader_stages[1].pName = "main";
        shader_stages[1].pNext = VK_NULL_HANDLE;

        static const VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
        dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_info.dynamicStateCount = 2;
        dynamic_state_info.pDynamicStates = dynamic_states;

        VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 0;
        vertex_input_info.pVertexBindingDescriptions = VK_NULL_HANDLE;
        vertex_input_info.vertexAttributeDescriptionCount = 0;
        vertex_input_info.pVertexAttributeDescriptions = VK_NULL_HANDLE;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_info.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)iris->image.width;
        viewport.height = (float)iris->image.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkExtent2D extent = {};
        extent.width = iris->image.width;
        extent.height = iris->image.height;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = extent;

        VkPipelineViewportStateCreateInfo viewport_state_info = {};
        viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_info.viewportCount = 1;
        viewport_state_info.pViewports = &viewport;
        viewport_state_info.scissorCount = 1;
        viewport_state_info.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer_info = {};
        rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_info.depthClampEnable = VK_FALSE;
        rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
        rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer_info.lineWidth = 1.0f;
        rasterizer_info.cullMode = VK_CULL_MODE_NONE;
        rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer_info.depthBiasEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState blend_attachment_state = {};
        blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blend_attachment_state.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo blend_state_info{};
        blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend_state_info.logicOpEnable = VK_FALSE;
        blend_state_info.attachmentCount = 1;
        blend_state_info.pAttachments = &blend_attachment_state;

        VkPipelineMultisampleStateCreateInfo multisampling_state_info = {};
        multisampling_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_state_info.sampleShadingEnable = VK_FALSE;
        multisampling_state_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly_info;
        pipeline_info.pViewportState = &viewport_state_info;
        pipeline_info.pRasterizationState = &rasterizer_info;
        pipeline_info.pMultisampleState = &multisampling_state_info;
        pipeline_info.pDepthStencilState = nullptr; // Optional
        pipeline_info.pColorBlendState = &blend_state_info;
        pipeline_info.pDynamicState = &dynamic_state_info;
        pipeline_info.layout = pass.pipeline_layout;
        pipeline_info.renderPass = pass.render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.pTessellationState = VK_NULL_HANDLE;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipeline_info.basePipelineIndex = -1; // Optional

        if (vkCreateGraphicsPipelines(iris->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pass.pipeline) != VK_SUCCESS) {
            fprintf(stderr, "render: Failed to create shader pipeline\n");

            return false;
        }
    }

    for (auto& fb : iris->shader_framebuffers) {
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
        // https://docs.vulkan.org/spec/latest/chapters/renderpass.html#renderpass-compatibility
        framebuffer_info.renderPass = iris->shader_passes[0].render_pass;
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

bool push_shader(iris::instance* iris, const void* data, size_t size) {
    shader_pass pass = {};

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pCode = (const uint32_t*)data;
    create_info.codeSize = size;

    if (vkCreateShaderModule(iris->device, &create_info, nullptr, &pass.frag_shader) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create encoder fragment shader module\n");

        return false;
    }

    iris->shader_passes.push_back(pass);

    return true;
}

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

    if (push_shader(iris, g_encoder_frag_shader_data, g_encoder_frag_shader_size) == false ||
        push_shader(iris, g_decoder_frag_shader_data, g_decoder_frag_shader_size) == false ||
        push_shader(iris, g_sharpen_frag_shader_data, g_sharpen_frag_shader_size) == false) {
        fprintf(stderr, "render: Failed to create default shaders\n");

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
    int i = 0;

    for (const auto& pass : iris->shader_passes) {
        const int fb = i & 1;
        const VkImageView input_view = i == 0 ? iris->image.view : iris->shader_framebuffers[fb ^ 1].view;

        output_view = iris->shader_framebuffers[fb].view;
        output_image = iris->shader_framebuffers[fb].image;

        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = pass.render_pass;
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

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(command_buffer, iris->index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline_layout, 0, 1, &iris->shader_descriptor_set, 0, nullptr);

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

        vkCmdPushConstants(command_buffer, pass.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants), &constants);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        i++;
    }
}

bool render_frame(iris::instance* iris, VkCommandBuffer command_buffer, VkFramebuffer framebuffer) {
    renderer_image image = renderer_get_frame(iris->renderer);

    bool need_rebuild = image.width != iris->image.width ||
                        image.height != iris->image.height ||
                        image.format != iris->image.format;

    iris->image = image;

    if (need_rebuild && image.view != VK_NULL_HANDLE) {
        if (!rebuild_shaders(iris)) {
            fprintf(stderr, "render: Failed to build shaders\n");

            return false;
        }
    }

    // Process shader passes here
    VkImageView output_view = iris->image.view;
    VkImage output_image = iris->image.image;

    if (output_view != VK_NULL_HANDLE) {
        render_shader_passes(iris, command_buffer, output_view, output_image);
    }

    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = iris->render_pass;
    info.framebuffer = framebuffer;
    info.renderArea.extent.width = iris->main_window_data.Width;
    info.renderArea.extent.height = iris->main_window_data.Height;
    info.clearValueCount = 1;
    info.pClearValues = &iris->clear_value;

    if (output_view != VK_NULL_HANDLE) {
        update_vertex_buffer(iris, command_buffer);
        update_descriptor_set(iris, output_view, iris->sampler);
    }

    vkCmdBeginRenderPass(command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, iris->pipeline);

    if (output_view != VK_NULL_HANDLE) {
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

    if (output_view != VK_NULL_HANDLE) {
        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);

    frame++;

    return true;
}

void destroy(iris::instance* iris) {
    renderer_destroy(iris->renderer);
}

}