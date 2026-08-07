#include "iris.hpp"
#include "config.hpp"

#include "ps2_elf.hpp"
#include "ps2_iso9660.hpp"
#include "ps2.hpp"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

#include <filesystem>

namespace iris::settings {

void print_version() {
    puts(
        "iris (" STR(_IRIS_VERSION) " " STR(_IRIS_OSVERSION) ")\n"
        "Copyright (C) 2026 Allkern/Lisandro Alarcon\n\n"
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
        "      --reset-settings     Reset the settings file to its defaults\n"
        "  -h, --help               Display this help and exit\n"
        "  -v, --version            Output version information and exit\n"
    );
}

bool parse_mappings_file(Instance* iris) {
    iris->paths.mappings_path = iris->paths.pref_path + "mappings.toml";

    std::ifstream mappings_file(iris->paths.mappings_path);

    if (!mappings_file.is_open())
        return false;

    toml::parse_result result = toml::parse(mappings_file);

    if (!result) {
        std::string desc(result.error().description());

        iris_error(&iris->log.settings, "Couldn't parse mappings file: {}", desc.c_str());

        return false;
    }

    toml::table& tbl = result.table();

    for (auto& map : tbl) {
        iris_info(&iris->log.settings, "Parsing input map \"{}\"...", map.first.data());

        Mapping input_mapping {};

        input_mapping.name = map.first.data();
        input_mapping.map = bidirectional_map<uint64_t, InputAction>();

        for (auto& input_map : *map.second.as_table()) {
            uint64_t key = std::stoull(input_map.first.data());
            uint64_t value = input_map.second.as_integer()->get();

            input_mapping.map.insert(key, static_cast<InputAction>(value));

            // printf("entry: %d -> %d\n", std::stoull(input_map.first.data()), input_map.second.as_integer()->get());
        }

        iris->input.input_maps.push_back(input_mapping);
    }

    return true;
}

bool parse_toml_settings(Instance* iris, bool reset) {
    iris->paths.settings_path = iris->paths.pref_path + "settings.toml";

    toml::table tbl;

    if (!reset) {
        toml::parse_result result = toml::parse_file(iris->paths.settings_path);

        if (!result) {
            std::string desc(result.error().description());

            iris_error(&iris->log.settings, "Couldn't parse settings file: {}", desc.c_str());

            return false;
        }

        tbl = std::move(result).table();
    }

    auto paths = tbl["paths"];
    iris->paths.bios_path = paths["bios_path"].value_or("");
    iris->paths.rom1_path = paths["rom1_path"].value_or("");
    iris->paths.rom2_path = paths["rom2_path"].value_or("");
    iris->paths.nvram_path = paths["nvram_path"].value_or("");
    iris->paths.mcd0_path = paths["mcd0_path"].value_or("");
    iris->paths.mcd1_path = paths["mcd1_path"].value_or("");
    iris->paths.snap_path = paths["snap_path"].value_or("snap");
    iris->paths.flash_path = paths["flash_path"].value_or("");
    iris->paths.gcdb_path = paths["gcdb_path"].value_or("");
    iris->paths.hdd_path = paths["hdd_path"].value_or("");
    iris->paths.auto_paths = paths["auto"].value_or(true);

    auto host = tbl["host"];
    iris->paths.host_path = host["path"].value_or("");
    iris->paths.host_from_elf = host["from_elf"].value_or(false);

    iris->paths.device_maps.clear();

    if (auto devices = tbl["devices"].as_table()) {
        for (auto&& [key, value] : *devices) {
            std::string device(key.str());

            auto path = value.value<std::string>();

            if (!path)
                continue;

            if (device == "host") {
                if (iris->paths.host_path.empty())
                    iris->paths.host_path = *path;

                continue;
            }

            iris->paths.device_maps.emplace_back(device, *path);
        }
    }

    auto window = tbl["window"];
    iris->window_width = window["window_width"].value_or(960);
    iris->window_height = window["window_height"].value_or(720);
    iris->fullscreen = window["fullscreen"].value_or(0);

    auto display = tbl["display"];
    iris->aspect_mode = display["aspect_mode"].value_or(render::AUTO);
    iris->filter = display["filter"].value_or(true);
    iris->integer_scaling = display["integer_scaling"].value_or(false);
    iris->scale = display["scale"].value_or(1.5f);
    iris->renderer_backend = display["renderer"].value_or(gs::renderer::BACKEND_HARDWARE);
    iris->window_width = display["window_width"].value_or(960);
    iris->window_height = display["window_height"].value_or(720);
    iris->ui.menubar_height = display["menubar_height"].value_or(0);
    iris->angle = display["angle"].value_or(0);
    iris->flip_x = display["flip_x"].value_or(false);
    iris->flip_y = display["flip_y"].value_or(false);
    iris->present_mode = display["present_mode"].value_or(render::FPS_60);

    auto audio = tbl["audio"];
    iris->audio.mute = audio["mute"].value_or(false);
    iris->audio.volume = audio["volume"].value_or(1.0);
    iris->audio.mute_adma = audio["mute_adma"].value_or(true);

    auto debugger = tbl["debugger"];
    iris->ui.show_status_bar = debugger["show_status_bar"].value_or(true);
    iris->ui.show_overlay = debugger["show_overlay"].value_or(false);
    iris->ui.show_imgui_demo = debugger["show_imgui_demo"].value_or(false);
    iris->skip_fmv = debugger["skip_fmv"].value_or(false);
    iris->timescale = debugger["timescale"].value_or(2);

    for (Applet* a : iris->applets.all) {
        if (a->persist)
            a->open = debugger[std::string("show_") + a->id].value_or(a->open);
    }

    auto usb = tbl["usb"];
    iris->input.usb_devices[0] = usb["port1_device"].value_or(usb::USB_DEVICE_NONE);
    iris->input.usb_devices[1] = usb["port2_device"].value_or(usb::USB_DEVICE_NONE);
    iris->input.usb_msd_paths[0] = usb["port1_msd_image"].value_or("");
    iris->input.usb_msd_paths[1] = usb["port2_msd_image"].value_or("");

    auto system = tbl["system"];
    iris->system = system["model"].value_or(ps2::AUTO);
    iris->autostart = system["autostart"].value_or(true);

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

    auto network = tbl["network"];
    iris->slirp_config.enabled    = network["enabled"].value_or(true);
    iris->slirp_config.network    = network["network"].value_or("10.0.2.0");
    iris->slirp_config.netmask    = network["netmask"].value_or("255.255.255.0");
    iris->slirp_config.gateway    = network["gateway"].value_or("10.0.2.2");
    iris->slirp_config.dhcp_start = network["dhcp_start"].value_or("10.0.2.15");
    iris->slirp_config.nameserver = network["nameserver"].value_or("10.0.2.3");

    auto screenshots = tbl["screenshots"];
    iris->screenshot_format = screenshots["format"].value_or(render::PNG);
    iris->screenshot_jpg_quality_mode = screenshots["jpg_quality_mode"].value_or(render::MAXIMUM);
    iris->screenshot_jpg_quality = screenshots["jpg_quality"].value_or(50);
    iris->screenshot_mode = screenshots["mode"].value_or(render::INTERNAL);
    iris->screenshot_shader_processing = screenshots["shader_processing"].value_or(false);

    auto hardware = tbl["hardware"];
    iris->hardware_backend_config.super_sampling = hardware["super_sampling"].value_or(0);
    iris->hardware_backend_config.super_sampled_quads = hardware["super_sampled_quads"].value_or(false);
    iris->hardware_backend_config.force_progressive = hardware["force_progressive"].value_or(false);
    iris->hardware_backend_config.overscan = hardware["overscan"].value_or(false);
    iris->hardware_backend_config.crtc_offsets = hardware["crtc_offsets"].value_or(false);
    iris->hardware_backend_config.disable_mipmaps = hardware["disable_mipmaps"].value_or(false);
    iris->hardware_backend_config.unsynced_readbacks = hardware["unsynced_readbacks"].value_or(false);
    iris->hardware_backend_config.backbuffer_promotion = hardware["backbuffer_promotion"].value_or(false);
    iris->hardware_backend_config.allow_blend_demote = hardware["allow_blend_demote"].value_or(false);
    iris->hardware_backend_config.enable_analog_video = hardware["enable_analog_video"].value_or(false);
    iris->hardware_backend_config.analog_cable = hardware["analog_cable"].value_or(0);
    iris->hardware_backend_config.analog_system = hardware["analog_system"].value_or(0);
    iris->hardware_backend_config.line_comb = hardware["line_comb"].value_or(false);
    iris->hardware_backend_config.skip_notch = hardware["skip_notch"].value_or(false);
    iris->hardware_backend_config.invert_fields = hardware["invert_fields"].value_or(false);

    auto vulkan = tbl["vulkan"];
    iris->vk.vulkan_physical_device = vulkan["physical_device"].value_or(-1);
    iris->vk.vulkan_enable_validation_layers = vulkan["enable_validation_layers"].value_or(false);

    auto ui = tbl["ui"];
    iris->ui.theme = ui["theme"].value_or(IRIS_THEME_GRANITE);
    iris->ui.codeview_font_scale = ui["codeview_font_scale"].value_or(1.0f);
    iris->ui.codeview_color_scheme = ui["codeview_color_scheme"].value_or(IRIS_CODEVIEW_COLOR_SCHEME_SOLARIZED_DARK);
    iris->ui.codeview_use_theme_background = ui["codeview_use_theme_background"].value_or(true);
    iris->ui.ui_scale = ui["scale"].value_or(1.0f);
    iris->ui.imgui_enable_viewports = ui["enable_viewports"].value_or(false);

    toml::array* bgcolor = tbl["ui"]["bgcolor"].as_array();

    if (bgcolor && bgcolor->size() == 3) {
        iris->vk.clear_value.color.float32[0] = (float)bgcolor->at(0).as_floating_point()->get();
        iris->vk.clear_value.color.float32[1] = (float)bgcolor->at(1).as_floating_point()->get();
        iris->vk.clear_value.color.float32[2] = (float)bgcolor->at(2).as_floating_point()->get();
    } else {
        iris->vk.clear_value.color.float32[0] = 0.11f;
        iris->vk.clear_value.color.float32[1] = 0.11f;
        iris->vk.clear_value.color.float32[2] = 0.11f;
    }

#ifdef _WIN32
    iris->windows_titlebar_style = tbl["ui"]["windows_titlebar_style"].value_or(IRIS_TITLEBAR_DEFAULT);
    iris->windows_enable_borders = tbl["ui"]["windows_enable_borders"].value_or(true);
    iris->windows_dark_mode = tbl["ui"]["windows_dark_mode"].value_or(true);
#endif

    toml::array* recents = tbl["recents"]["array"].as_array();

    if (recents) {
        for (int i = 0; i < recents->size(); i++) {
            toml::table* entry = recents->at(i).as_table();

            if (!entry) {
                // Provided for backcompat with older settings files
                std::string str = recents->at(i).as_string()->get();

                iris->recents.push_back({ str, RecentType::PS2 });

                continue;
            }

            Recent r = {
                entry->operator[]("path").value_or(std::string()),
                (RecentType)entry->operator[]("type").value_or(0)
            };

            iris->recents.push_back(r);
        }
    }

    toml::array* shaders = tbl["shaders"]["array"].as_array();
    iris->enable_shaders = tbl["shaders"]["enable"].value_or(false);

    if (shaders) {
        for (int i = 0; i < shaders->size(); i++)
            iris->vk.shader_passes_pending.push_back(shaders->at(i).as_string()->get());
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
        } else if (a == "--reset-settings") {
            Instance* tmp = create();

            if (std::filesystem::exists("portable") || std::filesystem::exists("portable.txt")) {
                tmp->paths.pref_path = "./";
            } else {
                char* pref = SDL_GetPrefPath("Allkern", "Iris");

                tmp->paths.pref_path = pref ? std::string(pref) : "./";

                if (pref)
                    SDL_free(pref);
            }

            // Load defaults (ignoring the existing file) and write them back out.
            parse_toml_settings(tmp, true);
            close(tmp);

            printf("iris: Settings reset\n");

            return true;
        }
    }

