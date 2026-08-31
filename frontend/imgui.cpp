#include "iris.hpp"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#include "imgui_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <array>
#include <cstdarg>

// External includes
#include "misc/cpp/imgui_stdlib.h"
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

static void (*g_platform_set_window_size)(ImGuiViewport*, ImVec2) = nullptr;

// Note: SDL resizes platform windows async. ImGui calls Platform_SetWindowSize and then
//       immediately hands the viewport to Renderer_SetWindowSize, which asks the surface
//       for its capabilities to size the new swapchain. Under X11 the server has usually
//       not applied the resize yet, so the swapchain gets built against the stale extent,
//       and doing that on a swapchain that has already presented is enough to make the
//       graphics driver lose the device, so we flush the resize before the renderer looks
static void install_viewport_resize_sync() {
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    if (!platform_io.Platform_SetWindowSize || g_platform_set_window_size)
        return;

    g_platform_set_window_size = platform_io.Platform_SetWindowSize;

    platform_io.Platform_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size) {
        g_platform_set_window_size(viewport, size);

        if (SDL_Window* window = SDL_GetWindowFromID((SDL_WindowID)(intptr_t)viewport->PlatformHandle))
            SDL_SyncWindow(window);
    };
}

static void (*g_platform_create_window)(ImGuiViewport*) = nullptr;
static Instance* g_platform_iris = nullptr;

static void install_viewport_window_style(Instance* iris) {
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    if (!platform_io.Platform_CreateWindow || g_platform_create_window)
        return;

    g_platform_iris = iris;
    g_platform_create_window = platform_io.Platform_CreateWindow;

    platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport) {
        g_platform_create_window(viewport);

        platform::apply_settings(g_platform_iris);
    };
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

static void draw_section(Instance* iris, const char* label, bool* open) {
    using namespace ImGui;

    constexpr float size = 11.0f;
    constexpr float tracking = 1.4f;

    ImFont* font = iris->ui.font_label;
    ImFontBaked* baked = font->GetFontBaked(size);
    ImDrawList* dl = GetWindowDrawList();

    float spacing = GetStyle().ItemSpacing.y;
    float row = size + spacing * 2.0f;
    float arrow = open ? size + 6.0f : 0.0f;

    ImVec2 start = GetCursorScreenPos();
    bool hovered = false;

    if (open) {
        PushID(label);

        if (InvisibleButton("##section", ImVec2(GetContentRegionAvail().x, row)))
            *open = !*open;

        hovered = IsItemHovered();

        PopID();

        SetCursorScreenPos(start);
    }

    ImVec2 pos = start;

    pos.y += spacing;

    ImU32 text_col = GetColorU32(hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);

    if (open) {
        constexpr float scale = 0.7f;

        float arrow_y = pos.y + size * 0.5f - GetFontSize() * 0.5f * scale;

        RenderArrow(dl, ImVec2(start.x, arrow_y), text_col, *open ? ImGuiDir_Down : ImGuiDir_Right, scale);
    }

    float x = pos.x + arrow;

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

    SetCursorScreenPos(start);
    Dummy(ImVec2(0.0f, row));
}

void section(Instance* iris, const char* label) {
    draw_section(iris, label, nullptr);
}

bool section(Instance* iris, const char* label, bool* open) {
    draw_section(iris, label, open);

    return *open;
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

void TextDisabledCentered(const char* fmt, ...) {
    char buf[1024];

    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ImVec2 size = ImGui::CalcTextSize(buf);
    ImVec2 avail = ImGui::GetContentRegionAvail();

    ImGui::SetCursorPos(ImVec2(
        ImGui::GetCursorPosX() + std::max(0.0f, (avail.x - size.x) * 0.5f),
        ImGui::GetCursorPosY() + std::max(0.0f, (avail.y - size.y) * 0.5f)
    ));

    ImGui::TextDisabled("%s", buf);
}

std::string format_size(uint64_t size) {
    char buf[64];

    if (size >= 0x40000000ull) {
        snprintf(buf, sizeof(buf), "%.1f GiB", (double)size / 0x40000000ull);
    } else if (size >= 0x100000ull) {
        snprintf(buf, sizeof(buf), "%.1f MiB", (double)size / 0x100000ull);
    } else if (size >= 0x400ull) {
        snprintf(buf, sizeof(buf), "%.1f KiB", (double)size / 0x400ull);
    } else {
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)size);
    }

    return std::string(buf);
}

