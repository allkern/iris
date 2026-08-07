#pragma once

#include "imgui.h"

namespace iris {

struct Instance;

namespace imgui {

// Values are persisted to settings.toml, so new themes get appended rather
// than slotted in
enum Theme {
    GRANITE_NEO,
    IMGUI_DARK,
    IMGUI_LIGHT,
    IMGUI_CLASSIC,
    CHERRY,
    SOURCE,
    GRANITE_NEO_LIGHT,
    GRANITE,
    THEME_COUNT
};

enum CodeviewColorScheme {
    SOLARIZED_DARK,
    SOLARIZED_LIGHT,
    ONE_DARK_PRO,
    CATPPUCCIN_LATTE,
    CATPPUCCIN_FRAPPE,
    CATPPUCCIN_MACCHIATO,
    CATPPUCCIN_MOCHA
};

bool init(Instance* iris);
void set_theme(Instance* iris, int theme, bool set_bg_color = true);
void set_codeview_scheme(Instance* iris, int scheme);
bool render_frame(Instance* iris, ImDrawData* draw_data);
void cleanup(Instance* iris);
void set_vsync(Instance* iris, bool vsync);
void start_dim(Instance* iris, float alpha, size_t ms);
void end_dim(Instance* iris);
void render_dim(Instance* iris);

bool BeginEx(const char* name, bool* p_open, ImGuiWindowFlags flags = 0);

void section(Instance* iris, const char* label);
bool section(Instance* iris, const char* label, bool* open);

bool MenuItem(const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);
bool MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled = true);
bool BeginMenu(const char* label, bool enabled = true);
bool Selectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0));
bool Selectable(const char* label, bool* p_selected, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0));

bool segmented(const char* id, int* value, const char* const* labels, int count, float width = 0.0f);

}

}
