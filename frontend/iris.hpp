#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <array>
#include <deque>

#include "gs/renderer/renderer.hpp"

#include <SDL3/SDL.h>
#include <volk.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include "ps2.h"
#include "config.hpp"

namespace iris {

#define RENDER_ASPECT_NATIVE 0
#define RENDER_ASPECT_STRETCH 1
#define RENDER_ASPECT_STRETCH_KEEP 2
#define RENDER_ASPECT_4_3 3
#define RENDER_ASPECT_16_9 4
#define RENDER_ASPECT_5_4 5
#define RENDER_ASPECT_AUTO 6

#define IRIS_THEME_GRANITE 0
#define IRIS_THEME_IMGUI_DARK 1
#define IRIS_THEME_IMGUI_LIGHT 2
#define IRIS_THEME_IMGUI_CLASSIC 3

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

struct vertex {
    struct {
        float x, y;
    } pos, uv;

    static constexpr const VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription binding_description = {};

        binding_description.binding = 0;
        binding_description.stride = sizeof(vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return binding_description;
    }

    static constexpr const std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions = {};

        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[0].offset = offsetof(vertex, pos);

        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[1].offset = offsetof(vertex, uv);

        return attribute_descriptions;
    }
};

struct shader_pass {
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkImageView input = VK_NULL_HANDLE;
    VkShaderModule vert_shader = VK_NULL_HANDLE;
    VkShaderModule frag_shader = VK_NULL_HANDLE;
};

struct instance {
    SDL_Window* window = nullptr;
    SDL_AudioStream* stream = nullptr;

    // Vulkan state
    std::vector <VkExtensionProperties> instance_extensions;
    std::vector <VkLayerProperties> instance_layers;
    std::vector <VkExtensionProperties> device_extensions;
    std::vector <VkLayerProperties> device_layers;
    std::vector <const char*> enabled_instance_extensions;
    std::vector <const char*> enabled_instance_layers;
    std::vector <const char*> enabled_device_extensions;
    std::vector <const char*> enabled_device_layers;
    VkApplicationInfo app_info = {};
    VkInstanceCreateInfo instance_create_info = {};
    VkDeviceCreateInfo device_create_info = {};
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceFeatures2 device_features = {};
    VkDeviceQueueCreateInfo queue_create_info = {};
    uint32_t queue_family = (uint32_t)-1;
    VkQueue queue = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    ImGui_ImplVulkanH_Window main_window_data = {};
    uint32_t min_image_count = 2;
    bool swapchain_rebuild = false;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    float main_scale = 1;
    VkPhysicalDeviceVulkan11Features vulkan_11_features = {};
    VkPhysicalDeviceVulkan12Features vulkan_12_features = {};
    VkPhysicalDeviceSubgroupSizeControlFeatures subgroup_size_control_features = {};
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkClearValue clear_value = {};
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;
    VkBuffer vertex_staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_staging_buffer_memory = VK_NULL_HANDLE;
    VkDeviceSize vertex_buffer_size = 0;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;
    std::array <vertex, 4> vertices = {};
    std::array <uint16_t, 6> indices = {};
    renderer_image image = {};

    // Multipass shader stuff
    std::vector <shader_pass> shader_passes = {};
    VkDescriptorSetLayout shader_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet shader_descriptor_set = VK_NULL_HANDLE;
    VkShaderModule default_vert_shader = VK_NULL_HANDLE;

    struct {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    } shader_framebuffers[2];

    struct ps2_state* ps2 = nullptr;

    unsigned int window_width = 960;
    unsigned int window_height = 720;
    unsigned int render_width = 640;
    unsigned int render_height = 480;

    unsigned int renderer_backend = RENDERER_BACKEND_HARDWARE;
    renderer_state* renderer = nullptr;

    uint32_t ps2_memory_card_icon_width = 0;
    uint32_t ps1_memory_card_icon_width = 0;
    uint32_t pocketstation_icon_width = 0;
    uint32_t iris_icon_width = 0;
    uint32_t ps2_memory_card_icon_height = 0;
    uint32_t ps1_memory_card_icon_height = 0;
    uint32_t pocketstation_icon_height = 0;
    uint32_t iris_icon_height = 0;
    VkDescriptorSet ps2_memory_card_icon_ds = VK_NULL_HANDLE;
    VkDescriptorSet ps1_memory_card_icon_ds = VK_NULL_HANDLE;
    VkDescriptorSet pocketstation_icon_ds = VK_NULL_HANDLE;
    VkDescriptorSet iris_icon_ds = VK_NULL_HANDLE;

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
    std::string nvram_path = "";
    std::string disc_path = "";
    std::string pref_path = "";
    std::string mcd0_path = "";
    std::string mcd1_path = "";
    std::string snap_path = "";
    std::string flash_path = "";
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

    bool fullscreen = false;
    int aspect_mode = RENDER_ASPECT_AUTO;
    bool bilinear = true;
    bool integer_scaling = false;
    float scale = 1.5f;
    int window_mode = 0;
    bool ee_control_follow_pc = true;
    bool iop_control_follow_pc = true;
    uint32_t ee_control_address = 0;
    uint32_t iop_control_address = 0;
    bool skip_fmv = false;
    int system = PS2_SYSTEM_AUTO;
    int theme = IRIS_THEME_GRANITE;

    std::deque <std::string> recents;

    bool dump_to_file = true;
    std::string settings_path = "";

    int frames = 0;
    float fps = 0.0f;
    unsigned int ticks = 0;
    int menubar_height = 0;
    bool mute = false;
    bool prev_mute = false;
    float volume = 1.0f;
    int timescale = 8;
    bool mute_adma = true;

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

namespace audio {
    bool init(iris::instance* iris);
    void close(iris::instance* iris);
    void update(void* udata, SDL_AudioStream* stream, int additional_amount, int total_amount);
    bool mute(iris::instance* iris);
    void unmute(iris::instance* iris);
}

namespace settings {
    bool init(iris::instance* iris, int argc, const char* argv[]);
    bool check_for_quick_exit(int argc, const char* argv[]);
    void close(iris::instance* iris);
}

namespace imgui {
    bool init(iris::instance* iris);
    void set_theme(iris::instance* iris, int theme, bool set_bg_color = true);
    bool render_frame(iris::instance* iris, ImDrawData* draw_data);
    bool present_frame(iris::instance* iris);
    void cleanup(iris::instance* iris);

    // Wrapper for ImGui::Begin that sets a default size
    bool BeginEx(const char* name, bool* p_open, ImGuiWindowFlags flags = 0);
}

namespace vulkan {
    bool init(iris::instance* iris, bool enable_validation = false);
    void cleanup(iris::instance* iris);
}

namespace platform {
    bool init(iris::instance* iris);
    void destroy(iris::instance* iris);
}

namespace elf {
    bool load_symbols_from_disc(iris::instance* iris);
    bool load_symbols_from_file(iris::instance* iris, std::string path);
}

namespace emu {
    bool init(iris::instance* iris);
    void destroy(iris::instance* iris);
}

namespace render {
    bool init(iris::instance* iris);
    void destroy(iris::instance* iris);
    bool render_frame(iris::instance* iris, VkCommandBuffer command_buffer, VkFramebuffer framebuffer);
    bool save_screenshot(iris::instance* iris, std::string path);
}

iris::instance* create();
bool init(iris::instance* iris, int argc, const char* argv[]);
void destroy(iris::instance* iris);
SDL_AppResult handle_events(iris::instance* iris, SDL_Event* event);
SDL_AppResult update(iris::instance* iris);
void update_window(iris::instance* iris);
int get_menubar_height(iris::instance* iris);

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

}