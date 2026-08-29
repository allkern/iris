#include "iris.hpp"
#include "config.hpp"

#include "ps2.hpp"
#include "ps2_elf.hpp"
#include "ps2_iso9660.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>

namespace iris::cli {

enum Type {
    FLAG,
    INT,
    NUMBER,
    TEXT,
    PATH,
    ENUM
};

struct EnumValue {
    const char* name;
    int value;
};

struct Value {
    std::string text = "";
    long long integer = 0;
    double number = 0.0;
    bool flag = false;
};

struct Option {
    const char* name;
    char abbrev;
    Type type;
    const char* meta;
    const char* help;
    void (*apply)(Instance*, const Value&);
    const EnumValue* values;
    bool early;
};

template <typename T> static void set(Instance* iris, T& field, T value) {
    T previous = field;

    field = value;

    iris->cli.overrides.push_back([&field, previous, value](bool restore) {
        if (restore) {
            if (field == value)
                field = previous;
        } else {
            if (field == previous)
                field = value;
        }
    });
}

static const EnumValue system_values[] = {
    { "auto", ps2::AUTO },
    { "retail", ps2::RETAIL },
    { "dragon", ps2::RETAIL_DRAGON },
    { "desr", ps2::PSX_DESR },
    { "test", ps2::TEST },
    { "tool", ps2::TOOL },
    { "python", ps2::KONAMI_PYTHON },
    { "python2", ps2::KONAMI_PYTHON2 },
    { "system147", ps2::NAMCO_SYSTEM_147 },
    { "system148", ps2::NAMCO_SYSTEM_148 },
    { "system246", ps2::NAMCO_SYSTEM_246 },
    { "system256", ps2::NAMCO_SYSTEM_256 },
    { "super256", ps2::NAMCO_SYSTEM_SUPER_256 },
    { "hvx", ps2::WEGA_HVX },
    { nullptr, 0 }
};

static const EnumValue renderer_values[] = {
    { "null", gs::renderer::BACKEND_NULL },
    { "software", gs::renderer::BACKEND_SOFTWARE },
    { "hardware", gs::renderer::BACKEND_HARDWARE },
    { nullptr, 0 }
};

static const EnumValue aspect_values[] = {
    { "native", render::NATIVE },
    { "stretch", render::STRETCH },
    { "stretch-keep", render::STRETCH_KEEP },
    { "4:3", render::FORCE_4_3 },
    { "16:9", render::FORCE_16_9 },
    { "5:4", render::FORCE_5_4 },
    { "auto", render::AUTO },
    { nullptr, 0 }
};

static const EnumValue present_values[] = {
    { "30", render::FPS_30 },
    { "60", render::FPS_60 },
    { "vsync", render::VSYNC },
    { "uncapped", render::UNCAPPED },
    { nullptr, 0 }
};

static const EnumValue screenshot_format_values[] = {
    { "png", render::PNG },
    { "bmp", render::BMP },
    { "jpg", render::JPG },
    { "tga", render::TGA },
    { nullptr, 0 }
};

static const EnumValue screenshot_mode_values[] = {
    { "internal", render::INTERNAL },
    { "display", render::DISPLAY },
    { nullptr, 0 }
};

static const EnumValue ssaa_values[] = {
    { "off", 0 },
    { "2x", 1 },
    { "4x", 2 },
    { "8x", 3 },
    { "16x", 4 },
    { nullptr, 0 }
};

static const EnumValue cable_values[] = {
    { "composite", 0 },
    { "s-video", 1 },
    { "component", 2 },
    { nullptr, 0 }
};

static const EnumValue analog_system_values[] = {
    { "ntsc", 0 },
    { "pal", 1 },
    { nullptr, 0 }
};

static const EnumValue usb_values[] = {
    { "none", usb::USB_DEVICE_NONE },
    { "keyboard", usb::USB_DEVICE_KEYBOARD },
    { "mouse", usb::USB_DEVICE_MOUSE },
    { "msd", usb::USB_DEVICE_MSD },
    { "an986", usb::USB_DEVICE_AN986 },
    { "p2io", usb::USB_DEVICE_P2IO },
    { nullptr, 0 }
};

static const EnumValue theme_values[] = {
    { "granite-neo", imgui::GRANITE_NEO },
    { "granite-neo-light", imgui::GRANITE_NEO_LIGHT },
    { "granite", imgui::GRANITE },
    { "imgui-dark", imgui::IMGUI_DARK },
    { "imgui-light", imgui::IMGUI_LIGHT },
    { "imgui-classic", imgui::IMGUI_CLASSIC },
    { "cherry", imgui::CHERRY },
    { "source", imgui::SOURCE },
    { "nord", imgui::NORD },
    { "gruvbox", imgui::GRUVBOX },
    { "tokyo-night", imgui::TOKYO_NIGHT },
    { "mocha", imgui::MOCHA },
    { "latte", imgui::LATTE },
    { "solarized", imgui::SOLARIZED },
    { "sakura", imgui::SAKURA },
    { "sakura-light", imgui::SAKURA_LIGHT },
    { nullptr, 0 }
};

static const EnumValue codeview_values[] = {
    { "solarized-dark", imgui::SOLARIZED_DARK },
    { "solarized-light", imgui::SOLARIZED_LIGHT },
    { "one-dark-pro", imgui::ONE_DARK_PRO },
    { "catppuccin-latte", imgui::CATPPUCCIN_LATTE },
    { "catppuccin-frappe", imgui::CATPPUCCIN_FRAPPE },
    { "catppuccin-macchiato", imgui::CATPPUCCIN_MACCHIATO },
    { "catppuccin-mocha", imgui::CATPPUCCIN_MOCHA },
    { nullptr, 0 }
};

#ifdef _WIN32
static const EnumValue titlebar_values[] = {
    { "default", IRIS_TITLEBAR_DEFAULT },
    { "seamless", IRIS_TITLEBAR_SEAMLESS },
    { nullptr, 0 }
};
#endif

static bool parse_mac(const std::string& text, uint8_t* mac) {
    unsigned int bytes[6] = { 0 };

    if (sscanf(text.c_str(), "%x:%x:%x:%x:%x:%x",
            &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != 6 &&
        sscanf(text.c_str(), "%x-%x-%x-%x-%x-%x",
            &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != 6)
        return false;

    for (int i = 0; i < 6; i++) {
        if (bytes[i] > 0xff)
            return false;

        mac[i] = (uint8_t)bytes[i];
    }

    return true;
}

static void map_device(Instance* iris, const std::string& text) {
    size_t equals = text.find('=');

    if (!equals || equals == std::string::npos) {
        iris_error(&iris->log.settings, "Invalid device mapping \"{}\", expected DEVICE=PATH", text.c_str());

        return;
    }

    std::string device = text.substr(0, equals);
    std::string path = text.substr(equals + 1);

    if (device == "host") {
        set(iris, iris->paths.host_path, path);

        return;
    }

    std::vector <std::pair <std::string, std::string>> maps = iris->paths.device_maps;

    auto existing = std::find_if(maps.begin(), maps.end(), [&](const auto& p) {
        return p.first == device;
    });

    if (existing != maps.end()) {
        existing->second = path;
    } else {
        maps.emplace_back(device, path);
    }

    set(iris, iris->paths.device_maps, maps);
}

static const Option g_options[] = {
    { nullptr, 0, FLAG, nullptr, "Files" },

    { "bios", 'b', PATH, "FILE", "PlayStation 2 BIOS dump to boot from",
        [](Instance* i, const Value& v) { set(i, i->paths.bios_path, v.text); } },
    { "rom1", 0, PATH, "FILE", "DVD player (ROM1) dump",
        [](Instance* i, const Value& v) { set(i, i->paths.rom1_path, v.text); set(i, i->paths.pinned_rom1, true); } },
    { "rom2", 0, PATH, "FILE", "ROM2 dump",
        [](Instance* i, const Value& v) { set(i, i->paths.rom2_path, v.text); set(i, i->paths.pinned_rom2, true); } },
    { "nvram", 0, PATH, "FILE", "NVRAM dump",
        [](Instance* i, const Value& v) { set(i, i->paths.nvram_path, v.text); set(i, i->paths.pinned_nvram, true); } },
    { "auto-paths", 0, FLAG, nullptr, "Look for ROM1/ROM2/NVRAM next to the BIOS",
        [](Instance* i, const Value& v) { set(i, i->paths.auto_paths, v.flag); } },
    { "disc", 'i', PATH, "FILE", "Disc image to insert and boot",
        [](Instance* i, const Value& v) { i->paths.disc_path = v.text; } },
    { "autoboot-disc", 0, PATH, "FILE", "Alias for --disc, kept for compatibility",
        [](Instance* i, const Value& v) { i->paths.disc_path = v.text; } },
    { "executable", 'x', PATH, "FILE", "ELF executable to side-load on startup",
        [](Instance* i, const Value& v) { i->paths.elf_path = v.text; } },
    { "boot", 'd', PATH, "PATH", "Boot the kernel straight into this path",
        [](Instance* i, const Value& v) { i->paths.boot_path = v.text; } },
    { "slot1", 0, PATH, "FILE", "Memory card image for slot 1",
        [](Instance* i, const Value& v) { set(i, i->paths.mcd0_path, v.text); } },
    { "slot2", 0, PATH, "FILE", "Memory card image for slot 2",
        [](Instance* i, const Value& v) { set(i, i->paths.mcd1_path, v.text); } },
    { "flash", 0, PATH, "FILE", "DEV9 flash image",
        [](Instance* i, const Value& v) { set(i, i->paths.flash_path, v.text); } },
    { "hdd", 0, PATH, "FILE", "DEV9 hard disk image",
        [](Instance* i, const Value& v) { set(i, i->paths.hdd_path, v.text); } },
    { "hdd-id", 0, PATH, "FILE", "SCE drive identity blob (Konami Python 2)",
        [](Instance* i, const Value& v) { set(i, i->paths.hdd_id_path, v.text); } },
    { "dongle-black", 0, PATH, "FILE", "DS2430 black dongle dump (Konami Python 2)",
        [](Instance* i, const Value& v) { set(i, i->paths.dongle_black_path, v.text); } },
    { "dongle-white", 0, PATH, "FILE", "DS2430 white dongle dump (Konami Python 2)",
        [](Instance* i, const Value& v) { set(i, i->paths.dongle_white_path, v.text); } },
    { "p2io-input", 0, INT, "N", "Python 2 I/O devices (0 DM, 1 GF, 2 DDR, 3 Toy's March, 4 Thrill Drive, 5 Dance 86.4)",
        [](Instance* i, const Value& v) { set(i, i->p2io_input_type, (int)v.integer); } },
    { "gcdb", 0, PATH, "FILE", "Game controller database",
        [](Instance* i, const Value& v) { set(i, i->paths.gcdb_path, v.text); } },
    { "snap", 0, PATH, "DIR", "Directory to write screenshots to",
        [](Instance* i, const Value& v) { set(i, i->paths.snap_path, v.text); } },
    { "host", 0, PATH, "DIR", "Directory the emulated host: device points at",
        [](Instance* i, const Value& v) { set(i, i->paths.host_path, v.text); } },
    { "host-from-elf", 0, FLAG, nullptr, "Point host: at the directory of the loaded executable",
        [](Instance* i, const Value& v) { set(i, i->paths.host_from_elf, v.flag); } },
    { "map", 0, TEXT, "DEV=PATH", "Map an IOP device to a host directory, repeatable",
        [](Instance* i, const Value& v) { map_device(i, v.text); } },

    { nullptr, 0, FLAG, nullptr, "System" },

    { "system", 0, ENUM, "MODEL", "Console model to emulate",
        [](Instance* i, const Value& v) { set(i, i->system, (int)v.integer); }, system_values },
    { "autostart", 0, FLAG, nullptr, "Start running as soon as something is loaded",
        [](Instance* i, const Value& v) { set(i, i->autostart, v.flag); } },
    { "cache-arcade-files", 0, FLAG, nullptr, "Keep arcade files extracted from archives between runs",
        [](Instance* i, const Value& v) { set(i, i->cache_arcade_files, v.flag); } },
    { "timescale", 0, INT, "N", "Run the machine N times faster than real time",
        [](Instance* i, const Value& v) { set(i, i->timescale, std::clamp((int)v.integer, 1, 16)); } },
    { "skip-fmv", 0, FLAG, nullptr, "Skip full motion videos",
        [](Instance* i, const Value& v) { set(i, i->skip_fmv, v.flag); } },
    { "mac", 0, TEXT, "ADDR", "SMAP MAC address, e.g. 00:1A:2B:3C:4D:5E",
        [](Instance* i, const Value& v) {
            uint8_t mac[6];

            if (!parse_mac(v.text, mac)) {
                iris_error(&i->log.settings, "Invalid MAC address \"{}\"", v.text.c_str());

                return;
            }

            for (int n = 0; n < 6; n++)
                set(i, i->mac_address[n], mac[n]);
        } },
    { "usb1", 0, ENUM, "DEVICE", "Device plugged into USB port 1",
        [](Instance* i, const Value& v) { set(i, i->input.usb_devices[0], (int)v.integer); }, usb_values },
    { "usb2", 0, ENUM, "DEVICE", "Device plugged into USB port 2",
        [](Instance* i, const Value& v) { set(i, i->input.usb_devices[1], (int)v.integer); }, usb_values },
    { "usb1-image", 0, PATH, "FILE", "Mass storage image for USB port 1",
        [](Instance* i, const Value& v) { set(i, i->input.usb_msd_paths[0], v.text); } },
    { "usb2-image", 0, PATH, "FILE", "Mass storage image for USB port 2",
        [](Instance* i, const Value& v) { set(i, i->input.usb_msd_paths[1], v.text); } },

    { nullptr, 0, FLAG, nullptr, "Display" },

    { "fullscreen", 'f', FLAG, nullptr, "Start in fullscreen",
        [](Instance* i, const Value& v) { set(i, i->fullscreen, v.flag); } },
    { "width", 0, INT, "N", "Window width",
        [](Instance* i, const Value& v) { set(i, i->window_width, (unsigned int)v.integer); } },
    { "height", 0, INT, "N", "Window height",
        [](Instance* i, const Value& v) { set(i, i->window_height, (unsigned int)v.integer); } },
    { "renderer", 'r', ENUM, "BACKEND", "GS renderer backend",
        [](Instance* i, const Value& v) { set(i, i->renderer_backend, (unsigned int)v.integer); }, renderer_values },
    { "aspect", 0, ENUM, "MODE", "Aspect ratio handling",
        [](Instance* i, const Value& v) { set(i, i->aspect_mode, (int)v.integer); }, aspect_values },
    { "present-mode", 0, ENUM, "MODE", "Frame pacing",
        [](Instance* i, const Value& v) { set(i, i->present_mode, (int)v.integer); }, present_values },
    { "scale", 0, NUMBER, "F", "Output scale factor",
        [](Instance* i, const Value& v) { set(i, i->scale, (float)v.number); } },
    { "filter", 0, FLAG, nullptr, "Bilinear filtering of the final image",
        [](Instance* i, const Value& v) { set(i, i->filter, (int)v.flag); } },
    { "integer-scaling", 0, FLAG, nullptr, "Round the output scale to whole pixels",
        [](Instance* i, const Value& v) { set(i, i->integer_scaling, v.flag); } },
    { "angle", 0, INT, "DEG", "Rotate the output by DEG degrees",
        [](Instance* i, const Value& v) { set(i, i->angle, (int)v.integer); } },
    { "flip-x", 0, FLAG, nullptr, "Flip the output horizontally",
        [](Instance* i, const Value& v) { set(i, i->flip_x, v.flag); } },
    { "flip-y", 0, FLAG, nullptr, "Flip the output vertically",
        [](Instance* i, const Value& v) { set(i, i->flip_y, v.flag); } },

    { nullptr, 0, FLAG, nullptr, "Hardware renderer" },

    { "super-sampling", 0, ENUM, "N", "Super sampling factor",
        [](Instance* i, const Value& v) {
            set(i, i->hardware_backend_config.super_sampling, (int)v.integer);

            if (v.integer)
                set(i, i->hardware_backend_config.force_progressive, true);
        }, ssaa_values },
    { "super-sampled-quads", 0, FLAG, nullptr, "Super sample UI and sprite quads too",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.super_sampled_quads, v.flag); } },
    { "force-progressive", 0, FLAG, nullptr, "Force progressive scan",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.force_progressive, v.flag); } },
    { "overscan", 0, FLAG, nullptr, "Show the overscan area",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.overscan, v.flag); } },
    { "crtc-offsets", 0, FLAG, nullptr, "Honour CRTC display offsets",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.crtc_offsets, v.flag); } },
    { "mipmaps", 0, FLAG, nullptr, "Texture mipmapping",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.disable_mipmaps, !v.flag); } },
    { "unsynced-readbacks", 0, FLAG, nullptr, "Read GS memory back without synchronizing",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.unsynced_readbacks, v.flag); } },
    { "backbuffer-promotion", 0, FLAG, nullptr, "Promote backbuffer copies to render targets",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.backbuffer_promotion, v.flag); } },
    { "blend-demote", 0, FLAG, nullptr, "Allow demoting blend modes to cheaper ones",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.allow_blend_demote, v.flag); } },
    { "analog-video", 0, FLAG, nullptr, "Emulate an analog video signal",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.enable_analog_video, v.flag); } },
    { "analog-cable", 0, ENUM, "TYPE", "Analog cable type",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.analog_cable, (int)v.integer); }, cable_values },
    { "analog-system", 0, ENUM, "STD", "Analog video standard",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.analog_system, (int)v.integer); }, analog_system_values },
    { "line-comb", 0, FLAG, nullptr, "3-line comb decode filter",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.line_comb, v.flag); } },
    { "skip-notch", 0, FLAG, nullptr, "Skip the notch decode filter",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.skip_notch, v.flag); } },
    { "invert-fields", 0, FLAG, nullptr, "Swap interlaced field order",
        [](Instance* i, const Value& v) { set(i, i->hardware_backend_config.invert_fields, v.flag); } },

    { nullptr, 0, FLAG, nullptr, "Shaders" },

    { "shaders", 0, FLAG, nullptr, "Run the post-processing shader chain",
        [](Instance* i, const Value& v) { set(i, i->enable_shaders, v.flag); } },
    { "shader", 0, TEXT, "ID", "Append a shader pass to the chain, repeatable",
        [](Instance* i, const Value& v) {
            i->vk.shader_passes_pending.push_back(v.text);
            i->cli.shaders.push_back(v.text);
        } },

    { nullptr, 0, FLAG, nullptr, "Audio" },

    { "mute", 0, FLAG, nullptr, "Mute all output",
        [](Instance* i, const Value& v) { set(i, i->audio.mute, v.flag); } },
    { "mute-adma", 0, FLAG, nullptr, "Mute ADMA (streamed) voices",
        [](Instance* i, const Value& v) { set(i, i->audio.mute_adma, v.flag); } },
    { "volume", 0, NUMBER, "F", "Output volume, 0.0 to 1.0",
        [](Instance* i, const Value& v) { set(i, i->audio.volume, std::clamp((float)v.number, 0.0f, 1.0f)); } },

    { nullptr, 0, FLAG, nullptr, "Screenshots" },

    { "screenshot-format", 0, ENUM, "FMT", "Screenshot file format",
        [](Instance* i, const Value& v) { set(i, i->screenshot_format, (int)v.integer); }, screenshot_format_values },
    { "screenshot-mode", 0, ENUM, "MODE", "Capture the internal buffer or what is displayed",
        [](Instance* i, const Value& v) { set(i, i->screenshot_mode, (int)v.integer); }, screenshot_mode_values },
    { "screenshot-shaders", 0, FLAG, nullptr, "Run screenshots through the shader chain",
        [](Instance* i, const Value& v) { set(i, i->screenshot_shader_processing, v.flag); } },
    { "jpg-quality", 0, INT, "N", "JPEG quality, 1 to 100",
        [](Instance* i, const Value& v) {
            set(i, i->screenshot_jpg_quality, std::clamp((int)v.integer, 1, 100));
            set(i, i->screenshot_jpg_quality_mode, (int)render::CUSTOM);
        } },
    { "snap-on-exit", 'S', FLAG, nullptr, "Take a screenshot right before quitting",
        [](Instance* i, const Value& v) { set(i, i->snap_on_exit, v.flag); } },

    { nullptr, 0, FLAG, nullptr, "Network" },

    { "network", 0, FLAG, nullptr, "Emulated network adapter",
        [](Instance* i, const Value& v) { set(i, i->slirp_config.enabled, v.flag); } },
    { "net-address", 0, TEXT, "ADDR", "Virtual network address",
        [](Instance* i, const Value& v) { set(i, i->slirp_config.network, v.text); } },
    { "net-netmask", 0, TEXT, "ADDR", "Virtual network mask",
        [](Instance* i, const Value& v) { set(i, i->slirp_config.netmask, v.text); } },
    { "net-gateway", 0, TEXT, "ADDR", "Virtual gateway address",
        [](Instance* i, const Value& v) { set(i, i->slirp_config.gateway, v.text); } },
    { "net-dhcp-start", 0, TEXT, "ADDR", "First address handed out by DHCP",
        [](Instance* i, const Value& v) { set(i, i->slirp_config.dhcp_start, v.text); } },
    { "net-nameserver", 0, TEXT, "ADDR", "Virtual nameserver address",
        [](Instance* i, const Value& v) { set(i, i->slirp_config.nameserver, v.text); } },

    { nullptr, 0, FLAG, nullptr, "Interface" },

    { "theme", 0, ENUM, "NAME", "Interface theme",
        [](Instance* i, const Value& v) { set(i, i->ui.theme, (int)v.integer); }, theme_values },
    { "ui-scale", 0, NUMBER, "F", "Interface scale factor",
        [](Instance* i, const Value& v) { set(i, i->ui.ui_scale, (float)v.number); } },
    { "viewports", 0, FLAG, nullptr, "Let windows be dragged outside the main window",
        [](Instance* i, const Value& v) { set(i, i->ui.imgui_enable_viewports, v.flag); } },
    { "status-bar", 0, FLAG, nullptr, "Show the status bar",
        [](Instance* i, const Value& v) { set(i, i->ui.show_status_bar, v.flag); } },
    { "no-decorations", 0, FLAG, nullptr, "Hide the menu bar and status bar, black background",
        [](Instance* i, const Value& v) { set(i, i->no_decorations, v.flag); } },
    { "remember-window-size", 0, FLAG, nullptr, "Save the window size when it is resized",
        [](Instance* i, const Value& v) { set(i, i->remember_window_size, v.flag); } },
    { "overlay", 0, FLAG, nullptr, "Show the performance overlay",
        [](Instance* i, const Value& v) { set(i, i->ui.show_overlay, v.flag); } },
    { "codeview-scheme", 0, ENUM, "NAME", "Disassembly colour scheme",
        [](Instance* i, const Value& v) { set(i, i->ui.codeview_color_scheme, (int)v.integer); }, codeview_values },
    { "codeview-font-scale", 0, NUMBER, "F", "Disassembly font scale",
        [](Instance* i, const Value& v) { set(i, i->ui.codeview_font_scale, (float)v.number); } },
