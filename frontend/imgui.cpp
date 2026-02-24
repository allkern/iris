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
INCBIN(dualshock2_icon, "../res/ds2.png");
INCBIN(pocketstation_icon, "../res/pocketstation.png");
INCBIN(iris_icon, "../res/iris.png");
INCBIN(vertex_shader, "../shaders/vertex.spv");
INCBIN(fragment_shader, "../shaders/fragment.spv");

#include "stb_image.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>

namespace iris::imgui {

static const ImWchar g_icon_range[] = { ICON_MIN_MS, ICON_MAX_16_MS, 0 };

static bool setup_vulkan_window(iris::instance* iris, ImGui_ImplVulkanH_Window* wd, int width, int height, bool vsync) {
    wd->Surface = iris->surface;

    VkAttachmentDescription attachment = {};
    attachment.format = wd->SurfaceFormat.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    wd->AttachmentDesc = attachment;

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

    io.FontDefault = iris->font_icons;

    return true;
}

void set_theme(iris::instance* iris, int theme, bool set_bg_color) {
    // Init 'Granite' theme
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding           = ImVec2(8.0, 8.0);
    style.FramePadding            = ImVec2(5.0, 5.0);
    style.ItemSpacing             = ImVec2(8.0, 6.0);
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
            colors[ImGuiCol_DragDropTarget]         = ImVec4(0.29f, 0.38f, 0.42f, 1.00f);
            colors[ImGuiCol_NavCursor]              = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);

            if (!set_bg_color) break;