    return false;
}

void parse_cli_settings(Instance* iris, int argc, const char* argv[]) {
    std::string bios_path;
    std::string rom1_path;
    std::string rom2_path;

    for (int i = 1; i < argc; i++) {
        std::string a(argv[i]);

        if (a == "-x" || a == "--executable") {
            iris->paths.elf_path = argv[i+1];

            ++i;
        } else if (a == "-d" || a == "--boot") {
            iris->paths.boot_path = argv[i+1];

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
            iris->paths.disc_path = argv[i+1];

            ++i;
        } else if (a == "--slot1") {
            iris->paths.mcd0_path = argv[i+1];

            ++i;
        } else if (a == "--slot2") {
            iris->paths.mcd1_path = argv[i+1];

            ++i;
        } else if (a == "-H" || a == "--headless") {
            iris->headless = true;
        } else if (a == "-S" || a == "--snap-on-exit") {
            iris->snap_on_exit = true;
        } else {
            iris->paths.disc_path = argv[i];
        }
    }

    if (bios_path.size()) {
        if (!ps2::load_bios(iris->ps2, bios_path.c_str())) {
            // push_info(iris, "Couldn't load BIOS");

            iris->applets.bios_setting.open = true;
        }
    } else {
        if (iris->paths.bios_path.size()) {
            if (!ps2::load_bios(iris->ps2, iris->paths.bios_path.c_str())) {
                // push_info(iris, "Couldn't load BIOS");

                iris->applets.bios_setting.open = true;
            }
        } else {
            iris->applets.bios_setting.open = true;
        }
    }

    if (rom1_path.size()) {
        if (!ps2::load_rom1(iris->ps2, rom1_path.c_str())) {
            // push_info(iris, "Couldn't load ROM1");
        }
    } else {
        if (iris->paths.rom1_path.size()) {
            if (!ps2::load_rom1(iris->ps2, iris->paths.rom1_path.c_str())) {
                // push_info(iris, "Couldn't load ROM1");
            }
        }
    }

    if (rom2_path.size()) {
        if (!ps2::load_rom2(iris->ps2, rom2_path.c_str())) {
            // push_info(iris, "Couldn't load ROM2");
        }
    } else {
        if (iris->paths.rom2_path.size()) {
            if (!ps2::load_rom2(iris->ps2, iris->paths.rom2_path.c_str())) {
                // push_info(iris, "Couldn't load ROM2");
            }
        }
    }

    if (iris->paths.elf_path.size()) {
        ps2::set_system(iris->ps2, iris->system);
        ps2::load_bios(iris->ps2, iris->paths.bios_path.c_str());
        elf::load(iris->ps2, iris->paths.elf_path.c_str());

        iris->loaded = iris->paths.elf_path;
    }

    if (iris->paths.boot_path.size()) {
        ps2::set_system(iris->ps2, iris->system);
        ps2::load_bios(iris->ps2, iris->paths.bios_path.c_str());
        ps2::boot_file(iris->ps2, iris->paths.boot_path.c_str());

        iris->loaded = iris->paths.boot_path;
    }

    if (iris->paths.disc_path.size()) {
        if (cdvd::open(iris->ps2->cdvd, iris->paths.disc_path.c_str(), 0))
            return;

        char* boot_file = iop::disc::get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file)
            return;

        ps2::set_system(iris->ps2, iris->system);
        ps2::load_bios(iris->ps2, iris->paths.bios_path.c_str());
        ps2::boot_file(iris->ps2, boot_file);

        iris->loaded = iris->paths.disc_path;
    }
}

