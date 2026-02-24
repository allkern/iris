#include "iris.hpp"
#include "config.hpp"

#include "ps2_elf.h"
#include "ps2_iso9660.h"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

namespace iris::settings {

void print_version() {
    puts(
        "iris (" STR(_IRIS_VERSION) " " STR(_IRIS_OSVERSION) ")\n"
        "Copyright (C) 2025 Allkern/Lisandro Alarcon\n\n"
        "MIT License\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE."
    );
}

void print_help() {
    puts(
        "Usage: iris [OPTION]... <path-to-disc-image>\n"
        "\n"
        "  -b, --bios               Specify a PlayStation 2 BIOS dump file\n"
        "      --rom1               Specify a DVD player dump file\n"
        "      --rom2               Specify a ROM2 dump file\n"
        "  -d, --boot               Specify a direct kernel boot path\n"
        "  -i, --disc               Specify a path to a disc image file\n"
        "  -x, --executable         Specify a path to an ELF executable to be\n"
        "                             loaded on system startup\n"
        "      --slot1              Specify a path to a memory card file to\n"
        "                             be inserted on slot 1\n"
        "      --slot2              Specify a path to a memory card file to\n"
        "                             be inserted on slot 2\n"
        "      --snap               Specify a directory for storing screenshots\n"
        "  -h, --help               Display this help and exit\n"
        "  -v, --version            Output version information and exit\n"
    );
}

bool parse_mappings_file(iris::instance* iris) {
    iris->mappings_path = iris->pref_path + "mappings.toml";

    std::ifstream mappings_file(iris->mappings_path);

    if (!mappings_file.is_open())
        return false;

    toml::parse_result result = toml::parse(mappings_file);

    if (!result) {
        std::string desc(result.error().description());

        printf("iris: Couldn't parse mappings file: %s\n", desc.c_str());

        return false;
    }

    toml::table& tbl = result.table();

    for (auto& map : tbl) {
        printf("input: Parsing input map \"%s\"...\n", map.first.data());

        mapping input_mapping {};

        input_mapping.name = map.first.data();
        input_mapping.map = bidirectional_map<uint64_t, input_action>();

        for (auto& input_map : *map.second.as_table()) {
            uint64_t key = std::stoull(input_map.first.data());
            uint64_t value = input_map.second.as_integer()->get();

            input_mapping.map.insert(key, static_cast<input_action>(value));

            // printf("entry: %d -> %d\n", std::stoull(input_map.first.data()), input_map.second.as_integer()->get());
        }

        iris->input_maps.push_back(input_mapping);
    }

    return true;
}

bool parse_toml_settings(iris::instance* iris) {
    iris->settings_path = iris->pref_path + "settings.toml";

    toml::parse_result result = toml::parse_file(iris->settings_path);

    if (!result) {
        std::string desc(result.error().description());

        printf("iris: Couldn't parse settings file: %s\n", desc.c_str());

        return false;
    }

    toml::table& tbl = result.table();

    auto paths = tbl["paths"];
    iris->bios_path = paths["bios_path"].value_or("");
    iris->rom1_path = paths["rom1_path"].value_or("");
    iris->rom2_path = paths["rom2_path"].value_or("");
    iris->nvram_path = paths["nvram_path"].value_or("");
    iris->mcd0_path = paths["mcd0_path"].value_or("");
    iris->mcd1_path = paths["mcd1_path"].value_or("");
    iris->snap_path = paths["snap_path"].value_or("snap");
    iris->flash_path = paths["flash_path"].value_or("");
    iris->gcdb_path = paths["gcdb_path"].value_or("");

    auto window = tbl["window"];
    iris->window_width = window["window_width"].value_or(960);
    iris->window_height = window["window_height"].value_or(720);
    iris->fullscreen = window["fullscreen"].value_or(0);

    auto display = tbl["display"];
    iris->aspect_mode = display["aspect_mode"].value_or(RENDER_ASPECT_AUTO);
    iris->filter = display["filter"].value_or(true);
    iris->integer_scaling = display["integer_scaling"].value_or(false);
    iris->scale = display["scale"].value_or(1.5f);
    iris->renderer_backend = display["renderer"].value_or(RENDERER_BACKEND_HARDWARE);
    iris->window_width = display["window_width"].value_or(960);
    iris->window_height = display["window_height"].value_or(720);
    iris->menubar_height = display["menubar_height"].value_or(0);

    auto audio = tbl["audio"];
    iris->mute = audio["mute"].value_or(false);
    iris->volume = audio["volume"].value_or(1.0);
    iris->mute_adma = audio["mute_adma"].value_or(true);

    auto debugger = tbl["debugger"];
    iris->show_ee_control = debugger["show_ee_control"].value_or(false);
    iris->show_ee_state = debugger["show_ee_state"].value_or(false);
    iris->show_ee_logs = debugger["show_ee_logs"].value_or(false);
    iris->show_ee_interrupts = debugger["show_ee_interrupts"].value_or(false);
    iris->show_ee_dmac = debugger["show_ee_dmac"].value_or(false);
    iris->show_iop_control = debugger["show_iop_control"].value_or(false);
    iris->show_iop_state = debugger["show_iop_state"].value_or(false);
    iris->show_iop_logs = debugger["show_iop_logs"].value_or(false);
    iris->show_iop_interrupts = debugger["show_iop_interrupts"].value_or(false);
    iris->show_iop_modules = debugger["show_iop_modules"].value_or(false);
    iris->show_iop_dma = debugger["show_iop_dma"].value_or(false);
    iris->show_gs_debugger = debugger["show_gs_debugger"].value_or(false);
    iris->show_spu2_debugger = debugger["show_spu2_debugger"].value_or(false);
    iris->show_memory_viewer = debugger["show_memory_viewer"].value_or(false);
    iris->show_memory_search = debugger["show_memory_search"].value_or(false);
    iris->show_vu_disassembler = debugger["show_vu_disassembler"].value_or(false);
    iris->show_status_bar = debugger["show_status_bar"].value_or(true);
    iris->show_pad_debugger = debugger["show_pad_debugger"].value_or(false);
    iris->show_threads = debugger["show_threads"].value_or(false);
    iris->show_sysmem_logs = debugger["show_sysmem_logs"].value_or(false);
    iris->show_overlay = debugger["show_overlay"].value_or(false);

    // iris->show_symbols = debugger["show_symbols"].value_or(false);
    iris->show_breakpoints = debugger["show_breakpoints"].value_or(false);
    iris->show_imgui_demo = debugger["show_imgui_demo"].value_or(false);
    iris->skip_fmv = debugger["skip_fmv"].value_or(false);
    iris->timescale = debugger["timescale"].value_or(8);

    auto system = tbl["system"];
    iris->system = system["model"].value_or(PS2_SYSTEM_AUTO);

    toml::array* mac_array = system["mac_address"].as_array();

    if (mac_array && mac_array->size() == 6) {
        for (int i = 0; i < 6; i++) {
            iris->mac_address[i] = static_cast<uint8_t>(mac_array->at(i).as_integer()->get());
        }
    } else {
        // Default MAC address
        iris->mac_address[0] = 0x00;
        iris->mac_address[1] = 0x1A;
        iris->mac_address[2] = 0x2B;
        iris->mac_address[3] = 0x3C;
        iris->mac_address[4] = 0x4D;
        iris->mac_address[5] = 0x5E;
    }

    auto screenshots = tbl["screenshots"];
    iris->screenshot_format = screenshots["format"].value_or(IRIS_SCREENSHOT_FORMAT_PNG);
    iris->screenshot_jpg_quality_mode = screenshots["jpg_quality_mode"].value_or(IRIS_SCREENSHOT_JPG_QUALITY_MAXIMUM);
    iris->screenshot_jpg_quality = screenshots["jpg_quality"].value_or(50);
    iris->screenshot_mode = screenshots["mode"].value_or(IRIS_SCREENSHOT_MODE_INTERNAL);
    iris->screenshot_shader_processing = screenshots["shader_processing"].value_or(false);

    auto hardware = tbl["hardware"];
    iris->hardware_backend_config.super_sampling = hardware["super_sampling"].value_or(0);
    iris->hardware_backend_config.force_progressive = hardware["force_progressive"].value_or(false);
    iris->hardware_backend_config.overscan = hardware["overscan"].value_or(false);
    iris->hardware_backend_config.crtc_offsets = hardware["crtc_offsets"].value_or(false);
    iris->hardware_backend_config.disable_mipmaps = hardware["disable_mipmaps"].value_or(false);
    iris->hardware_backend_config.unsynced_readbacks = hardware["unsynced_readbacks"].value_or(false);
    iris->hardware_backend_config.backbuffer_promotion = hardware["backbuffer_promotion"].value_or(false);
    iris->hardware_backend_config.allow_blend_demote = hardware["allow_blend_demote"].value_or(false);

    auto vulkan = tbl["vulkan"];
    iris->vulkan_physical_device = vulkan["physical_device"].value_or(-1);
    iris->vulkan_enable_validation_layers = vulkan["enable_validation_layers"].value_or(false);

    auto ui = tbl["ui"];
    iris->theme = ui["theme"].value_or(IRIS_THEME_GRANITE);
    iris->codeview_font_scale = ui["codeview_font_scale"].value_or(1.0f);
    iris->codeview_color_scheme = ui["codeview_color_scheme"].value_or(IRIS_CODEVIEW_COLOR_SCHEME_SOLARIZED_DARK);
    iris->codeview_use_theme_background = ui["codeview_use_theme_background"].value_or(true);
    iris->ui_scale = ui["scale"].value_or(1.0f);
    iris->imgui_enable_viewports = ui["enable_viewports"].value_or(false);

    toml::array* bgcolor = tbl["ui"]["bgcolor"].as_array();

    if (bgcolor && bgcolor->size() == 3) {
        iris->clear_value.color.float32[0] = (float)bgcolor->at(0).as_floating_point()->get();
        iris->clear_value.color.float32[1] = (float)bgcolor->at(1).as_floating_point()->get();
        iris->clear_value.color.float32[2] = (float)bgcolor->at(2).as_floating_point()->get();
    } else {
        iris->clear_value.color.float32[0] = 0.11f;
        iris->clear_value.color.float32[1] = 0.11f;
        iris->clear_value.color.float32[2] = 0.11f;
    }

#ifdef _WIN32
    iris->windows_titlebar_style = tbl["ui"]["windows_titlebar_style"].value_or(IRIS_TITLEBAR_DEFAULT);
    iris->windows_enable_borders = tbl["ui"]["windows_enable_borders"].value_or(true);
    iris->windows_dark_mode = tbl["ui"]["windows_dark_mode"].value_or(true);
#endif

    toml::array* recents = tbl["recents"]["array"].as_array();

    if (recents) {
        for (int i = 0; i < recents->size(); i++)
            iris->recents.push_back(recents->at(i).as_string()->get());
    }

    toml::array* shaders = tbl["shaders"]["array"].as_array();
    iris->enable_shaders = tbl["shaders"]["enable"].value_or(false);

    if (shaders) {
        for (int i = 0; i < shaders->size(); i++)
            iris->shader_passes_pending.push_back(shaders->at(i).as_string()->get());
    }

    return parse_mappings_file(iris);
}

bool check_for_quick_exit(int argc, const char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string a(argv[i]);

        if (a == "-h" || a == "--help") {
            print_help();

            return true;
        } else if (a == "-v" || a == "--version") {
            print_version();

            return true;
        }
    }