#ifdef _WIN32
    { "titlebar", 0, ENUM, "STYLE", "Window titlebar style",
        [](Instance* i, const Value& v) { set(i, i->windows_titlebar_style, (int)v.integer); }, titlebar_values },
    { "window-borders", 0, FLAG, nullptr, "Draw a border around the window",
        [](Instance* i, const Value& v) { set(i, i->windows_enable_borders, v.flag); } },
#endif
#ifdef IRIS_HAS_DARK_TITLEBAR
    { "dark-mode", 0, FLAG, nullptr, "Use the dark window frame",
        [](Instance* i, const Value& v) { set(i, i->dark_titlebar, v.flag); } },
#endif

    { nullptr, 0, FLAG, nullptr, "Vulkan" },

    { "gpu", 0, INT, "N", "Index of the physical device to render on, -1 picks one",
        [](Instance* i, const Value& v) { set(i, i->vk.vulkan_physical_device, (int)v.integer); } },
    { "validation-layers", 0, FLAG, nullptr, "Enable the Vulkan validation layers",
        [](Instance* i, const Value& v) { set(i, i->vk.vulkan_enable_validation_layers, v.flag); } },

    { nullptr, 0, FLAG, nullptr, "Diagnostics" },

    { "headless", 'H', FLAG, nullptr, "Run without showing the window",
        [](Instance* i, const Value& v) { set(i, i->headless, v.flag); } },
    { "log", 0, PATH, "FILE", "Write the log to FILE",
        [](Instance* i, const Value& v) { set(i, i->paths.log_path, v.text); } },
    { "log-file", 0, FLAG, nullptr, "Write the log to a file",
        [](Instance* i, const Value& v) { set(i, i->log_to_file, v.flag); } },
    { "log-console", 0, FLAG, nullptr, "Write the log to the console",
        [](Instance* i, const Value& v) { set(i, i->log_to_console, v.flag); } },

    { nullptr, 0, FLAG, nullptr, "Configuration" },

    { "config", 'c', PATH, "FILE", "Settings file to use instead of the default one",
        [](Instance* i, const Value& v) { i->paths.settings_path = v.text; }, nullptr, true },
    { "mappings", 0, PATH, "FILE", "Input mappings file to use instead of the default one",
        [](Instance* i, const Value& v) { i->paths.mappings_path = v.text; }, nullptr, true },
    { "portable", 0, FLAG, nullptr, "Keep settings next to the executable",
        [](Instance* i, const Value& v) { i->cli.portable = v.flag; }, nullptr, true },
    { "reset-settings", 0, FLAG, nullptr, "Ignore the settings file and start from the defaults",
        [](Instance* i, const Value& v) { i->cli.reset_settings = v.flag; }, nullptr, true },
    { "save-settings", 0, FLAG, nullptr, "Write the settings file back out on exit",
        [](Instance* i, const Value& v) { i->dump_to_file = v.flag; }, nullptr, true },

    { nullptr, 0, FLAG, nullptr, "Miscellaneous" },

    { "help", 'h', FLAG, nullptr, "Display this help and exit", nullptr, nullptr, true },
    { "version", 'v', FLAG, nullptr, "Output version information and exit", nullptr, nullptr, true }
};

