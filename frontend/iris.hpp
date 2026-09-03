#pragma once

#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <array>
#include <deque>
#include <atomic>
#include <mutex>

#include "gs/renderer/renderer.hpp"
#include "gs/renderer/config.hpp"

#include <SDL3/SDL.h>
#include <volk.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include "ps2_decl.hpp"
#include "iop/spu2_decl.hpp"
#include "iop/usb.hpp"
#include "config.hpp"
#include "slirp.hpp"

#include "emu.hpp"
#include "log.hpp"
#include "bidirectional_map.hpp"
#include "debug.hpp"
#include "notifications.hpp"
#include "elf.hpp"
#include "emu.hpp"
#include "input.hpp"
#include "vulkan.hpp"
#include "shaders.hpp"
#include "imgui.hpp"
#include "render.hpp"
#include "platform.hpp"
#include "audio.hpp"
#include "settings.hpp"
#include "gamelist.hpp"
#include "applets.hpp"
#include "cli.hpp"

namespace iris::gs::dump { struct Dump; }
namespace iris::dev::ds { struct Ds; }
namespace iris::dev::mcd { struct Mcd; }

namespace iris {

struct Instance;

struct Instance {
    // Vulkan device, swapchain and the multipass shader plumbing
    struct {
        std::vector <VkExtensionProperties> instance_extensions;
        std::vector <VkLayerProperties> instance_layers;
        std::vector <VkExtensionProperties> device_extensions;
        std::vector <VkLayerProperties> device_layers;
        std::vector <const char*> enabled_instance_extensions;
        std::vector <const char*> enabled_instance_layers;
        std::vector <const char*> enabled_device_extensions;
        std::vector <const char*> enabled_device_layers;
        std::vector <VulkanGpu> vulkan_gpus;
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
        bool device_lost = false;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        float main_scale = 1;
        VkPhysicalDeviceVulkan11Features vulkan_11_features = {};
        VkPhysicalDeviceVulkan12Features vulkan_12_features = {};
        VkPhysicalDeviceSubgroupSizeControlFeatures subgroup_size_control_features = {};
        VkPhysicalDeviceSynchronization2Features synchronization2_features = {};
        VkPhysicalDeviceFaultFeaturesEXT fault_features = {};
        bool device_fault_supported = false;
        bool device_fault_dumped = false;
        VkSampler sampler[3] = { VK_NULL_HANDLE };
        bool cubic_supported = false;
        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        std::vector <VkDescriptorSet> descriptor_sets = {};
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VkRenderPass render_pass = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkClearValue clear_value = { 11 / 255.0f, 11 / 255.0f, 13 / 255.0f, 1.0 };
        VkBuffer vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;
        VkBuffer vertex_staging_buffer = VK_NULL_HANDLE;
        VkDeviceMemory vertex_staging_buffer_memory = VK_NULL_HANDLE;
        VkDeviceSize vertex_buffer_size = 0;
        VkBuffer index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;
        std::array <Vertex, 4> vertices = {};
        std::array <uint16_t, 6> indices = {};
        gs::renderer::Image image = {};
        gs::renderer::Image output_image = {};
        std::vector <std::string> shader_passes_pending;
        std::vector <shaders::Pass*> shader_passes = {};
        VkDescriptorSetLayout shader_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSet shader_descriptor_set = VK_NULL_HANDLE;
        std::vector <VkDescriptorSet> shader_descriptor_sets = {};
        VkShaderModule default_vert_shader = VK_NULL_HANDLE;
        struct {
            VkImage image = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
        } shader_framebuffers[2];
        std::vector <std::array <VkFramebuffer, 2>> shader_pass_framebuffers = {};

        struct {
            VkImage image = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
        } fsr_targets[2];

