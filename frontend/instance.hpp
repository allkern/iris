#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL.h>

#include "ps2.h"
#include "ee/renderer/software.hpp"

namespace lunar {

enum : int {
    BKPT_CPU_EE,
    BKPT_CPU_IOP
};

struct breakpoint {
    uint32_t addr;
    int cpu;
    bool cond_r, cond_w, cond_x;
    int size;
    bool enabled;
};

struct instance {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_GLContext gl_context = nullptr;

    struct ps2_state* ps2 = nullptr;

    unsigned int window_width = 960;
    unsigned int window_height = 720;
    unsigned int texture_width;
    unsigned int texture_height;
    uint32_t* texture_buf = nullptr;

    software_state* ctx = nullptr;

    ImFont* font_small_code = nullptr;
    ImFont* font_code = nullptr;
    ImFont* font_small = nullptr;
    ImFont* font_heading = nullptr;
    ImFont* font_body = nullptr;
    ImFont* font_icons = nullptr;

    std::string elf_path = "";
    std::string boot_path = "";
    std::string bios_path = "";
    std::string disc_path = "";

    bool open = false;
    bool pause = true;
    bool step = false;

    bool show_ee_control = false;
    bool show_ee_state = false;
    bool show_ee_logs = false;
    bool show_ee_interrupts = false;
    bool show_ee_dmac = false;
    bool show_iop_control = false;
    bool show_iop_state = false;
    bool show_iop_logs = false;
    bool show_iop_interrupts = false;
    bool show_iop_dma = false;
    bool show_gs_debugger = false;
    bool show_memory_viewer = false;
    bool show_status_bar = true;
    bool show_breakpoints = false;
    bool show_imgui_demo = false;

    // Special windows
    bool show_bios_setting_window = false;
    bool show_about_window = false;

    bool fullscreen = false;
    int aspect_mode = 0;
    bool bilinear = true;
    bool integer_scaling = false;
    float scale = 1.5f;

    bool dump_to_file = true;
    std::string settings_path = "";

    int frames = 0;
    float fps = 0.0f;
    unsigned int ticks = 0;
    int menubar_height = 0;
    bool mute = false;

    std::string loaded = "";

    std::vector <std::string> ee_log = { "" };
    std::vector <std::string> iop_log = { "" };

    std::vector <breakpoint> breakpoints = {};

    struct ds_state* ds = nullptr;
};

lunar::instance* create();
void init(lunar::instance* lunar, int argc, const char* argv[]);
void close(lunar::instance* lunar);
void destroy(lunar::instance* lunar);
bool is_open(lunar::instance* lunar);

void update(lunar::instance* lunar);
void update_window(lunar::instance* lunar);

void show_main_menubar(lunar::instance* lunar);
void show_ee_control(lunar::instance* lunar);
void show_ee_state(lunar::instance* lunar);
void show_ee_logs(lunar::instance* lunar);
void show_ee_interrupts(lunar::instance* lunar);
void show_ee_dmac(lunar::instance* lunar);
void show_iop_control(lunar::instance* lunar);
void show_iop_state(lunar::instance* lunar);
void show_iop_logs(lunar::instance* lunar);
void show_iop_interrupts(lunar::instance* lunar);
void show_iop_dma(lunar::instance* lunar);
void show_gs_debugger(lunar::instance* lunar);
void show_memory_viewer(lunar::instance* lunar);
void show_status_bar(lunar::instance* lunar);
void show_breakpoints(lunar::instance* lunar);
void show_about_window(lunar::instance* lunar);

}