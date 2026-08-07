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

constexpr unsigned char g_inter_data[] = {
#embed "../res/Inter-Regular.ttf"
};
constexpr unsigned int g_inter_size = sizeof(g_inter_data);

constexpr unsigned char g_inter_semibold_data[] = {
#embed "../res/Inter-SemiBold.ttf"
};
constexpr unsigned int g_inter_semibold_size = sizeof(g_inter_semibold_data);

constexpr unsigned char g_inter_black_data[] = {
#embed "../res/Inter-Black.ttf"
};
constexpr unsigned int g_inter_black_size = sizeof(g_inter_black_data);

constexpr unsigned char g_symbols_data[] = {
#embed "../res/MaterialSymbolsRounded.ttf"
};
constexpr unsigned int g_symbols_size = sizeof(g_symbols_data);

constexpr unsigned char g_mono_data[] = {
#embed "../res/JetBrainsMono.ttf"
};
constexpr unsigned int g_mono_size = sizeof(g_mono_data);

constexpr unsigned char g_ps1_memory_card_icon_data[] = {
#embed "../res/ps1_mcd.png"
};
constexpr unsigned int g_ps1_memory_card_icon_size = sizeof(g_ps1_memory_card_icon_data);

constexpr unsigned char g_ps2_memory_card_icon_data[] = {
#embed "../res/ps2_mcd.png"
};
constexpr unsigned int g_ps2_memory_card_icon_size = sizeof(g_ps2_memory_card_icon_data);

constexpr unsigned char g_dualshock2_icon_data[] = {
#embed "../res/ds2.png"
};
constexpr unsigned int g_dualshock2_icon_size = sizeof(g_dualshock2_icon_data);

constexpr unsigned char g_pocketstation_icon_data[] = {
#embed "../res/pocketstation.png"
};
constexpr unsigned int g_pocketstation_icon_size = sizeof(g_pocketstation_icon_data);

constexpr unsigned char g_iris_icon_data[] = {
#embed "../res/iris.png"
};
constexpr unsigned int g_iris_icon_size = sizeof(g_iris_icon_data);

constexpr unsigned char g_vertex_shader_data[] = {
#embed "../shaders/vertex.spv"
};
constexpr unsigned int g_vertex_shader_size = sizeof(g_vertex_shader_data);

constexpr unsigned char g_fragment_shader_data[] = {
#embed "../shaders/fragment.spv"
};
constexpr unsigned int g_fragment_shader_size = sizeof(g_fragment_shader_data);

#include "stb_image.h"

// #define VOLK_IMPLEMENTATION
#include <volk.h>

namespace iris::imgui {

static constexpr uint32_t DESCRIPTOR_SET_RING_SIZE = 8;

static const ImWchar g_icon_range[] = { ICON_MIN_MS, ICON_MAX_16_MS, 0 };

static bool setup_vulkan_window(Instance* iris, ImGui_ImplVulkanH_Window* wd, int width, int height, bool vsync) {
    wd->Surface = iris->vk.surface;

    VkAttachmentDescription attachment = {};
    attachment.format = wd->SurfaceFormat.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    wd->AttachmentDesc = attachment;

    // Check for WSI support
    VkBool32 res;

    vkGetPhysicalDeviceSurfaceSupportKHR(iris->vk.physical_device, iris->vk.queue_family, wd->Surface, &res);

    if (!res) {
        iris_error(&iris->log.imgui, "No WSI support on physical device");
        
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
        iris->vk.physical_device,
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
        iris->vk.physical_device,
        wd->Surface,
        present_modes.data(),
        present_modes.size()
    );

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(iris->vk.min_image_count >= 2);

    ImGui_ImplVulkanH_CreateOrResizeWindow(
        iris->vk.instance,
        iris->vk.physical_device,
        iris->vk.device,
        wd,
        iris->vk.queue_family,
        VK_NULL_HANDLE,
        width, height,
        iris->vk.min_image_count,
        0
    );

    return true;
}

void set_vsync(Instance* iris, bool vsync) {
    std::vector <VkPresentModeKHR> present_modes;

    if (vsync) {
        present_modes.push_back(VK_PRESENT_MODE_FIFO_KHR);
    } else {
        present_modes.push_back(VK_PRESENT_MODE_MAILBOX_KHR);
        present_modes.push_back(VK_PRESENT_MODE_IMMEDIATE_KHR);
        present_modes.push_back(VK_PRESENT_MODE_FIFO_KHR);
    }
 
    iris->vk.main_window_data.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        iris->vk.physical_device,
        iris->vk.main_window_data.Surface,
        present_modes.data(),
        present_modes.size()
    );

    render::refresh(iris);
}

