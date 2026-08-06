#pragma once

#include "imgui.h"

namespace iris {

struct Instance;

#define IRIS_THEME_GRANITE 0
#define IRIS_THEME_IMGUI_DARK 1
#define IRIS_THEME_IMGUI_LIGHT 2
#define IRIS_THEME_IMGUI_CLASSIC 3
#define IRIS_THEME_CHERRY 4
#define IRIS_THEME_SOURCE 5

#define IRIS_CODEVIEW_COLOR_SCHEME_SOLARIZED_DARK 0
#define IRIS_CODEVIEW_COLOR_SCHEME_SOLARIZED_LIGHT 1
#define IRIS_CODEVIEW_COLOR_SCHEME_ONE_DARK_PRO 2
#define IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_LATTE 3
#define IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_FRAPPE 4
#define IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_MACCHIATO 5
#define IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_MOCHA 6

namespace imgui {

bool init(Instance* iris);
void set_theme(Instance* iris, int theme, bool set_bg_color = true);
void set_codeview_scheme(Instance* iris, int scheme);
bool render_frame(Instance* iris, ImDrawData* draw_data);
void cleanup(Instance* iris);
void set_vsync(Instance* iris, bool vsync);
void start_dim(Instance* iris, float alpha, size_t ms);
void end_dim(Instance* iris);
void render_dim(Instance* iris);

// Wrapper for ImGui::Begin that sets a default size
bool BeginEx(const char* name, bool* p_open, ImGuiWindowFlags flags = 0);

}

}