static std::string enum_list(const EnumValue* values) {
    std::string list;

    for (const EnumValue* v = values; v->name; v++) {
        if (list.size())
            list += ", ";

        list += v->name;
    }

    return list;
}

template <typename... Args> static bool usage_error(fmt::format_string <Args...> format, Args&&... args) {
    fmt::print(stderr, "iris: ");
    fmt::print(stderr, format, std::forward <Args> (args)...);
    fmt::print(stderr, "\nTry \'iris --help\' for more information\n");

    return false;
}

static const Option* find_long(const std::string& name, bool& negated) {
    for (const Option& opt : g_options) {
        if (!opt.name)
            continue;

        if (name == opt.name) {
            negated = false;

            return &opt;
        }

        if (opt.type == FLAG && name.size() > 3 && name.compare(0, 3, "no-") == 0 &&
            name.compare(3, std::string::npos, opt.name) == 0) {
            negated = true;

            return &opt;
        }
    }

    return nullptr;
}

static const Option* find_short(char abbrev) {
    for (const Option& opt : g_options)
        if (opt.name && opt.abbrev == abbrev)
            return &opt;

    return nullptr;
}

static bool parse_bool(const std::string& text, bool& out) {
    if (text == "1" || text == "true" || text == "yes" || text == "on") {
        out = true;

        return true;
    }

    if (text == "0" || text == "false" || text == "no" || text == "off") {
        out = false;

        return true;
    }

    return false;
}