void apply_device_maps(Instance* iris) {
    ps2::iop_clear_device_maps(iris->ps2);

    const std::string& host = iris->paths.host_from_elf ? iris->paths.host_elf_dir : iris->paths.host_path;

    ps2::iop_map_device(iris->ps2, "host", host.c_str());

    for (const auto& p : iris->paths.device_maps) {
        const std::string& device = p.first;
        const std::string& path = p.second;

        if (device.size() && path.size() && device != "host") {
            ps2::iop_map_device(iris->ps2, device.c_str(), path.c_str());
        }
    }
}

bool init(Instance* iris, int argc, const char* argv[]) {
    parse_toml_settings(iris, false);

    parse_cli_settings(iris, argc, argv);

    emu::load_rom_files(iris);

    if (iris->paths.mcd0_path.size())
        emu::attach_memory_card(iris, 0, iris->paths.mcd0_path.c_str());

    if (iris->paths.mcd1_path.size())
        emu::attach_memory_card(iris, 1, iris->paths.mcd1_path.c_str());

    // Apply settings loaded from file/CLI
    ps2::set_timescale(iris->ps2, iris->timescale);

    apply_device_maps(iris);

    ee::set_fmv_skip(iris->ps2->ee, iris->skip_fmv);

    ps2::set_system(iris->ps2, iris->system);
    speed::load_flash(iris->ps2->speed, iris->paths.flash_path.c_str());
    speed::load_hdd(iris->ps2->speed, iris->paths.hdd_path.c_str());
    speed::set_mac_address(iris->ps2->speed, iris->mac_address);

    slirp::start(iris->ps2->speed->smap, iris->slirp_config, &iris->log.slirp);

    for (int i = 0; i < 2; i++) {
        if (iris->input.usb_msd_paths[i].size())
            usb::msd_set_image(iris->ps2->usb, i, iris->input.usb_msd_paths[i].c_str());

        usb::set_port_device(iris->ps2->usb, i, iris->input.usb_devices[i]);
    }

    for (int i = 1; i < argc; i++) {
        std::string a(argv[i]);

        if (a == "--autoboot-disc") {
            std::string path = argv[i+1];

            if (cdvd::open(iris->ps2->cdvd, path.c_str(), 0))
                return false;

            char* boot_file = iop::disc::get_boot_path(iris->ps2->cdvd->disc);

            if (!boot_file)
                return false;

            ps2::set_system(iris->ps2, iris->system);
            ps2::load_bios(iris->ps2, iris->paths.bios_path.c_str());
            ps2::boot_file(iris->ps2, boot_file);

            iris->debug.pause = false;
        }
    }

    return true;
}

