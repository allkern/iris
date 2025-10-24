#include "iris.hpp"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

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

    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;

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