        VkFormat fsr_format = VK_FORMAT_UNDEFINED;
        int fsr_width = 0;
        int fsr_height = 0;
        shaders::Pass* fsr_easu = nullptr;
        shaders::Pass* fsr_rcas = nullptr;
        int vulkan_physical_device = -1;
        int vulkan_selected_device_index = 0;
        bool vulkan_enable_validation_layers = false;
    } vk;

    // ImGui fonts, colors, textures and window visibility
    struct {
        Texture ps2_memory_card_icon = {};
        Texture ps1_memory_card_icon = {};
        Texture pocketstation_icon = {};
        Texture dualshock2_icon = {};
        Texture iris_icon = {};
        ImFont* font_small_code = nullptr;
        ImFont* font_code = nullptr;
        ImFont* font_small = nullptr;
        ImFont* font_label = nullptr;
        ImFont* font_heading = nullptr;
        ImFont* font_body = nullptr;
        ImFont* font_icons = nullptr;
        ImFont* font_icons_big = nullptr;
        ImFont* font_black = nullptr;
        bool show_status_bar = true;
        bool show_imgui_demo = false;
        bool show_overlay = false;
        bool show_gamelist = false;
        int theme = imgui::GRANITE_NEO;
        bool imgui_enable_viewports = true;
        int codeview_color_scheme = 0;
        ImColor codeview_color_text = IM_COL32(131, 148, 150, 255);
        ImColor codeview_color_comment = IM_COL32(88, 110, 117, 255);
        ImColor codeview_color_mnemonic = IM_COL32(211, 167, 30, 255);
        ImColor codeview_color_number = IM_COL32(138, 143, 226, 255);
        ImColor codeview_color_register = IM_COL32(68, 169, 240, 255);
        ImColor codeview_color_other = IM_COL32(89, 89, 89, 255);
        ImColor codeview_color_background = IM_COL32(30, 30, 30, 255);
        ImColor codeview_color_highlight = IM_COL32(75, 75, 75, 255);
        float codeview_font_scale = 1.0f;
        bool codeview_use_theme_background = true;
        std::unordered_map <std::string, Texture> covers = {};
        int menubar_height = 0;
        float ui_scale = 1.0f;
        int docking_mode = 0;
        std::deque <Notification> notifications = {};
        float dim_target_alpha = 0.0f;
        float dim_current_alpha = 0.0f;
        size_t dim_start = 0;
        size_t dim_ms = 0;
        bool dim_end = false;
        bool dim_active = false;
        bool drop_file_active = false;

        ImVec2 drop_pos = ImVec2(-1.0f, -1.0f);
        ImVec2 drop_insert_min = ImVec2(0.0f, 0.0f);
        ImVec2 drop_insert_max = ImVec2(0.0f, 0.0f);
        bool loading_file_active = false;
        std::string loading_target = "";
    } ui;

    // Gamepads, keyboard mappings and the emulated pads/cards
    struct {
        uint64_t double_click_interval = 500;
        uint64_t double_click_counter = 0;
        InputDevice* input_devices[2] = { nullptr };
        std::unordered_map <SDL_JoystickID, SDL_Gamepad*> gamepads;
        std::vector <Mapping> input_maps = {};
        int input_map[2] = { -1, -1 };
        int usb_devices[2] = { usb::USB_DEVICE_NONE, usb::USB_DEVICE_NONE };
        std::string usb_msd_paths[2] = { "", "" };
        InputEvent last_input_event = {};
        bool last_input_event_read = true;
        float last_input_event_value = 0.0f;
        dev::ds::Ds* ds[2] = { nullptr };
        dev::mcd::Mcd* mcd[2] = { nullptr };
        int mcd_slot_type[2] = { 0 };
    } input;

