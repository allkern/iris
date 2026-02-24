#pragma once

#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <array>
#include <deque>

#include "gs/renderer/renderer.hpp"
#include "gs/renderer/config.hpp"

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
#define IRIS_THEME_CHERRY 4
#define IRIS_THEME_SOURCE 5

#define IRIS_SCREENSHOT_FORMAT_PNG 0
#define IRIS_SCREENSHOT_FORMAT_BMP 1
#define IRIS_SCREENSHOT_FORMAT_JPG 2
#define IRIS_SCREENSHOT_FORMAT_TGA 3

#define IRIS_SCREENSHOT_MODE_INTERNAL 0
#define IRIS_SCREENSHOT_MODE_DISPLAY 1

#define IRIS_SCREENSHOT_JPG_QUALITY_MINIMUM 0
#define IRIS_SCREENSHOT_JPG_QUALITY_LOW 1
#define IRIS_SCREENSHOT_JPG_QUALITY_MEDIUM 2
#define IRIS_SCREENSHOT_JPG_QUALITY_HIGH 3
#define IRIS_SCREENSHOT_JPG_QUALITY_MAXIMUM 4
#define IRIS_SCREENSHOT_JPG_QUALITY_CUSTOM 5 

#define IRIS_CODEVIEW_COLOR_SCHEME_SOLARIZED_DARK 0
#define IRIS_CODEVIEW_COLOR_SCHEME_SOLARIZED_LIGHT 1
#define IRIS_CODEVIEW_COLOR_SCHEME_ONE_DARK_PRO 2
#define IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_LATTE 3
#define IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_FRAPPE 4
#define IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_MACCHIATO 5
#define IRIS_CODEVIEW_COLOR_SCHEME_CATPPUCCIN_MOCHA 6

#define IRIS_TITLEBAR_DEFAULT 0
#define IRIS_TITLEBAR_SEAMLESS 1

class instance;

// class widget {
// public:
//     virtual bool init(iris::instance* iris) = 0;
//     virtual void render(iris::instance* iris) = 0;
//     virtual ~widget() = default;
// };

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

enum {
    INPUT_CONTROLLER_DUALSHOCK2

    // Large To-do list here, we're missing the Namco GunCon
    // controllers, JogCon, NegCon, Buzz! Buzzer, the Train
    // controllers, Taiko Drum Master controller, the Dance Dance
    // Revolution mat, Guitar Hero controllers, etc.
};

struct input_device {
    int m_slot;

    void set_slot(int slot) {
        this->m_slot = slot;
    }

    int get_slot() {
        return this->m_slot;
    }

    virtual int get_type() = 0;
    virtual void handle_event(iris::instance* iris, SDL_Event* event) = 0;
};

class keyboard_device : public input_device {
public:
    int get_type() override {
        return 0;
    }

    void handle_event(iris::instance* iris, SDL_Event* event) override;
};

class gamepad_device : public input_device {
    SDL_JoystickID id;

public:
    gamepad_device(SDL_JoystickID id) : id(id) {}

    int get_type() override {
        return 1;
    }

    SDL_JoystickID get_id() {
        return id;
    }

    void handle_event(iris::instance* iris, SDL_Event* event) override;
};

union input_event {
    struct {
        uint32_t id;
        uint32_t type;
    };

    uint64_t u64;
};

enum event {
    IRIS_EVENT_KEYBOARD,
    IRIS_EVENT_GAMEPAD_BUTTON,
    IRIS_EVENT_GAMEPAD_AXIS_POS,
    IRIS_EVENT_GAMEPAD_AXIS_NEG
};