bool setup_fonts(Instance* iris, ImGuiIO& io) {
    io.Fonts->AddFontDefault();

    ImFontConfig font_config;
    font_config.MergeMode = true;
    font_config.GlyphMinAdvanceX = 13.0f;
    font_config.GlyphOffset = ImVec2(0.0f, 4.0f);
    font_config.FontDataOwnedByAtlas = false;

    ImFontConfig config_no_own;
    config_no_own.FontDataOwnedByAtlas = false;

    ImFontConfig config_ui = config_no_own;
    config_ui.GlyphExcludeRanges = g_icon_range;

    iris->ui.font_small_code = io.Fonts->AddFontFromMemoryTTF((void*)g_mono_data, g_mono_size, 12.0F, &config_no_own);
    iris->ui.font_code       = io.Fonts->AddFontFromMemoryTTF((void*)g_mono_data, g_mono_size, 16.0F, &config_no_own);
    iris->ui.font_small      = io.Fonts->AddFontFromMemoryTTF((void*)g_inter_data, g_inter_size, 12.0F, &config_ui);
    iris->ui.font_label      = io.Fonts->AddFontFromMemoryTTF((void*)g_inter_semibold_data, g_inter_semibold_size, 11.0F, &config_ui);
    iris->ui.font_heading    = io.Fonts->AddFontFromMemoryTTF((void*)g_inter_semibold_data, g_inter_semibold_size, 20.0F, &config_ui);
    iris->ui.font_body       = io.Fonts->AddFontFromMemoryTTF((void*)g_inter_data, g_inter_size, 16.0F, &config_ui);
    iris->ui.font_icons      = io.Fonts->AddFontFromMemoryTTF((void*)g_symbols_data, g_symbols_size, 20.0F, &font_config, g_icon_range);
    iris->ui.font_icons_big  = io.Fonts->AddFontFromMemoryTTF((void*)g_symbols_data, g_symbols_size, 50.0F, &config_no_own, g_icon_range);
    iris->ui.font_black      = io.Fonts->AddFontFromMemoryTTF((void*)g_inter_black_data, g_inter_black_size, 30.0F, &config_ui);

    if (!iris->ui.font_small_code ||
        !iris->ui.font_code ||
        !iris->ui.font_small ||
        !iris->ui.font_label ||
        !iris->ui.font_heading ||
        !iris->ui.font_body ||
        !iris->ui.font_icons ||
        !iris->ui.font_icons_big ||
        !iris->ui.font_black) {
        return false;
    }

    io.FontDefault = iris->ui.font_icons;

    return true;
}

void section(Instance* iris, const char* label) {
    using namespace ImGui;

    constexpr float size = 11.0f;
    constexpr float tracking = 1.4f;

    ImFont* font = iris->ui.font_label;
    ImFontBaked* baked = font->GetFontBaked(size);
    ImDrawList* dl = GetWindowDrawList();

    ImVec2 pos = GetCursorScreenPos();

    pos.y += GetStyle().ItemSpacing.y;

    ImU32 text_col = GetColorU32(ImGuiCol_TextDisabled);
    float x = pos.x;

    for (const char* p = label; *p; p++) {
        unsigned int c = (unsigned char)*p;

        if (c >= 'a' && c <= 'z')
            c -= 32;

        ImFontGlyph* glyph = baked->FindGlyph((ImWchar)c);

        if (!glyph)
            continue;

        font->RenderChar(dl, size, ImVec2(x, pos.y), text_col, (ImWchar)c);

        x += glyph->AdvanceX + tracking;
    }

    float avail = GetContentRegionAvail().x;
    float rule_x = x + 8.0f;
    float rule_end = pos.x + avail;

    if (rule_end > rule_x) {
        dl->AddLine(
            ImVec2(rule_x, pos.y + size * 0.5f),
            ImVec2(rule_end, pos.y + size * 0.5f),
            GetColorU32(ImGuiCol_Separator)
        );
    }

    Dummy(ImVec2(0.0f, size + GetStyle().ItemSpacing.y));
}

constexpr float menu_rounding = 5.0f;

static ImDrawListSplitter menu_splitter;

static ImDrawList* begin_menu_highlight() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    menu_splitter.Split(draw_list, 2);
    menu_splitter.SetCurrentChannel(draw_list, 1);

    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

    return draw_list;
}

static void end_menu_highlight(ImDrawList* draw_list, bool selected) {
    ImGui::PopStyleColor(3);

    bool hovered = ImGui::IsItemHovered();

    if (hovered || selected) {
        menu_splitter.SetCurrentChannel(draw_list, 0);

        draw_list->AddRectFilled(
            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
            ImGui::GetColorU32(hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header),
            menu_rounding
        );
    }

    menu_splitter.Merge(draw_list);
}

static bool menu_item_rounded(const char* label, const char* shortcut, bool selected, bool* p_selected, bool enabled) {
    ImDrawList* draw_list = begin_menu_highlight();

    bool pressed = p_selected
        ? ImGui::MenuItem(label, shortcut, p_selected, enabled)
        : ImGui::MenuItem(label, shortcut, selected, enabled);

    end_menu_highlight(draw_list, p_selected ? *p_selected : selected);

    return pressed;
}

bool MenuItem(const char* label, const char* shortcut, bool selected, bool enabled) {
    return menu_item_rounded(label, shortcut, selected, nullptr, enabled);
}

bool MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled) {
    return menu_item_rounded(label, shortcut, false, p_selected, enabled);
}

static bool selectable_rounded(const char* label, bool selected, bool* p_selected, ImGuiSelectableFlags flags, const ImVec2& size) {
    ImDrawList* draw_list = begin_menu_highlight();

    bool pressed = p_selected
        ? ImGui::Selectable(label, p_selected, flags, size)
        : ImGui::Selectable(label, selected, flags, size);

    end_menu_highlight(draw_list, p_selected ? *p_selected : selected);

    return pressed;
}

bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size) {
    return selectable_rounded(label, selected, nullptr, flags, size);
}

bool Selectable(const char* label, bool* p_selected, ImGuiSelectableFlags flags, const ImVec2& size) {
    return selectable_rounded(label, false, p_selected, flags, size);
}

bool BeginMenu(const char* label, bool enabled) {
    ImDrawList* draw_list = begin_menu_highlight();

    bool open = ImGui::BeginMenu(label, enabled);

    end_menu_highlight(draw_list, open);

    return open;
}

