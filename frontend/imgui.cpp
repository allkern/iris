#include "iris.hpp"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <array>

// External includes
#include "res/IconsMaterialSymbols.h"

// INCBIN stuff
#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include "incbin.h"

INCBIN(roboto, "../res/Roboto-Regular.ttf");
INCBIN(roboto_black, "../res/Roboto-Black.ttf");
INCBIN(symbols, "../res/MaterialSymbolsRounded.ttf");
INCBIN(firacode, "../res/FiraCode-Regular.ttf");
INCBIN(ps1_memory_card_icon, "../res/ps1_mcd.png");
INCBIN(ps2_memory_card_icon, "../res/ps2_mcd.png");
INCBIN(pocketstation_icon, "../res/pocketstation.png");
INCBIN(iris_icon, "../res/iris.png");
INCBIN(vertex_shader, "../shaders/vertex.spv");
INCBIN(fragment_shader, "../shaders/fragment.spv");

#define VOLK_IMPLEMENTATION
#include <volk.h>

namespace iris::imgui {

enum : int {
    IRIS_THEME_GRANITE,
    IRIS_THEME_IMGUI_DARK,
    IRIS_THEME_IMGUI_LIGHT,
    IRIS_THEME_IMGUI_CLASSIC
};

static const ImWchar g_icon_range[] = { ICON_MIN_MS, ICON_MAX_16_MS, 0 };

static bool setup_vulkan_window(iris::instance* iris, ImGui_ImplVulkanH_Window* wd, int width, int height, bool vsync) {
    wd->Surface = iris->surface;
    wd->ClearEnable = false;

    // Check for WSI support
    VkBool32 res;

    vkGetPhysicalDeviceSurfaceSupportKHR(iris->physical_device, iris->queue_family, wd->Surface, &res);

    if (!res) {
        fprintf(stderr, "imgui: No WSI support on physical device\n");
        
        return false;
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM
    };

    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        iris->physical_device,
        wd->Surface,
        requestSurfaceImageFormat,
        (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
        requestSurfaceColorSpace
    );

    // Select Present Mode
    std::vector <VkPresentModeKHR> present_modes;

    if (vsync) {
        present_modes.push_back(VK_PRESENT_MODE_FIFO_KHR);
    } else {
        present_modes.push_back(VK_PRESENT_MODE_MAILBOX_KHR);
        present_modes.push_back(VK_PRESENT_MODE_IMMEDIATE_KHR);
        present_modes.push_back(VK_PRESENT_MODE_FIFO_KHR);
    }
 
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        iris->physical_device,
        wd->Surface,
        present_modes.data(),
        present_modes.size()
    );

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(iris->min_image_count >= 2);

    ImGui_ImplVulkanH_CreateOrResizeWindow(
        iris->instance,
        iris->physical_device,
        iris->device,
        wd,
        iris->queue_family,
        VK_NULL_HANDLE,
        width, height,
        iris->min_image_count,
        0
    );

    return true;
}

bool setup_fonts(iris::instance* iris, ImGuiIO& io) {
    io.Fonts->AddFontDefault();

    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphMinAdvanceX = 13.0f;
    config.GlyphOffset = ImVec2(0.0f, 4.0f);
    config.FontDataOwnedByAtlas = false;

    ImFontConfig config_no_own;
    config_no_own.FontDataOwnedByAtlas = false;

    iris->font_small_code = io.Fonts->AddFontFromMemoryTTF((void*)g_firacode_data, g_firacode_size, 12.0F, &config_no_own);
    iris->font_code       = io.Fonts->AddFontFromMemoryTTF((void*)g_firacode_data, g_firacode_size, 16.0F, &config_no_own);
    iris->font_small      = io.Fonts->AddFontFromMemoryTTF((void*)g_roboto_data, g_roboto_size, 12.0F, &config_no_own);
    iris->font_heading    = io.Fonts->AddFontFromMemoryTTF((void*)g_roboto_data, g_roboto_size, 20.0F, &config_no_own);
    iris->font_body       = io.Fonts->AddFontFromMemoryTTF((void*)g_roboto_data, g_roboto_size, 16.0F, &config_no_own);
    iris->font_icons      = io.Fonts->AddFontFromMemoryTTF((void*)g_symbols_data, g_symbols_size, 20.0F, &config, g_icon_range);
    iris->font_icons_big  = io.Fonts->AddFontFromMemoryTTF((void*)g_symbols_data, g_symbols_size, 50.0F, &config_no_own, g_icon_range);
    iris->font_black      = io.Fonts->AddFontFromMemoryTTF((void*)g_roboto_black_data, g_roboto_black_size, 30.0F, &config_no_own);

    if (!iris->font_small_code ||
        !iris->font_code ||
        !iris->font_small ||
        !iris->font_heading ||
        !iris->font_body ||
        !iris->font_icons ||
        !iris->font_icons_big ||
        !iris->font_black) {
        return false;
    }

    io.FontDefault = iris->font_body;

    return true;
}