    return false;
}

void parse_cli_settings(iris::instance* iris, int argc, const char* argv[]) {
    std::string bios_path;
    std::string rom1_path;
    std::string rom2_path;

    for (int i = 1; i < argc; i++) {
        std::string a(argv[i]);

        if (a == "-x" || a == "--executable") {
            iris->elf_path = argv[i+1];

            ++i;
        } else if (a == "-d" || a == "--boot") {
            iris->boot_path = argv[i+1];

            ++i;
        } else if (a == "-b" || a == "--bios") {
            bios_path = argv[i+1];

            ++i;
        } else if (a == "--rom1") {
            rom1_path = argv[i+1];

            ++i;
        } else if (a == "--rom2") {
            rom2_path = argv[i+1];

            ++i;
        } else if (a == "-i" || a == "--disc") {
            iris->disc_path = argv[i+1];

            ++i;
        } else if (a == "--slot1") {
            iris->mcd0_path = argv[i+1];

            ++i;
        } else if (a == "--slot2") {
            iris->mcd1_path = argv[i+1];

            ++i;
        } else {
            iris->disc_path = argv[i];
        }
    }

    if (bios_path.size()) {
        if (!ps2_load_bios(iris->ps2, bios_path.c_str())) {
            // push_info(iris, "Couldn't load BIOS");

            iris->show_bios_setting_window = true;
        }
    } else {
        if (iris->bios_path.size()) {
            if (!ps2_load_bios(iris->ps2, iris->bios_path.c_str())) {
                // push_info(iris, "Couldn't load BIOS");

                iris->show_bios_setting_window = true;
            }
        } else {
            iris->show_bios_setting_window = true;
        }
    }

    if (rom1_path.size()) {
        if (!ps2_load_rom1(iris->ps2, rom1_path.c_str())) {
            // push_info(iris, "Couldn't load ROM1");
        }
    } else {
        if (iris->rom1_path.size()) {
            if (!ps2_load_rom1(iris->ps2, iris->rom1_path.c_str())) {
                // push_info(iris, "Couldn't load ROM1");
            }
        }
    }

    if (rom2_path.size()) {
        if (!ps2_load_rom2(iris->ps2, rom2_path.c_str())) {
            // push_info(iris, "Couldn't load ROM2");
        }
    } else {
        if (iris->rom2_path.size()) {
            if (!ps2_load_rom2(iris->ps2, iris->rom2_path.c_str())) {
                // push_info(iris, "Couldn't load ROM2");
            }
        }
    }

    if (iris->elf_path.size()) {
        ps2_elf_load(iris->ps2, iris->elf_path.c_str());

        iris->loaded = iris->elf_path;
    }

    if (iris->boot_path.size()) {
        ps2_boot_file(iris->ps2, iris->boot_path.c_str());

        iris->loaded = iris->boot_path;
    }

    if (iris->disc_path.size()) {
        if (ps2_cdvd_open(iris->ps2->cdvd, iris->disc_path.c_str(), 0))
            return;

        char* boot_file = disc_get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file)
            return;

        ps2_boot_file(iris->ps2, boot_file);

        iris->loaded = iris->disc_path;
    }
}