namespace granite {

constexpr ImVec4 rgb(int hex, float a = 1.0f) {
    return ImVec4(
        ((hex >> 16) & 0xff) / 255.0f,
        ((hex >>  8) & 0xff) / 255.0f,
        ( hex        & 0xff) / 255.0f,
        a
    );
}

constexpr ImVec4 alpha(ImVec4 c, float a) {
    return ImVec4(c.x, c.y, c.z, a);
}

struct Palette {
    ImVec4 app, window, popup, frame, hover, active, border;
    ImVec4 text, text_dim;
    ImVec4 accent, accent_hi, link;
    ImVec4 scroll, scroll_hover, scroll_active;
    ImVec4 alt_row, grip, table_line;
};

constexpr Palette dark = {
    .app          = rgb(0x0b0b0d),
    .window       = rgb(0x131316),
    .popup        = rgb(0x19191d),
    .frame        = rgb(0x202025),
    .hover        = rgb(0x2b2b32),
    .active       = rgb(0x373740),
    .border       = rgb(0x28282e),
    .text         = rgb(0xe6e6ec),
    .text_dim     = rgb(0x8b8b96),
    .accent       = rgb(0x8b5cf6),
    .accent_hi    = rgb(0x9d75f8),
    .link         = rgb(0x60a5fa),
    .scroll       = rgb(0xffffff, 0.10f),
    .scroll_hover = rgb(0xffffff, 0.18f),
    .scroll_active= rgb(0xffffff, 0.26f),
    .alt_row      = rgb(0xffffff, 0.022f),
    .grip         = rgb(0xffffff, 0.06f),
    .table_line   = rgb(0xffffff, 0.04f)
};

// Surfaces invert but the ramp direction does too: on light, hovering a
// control darkens it. The accent drops two steps so it still carries
// contrast against a near-white background.
constexpr Palette light = {
    .app          = rgb(0xe8e8ea),
    .window       = rgb(0xf7f7f8),
    .popup        = rgb(0xffffff),
    .frame        = rgb(0xecedef),
    .hover        = rgb(0xe0e0e5),
    .active       = rgb(0xd2d2d9),
    .border       = rgb(0xdcdce1),
    .text         = rgb(0x1c1c21),
    .text_dim     = rgb(0x6b6b75),
    .accent       = rgb(0x7c3aed),
    .accent_hi    = rgb(0x6d28d9),
    .link         = rgb(0x2563eb),
    .scroll       = rgb(0x000000, 0.16f),
    .scroll_hover = rgb(0x000000, 0.26f),
    .scroll_active= rgb(0x000000, 0.36f),
    .alt_row      = rgb(0x000000, 0.028f),
    .grip         = rgb(0x000000, 0.10f),
    .table_line   = rgb(0x000000, 0.06f)
};

}

