#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <deque>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL3/SDL.h>

#include "ps2.h"
#include "gs/renderer/renderer.hpp"

namespace iris {

enum : int {
    BKPT_CPU_EE,
    BKPT_CPU_IOP
};

struct breakpoint {
    uint32_t addr;
    const char* symbol = nullptr;
    int cpu;
    bool cond_r, cond_w, cond_x;
    int size;
    bool enabled;
};

struct move_animation {
    int frames;
    int frames_remaining;
    float source_x, source_y;
    float target_x, target_y;
    float x, y;
};

struct fade_animation {
    int frames;
    int frames_remaining;
    int source_alpha, target_alpha;
    int alpha;
};

struct notification {
    int type;
    int state;
    int frames;
    int frames_remaining;
    float width, height;
    float text_width, text_height;
    bool end;
    move_animation move;
    fade_animation fade;
    std::string text;
};

struct elf_symbol {
    char* name;
    uint32_t addr;
    uint32_t size;
};

// Event -> Action
enum {
    INPUT_ACTION_PRESS_BUTTON,
    INPUT_ACTION_RELEASE_BUTTON,
    INPUT_ACTION_MOVE_AXIS
};

enum {
    INPUT_CONTROLLER_DUALSHOCK2

    // Large To-do list here, we're missing the Namco GunCon
    // controllers, JogCon, NegCon, Buzz! Buzzer, the Train
    // controllers, Taiko Drum Master controller, the Dance Dance
    // Revolution mat, Guitar Hero controllers, etc.
};

// struct input_action {
//     int action;

//     union {
//         uint32_t button;
//         uint8_t axis;
//     };
// };

// class input_device {
//     int controller;

// public:
//     void set_controller(int controller);
//     int get_controller();
//     virtual input_action map_event(SDL_Event* event) = 0;
// };

struct instance {
    SDL_Window* window = nullptr;
    SDL_GPUDevice* device = nullptr;
    SDL_AudioStream* stream = nullptr;

    struct ps2_state* ps2 = nullptr;

    unsigned int window_width = 960;
    unsigned int window_height = 720;
    unsigned int texture_width;
    unsigned int texture_height;
    uint32_t* texture_buf = nullptr;

    unsigned int renderer_backend = RENDERER_SOFTWARE_THREAD;
    renderer_state* ctx = nullptr;

    uint32_t ps2_memory_card_icon_width = 0;
    uint32_t ps1_memory_card_icon_width = 0;
    uint32_t pocketstation_icon_width = 0;
    uint32_t iris_icon_width = 0;
    uint32_t ps2_memory_card_icon_height = 0;
    uint32_t ps1_memory_card_icon_height = 0;
    uint32_t pocketstation_icon_height = 0;
    uint32_t iris_icon_height = 0;
    SDL_GPUTexture* ps2_memory_card_icon_tex = nullptr;
    SDL_GPUTexture* ps1_memory_card_icon_tex = nullptr;
    SDL_GPUTexture* pocketstation_icon_tex = nullptr;
    SDL_GPUTexture* iris_icon_tex = nullptr;

    ImFont* font_small_code = nullptr;
    ImFont* font_code = nullptr;
    ImFont* font_small = nullptr;
    ImFont* font_heading = nullptr;
    ImFont* font_body = nullptr;
    ImFont* font_icons = nullptr;
    ImFont* font_icons_big = nullptr;
    ImFont* font_black = nullptr;

    std::string elf_path = "";
    std::string boot_path = "";
    std::string bios_path = "";
    std::string rom1_path = "";
    std::string rom2_path = "";
    std::string disc_path = "";
    std::string pref_path = "";
    std::string mcd0_path = "";
    std::string mcd1_path = "";
    std::string snap_path = "";
    std::string ini_path = "";

    bool core0_mute[24] = { false };
    bool core1_mute[24] = { false };
    int core0_solo = -1;
    int core1_solo = -1;

    bool open = false;
    bool pause = true;
    bool step = false;
    bool step_over = false;
    bool step_out = false;
    uint32_t step_over_addr = 0;