void close(Instance* iris) {
    if (!iris->dump_to_file)
        return;

    std::ofstream file(iris->paths.settings_path);
    std::ofstream mappings_file(iris->paths.pref_path + "mappings.toml");

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
            } },
            { "autostart", iris->autostart }
        } },
        { "network", toml::table {
            { "enabled", iris->slirp_config.enabled },
            { "network", iris->slirp_config.network },
            { "netmask", iris->slirp_config.netmask },
            { "gateway", iris->slirp_config.gateway },
            { "dhcp_start", iris->slirp_config.dhcp_start },
            { "nameserver", iris->slirp_config.nameserver }
        } },
        { "input", toml::table {
            { "slot1_device", iris->input.input_devices[0] ? iris->input.input_devices[0]->get_type() : 0 },
            { "slot2_device", iris->input.input_devices[1] ? iris->input.input_devices[1]->get_type() : 0 },
            { "slot1_mapping", iris->input.input_map[0] },
            { "slot2_mapping", iris->input.input_map[1] }
        } },
        { "usb", toml::table {
            { "port1_device", iris->input.usb_devices[0] },
            { "port2_device", iris->input.usb_devices[1] },
            { "port1_msd_image", iris->input.usb_msd_paths[0] },
            { "port2_msd_image", iris->input.usb_msd_paths[1] }
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
            { "super_sampled_quads", iris->hardware_backend_config.super_sampled_quads },
            { "force_progressive", iris->hardware_backend_config.force_progressive },
            { "overscan", iris->hardware_backend_config.overscan },
            { "crtc_offsets", iris->hardware_backend_config.crtc_offsets },
            { "disable_mipmaps", iris->hardware_backend_config.disable_mipmaps },
            { "unsynced_readbacks", iris->hardware_backend_config.unsynced_readbacks },
            { "backbuffer_promotion", iris->hardware_backend_config.backbuffer_promotion },
            { "allow_blend_demote", iris->hardware_backend_config.allow_blend_demote },
            { "enable_analog_video", iris->hardware_backend_config.enable_analog_video },
            { "analog_cable", iris->hardware_backend_config.analog_cable },
            { "analog_system", iris->hardware_backend_config.analog_system },
            { "line_comb", iris->hardware_backend_config.line_comb },
            { "skip_notch", iris->hardware_backend_config.skip_notch },
            { "invert_fields", iris->hardware_backend_config.invert_fields }
        } },
        { "vulkan", toml::table {
            { "physical_device", iris->vk.vulkan_physical_device },
            { "enable_validation_layers", iris->vk.vulkan_enable_validation_layers }
        } },
        { "debugger", toml::table {
            { "show_status_bar", iris->ui.show_status_bar },
            { "show_imgui_demo", iris->ui.show_imgui_demo },
            { "show_overlay", iris->ui.show_overlay },
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
            { "menubar_height", iris->ui.menubar_height },
            { "angle", iris->angle },
            { "flip_x", iris->flip_x },
            { "flip_y", iris->flip_y },
            { "present_mode", iris->present_mode }
        } },
        { "ui", toml::table {
            { "theme", iris->ui.theme },
            { "codeview_color_scheme", iris->ui.codeview_color_scheme },
            { "codeview_font_scale", iris->ui.codeview_font_scale },
            { "codeview_use_theme_background", iris->ui.codeview_use_theme_background },
            { "scale", iris->ui.ui_scale },
            { "bgcolor", toml::array {
                iris->vk.clear_value.color.float32[0],
                iris->vk.clear_value.color.float32[1],
                iris->vk.clear_value.color.float32[2]
            } },
            { "enable_viewports", iris->ui.imgui_enable_viewports },
#ifdef _WIN32
            { "windows_titlebar_style", iris->windows_titlebar_style },
            { "windows_enable_borders", iris->windows_enable_borders },
            { "windows_dark_mode", iris->windows_dark_mode },
#endif
        } },
        { "audio", toml::table {
            { "mute", iris->audio.mute },
            { "mute_adma", iris->audio.mute_adma },
            { "volume", iris->audio.volume }
        } },
        { "paths", toml::table {
            { "bios_path", iris->paths.bios_path },
            { "rom1_path", iris->paths.rom1_path },
            { "rom2_path", iris->paths.rom2_path },
            { "nvram_path", iris->paths.nvram_path },
            { "mcd0_path", iris->paths.mcd0_path },
            { "mcd1_path", iris->paths.mcd1_path },
            { "snap_path", iris->paths.snap_path },
            { "flash_path", iris->paths.flash_path },
            { "gcdb_path", iris->paths.gcdb_path },
            { "hdd_path", iris->paths.hdd_path },
            { "auto", iris->paths.auto_paths }
        } },
        { "host", toml::table {
            { "path", iris->paths.host_path },
            { "from_elf", iris->paths.host_from_elf }
        } },
        { "devices", toml::table {} },
        { "recents", toml::table {
            { "array", toml::array() }
        } },
        { "shaders", toml::table {
            { "enable", iris->enable_shaders },
            { "array", toml::array() }
        } },
    };

    toml::table* devices = tbl["devices"].as_table();

    for (const auto& p : iris->paths.device_maps) {
        const std::string& dev = p.first;
        const std::string& host = p.second;

        if (dev.size()) {
            devices->insert_or_assign(dev, host);
        }
    }

    toml::array* recents = tbl["recents"]["array"].as_array();

    for (const auto& s : iris->recents)
        recents->push_back(toml::table { { "type", (int)s.type }, { "path", s.path } });

    toml::array* shaders = tbl["shaders"]["array"].as_array();

    for (auto& s : shaders::vector(iris))
        shaders->push_back(s->get_id());

    // Generate input mappings file
    mappings_file << "# File auto-generated by " IRIS_TITLE "\n\n";

    toml::table mappings_tbl {};

    for (auto& map : iris->input.input_maps) {
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

    if (toml::table* debugger_tbl = tbl["debugger"].as_table()) {
        for (Applet* a : iris->applets.all) {
            if (a->persist)
                debugger_tbl->insert_or_assign(std::string("show_") + a->id, a->open);
        }
    }

    file << tbl;
    mappings_file << mappings_tbl;
}

}