bool init(iris::instance* iris, int argc, const char* argv[]) {
    int r = parse_toml_settings(iris);

    parse_cli_settings(iris, argc, argv);

    if (iris->nvram_path.size())
        ps2_cdvd_load_nvram(iris->ps2->cdvd, iris->nvram_path.c_str());

    if (iris->mcd0_path.size())
        emu::attach_memory_card(iris, 0, iris->mcd0_path.c_str());

    if (iris->mcd1_path.size())
        emu::attach_memory_card(iris, 1, iris->mcd1_path.c_str());

    // Apply settings loaded from file/CLI
    ps2_set_timescale(iris->ps2, iris->timescale);

    ee_set_fmv_skip(iris->ps2->ee, iris->skip_fmv);

    ps2_set_system(iris->ps2, iris->system);
    ps2_speed_load_flash(iris->ps2->speed, iris->flash_path.c_str());
    ps2_speed_set_mac_address(iris->ps2->speed, iris->mac_address);

    return true;
}

void close(iris::instance* iris) {
    if (!iris->dump_to_file)
        return;

    std::ofstream file(iris->settings_path);
    std::ofstream mappings_file(iris->pref_path + "mappings.toml");

    file << "# File auto-generated by " IRIS_TITLE "\n\n";

    auto tbl = toml::table {
        { "system", toml::table {
            { "model", iris->system },
            { "mac_address", toml::array {
                iris->mac_address[0],
                iris->mac_address[1],
                iris->mac_address[2],
                iris->mac_address[3],
                iris->mac_address[4],
                iris->mac_address[5]
            } }
        } },
        { "input", toml::table {
            { "slot1_device", iris->input_devices[0] ? iris->input_devices[0]->get_type() : 0 },
            { "slot2_device", iris->input_devices[1] ? iris->input_devices[1]->get_type() : 0 },
            { "slot1_mapping", iris->input_map[0] },
            { "slot2_mapping", iris->input_map[1] }
        } },
        { "screenshots", toml::table {
            { "format", iris->screenshot_format },
            { "mode", iris->screenshot_mode },
            { "jpg_quality_mode", iris->screenshot_jpg_quality_mode },
            { "jpg_quality", iris->screenshot_jpg_quality },
            { "shader_processing", iris->screenshot_shader_processing }
        } },

        // To-do: Change this to "backends" and use dotted entries
        // e.g.
        // [backend_settings]
        // hardware.super_sampling = 2
        // etc.
        { "hardware", toml::table {
            { "super_sampling", iris->hardware_backend_config.super_sampling },
            { "force_progressive", iris->hardware_backend_config.force_progressive },
            { "overscan", iris->hardware_backend_config.overscan },
            { "crtc_offsets", iris->hardware_backend_config.crtc_offsets },
            { "disable_mipmaps", iris->hardware_backend_config.disable_mipmaps },
            { "unsynced_readbacks", iris->hardware_backend_config.unsynced_readbacks },
            { "backbuffer_promotion", iris->hardware_backend_config.backbuffer_promotion },
            { "allow_blend_demote", iris->hardware_backend_config.allow_blend_demote }
        } },
        { "vulkan", toml::table {
            { "physical_device", iris->vulkan_physical_device },
            { "enable_validation_layers", iris->vulkan_enable_validation_layers }
        } },
        { "debugger", toml::table {
            { "show_ee_control", iris->show_ee_control },
            { "show_ee_state", iris->show_ee_state },
            { "show_ee_logs", iris->show_ee_logs },
            { "show_ee_interrupts", iris->show_ee_interrupts },
            { "show_ee_dmac", iris->show_ee_dmac },
            { "show_iop_control", iris->show_iop_control },
            { "show_iop_state", iris->show_iop_state },
            { "show_iop_logs", iris->show_iop_logs },
            { "show_iop_interrupts", iris->show_iop_interrupts },
            { "show_iop_modules", iris->show_iop_modules},
            { "show_iop_dma", iris->show_iop_dma },
            { "show_gs_debugger", iris->show_gs_debugger },
            { "show_spu2_debugger", iris->show_spu2_debugger },
            { "show_memory_viewer", iris->show_memory_viewer },
            { "show_memory_search", iris->show_memory_search },
            { "show_vu_disassembler", iris->show_vu_disassembler },
            { "show_status_bar", iris->show_status_bar },
            { "show_pad_debugger", iris->show_pad_debugger },
            { "show_breakpoints", iris->show_breakpoints },
            { "show_threads", iris->show_threads },
            { "show_sysmem_logs", iris->show_sysmem_logs },
            { "show_imgui_demo", iris->show_imgui_demo },
            { "show_overlay", iris->show_overlay },
            { "skip_fmv", iris->skip_fmv },
            { "timescale", iris->timescale }
        } },
        { "display", toml::table {
            { "scale", iris->scale },
            { "aspect_mode", iris->aspect_mode },
            { "integer_scaling", iris->integer_scaling },
            { "fullscreen", iris->fullscreen },
            { "filter", iris->filter },
            { "renderer", iris->renderer_backend },
            { "window_width", iris->window_width },
            { "window_height", iris->window_height },
            { "menubar_height", iris->menubar_height }
        } },
        { "ui", toml::table {
            { "theme", iris->theme },
            { "codeview_color_scheme", iris->codeview_color_scheme },
            { "codeview_font_scale", iris->codeview_font_scale },
            { "codeview_use_theme_background", iris->codeview_use_theme_background },
            { "scale", iris->ui_scale },
            { "bgcolor", toml::array {
                iris->clear_value.color.float32[0],
                iris->clear_value.color.float32[1],
                iris->clear_value.color.float32[2]
            } },
            { "enable_viewports", iris->imgui_enable_viewports },
#ifdef _WIN32
            { "windows_titlebar_style", iris->windows_titlebar_style },
            { "windows_enable_borders", iris->windows_enable_borders },
            { "windows_dark_mode", iris->windows_dark_mode },
#endif
        } },
        { "audio", toml::table {
            { "mute", iris->mute },
            { "mute_adma", iris->mute_adma },
            { "volume", iris->volume }
        } },
        { "paths", toml::table {
            { "bios_path", iris->bios_path },
            { "rom1_path", iris->rom1_path },
            { "rom2_path", iris->rom2_path },
            { "nvram_path", iris->nvram_path },
            { "mcd0_path", iris->mcd0_path },
            { "mcd1_path", iris->mcd1_path },
            { "snap_path", iris->snap_path },
            { "flash_path", iris->flash_path },
            { "gcdb_path", iris->gcdb_path },
        } },
        { "recents", toml::table {
            { "array", toml::array() }
        } },
        { "shaders", toml::table {
            { "enable", iris->enable_shaders },
            { "array", toml::array() }
        } },
    };

    toml::array* recents = tbl["recents"]["array"].as_array();

    for (const std::string& s : iris->recents)
        recents->push_back(s);

    toml::array* shaders = tbl["shaders"]["array"].as_array();

    for (auto& s : shaders::vector(iris))
        shaders->push_back(s->get_id());

    // Generate input mappings file
    mappings_file << "# File auto-generated by " IRIS_TITLE "\n\n";

    toml::table mappings_tbl {};

    for (auto& map : iris->input_maps) {
        toml::table map_tbl { { map.name, toml::table {} } };

        for (auto& entry : map.map.forward_map()) {
            toml::table t {
                { std::to_string(entry.first), entry.second }
            };

            t.is_inline(true);

            map_tbl[map.name].as_table()->insert(std::to_string(entry.first), entry.second);
        }

        mappings_tbl.insert(map.name, map_tbl[map.name]);
    }

    file << tbl;
    mappings_file << mappings_tbl;
}

}