    bool show_ee_control = false;
    bool show_ee_state = false;
    bool show_ee_logs = false;
    bool show_ee_interrupts = false;
    bool show_ee_dmac = false;
    bool show_iop_control = false;
    bool show_iop_state = false;
    bool show_iop_logs = false;
    bool show_iop_interrupts = false;
    bool show_iop_modules = false;
    bool show_iop_dma = false;
    bool show_gs_debugger = false;
    bool show_spu2_debugger = false;
    bool show_memory_viewer = false;
    bool show_status_bar = true;
    bool show_breakpoints = false;
    bool show_settings = false;
    bool show_pad_debugger = false;
    bool show_symbols = false;
    bool show_threads = false;
    bool show_memory_card_tool = false;
    bool show_imgui_demo = false;
    bool show_vu_disassembler = false;
    bool show_overlay = false;

    // Special windows
    bool show_bios_setting_window = false;
    bool show_about_window = false;

    bool fullscreen = 0;
    int aspect_mode = RENDERER_ASPECT_AUTO;
    bool bilinear = true;
    bool integer_scaling = false;
    float scale = 1.5f;
    int window_mode = 0;
    bool ee_control_follow_pc = true;
    bool iop_control_follow_pc = true;
    uint32_t ee_control_address = 0;
    uint32_t iop_control_address = 0;
    bool skip_fmv = false;

    std::deque <std::string> recents;

    bool dump_to_file = true;
    std::string settings_path = "";

    int frames = 0;
    float fps = 0.0f;
    unsigned int ticks = 0;
    int menubar_height = 0;
    bool mute = false;
    float volume = 1.0f;

    bool limit_fps = true;
    float fps_cap = 60.0f;

    std::string loaded = "";

    std::vector <std::string> ee_log = { "" };
    std::vector <std::string> iop_log = { "" };

    std::vector <iris::breakpoint> breakpoints = {};
    std::deque <iris::notification> notifications = {};

    struct ds_state* ds[2] = { nullptr };
    struct mcd_state* mcd[2] = { nullptr };

    // input_device* device[2];

    float drop_file_alpha = 0.0f;
    float drop_file_alpha_delta = 0.0f;
    float drop_file_alpha_target = 0.0f;
    bool drop_file_active = false;

    // Debug
    std::vector <elf_symbol> symbols;
    std::vector <uint8_t> strtab;

    std::vector <spu2_sample> audio_buf;

    float avg_fps;
    float avg_frames;
    int screenshot_counter = 0;
};

int init_audio(iris::instance* iris);
int init_settings(iris::instance* iris, int argc, const char* argv[]);
void cli_check_for_help_version(iris::instance* iris, int argc, const char* argv[]);
void close_settings(iris::instance* iris);

void update(iris::instance* iris);
void update_window(iris::instance* iris);

void show_main_menubar(iris::instance* iris);
void show_ee_control(iris::instance* iris);
void show_ee_state(iris::instance* iris);
void show_ee_logs(iris::instance* iris);
void show_ee_interrupts(iris::instance* iris);
void show_ee_dmac(iris::instance* iris);
void show_iop_control(iris::instance* iris);
void show_iop_state(iris::instance* iris);
void show_iop_logs(iris::instance* iris);
void show_iop_interrupts(iris::instance* iris);
void show_iop_modules(iris::instance* iris);
void show_iop_dma(iris::instance* iris);
void show_gs_debugger(iris::instance* iris);
void show_spu2_debugger(iris::instance* iris);
void show_memory_viewer(iris::instance* iris);
void show_vu_disassembler(iris::instance* iris);
void show_status_bar(iris::instance* iris);
void show_breakpoints(iris::instance* iris);
void show_about_window(iris::instance* iris);
void show_settings(iris::instance* iris);
void show_pad_debugger(iris::instance* iris);
void show_symbols(iris::instance* iris);
void show_threads(iris::instance* iris);
void show_overlay(iris::instance* iris);
void show_memory_card_tool(iris::instance* iris);
void show_bios_setting_window(iris::instance* iris);
// void show_gamelist(iris::instance* iris);

void handle_keydown_event(iris::instance* iris, SDL_KeyboardEvent& key);
void handle_keyup_event(iris::instance* iris, SDL_KeyboardEvent& key);
void handle_scissor_event(void* udata);
void handle_drag_and_drop_event(void* udata, const char* path);
void handle_ee_tty_event(void* udata, char c);
void handle_iop_tty_event(void* udata, char c);

void handle_animations(iris::instance* iris);

void push_info(iris::instance* iris, std::string text);

void add_recent(iris::instance* iris, std::string file);
int open_file(iris::instance* iris, std::string file);

bool save_screenshot(iris::instance* iris, std::string path);
std::string get_default_screenshot_filename(iris::instance* iris);

void audio_update(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);

}