void set_theme(iris::instance* iris, int theme) {
    // Init 'Granite' theme
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding           = ImVec2(8.0, 8.0);
    style.FramePadding            = ImVec2(5.0, 5.0);
    style.ItemSpacing             = ImVec2(8.0, 4.0);
    style.WindowBorderSize        = 0;
    style.ChildBorderSize         = 0;
    style.FrameBorderSize         = 1;
    style.PopupBorderSize         = 0;
    style.TabBorderSize           = 0;
    style.TabBarBorderSize        = 0;
    style.WindowRounding          = 6;
    style.ChildRounding           = 4;
    style.FrameRounding           = 4;
    style.PopupRounding           = 4;
    style.ScrollbarRounding       = 9;
    style.GrabRounding            = 2;
    style.TabRounding             = 4;
    style.WindowTitleAlign        = ImVec2(0.5, 0.5);
    style.DockingSeparatorSize    = 0;
    style.SeparatorTextBorderSize = 1;
    style.SeparatorTextPadding    = ImVec2(20, 0);

    // Use ImGui's default dark style as a base for our own style
    ImGui::StyleColorsDark();

    switch (theme) {
        case IRIS_THEME_GRANITE: {
            ImVec4* colors = style.Colors;

            colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            colors[ImGuiCol_TextDisabled]           = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
            colors[ImGuiCol_WindowBg]               = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
            colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_PopupBg]                = ImVec4(0.07f, 0.09f, 0.10f, 1.00f);
            colors[ImGuiCol_Border]                 = ImVec4(0.10f, 0.12f, 0.13f, 1.00f);
            colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_FrameBg]                = ImVec4(0.10f, 0.12f, 0.13f, 0.50f);
            colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.24f, 0.26f, 0.50f);
            colors[ImGuiCol_FrameBgActive]          = ImVec4(0.29f, 0.35f, 0.39f, 0.50f);
            colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
            colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
            colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
            colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
            colors[ImGuiCol_CheckMark]              = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
            colors[ImGuiCol_SliderGrab]             = ImVec4(0.39f, 0.47f, 0.52f, 0.50f);
            colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.49f, 0.59f, 0.65f, 0.50f);
            colors[ImGuiCol_Button]                 = ImVec4(0.13f, 0.16f, 0.17f, 0.25f);
            colors[ImGuiCol_ButtonHovered]          = ImVec4(0.20f, 0.24f, 0.26f, 0.50f);
            colors[ImGuiCol_ButtonActive]           = ImVec4(0.29f, 0.35f, 0.39f, 0.50f);
            colors[ImGuiCol_Header]                 = ImVec4(0.13f, 0.16f, 0.17f, 0.50f);
            colors[ImGuiCol_HeaderHovered]          = ImVec4(0.20f, 0.24f, 0.26f, 0.50f);
            colors[ImGuiCol_HeaderActive]           = ImVec4(0.29f, 0.35f, 0.39f, 0.50f);
            colors[ImGuiCol_Separator]              = ImVec4(0.23f, 0.28f, 0.30f, 1.00f);
            colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.33f, 0.39f, 0.43f, 1.00f);
            colors[ImGuiCol_SeparatorActive]        = ImVec4(0.38f, 0.46f, 0.51f, 1.00f);
            colors[ImGuiCol_ResizeGrip]             = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 0.30f, 0.25f, 1.00f);
            colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.00f, 0.39f, 0.32f, 1.00f);
            colors[ImGuiCol_InputTextCursor]        = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            colors[ImGuiCol_TabHovered]             = ImVec4(0.23f, 0.28f, 0.30f, 0.59f);
            colors[ImGuiCol_Tab]                    = ImVec4(0.20f, 0.24f, 0.26f, 0.59f);
            colors[ImGuiCol_TabSelected]            = ImVec4(0.26f, 0.31f, 0.35f, 0.59f);
            colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.00f, 0.39f, 0.32f, 1.00f);
            colors[ImGuiCol_TabDimmed]              = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
            colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.10f, 0.12f, 0.13f, 1.00f);
            colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
            colors[ImGuiCol_DockingPreview]         = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
            colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
            colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
            colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
            colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
            colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
            colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
            colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
            colors[ImGuiCol_TextLink]               = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
            colors[ImGuiCol_NavCursor]              = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
        } break;

        case IRIS_THEME_IMGUI_DARK: {
            ImGui::StyleColorsDark();
        } break;

        case IRIS_THEME_IMGUI_LIGHT: {
            ImGui::StyleColorsLight();
        } break;

        case IRIS_THEME_IMGUI_CLASSIC: {
            ImGui::StyleColorsClassic();
        } break;
    }

    ImPlotStyle& pstyle = ImPlot::GetStyle();

    pstyle.MinorGridSize = ImVec2(0.0f, 0.0f);
    pstyle.MajorGridSize = ImVec2(0.0f, 0.0f);
    pstyle.MinorTickLen = ImVec2(0.0f, 0.0f);
    pstyle.MajorTickLen = ImVec2(0.0f, 0.0f);
    pstyle.PlotDefaultSize = ImVec2(250.0f, 150.0f);
    pstyle.PlotPadding = ImVec2(0.0f, 0.0f);
    pstyle.LegendPadding = ImVec2(0.0f, 0.0f);
    pstyle.LegendInnerPadding = ImVec2(0.0f, 0.0f);
    pstyle.LineWeight = 2.0f;

    pstyle.Colors[ImPlotCol_Line]       = ImVec4(0.0f, 1.0f, 0.2f, 1.0f);
    pstyle.Colors[ImPlotCol_FrameBg]    = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    pstyle.Colors[ImPlotCol_PlotBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}