static bool parse_value(const Option& opt, const std::string& text, Value& value) {
    value.text = text;

    if (opt.type == INT) {
        char* end = nullptr;

        value.integer = strtoll(text.c_str(), &end, 0);

        if (end == text.c_str() || *end)
            return usage_error("\"{}\" is not a whole number (--{})", text, opt.name);
    }

    if (opt.type == NUMBER) {
        char* end = nullptr;

        value.number = strtod(text.c_str(), &end);

        if (end == text.c_str() || *end)
            return usage_error("\"{}\" is not a number (--{})", text, opt.name);
    }

    if (opt.type == ENUM) {
        for (const EnumValue* v = opt.values; v->name; v++) {
            if (text == v->name) {
                value.integer = v->value;

                return true;
            }
        }

        return usage_error("invalid value \"{}\" for --{}, expected one of: {}",
            text, opt.name, enum_list(opt.values));
    }

    return true;
}

static void queue(Instance* iris, const Option* opt, const Value& value) {
    if (!opt->apply)
        return;

    if (opt->early) {
        opt->apply(iris, value);

        return;
    }

    iris->cli.pending.push_back([opt, value](Instance* i) { opt->apply(i, value); });
}

static bool parse_short(Instance* iris, const std::string& arg, int& i, int argc, const char* argv[]) {
    for (size_t c = 1; c < arg.size(); c++) {
        const Option* opt = find_short(arg[c]);

        if (!opt)
            return usage_error("unrecognized option \"-{}\"", arg[c]);

        Value value;

        if (opt->type == FLAG) {
            value.flag = true;

            queue(iris, opt, value);

            continue;
        }

        std::string text;

        if (c + 1 < arg.size()) {
            text = arg.substr(c + 1);
        } else if (i + 1 < argc) {
            text = argv[++i];
        } else {
            return usage_error("option \"-{}\" requires an argument", opt->abbrev);
        }

        if (!parse_value(*opt, text, value))
            return false;

        queue(iris, opt, value);

        return true;
    }

    return true;
}