static void apply_granite(ImGuiStyle& style, const granite::Palette& p) {
    using granite::alpha;

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text]                   = p.text;
    colors[ImGuiCol_TextDisabled]           = p.text_dim;
    colors[ImGuiCol_WindowBg]               = p.window;
    colors[ImGuiCol_ChildBg]                = alpha(p.window, 0.00f);
    colors[ImGuiCol_PopupBg]                = p.popup;
    colors[ImGuiCol_Border]                 = p.border;
    colors[ImGuiCol_BorderShadow]           = alpha(p.window, 0.00f);
    colors[ImGuiCol_FrameBg]                = p.frame;
    colors[ImGuiCol_FrameBgHovered]         = p.hover;
    colors[ImGuiCol_FrameBgActive]          = p.active;
    colors[ImGuiCol_TitleBg]                = p.window;
    colors[ImGuiCol_TitleBgActive]          = p.window;
    colors[ImGuiCol_TitleBgCollapsed]       = p.app;
    colors[ImGuiCol_MenuBarBg]              = p.window;
    colors[ImGuiCol_ScrollbarBg]            = alpha(p.window, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]          = p.scroll;
    colors[ImGuiCol_ScrollbarGrabHovered]   = p.scroll_hover;
    colors[ImGuiCol_ScrollbarGrabActive]    = p.scroll_active;
    colors[ImGuiCol_CheckMark]              = p.accent;
    colors[ImGuiCol_SliderGrab]             = p.accent;
    colors[ImGuiCol_SliderGrabActive]       = p.accent_hi;
    colors[ImGuiCol_Button]                 = p.frame;
    colors[ImGuiCol_ButtonHovered]          = p.hover;
    colors[ImGuiCol_ButtonActive]           = p.active;
    colors[ImGuiCol_Header]                 = alpha(p.accent, 0.20f);
    colors[ImGuiCol_HeaderHovered]          = alpha(p.accent, 0.28f);
    colors[ImGuiCol_HeaderActive]           = alpha(p.accent, 0.38f);
    colors[ImGuiCol_Separator]              = p.border;
    colors[ImGuiCol_SeparatorHovered]       = p.accent;
    colors[ImGuiCol_SeparatorActive]        = p.accent_hi;
    colors[ImGuiCol_ResizeGrip]             = p.grip;
    colors[ImGuiCol_ResizeGripHovered]      = alpha(p.accent, 0.50f);
    colors[ImGuiCol_ResizeGripActive]       = p.accent;
    colors[ImGuiCol_InputTextCursor]        = p.accent;
    colors[ImGuiCol_Tab]                    = alpha(p.window, 0.00f);
    colors[ImGuiCol_TabHovered]             = p.hover;
    colors[ImGuiCol_TabSelected]            = alpha(p.accent, 0.16f);
    colors[ImGuiCol_TabSelectedOverline]    = p.accent;
    colors[ImGuiCol_TabDimmed]              = alpha(p.window, 0.00f);
    colors[ImGuiCol_TabDimmedSelected]      = p.popup;
    colors[ImGuiCol_TabDimmedSelectedOverline] = alpha(p.accent, 0.00f);
    colors[ImGuiCol_DockingPreview]         = alpha(p.accent, 0.30f);
    colors[ImGuiCol_DockingEmptyBg]         = p.app;
    colors[ImGuiCol_PlotLines]              = p.accent;
    colors[ImGuiCol_PlotLinesHovered]       = p.accent_hi;
    colors[ImGuiCol_PlotHistogram]          = p.accent;
    colors[ImGuiCol_PlotHistogramHovered]   = p.accent_hi;
    colors[ImGuiCol_TableHeaderBg]          = p.popup;
    colors[ImGuiCol_TableBorderStrong]      = p.border;
    colors[ImGuiCol_TableBorderLight]       = p.table_line;
    colors[ImGuiCol_TableRowBg]             = alpha(p.window, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = p.alt_row;
    colors[ImGuiCol_TextLink]               = p.link;
    colors[ImGuiCol_TextSelectedBg]         = alpha(p.accent, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = p.accent;
    colors[ImGuiCol_NavCursor]              = p.accent;
    colors[ImGuiCol_NavWindowingHighlight]  = alpha(p.text, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = granite::rgb(0x000000, 0.45f);
    colors[ImGuiCol_ModalWindowDimBg]       = granite::rgb(0x000000, 0.55f);
}

static void set_clear_color(Instance* iris, const ImVec4& c) {
    iris->vk.clear_value.color.float32[0] = c.x;
    iris->vk.clear_value.color.float32[1] = c.y;
    iris->vk.clear_value.color.float32[2] = c.z;
    iris->vk.clear_value.color.float32[3] = 1.00f;
}

// Granite's original geometry: tighter, flatter, no window borders
static void set_classic_geometry(ImGuiStyle& style) {
    style.WindowPadding           = ImVec2(8.0, 8.0);
    style.FramePadding            = ImVec2(5.0, 5.0);
    style.ItemSpacing             = ImVec2(8.0, 6.0);
    style.WindowBorderSize        = 0;
    style.ChildBorderSize         = 0;
    style.PopupBorderSize         = 0;
    style.WindowRounding          = 6;
    style.ChildRounding           = 4;
    style.FrameRounding           = 4;
    style.PopupRounding           = 4;
    style.ScrollbarRounding       = 9;
    style.GrabRounding            = 2;
    style.TabRounding             = 4;
    style.WindowTitleAlign        = ImVec2(0.5, 0.5);
    style.DockingSeparatorSize    = 0;
    style.SeparatorTextPadding    = ImVec2(20, 0);
}

void set_theme(Instance* iris, int theme, bool set_bg_color) {
    // Init 'Granite' theme
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding           = ImVec2(12.0, 10.0);
    style.FramePadding            = ImVec2(9.0, 6.0);
    style.ItemSpacing             = ImVec2(8.0, 8.0);
    style.ItemInnerSpacing        = ImVec2(8.0, 6.0);
    style.CellPadding             = ImVec2(8.0, 5.0);
    style.TouchExtraPadding       = ImVec2(0.0, 0.0);
    style.IndentSpacing           = 20.0;
    style.ScrollbarSize           = 11.0;
    style.GrabMinSize             = 9.0;
    style.WindowBorderSize        = 1;
    style.ChildBorderSize         = 1;
    style.FrameBorderSize         = 1;
    style.PopupBorderSize         = 1;
    style.TabBorderSize           = 0;
    style.TabBarBorderSize        = 0;
    style.WindowRounding          = 8;
    style.ChildRounding           = 8;
    style.FrameRounding           = 6;
    style.PopupRounding           = 8;
    style.ScrollbarRounding       = 6;
    style.GrabRounding            = 6;
    style.TabRounding             = 6;
    style.WindowTitleAlign        = ImVec2(0.0, 0.5);
    style.DockingSeparatorSize    = 2;
    style.SeparatorTextBorderSize = 1;
    style.SeparatorTextAlign      = ImVec2(0.0, 0.5);
    style.SeparatorTextPadding    = ImVec2(16, 8);

    // Use ImGui's default dark style as a base for our own style
    ImGui::StyleColorsDark();

    switch (theme) {
        case Theme::GRANITE_NEO: {
            apply_granite(style, granite::dark);

            if (!set_bg_color) break;

            set_clear_color(iris, granite::dark.app);
        } break;

        case Theme::GRANITE_NEO_LIGHT: {
            // Light base so anything ImGui adds later doesn't come through dark
            ImGui::StyleColorsLight();

            apply_granite(style, granite::light);

            if (!set_bg_color) break;

            set_clear_color(iris, granite::light.app);
        } break;

        case Theme::GRANITE: {
            set_classic_geometry(style);

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

            iris->vk.clear_value.color.float32[0] = 0.11f;
            iris->vk.clear_value.color.float32[1] = 0.11f;
            iris->vk.clear_value.color.float32[2] = 0.11f;
            iris->vk.clear_value.color.float32[3] = 1.00f;
        } break;

        case Theme::IMGUI_DARK: {
            ImGui::StyleColorsDark();

            if (!set_bg_color) break;

            iris->vk.clear_value.color.float32[0] = 0.11f;
            iris->vk.clear_value.color.float32[1] = 0.11f;
            iris->vk.clear_value.color.float32[2] = 0.11f;
            iris->vk.clear_value.color.float32[3] = 1.00f;
        } break;

        case Theme::IMGUI_LIGHT: {
            ImGui::StyleColorsLight();

            if (!set_bg_color) break;

            iris->vk.clear_value.color.float32[0] = 0.89f;
            iris->vk.clear_value.color.float32[1] = 0.89f;
            iris->vk.clear_value.color.float32[2] = 0.89f;
            iris->vk.clear_value.color.float32[3] = 1.00f;
        } break;

        case Theme::IMGUI_CLASSIC: {
            ImGui::StyleColorsClassic();

            if (!set_bg_color) break;

            iris->vk.clear_value.color.float32[0] = 0.11f;
            iris->vk.clear_value.color.float32[1] = 0.11f;
            iris->vk.clear_value.color.float32[2] = 0.11f;
            iris->vk.clear_value.color.float32[3] = 1.00f;
        } break;

        case Theme::CHERRY: {
            // cherry colors, 3 intensities
            #define COL_HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
            #define COL_MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
            #define COL_LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
            // backgrounds
            #define COL_BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
            // text
            #define COL_TEXT(v) ImVec4(0.860f, 0.930f, 0.890f, v)

            auto &style = ImGui::GetStyle();
            style.Colors[ImGuiCol_Text]                  = COL_TEXT(0.78f);
            style.Colors[ImGuiCol_TextDisabled]          = COL_TEXT(0.28f);
            style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
            style.Colors[ImGuiCol_ChildBg]               = COL_BG( 0.58f);
            style.Colors[ImGuiCol_PopupBg]               = COL_BG( 0.9f);
            style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
            style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[ImGuiCol_FrameBg]               = COL_BG( 1.00f);
            style.Colors[ImGuiCol_FrameBgHovered]        = COL_MED( 0.78f);
            style.Colors[ImGuiCol_FrameBgActive]         = COL_MED( 1.00f);
            style.Colors[ImGuiCol_TitleBg]               = COL_LOW( 1.00f);
            style.Colors[ImGuiCol_TitleBgActive]         = COL_HI( 1.00f);
            style.Colors[ImGuiCol_TitleBgCollapsed]      = COL_BG( 0.75f);
            style.Colors[ImGuiCol_MenuBarBg]             = COL_BG( 0.47f);
            style.Colors[ImGuiCol_ScrollbarBg]           = COL_BG( 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrabHovered]  = COL_MED( 0.78f);
            style.Colors[ImGuiCol_ScrollbarGrabActive]   = COL_MED( 1.00f);
            style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
            style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_Button]                = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
            style.Colors[ImGuiCol_ButtonHovered]         = COL_MED( 0.86f);
            style.Colors[ImGuiCol_ButtonActive]          = COL_MED( 1.00f);
            style.Colors[ImGuiCol_Header]                = COL_MED( 0.76f);
            style.Colors[ImGuiCol_HeaderHovered]         = COL_MED( 0.86f);
            style.Colors[ImGuiCol_HeaderActive]          = COL_HI( 1.00f);
            style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
            style.Colors[ImGuiCol_ResizeGripHovered]     = COL_MED( 0.78f);
            style.Colors[ImGuiCol_ResizeGripActive]      = COL_MED( 1.00f);
            style.Colors[ImGuiCol_PlotLines]             = COL_TEXT(0.63f);
            style.Colors[ImGuiCol_PlotLinesHovered]      = COL_MED( 1.00f);
            style.Colors[ImGuiCol_PlotHistogram]         = COL_TEXT(0.63f);
            style.Colors[ImGuiCol_PlotHistogramHovered]  = COL_MED( 1.00f);
            style.Colors[ImGuiCol_TextSelectedBg]        = COL_MED( 0.43f);
            style.Colors[ImGuiCol_ModalWindowDimBg]      = COL_BG( 0.73f);

            #undef COL_HI
            #undef COL_MED
            #undef COL_LOW
            #undef COL_BG
            #undef COL_TEXT

            if (!set_bg_color) break;

            iris->vk.clear_value.color.float32[0] = 0.20f * 0.5f;
            iris->vk.clear_value.color.float32[1] = 0.22f * 0.5f;
            iris->vk.clear_value.color.float32[2] = 0.27f * 0.5f;
            iris->vk.clear_value.color.float32[3] = 1.00f;
        } break;

        case Theme::SOURCE: {
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

            iris->vk.clear_value.color.float32[0] = 0.13f;
            iris->vk.clear_value.color.float32[1] = 0.15f;
            iris->vk.clear_value.color.float32[2] = 0.11f;
            iris->vk.clear_value.color.float32[3] = 1.00f;
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

void set_codeview_scheme(Instance* iris, int scheme) {
    switch (scheme) {
        default: case CodeviewColorScheme::SOLARIZED_DARK: {
            iris->ui.codeview_color_text = IM_COL32(131, 148, 150, 255);
            iris->ui.codeview_color_comment = IM_COL32(88, 110, 117, 255);
            iris->ui.codeview_color_mnemonic = IM_COL32(211, 167, 30, 255);
            iris->ui.codeview_color_number = IM_COL32(138, 143, 226, 255);
            iris->ui.codeview_color_register = IM_COL32(68, 169, 240, 255);
            iris->ui.codeview_color_other = IM_COL32(89, 89, 89, 255);
            iris->ui.codeview_color_background = IM_COL32(0, 43, 54, 255);
            iris->ui.codeview_color_highlight = IM_COL32(7, 54, 66, 255);
        } break;

        case CodeviewColorScheme::SOLARIZED_LIGHT: {
            iris->ui.codeview_color_text = IM_COL32(101, 123, 131, 255);
            iris->ui.codeview_color_comment = IM_COL32(147, 161, 161, 255);
            iris->ui.codeview_color_mnemonic = IM_COL32(147, 101, 21, 255);
            iris->ui.codeview_color_number = IM_COL32(101, 123, 179, 255);
            iris->ui.codeview_color_register = IM_COL32(38, 139, 210, 255);
            iris->ui.codeview_color_other = IM_COL32(88, 110, 117, 255);
            iris->ui.codeview_color_background = IM_COL32(253, 246, 227, 255);
            iris->ui.codeview_color_highlight = IM_COL32(238, 232, 213, 255);
        } break;

        case CodeviewColorScheme::ONE_DARK_PRO: {
            iris->ui.codeview_color_text = IM_COL32(171, 178, 191, 255);
            iris->ui.codeview_color_comment = IM_COL32(92, 99, 112, 255);
            iris->ui.codeview_color_mnemonic = IM_COL32(198, 120, 221, 255);
            iris->ui.codeview_color_number = IM_COL32(209, 154, 102, 255);
            iris->ui.codeview_color_register = IM_COL32(97, 175, 239, 255);
            iris->ui.codeview_color_other = IM_COL32(171, 178, 191, 255);
            iris->ui.codeview_color_background = IM_COL32(40, 44, 52, 255);
            iris->ui.codeview_color_highlight = IM_COL32(60, 64, 72, 255);
        } break;

        case CodeviewColorScheme::CATPPUCCIN_LATTE: {
            iris->ui.codeview_color_text = IM_COL32(76, 79, 105, 255);
            iris->ui.codeview_color_comment = IM_COL32(124, 127, 147, 255);
            iris->ui.codeview_color_mnemonic = IM_COL32(136, 57, 239, 255);
            iris->ui.codeview_color_number = IM_COL32(254, 100, 11, 255);
            iris->ui.codeview_color_register = IM_COL32(4, 165, 229, 255);
            iris->ui.codeview_color_other = IM_COL32(114, 135, 253, 255);
            iris->ui.codeview_color_background = IM_COL32(239, 241, 245, 255);
            iris->ui.codeview_color_highlight = IM_COL32(204, 208, 218, 255);
        } break;

        case CodeviewColorScheme::CATPPUCCIN_FRAPPE: {
            iris->ui.codeview_color_text = IM_COL32(198, 208, 245, 255);
            iris->ui.codeview_color_comment = IM_COL32(148, 156, 187, 255);
            iris->ui.codeview_color_mnemonic = IM_COL32(202, 158, 230, 255);
            iris->ui.codeview_color_number = IM_COL32(239, 159, 118, 255);
            iris->ui.codeview_color_register = IM_COL32(153, 209, 219, 255);
            iris->ui.codeview_color_other = IM_COL32(186, 187, 241, 255);
            iris->ui.codeview_color_background = IM_COL32(48, 52, 70, 255);
            iris->ui.codeview_color_highlight = IM_COL32(81, 87, 109, 255);
        } break;

        case CodeviewColorScheme::CATPPUCCIN_MACCHIATO: {
            iris->ui.codeview_color_text = IM_COL32(174, 178, 208, 255);
            iris->ui.codeview_color_comment = IM_COL32(134, 138, 162, 255);
            iris->ui.codeview_color_mnemonic = IM_COL32(190, 132, 255, 255);
            iris->ui.codeview_color_number = IM_COL32(245, 142, 110, 255);
            iris->ui.codeview_color_register = IM_COL32(125, 182, 191, 255);
            iris->ui.codeview_color_other = IM_COL32(166, 167, 222, 255);
            iris->ui.codeview_color_background = IM_COL32(58, 60, 79, 255);
            iris->ui.codeview_color_highlight = IM_COL32(97, 100, 120, 255);
        } break;

        case CodeviewColorScheme::CATPPUCCIN_MOCHA: {
            iris->ui.codeview_color_text = IM_COL32(205, 214, 244, 255);
            iris->ui.codeview_color_comment = IM_COL32(145, 151, 181, 255);
            iris->ui.codeview_color_mnemonic = IM_COL32(220, 162, 255, 255);
            iris->ui.codeview_color_number = IM_COL32(248, 159, 128, 255);
            iris->ui.codeview_color_register = IM_COL32(159, 226, 235, 255);
            iris->ui.codeview_color_other = IM_COL32(189, 191, 248, 255);
            iris->ui.codeview_color_background = IM_COL32(46, 49, 64, 255);
            iris->ui.codeview_color_highlight = IM_COL32(76, 80, 100, 255);
        } break;
    }
}

VkShaderModule create_shader(Instance* iris, uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pCode = code;
    info.codeSize = size;

    VkShaderModule shader;

    if (vkCreateShaderModule(iris->vk.device, &info, nullptr, &shader) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return shader;
}

VkPipeline create_pipeline(Instance* iris, VkShaderModule vert_shader, VkShaderModule frag_shader) {
    // Create pipeline layout
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &iris->vk.descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = VK_NULL_HANDLE;

    if (vkCreatePipelineLayout(iris->vk.device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        iris_error(&iris->log.imgui, "Failed to create pipeline layout");

        return VK_NULL_HANDLE;
    }

    iris->vk.pipeline_layout = pipeline_layout;

    VkRenderPass render_pass = iris->vk.main_window_data.RenderPass;

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

    const auto binding_description = Vertex::get_binding_description();
    const auto attribute_descriptions = Vertex::get_attribute_descriptions();

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
    viewport.width = (float)iris->vk.main_window_data.Width;
    viewport.height = (float)iris->vk.main_window_data.Height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkExtent2D extent = {};
    extent.width = iris->vk.main_window_data.Width;
    extent.height = iris->vk.main_window_data.Height;

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

    if (vkCreateGraphicsPipelines(iris->vk.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    vkDestroyShaderModule(iris->vk.device, frag_shader, nullptr);
    vkDestroyShaderModule(iris->vk.device, vert_shader, nullptr);

    return pipeline;
}

bool init(Instance* iris) {
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

    if (vkCreateDescriptorSetLayout(iris->vk.device, &layout_info, nullptr, &iris->vk.descriptor_set_layout) != VK_SUCCESS) {
        iris_error(&iris->log.imgui, "Failed to create descriptor set layout");

        return false;
    }

    std::vector <VkDescriptorSetLayout> layouts(DESCRIPTOR_SET_RING_SIZE, iris->vk.descriptor_set_layout);

    iris->vk.descriptor_sets.resize(DESCRIPTOR_SET_RING_SIZE, VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = iris->vk.descriptor_pool;
    alloc_info.descriptorSetCount = DESCRIPTOR_SET_RING_SIZE;
    alloc_info.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(iris->vk.device, &alloc_info, iris->vk.descriptor_sets.data()) != VK_SUCCESS) {
        iris_error(&iris->log.imgui, "Failed to allocate descriptor sets");

        return false;
    }

    iris->vk.descriptor_set = iris->vk.descriptor_sets[0];

    if (!SDL_Vulkan_CreateSurface(iris->window, iris->vk.instance, VK_NULL_HANDLE, &iris->vk.surface)) {
        iris_error(&iris->log.imgui, "Failed to create Vulkan surface");

        return false;
    }

    if (!setup_vulkan_window(iris, &iris->vk.main_window_data, iris->window_width, iris->window_height, iris->present_mode == render::VSYNC)) {
        iris_error(&iris->log.imgui, "Failed to setup Vulkan window");

        return false;
    }

    iris->paths.ini_path = iris->paths.pref_path + "imgui.ini";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    if (iris->ui.imgui_enable_viewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoDecoration = false;
        io.ConfigViewportsNoAutoMerge = true;
    }

    io.IniFilename = iris->paths.ini_path.c_str();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(iris->vk.main_scale);
    style.FontScaleDpi = iris->vk.main_scale;
    style.FontScaleMain = iris->ui.ui_scale;

    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    // Setup Platform/Renderer backends
    if (!ImGui_ImplSDL3_InitForVulkan(iris->window)) {
        iris_error(&iris->log.imgui, "Failed to initialize SDL3/Vulkan backend");

        return false;
    }

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = IRIS_VULKAN_API_VERSION;
    init_info.Instance = iris->vk.instance;
    init_info.PhysicalDevice = iris->vk.physical_device;
    init_info.Device = iris->vk.device;
    init_info.QueueFamily = iris->vk.queue_family;
    init_info.Queue = iris->vk.queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = iris->vk.descriptor_pool;
    init_info.MinImageCount = iris->vk.min_image_count;
    init_info.ImageCount = iris->vk.main_window_data.ImageCount;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.PipelineInfoMain.RenderPass = iris->vk.main_window_data.RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = VK_NULL_HANDLE;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        iris_error(&iris->log.imgui, "Failed to initialize Vulkan backend");

        return false;
    }

    if (!setup_fonts(iris, io)) {
        iris_error(&iris->log.imgui, "Failed to setup fonts");

        return false;
    }

    set_theme(iris, iris->ui.theme, false);
    set_codeview_scheme(iris, iris->ui.codeview_color_scheme);

    // Initialize our pipeline
    VkShaderModule vert_shader = create_shader(iris, (uint32_t*)g_vertex_shader_data, g_vertex_shader_size);
    VkShaderModule frag_shader = create_shader(iris, (uint32_t*)g_fragment_shader_data, g_fragment_shader_size);

    if (!vert_shader || !frag_shader) {
        iris_error(&iris->log.imgui, "Failed to create shader modules");

        return false;
    }

    iris->vk.pipeline = create_pipeline(iris, vert_shader, frag_shader);

    if (!iris->vk.pipeline) {
        iris_error(&iris->log.imgui, "Failed to create graphics pipeline");

        return false;
    }

    iris->ui.ps1_memory_card_icon = vulkan::load_texture_from_memory(iris, g_ps1_memory_card_icon_data, g_ps1_memory_card_icon_size);
    iris->ui.ps2_memory_card_icon = vulkan::load_texture_from_memory(iris, g_ps2_memory_card_icon_data, g_ps2_memory_card_icon_size);
    iris->ui.pocketstation_icon = vulkan::load_texture_from_memory(iris, g_pocketstation_icon_data, g_pocketstation_icon_size);
    iris->ui.dualshock2_icon = vulkan::load_texture_from_memory(iris, g_dualshock2_icon_data, g_dualshock2_icon_size);
    iris->ui.iris_icon = vulkan::load_texture_from_memory(iris, g_iris_icon_data, g_iris_icon_size);

    return true;
}

void cleanup(Instance* iris) {
    vulkan::wait_idle(iris);

    vulkan::free_texture(iris, iris->ui.ps1_memory_card_icon);
    vulkan::free_texture(iris, iris->ui.ps2_memory_card_icon);
    vulkan::free_texture(iris, iris->ui.pocketstation_icon);
    vulkan::free_texture(iris, iris->ui.dualshock2_icon);
    vulkan::free_texture(iris, iris->ui.iris_icon);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    ImGui_ImplVulkanH_DestroyWindow(iris->vk.instance, iris->vk.device, &iris->vk.main_window_data, VK_NULL_HANDLE);

    iris->vk.instance = NULL;
}

// Between a successful acquire and a successful present there is no way to undo
// a failure in place: the image stays acquired and the acquire semaphore may be
// left signalled with nothing to wait on it. Re-entering the loop in that state
// is what turns one transient failure into an endless stream of
// VUID-vkAcquireNextImageKHR-{semaphore-01779,surface-07783}. Forcing a rebuild
// is the only recovery, since CreateOrResizeWindow recreates the semaphores
// along with the swapchain.
static bool abort_frame(Instance* iris, const char* what, VkResult err) {
    iris_error(&iris->log.imgui, "{} ({})", what, (int)err);

    if (err == VK_ERROR_DEVICE_LOST) {
        vulkan::dump_device_fault(iris);

        // Every subsequent call on this device would fail the same way, so stop
        // rendering rather than rebuilding into the same error.
        iris->vk.device_lost = true;

        if (!iris->fatal_error) {
            iris->fatal_error = true;
            iris->fatal_error_text = fmt::format(
                "The GPU device was lost during {}. The emulator cannot keep rendering; "
                "please restart it.", what
            );
        }
    } else {
        iris->vk.swapchain_rebuild = true;
    }

    return false;
}

bool render_frame(Instance* iris, ImDrawData* draw_data) {
    if (iris->vk.swapchain_rebuild || iris->vk.device_lost)
        return true;

    ImGui_ImplVulkanH_Window* wd = &iris->vk.main_window_data;
    VkSemaphore acquire_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;

    uint32_t image_index = 0;

    VkResult err;

    err = vkAcquireNextImageKHR(
        iris->vk.device,
        wd->Swapchain,
        UINT64_MAX,
        acquire_semaphore,
        VK_NULL_HANDLE,
        &image_index
    );

    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        iris->vk.swapchain_rebuild = true;

        return true;
    } else if (err != VK_SUCCESS) {
        // Nothing was acquired, so there is no swapchain state to unwind
        if (err == VK_ERROR_DEVICE_LOST)
            return abort_frame(iris, "Failed to acquire next image", err);

        iris_error(&iris->log.imgui, "Failed to acquire next image ({})", (int)err);

        return false;
    }

    // image_index is only meaningful once the acquire succeeded
    VkSemaphore submit_semaphore = wd->FrameSemaphores[image_index].RenderCompleteSemaphore;

    wd->FrameIndex = image_index;

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];

    if ((err = vkWaitForFences(iris->vk.device, 1, &fd->Fence, VK_TRUE, UINT64_MAX)) != VK_SUCCESS)
        return abort_frame(iris, "Failed to wait for fence", err);

    if ((err = vkResetFences(iris->vk.device, 1, &fd->Fence)) != VK_SUCCESS)
        return abort_frame(iris, "Failed to reset fence", err);

    if ((err = vkResetCommandPool(iris->vk.device, fd->CommandPool, 0)) != VK_SUCCESS)
        return abort_frame(iris, "Failed to reset command pool", err);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if ((err = vkBeginCommandBuffer(fd->CommandBuffer, &begin_info)) != VK_SUCCESS)
        return abort_frame(iris, "Failed to begin command buffer", err);

    if (iris->vk.instance && (iris->headless || !iris->ui.show_gamelist)) {
        render::render_frame(iris, fd->CommandBuffer, fd->Framebuffer);
    }

    {
        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = wd->RenderPass;
        render_pass_info.framebuffer = fd->Framebuffer;
        render_pass_info.renderArea.extent.width = wd->Width;
        render_pass_info.renderArea.extent.height = wd->Height;
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &iris->vk.clear_value;

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

        if ((err = vkEndCommandBuffer(fd->CommandBuffer)) != VK_SUCCESS)
            return abort_frame(iris, "Failed to end command buffer", err);

        if ((err = vkQueueSubmit(iris->vk.queue, 1, &submit_info, fd->Fence)) != VK_SUCCESS)
            return abort_frame(iris, "Failed to submit queue", err);
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
    present_info.pImageIndices = &image_index;

    err = vkQueuePresentKHR(iris->vk.queue, &present_info);

    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        iris->vk.swapchain_rebuild = true;

        return true;
    } else if (err != VK_SUCCESS) {
        return abort_frame(iris, "Failed to present image", err);
    }

    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;

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

void start_dim(Instance* iris, float alpha, size_t ms) {
    if (iris->ui.dim_active) {
        return;
    }

    iris->ui.dim_target_alpha = alpha;
    iris->ui.dim_current_alpha = 0.0f;
    iris->ui.dim_ms = ms;
    iris->ui.dim_start = SDL_GetTicks();
    iris->ui.dim_active = true;
    iris->ui.dim_end = false;
}

void end_dim(Instance* iris) {
    iris->ui.dim_current_alpha = 1.0f;
    iris->ui.dim_active = true;
    iris->ui.dim_end = true;
    iris->ui.dim_start = SDL_GetTicks();
}

void render_dim(Instance* iris) {
    using namespace ImGui;

    ImDrawList* draw_list = GetForegroundDrawList(GetMainViewport());

    size_t ticks = SDL_GetTicks();
    size_t diff = ticks - iris->ui.dim_start;
    
    iris->ui.dim_current_alpha = (float)diff / (float)iris->ui.dim_ms;

    if (iris->ui.dim_end) {
        iris->ui.dim_current_alpha = 1.0f - iris->ui.dim_current_alpha;

        if (iris->ui.dim_current_alpha <= 0.0f) {
            iris->ui.dim_active = false;
            iris->ui.dim_end = false;
            iris->ui.dim_current_alpha = 0.0f;

            return;
        }
    } else {
        if (iris->ui.dim_current_alpha >= 1.0f) {
            iris->ui.dim_current_alpha = 1.0f;
        }
    }

    ImVec2 pos = GetMainViewport()->Pos;
    ImVec2 size = GetMainViewport()->Size;

    draw_list->AddRectFilled(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        ImColor(0.0f, 0.0f, 0.0f, iris->ui.dim_target_alpha * iris->ui.dim_current_alpha)
    );
}

}