            iris->clear_value.color.float32[0] = 0.11f;
            iris->clear_value.color.float32[1] = 0.11f;
            iris->clear_value.color.float32[2] = 0.11f;
            iris->clear_value.color.float32[3] = 1.00f;
        } break;

        case IRIS_THEME_IMGUI_DARK: {
            ImGui::StyleColorsDark();

            if (!set_bg_color) break;

            iris->clear_value.color.float32[0] = 0.11f;
            iris->clear_value.color.float32[1] = 0.11f;
            iris->clear_value.color.float32[2] = 0.11f;
            iris->clear_value.color.float32[3] = 1.00f;
        } break;

        case IRIS_THEME_IMGUI_LIGHT: {
            ImGui::StyleColorsLight();

            if (!set_bg_color) break;

            iris->clear_value.color.float32[0] = 0.89f;
            iris->clear_value.color.float32[1] = 0.89f;
            iris->clear_value.color.float32[2] = 0.89f;
            iris->clear_value.color.float32[3] = 1.00f;
        } break;

        case IRIS_THEME_IMGUI_CLASSIC: {
            ImGui::StyleColorsClassic();

            if (!set_bg_color) break;

            iris->clear_value.color.float32[0] = 0.11f;
            iris->clear_value.color.float32[1] = 0.11f;
            iris->clear_value.color.float32[2] = 0.11f;
            iris->clear_value.color.float32[3] = 1.00f;
        } break;

        case IRIS_THEME_CHERRY: {
            // cherry colors, 3 intensities
            #define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
            #define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
            #define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
            // backgrounds
            #define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
            // text
            #define TEXT(v) ImVec4(0.860f, 0.930f, 0.890f, v)

            auto &style = ImGui::GetStyle();
            style.Colors[ImGuiCol_Text]                  = TEXT(0.78f);
            style.Colors[ImGuiCol_TextDisabled]          = TEXT(0.28f);
            style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
            style.Colors[ImGuiCol_ChildBg]               = BG( 0.58f);
            style.Colors[ImGuiCol_PopupBg]               = BG( 0.9f);
            style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
            style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[ImGuiCol_FrameBg]               = BG( 1.00f);
            style.Colors[ImGuiCol_FrameBgHovered]        = MED( 0.78f);
            style.Colors[ImGuiCol_FrameBgActive]         = MED( 1.00f);
            style.Colors[ImGuiCol_TitleBg]               = LOW( 1.00f);
            style.Colors[ImGuiCol_TitleBgActive]         = HI( 1.00f);
            style.Colors[ImGuiCol_TitleBgCollapsed]      = BG( 0.75f);
            style.Colors[ImGuiCol_MenuBarBg]             = BG( 0.47f);
            style.Colors[ImGuiCol_ScrollbarBg]           = BG( 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrabHovered]  = MED( 0.78f);
            style.Colors[ImGuiCol_ScrollbarGrabActive]   = MED( 1.00f);
            style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
            style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_Button]                = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
            style.Colors[ImGuiCol_ButtonHovered]         = MED( 0.86f);
            style.Colors[ImGuiCol_ButtonActive]          = MED( 1.00f);
            style.Colors[ImGuiCol_Header]                = MED( 0.76f);
            style.Colors[ImGuiCol_HeaderHovered]         = MED( 0.86f);
            style.Colors[ImGuiCol_HeaderActive]          = HI( 1.00f);
            style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
            style.Colors[ImGuiCol_ResizeGripHovered]     = MED( 0.78f);
            style.Colors[ImGuiCol_ResizeGripActive]      = MED( 1.00f);
            style.Colors[ImGuiCol_PlotLines]             = TEXT(0.63f);
            style.Colors[ImGuiCol_PlotLinesHovered]      = MED( 1.00f);
            style.Colors[ImGuiCol_PlotHistogram]         = TEXT(0.63f);
            style.Colors[ImGuiCol_PlotHistogramHovered]  = MED( 1.00f);
            style.Colors[ImGuiCol_TextSelectedBg]        = MED( 0.43f);
            style.Colors[ImGuiCol_ModalWindowDimBg]      = BG( 0.73f);

            #undef HI
            #undef MED
            #undef LOW
            #undef BG
            #undef TEXT

            if (!set_bg_color) break;

            iris->clear_value.color.float32[0] = 0.20f * 0.5f;
            iris->clear_value.color.float32[1] = 0.22f * 0.5f;
            iris->clear_value.color.float32[2] = 0.27f * 0.5f;
            iris->clear_value.color.float32[3] = 1.00f;
        } break;

        case IRIS_THEME_SOURCE: {
            ImVec4* colors = ImGui::GetStyle().Colors;

            colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            colors[ImGuiCol_WindowBg]              = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
            colors[ImGuiCol_ChildBg]               = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
            colors[ImGuiCol_PopupBg]               = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
            colors[ImGuiCol_Border]                = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
            colors[ImGuiCol_BorderShadow]          = ImVec4(0.14f, 0.16f, 0.11f, 0.52f);
            colors[ImGuiCol_FrameBg]               = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.27f, 0.30f, 0.23f, 1.00f);
            colors[ImGuiCol_FrameBgActive]         = ImVec4(0.30f, 0.34f, 0.26f, 1.00f);
            colors[ImGuiCol_TitleBg]               = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
            colors[ImGuiCol_TitleBgActive]         = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
            colors[ImGuiCol_MenuBarBg]             = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.28f, 0.32f, 0.24f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.25f, 0.30f, 0.22f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.23f, 0.27f, 0.21f, 1.00f);
            colors[ImGuiCol_CheckMark]             = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
            colors[ImGuiCol_SliderGrab]            = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
            colors[ImGuiCol_Button]                = ImVec4(0.29f, 0.34f, 0.26f, 0.40f);
            colors[ImGuiCol_ButtonHovered]         = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
            colors[ImGuiCol_ButtonActive]          = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
            colors[ImGuiCol_Header]                = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
            colors[ImGuiCol_HeaderHovered]         = ImVec4(0.35f, 0.42f, 0.31f, 0.60f);
            colors[ImGuiCol_HeaderActive]          = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
            colors[ImGuiCol_Separator]             = ImVec4(0.14f, 0.16f, 0.11f, 1.00f);
            colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.54f, 0.57f, 0.51f, 1.00f);
            colors[ImGuiCol_SeparatorActive]       = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
            colors[ImGuiCol_ResizeGrip]            = ImVec4(0.19f, 0.23f, 0.18f, 0.00f);
            colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.54f, 0.57f, 0.51f, 1.00f);
            colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
            colors[ImGuiCol_Tab]                   = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
            colors[ImGuiCol_TabHovered]            = ImVec4(0.54f, 0.57f, 0.51f, 0.78f);
            colors[ImGuiCol_TabActive]             = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
            colors[ImGuiCol_TabUnfocused]          = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
            colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
            colors[ImGuiCol_DockingPreview]        = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
            colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
            colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
            colors[ImGuiCol_PlotHistogram]         = ImVec4(1.00f, 0.78f, 0.28f, 1.00f);
            colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
            colors[ImGuiCol_DragDropTarget]        = ImVec4(0.73f, 0.67f, 0.24f, 1.00f);
            colors[ImGuiCol_NavHighlight]          = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
            colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

            if (!set_bg_color) break;

            iris->clear_value.color.float32[0] = 0.13f;
            iris->clear_value.color.float32[1] = 0.15f;
            iris->clear_value.color.float32[2] = 0.11f;
            iris->clear_value.color.float32[3] = 1.00f;
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