bool splitter(const char* id, bool vertical, float place, float* size1, float* size2, float min1, float min2, float long_axis) {
    using namespace ImGui;

    constexpr float thickness = 1.0f;
    constexpr float grab = 4.0f;

    ImGuiWindow* window = GetCurrentWindow();

    ImRect bb;

    ImVec2 cursor = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(vertical ? ImVec2(thickness, long_axis) : ImVec2(long_axis, thickness), 0.0f, 0.0f);

    bb.Min = ImVec2(cursor.x + (vertical ? place : 0.0f), cursor.y + (vertical ? 0.0f : place));
    bb.Max = ImVec2(bb.Min.x + size.x, bb.Min.y + size.y);

    PushStyleColor(ImGuiCol_SeparatorHovered, GetStyleColorVec4(ImGuiCol_Separator));
    PushStyleColor(ImGuiCol_Separator, ImVec4(0.0, 0.0, 0.0, 0.0));

    bool r = SplitterBehavior(bb, window->GetID(id), vertical ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min1, min2, grab);

    PopStyleColor(2);

    return r;
}

float splitter_before(bool vertical, float size1) {
    ImGuiStyle& style = ImGui::GetStyle();

    return size1 + ((vertical ? style.ItemSpacing.x : style.ItemSpacing.y) - 1.0f) * 0.5f;
}

float splitter_at_cursor(bool vertical) {
    ImGuiStyle& style = ImGui::GetStyle();

    return -((vertical ? style.ItemSpacing.x : style.ItemSpacing.y) + 1.0f) * 0.5f;
}

bool text_input(const char* id, std::string* value, const char* placeholder, float width) {
    using namespace ImGui;

    static std::string original;

    if (width > 0.0f)
        SetNextItemWidth(width);

    bool entered = InputTextWithHint(id, placeholder, value, ImGuiInputTextFlags_EnterReturnsTrue);

    if (IsItemActivated())
        original = *value;

    if (!entered && !IsItemDeactivatedAfterEdit())
        return false;

    return original != *value;
}