enum input_action : uint32_t {
    IRIS_DS_BT_CROSS,
    IRIS_DS_BT_CIRCLE,
    IRIS_DS_BT_SQUARE,
    IRIS_DS_BT_TRIANGLE,
    IRIS_DS_BT_START,
    IRIS_DS_BT_SELECT,
    IRIS_DS_BT_ANALOG,
    IRIS_DS_BT_UP,
    IRIS_DS_BT_DOWN,
    IRIS_DS_BT_LEFT,
    IRIS_DS_BT_RIGHT,
    IRIS_DS_BT_L1,
    IRIS_DS_BT_R1,
    IRIS_DS_BT_L2,
    IRIS_DS_BT_R2,
    IRIS_DS_BT_L3,
    IRIS_DS_BT_R3,
    IRIS_DS_AX_RIGHTV_POS,
    IRIS_DS_AX_RIGHTV_NEG,
    IRIS_DS_AX_RIGHTH_POS,
    IRIS_DS_AX_RIGHTH_NEG,
    IRIS_DS_AX_LEFTV_POS,
    IRIS_DS_AX_LEFTV_NEG,
    IRIS_DS_AX_LEFTH_POS,
    IRIS_DS_AX_LEFTH_NEG
};

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

struct texture {
    int width = 0, height = 0, stride = 0;
    VkDeviceSize image_size = 0;
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

namespace shaders {
    class pass;
}

struct vulkan_gpu {
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    VkPhysicalDevice device = VK_NULL_HANDLE;
    std::string name = "";
    uint32_t api_version = 0;
};

template <typename Key, typename Value> class bidirectional_map {
    std::unordered_map <Key, Value> m_forward_map;
    std::unordered_map <Value, Key> m_reverse_map;

public:
    void insert(const Key& key, const Value& value) {
        m_forward_map[key] = value;
        m_reverse_map[value] = key;
    }

    std::unordered_map <Key, Value>& forward_map() {
        return m_forward_map;
    }

    std::unordered_map <Value, Key>& reverse_map() {
        return m_reverse_map;
    }

    bool erase_by_key(const Key& key) {
        auto it = m_forward_map.find(key);
        if (it != m_forward_map.end()) {
            Value value = it->second;
            m_forward_map.erase(it);
            m_reverse_map.erase(value);
            return true;
        }
        return false;
    }

    bool erase_by_value(const Value& value) {
        auto it = m_reverse_map.find(value);
        if (it != m_reverse_map.end()) {
            Key key = it->second;
            m_reverse_map.erase(it);
            m_forward_map.erase(key);
            return true;
        }
        return false;
    }

    void clear() {
        m_forward_map.clear();
        m_reverse_map.clear();
    }

    Value* get_value(const Key& key) {
        auto it = m_forward_map.find(key);
        if (it != m_forward_map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    Key* get_key(const Value& value) {
        auto it = m_reverse_map.find(value);
        if (it != m_reverse_map.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

struct mapping {
    std::string name;
    bidirectional_map <uint64_t, input_action> map;
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
    std::vector <vulkan_gpu> vulkan_gpus;
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
    VkSampler sampler[3] = { VK_NULL_HANDLE };
    bool cubic_supported = false;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkClearValue clear_value = { 0.11, 0.11, 0.11, 1.0 };
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
    renderer_image output_image = {};

    // Multipass shader stuff
    std::vector <std::string> shader_passes_pending;
    std::vector <shaders::pass*> shader_passes = {};
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

    texture ps2_memory_card_icon = {};
    texture ps1_memory_card_icon = {};
    texture pocketstation_icon = {};
    texture dualshock2_icon = {};
    texture iris_icon = {};

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
    std::string gcdb_path = "";

    uint8_t mac_address[6] = { 0 };

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
    bool show_sysmem_logs = false;
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
    bool show_memory_search = false;

    // Special windows
    bool show_bios_setting_window = false;
    bool show_about_window = false;

    bool fullscreen = false;
    int aspect_mode = RENDER_ASPECT_AUTO;
    int filter = 1;
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
    bool enable_shaders = false;
    int vulkan_physical_device = -1;
    int vulkan_selected_device_index = 0;
    bool vulkan_enable_validation_layers = false;
    bool imgui_enable_viewports = false;
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

    std::deque <std::string> recents;

    bool dump_to_file = true;
    std::string settings_path = "";
    std::string mappings_path = "";

    int frames = 0;
    float fps = 0.0f;
    unsigned int ticks = 0;
    int menubar_height = 0;
    bool mute = false;
    bool prev_mute = false;
    float volume = 1.0f;
    int timescale = 8;
    bool mute_adma = true;
    float ui_scale = 1.0f;
    int screenshot_format = IRIS_SCREENSHOT_FORMAT_PNG;
    int screenshot_jpg_quality_mode = IRIS_SCREENSHOT_JPG_QUALITY_MAXIMUM;
    int screenshot_jpg_quality = 50;
    int screenshot_mode = IRIS_SCREENSHOT_MODE_INTERNAL;
    int docking_mode = 0;
    bool screenshot_shader_processing = false;
    input_device* input_devices[2] = { nullptr };
    std::unordered_map <SDL_JoystickID, SDL_Gamepad*> gamepads;
    std::vector <mapping> input_maps = {};
    int input_map[2] = { -1, -1 };
    input_event last_input_event = {};
    bool last_input_event_read = true;
    float last_input_event_value = 0.0f;

    bool limit_fps = true;
    float fps_cap = 60.0f;

    std::string loaded = "";

    std::vector <std::string> ee_log = { "" };
    std::vector <std::string> iop_log = { "" };
    std::vector <std::string> sysmem_log = { "" };

    std::vector <iris::breakpoint> breakpoints = {};
    std::deque <iris::notification> notifications = {};

    struct ds_state* ds[2] = { nullptr };
    struct mcd_state* mcd[2] = { nullptr };
    int mcd_slot_type[2] = { 0 };

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

    // Renderer configs
    hardware_config hardware_backend_config;

    // Windows-specific settings
#ifdef _WIN32
    int windows_titlebar_style = IRIS_TITLEBAR_DEFAULT;
    bool windows_enable_borders = true;
    bool windows_dark_mode = true;
#endif
};

struct push_constants {
    float resolution[2];
    int frame;
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

namespace shaders {
    class pass {
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        VkRenderPass m_render_pass = VK_NULL_HANDLE;
        VkImageView m_input = VK_NULL_HANDLE;
        VkShaderModule m_vert_shader = VK_NULL_HANDLE;
        VkShaderModule m_frag_shader = VK_NULL_HANDLE;
        iris::instance* m_iris = nullptr;
        std::string m_id;

    public:
        void destroy();
        bool init(iris::instance* iris, const void* data, size_t size, std::string id);

        void swap(pass& rhs) {
            VkPipelineLayout pipeline_layout = m_pipeline_layout;
            VkPipeline pipeline = m_pipeline;
            VkRenderPass render_pass = m_render_pass;
            VkImageView input = m_input;
            VkShaderModule vert_shader = m_vert_shader;
            VkShaderModule frag_shader = m_frag_shader;
            iris::instance* iris = m_iris;
            std::string id = m_id;

            m_pipeline_layout = rhs.m_pipeline_layout;
            m_pipeline = rhs.m_pipeline;
            m_render_pass = rhs.m_render_pass;
            m_input = rhs.m_input;
            m_vert_shader = rhs.m_vert_shader;
            m_frag_shader = rhs.m_frag_shader;
            m_iris = rhs.m_iris;
            m_id = rhs.m_id;

            rhs.m_pipeline_layout = pipeline_layout;
            rhs.m_pipeline = pipeline;
            rhs.m_render_pass = render_pass;
            rhs.m_input = input;
            rhs.m_vert_shader = vert_shader;
            rhs.m_frag_shader = frag_shader;
            rhs.m_iris = iris;
            rhs.m_id = id;
        }

        pass(iris::instance* iris, const void* data, size_t size, std::string id);
        pass(pass&& other);
        pass() = default;
        ~pass();

        pass& operator=(pass&& other);

        VkPipelineLayout& get_pipeline_layout();
        VkPipeline& get_pipeline();
        VkRenderPass& get_render_pass();
        VkImageView& get_input();
        VkShaderModule& get_vert_shader();
        VkShaderModule& get_frag_shader();
        std::string get_id() const;

        bool bypass = false;
        bool ready();
        bool rebuild();
    };

    void push(iris::instance* iris, void* data, size_t size, std::string id);
    void push(iris::instance* iris, std::string id);
    void pop(iris::instance* iris);
    void insert(iris::instance* iris, int i, void* data, size_t size, std::string id);
    void insert(iris::instance* iris, std::string id);
    void erase(iris::instance* iris, int i);
    pass* at(iris::instance* iris, int i);
    void swap(iris::instance* iris, int i1, int i2);
    pass* front(iris::instance* iris);
    pass* back(iris::instance* iris);
    size_t count(iris::instance* iris);
    void clear(iris::instance* iris);
    std::vector <shaders::pass*>& vector(iris::instance* iris);
}

namespace imgui {
    bool init(iris::instance* iris);
    void set_theme(iris::instance* iris, int theme, bool set_bg_color = true);
    void set_codeview_scheme(iris::instance* iris, int scheme);
    bool render_frame(iris::instance* iris, ImDrawData* draw_data);
    void cleanup(iris::instance* iris);

    // Wrapper for ImGui::Begin that sets a default size
    bool BeginEx(const char* name, bool* p_open, ImGuiWindowFlags flags = 0);
}

namespace vulkan {
    bool init(iris::instance* iris, bool enable_validation = false);
    void cleanup(iris::instance* iris);
    texture upload_texture(iris::instance* iris, void* pixels, int width, int height, int stride);
    void free_texture(iris::instance* iris, texture& tex);
    void* read_image(iris::instance* iris, VkImage image, VkFormat format, int width, int height);
}

namespace platform {
    bool init(iris::instance* iris);
    bool apply_settings(iris::instance* iris);
    void destroy(iris::instance* iris);
}

namespace elf {
    bool load_symbols_from_disc(iris::instance* iris);
    bool load_symbols_from_file(iris::instance* iris, std::string path);
}

namespace emu {
    bool init(iris::instance* iris);
    void destroy(iris::instance* iris);
    int attach_memory_card(iris::instance* iris, int slot, const char* path);
    void detach_memory_card(iris::instance* iris, int slot);
}

namespace render {
    bool init(iris::instance* iris);
    void destroy(iris::instance* iris);
    bool render_frame(iris::instance* iris, VkCommandBuffer command_buffer, VkFramebuffer framebuffer);
    bool save_screenshot(iris::instance* iris, std::string path);
    void switch_backend(iris::instance* iris, int backend);
    void refresh(iris::instance* iris);
}

namespace input {
    bool init(iris::instance* iris);
    void load_db_default(iris::instance* iris);
    bool load_db_from_file(iris::instance* iris, const char* path);
    input_action* get_input_action(iris::instance* iris, int slot, uint64_t input);
    input_event sdl_event_to_input_event(SDL_Event* event);
    std::string get_default_screenshot_filename(iris::instance* iris);
    void execute_action(iris::instance* iris, input_action action, int slot, float value);
    bool save_screenshot(iris::instance* iris, std::string path = "");
    void handle_keydown_event(iris::instance* iris, SDL_Event* event);
    void handle_keyup_event(iris::instance* iris, SDL_Event* event);
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
void show_sysmem_logs(iris::instance* iris);
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
void show_memory_search(iris::instance* iris);
// void show_gamelist(iris::instance* iris);

void handle_keydown_event(iris::instance* iris, SDL_Event* event);
void handle_keyup_event(iris::instance* iris, SDL_Event* event);
void handle_scissor_event(void* udata);
void handle_drag_and_drop_event(void* udata, const char* path);
void handle_ee_tty_event(void* udata, char c);
void handle_iop_tty_event(void* udata, char c);
void handle_sysmem_tty_event(void* udata, char c);

void handle_animations(iris::instance* iris);

void push_info(iris::instance* iris, std::string text);

void add_recent(iris::instance* iris, std::string file);
int open_file(iris::instance* iris, std::string file);

}