void set_codeview_scheme(iris::instance* iris, int scheme) {
    switch (scheme) {
        default: case IRIS_CODEVIEW_COLOR_SCHEME_SOLARIZED_DARK: {
            iris->codeview_color_text = IM_COL32(131, 148, 150, 255);
            iris->codeview_color_comment = IM_COL32(88, 110, 117, 255);
            iris->codeview_color_mnemonic = IM_COL32(211, 167, 30, 255);
            iris->codeview_color_number = IM_COL32(138, 143, 226, 255);
            iris->codeview_color_register = IM_COL32(68, 169, 240, 255);
            iris->codeview_color_other = IM_COL32(89, 89, 89, 255);
            iris->codeview_color_background = IM_COL32(0, 43, 54, 255);
            iris->codeview_color_highlight = IM_COL32(7, 54, 66, 255);
        } break;

        case IRIS_CODEVIEW_COLOR_SCHEME_SOLARIZED_LIGHT: {
            iris->codeview_color_text = IM_COL32(101, 123, 131, 255);
            iris->codeview_color_comment = IM_COL32(147, 161, 161, 255);
            iris->codeview_color_mnemonic = IM_COL32(147, 101, 21, 255);
            iris->codeview_color_number = IM_COL32(101, 123, 179, 255);
            iris->codeview_color_register = IM_COL32(38, 139, 210, 255);
            iris->codeview_color_other = IM_COL32(88, 110, 117, 255);
            iris->codeview_color_background = IM_COL32(253, 246, 227, 255);
            iris->codeview_color_highlight = IM_COL32(238, 232, 213, 255);
        } break;

        case IRIS_CODEVIEW_COLOR_SCHEME_ONE_DARK_PRO: {
            iris->codeview_color_text = IM_COL32(171, 178, 191, 255);
            iris->codeview_color_comment = IM_COL32(92, 99, 112, 255);
            iris->codeview_color_mnemonic = IM_COL32(198, 120, 221, 255);
            iris->codeview_color_number = IM_COL32(209, 154, 102, 255);
            iris->codeview_color_register = IM_COL32(97, 175, 239, 255);
            iris->codeview_color_other = IM_COL32(171, 178, 191, 255);
            iris->codeview_color_background = IM_COL32(40, 44, 52, 255);
            iris->codeview_color_highlight = IM_COL32(60, 64, 72, 255);
        } break;

        case IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_LATTE: {
            iris->codeview_color_text = IM_COL32(76, 79, 105, 255);
            iris->codeview_color_comment = IM_COL32(124, 127, 147, 255);
            iris->codeview_color_mnemonic = IM_COL32(136, 57, 239, 255);
            iris->codeview_color_number = IM_COL32(254, 100, 11, 255);
            iris->codeview_color_register = IM_COL32(4, 165, 229, 255);
            iris->codeview_color_other = IM_COL32(114, 135, 253, 255);
            iris->codeview_color_background = IM_COL32(239, 241, 245, 255);
            iris->codeview_color_highlight = IM_COL32(204, 208, 218, 255);
        } break;

        case IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_FRAPPE: {
            iris->codeview_color_text = IM_COL32(198, 208, 245, 255);
            iris->codeview_color_comment = IM_COL32(148, 156, 187, 255);
            iris->codeview_color_mnemonic = IM_COL32(202, 158, 230, 255);
            iris->codeview_color_number = IM_COL32(239, 159, 118, 255);
            iris->codeview_color_register = IM_COL32(153, 209, 219, 255);
            iris->codeview_color_other = IM_COL32(186, 187, 241, 255);
            iris->codeview_color_background = IM_COL32(48, 52, 70, 255);
            iris->codeview_color_highlight = IM_COL32(81, 87, 109, 255);
        } break;

        case IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_MACCHIATO: {
            iris->codeview_color_text = IM_COL32(174, 178, 208, 255);
            iris->codeview_color_comment = IM_COL32(134, 138, 162, 255);
            iris->codeview_color_mnemonic = IM_COL32(190, 132, 255, 255);
            iris->codeview_color_number = IM_COL32(245, 142, 110, 255);
            iris->codeview_color_register = IM_COL32(125, 182, 191, 255);
            iris->codeview_color_other = IM_COL32(166, 167, 222, 255);
            iris->codeview_color_background = IM_COL32(58, 60, 79, 255);
            iris->codeview_color_highlight = IM_COL32(97, 100, 120, 255);
        } break;

        case IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_MOCHA: {
            iris->codeview_color_text = IM_COL32(205, 214, 244, 255);
            iris->codeview_color_comment = IM_COL32(145, 151, 181, 255);
            iris->codeview_color_mnemonic = IM_COL32(220, 162, 255, 255);
            iris->codeview_color_number = IM_COL32(248, 159, 128, 255);
            iris->codeview_color_register = IM_COL32(159, 226, 235, 255);
            iris->codeview_color_other = IM_COL32(189, 191, 248, 255);
            iris->codeview_color_background = IM_COL32(46, 49, 64, 255);
            iris->codeview_color_highlight = IM_COL32(76, 80, 100, 255);
        } break;
    }
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
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &iris->descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = VK_NULL_HANDLE;

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
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = 0;
    dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    VkRenderPass render_pass = VK_NULL_HANDLE;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

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
    VkDescriptorSetLayoutBinding sampler_layout_binding = {};
    sampler_layout_binding.binding = 0;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.pImmutableSamplers = nullptr;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // VkDescriptorBindingFlags flags = {};
    // flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    // VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags = {};
    // binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    // binding_flags.pNext = nullptr;
    // binding_flags.pBindingFlags = &flags;
    // binding_flags.bindingCount = 1;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &sampler_layout_binding;
    // layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    // layout_info.pNext = &binding_flags;

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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    if (iris->imgui_enable_viewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoDecoration = false;
        io.ConfigViewportsNoAutoMerge = true;
    }

    io.IniFilename = iris->ini_path.c_str();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(iris->main_scale);
    style.FontScaleDpi = iris->main_scale;
    style.FontScaleMain = iris->ui_scale;

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

    set_theme(iris, iris->theme, false);
    set_codeview_scheme(iris, iris->codeview_color_scheme);

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

    auto load_texture = [iris](const stbi_uc* data, size_t size) -> texture {
        int x, y, c;

        stbi_uc* buf = stbi_load_from_memory(data, size, &x, &y, &c, 4);

        auto tex = vulkan::upload_texture(iris, buf, x, y, c);

        stbi_image_free(buf);

        return tex;
    };

    iris->ps1_memory_card_icon = load_texture(g_ps1_memory_card_icon_data, g_ps1_memory_card_icon_size);
    iris->ps2_memory_card_icon = load_texture(g_ps2_memory_card_icon_data, g_ps2_memory_card_icon_size);
    iris->pocketstation_icon = load_texture(g_pocketstation_icon_data, g_pocketstation_icon_size);
    iris->dualshock2_icon = load_texture(g_dualshock2_icon_data, g_dualshock2_icon_size);
    iris->iris_icon = load_texture(g_iris_icon_data, g_iris_icon_size);

    return true;
}

