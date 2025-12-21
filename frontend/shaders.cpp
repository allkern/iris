#include <unordered_map>
#include <algorithm>

#include "config.hpp"
#include "iris.hpp"

// INCBIN stuff
#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include "incbin.h"

INCBIN(encoder_frag_shader, "../shaders/encoder.spv");
INCBIN(decoder_frag_shader, "../shaders/decoder.spv");
INCBIN(sharpen_frag_shader, "../shaders/sharpen.spv");

namespace iris::shaders {

bool pass::init(iris::instance* iris, const void* data, size_t size, std::string id) {
    m_vert_shader = iris->default_vert_shader;
    m_iris = iris;
    m_id = id;

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pCode = (const uint32_t*)data;
    create_info.codeSize = size;

    if (vkCreateShaderModule(m_iris->device, &create_info, nullptr, &m_frag_shader) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create fragment shader module\n");

        return false;
    }

    return rebuild();
}

pass::pass(iris::instance* iris, const void* data, size_t size, std::string id) {
    init(iris, data, size, id);
}

pass::pass(pass&& other) {
    m_pipeline_layout = other.m_pipeline_layout;
    m_pipeline = other.m_pipeline;
    m_render_pass = other.m_render_pass;
    m_input = other.m_input;
    m_vert_shader = other.m_vert_shader;
    m_frag_shader = other.m_frag_shader;
    m_iris = other.m_iris;
    m_id = other.m_id;
    bypass = other.bypass;

    other.m_pipeline_layout = VK_NULL_HANDLE;
    other.m_pipeline = VK_NULL_HANDLE;
    other.m_render_pass = VK_NULL_HANDLE;
    other.m_input = VK_NULL_HANDLE;
    other.m_vert_shader = VK_NULL_HANDLE;
    other.m_frag_shader = VK_NULL_HANDLE;
    other.m_iris = nullptr;
    other.m_id = "";
    other.bypass = false;
}

pass& pass::operator=(pass&& other) {
    if (this != &other) {
        m_pipeline_layout = other.m_pipeline_layout;
        m_pipeline = other.m_pipeline;
        m_render_pass = other.m_render_pass;
        m_input = other.m_input;
        m_vert_shader = other.m_vert_shader;
        m_frag_shader = other.m_frag_shader;
        m_iris = other.m_iris;
        m_id = other.m_id;
        bypass = other.bypass;
    }

    other.m_pipeline_layout = VK_NULL_HANDLE;
    other.m_pipeline = VK_NULL_HANDLE;
    other.m_render_pass = VK_NULL_HANDLE;
    other.m_input = VK_NULL_HANDLE;
    other.m_vert_shader = VK_NULL_HANDLE;
    other.m_frag_shader = VK_NULL_HANDLE;
    other.m_iris = nullptr;
    other.m_id = "";
    other.bypass = false;

    return *this;
}

void pass::destroy() {
    if (!m_iris)
        return;

    vkQueueWaitIdle(m_iris->queue);
    vkDeviceWaitIdle(m_iris->device);

    if (m_pipeline_layout) vkDestroyPipelineLayout(m_iris->device, m_pipeline_layout, nullptr);
    if (m_pipeline) vkDestroyPipeline(m_iris->device, m_pipeline, nullptr);
    if (m_render_pass) vkDestroyRenderPass(m_iris->device, m_render_pass, nullptr);
    if (m_frag_shader) vkDestroyShaderModule(m_iris->device, m_frag_shader, nullptr);

    if (m_vert_shader != m_iris->default_vert_shader)
        if (m_vert_shader) vkDestroyShaderModule(m_iris->device, m_vert_shader, nullptr);
}

pass::~pass() {
    destroy();
}

bool pass::rebuild() {
    vkQueueWaitIdle(m_iris->queue);
    vkDeviceWaitIdle(m_iris->device);

    if (m_pipeline_layout) vkDestroyPipelineLayout(m_iris->device, m_pipeline_layout, nullptr);
    if (m_pipeline) vkDestroyPipeline(m_iris->device, m_pipeline, nullptr);
    if (m_render_pass) vkDestroyRenderPass(m_iris->device, m_render_pass, nullptr);

    VkPushConstantRange push_constant_range = {};
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(push_constants);
    push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_iris->shader_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    if (vkCreatePipelineLayout(m_iris->device, &pipeline_layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create shader pipeline layout\n");

        destroy();

        return false;
    }

    // Create render pass
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = m_iris->image.format;
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

    if (vkCreateRenderPass(m_iris->device, &render_pass_info, nullptr, &m_render_pass) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create shader render pass\n");

        destroy();

        return false;
    }

    // Create graphics pipeline
    VkPipelineShaderStageCreateInfo shader_stages[2] = {};
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = m_iris->default_vert_shader;
    shader_stages[0].pName = "main";
    shader_stages[0].pNext = VK_NULL_HANDLE;
    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = m_frag_shader;
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
    viewport.width = (float)m_iris->image.width;
    viewport.height = (float)m_iris->image.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkExtent2D extent = {};
    extent.width = m_iris->image.width;
    extent.height = m_iris->image.height;

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
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.pTessellationState = VK_NULL_HANDLE;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipeline_info.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(m_iris->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline) != VK_SUCCESS) {
        fprintf(stderr, "render: Failed to create shader pipeline\n");

        destroy();

        return false;
    }

    return true;
}

bool pass::ready() {
    return m_pipeline_layout &&
           m_pipeline &&
           m_render_pass &&
           m_vert_shader &&
           m_frag_shader &&
           m_iris;
}

VkPipelineLayout& pass::get_pipeline_layout() {
    return m_pipeline_layout;
}

VkPipeline& pass::get_pipeline() {
    return m_pipeline;
}

VkRenderPass& pass::get_render_pass() {
    return m_render_pass;
}

VkImageView& pass::get_input() {
    return m_input;
}

VkShaderModule& pass::get_vert_shader() {
    return m_vert_shader;
}

VkShaderModule& pass::get_frag_shader() {
    return m_frag_shader;
}

std::string pass::get_id() const {
    return m_id;
}

std::unordered_map <std::string, std::pair <void*, size_t>> g_builtin_shaders = {
    { "iris-ntsc-encoder", { (void*)g_encoder_frag_shader_data, (size_t)g_encoder_frag_shader_size } },
    { "iris-ntsc-decoder", { (void*)g_decoder_frag_shader_data, (size_t)g_decoder_frag_shader_size } },
    { "iris-ntsc-sharpen", { (void*)g_sharpen_frag_shader_data, (size_t)g_sharpen_frag_shader_size } },
};

void push(iris::instance* iris, void* data, size_t size, std::string id) {
    iris->shader_passes.push_back(new pass(iris, data, size, id));
}

void push(iris::instance* iris, std::string id) {
    auto s = g_builtin_shaders.find(id);

    if (s != g_builtin_shaders.end()) {
        push(iris, s->second.first, s->second.second, id);
    }
}

void pop(iris::instance* iris) {
    delete iris->shader_passes.back();

    iris->shader_passes.pop_back();
}

void insert(iris::instance* iris, int i, void* data, size_t size, std::string id) {
    iris->shader_passes.insert(
        iris->shader_passes.begin() + i,
        new pass(iris, data, size, id)
    );
}

void erase(iris::instance* iris, int i) {
    delete iris->shader_passes.at(i);

    iris->shader_passes.erase(iris->shader_passes.begin() + i);
}

pass* at(iris::instance* iris, int i) {
    return iris->shader_passes.at(i);
}

void swap(iris::instance* iris, int a, int b) {
    pass* t = iris->shader_passes.at(a);

    iris->shader_passes[a] = iris->shader_passes[b];
    iris->shader_passes[b] = t;
}

pass* front(iris::instance* iris) {
    return iris->shader_passes.front();
}

pass* back(iris::instance* iris) {
    return iris->shader_passes.back();
}

size_t count(iris::instance* iris) {
    return iris->shader_passes.size();
}

void clear(iris::instance* iris) {
    for (auto& pass : iris->shader_passes) {
        delete pass;
    }

    iris->shader_passes.clear();
}

std::vector <shaders::pass*>& vector(iris::instance* iris) {
    return iris->shader_passes;
}

}