bool parse(Instance* iris, int argc, const char* argv[]) {
    bool options_ended = false;

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);

        if (arg == "--" && !options_ended) {
            options_ended = true;

            continue;
        }

        if (options_ended || arg.size() < 2 || arg[0] != '-') {
            if (iris->cli.open_path.size())
                return usage_error("unexpected argument \"{}\"", arg);

            iris->cli.open_path = arg;

            continue;
        }

        if (arg[1] != '-') {
            if (!parse_short(iris, arg, i, argc, argv))
                return false;

            continue;
        }

        std::string name = arg.substr(2);
        std::string text;
        bool has_text = false;

        size_t equals = name.find('=');

        if (equals != std::string::npos) {
            text = name.substr(equals + 1);
            name.erase(equals);
            has_text = true;
        }

        bool negated = false;
        const Option* opt = find_long(name, negated);

        if (!opt)
            return usage_error("unrecognized option \"--{}\"", name);

        Value value;

        if (opt->type == FLAG) {
            value.flag = !negated;

            if (has_text && !parse_bool(text, value.flag))
                return usage_error("invalid value \"{}\" for --{}, expected a boolean", text, name);
        } else {
            if (!has_text) {
                if (i + 1 >= argc)
                    return usage_error("option \"--{}\" requires an argument", name);

                text = argv[++i];
            }

            if (!parse_value(*opt, text, value))
                return false;
        }

        queue(iris, opt, value);
    }

    return true;
}