VkShaderModule create_shader(iris::instance* iris, uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pCode = code;
    info.codeSize = size;

    VkShaderModule shader;

    if (vkCreateShaderModule(iris->device, &info, nullptr, &shader) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return shader;
}

VkPipeline create_pipeline(iris::instance* iris, VkShaderModule vert_shader, VkShaderModule frag_shader) {
    // Create pipeline layout
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1; // Optional
    pipeline_layout_info.pSetLayouts = &iris->descriptor_set_layout; // Optional
    pipeline_layout_info.pushConstantRangeCount = 0; // Optional
    pipeline_layout_info.pPushConstantRanges = VK_NULL_HANDLE; // Optional

    if (vkCreatePipelineLayout(iris->device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to create pipeline layout\n");

        return VK_NULL_HANDLE;
    }

    iris->pipeline_layout = pipeline_layout;

    // Create render pass
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = iris->main_window_data.SurfaceFormat.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPass render_pass = VK_NULL_HANDLE;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    if (vkCreateRenderPass(iris->device, &render_pass_info, nullptr, &render_pass) != VK_SUCCESS) {
        fprintf(stderr, "vulkan: Failed to create render pass\n");

        return VK_NULL_HANDLE;
    }

    iris->render_pass = render_pass;

    // Create graphics pipeline
    VkPipelineShaderStageCreateInfo shader_stages[2] = {};
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vert_shader;
    shader_stages[0].pName = "main";
    shader_stages[0].pNext = VK_NULL_HANDLE;
    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = frag_shader;
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

    const auto binding_description = vertex::get_binding_description();
    const auto attribute_descriptions = vertex::get_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = 2;
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)iris->main_window_data.Width;
    viewport.height = (float)iris->main_window_data.Height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkExtent2D extent = {};
    extent.width = iris->main_window_data.Width;
    extent.height = iris->main_window_data.Height;

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
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.pTessellationState = VK_NULL_HANDLE;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipeline_info.basePipelineIndex = -1; // Optional

    VkPipeline pipeline = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(iris->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    vkDestroyShaderModule(iris->device, frag_shader, nullptr);
    vkDestroyShaderModule(iris->device, vert_shader, nullptr);

    return pipeline;
}


bool init(iris::instance* iris) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 0;

    for (VkDescriptorPoolSize& pool_size : pool_sizes)
        pool_info.maxSets += pool_size.descriptorCount;

    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(iris->device, &pool_info, VK_NULL_HANDLE, &iris->descriptor_pool) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to create descriptor pool\n");

        return false;
    }

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

    if (vkCreateDescriptorSetLayout(iris->device, &layout_info, nullptr, &iris->descriptor_set_layout) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to create descriptor set layout\n");

        return false;
    }

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = iris->descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &iris->descriptor_set_layout;

    if (vkAllocateDescriptorSets(iris->device, &alloc_info, &iris->descriptor_set) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to allocate descriptor sets\n");

        return false;
    }

    if (!SDL_Vulkan_CreateSurface(iris->window, iris->instance, VK_NULL_HANDLE, &iris->surface)) {
        printf("imgui: Failed to create Vulkan surface\n");

        return false;
    }

    SDL_ShowWindow(iris->window);

    if (!setup_vulkan_window(iris, &iris->main_window_data, iris->window_width, iris->window_height, true)) {
        printf("imgui: Failed to setup Vulkan window\n");

        return false;
    }

    iris->ini_path = iris->pref_path + "imgui.ini";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;
    io.IniFilename = iris->ini_path.c_str();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(iris->main_scale);
    style.FontScaleDpi = iris->main_scale;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    // Setup Platform/Renderer backends
    if (!ImGui_ImplSDL3_InitForVulkan(iris->window)) {
        fprintf(stderr, "imgui: Failed to initialize SDL3/Vulkan backend\n");

        return false;
    }

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = IRIS_VULKAN_API_VERSION;
    init_info.Instance = iris->instance;
    init_info.PhysicalDevice = iris->physical_device;
    init_info.Device = iris->device;
    init_info.QueueFamily = iris->queue_family;
    init_info.Queue = iris->queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = iris->descriptor_pool;
    init_info.MinImageCount = iris->min_image_count;
    init_info.ImageCount = iris->main_window_data.ImageCount;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.PipelineInfoMain.RenderPass = iris->main_window_data.RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = VK_NULL_HANDLE;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        fprintf(stderr, "imgui: Failed to initialize Vulkan backend\n");

        return false;
    }

    if (!setup_fonts(iris, io)) {
        fprintf(stderr, "imgui: Failed to setup fonts\n");

        return false;
    }

    // To-do: Make theme selectable, for now just use granite
    set_theme(iris, IRIS_THEME_GRANITE);

    iris->clear_value.color.float32[0] = 0.11f;
    iris->clear_value.color.float32[1] = 0.11f;
    iris->clear_value.color.float32[2] = 0.11f;
    iris->clear_value.color.float32[3] = 1.0f;

    // Initialize our pipeline
    VkShaderModule vert_shader = create_shader(iris, (uint32_t*)g_vertex_shader_data, g_vertex_shader_size);
    VkShaderModule frag_shader = create_shader(iris, (uint32_t*)g_fragment_shader_data, g_fragment_shader_size);

    if (!vert_shader || !frag_shader) {
        fprintf(stderr, "vulkan: Failed to create shader modules\n");

        return false;
    }

    iris->pipeline = create_pipeline(iris, vert_shader, frag_shader);

    if (!iris->pipeline) {
        fprintf(stderr, "imgui: Failed to create graphics pipeline\n");

        return false;
    }

    return true;
}