bool segmented(const char* id, int* value, const char* const* labels, int count, float width) {
    using namespace ImGui;

    if (count <= 0)
        return false;

    bool stretch = width <= 0.0f;

    ImGuiStyle& style = GetStyle();
    ImDrawList* draw_list = GetWindowDrawList();

    float height = GetFrameHeight();
    float total = stretch ? GetContentRegionAvail().x : width * count;
    float step = total / count;
    float rounding = style.FrameRounding;

    ImVec2 origin = GetCursorScreenPos();

    PushID(id);

    bool pressed = InvisibleButton("##segmented", ImVec2(total, height));
    bool over = IsItemHovered();

    PopID();

    int hovered = -1;

    if (over) {
        hovered = (int)((GetIO().MousePos.x - origin.x) / step);
        hovered = hovered < 0 ? 0 : (hovered >= count ? count - 1 : hovered);
    }

    bool changed = false;

    if (pressed && hovered >= 0 && hovered != *value) {
        *value = hovered;
        changed = true;
    }

    auto edge = [&](int i) {
        if (count == 1)
            return ImDrawFlags_RoundCornersAll;

        if (i == 0)
            return ImDrawFlags_RoundCornersLeft;

        if (i == count - 1)
            return ImDrawFlags_RoundCornersRight;

        return ImDrawFlags_RoundCornersNone;
    };

    auto cell = [&](int i, ImVec2* min, ImVec2* max) {
        *min = ImVec2(origin.x + step * i, origin.y);
        *max = ImVec2(min->x + step, origin.y + height);
    };

    ImVec2 min, max;

    draw_list->AddRectFilled(origin, ImVec2(origin.x + total, origin.y + height), GetColorU32(ImGuiCol_FrameBg), rounding);

    if (hovered >= 0 && hovered != *value) {
        cell(hovered, &min, &max);

        draw_list->AddRectFilled(min, max, GetColorU32(ImGuiCol_FrameBgHovered), rounding, edge(hovered));
    }

    for (int i = 1; i < count; i++) {
        if (i == *value || i - 1 == *value)
            continue;

        float x = origin.x + step * i;

        draw_list->AddLine(ImVec2(x, origin.y + 4.0f), ImVec2(x, origin.y + height - 4.0f), GetColorU32(ImGuiCol_Border));
    }

    if (*value >= 0 && *value < count) {
        cell(*value, &min, &max);

        draw_list->AddRectFilled(min, max, GetColorU32(ImGuiCol_HeaderActive), rounding, edge(*value));
    }

    for (int i = 0; i < count; i++) {
        cell(i, &min, &max);

        ImVec2 size = CalcTextSize(labels[i]);
        ImVec2 pos = ImVec2(min.x + (step - size.x) * 0.5f, min.y + (height - size.y) * 0.5f);

        draw_list->PushClipRect(min, max, true);
        draw_list->AddText(pos, GetColorU32(ImGuiCol_Text), labels[i]);
        draw_list->PopClipRect();
    }

    draw_list->AddRect(origin, ImVec2(origin.x + total, origin.y + height), GetColorU32(ImGuiCol_Border), rounding);

    return changed;
}

bool BeginMenu(const char* label, bool enabled) {
    ImDrawList* draw_list = begin_menu_highlight();

    bool open = ImGui::BeginMenu(label, enabled);

    end_menu_highlight(draw_list, open);

    return open;
}

void badge(const char* text, const ImVec4& color, float bg_alpha) {
    using namespace ImGui;

    ImGuiStyle& style = GetStyle();
    ImDrawList* draw_list = GetWindowDrawList();

    ImVec2 padding = ImVec2(style.FramePadding.x * 0.75f, style.FramePadding.y * 0.5f);
    ImVec2 size = CalcTextSize(text);
    ImVec2 total = ImVec2(size.x + padding.x * 2.0f, size.y + padding.y * 2.0f);
    ImVec2 origin = GetCursorScreenPos();

    Dummy(total);

    ImVec2 end = ImVec2(origin.x + total.x, origin.y + total.y);
    float rounding = total.y * 0.5f;

    draw_list->AddRectFilled(origin, end, GetColorU32(ImVec4(color.x, color.y, color.z, bg_alpha)), rounding);
    draw_list->AddRect(origin, end, GetColorU32(ImVec4(color.x, color.y, color.z, bg_alpha * 2.5f)), rounding);
    draw_list->AddText(ImVec2(origin.x + padding.x, origin.y + padding.y), GetColorU32(color), text);
}

namespace palette {

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
    bool light = false;
};

constexpr Palette granite_dark = {
    .app           = rgb(0x0b0b0d),
    .window        = rgb(0x131316),
    .popup         = rgb(0x19191d),
    .frame         = rgb(0x202025),
    .hover         = rgb(0x2b2b32),
    .active        = rgb(0x373740),
    .border        = rgb(0x28282e),
    .text          = rgb(0xe6e6ec),
    .text_dim      = rgb(0x8b8b96),
    .accent        = rgb(0x8b5cf6),
    .accent_hi     = rgb(0x9d75f8),
    .link          = rgb(0x60a5fa),
    .scroll        = rgb(0xffffff, 0.10f),
    .scroll_hover  = rgb(0xffffff, 0.18f),
    .scroll_active = rgb(0xffffff, 0.26f),
    .alt_row       = rgb(0xffffff, 0.022f),
    .grip          = rgb(0xffffff, 0.06f),
    .table_line    = rgb(0xffffff, 0.04f)
};