void apply(Instance* iris) {
    for (const auto& action : iris->cli.pending)
        action(iris);

    iris->cli.pending.clear();
}

void unapply(Instance* iris) {
    for (auto it = iris->cli.overrides.rbegin(); it != iris->cli.overrides.rend(); ++it)
        (*it)(true);
}

void reapply(Instance* iris) {
    for (const auto& entry : iris->cli.overrides)
        entry(false);
}

static void prepare_executable(Instance* iris, const std::string& path) {
    elf::load_symbols_from_file(iris, path);

    iris->paths.host_elf_dir = std::filesystem::path(path).parent_path().string();

    if (iris->paths.host_from_elf)
        settings::apply_device_maps(iris);
}

void boot(Instance* iris) {
    std::string disc = iris->paths.disc_path;
    std::string executable = iris->paths.elf_path;
    std::string boot_path = iris->paths.boot_path;

    bool host_boot = false;

    if (iris->cli.open_path.size()) {
        if (emu::is_arcade_file(iris, iris->cli.open_path)) {
            if (!emu::load_arcade_files(iris, iris->cli.open_path)) {
                iris_error(&iris->log.settings, "Couldn't start arcade game \"{}\"", iris->cli.open_path.c_str());

                return;
            }

            if (iris->autostart)
                iris->debug.pause = false;

            return;
        }

        if (emu::is_disc_image(iris->cli.open_path)) {
            disc = iris->cli.open_path;
        } else {
            executable = iris->cli.open_path;
            host_boot = true;
        }
    }

    if (disc.empty() && executable.empty() && boot_path.empty())
        return;

    if (disc.size()) {
        if (cdvd::open(iris->ps2->cdvd, disc.c_str(), 0)) {
            iris_error(&iris->log.settings, "Couldn't open disc image \"{}\"", disc.c_str());

            return;
        }

        iris->loaded = disc;
    }

    if (executable.size()) {
        prepare_executable(iris, executable);

        if (host_boot) {
            // Note: We need the trailing whitespaces here because of IOMAN HLE
            ps2::boot_file(iris->ps2, ("host:  " + executable).c_str());
        } else {
            elf::load(iris->ps2, executable.c_str());
        }

        iris->loaded = executable;
    } else if (boot_path.size()) {
        ps2::boot_file(iris->ps2, boot_path.c_str());

        iris->loaded = boot_path;
    } else {
        char* file = iop::disc::get_boot_path(iris->ps2->cdvd->disc);

        if (!file) {
            iris_error(&iris->log.settings, "Couldn't find a boot file on \"{}\"", disc.c_str());

            return;
        }

        elf::load_symbols_from_disc(iris);

        ps2::boot_file(iris->ps2, file);
    }

    iris->debug.pause = !iris->autostart;
}