void cleanup(iris::instance* iris) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    ImGui_ImplVulkanH_DestroyWindow(iris->instance, iris->device, &iris->main_window_data, VK_NULL_HANDLE);

    vkDestroyDescriptorPool(iris->device, iris->descriptor_pool, nullptr);
}

bool render_frame(iris::instance* iris, ImDrawData* draw_data) {
    ImGui_ImplVulkanH_Window* wd = &iris->main_window_data;

    VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

    VkResult err = vkAcquireNextImageKHR(
        iris->device,
        wd->Swapchain,
        UINT64_MAX,
        image_acquired_semaphore,
        VK_NULL_HANDLE,
        &wd->FrameIndex
    );

    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        iris->swapchain_rebuild = true;
    }

    if (err == VK_ERROR_OUT_OF_DATE_KHR) {
        return true;
    }

    if (err != VK_SUBOPTIMAL_KHR) {
        if (err != VK_SUCCESS) {
            fprintf(stderr, "imgui: Failed to acquire next image\n");

            return false;
        }
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    
    if (vkWaitForFences(iris->device, 1, &fd->Fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to wait for fence\n");

        return false;
    }

    if (vkResetFences(iris->device, 1, &fd->Fence) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to reset fence\n");

        return false;
    }

    if (vkResetCommandPool(iris->device, fd->CommandPool, 0) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to reset command pool\n");

        return false;
    }

    // To-do: Render backend image here!!!

    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(fd->CommandBuffer, &info) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to begin command buffer\n");
        
        return false;
    }

    render::render_frame(iris, fd->CommandBuffer, fd->Framebuffer);

    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &iris->clear_value;

        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);

    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        if (vkEndCommandBuffer(fd->CommandBuffer) != VK_SUCCESS) {
            fprintf(stderr, "imgui: Failed to end command buffer\n");
        
            return false;
        }

        if (vkQueueSubmit(iris->queue, 1, &info, fd->Fence) != VK_SUCCESS) {
            fprintf(stderr, "imgui: Failed to submit queue\n");
        
            return false;
        }
    }

    return true;
}

bool present_frame(iris::instance* iris) {
    if (iris->swapchain_rebuild)
        return true;

    ImGui_ImplVulkanH_Window* wd = &iris->main_window_data;

    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;

    VkResult err = vkQueuePresentKHR(iris->queue, &info);

    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        iris->swapchain_rebuild = true;
    }

    if (err == VK_ERROR_OUT_OF_DATE_KHR) {
        return true;
    }

    if (err != VK_SUBOPTIMAL_KHR) {
        if (err != VK_SUCCESS) {
            fprintf(stderr, "imgui: Failed to present frame\n");

            return false;
        }
    }

    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;

    return true;
}

}