constexpr Palette granite_light = {
    .app           = rgb(0xe8e8ea),
    .window        = rgb(0xf7f7f8),
    .popup         = rgb(0xffffff),
    .frame         = rgb(0xecedef),
    .hover         = rgb(0xe0e0e5),
    .active        = rgb(0xd2d2d9),
    .border        = rgb(0xdcdce1),
    .text          = rgb(0x1c1c21),
    .text_dim      = rgb(0x6b6b75),
    .accent        = rgb(0x7c3aed),
    .accent_hi     = rgb(0x6d28d9),
    .link          = rgb(0x2563eb),
    .scroll        = rgb(0x000000, 0.16f),
    .scroll_hover  = rgb(0x000000, 0.26f),
    .scroll_active = rgb(0x000000, 0.36f),
    .alt_row       = rgb(0x000000, 0.028f),
    .grip          = rgb(0x000000, 0.10f),
    .table_line    = rgb(0x000000, 0.06f),
    .light         = true
};

constexpr Palette nord = {
    .app           = rgb(0x242933),
    .window        = rgb(0x2e3440),
    .popup         = rgb(0x3b4252),
    .frame         = rgb(0x3b4252),
    .hover         = rgb(0x434c5e),
    .active        = rgb(0x4c566a),
    .border        = rgb(0x434c5e),
    .text          = rgb(0xeceff4),
    .text_dim      = rgb(0x8b98b0),
    .accent        = rgb(0x88c0d0),
    .accent_hi     = rgb(0xa3d4e2),
    .link          = rgb(0x81a1c1),
    .scroll        = rgb(0xd8dee9, 0.14f),
    .scroll_hover  = rgb(0xd8dee9, 0.24f),
    .scroll_active = rgb(0xd8dee9, 0.34f),
    .alt_row       = rgb(0xd8dee9, 0.030f),
    .grip          = rgb(0xd8dee9, 0.08f),
    .table_line    = rgb(0xd8dee9, 0.06f)
};

constexpr Palette gruvbox = {
    .app           = rgb(0x1d2021),
    .window        = rgb(0x282828),
    .popup         = rgb(0x32302f),
    .frame         = rgb(0x3c3836),
    .hover         = rgb(0x504945),
    .active        = rgb(0x665c54),
    .border        = rgb(0x3c3836),
    .text          = rgb(0xebdbb2),
    .text_dim      = rgb(0xa89984),
    .accent        = rgb(0xfe8019),
    .accent_hi     = rgb(0xffa04d),
    .link          = rgb(0x83a598),
    .scroll        = rgb(0xfbf1c7, 0.12f),
    .scroll_hover  = rgb(0xfbf1c7, 0.20f),
    .scroll_active = rgb(0xfbf1c7, 0.30f),
    .alt_row       = rgb(0xfbf1c7, 0.028f),
    .grip          = rgb(0xfbf1c7, 0.07f),
    .table_line    = rgb(0xfbf1c7, 0.05f)
};

constexpr Palette tokyo_night = {
    .app           = rgb(0x16161e),
    .window        = rgb(0x1a1b26),
    .popup         = rgb(0x1f2335),
    .frame         = rgb(0x24283b),
    .hover         = rgb(0x292e42),
    .active        = rgb(0x414868),
    .border        = rgb(0x292e42),
    .text          = rgb(0xc0caf5),
    .text_dim      = rgb(0x6b7394),
    .accent        = rgb(0x7aa2f7),
    .accent_hi     = rgb(0x9db8ff),
    .link          = rgb(0x7dcfff),
    .scroll        = rgb(0xc0caf5, 0.12f),
    .scroll_hover  = rgb(0xc0caf5, 0.20f),
    .scroll_active = rgb(0xc0caf5, 0.30f),
    .alt_row       = rgb(0xc0caf5, 0.026f),
    .grip          = rgb(0xc0caf5, 0.07f),
    .table_line    = rgb(0xc0caf5, 0.05f)
};