static void print_entry(const std::string& syntax, const std::string& help) {
    constexpr size_t column = 36;
    constexpr size_t width = 80;

    std::string indent(column, ' ');

    if (syntax.size() >= column) {
        fmt::print("{}\n{}", syntax, indent);
    } else {
        fmt::print("{}{}", syntax, std::string(column - syntax.size(), ' '));
    }

    size_t used = 0;
    size_t pos = 0;

    while (true) {
        size_t space = help.find(' ', pos);
        size_t end = space == std::string::npos ? help.size() : space;
        std::string word = help.substr(pos, end - pos);

        if (used && column + used + 1 + word.size() > width) {
            fmt::print("\n{}", indent);

            used = 0;
        }

        if (used) {
            fmt::print(" ");

            used++;
        }

        fmt::print("{}", word);

        used += word.size();

        if (space == std::string::npos)
            break;

        pos = space + 1;
    }

    fmt::print("\n");
}

void print_help() {
    fmt::print(
        "Usage: iris [OPTION]... [FILE]\n"
        "\n"
        "Start Iris, loading FILE if it is given. FILE may be a disc image or an\n"
        "executable. Options take precedence over the settings file and are never\n"
        "written back to it, so they only apply to this run.\n"
        "\n"
        "Options spelled --[no-]NAME are switches, --no-NAME turns them off.\n"
    );

    for (const Option& opt : g_options) {
        if (!opt.name) {
            fmt::print("\n{}:\n", opt.help);

            continue;
        }

        std::string syntax = opt.abbrev ? fmt::format("  -{}, --", opt.abbrev) : "      --";

        if (opt.type == FLAG && opt.apply)
            syntax += "[no-]";

        syntax += opt.name;

        if (opt.meta) {
            syntax += " ";
            syntax += opt.meta;
        }

        std::string help = opt.help;

        if (opt.values)
            help += fmt::format(" ({})", enum_list(opt.values));

        print_entry(syntax, help);
    }
}

void print_version() {
    fmt::print(
        "iris (" STR(_IRIS_VERSION) " " STR(_IRIS_OSVERSION) ")\n"
        "Copyright (C) 2026 Allkern/Lisandro Alarcon\n\n"
        "MIT License\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE.\n"
    );
}

bool quick_exit(int argc, const char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);

        if (arg == "--")
            break;

        if (arg == "-h" || arg == "--help") {
            print_help();

            return true;
        }

        if (arg == "-v" || arg == "--version") {
            print_version();

            return true;
        }
    }

    return false;
}

}
