#include "iris.hpp"
#include "iop/mg.hpp"
#include "dev/mcd.hpp"
#include "config.hpp"

#include "ps2_elf.hpp"
#include "ps2_iso9660.hpp"
#include "ps2.hpp"
#include "kp2/p2io.hpp"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

#include <algorithm>
#include <filesystem>

namespace iris::settings {

bool parse_mappings_file(Instance* iris) {
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
    toml::table tbl;

    if (!reset) {
        toml::parse_result result = toml::parse_file(iris->paths.settings_path);

        if (result) {
            tbl = std::move(result).table();
        } else if (std::filesystem::exists(iris->paths.settings_path)) {
            std::string desc(result.error().description());

            iris_error(&iris->log.settings, "Couldn't parse settings file: {}", desc.c_str());
        }
    }

    auto paths = tbl["paths"];
    iris->paths.bios_path = paths["bios_path"].value_or("");

    for (int i = 0; i < emu::ARCADE_BIOS_COUNT; i++) {
        iris->paths.arcade_bios_paths[i] = paths[emu::get_arcade_bios_key(i)].value_or("");
    }

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
    iris->paths.log_path = paths["log_path"].value_or("");
    iris->paths.hdd_id_path = paths["hdd_id_path"].value_or("");
    iris->paths.dongle_black_path = paths["dongle_black_path"].value_or("");
    iris->paths.dongle_white_path = paths["dongle_white_path"].value_or("");
    iris->paths.mecha_civ_path = paths["mecha_civ_path"].value_or("");
    iris->paths.mecha_cks_path = paths["mecha_cks_path"].value_or("");
    iris->paths.mecha_eks_path = paths["mecha_eks_path"].value_or("");
    iris->paths.mecha_kek_path = paths["mecha_kek_path"].value_or("");
    iris->paths.mecha_kelf_kbit_path = paths["mecha_kelf_kbit_path"].value_or("");
    iris->paths.mecha_kelf_kc_path = paths["mecha_kelf_kc_path"].value_or("");

    if (iris->paths.log_path.empty())
        iris->paths.log_path = iris->paths.pref_path + "iris.log";

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
    iris->audio.mute_adma = audio["mute_adma"].value_or(false);

    auto debugger = tbl["debugger"];
    iris->ui.show_status_bar = debugger["show_status_bar"].value_or(true);
    iris->ui.show_overlay = debugger["show_overlay"].value_or(false);
    iris->ui.show_imgui_demo = debugger["show_imgui_demo"].value_or(false);
    iris->skip_fmv = debugger["skip_fmv"].value_or(false);
    iris->timescale = debugger["timescale"].value_or(1);
    iris->log_to_console = debugger["log_to_console"].value_or(true);
    iris->log_to_file = debugger["log_to_file"].value_or(true);
    iris->log_level = log_level_from_name(debugger["log_level"].value_or(std::string("info")));

    for (Applet* a : iris->applets.all) {
        if (a->persist) {
            a->open = debugger[std::string("show_") + a->id].value_or(a->open);
        }
    }

    Debugger& dbg = iris->applets.debugger;

    dbg.show_left = debugger["dbg_show_left"].value_or(dbg.show_left);
    dbg.show_memory = debugger["dbg_show_memory"].value_or(dbg.show_memory);
    dbg.show_logs = debugger["dbg_show_logs"].value_or(dbg.show_logs);
    dbg.memory_open = debugger["dbg_memory_open"].value_or(dbg.memory_open);
    dbg.logs_open = debugger["dbg_logs_open"].value_or(dbg.logs_open);
    dbg.left_width = debugger["dbg_left_width"].value_or(dbg.left_width);
    dbg.right_width = debugger["dbg_right_width"].value_or(dbg.right_width);
    dbg.disasm_height = debugger["dbg_disasm_height"].value_or(dbg.disasm_height);
    dbg.memory_height = debugger["dbg_memory_height"].value_or(dbg.memory_height);

    FileExplorer& fe = iris->applets.file_explorer;

    fe.sidebar_width = debugger["fe_sidebar_width"].value_or(fe.sidebar_width);
    fe.raw_view = debugger["fe_raw_view"].value_or(fe.raw_view);
    fe.hide_ecc = debugger["fe_hide_ecc"].value_or(fe.hide_ecc);
    fe.show_deleted = debugger["fe_show_deleted"].value_or(fe.show_deleted);
    fe.show_hidden = debugger["fe_show_hidden"].value_or(fe.show_hidden);
    fe.preview_height = debugger["fe_preview_height"].value_or(fe.preview_height);
    fe.show_preview = debugger["fe_show_preview"].value_or(fe.show_preview);
    fe.sort_column = debugger["fe_sort_column"].value_or(fe.sort_column);
    fe.sort_ascending = debugger["fe_sort_ascending"].value_or(fe.sort_ascending);
    fe.last_extract_dir = debugger["fe_last_extract_dir"].value_or("");
    fe.last_device = debugger["fe_last_device"].value_or("");

    auto usb = tbl["usb"];
    iris->input.usb_devices[0] = usb["port1_device"].value_or(usb::USB_DEVICE_NONE);
    iris->input.usb_devices[1] = usb["port2_device"].value_or(usb::USB_DEVICE_NONE);
    iris->input.usb_msd_paths[0] = usb["port1_msd_image"].value_or("");
    iris->input.usb_msd_paths[1] = usb["port2_msd_image"].value_or("");

    auto system = tbl["system"];
    iris->system = system["model"].value_or(ps2::AUTO);
    iris->autostart = system["autostart"].value_or(true);
    iris->cache_arcade_files = system["cache_arcade_files"].value_or(false);
    iris->arcade_dongle_boot = system["arcade_dongle_boot"].value_or(false);
    iris->enable_magicgate = system["enable_magicgate"].value_or(true);
    iris->p2io_input_type = system["p2io_input_type"].value_or(kp2::p2io::INPUT_THRILL_DRIVE);

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
    iris->slirp_config.enabled = network["enabled"].value_or(true);
    iris->slirp_config.network = network["network"].value_or("10.0.2.0");
    iris->slirp_config.netmask = network["netmask"].value_or("255.255.255.0");
    iris->slirp_config.gateway = network["gateway"].value_or("10.0.2.2");
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
    iris->ui.theme = ui["theme"].value_or(imgui::GRANITE_NEO);
    iris->ui.codeview_font_scale = ui["codeview_font_scale"].value_or(1.0f);
    iris->ui.codeview_color_scheme = ui["codeview_color_scheme"].value_or(imgui::CodeviewColorScheme::SOLARIZED_DARK);
    iris->ui.codeview_use_theme_background = ui["codeview_use_theme_background"].value_or(true);
    iris->ui.ui_scale = ui["scale"].value_or(1.0f);
    iris->ui.imgui_enable_viewports = ui["enable_viewports"].value_or(true);

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
    iris->windows_titlebar_style = tbl["ui"]["windows_titlebar_style"].value_or(IRIS_TITLEBAR_SEAMLESS);
    iris->windows_enable_borders = tbl["ui"]["windows_enable_borders"].value_or(true);
#endif

#ifdef IRIS_HAS_DARK_TITLEBAR
    iris->dark_titlebar = tbl["ui"]["dark_titlebar"].value_or(tbl["ui"]["windows_dark_mode"].value_or(true));
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

static int mg_mode_for_system(int system) {
    switch (system) {
        case ps2::KONAMI_PYTHON:

        // Python 2 is a retail system
        // case ps2::KONAMI_PYTHON2:
        case ps2::NAMCO_SYSTEM_147:
        case ps2::NAMCO_SYSTEM_148:
        case ps2::NAMCO_SYSTEM_246:
        case ps2::NAMCO_SYSTEM_256:
        case ps2::NAMCO_SYSTEM_SUPER_256: {
            return mg::KEY_STORE_MODE_ARCADE;
        } break;
    }

    return mg::KEY_STORE_MODE_RETAIL;
}

static int mg_card_key_source(Instance* iris) {
    int mode = mg_mode_for_system(iris->ps2->detected_system);

    return mode == mg::KEY_STORE_MODE_ARCADE
        ? dev::mcd::CARD_KEY_ARCADE
        : dev::mcd::CARD_KEY_RETAIL;
}

void apply_card_magicgate(Instance* iris, int slot) {
    if (slot < 0 || slot >= 2 || !iris->input.mcd[slot])
        return;

    dev::mcd::set_magicgate(iris->input.mcd[slot], iris->enable_magicgate, mg_card_key_source(iris),
        cdvd::mg_challenge_iv(iris->ps2->cdvd), iris->paths.mecha_card_id_path.c_str());
}

void apply_mg_keys(Instance* iris) {
    cdvd::Cdvd* cdvd = iris->ps2->cdvd;

    cdvd::load_mg_key(cdvd, mg::KEY_CHALLENGE_IV, iris->paths.mecha_civ_path.c_str());
    cdvd::load_mg_key(cdvd, mg::KEY_CARD_KEY_STORE, iris->paths.mecha_cks_path.c_str());
    cdvd::load_mg_key(cdvd, mg::KEY_ENCRYPTED_KEY_STORE, iris->paths.mecha_eks_path.c_str());
    cdvd::load_mg_key(cdvd, mg::KEY_STORE_KEY, iris->paths.mecha_kek_path.c_str());
    cdvd::load_mg_key(cdvd, mg::KEY_ARCADE_KELF_KBIT, iris->paths.mecha_kelf_kbit_path.c_str());
    cdvd::load_mg_key(cdvd, mg::KEY_ARCADE_KELF_KC, iris->paths.mecha_kelf_kc_path.c_str());

    int mode = mg_mode_for_system(iris->ps2->detected_system);

    iris_debug(&iris->log.settings, "MagicGate: system {} selects key store mode {}",
        iris->ps2->detected_system, mode);

    cdvd::derive_mg_keys(cdvd, mode);

    apply_magicgate(iris);
}

void apply_magicgate(Instance* iris) {
    cdvd::set_mg_enabled(iris->ps2->cdvd, iris->enable_magicgate);

    for (int i = 0; i < 2; i++) {
        apply_card_magicgate(iris, i);
    }
}

void apply_p2io(Instance* iris) {
    usb::p2io_set_input_type(iris->ps2->usb, iris->p2io_input_type);

    usb::p2io_set_dongle(iris->ps2->usb, kp2::p2io::DONGLE_BLACK, iris->paths.dongle_black_path.c_str());
    usb::p2io_set_dongle(iris->ps2->usb, kp2::p2io::DONGLE_WHITE, iris->paths.dongle_white_path.c_str());
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

bool init(Instance* iris) {
    if (iris->paths.settings_path.empty())
        iris->paths.settings_path = iris->paths.pref_path + "settings.toml";

    if (iris->paths.mappings_path.empty())
        iris->paths.mappings_path = iris->paths.pref_path + "mappings.toml";

    parse_toml_settings(iris, iris->cli.reset_settings);

    // CLI settings override TOML settings
    cli::apply(iris);

    if (iris->cli.reset_settings)
        save(iris);

    ps2::set_system(iris->ps2, iris->system);

    if (!emu::load_rom_files(iris))
        iris->applets.bios_setting.show();

    if (iris->paths.mcd0_path.size())
        emu::attach_memory_card(iris, 0, iris->paths.mcd0_path.c_str());

    if (iris->paths.mcd1_path.size())
        emu::attach_memory_card(iris, 1, iris->paths.mcd1_path.c_str());

    ps2::set_timescale(iris->ps2, iris->timescale);

    apply_device_maps(iris);

    ee::set_fmv_skip(iris->ps2->ee, iris->skip_fmv);

    speed::load_flash(iris->ps2->speed, iris->paths.flash_path.c_str());
    speed::load_hdd(iris->ps2->speed, iris->paths.hdd_path.c_str());
    speed::load_hdd_id(iris->ps2->speed, iris->paths.hdd_id_path.c_str());
    speed::set_mac_address(iris->ps2->speed, iris->mac_address);

    slirp::start(iris->ps2->speed->smap, iris->slirp_config, &iris->log.slirp);

    for (int i = 0; i < 2; i++) {
        if (iris->input.usb_msd_paths[i].size())
            usb::msd_set_image(iris->ps2->usb, i, iris->input.usb_msd_paths[i].c_str());

        if (usb::get_port_device(iris->ps2->usb, i) == usb::USB_DEVICE_P2IO)
            continue;

        usb::set_port_device(iris->ps2->usb, i, iris->input.usb_devices[i]);
    }

    apply_mg_keys(iris);

    apply_p2io(iris);

    emu::clean_arcade_files(iris);

    cli::boot(iris);

    return true;
}

void save(Instance* iris) {
    if (!iris->dump_to_file)
        return;

    cli::unapply(iris);

    std::ofstream file(iris->paths.settings_path);
    std::ofstream mappings_file(iris->paths.mappings_path);

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
            { "autostart", iris->autostart },
            { "cache_arcade_files", iris->cache_arcade_files },
            { "arcade_dongle_boot", iris->arcade_dongle_boot },
            { "enable_magicgate", iris->enable_magicgate },
            { "p2io_input_type", iris->p2io_input_type }
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
            { "timescale", iris->timescale },
            { "log_to_console", iris->log_to_console },
            { "log_to_file", iris->log_to_file },
            { "log_level", log_level_name(iris->log_level) }
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
#endif
#ifdef IRIS_HAS_DARK_TITLEBAR
            { "dark_titlebar", iris->dark_titlebar },
#endif
        } },
        { "audio", toml::table {
            { "mute", iris->audio.mute },
            { "mute_adma", iris->audio.mute_adma },
            { "volume", iris->audio.volume }
        } },
        { "paths", toml::table {
            { "bios_path", iris->paths.bios_path },
            { "arcade_bios_147_path", iris->paths.arcade_bios_paths[emu::ARCADE_BIOS_147] },
            { "arcade_bios_148_path", iris->paths.arcade_bios_paths[emu::ARCADE_BIOS_148] },
            { "arcade_bios_246_path", iris->paths.arcade_bios_paths[emu::ARCADE_BIOS_246] },
            { "arcade_bios_256_path", iris->paths.arcade_bios_paths[emu::ARCADE_BIOS_256] },
            { "arcade_bios_python_path", iris->paths.arcade_bios_paths[emu::ARCADE_BIOS_PYTHON] },
            { "arcade_bios_python2_path", iris->paths.arcade_bios_paths[emu::ARCADE_BIOS_PYTHON2] },
            { "rom1_path", iris->paths.rom1_path },
            { "rom2_path", iris->paths.rom2_path },
            { "nvram_path", iris->paths.nvram_path },
            { "mcd0_path", iris->paths.mcd0_path },
            { "mcd1_path", iris->paths.mcd1_path },
            { "snap_path", iris->paths.snap_path },
            { "flash_path", iris->paths.flash_path },
            { "gcdb_path", iris->paths.gcdb_path },
            { "hdd_path", iris->paths.hdd_path },
            { "log_path", iris->paths.log_path },
            { "hdd_id_path", iris->paths.hdd_id_path },
            { "dongle_black_path", iris->paths.dongle_black_path },
            { "dongle_white_path", iris->paths.dongle_white_path },
            { "mecha_civ_path", iris->paths.mecha_civ_path },
            { "mecha_cks_path", iris->paths.mecha_cks_path },
            { "mecha_eks_path", iris->paths.mecha_eks_path },
            { "mecha_kek_path", iris->paths.mecha_kek_path },
            { "mecha_kelf_kbit_path", iris->paths.mecha_kelf_kbit_path },
            { "mecha_kelf_kc_path", iris->paths.mecha_kelf_kc_path },
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

    for (auto& s : shaders::vector(iris)) {
        std::string id = s->get_id();

        if (std::find(iris->cli.shaders.begin(), iris->cli.shaders.end(), id) != iris->cli.shaders.end())
            continue;

        shaders->push_back(id);
    }

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

        const Debugger& dbg = iris->applets.debugger;

        debugger_tbl->insert_or_assign("dbg_show_left", dbg.show_left);
        debugger_tbl->insert_or_assign("dbg_show_memory", dbg.show_memory);
        debugger_tbl->insert_or_assign("dbg_show_logs", dbg.show_logs);
        debugger_tbl->insert_or_assign("dbg_memory_open", dbg.memory_open);
        debugger_tbl->insert_or_assign("dbg_logs_open", dbg.logs_open);
        debugger_tbl->insert_or_assign("dbg_left_width", dbg.left_width);
        debugger_tbl->insert_or_assign("dbg_right_width", dbg.right_width);
        debugger_tbl->insert_or_assign("dbg_disasm_height", dbg.disasm_height);
        debugger_tbl->insert_or_assign("dbg_memory_height", dbg.memory_height);

        const FileExplorer& fe = iris->applets.file_explorer;

        debugger_tbl->insert_or_assign("fe_sidebar_width", fe.sidebar_width);
        debugger_tbl->insert_or_assign("fe_raw_view", fe.raw_view);
        debugger_tbl->insert_or_assign("fe_hide_ecc", fe.hide_ecc);
        debugger_tbl->insert_or_assign("fe_show_deleted", fe.show_deleted);
        debugger_tbl->insert_or_assign("fe_show_hidden", fe.show_hidden);
        debugger_tbl->insert_or_assign("fe_preview_height", fe.preview_height);
        debugger_tbl->insert_or_assign("fe_show_preview", fe.show_preview);
        debugger_tbl->insert_or_assign("fe_sort_column", fe.sort_column);
        debugger_tbl->insert_or_assign("fe_sort_ascending", fe.sort_ascending);
        debugger_tbl->insert_or_assign("fe_last_extract_dir", fe.last_extract_dir);
        debugger_tbl->insert_or_assign("fe_last_device", fe.last_device);
    }

    file << tbl;
    mappings_file << mappings_tbl;

    cli::reapply(iris);
}

void close(Instance* iris) {
    save(iris);
}

}