void cleanup(iris::instance* iris) {
    vkQueueWaitIdle(iris->queue);
    vkDeviceWaitIdle(iris->device);

    vulkan::free_texture(iris, iris->ps1_memory_card_icon);
    vulkan::free_texture(iris, iris->ps2_memory_card_icon);
    vulkan::free_texture(iris, iris->pocketstation_icon);
    vulkan::free_texture(iris, iris->dualshock2_icon);
    vulkan::free_texture(iris, iris->iris_icon);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    ImGui_ImplVulkanH_DestroyWindow(iris->instance, iris->device, &iris->main_window_data, VK_NULL_HANDLE);
}

bool render_frame(iris::instance* iris, ImDrawData* draw_data) {
    if (iris->swapchain_rebuild)
        return true;

    ImGui_ImplVulkanH_Window* wd = &iris->main_window_data;
    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];

    if (vkWaitForFences(iris->device, 1, &fd->Fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to wait for fence\n");

        return false;
    }

    if (vkResetFences(iris->device, 1, &fd->Fence) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to reset fence\n");

        return false;
    }

    VkSemaphore acquire_semaphore = wd->FrameSemaphores[wd->FrameIndex].ImageAcquiredSemaphore;

    uint32_t image_index;

    VkResult err;

    err = vkAcquireNextImageKHR(
        iris->device,
        wd->Swapchain,
        UINT64_MAX,
        acquire_semaphore,
        VK_NULL_HANDLE,
        &image_index
    );

    VkSemaphore submit_semaphore = wd->FrameSemaphores[image_index].RenderCompleteSemaphore;

    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        iris->swapchain_rebuild = true;

        return true;
    } else if (err != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to acquire next image\n");

        return false;
    }

    if (vkResetCommandPool(iris->device, fd->CommandPool, 0) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to reset command pool\n");

        return false;
    }

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(fd->CommandBuffer, &begin_info) != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to begin command buffer\n");
        
        return false;
    }

    render::render_frame(iris, fd->CommandBuffer, fd->Framebuffer);

    {
        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = wd->RenderPass;
        render_pass_info.framebuffer = fd->Framebuffer;
        render_pass_info.renderArea.extent.width = wd->Width;
        render_pass_info.renderArea.extent.height = wd->Height;
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &iris->clear_value;

        vkCmdBeginRenderPass(fd->CommandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);

    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &acquire_semaphore;
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &fd->CommandBuffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &submit_semaphore;

        if (vkEndCommandBuffer(fd->CommandBuffer) != VK_SUCCESS) {
            fprintf(stderr, "imgui: Failed to end command buffer\n");
        
            return false;
        }

        if (vkQueueSubmit(iris->queue, 1, &submit_info, fd->Fence) != VK_SUCCESS) {
            fprintf(stderr, "imgui: Failed to submit queue\n");
        
            return false;
        }
    }

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &submit_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &wd->Swapchain;
    present_info.pImageIndices = &wd->FrameIndex;

    err = vkQueuePresentKHR(iris->queue, &present_info);

    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        iris->swapchain_rebuild = true;

        return true;
    } else if (err != VK_SUCCESS) {
        fprintf(stderr, "imgui: Failed to acquire next image\n");

        return false;
    }

    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
    wd->FrameIndex = (wd->FrameIndex + 1) % wd->Frames.size();

    return true;
}

bool BeginEx(const char* name, bool* p_open, ImGuiWindowFlags flags) {
    ImGui::SetNextWindowSize(ImVec2(600.0, 600.0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(50.0, 50.0), ImGuiCond_FirstUseEver);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        flags |= ImGuiWindowFlags_NoTitleBar;
    }

    return ImGui::Begin(name, p_open, flags);
}

}