constexpr Palette mocha = {
    .app           = rgb(0x11111b),
    .window        = rgb(0x1e1e2e),
    .popup         = rgb(0x181825),
    .frame         = rgb(0x313244),
    .hover         = rgb(0x45475a),
    .active        = rgb(0x585b70),
    .border        = rgb(0x313244),
    .text          = rgb(0xcdd6f4),
    .text_dim      = rgb(0x9399b2),
    .accent        = rgb(0xcba6f7),
    .accent_hi     = rgb(0xddc0ff),
    .link          = rgb(0x89b4fa),
    .scroll        = rgb(0xcdd6f4, 0.12f),
    .scroll_hover  = rgb(0xcdd6f4, 0.20f),
    .scroll_active = rgb(0xcdd6f4, 0.30f),
    .alt_row       = rgb(0xcdd6f4, 0.026f),
    .grip          = rgb(0xcdd6f4, 0.07f),
    .table_line    = rgb(0xcdd6f4, 0.05f)
};

constexpr Palette latte = {
    .app           = rgb(0xdce0e8),
    .window        = rgb(0xeff1f5),
    .popup         = rgb(0xfbfcfe),
    .frame         = rgb(0xe6e9ef),
    .hover         = rgb(0xdce0e8),
    .active        = rgb(0xccd0da),
    .border        = rgb(0xccd0da),
    .text          = rgb(0x4c4f69),
    .text_dim      = rgb(0x6c6f85),
    .accent        = rgb(0x8839ef),
    .accent_hi     = rgb(0x7024d4),
    .link          = rgb(0x1e66f5),
    .scroll        = rgb(0x4c4f69, 0.18f),
    .scroll_hover  = rgb(0x4c4f69, 0.28f),
    .scroll_active = rgb(0x4c4f69, 0.38f),
    .alt_row       = rgb(0x4c4f69, 0.030f),
    .grip          = rgb(0x4c4f69, 0.10f),
    .table_line    = rgb(0x4c4f69, 0.07f),
    .light         = true
};

constexpr Palette solarized = {
    .app           = rgb(0x00212b),
    .window        = rgb(0x002b36),
    .popup         = rgb(0x073642),
    .frame         = rgb(0x073642),
    .hover         = rgb(0x0d4a59),
    .active        = rgb(0x14606f),
    .border        = rgb(0x0b414f),
    .text          = rgb(0x93a1a1),
    .text_dim      = rgb(0x657b83),
    .accent        = rgb(0x268bd2),
    .accent_hi     = rgb(0x4aa8e8),
    .link          = rgb(0x2aa198),
    .scroll        = rgb(0x93a1a1, 0.16f),
    .scroll_hover  = rgb(0x93a1a1, 0.26f),
    .scroll_active = rgb(0x93a1a1, 0.36f),
    .alt_row       = rgb(0x93a1a1, 0.030f),
    .grip          = rgb(0x93a1a1, 0.08f),
    .table_line    = rgb(0x93a1a1, 0.06f)
};

constexpr Palette sakura = {
    .app           = rgb(0x1a1017),
    .window        = rgb(0x241820),
    .popup         = rgb(0x2e1f29),
    .frame         = rgb(0x3a2833),
    .hover         = rgb(0x4b3341),
    .active        = rgb(0x5e4050),
    .border        = rgb(0x3d2a35),
    .text          = rgb(0xf9e4ed),
    .text_dim      = rgb(0xb692a3),
    .accent        = rgb(0xff8fbc),
    .accent_hi     = rgb(0xffb3d1),
    .link          = rgb(0xc9a6ff),
    .scroll        = rgb(0xffd7e6, 0.14f),
    .scroll_hover  = rgb(0xffd7e6, 0.24f),
    .scroll_active = rgb(0xffd7e6, 0.34f),
    .alt_row       = rgb(0xffd7e6, 0.032f),
    .grip          = rgb(0xffd7e6, 0.08f),
    .table_line    = rgb(0xffd7e6, 0.06f)
};