    // Host audio streams and per-core mixing
    struct {
        SDL_AudioStream* streams[2] = { nullptr };
        SDL_AudioDeviceID audio_device;
        bool core0_mute[24] = { false };
        bool core1_mute[24] = { false };
        int core0_solo = -1;
        int core1_solo = -1;
        bool mute = false;
        bool prev_mute = false;
        float volume = 1.0f;
        bool mute_adma = true;
        std::vector <spu2::Sample> audio_buf;
        double adma_position = 0.0;
        double voice_position = 0.0;
        spu2::Sample voice_prev = {};
        spu2::Sample voice_next = {};
    } audio;

    // Debugger state: stepping, breakpoints, logs, symbols, GS dumps
    struct {
        bool pause = true;
        bool step = false;
        bool step_over = false;
        bool step_out = false;
        uint32_t step_over_addr = 0;
        bool ee_control_follow_pc = true;
        bool iop_control_follow_pc = true;
        uint32_t ee_control_address = 0;
        uint32_t iop_control_address = 0;
        std::vector <std::string> ee_log = { "" };
        std::vector <std::string> iop_log = { "" };
        std::vector <std::string> sysmem_log = { "" };
        std::vector <Breakpoint> breakpoints = {};
        uint32_t pushed_ee[MAX_EXEC_BREAKPOINTS] = {};
        uint32_t pushed_iop[MAX_EXEC_BREAKPOINTS] = {};
        int pushed_ee_count = -1;
        int pushed_iop_count = -1;
        std::vector <elf::Symbol> symbols;
        std::vector <uint8_t> strtab;
        gs::dump::Dump* gsdump = nullptr;
        bool gsdump_armed = false;
        int gsdump_delay_remaining = 0;
        int gsdump_frames_remaining = 0;
        std::string gsdump_path = "";
        std::string gsdump_serial = "";
        bool gsdump_prev_pause = false;
    } debug;

    // Filesystem locations
    struct {
        std::string elf_path = "";
        std::string boot_path = "";
        std::string bios_path = "";
        std::string arcade_bios_paths[emu::ARCADE_BIOS_COUNT] = {};
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
        std::string gcdb_path = "";
        std::string hdd_path = "";
        std::string hdd_id_path = "";
        std::string dongle_black_path = "";
        std::string dongle_white_path = "";
        std::string mecha_card_id_path = "";
        std::string mecha_civ_path = "";
        std::string mecha_cks_path = "";
        std::string mecha_eks_path = "";
        std::string mecha_kek_path = "";
        std::string mecha_kelf_kbit_path = "";
        std::string mecha_kelf_kc_path = "";
        bool auto_paths = true;

        bool pinned_rom1 = false;
        bool pinned_rom2 = false;
        bool pinned_nvram = false;
        std::string host_path = "";
        bool host_from_elf = false;
        std::string host_elf_dir = "";
        std::vector <std::pair <std::string, std::string>> device_maps;
        std::string settings_path = "";
        std::string mappings_path = "";
        std::string log_path = "";
    } paths;

    Applets applets;

    cli::State cli;

    SDL_Window* window = nullptr;

    ps2::Ps2* ps2 = nullptr;
    logger::Logger* logger = nullptr;

    // Logger sources
    struct {
        LogSource iris;
        LogSource vulkan;
        LogSource render;
        LogSource shaders;
        LogSource imgui;
        LogSource elf;
        LogSource settings;
        LogSource emu;
        LogSource ee_tty;
        LogSource iop_tty;
        LogSource sysmem_tty;
        LogSource input;
        LogSource audio;
        LogSource slirp;
        LogSource net;
        LogSource gamelist;
        LogSource platform;
        LogSource ui;
    } log;

    std::deque <LogEntry> log_history;
    std::mutex log_mutex;
    bool log_to_console = true;
    bool log_to_file = true;
    logger::Level log_level = logger::LEVEL_INFO;
    FILE* log_file = nullptr;
    std::string log_file_path = "";
    std::vector <size_t> frontend_log_sources;

