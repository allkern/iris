#include <algorithm>
#include <vector>
#include <string>
#include <cctype>
#include <cstring>
#include <functional>
#include <random>

#include "iris.hpp"

#include "misc/cpp/imgui_stdlib.h"
#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"
#include "ps2.hpp"
#include "settings.hpp"
#include "imgui.hpp"
#include "iop/mg.hpp"

namespace iris {

static bool hovered = false;
static std::string tooltip = "";

static float segment_width(const char* const* labels, int count) {
    using namespace ImGui;

    float width = 0.0f;

    for (int i = 0; i < count; i++) {
        float w = CalcTextSize(labels[i]).x;

        if (w > width) width = w;
    }

    return width + GetStyle().FramePadding.x * 2.0f + 12.0f;
}

struct ResetPrompt {
    const char* setting = "";
    std::string detail = "";

    std::function<void()> commit;
    std::function<void()> apply;
    std::function<void()> cancel;

    bool pending = false;
    bool deferred = false;
};

static ResetPrompt reset_prompt;

static void request_reset(const char* setting, std::string detail, std::function<void()> commit, std::function<void()> apply, std::function<void()> cancel) {
    reset_prompt.setting = setting;
    reset_prompt.detail = std::move(detail);
    reset_prompt.commit = std::move(commit);
    reset_prompt.apply = std::move(apply);
    reset_prompt.cancel = std::move(cancel);
    reset_prompt.pending = true;
}

static void draw_reset_prompt(Instance* iris) {
    using namespace ImGui;

    if (reset_prompt.pending) {
        OpenPopup("###resetprompt");

        reset_prompt.pending = false;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    if (GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable && !GetIO().ConfigViewportsNoDecoration)
        flags |= ImGuiWindowFlags_NoTitleBar;

    ImGuiWindowClass window_class;

    window_class.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;

    SetNextWindowClass(&window_class);
    SetNextWindowPos(GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!BeginPopupModal("Reset required###resetprompt", nullptr, flags))
        return;

    PushFont(iris->ui.font_heading);
    Text("%s", reset_prompt.setting);
    PopFont();

    Spacing();

    TextUnformatted(reset_prompt.detail.c_str());

    Spacing();
    TextDisabled(ICON_MS_WARNING " Resetting now will lose any unsaved progress.");
    Spacing();
    Separator();
    Spacing();

    if (Button("Reset")) {
        reset_prompt.commit();
        reset_prompt.apply();

        ps2::reset(iris->ps2);

        CloseCurrentPopup();
    } SameLine();

    if (Button("Apply at next start")) {
        reset_prompt.commit();

        reset_prompt.deferred = true;

        CloseCurrentPopup();
    } SameLine();

    if (Button("Cancel")) {
        if (reset_prompt.cancel)
            reset_prompt.cancel();

        CloseCurrentPopup();
    }

    EndPopup();
}

Mapping* get_input_mapping(Instance* iris, int slot) {
    if (iris->input.input_map[slot] == -1)
        return nullptr;

    return &iris->input.input_maps[iris->input.input_map[slot]];
}

const char* get_input_name(InputAction action) {
    switch (action) {
        case IRIS_DS_BT_SELECT: return "Select";
        case IRIS_DS_BT_L3: return "L3";
        case IRIS_DS_BT_R3: return "R3";
        case IRIS_DS_BT_START: return "Start";
        case IRIS_DS_BT_UP: return "D-pad Up";
        case IRIS_DS_BT_RIGHT: return "D-pad Right";
        case IRIS_DS_BT_DOWN: return "D-pad Down";
        case IRIS_DS_BT_LEFT: return "D-pad Left";
        case IRIS_DS_BT_L2: return "L2";
        case IRIS_DS_BT_R2: return "R2";
        case IRIS_DS_BT_L1: return "L1";
        case IRIS_DS_BT_R1: return "R1";
        case IRIS_DS_BT_TRIANGLE: return "Triangle";
        case IRIS_DS_BT_CIRCLE: return "Circle";
        case IRIS_DS_BT_CROSS: return "Cross";
        case IRIS_DS_BT_SQUARE: return "Square";
        case IRIS_DS_BT_ANALOG: return "Analog";
        case IRIS_DS_AX_RIGHTV_POS: return "Right Stick Vertical+";
        case IRIS_DS_AX_RIGHTV_NEG: return "Right Stick Vertical-";
        case IRIS_DS_AX_RIGHTH_POS: return "Right Stick Horizontal+";
        case IRIS_DS_AX_RIGHTH_NEG: return "Right Stick Horizontal-";
        case IRIS_DS_AX_LEFTV_POS: return "Left Stick Vertical+";
        case IRIS_DS_AX_LEFTV_NEG: return "Left Stick Vertical-";
        case IRIS_DS_AX_LEFTH_POS: return "Left Stick Horizontal+";
        case IRIS_DS_AX_LEFTH_NEG: return "Left Stick Horizontal-";

        case IRIS_S14X_SW_DOWN: return "System 147/148 Down";
        case IRIS_S14X_SW_UP: return "System 147/148 Up";
        case IRIS_S14X_SW_ENTER: return "System 147/148 Enter";
        case IRIS_S14X_SW_TEST: return "System 147/148 Test";
        case IRIS_S14X_SW_SERVICE: return "System 147/148 Service";
        case IRIS_S14X_SW_P1_START: return "System 147/148 P1 Start";
        case IRIS_S14X_SW_P2_START: return "System 147/148 P2 Start";
        case IRIS_S14X_SW_P3_START: return "System 147/148 P3 Start";
        case IRIS_S14X_SW_P4_START: return "System 147/148 P4 Start";

        case IRIS_S2X6_SW_COIN1: return "Arcade Coin 1";
        case IRIS_S2X6_SW_COIN2: return "Arcade Coin 2";
        case IRIS_P2IO_SW_CARD1: return "Python 2 Insert Card 1";
        case IRIS_P2IO_SW_CARD2: return "Python 2 Insert Card 2";
        case IRIS_S2X6_SW_TEST: return "System 246/256 Test";
        case IRIS_INPUT_ACTION_MAX: break;
    }

    return "";
}

std::string get_event_name(const InputEvent& event) {
    std::string name;

    switch (event.type) {
        case EventType::KEYBOARD: {
            SDL_Keycode keycode = static_cast<SDL_Keycode>(event.id);

            name = SDL_GetKeyName(keycode & 0xf0000fff);

            // Append modifier names
            if ((keycode >> 12) & SDL_KMOD_LSHIFT) name = "Left Shift + " + name;
            if ((keycode >> 12) & SDL_KMOD_RSHIFT) name = "Right Shift + " + name;
            if ((keycode >> 12) & SDL_KMOD_LCTRL) name = "Left Ctrl + " + name;
            if ((keycode >> 12) & SDL_KMOD_RCTRL) name = "Right Ctrl + " + name;
            if ((keycode >> 12) & SDL_KMOD_LALT) name = "Left Alt + " + name;
            if ((keycode >> 12) & SDL_KMOD_RALT) name = "Right Alt + " + name;
        } break;

        case EventType::GAMEPAD_BUTTON: {
            SDL_GamepadButton button = static_cast<SDL_GamepadButton>(event.id);

            name = SDL_GetGamepadStringForButton(button);
        } break;

        case EventType::GAMEPAD_AXIS_POS: {
            SDL_GamepadAxis axis = static_cast<SDL_GamepadAxis>(event.id);

            name = SDL_GetGamepadStringForAxis(axis) + std::string("+");
        } break;

        case EventType::GAMEPAD_AXIS_NEG: {
            SDL_GamepadAxis axis = static_cast<SDL_GamepadAxis>(event.id);

            name = SDL_GetGamepadStringForAxis(axis) + std::string("-");
        } break;

        default: {
            name = "unknown";
        } break;
    }

    // Capitalize first letter
    if (!name.empty()) {
        name[0] = std::toupper(name[0]);
    }

    return name;
}

static const char* settings_renderer_names[] = {
    "Null",
    "Software",
    "Software (Threaded)"
};

static const char* settings_aspect_mode_names[] = {
    "Native",
    "Stretch",
    "Stretch (Keep aspect ratio)",
    "Force 4:3 (NTSC)",
    "Force 16:9 (Widescreen)",
    "Force 5:4 (PAL)",
    "Auto"
};

static const char* settings_fullscreen_names[] = {
    "Windowed",
    "Fullscreen (Desktop)",
};

static const char* settings_present_mode_names[] = {
    "Limit to 30 FPS",
    "Limit to 60 FPS",
    "VSync",
    "Uncapped"
};

static const char* settings_rotation_names[] = {
    "0 degrees",
    "90 degrees",
    "180 degrees",
    "270 degrees"
};

static int settings_fullscreen_flags[] = {
    0,
    SDL_WINDOW_FULLSCREEN
};

static const char* settings_buttons[] = {
    " " ICON_MS_DEPLOYED_CODE "  System",
    " " ICON_MS_FOLDER "  Paths",
    " " ICON_MS_MONITOR "  Graphics",
    " " ICON_MS_BRUSH "  Shaders",
    " " ICON_MS_STADIA_CONTROLLER "  Input",
    " " ICON_MS_SD_CARD "  Memory cards",
    " " ICON_MS_USB "  USB",
    " " ICON_MS_HARD_DRIVE "  Devices",
    " " ICON_MS_MORE_HORIZ "  Misc.",
    nullptr
};

static const char* system_names[] = {
    "Auto",
    "Retail (Fat)",
    "Retail (Slim)",
    "PSX DESR",
    "TEST unit (DTL-H)",
    "TOOL unit (DTL-T)",
    "Konami Python",
    "Konami Python 2",
    "Namco System 147",
    "Namco System 148",
    "Namco System 246",
    "Namco System 256",
    "WEGA HVX"
};

static const char* mechacon_model_names[] = {
    "SPC970",
    "Dragon"
};

static int mac_address_callback(ImGuiInputTextCallbackData* data) {
    char hex[13];
    int hexlen = 0;
    int hex_before_cursor = 0;

    for (int i = 0; i < data->BufTextLen && hexlen < 12; i++) {
        if (!isxdigit((unsigned char)data->Buf[i]))
            continue;

        if (i < data->CursorPos)
            hex_before_cursor++;

        hex[hexlen++] = (char)toupper((unsigned char)data->Buf[i]);
    }

    hex[hexlen] = '\0';

    char formatted[18];
    int len = 0;
    int cursor = 0;

    for (int i = 0; i < hexlen; i++) {
        if (i && (i % 2) == 0)
            formatted[len++] = ':';

        if (i == hex_before_cursor)
            cursor = len;

        formatted[len++] = hex[i];
    }

    formatted[len] = '\0';

    if (hex_before_cursor >= hexlen)
        cursor = len;

    if (strcmp(formatted, data->Buf) != 0) {
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, formatted);
        data->CursorPos = cursor;
        data->SelectionStart = data->SelectionEnd = cursor;
    }

    return 0;
}

static void mg_key_input(Instance* iris, const char* label, const char* id, std::string& path) {
    using namespace ImGui;

    Text("%s", label);

    SetNextItemWidth(400.0);

    InputText(id, &path);

    SameLine();

    PushID(id);

    if (Button(ICON_MS_MORE_HORIZ)) {
        audio::mute(iris);

        auto f = pfd::open_file("Select " + std::string(label), path, {
            "Key files (*.bin)", "*.bin",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (!f.result().empty())
            path = f.result()[0];
    }

    SameLine();

    if (Button(ICON_MS_CLOSE))
        path = "";

    PopID();
}

void show_system_settings(Instance* iris) {
    using namespace ImGui;

    Text("Model");

    if (BeginCombo("##combo", system_names[iris->system])) {
        for (int i = 0; i < IM_ARRAYSIZE(system_names); i++) {
            if (imgui::Selectable(system_names[i], i == iris->system)) {
                iris->system = i;

                ps2::set_system(iris->ps2, i);
            }
        }

        EndCombo();
    }

    if (BeginTable("##specs-table", 2, ImGuiTableFlags_SizingFixedSame)) {
        TableNextRow();

        if (iris->system == 0) {
            TableSetColumnIndex(0);
            TextDisabled("Detected system");
            TableSetColumnIndex(1);
            Text("%s", system_names[iris->ps2->detected_system]);
            TableNextRow();
        }

        TableSetColumnIndex(0);
        TextDisabled("Main RAM");
        TableSetColumnIndex(1);
        Text("%zu MB", iris->ps2->ee_ram->size / (1024 * 1024));

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("IOP RAM");
        TableSetColumnIndex(1);
        Text("%zu MB", iris->ps2->iop_ram->size / (1024 * 1024));

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("MechaCon Model");
        TableSetColumnIndex(1);
        Text("%s", mechacon_model_names[iris->ps2->cdvd->mechacon_model]);

        EndTable();
    }

    Text("\nTimescale");

    char buf[16];

    sprintf(buf, "%dx", iris->timescale);

    if (BeginCombo("##timescale", buf)) {
        for (int i = 0; i < 9; i++) {
            char buf[16]; snprintf(buf, 16, "%dx", 1 << i);

            if (imgui::Selectable(buf, iris->timescale == (1 << i))) {
                iris->timescale = (1 << i);

                ps2::set_timescale(iris->ps2, iris->timescale);
            }
        }

        EndCombo();
    }

    auto print_freq = [iris](const char* label, double freq) {
        TableNextRow();

        TableSetColumnIndex(0);
        TextDisabled("%s", label);
        TableSetColumnIndex(1);

        double f = freq / (double)iris->timescale;

        if (f <= 1.0) {
            Text("%.3f KHz", f * 1000.0);
        } else if (f >= 1000.0) {
            Text("%.3f GHz", f / 1000.0);
        } else {
            Text("%.3f MHz", f);
        }
    };

    if (BeginTable("##effective-clock", 2, ImGuiTableFlags_SizingFixedSame)) {
        print_freq("EE clock", 294.9121); // in MHz
        print_freq("IOP clock", 36.8641);

        EndTable();
    }

    imgui::section(iris, "Network");

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);

    if (Checkbox("Enable networking", &iris->slirp_config.enabled))
        slirp::restart(iris->ps2->speed->smap, iris->slirp_config, &iris->log.slirp);

    PopStyleVar();

    SameLine();
    TextDisabled("%s", slirp::running() ? "(running)" : "(stopped)");

    BeginDisabled(!iris->slirp_config.enabled);

    Spacing();

    bool valid = true;

    auto ip_input = [&](const char* label, const char* id, std::string& value) {
        TableNextRow();

        bool ok = slirp::valid_ipv4(value);

        valid = valid && ok;

        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        Text("%s", label);

        PushFont(iris->ui.font_code);
        SetNextItemWidth((CalcTextSize("000.000.000.000").x + GetStyle().FramePadding.x) * 2.0f);

        if (!ok) {
            PushStyleColor(ImGuiCol_Text, IM_COL32(230, 90, 90, 255));
        }

        TableSetColumnIndex(1);
        InputTextWithHint(id, "0.0.0.0", &value, ImGuiInputTextFlags_CharsDecimal);

        if (!ok) {
            PopStyleColor();
        }

        PopFont();
    };

    if (BeginTable("##network-table", 2, ImGuiTableFlags_SizingFixedFit)) {
        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        Text("MAC Address");

        static char mac_address[18];
        static bool mac_editing = false;

        if (!mac_editing) {
            snprintf(mac_address, sizeof(mac_address), "%02X:%02X:%02X:%02X:%02X:%02X",
                     iris->mac_address[0], iris->mac_address[1], iris->mac_address[2],
                     iris->mac_address[3], iris->mac_address[4], iris->mac_address[5]);
        }

        TableSetColumnIndex(1);

        PushFont(iris->ui.font_code);
        SetNextItemWidth((CalcTextSize("000.000.000.000").x + GetStyle().FramePadding.x) * 2.0f);

        if (InputTextWithHint("##macaddress", "00:00:00:00:00:00", mac_address, IM_ARRAYSIZE(mac_address), ImGuiInputTextFlags_CallbackEdit, mac_address_callback)) {
            sscanf(mac_address, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
                   &iris->mac_address[0], &iris->mac_address[1], &iris->mac_address[2],
                   &iris->mac_address[3], &iris->mac_address[4], &iris->mac_address[5]);

            ps2::set_mac_address(iris->ps2, iris->mac_address);
        }

        mac_editing = IsItemActive();

        PopFont();

        SameLine();

        if (Button(ICON_MS_REFRESH "##macaddress")) {
            static std::mt19937 rng(std::random_device{}() ^ (unsigned)(GetTime() * 1e6));

            std::uniform_int_distribution<int> byte(0, 255);

            for (int i = 0; i < 6; i++)
                iris->mac_address[i] = (uint8_t)byte(rng);

            // Locally administered, unicast
            iris->mac_address[0] = (iris->mac_address[0] & 0xFC) | 0x02;

            ps2::set_mac_address(iris->ps2, iris->mac_address);
        }

        ip_input("Network",    "##network", iris->slirp_config.network);
        ip_input("Netmask",    "##netmask", iris->slirp_config.netmask);
        ip_input("Gateway",    "##gateway", iris->slirp_config.gateway);
        ip_input("DHCP start", "##dhcp_start", iris->slirp_config.dhcp_start);
        ip_input("DNS server", "##nameserver", iris->slirp_config.nameserver);

        EndTable();
    }

    BeginDisabled(!valid);

    if (Button("Apply##slirp")) {
        slirp::restart(iris->ps2->speed->smap, iris->slirp_config, &iris->log.slirp);
    } SameLine();

    EndDisabled();

    if (Button("Restore defaults##slirp")) {
        iris->slirp_config.network    = "10.0.2.0";
        iris->slirp_config.netmask    = "255.255.255.0";
        iris->slirp_config.gateway    = "10.0.2.2";
        iris->slirp_config.dhcp_start = "10.0.2.15";
        iris->slirp_config.nameserver = "10.0.2.3";

        slirp::restart(iris->ps2->speed->smap, iris->slirp_config, &iris->log.slirp);
    }
    
    EndDisabled();

    imgui::section(iris, "Logging");

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Log to system console", &iris->log_to_console);

    if (Checkbox("Log to file", &iris->log_to_file))
        log_apply_settings(iris);

    PopStyleVar();

    BeginDisabled(!iris->log_to_file);

    Text("Log file");

    SetNextItemWidth(400.0);

    if (InputText("##logpath", &iris->paths.log_path, ImGuiInputTextFlags_EnterReturnsTrue))
        log_apply_settings(iris);

    SameLine();

    if (Button(ICON_MS_MORE_HORIZ "##logpath")) {
        audio::mute(iris);

        auto f = pfd::save_file("Select log file", iris->paths.log_path, {
            "Log files (*.log)", "*.log",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (!f.result().empty()) {
            iris->paths.log_path = f.result();

            log_apply_settings(iris);
        }
    }

    EndDisabled();

    imgui::section(iris, "Misc.");

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Start games automatically", &iris->autostart);
    Checkbox("Skip FMVs", &iris->skip_fmv);
    Checkbox("Keep arcade files extracted from archives", &iris->cache_arcade_files);
    SetItemTooltip("Loads archived arcade games faster, at the cost of keeping a second copy of their files");
    Checkbox("Fastboot System 246/256 games", &iris->arcade_dongle_boot);
    SetItemTooltip("Runs the game's own boot program on the dongle even when a boot.elf is there to chainload it. Games without a boot.elf take this route regardless");

    imgui::section(iris, "DIP Switches");

    Text("System 246/256 (SW1)");

    bool dip_changed = false;

    dip_changed |= Checkbox("RGB level 0.7Vp-p", &iris->system_2x6_rgb_level);
    dip_changed |= Checkbox("31kHz output", &iris->system_2x6_monitor_frequency);
    dip_changed |= Checkbox("Separate sync", &iris->system_2x6_video_sync);

    if (dip_changed) {
        settings::apply_arcade_dip_switches(iris);
    }

    PopStyleVar();
}

static const char* ssaa_names[] = {
    "Disabled",
    "2x",
    "4x",
    "8x",
    "16x"
};

void show_hardware_renderer_settings(Instance* iris) {
    using namespace ImGui;

    Text("SSAA");

    if (BeginCombo("##ssaa", ssaa_names[iris->hardware_backend_config.super_sampling])) {
        for (int i = 0; i < IM_ARRAYSIZE(ssaa_names); i++) {
            if (imgui::Selectable(ssaa_names[i], iris->hardware_backend_config.super_sampling == i)) {
                iris->hardware_backend_config.super_sampling = i;

                if (i != 0) {
                    iris->hardware_backend_config.force_progressive = true;
                }

                render::refresh(iris);
            }
        }

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    BeginDisabled(iris->hardware_backend_config.super_sampling == 0);

    if (Checkbox(" Smooth UI/sprites", &iris->hardware_backend_config.super_sampled_quads)) {
        render::refresh(iris);
    }

    // if (IsItemHovered()) {
    //     SetTooltip("Super-sample textured 2D sprites instead of snapping them to the native\n"
    //                "pixel grid, so upscaled UI is smoothed rather than point-scaled.\n"
    //                "May cause minor texture-atlas bleeding in some games.");
    // }

    EndDisabled();

    // BeginDisabled(iris->hardware_backend_config.super_sampling != 0);
    if (Checkbox(" Force progressive scan", &iris->hardware_backend_config.force_progressive)) {
        render::refresh(iris);
    }
    // EndDisabled();

    if (Checkbox(" Overscan", &iris->hardware_backend_config.overscan)) {
        render::refresh(iris);
    }
    PopStyleVar();

    imgui::section(iris, "Analog Video");
    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    if (Checkbox(" Enable", &iris->hardware_backend_config.enable_analog_video)) {
        render::refresh(iris);
    }
    PopStyleVar();

    Text("Video standard");

    static const char* video_standard_names[] = {
        "NTSC",
        "PAL"
    };

    if (BeginCombo("##videostandard", video_standard_names[iris->hardware_backend_config.analog_system])) {
        for (int i = 0; i < IM_ARRAYSIZE(video_standard_names); i++) {
            if (imgui::Selectable(video_standard_names[i], iris->hardware_backend_config.analog_system == i)) {
                iris->hardware_backend_config.analog_system = i;
                render::refresh(iris);
            }
        }
        EndCombo();
    }

    Text("Cable type");

    static const char* cable_type_names[] = {
        "Composite",
        "S-Video",
        "Component"
    };

    if (BeginCombo("##cabletype", cable_type_names[iris->hardware_backend_config.analog_cable])) {
        for (int i = 0; i < IM_ARRAYSIZE(cable_type_names); i++) {
            if (imgui::Selectable(cable_type_names[i], iris->hardware_backend_config.analog_cable == i)) {
                iris->hardware_backend_config.analog_cable = i;
                render::refresh(iris);
            }
        }
        EndCombo();
    }

    Text("Decode filter");

    static const char* decode_filter_names[] = {
        "Notch",
        "3-line Comb",
        "3-line Comb + Notch"
    };

    int decode_filter_index = 0;

    if (iris->hardware_backend_config.line_comb) {
        decode_filter_index = 1;

        if (!iris->hardware_backend_config.skip_notch) {
            decode_filter_index = 2;
        }
    } else {
        decode_filter_index = 0;
    }

    if (BeginCombo("##decodefilter", decode_filter_names[decode_filter_index])) {
        for (int i = 0; i < IM_ARRAYSIZE(decode_filter_names); i++) {
            if (imgui::Selectable(decode_filter_names[i], decode_filter_index == i)) {
                switch (i) {
                    case 0: {
                        iris->hardware_backend_config.line_comb = false;
                        iris->hardware_backend_config.skip_notch = false;
                    } break;

                    case 1: {
                        iris->hardware_backend_config.line_comb = true;
                        iris->hardware_backend_config.skip_notch = true;
                    } break;

                    case 2: {
                        iris->hardware_backend_config.line_comb = true;
                        iris->hardware_backend_config.skip_notch = false;
                    } break;
                }

                render::refresh(iris);
            }
        }

        EndCombo();
    }

    imgui::section(iris, "Advanced");

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    if (Checkbox(" CRTC Offsets", &iris->hardware_backend_config.crtc_offsets)) {
        render::refresh(iris);
    }

    if (Checkbox(" Disable Mipmaps", &iris->hardware_backend_config.disable_mipmaps)) {
        render::refresh(iris);
    }

    if (Checkbox(" Unsynced Readbacks", &iris->hardware_backend_config.unsynced_readbacks)) {
        render::refresh(iris);
    }

    if (Checkbox(" Backbuffer Promotion", &iris->hardware_backend_config.backbuffer_promotion)) {
        render::refresh(iris);
    }

    if (Checkbox(" Allow Blend Demote", &iris->hardware_backend_config.allow_blend_demote)) {
        render::refresh(iris);
    }

    if (Checkbox(" Invert Fields", &iris->hardware_backend_config.invert_fields)) {
        render::refresh(iris);
    }
    PopStyleVar();
}

void show_graphics_settings(Instance* iris) {
    using namespace ImGui;

    static const char* settings_renderer_names[] = {
        "Null",
        "Software",
        "Hardware"
    };

    Text("Renderer");

    if (BeginCombo("##renderer", settings_renderer_names[iris->renderer_backend], ImGuiComboFlags_HeightSmall)) {
        for (int i = 0; i < 3; i++) {
            BeginDisabled(i == gs::renderer::BACKEND_SOFTWARE);

            if (imgui::Selectable(settings_renderer_names[i], i == iris->renderer_backend)) {
                render::switch_backend(iris, i);
            }

            EndDisabled();
        }

        EndCombo();
    }

    Text("Aspect mode");

    if (BeginCombo("##aspectmode", settings_aspect_mode_names[iris->aspect_mode])) {
        for (int i = 0; i < 7; i++) {
            if (imgui::Selectable(settings_aspect_mode_names[i], iris->aspect_mode == i)) {
                iris->aspect_mode = i;
            }
        }

        EndCombo();
    }

    BeginDisabled(
        iris->aspect_mode == render::AUTO ||
        iris->aspect_mode == render::STRETCH ||
        iris->aspect_mode == render::STRETCH_KEEP
    );

    Text("Scale");

    char buf[16]; snprintf(buf, 16, "%.1fx", (float)iris->scale);

    if (BeginCombo("##scale", buf, ImGuiComboFlags_HeightSmall)) {
        for (int i = 2; i <= 6; i++) {
            snprintf(buf, 16, "%.1fx", (float)i * 0.5f);

            if (imgui::Selectable(buf, ((float)i * 0.5f) == iris->scale)) {
                iris->scale = (float)i * 0.5f;
            }
        }

        EndCombo();
    }

    EndDisabled();

    Text("Scaling");

    const char* filter_names[] = {
        "Nearest",
        "Bilinear",
        "Bilinear (FSR)",
        "Cubic"
    };

    if (BeginCombo("##scalingfilter", filter_names[iris->filter])) {
        for (int i = 0; i < 4; i++) {
            BeginDisabled(i == render::CUBIC && !iris->vk.cubic_supported);
            if (imgui::Selectable(filter_names[i], iris->filter == i)) {
                iris->filter = i;
            }
            EndDisabled();
        }

        EndCombo();
    }

    const int normalized_angle = ((iris->angle % 360) + 360) % 360;
    const int rotation_index = normalized_angle / 90;

    Text("Rotation");

    if (BeginCombo("##rotation", settings_rotation_names[rotation_index])) {
        for (int i = 0; i < 4; i++) {
            if (imgui::Selectable(settings_rotation_names[i], rotation_index == i)) {
                iris->angle = i * 90;
            }
        }

        EndCombo();
    }

    Text("Window mode");

    if (BeginCombo("##windowmode", settings_fullscreen_names[iris->fullscreen])) {
        for (int i = 0; i < 2; i++) {
            if (imgui::Selectable(settings_fullscreen_names[i], iris->fullscreen == i)) {
                iris->fullscreen = i;

                SDL_SetWindowFullscreen(iris->window, settings_fullscreen_flags[i]);
            }
        }

        EndCombo();
    }

    imgui::section(iris, "Misc.");

    Text("Present mode");

    if (BeginCombo("##presentmode", settings_present_mode_names[iris->present_mode])) {
        for (int i = 0; i < IM_ARRAYSIZE(settings_present_mode_names); i++) {
            if (imgui::Selectable(settings_present_mode_names[i], iris->present_mode == i)) {
                iris->present_mode = i;
            }
        }

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox(" Integer scaling", &iris->integer_scaling);
    Checkbox(" Flip horizontally", &iris->flip_x);
    Checkbox(" Flip vertically", &iris->flip_y);
    Checkbox(" Remember window size", &iris->remember_window_size);

    if (Checkbox(" No decorations", &iris->no_decorations)) {
        SDL_SetWindowSize(iris->window, iris->window_width, iris->window_height + get_menubar_height(iris));
    }

    SetItemTooltip("Hides the menu bar and status bar, and paints the background black (F10)");

    PopStyleVar();

    if (iris->renderer_backend == gs::renderer::BACKEND_HARDWARE) {
        imgui::section(iris, "Renderer settings");

        show_hardware_renderer_settings(iris);
    }

    imgui::section(iris, "Vulkan settings");

    Text("GPU");

    static bool changed = false;
    const char* hint;
    const auto& selected_device = iris->vk.vulkan_gpus[iris->vk.vulkan_selected_device_index];

    if (iris->vk.vulkan_physical_device < 0) {
        hint = "Auto";
    } else {
        hint = iris->vk.vulkan_gpus[iris->vk.vulkan_physical_device].name.c_str();
    }

    if (changed) {
        SameLine();
        TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Restart the emulator to apply these changes");
    }

    PushStyleVarY(ImGuiStyleVar_ItemSpacing, 5.0F);

    if (BeginCombo("##gpu", hint)) {
        if (imgui::Selectable("Auto", iris->vk.vulkan_physical_device < 0)) {
            iris->vk.vulkan_physical_device = -1;
        }

        for (int i = 0; i < iris->vk.vulkan_gpus.size(); i++) {
            const auto& device = iris->vk.vulkan_gpus[i];

            std::string name = device.name;

            if (device.device == selected_device.device) {
                name += " (Current)";
            }

            if (imgui::Selectable(name.c_str(), device.device == selected_device.device)) {
                changed = iris->vk.vulkan_physical_device != i;

                iris->vk.vulkan_physical_device = i;
            }
        }

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    if (Checkbox(" Enable validation layers", &iris->vk.vulkan_enable_validation_layers)) {
        changed = true;
    }
    PopStyleVar(2);
}

void show_controller_slot(Instance* iris, int slot) {
    using namespace ImGui;

    char label[9] = "Slot #";

    label[5] = '1' + slot;

    ImVec4 col = GetStyleColorVec4(iris->input.ds[slot] ? ImGuiCol_Text : ImGuiCol_TextDisabled);

    col.w = 1.0;

    if (BeginChild(label, ImVec2(GetContentRegionAvail().x / 2.0 - 10.0, 0), ImGuiChildFlags_AutoResizeY)) {
        Text("Controller");

        std::string controller_name = "None";

        if (iris->input.ds[slot]) {
            controller_name = "DualShock 2";
        }

        float avail_width = GetContentRegionAvail().x;

        SetNextItemWidth(avail_width);

        if (BeginCombo("##controller", controller_name.c_str())) {
            if (imgui::Selectable("None")) {
                if (iris->input.ds[slot]) {
                    sio2::detach_device(iris->ps2->sio2, slot);

                    iris->input.ds[slot] = nullptr;
                }
            }

            if (imgui::Selectable("DualShock 2")) {
                if (!iris->input.ds[slot]) {
                    iris->input.ds[slot] = dev::ds::attach(iris->logger, iris->ps2->sio2, slot);
                }
            }

            EndCombo();
        }
    } EndChild(); SameLine(0.0, 10.0);

    if (BeginChild((std::string(label) + "##icon").c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeY)) {
        BeginDisabled(!iris->input.ds[slot]);

        float avail_width = GetContentRegionAvail().x;

        Text("Input device");

        std::string name = "None";

        if (!iris->input.input_devices[slot]) {
            name = "None";
        } else if (iris->input.input_devices[slot]->get_type() == 0) {
            name = "Keyboard";
        } else if (iris->input.input_devices[slot]->get_type() == 1) {
            GamepadDevice* gp = static_cast<GamepadDevice*>(iris->input.input_devices[slot]);

            name = SDL_GetGamepadNameForID(gp->get_id());
        }

        SetNextItemWidth(avail_width);

        if (BeginCombo("##devicetype", name.c_str())) {
            if (imgui::Selectable("None")) {
                if (iris->input.input_devices[slot]) {
                    delete iris->input.input_devices[slot];

                    iris->input.input_devices[slot] = nullptr;
                }
            }

            if (imgui::Selectable("Keyboard")) {
                if (iris->input.input_devices[slot]) {
                    delete iris->input.input_devices[slot];

                    iris->input.input_devices[slot] = nullptr;
                }

                iris->input.input_devices[slot] = new KeyboardDevice();
                iris->input.input_devices[slot]->set_slot(slot);

                if (iris->input.input_map[slot] <= 1) {
                    iris->input.input_map[slot] = 0;
                }
            }

            for (auto gamepad : iris->input.gamepads) {
                if (imgui::Selectable(SDL_GetGamepadNameForID(gamepad.first))) {
                    if (iris->input.input_devices[slot]) {
                        delete iris->input.input_devices[slot];

                        iris->input.input_devices[slot] = nullptr;
                    }

                    iris->input.input_devices[slot] = new GamepadDevice(gamepad.first);
                    iris->input.input_devices[slot]->set_slot(slot);

                    if (iris->input.input_map[slot] <= 1) {
                        iris->input.input_map[slot] = 1;
                    }
                }
            }

            EndCombo();
        }

        EndDisabled();
    } EndChild();

    InvisibleButton("##slot0", ImVec2(10, 10));

    Texture* tex = &iris->ui.dualshock2_icon;

    float width = 250.0f;
    float height = (tex->height * width) / tex->width;

    SetCursorPosX((GetContentRegionAvail().x / 2.0) - (width / 2.0));

    Image(
        (ImTextureID)(intptr_t)tex->descriptor_set,
        ImVec2(width, height),
        ImVec2(0, 0), ImVec2(1, 1),
        col,
        ImVec4(0.0, 0.0, 0.0, 0.0)
    );

    InvisibleButton("##pad1", ImVec2(10, 10));

    Text("Mapping");

    SetNextItemWidth(GetContentRegionAvail().x / 2.0 - 10.0);

    Mapping* selected = get_input_mapping(iris, slot);

    if (BeginCombo("##mapping", selected ? selected->name.c_str() : "None")) {
        if (imgui::Selectable("None", selected == nullptr)) {
            iris->input.input_map[slot] = -1;
        }

        int i = 0;

        for (auto& map : iris->input.input_maps) {
            if (imgui::Selectable(map.name.c_str(), selected == &map)) {
                iris->input.input_map[slot] = i;
            }

            i++;
        }

        EndCombo();
    }
}

bool event_is_mod_key(const InputEvent& event) {
    if (event.type != EventType::KEYBOARD) {
        return false;
    }

    SDL_Keycode keycode = static_cast<SDL_Keycode>(event.id);

    return (keycode & 0xf0000fff) == SDLK_LSHIFT ||
           (keycode & 0xf0000fff) == SDLK_RSHIFT ||
           (keycode & 0xf0000fff) == SDLK_LCTRL ||
           (keycode & 0xf0000fff) == SDLK_RCTRL ||
           (keycode & 0xf0000fff) == SDLK_LALT ||
           (keycode & 0xf0000fff) == SDLK_RALT;
}

static int selected_mapping = 0;
static bool waiting_for_input = false;
static uint64_t mapping_editing = 0;

void show_mappings_editor(Instance* iris) {
    using namespace ImGui;

    Text("Game controller DB");

    SetNextItemWidth(300);

    if (imgui::text_input("##gcdbinput", &iris->paths.gcdb_path, "Not configured (using default)")) {
        if (iris->paths.gcdb_path.size()) {
            // To-do: Check return value
            input::load_db_from_file(iris, iris->paths.gcdb_path.c_str());
        } else {
            input::load_db_default(iris);
        }
    }

    SameLine();

    if (Button(ICON_MS_FOLDER "##gcdbbtn")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select Game controller DB file", "", {
            "Game controller DB (*.txt)", "*.txt",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            iris->paths.gcdb_path = f.result().at(0);

            // To-do: Check return value
            input::load_db_from_file(iris, iris->paths.gcdb_path.c_str());
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##gcdbclear")) {
        iris->paths.gcdb_path = "";

        input::load_db_default(iris);
    }

    Text("Mapping");

    if (BeginCombo("##mapping", iris->input.input_maps[selected_mapping].name.c_str())) {
        int i = 0;

        for (auto& map : iris->input.input_maps) {
            if (imgui::Selectable(map.name.c_str(), selected_mapping == i)) {
                selected_mapping = i;
            }

            i++;
        }

        EndCombo();
    } SameLine();

    if (selected_mapping <= 1) {
        if (Button(ICON_MS_REFRESH " Default")) {
            input::init_default_mapping(iris, selected_mapping);
        }
    }

    SetNextItemWidth(GetContentRegionAvail().x);

    if (BeginTable("##mappingeditor", 2, ImGuiTableFlags_SizingStretchProp)) {
        TableSetupColumn("Input");
        TableSetupColumn("Mapping");

        std::vector<std::pair<uint64_t, InputAction>> elems(
            iris->input.input_maps[selected_mapping].map.forward_map().begin(),
            iris->input.input_maps[selected_mapping].map.forward_map().end());

        std::sort(elems.begin(), elems.end(), [](const std::pair<uint64_t, InputAction>& a, const std::pair<uint64_t, InputAction>& b) {
            return a.second < b.second;
        });

        for (auto& entry : elems) {
            TableNextRow();

            std::string key_name = get_input_name(static_cast<InputAction>(entry.second));

            TableSetColumnIndex(0);
            AlignTextToFramePadding();
            Text("%s", key_name.c_str());

            TableSetColumnIndex(1);

            InputEvent event;
            event.u64 = entry.first;

            std::string value_name = get_event_name(event) + "##" + key_name;

            if (waiting_for_input && (mapping_editing == entry.first)) {
                PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled));

                if (Button("Press a key or button...", ImVec2(GetContentRegionAvail().x, 0))) {
                    waiting_for_input = false;
                }

                PopStyleColor();

                if (iris->input.last_input_event_read == false && iris->input.last_input_event_value > 0.5f && !event_is_mod_key(iris->input.last_input_event)) {
                    iris->input.last_input_event_read = true;

                    waiting_for_input = false;
                    mapping_editing = 0;

                    auto event = iris->input.last_input_event;
                    auto action = entry.second;

                    // printf("Mapping input event %s (%llu) to action %s (%llu)\n",
                    //     get_event_name(iris->input.last_input_event).c_str(),
                    //     iris->input.last_input_event.u64,
                    //     get_input_name(action),
                    //     static_cast<uint64_t>(entry.second)
                    // );

                    auto* value_ptr = iris->input.input_maps[selected_mapping].map.get_value(event.u64);

                    if (value_ptr != nullptr) {
                        // Remove previous mapping for this input event
                        auto value = *value_ptr;
                        auto key = *iris->input.input_maps[selected_mapping].map.get_key(action);

                        // printf("Removing previous mapping of event %s (%llu) to action %s (%llu)\n",
                        //     get_event_name(event).c_str(),
                        //     event.u64,
                        //     get_input_name(value),
                        //     static_cast<uint64_t>(value)
                        // );

                        iris->input.input_maps[selected_mapping].map.erase_by_key(event.u64);
                        iris->input.input_maps[selected_mapping].map.erase_by_value(action);
                        iris->input.input_maps[selected_mapping].map.insert(event.u64, action);
                        iris->input.input_maps[selected_mapping].map.insert(key, value);
                    } else {
                        iris->input.input_maps[selected_mapping].map.erase_by_value(action);
                        iris->input.input_maps[selected_mapping].map.insert(event.u64, action);
                    }
                }
            } else {
                if (Button(value_name.c_str(), ImVec2(GetContentRegionAvail().x, 0))) {
                    iris->input.last_input_event_read = true;
                    waiting_for_input = true;
                    mapping_editing = entry.first;
                }
            }

            // if (IsMouseDoubleClicked(ImGuiMouseButton_Left) && IsItemHovered()) {
            //     iris->input.last_input_event_read = true;
            //     waiting_for_input = true;
            //     mapping_editing = entry.first;
            // }
        }

        EndTable();
    }
}

void show_input_settings(Instance* iris) {
    using namespace ImGui;

    static const char* const tabs[] = { "Slot 1", "Slot 2", "Mappings" };
    static int tab = 0;

    imgui::segmented("##inputtabs", &tab, tabs, IM_ARRAYSIZE(tabs), segment_width(tabs, IM_ARRAYSIZE(tabs)));

    Spacing();

    switch (tab) {
        case 0: show_controller_slot(iris, 0); break;
        case 1: show_controller_slot(iris, 1); break;
        case 2: show_mappings_editor(iris); break;
    }
}

void show_usb_port(Instance* iris, int port) {
    using namespace ImGui;

    int current = iris->input.usb_devices[port];
    const char* current_name = usb::device_type_name(current);

    Text("Device");

    SetNextItemWidth(300.0);

    if (BeginCombo("##usbdevice", current_name ? current_name : "None")) {
        for (int i = 0; i < usb::USB_DEVICE_TYPE_COUNT; i++) {
            const char* name = usb::device_type_name(i);

            if (imgui::Selectable(name, i == current)) {
                iris->input.usb_devices[port] = i;

                usb::set_port_device(iris->ps2->usb, port, i);
            }
        }

        EndCombo();
    }

    if (iris->input.usb_devices[port] == usb::USB_DEVICE_MSD) {
        std::string& path = iris->input.usb_msd_paths[port];

        Separator();
        Text("Image");

        float spacing = GetStyle().ItemSpacing.x;
        float width = 300.0f;

        ImVec2 size((float)(int)((width - spacing * 3.0f) / 4.0f), 0.0f);
        ImVec2 last(width - (size.x + spacing) * 3.0f, 0.0f);

        SetNextItemWidth(width);

        if (imgui::text_input("##msdimage", &path, "No image (drive empty)"))
            usb::msd_set_image(iris->ps2->usb, port, path.size() ? path.c_str() : nullptr);

        if (Button(ICON_MS_FOLDER "##msdimage", size)) {
            audio::mute(iris);

            auto f = pfd::open_file("Select USB drive image", "", {
                "Disk images (*.img; *.bin; *.iso; *.raw)", "*.img *.bin *.iso *.raw",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                path = f.result().at(0);

                usb::msd_set_image(iris->ps2->usb, port, path.c_str());
            }
        }

        SetItemTooltip("Select an existing drive image");

        SameLine();

        if (Button(ICON_MS_NOTE_ADD "##msdcreate", size))
            iris->applets.media_tool.open_for_slot(MEDIA_USB_DRIVE, port);

        SetItemTooltip("Create a new drive image");

        SameLine();

        BeginDisabled(path.empty());

        if (Button(ICON_MS_CLEAR "##msdclear", size)) {
            path = "";

            usb::msd_set_image(iris->ps2->usb, port, nullptr);
        }

        EndDisabled();

        SetItemTooltip("Eject this drive image");

        SameLine();

        BeginDisabled(path.empty());

        if (Button(ICON_MS_FOLDER_OPEN "##msdbrowse", last))
            browse_device(iris, FE_DEV_USB, port);

        EndDisabled();

        SetItemTooltip("Browse the files on this drive image");
    }
}

void show_usb_settings(Instance* iris) {
    using namespace ImGui;

    static const char* const tabs[] = { "Port 1", "Port 2" };
    static int tab = 0;

    imgui::segmented("##usbtabs", &tab, tabs, IM_ARRAYSIZE(tabs), segment_width(tabs, IM_ARRAYSIZE(tabs)));

    Spacing();

    show_usb_port(iris, tab);
}

void show_paths_settings(Instance* iris) {
    using namespace ImGui;

    SettingsWindow& settings = iris->applets.settings;

    std::string& buf = settings.bios_buf;
    std::string& dvd_buf = settings.rom1_buf;
    std::string& rom2_buf = settings.rom2_buf;
    std::string& nvram_buf = settings.nvram_buf;
    std::string& hdd_buf = settings.hdd_buf;
    std::string& flash_buf = settings.flash_buf;

    Text("BIOS (rom0)");

    if (IsItemHovered()) {
        hovered = true;

        tooltip = ICON_MS_INFO " Select a BIOS file, this is required for the emulator to function properly";
    }

    SetNextItemWidth(300);

    imgui::text_input("##rom0", &buf, "e.g. scph10000.bin");
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom0")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select BIOS file", "", {
            "BIOS dumps (*.bin; *.rom0)", "*.bin *.rom0",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            buf = f.result().at(0);
        }
    }

    if (BeginTable("##rom-info", 2, ImGuiTableFlags_SizingFixedFit)) {
        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("Model" " ");
        TableSetColumnIndex(1);
        Text("%s", iris->ps2->rom0_info.model);

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("Version" " ");
        TableSetColumnIndex(1);
        Text("%s", iris->ps2->rom0_info.version);

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("Region" " ");
        TableSetColumnIndex(1);
        Text("%s", iris->ps2->rom0_info.region);

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("MD5 hash" " ");
        TableSetColumnIndex(1);
        Text("%s", iris->ps2->rom0_info.md5); SameLine();
        if (SmallButton(ICON_MS_CONTENT_COPY)) {
            SDL_SetClipboardText(iris->ps2->rom0_info.md5);
        }

        EndTable();
    }

    Separator();

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Auto-detect", &iris->paths.auto_paths);
    PopStyleVar();

    BeginDisabled(iris->paths.auto_paths);

    Text("DVD Player (rom1)");

    SetNextItemWidth(300);

    imgui::text_input("##rom1", &dvd_buf, "Not configured");
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom1")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select DVD BIOS file", "", {
            "DVD BIOS dumps (*.bin; *.rom1)", "*.bin *.rom1",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            dvd_buf = f.result().at(0);

            ps2::load_rom1(iris->ps2, dvd_buf.c_str());
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##rom1")) {
        iris->paths.rom1_path = "";

        dvd_buf.clear();
    }

    if (iris->paths.rom1_path.size()) {
        if (BeginTable("##rom1-info", 2, ImGuiTableFlags_SizingFixedFit)) {
            TableNextRow();
            TableSetColumnIndex(0);
            TextDisabled("Version" " ");
            TableSetColumnIndex(1);
            Text("%s", iris->ps2->rom1_info.version);

            TableNextRow();
            TableSetColumnIndex(0);
            TextDisabled("MD5 hash" " ");
            TableSetColumnIndex(1);
            Text("%s", iris->ps2->rom1_info.md5); SameLine();
            if (SmallButton(ICON_MS_CONTENT_COPY)) {
                SDL_SetClipboardText(iris->ps2->rom1_info.md5);
            }

            EndTable();
        }

        Separator();
    }

    Text("Chinese extensions (rom2)");

    SetNextItemWidth(300);

    imgui::text_input("##rom2", &rom2_buf, "Not configured");
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom2")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select ROM2 file", "", {
            "ROM2 dumps (*.bin; *.rom2)", "*.bin *.rom2",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            rom2_buf = f.result().at(0);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##rom2")) {
        iris->paths.rom2_path = "";

        rom2_buf.clear();
    } 

    Text("EEPROM memory (nvram)");

    SetNextItemWidth(300);

    imgui::text_input("##nvram", &nvram_buf, "Not configured");
    SameLine();

    if (Button(ICON_MS_FOLDER "##nvram")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select NVRAM file", "", {
            "NVRAM dumps (*.nvm; *.bin)", "*.nvm *.bin",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            nvram_buf = f.result().at(0);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##nvram")) {
        iris->paths.nvram_path = "";

        nvram_buf.clear();
    }

    EndDisabled();

    Separator();

    Text("Hard Disk Drive (hdd0)");

    SetNextItemWidth(300);
    imgui::text_input("##hdd", &hdd_buf, "Not configured");
    SameLine();

    if (Button(ICON_MS_FOLDER "##hdd")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select HDD image file", "", {
            "HDD images (*.isif; *.raw; *.bin)", "*.isif *.raw *.bin",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            hdd_buf = f.result().at(0);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##hdd")) {
        iris->paths.hdd_path = "";

        hdd_buf.clear();
    }

    SameLine();

    if (Button(ICON_MS_FOLDER_OPEN "##hddbrowse"))
        browse_device(iris, FE_DEV_HDD, 0);

    SetItemTooltip("Browse files on the HDD");

    Text("Flash memory (xfrom)");

    SetNextItemWidth(300);

    imgui::text_input("##flash", &flash_buf, "Not configured");
    SameLine();

    if (Button(ICON_MS_FOLDER "##flash")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select Flash/XFROM dump file", "", {
            "XFROM dumps (*.bin)", "*.bin",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            flash_buf = f.result().at(0);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##xfrom")) {
        iris->paths.flash_path = "";

        flash_buf.clear();
    }

    SameLine();

    BeginDisabled(!iris->paths.flash_path.size());

    if (Button(ICON_MS_FOLDER_OPEN "##xfrombrowse"))
        browse_device(iris, FE_DEV_XFROM, 0);

    EndDisabled();

    SetItemTooltip("Browse files on the internal flash");

    imgui::section(iris, "Arcade");

    for (int i = 0; i < emu::ARCADE_BIOS_COUNT; i++) {
        std::string& arcade_buf = settings.arcade_bios_bufs[i];

        PushID(i);

        Text("%s", emu::get_arcade_bios_label(i));

        SetNextItemWidth(300);

        imgui::text_input("##arcadebios", &arcade_buf, "No board BIOS selected");

        SameLine();

        if (Button(ICON_MS_FOLDER "##arcadebios")) {
            audio::mute(iris);

            auto f = pfd::open_file("Select board BIOS file", "", {
                "Bootrom dumps (*.bin; *.ic1; *.7d; *.8g)", "*.bin *.ic1 *.7d *.8g",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size())
                arcade_buf = f.result().at(0);
        } SameLine();

        if (Button(ICON_MS_CLOSE "##arcadebios"))
            arcade_buf.clear();

        PopID();
    }

    if (Button(ICON_MS_SAVE " Save")) {
        std::string bios_path = buf;
        std::string rom1_path = dvd_buf;
        std::string rom2_path = rom2_buf;
        std::string flash_path = flash_buf;
        std::string nvram_path = nvram_buf;
        std::string hdd_path = hdd_buf;

        std::vector<std::string> arcade_bios_paths(settings.arcade_bios_bufs, settings.arcade_bios_bufs + emu::ARCADE_BIOS_COUNT);

        request_reset(
            "System paths",
            "The BIOS and ROM images are read when the system boots, so the\n"
            "running emulator keeps using the current ones until it resets.",
            [=]() {
                if (bios_path.size()) iris->paths.bios_path = bios_path;
                if (rom1_path.size()) iris->paths.rom1_path = rom1_path;
                if (rom2_path.size()) iris->paths.rom2_path = rom2_path;
                if (flash_path.size()) iris->paths.flash_path = flash_path;
                if (nvram_path.size()) iris->paths.nvram_path = nvram_path;
                if (hdd_path.size()) iris->paths.hdd_path = hdd_path;

                for (int i = 0; i < emu::ARCADE_BIOS_COUNT; i++)
                    iris->paths.arcade_bios_paths[i] = arcade_bios_paths[i];
            },
            [=]() {
                if (iris->paths.bios_path.size())
                    ps2::load_bios(iris->ps2, iris->paths.bios_path.c_str());

                emu::load_rom_files(iris);
            },
            [iris]() {
                iris->applets.settings.sync_paths();
            }
        );
    } SameLine();

    if (settings.paths_dirty()) {
        TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_FIBER_MANUAL_RECORD " Unsaved changes");
    } else if (reset_prompt.deferred) {
        TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_SCHEDULE " Saved, applies on next start");
    }
}

void SettingsWindow::sync_paths() {
    bios_buf = iris->paths.bios_path;
    rom1_buf = iris->paths.rom1_path;
    rom2_buf = iris->paths.rom2_path;
    nvram_buf = iris->paths.nvram_path;
    hdd_buf = iris->paths.hdd_path;
    flash_buf = iris->paths.flash_path;

    for (int i = 0; i < emu::ARCADE_BIOS_COUNT; i++)
        arcade_bios_bufs[i] = iris->paths.arcade_bios_paths[i];
}

bool SettingsWindow::paths_dirty() const {
    for (int i = 0; i < emu::ARCADE_BIOS_COUNT; i++)
        if (iris->paths.arcade_bios_paths[i] != arcade_bios_bufs[i])
            return true;

    return iris->paths.bios_path != bios_buf ||
           iris->paths.rom1_path != rom1_buf ||
           iris->paths.rom2_path != rom2_buf ||
           iris->paths.nvram_path != nvram_buf ||
           iris->paths.hdd_path != hdd_buf ||
           iris->paths.flash_path != flash_buf;
}

void SettingsWindow::on_open() {
    sync_paths();

    reset_prompt.deferred = false;
}

static int format_slot = -1;
static bool format_pending = false;

static void draw_format_prompt(Instance* iris) {
    using namespace ImGui;

    if (format_slot == -1)
        return;

    if (format_pending) {
        OpenPopup("###formatprompt");

        format_pending = false;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    if (GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable && !GetIO().ConfigViewportsNoDecoration)
        flags |= ImGuiWindowFlags_NoTitleBar;

    ImGuiWindowClass window_class;

    window_class.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;

    SetNextWindowClass(&window_class);
    SetNextWindowPos(GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!BeginPopupModal("Format memory card###formatprompt", nullptr, flags))
        return;

    const std::string& path = format_slot ? iris->paths.mcd1_path : iris->paths.mcd0_path;

    PushFont(iris->ui.font_heading);
    Text("Slot %d", format_slot + 1);
    PopFont();

    Spacing();

    TextUnformatted(path.c_str());

    Spacing();
    TextDisabled(ICON_MS_WARNING " Every save on this card will be lost.");
    Spacing();
    Separator();
    Spacing();

    if (Button("Format")) {
        if (emu::format_memory_card(iris, format_slot)) {
            push_info(iris, "Memory card formatted successfully.");
        } else {
            push_info(iris, "Failed to format memory card.");
        }

        format_slot = -1;

        CloseCurrentPopup();
    } SameLine();

    if (Button("Cancel")) {
        format_slot = -1;

        CloseCurrentPopup();
    }

    EndPopup();
}

static const char* get_memory_card_type_name(int type) {
    switch (type) {
        case 0: return "None";
        case 1: return "PS2 Memory Card";
        case 2: return "PS1 Memory Card";
        case 3: return "PocketStation";
        default: return "Unknown";
    }

    return "Unknown";
}

void show_memory_card(Instance* iris, int slot) {
    using namespace ImGui;

    char label[9] = "##mcard0";

    label[7] = '0' + slot;

    if (BeginChild(label, ImVec2(0, 0))) {
        std::string& path = slot ? iris->paths.mcd1_path : iris->paths.mcd0_path;

        ImVec4 col = GetStyleColorVec4(iris->input.mcd_slot_type[slot] ? ImGuiCol_Text : ImGuiCol_TextDisabled);

        col.w = 1.0;

        InvisibleButton("##pad0", ImVec2(10, 10));

        Texture* tex = &iris->ui.ps2_memory_card_icon;

        if (iris->input.mcd_slot_type[slot] == 2) {
            tex = &iris->ui.ps1_memory_card_icon;
        } else if (iris->input.mcd_slot_type[slot] == 3) {
            tex = &iris->ui.pocketstation_icon;
        }

        SetCursorPosX((GetContentRegionAvail().x / 2.0) - (tex->width / 2.0));

        Image(
            (ImTextureID)(intptr_t)tex->descriptor_set,
            ImVec2(tex->width, tex->height),
            ImVec2(0, 0), ImVec2(1, 1),
            col,
            ImVec4(0.0, 0.0, 0.0, 0.0)
        );

        InvisibleButton("##pad1", ImVec2(10, 10));

        if (path.size() && !iris->input.mcd_slot_type[slot]) {
            TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Check file");

            if (IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                if (BeginTooltip()) {
                    Text("Please check files ");
    
                    EndTooltip();
                }
            }
        }

        PushFont(iris->ui.font_heading);
        Text("%s", get_memory_card_type_name(iris->input.mcd_slot_type[slot]));
        PopFont();

        char it_label[7] = "##mcd0";
        char bt_label[10] = ICON_MS_FOLDER "##mcd0";
        char cr_label[15] = ICON_MS_NOTE_ADD "##mcdcr0";
        char br_label[15] = ICON_MS_FOLDER_OPEN "##mcdbr0";
        char fm_label[15] = ICON_MS_DELETE_SWEEP "##mcdfm0";
        char ed_label[10];

        snprintf(ed_label, 10, "%s##mcd0", iris->input.mcd_slot_type[slot] ? ICON_MS_ARROW_DOWNWARD : ICON_MS_ARROW_UPWARD);

        it_label[5] = '0' + slot;
        bt_label[8] = '0' + slot;
        cr_label[10] = '0' + slot;
        ed_label[8] = '0' + slot;
        br_label[10] = '0' + slot;
        fm_label[10] = '0' + slot;

        float spacing = GetStyle().ItemSpacing.x;
        float avail = GetContentRegionAvail().x;

        // Truncated so the row can never total more than the field above it and wrap.
        // The last button takes whatever that leaves over, landing the row on the same edge
        ImVec2 size((float)(int)((avail - spacing * 4.0f) / 5.0f), 0.0f);
        ImVec2 last(avail - (size.x + spacing) * 4.0f, 0.0f);

        SetNextItemWidth(avail);

        if (imgui::text_input(it_label, &path, "Not configured")) {
            if (iris->input.mcd_slot_type[slot])
                emu::attach_memory_card(iris, slot, path.c_str());
        }

        if (Button(bt_label, size)) {
            audio::mute(iris);

            auto f = pfd::open_file("Select Memory Card file for Slot 1", iris->paths.pref_path, {
                "Memory Card files (*.ps2; *.mcd; *.bin; *.psm; *.pocket)", "*.ps2 *.mcd *.bin *.psm *.pocket",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                path = f.result().at(0);

                emu::attach_memory_card(iris, slot, path.c_str());
            }
        }

        SetItemTooltip("Select an existing memory card image");

        SameLine();

        if (Button(cr_label, size))
            iris->applets.media_tool.open_for_slot(MEDIA_MEMORY_CARD, slot);

        SetItemTooltip("Create a new memory card image");

        SameLine();

        BeginDisabled((!iris->input.mcd_slot_type[slot]) && (!path.size()));

        if (Button(ed_label, size)) {
            if (iris->input.mcd_slot_type[slot]) {
                emu::detach_memory_card(iris, slot);
            } else {
                emu::attach_memory_card(iris, slot, path.c_str());
            }
        }

        EndDisabled();

        SetItemTooltip(iris->input.mcd_slot_type[slot] ? "Detach this memory card" : "Attach this memory card");

        SameLine();

        BeginDisabled((!iris->input.mcd_slot_type[slot]) && (!path.size()));

        if (Button(br_label, size))
            browse_device(iris, FE_DEV_MCD, slot);

        EndDisabled();

        SetItemTooltip("Browse the files on this memory card");

        SameLine();

        BeginDisabled(path.empty());

        if (Button(fm_label, last)) {
            format_slot = slot;
            format_pending = true;
        }

        EndDisabled();

        SetItemTooltip("Format this memory card, erasing every save on it");
    } EndChild();
}

void show_memory_card_settings(Instance* iris) {
    using namespace ImGui;

    static const char* const tabs[] = { "Slot 1", "Slot 2" };
    static int tab = 0;

    imgui::segmented("##mcardtabs", &tab, tabs, IM_ARRAYSIZE(tabs), segment_width(tabs, IM_ARRAYSIZE(tabs)));

    Spacing();

    show_memory_card(iris, tab);

    draw_format_prompt(iris);
}

static const char* const theme_names[] = {
    "Granite Neo",
    "ImGui Dark",
    "ImGui Light",
    "ImGui Classic",
    "Cherry",
    "Source",
    "Granite Neo Light",
    "Granite",
    "Nord",
    "Gruvbox",
    "Tokyo Night",
    "Catppuccin Mocha",
    "Catppuccin Latte",
    "Solarized Dark",
    "Sakura",
    "Sakura Light"
};

static_assert(IM_ARRAYSIZE(theme_names) == imgui::THEME_COUNT);

static const char* const codeview_color_scheme_names[] = {
    "Solarized Dark",
    "Solarized Light",
    "One Dark Pro",
    "Catppuccin Latte",
    "Catppuccin Frappé",
    "Catppuccin Macchiato",
    "Catppuccin Mocha"
};

#ifdef _WIN32
static const char* titlebar_style_names[] = {
    "Default",
    "Seamless"
};
#endif

void show_misc_settings(Instance* iris) {
    using namespace ImGui;

    imgui::section(iris, "Style");

    Text("Theme");

#define THEME(id)     if (imgui::Selectable(theme_names[id], iris->ui.theme == id)) {         iris->ui.theme = id;         imgui::set_theme(iris, id);         platform::apply_settings(iris);     }

    if (BeginCombo("##theme", theme_names[iris->ui.theme])) {
        PushFont(iris->ui.font_small);
        TextDisabled("Dark");
        PopFont();

        THEME(imgui::GRANITE_NEO);
        THEME(imgui::GRANITE);
        THEME(imgui::NORD);
        THEME(imgui::GRUVBOX);
        THEME(imgui::TOKYO_NIGHT);
        THEME(imgui::MOCHA);
        THEME(imgui::SOLARIZED);
        THEME(imgui::SAKURA);
        THEME(imgui::IMGUI_DARK);
        THEME(imgui::IMGUI_CLASSIC);
        THEME(imgui::CHERRY);
        THEME(imgui::SOURCE);

        PushFont(iris->ui.font_small);
        TextDisabled("Light");
        PopFont();

        THEME(imgui::GRANITE_NEO_LIGHT);
        THEME(imgui::LATTE);
        THEME(imgui::SAKURA_LIGHT);
        THEME(imgui::IMGUI_LIGHT);

        EndCombo();
    }

#undef THEME

    Text("Background color");

    ColorEdit3("##bgcolor", (float*)&iris->vk.clear_value.color);

    Text("UI scale");

    DragFloat("##uiscale", &iris->ui.ui_scale, 0.05f, 0.5f, 1.5f, "%.1f");

    GetStyle().FontScaleMain = iris->ui.ui_scale;

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Enable viewports", &iris->ui.imgui_enable_viewports); SameLine();
    PopStyleVar();

    TextDisabled(ICON_MS_WARNING " requires restart");

#ifdef _WIN32
    Text("Titlebar style (Windows only)");

    if (BeginCombo("##titlebar_style", titlebar_style_names[iris->windows_titlebar_style])) {
        for (int i = 0; i < 2; i++) {
            if (imgui::Selectable(titlebar_style_names[i], iris->windows_titlebar_style == i)) {
                iris->windows_titlebar_style = i;

                platform::apply_settings(iris);
            }
        }

        EndCombo();
    }
#endif

#ifdef IRIS_HAS_DARK_TITLEBAR
    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);

#ifdef _WIN32
    BeginDisabled(iris->windows_titlebar_style != IRIS_TITLEBAR_DEFAULT);
#endif

    if (Checkbox(" Dark titlebar", &iris->dark_titlebar)) {
        platform::apply_settings(iris);
    }

#ifdef _WIN32
    EndDisabled();

    if (Checkbox(" Show window borders", &iris->windows_enable_borders)) {
        platform::apply_settings(iris);
    }
#endif

    PopStyleVar();
#endif

    imgui::section(iris, "Codeview");

#define SCHEME(str, id) \
    if (Selectable(str, iris->ui.codeview_color_scheme == id)) { \
        iris->ui.codeview_color_scheme = id; \
        imgui::set_codeview_scheme(iris, id); \
    }

    Text("Color scheme");

    if (BeginCombo("##codeview_color_scheme", codeview_color_scheme_names[iris->ui.codeview_color_scheme])) {
        PushFont(iris->ui.font_small);
        TextDisabled("Dark");
        PopFont();

        SCHEME("Solarized Dark", imgui::CodeviewColorScheme::SOLARIZED_DARK);
        SCHEME("One Dark Pro", imgui::CodeviewColorScheme::ONE_DARK_PRO);
        SCHEME("Catppuccin Mocha", imgui::CodeviewColorScheme::CATPPUCCIN_MOCHA);
        SCHEME("Catppuccin Macchiato", imgui::CodeviewColorScheme::CATPPUCCIN_MACCHIATO);
        SCHEME("Catppuccin Frappé", imgui::CodeviewColorScheme::CATPPUCCIN_FRAPPE);

        PushFont(iris->ui.font_small);
        TextDisabled("Light");
        PopFont();

        SCHEME("Solarized Light", imgui::CodeviewColorScheme::SOLARIZED_LIGHT);
        SCHEME("Catppuccin Latte", imgui::CodeviewColorScheme::CATPPUCCIN_LATTE);

        EndCombo();
    }

#undef SCHEME

    static bool use_theme_background = !iris->ui.codeview_use_theme_background;

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);

    if (Checkbox("Use scheme background", &use_theme_background))  {
        iris->ui.codeview_use_theme_background = !use_theme_background;
    }

    PopStyleVar();

    Text("Font scale");

    DragFloat("##codeview_font_scale", &iris->ui.codeview_font_scale, 0.05f, 0.75f, 1.5f, "%.1f");

    imgui::section(iris, "Screenshots");

    const char* format_names[] = {
        "PNG",
        "BMP",
        "JPG",
        "TGA"
    };

    const char* jpg_quality_names[] = {
        "Minimum", // 1
        "Low", // 25
        "Medium", // 50
        "High", // 90
        "Maximum", // 100
        "Custom..."
    };

    const char* mode_names[] = {
        "Internal",
        "Display"
    };

    Text("Format");

    if (BeginCombo("##screenshotformat", format_names[iris->screenshot_format])) {
        for (int i = 0; i < 4; i++) {
            if (imgui::Selectable(format_names[i], iris->screenshot_format == i)) {
                iris->screenshot_format = i;
            }
        }

        EndCombo();
    }

    Text("Resolution mode");

    if (BeginCombo("##screenshotmode", mode_names[iris->screenshot_mode])) {
        for (int i = 0; i < 2; i++) {
            if (imgui::Selectable(mode_names[i], iris->screenshot_mode == i)) {
                iris->screenshot_mode = i;
            }
        }

        EndCombo();
    }

    if (iris->screenshot_format == render::JPG) {
        Text("JPG Quality");

        if (BeginCombo("##jpgquality", jpg_quality_names[iris->screenshot_jpg_quality_mode])) {
            for (int i = 0; i < 6; i++) {
                if (imgui::Selectable(jpg_quality_names[i], iris->screenshot_jpg_quality_mode == i)) {
                    iris->screenshot_jpg_quality_mode = i;
                }
            }

            EndCombo();
        }

        if (iris->screenshot_jpg_quality_mode == render::CUSTOM) {
            SliderInt("Quality##jpgqualitycustom", &iris->screenshot_jpg_quality, 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
        }
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox(" Include shader processing", &iris->screenshot_shader_processing);
    PopStyleVar();

    imgui::section(iris, "MagicGate");

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);

    if (Checkbox(" Enable MagicGate", &iris->enable_magicgate)) {
        settings::apply_magicgate(iris);
    }

    PopStyleVar();

    Spacing();

    BeginDisabled(!iris->enable_magicgate);

    mg_key_input(iris, "Encrypted key store", "##mecha_eks", iris->paths.mecha_eks_path);
    mg_key_input(iris, "Key store key", "##mecha_kek", iris->paths.mecha_kek_path);
    mg_key_input(iris, "Card key store", "##mecha_cks", iris->paths.mecha_cks_path);
    mg_key_input(iris, "Challenge IV", "##mecha_civ", iris->paths.mecha_civ_path);
    mg_key_input(iris, "Arcade KELF kbit", "##mecha_kbit", iris->paths.mecha_kelf_kbit_path);
    mg_key_input(iris, "Arcade KELF kc", "##mecha_kc", iris->paths.mecha_kelf_kc_path);

    Spacing();

    if (Button("Apply##magicgate")) {
        settings::apply_mg_keys(iris);
    }

    EndDisabled();

    SameLine();

    AlignTextToFramePadding();

    if (iris->enable_magicgate && cdvd::mg_ready(iris->ps2->cdvd)) {
        imgui::badge(ICON_MS_CHECK "  Key store ready", ImVec4(0.42f, 0.85f, 0.1f, 1.0f));
    } else {
        imgui::badge(ICON_MS_INFO "  Using HLE authentication", ImVec4(0.90f, 0.73f, 0.2f, 1.0f));
    }
}

static const char* builtin_shader_names[] = {
    "iris-ntsc-encoder",
    "iris-ntsc-decoder",
    "iris-ntsc-curvature",
    "iris-ntsc-scanlines",
    "iris-ntsc-noise",
};

static const char* presets[] = {
    "NTSC codec",
    0
};

void show_shader_settings(Instance* iris) {
    using namespace ImGui;

    static const char* selected_shader = "";

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0f);
    Checkbox(" Enable shaders", &iris->enable_shaders);
    PopStyleVar();

    Separator();

    Text("Add shader");
    if (BeginCombo("##combo", selected_shader)) {
        for (int i = 0; i < IM_ARRAYSIZE(builtin_shader_names); i++) {
            if (imgui::Selectable(builtin_shader_names[i], selected_shader == builtin_shader_names[i])) {
                selected_shader = builtin_shader_names[i];
            }
        }

        EndCombo();
    } SameLine();

    if (Button(ICON_MS_ADD)) {
        if (selected_shader && selected_shader[0]) {
            std::string shader(selected_shader);

            shaders::push(iris, selected_shader);
        }
    } SameLine();

    if (Button(ICON_MS_REMOVE_SELECTION)) {
        shaders::clear(iris);
    }

    // Text("Preset");

    // if (BeginCombo("##presets", selected_shader)) {
    //     for (int i = 0; i < 3; i++) {
    //         if (imgui::Selectable(presets[i], selected_shader == builtin_shader_names[i])) {
    //             selected_shader = builtin_shader_names[i];
    //         }
    //     }

    //     EndCombo();
    // }

    if (BeginTable("##shaders", 1, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < shaders::count(iris); i++) {
            TableNextRow();

            char bypass[16];
            char del[16];
            char id[1024];

            sprintf(bypass, "%s##%d", shaders::at(iris, i)->bypass ? ICON_MS_CHECK_BOX_OUTLINE_BLANK : ICON_MS_CHECK_BOX, i);
            sprintf(del, ICON_MS_DELETE "##%d", i);
            sprintf(id, "%s##%d", shaders::at(iris, i)->get_id().c_str(), i);

            TableSetColumnIndex(0);
            if (SmallButton(del)) {
                iris->vk.shader_passes.erase(iris->vk.shader_passes.begin() + i);

                break;
            } SameLine();

            if (SmallButton(bypass)) {
                shaders::at(iris, i)->bypass = !shaders::at(iris, i)->bypass;
            } SameLine();

            Selectable(id, false, ImGuiSelectableFlags_SpanAllColumns);

            if (BeginDragDropSource()) {
                SetDragDropPayload("SHADER_DND_PAYLOAD", &i, sizeof(int));

                EndDragDropSource();
            }

            if (BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = AcceptDragDropPayload("SHADER_DND_PAYLOAD")) {
                    int src = *(int*)payload->Data;

                    shaders::swap(iris, src, i);
                }

                EndDragDropTarget();
            }
        }

        EndTable();
    }
}

void show_device_settings(Instance* iris) {
    using namespace ImGui;

    bool changed = false;

    imgui::section(iris, "host");

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);

    if (Checkbox("Use ELF directory as host", &iris->paths.host_from_elf)) {
        changed = true;
    }

    PopStyleVar();

    if (IsItemHovered()) {
        SetTooltip("Point host: at the folder of the loaded ELF, so relative file accesses resolve next to it");
    }

    BeginDisabled(iris->paths.host_from_elf);

    const char* host_hint = "Working directory";

    if (iris->paths.host_from_elf) {
        if (iris->paths.host_elf_dir.size()) {
            host_hint = iris->paths.host_elf_dir.c_str();
        } else {
            host_hint = "ELF folder";
        }
    }

    SetNextItemWidth(300);

    if (InputTextWithHint("##host-path", host_hint, &iris->paths.host_path))
        changed = true;

    SameLine();

    if (Button(ICON_MS_FOLDER "##host-pick")) {
        audio::mute(iris);

        auto f = pfd::select_folder("Select host folder", "", pfd::opt::none);

        while (!f.ready());

        audio::unmute(iris);

        std::string result = f.result();

        if (result.size()) {
            iris->paths.host_path = result;

            changed = true;
        }
    }

    EndDisabled();

    Spacing();

    imgui::section(iris, "Additional devices");

    int remove_index = -1;

    if (BeginTable("##device-maps", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH)) {
        TableSetupColumn("Device", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        TableSetupColumn("Host folder", ImGuiTableColumnFlags_WidthStretch);
        TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed);
        TableHeadersRow();

        for (int i = 0; i < (int)iris->paths.device_maps.size(); i++) {
            PushID(i);

            TableNextRow();

            TableSetColumnIndex(0);
            SetNextItemWidth(-1);
            if (InputTextWithHint("##device", "mass0", &iris->paths.device_maps[i].first))
                changed = true;

            TableSetColumnIndex(1);
            SetNextItemWidth(-1);

            const char* path_hint;

#ifdef _WIN32
            path_hint = "C:/foo/usb";
#else
            path_hint = "/home/user/foo/usb";
#endif
            if (InputTextWithHint("##host", path_hint, &iris->paths.device_maps[i].second))
                changed = true;

            TableSetColumnIndex(2);

            if (Button(ICON_MS_FOLDER "##pick")) {
                audio::mute(iris);

                auto f = pfd::select_folder("Select host folder", "", pfd::opt::none);

                while (!f.ready());

                audio::unmute(iris);

                std::string result = f.result();

                if (result.size()) {
                    iris->paths.device_maps[i].second = result;

                    changed = true;
                }
            }

            SameLine();

            if (Button(ICON_MS_DELETE "##del"))
                remove_index = i;

            PopID();
        }

        EndTable();
    }

    if (remove_index >= 0) {
        iris->paths.device_maps.erase(iris->paths.device_maps.begin() + remove_index);

        changed = true;
    }

    Spacing();

    if (Button(ICON_MS_ADD " Add mapping")) {
        iris->paths.device_maps.emplace_back("", "");

        changed = true;
    }

    if (changed)
        settings::apply_device_maps(iris);
}

bool SettingsWindow::begin() {
    using namespace ImGui;

    hovered = false;

    SetNextWindowSize(ImVec2(675, 500), ImGuiCond_FirstUseEver);
    PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(675, 500));

    if (GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable && !GetIO().ConfigViewportsNoDecoration)
        flags |= ImGuiWindowFlags_NoTitleBar;

    return Begin(title, &open, flags);
}

void SettingsWindow::end() {
    ImGui::End();
    ImGui::PopStyleVar();
}

void SettingsWindow::on_render() {
    using namespace ImGui;

    PushStyleVarX(ImGuiStyleVar_ButtonTextAlign, 0.0);
    PushStyleVarY(ImGuiStyleVar_ItemSpacing, 6.0);

    if (BeginChild("##sidebar", ImVec2(175, GetContentRegionAvail().y), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders)) {
        for (int i = 0; settings_buttons[i]; i++) {
            if (selected == i) PushStyleColor(ImGuiCol_Button, GetStyle().Colors[ImGuiCol_ButtonHovered]);

            bool pressed = Button(settings_buttons[i], ImVec2(175, 35));
            
            if (selected == i) PopStyleColor();

            if (pressed) {
                selected = i;
            }
        }
    } EndChild(); SameLine(0.0, 10.0);

    PopStyleVar(2);

    if (BeginChild("##content", ImVec2(0, GetContentRegionAvail().y), ImGuiChildFlags_AutoResizeY)) {
        switch (selected) {
            case 0: show_system_settings(iris); break;
            case 1: show_paths_settings(iris); break;
            case 2: show_graphics_settings(iris); break;
            case 3: show_shader_settings(iris); break;
            case 4: show_input_settings(iris); break;
            case 5: show_memory_card_settings(iris); break;
            case 6: show_usb_settings(iris); break;
            case 7: show_device_settings(iris); break;
            case 8: show_misc_settings(iris); break;
        }
    } EndChild();

    draw_reset_prompt(iris);
}

}