constexpr Palette sakura_light = {
    .app           = rgb(0xf0dde5),
    .window        = rgb(0xfdf4f7),
    .popup         = rgb(0xffffff),
    .frame         = rgb(0xfbe9f0),
    .hover         = rgb(0xf6d8e4),
    .active        = rgb(0xeec5d6),
    .border        = rgb(0xf3dbe4),
    .text          = rgb(0x3d2430),
    .text_dim      = rgb(0x8a6b78),
    .accent        = rgb(0xd9497f),
    .accent_hi     = rgb(0xbc3468),
    .link          = rgb(0x7c4dd8),
    .scroll        = rgb(0x6b2440, 0.16f),
    .scroll_hover  = rgb(0x6b2440, 0.26f),
    .scroll_active = rgb(0x6b2440, 0.36f),
    .alt_row       = rgb(0x6b2440, 0.032f),
    .grip          = rgb(0x6b2440, 0.10f),
    .table_line    = rgb(0x6b2440, 0.07f),
    .light         = true
};

static const Palette* find(int theme) {
    switch (theme) {
        case Theme::GRANITE_NEO:       return &granite_dark;
        case Theme::GRANITE_NEO_LIGHT: return &granite_light;
        case Theme::NORD:              return &nord;
        case Theme::GRUVBOX:           return &gruvbox;
        case Theme::TOKYO_NIGHT:       return &tokyo_night;
        case Theme::MOCHA:             return &mocha;
        case Theme::LATTE:             return &latte;
        case Theme::SOLARIZED:         return &solarized;
        case Theme::SAKURA:            return &sakura;
        case Theme::SAKURA_LIGHT:      return &sakura_light;
    }

    return nullptr;
}

}

static void apply_palette(ImGuiStyle& style, const palette::Palette& p) {
    using palette::alpha;

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
    colors[ImGuiCol_DragDropTargetBg]       = alpha(p.accent, 0.20f);
    colors[ImGuiCol_TreeLines]              = p.table_line;
    colors[ImGuiCol_UnsavedMarker]          = p.text_dim;
    colors[ImGuiCol_NavCursor]              = p.accent;
    colors[ImGuiCol_NavWindowingHighlight]  = alpha(p.text, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = palette::rgb(0x000000, 0.45f);
    colors[ImGuiCol_ModalWindowDimBg]       = palette::rgb(0x000000, 0.55f);
}

static const ImVec4 unset_color(-1.0f, -1.0f, -1.0f, -1.0f);

static void derive_unset_colors(ImGuiStyle& style, const ImVec4* fallback) {
    ImVec4* colors = style.Colors;

    auto set = [&](ImGuiCol idx) { return colors[idx].w >= 0.0f; };
    auto get = [&](ImGuiCol idx) { return set(idx) ? colors[idx] : fallback[idx]; };

    auto alpha = [](ImVec4 c, float a) { return ImVec4(c.x, c.y, c.z, a); };
    auto mix = [](ImVec4 a, ImVec4 b, float t) {
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w);
    };

    const ImVec4 text = get(ImGuiCol_Text);
    const ImVec4 window = get(ImGuiCol_WindowBg);
    const ImVec4 border = get(ImGuiCol_Border);
    const ImVec4 accent = get(ImGuiCol_CheckMark);
    const ImVec4 title = get(ImGuiCol_TitleBg);
    const ImVec4 title_hi = get(ImGuiCol_TitleBgActive);

    const ImVec4 line = border.w < 0.05f ? alpha(text, 0.20f) : border;

    struct Rule { ImGuiCol idx; ImVec4 color; };

    const Rule rules[] = {
        { ImGuiCol_Separator,                 line },
        { ImGuiCol_SeparatorHovered,          get(ImGuiCol_HeaderHovered) },
        { ImGuiCol_SeparatorActive,           get(ImGuiCol_HeaderActive) },
        { ImGuiCol_InputTextCursor,           text },
        { ImGuiCol_Tab,                       title },
        { ImGuiCol_TabHovered,                get(ImGuiCol_HeaderHovered) },
        { ImGuiCol_TabSelected,               title_hi },
        { ImGuiCol_TabSelectedOverline,       accent },
        { ImGuiCol_TabDimmed,                 title },
        { ImGuiCol_TabDimmedSelected,         mix(title, title_hi, 0.5f) },
        { ImGuiCol_TabDimmedSelectedOverline, alpha(accent, 0.00f) },
        { ImGuiCol_DockingPreview,            alpha(accent, 0.35f) },
        { ImGuiCol_DockingEmptyBg,            mix(window, ImVec4(0.0f, 0.0f, 0.0f, window.w), 0.35f) },
        { ImGuiCol_TableHeaderBg,             mix(window, text, 0.08f) },
        { ImGuiCol_TableBorderStrong,         alpha(text, 0.18f) },
        { ImGuiCol_TableBorderLight,          alpha(text, 0.08f) },
        { ImGuiCol_TableRowBg,                alpha(text, 0.00f) },
        { ImGuiCol_TableRowBgAlt,             alpha(text, 0.035f) },
        { ImGuiCol_TreeLines,                 alpha(text, 0.16f) },
        { ImGuiCol_TextLink,                  accent },
        { ImGuiCol_DragDropTarget,            accent },
        { ImGuiCol_DragDropTargetBg,          alpha(accent, 0.20f) },
        { ImGuiCol_UnsavedMarker,             alpha(text, 0.60f) },
        { ImGuiCol_NavCursor,                 accent },
        { ImGuiCol_NavWindowingHighlight,     alpha(text, 0.70f) },
        { ImGuiCol_NavWindowingDimBg,         ImVec4(0.0f, 0.0f, 0.0f, 0.45f) },
        { ImGuiCol_ModalWindowDimBg,          ImVec4(0.0f, 0.0f, 0.0f, 0.55f) }
    };

    for (const Rule& r : rules) {
        if (!set(r.idx)) colors[r.idx] = r.color;
    }

    for (int i = 0; i < ImGuiCol_COUNT; i++) {
        if (!set((ImGuiCol)i)) colors[i] = fallback[i];
    }
}