    bool fatal_error = false;
    std::string fatal_error_text;
    unsigned int window_width = 960;
    unsigned int window_height = 720;
    unsigned int render_width = 640;
    unsigned int render_height = 480;
    float render_x = 0.0f;
    float render_y = 0.0f;
    unsigned int renderer_backend = gs::renderer::BACKEND_HARDWARE;
    gs::renderer::Renderer* renderer = nullptr;
    uint8_t mac_address[6] = { 0 };
    slirp::Config slirp_config;
    bool open = false;

    bool fullscreen = false;
    bool no_decorations = false;
    bool remember_window_size = false;
    int aspect_mode = render::AUTO;
    int filter = render::BILINEAR;
    bool integer_scaling = false;
    float fsr_sharpness = 0.25f;
    float scale = 1.5f;
    int window_mode = 0;
    bool skip_fmv = false;
    int system = ps2::AUTO;
    bool enable_shaders = false;
    bool autostart = true;
    bool cache_arcade_files = false;
    bool arcade_dongle_boot = false;

    bool system_2x6_rgb_level = false;
    bool system_2x6_monitor_frequency = false;
    bool system_2x6_video_sync = false;
    bool enable_magicgate = true;

    int p2io_input_type = 4;
    int angle = 0;
    bool flip_x = false;
    bool flip_y = false;
    std::deque <Recent> recents;
    bool dump_to_file = true;
    bool headless = false;
    bool snap_on_exit = false;
    int frames = 0;
    float fps = 0.0f;
    unsigned int ticks = 0;
    int timescale = 1;
    int present_mode = render::FPS_60;
    int capped_present_mode = render::FPS_60;
    int screenshot_format = render::PNG;
    int screenshot_jpg_quality_mode = render::MAXIMUM;
    int screenshot_jpg_quality = 50;
    int screenshot_mode = render::INTERNAL;
    bool screenshot_shader_processing = false;
    bool limit_fps = true;
    float fps_cap = 60.0f;
    std::chrono::high_resolution_clock::time_point frame_deadline;
    std::string loaded = "";
    std::string arcade_id = "";

    std::atomic <bool> load_ready = false;
    int load_result = 0;
    std::string load_pending_name = "";
    bool load_start_pending = false;
    bool load_pending_boot = false;
    bool load_pending_arcade = false;
    std::string load_pending_file = "";

    float avg_fps;
    float avg_frames;
    int screenshot_counter = 0;

    gs::renderer::HardwareConfig hardware_backend_config;

#ifdef _WIN32
    int windows_titlebar_style = IRIS_TITLEBAR_SEAMLESS;
    bool windows_enable_borders = true;
#endif

#ifdef IRIS_HAS_DARK_TITLEBAR
    bool dark_titlebar = true;
#endif

};

Instance* create();
bool init(Instance* iris);
void destroy(Instance* iris);
SDL_AppResult handle_events(Instance* iris, SDL_Event* event);
SDL_AppResult update(Instance* iris);
void update_window(Instance* iris);
int get_menubar_height(Instance* iris);
int get_menubar_inset(Instance* iris);

void show_main_menubar(Instance* iris);
void show_status_bar(Instance* iris);
void show_fatal_error(Instance* iris);
void show_overlay(Instance* iris);
void show_gamelist(Instance* iris);

void handle_keydown_event(Instance* iris, SDL_Event* event);
void handle_keyup_event(Instance* iris, SDL_Event* event);
void handle_scissor_event(void* udata);
void handle_drag_and_drop_event(void* udata, const char* path);
void handle_ee_tty_event(void* udata, char c);
void handle_iop_tty_event(void* udata, char c);
void handle_sysmem_tty_event(void* udata, char c);
void handle_log_event(void* udata, logger::Level level, const logger::Source& source, const std::string& text);

void init_logger(Instance* iris);

void handle_animations(Instance* iris);

void push_info(Instance* iris, std::string text);

void add_recent(Instance* iris, std::string file, RecentType type);
int open_file(Instance* iris, std::string file);

}