static void set_clear_color(Instance* iris, const ImVec4& c) {
    iris->vk.clear_value.color.float32[0] = c.x;
    iris->vk.clear_value.color.float32[1] = c.y;
    iris->vk.clear_value.color.float32[2] = c.z;
    iris->vk.clear_value.color.float32[3] = 1.00f;
}

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

    const palette::Palette* p = palette::find(theme);

    ImVec4 fallback[ImGuiCol_COUNT];

    memcpy(fallback, style.Colors, sizeof(fallback));

    if (!p) {
        for (int i = 0; i < ImGuiCol_COUNT; i++)
            style.Colors[i] = unset_color;
    }

    if (p) {
        // Light base so anything ImGui adds later doesn't come through dark
        if (p->light) ImGui::StyleColorsLight();

        apply_palette(style, *p);

        if (set_bg_color) set_clear_color(iris, p->app);
    } else switch (theme) {
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

    if (!p) derive_unset_colors(style, fallback);

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

    install_viewport_resize_sync();
    install_viewport_window_style(iris);

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

static bool abort_frame(Instance* iris, const char* what, VkResult err) {
    iris_error(&iris->log.imgui, "{} ({})", what, (int)err);

    if (err == VK_ERROR_DEVICE_LOST) {
        vulkan::dump_device_fault(iris);

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

    render::update_fsr(iris);

    if ((err = vkBeginCommandBuffer(fd->CommandBuffer, &begin_info)) != VK_SUCCESS)
        return abort_frame(iris, "Failed to begin command buffer", err);

    if (iris->vk.instance && (iris->headless || !iris->ui.show_gamelist)) {
        render::render_frame(iris, fd->CommandBuffer, fd->Framebuffer);
    }

    {
        VkClearValue clear = render::background(iris);

        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = wd->RenderPass;
        render_pass_info.framebuffer = fd->Framebuffer;
        render_pass_info.renderArea.extent.width = wd->Width;
        render_pass_info.renderArea.extent.height = wd->Height;
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear;

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
