#include <filesystem>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"

#include "ps2_elf.hpp"
#include "ps2_iso9660.hpp"
#include "ps2.hpp"

namespace iris {

const char* aspect_mode_names[] = {
    "Native",
    "Stretch",
    "Stretch (Keep aspect ratio)",
    "Force 4:3 (NTSC)",
    "Force 16:9 (Widescreen)",
    "Force 5:4 (PAL)",
    "Auto"
};

const char* renderer_names[] = {
    "Null",
    "Software",
    "Hardware (Vulkan)"
};

const char* fullscreen_names[] = {
    "Windowed",
    "Fullscreen"
};

const char* rotation_names[] = {
    "0 degrees",
    "90 degrees",
    "180 degrees",
    "270 degrees"
};

int fullscreen_flags[] = {
    0,
    SDL_WINDOW_FULLSCREEN
};

static void show_recent_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu(ICON_MS_HISTORY " Open Recent", iris->recents.size())) {
        for (const auto& entry : iris->recents) {
            if (MenuItem(entry.path.c_str())) {
                if (entry.type == RecentType::PS2) {
                    if (emu::open_file(iris, entry.path)) {
                        push_info(iris, "Failed to open file: " + entry.path);
                    } else {
                        add_recent(iris, entry.path, entry.type);
                    }
                } else {
                    if (!emu::load_arcade(iris, entry.path)) {
                        push_info(iris, "Failed to boot arcade: " + entry.path);
                    } else {
                        add_recent(iris, entry.path, RecentType::ARCADE);
                    }
                }

                break;
            }
        }

        Separator();

        if (MenuItem(ICON_MS_DELETE_HISTORY " Clear all recents")) {
            iris->recents.clear();
        }

        // To-do: Use try_open_file
        // if (MenuItem("Clear invalid recents")) {
        //     iris->recents.clear();
        // }

        // To-do
        // if (MenuItem("Stop recents history")) {
        // }

        ImGui::EndMenu();
    }
}

static void show_iris_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu("Iris")) {
        if (MenuItem(ICON_MS_DRIVE_FILE_MOVE " Open...")) {
            audio::mute(iris);

            auto f = pfd::open_file("Select a file to load", "", {
                "All File Types (*.iso; *.bin; *.cue; *.chd; *.cso; *.zso; *.elf)", "*.iso *.bin *.cue *.chd *.cso *.zso *.elf",
                "Disc Images (*.iso; *.bin; *.cue; *.chd; *.cso; *.zso)", "*.iso *.bin *.cue *.chd *.cso *.zso",
                "CD Images (*.bin; *.cue; *.chd)", "*.bin *.cue *.chd",
                "DVD Images (*.iso; *.chd; *.cso; *.zso)", "*.iso *.chd *.cso *.zso",
                "ISO Files (*.iso)", "*.iso",
                "CUE Files (*.cue)", "*.cue",
                "BIN Files (*.bin)", "*.bin",
                "CHD Files (*.chd)", "*.chd",
                "CSO/ZSO Files (*.cso; *.zso)", "*.cso *.zso",
                "ELF Executables (*.elf)", "*.elf",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                std::string path = f.result().at(0);

                if (path.size()) {
                    if (emu::open_file(iris, path)) {
                        push_info(iris, "Failed to open file: " + path);
                    } else {
                        add_recent(iris, path, RecentType::PS2);
                    }
                }
            }
        }

        show_recent_menu(iris);

        if (MenuItem(ICON_MS_JOYSTICK " Open Arcade...")) {
            audio::mute(iris);

            auto f = pfd::select_folder("Select arcade game folder", "", pfd::opt::none);

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                std::string path = f.result();

                if (path.size()) {
                    if (!emu::load_arcade(iris, path)) {
                        push_info(iris, "Failed to boot arcade: " + path);
                    } else {
                        add_recent(iris, path, RecentType::ARCADE);
                    }
                }
            }
        }

        Separator();

        if (MenuItem(iris->debug.pause ? ICON_MS_PLAY_ARROW " Run" : ICON_MS_PAUSE " Pause", "Space")) {
            iris->debug.pause = !iris->debug.pause;
        }

        // To-do: Show confirm dialog maybe?
        if (MenuItem(ICON_MS_REFRESH " Reset")) {
            ps2::reset(iris->ps2);
        }

        if (MenuItem(ICON_MS_FOLDER " Change disc...")) {
            audio::mute(iris);

            auto f = pfd::open_file("Select CD/DVD image", "", {
                "Disc Images (*.iso; *.bin; *.cue; *.chd; *.cso; *.zso)", "*.iso *.bin *.cue *.chd *.cso *.zso",
                "CD Images (*.bin; *.cue; *.chd)", "*.bin *.cue *.chd",
                "DVD Images (*.iso; *.chd; *.cso; *.zso)", "*.iso *.chd *.cso *.zso",
                "ISO Files (*.iso)", "*.iso",
                "CUE Files (*.cue)", "*.cue",
                "BIN Files (*.bin)", "*.bin",
                "CHD Files (*.chd)", "*.chd",
                "CSO/ZSO Files (*.cso; *.zso)", "*.cso *.zso",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                // 2-second delay to allow the disc to spin up
                if (!cdvd::open(iris->ps2->cdvd, f.result().at(0).c_str(), 38860800*2)) {
                    iris->loaded = f.result().at(0);
                }
            }
        }

        if (MenuItem(ICON_MS_EJECT " Eject disc")) {
            iris->loaded = "";

            cdvd::close(iris->ps2->cdvd);
        }

        if (MenuItem(ICON_MS_CLOSE " Close")) {
            iris->debug.pause = true;
            iris->ui.show_gamelist = true;
        }

        ImGui::EndMenu();
    }
}

static void show_display_settings_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu(ICON_MS_MONITOR " Display")) {
        if (BeginMenu(ICON_MS_BRUSH " Renderer")) {
            for (int i = 0; i < 3; i++) {
                BeginDisabled(i == gs::renderer::BACKEND_SOFTWARE);

                if (MenuItem(renderer_names[i], nullptr, i == iris->renderer_backend)) {
                    render::switch_backend(iris, i);
                }

                EndDisabled();
            }

            ImGui::EndMenu();
        }

        if (BeginMenu(ICON_MS_CROP " Scale")) {
            for (int i = 2; i <= 6; i++) {
                char buf[16]; snprintf(buf, 16, "%.1fx", (float)i * 0.5f);

                if (MenuItem(buf, nullptr, ((float)i * 0.5f) == iris->scale)) {
                    iris->scale = (float)i * 0.5f;

                    // renderer_set_scale(iris->ctx, iris->scale);
                }
            }

            ImGui::EndMenu();
        }

        if (BeginMenu(ICON_MS_ASPECT_RATIO " Aspect mode")) {
            for (int i = 0; i < 7; i++) {
                if (MenuItem(aspect_mode_names[i], nullptr, iris->aspect_mode == i)) {
                    iris->aspect_mode = i;

                    // renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
                }
            }

            ImGui::EndMenu();
        }

        if (BeginMenu(ICON_MS_FILTER " Scaling filter")) {
            const char* filter_names[] = {
                "Nearest",
                "Bilinear",
                "Cubic"
            };

            for (int i = 0; i < 3; i++) {
                BeginDisabled(i == 2 && !iris->vk.cubic_supported);
                if (MenuItem(filter_names[i], nullptr, iris->filter == i)) {
                    iris->filter = i;
                }
                EndDisabled();
            }

            ImGui::EndMenu();
        }

        if (BeginMenu(ICON_MS_SCREEN_ROTATION " Rotation")) {
            const int normalized_angle = ((iris->angle % 360) + 360) % 360;
            const int rotation_index = normalized_angle / 90;

            for (int i = 0; i < 4; i++) {
                if (MenuItem(rotation_names[i], nullptr, rotation_index == i)) {
                    iris->angle = i * 90;
                }
            }

            ImGui::EndMenu();
        }

        if (BeginMenu(ICON_MS_ASPECT_RATIO " Window size")) {
            const char* sizes[] = {
                "640x480",
                "800x600",
                "960x720",
                "1024x768",
                "1280x720",
                "1280x960"
            };

            int widths[] = {
                640, 800, 960, 1024, 1280, 1280
            };

            int heights[] = {
                480, 600, 720, 768, 720, 960
            };

            for (int i = 0; i < 6; i++) {
                bool selected = iris->window_width == widths[i] && iris->window_height == heights[i];

                if (MenuItem(sizes[i], nullptr, selected)) {
                    iris->window_width = widths[i];
                    iris->window_height = heights[i];

                    SDL_SetWindowSize(iris->window, iris->window_width, iris->window_height + get_menubar_height(iris));
                }
            }

            ImGui::EndMenu();
        }

        if (BeginMenu(ICON_MS_SYNC " Present mode")) {
            const char* settings_present_mode_names[] = {
                "Limit to 30 FPS",
                "Limit to 60 FPS",
                "VSync",
                "Uncapped"
            };

            for (int i = 0; i < IM_ARRAYSIZE(settings_present_mode_names); i++) {
                if (MenuItem(settings_present_mode_names[i], nullptr, iris->present_mode == i)) {
                    iris->present_mode = i;

                    if (iris->present_mode == render::VSYNC) {
                        imgui::set_vsync(iris, true);
                    } else {
                        imgui::set_vsync(iris, false);
                    }

                    iris->vk.swapchain_rebuild = true;
                }
            }

            ImGui::EndMenu();
        }

        if (MenuItem(ICON_MS_SPEED_2X " Integer scaling", nullptr, &iris->integer_scaling)) {
            // renderer_set_integer_scaling(iris->ctx, iris->integer_scaling);
        }

        MenuItem(ICON_MS_FLIP " Flip horizontally", nullptr, &iris->flip_x);
        MenuItem(ICON_MS_FLIP " Flip vertically", nullptr, &iris->flip_y);

        if (MenuItem(ICON_MS_FULLSCREEN " Fullscreen", "F11", &iris->fullscreen)) {
            SDL_SetWindowFullscreen(iris->window, iris->fullscreen);
        }

        if (MenuItem(ICON_MS_IMAGE " Enable shaders", nullptr, &iris->enable_shaders)) {
            // renderer_set_shaders_enabled(iris->ctx, iris->enable_shaders);
        }

        ImGui::EndMenu();
    }
}

static void show_audio_settings_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu(ICON_MS_MUSIC_NOTE " Audio")) {
        PushStyleVarY(ImGuiStyleVar_FramePadding, 0.0f);
        AlignTextToFramePadding();

        const char* icon = ICON_MS_VOLUME_UP;

        if (iris->audio.volume == 0.0f) {
            icon = ICON_MS_VOLUME_MUTE;
        } else if (iris->audio.volume <= 0.5f) {
            icon = ICON_MS_VOLUME_DOWN;
        }

        Text("%s", icon); SameLine();
        
        SetNextItemWidth(100.0f);
        SliderFloat("Volume", &iris->audio.volume, 0.0f, 1.0f, "%.1f");
        PopStyleVar();
 
        MenuItem(ICON_MS_VOLUME_OFF " Mute", nullptr, &iris->audio.mute);
        MenuItem(ICON_MS_MUSIC_OFF " Mute ADMA", nullptr, &iris->audio.mute_adma);

        ImGui::EndMenu();
    }
}

static void show_settings_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu("Settings")) {
        show_display_settings_menu(iris);

        show_audio_settings_menu(iris);

        if (MenuItem(ICON_MS_DOCK_TO_BOTTOM " Show status bar", nullptr, &iris->ui.show_status_bar)) {
            SDL_SetWindowSize(iris->window, iris->window_width, iris->window_height + get_menubar_height(iris));
        }

        if (MenuItem(ICON_MS_OPEN_IN_NEW " Open data folder")) {
            SDL_OpenURL(iris->paths.pref_path.c_str());
        }

        Separator();

        if (MenuItem(ICON_MS_MANUFACTURING " Settings...")) {
            iris->applets.settings.show();
        }

        ImGui::EndMenu();
    }
}

static void show_tools_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu("Tools")) {
        MenuItem(ICON_MS_BUILD " ImGui Demo", NULL, &iris->ui.show_imgui_demo);
        MenuItem(ICON_MS_SEARCH " Memory search", NULL, &iris->applets.memory_search.open);
        if (MenuItem(ICON_MS_PHOTO_CAMERA " Take screenshot...", "F9")) {
            audio::mute(iris);

            std::string filename = input::get_default_screenshot_filename(iris);

            auto f = pfd::save_file("Save screenshot", filename, {
                "PNG (*.png)", "*.png",
                "JPG (*.jpg)", "*.jpg",
                "BMP (*.bmp)", "*.bmp",
                "TGA (*.tga)", "*.tga",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                input::save_screenshot(iris, f.result());
            }
        }

        if (MenuItem(ICON_MS_MOVIE " Dump GS frames...")) {
            iris->debug.gsdump_prev_pause = iris->debug.pause;
            iris->debug.pause = true;
            iris->applets.gs_dump_tool.show();
        }

        if (MenuItem(ICON_MS_SD_CARD " Memory Card tool")) {
            iris->applets.memory_card_tool.show();
        }

        if (MenuItem(ICON_MS_HARD_DRIVE " HDD tool")) {
            iris->applets.hdd_tool.show();
        }

        ImGui::EndMenu();
    }
}

static void show_timescale_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu(ICON_MS_MORE_TIME " Timescale")) {
        for (int i = 0; i < 9; i++) {
            char buf[16]; snprintf(buf, 16, "%dx", 1 << i);

            if (MenuItem(buf, nullptr, iris->timescale == (1 << i))) {
                iris->timescale = (1 << i);

                ps2::set_timescale(iris->ps2, iris->timescale);
            }
        }

        ImGui::EndMenu();
    }
}

static void show_debug_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu("Debug")) {
        SeparatorText("EE");
        // if (BeginMenu(ICON_MS_BUG_REPORT " EE")) {
            MenuItem(ICON_MS_SETTINGS " Control##ee", NULL, &iris->applets.ee_control.open);
            MenuItem(ICON_MS_EDIT_NOTE " State##ee", NULL, &iris->applets.ee_state.open);
            MenuItem(ICON_MS_TERMINAL " Logs##ee", NULL, &iris->applets.ee_logs.open);
            MenuItem(ICON_MS_BOLT " Interrupts##ee", NULL, &iris->applets.ee_interrupts.open);

            BeginDisabled(iris->debug.symbols.empty());
            MenuItem(ICON_MS_CODE " Symbols##ee", NULL, &iris->applets.symbols.open);
            EndDisabled();

            MenuItem(ICON_MS_ACCOUNT_TREE " Threads##ee", NULL, &iris->applets.ee_threads.open);

            // ImGui::EndMenu();
        // }

        SeparatorText("IOP");
        // if (BeginMenu(ICON_MS_BUG_REPORT " IOP")) {
            MenuItem(ICON_MS_SETTINGS " Control##iop", NULL, &iris->applets.iop_control.open);
            MenuItem(ICON_MS_EDIT_NOTE " State##iop", NULL, &iris->applets.iop_state.open);
            MenuItem(ICON_MS_TERMINAL " Logs##iop", NULL, &iris->applets.iop_logs.open);
            MenuItem(ICON_MS_BOLT " Interrupts##iop", NULL, &iris->applets.iop_interrupts.open);
            MenuItem(ICON_MS_EXTENSION " Modules##iop", NULL, &iris->applets.iop_modules.open);
            MenuItem(ICON_MS_ACCOUNT_TREE " Threads##iop", NULL, &iris->applets.iop_threads.open);

        //     ImGui::EndMenu();
        // }

        Separator();

        MenuItem(ICON_MS_BUG_REPORT " Breakpoints", NULL, &iris->applets.breakpoints.open);
        MenuItem(ICON_MS_BRUSH " GS debugger", NULL, &iris->applets.gs_debugger.open);
        MenuItem(ICON_MS_MUSIC_NOTE " SPU2 debugger", NULL, &iris->applets.spu2_debugger.open);
        MenuItem(ICON_MS_MEMORY " Memory viewer", NULL, &iris->applets.memory_viewer.open);
        MenuItem(ICON_MS_VIEW_IN_AR " VU disassembler", NULL, &iris->applets.vu_disassembler.open);
        MenuItem(ICON_MS_GAMEPAD " DualShock debugger", NULL, &iris->applets.pad_debugger.open);
        MenuItem(ICON_MS_TIMER " Timers", NULL, &iris->applets.timers.open);
        MenuItem(ICON_MS_BUG_REPORT " Performance overlay", NULL, &iris->ui.show_overlay);
        MenuItem(ICON_MS_TERMINAL " SYSMEM logs", NULL, &iris->applets.sysmem_logs.open);
        MenuItem(ICON_MS_LIST_ALT " Console", NULL, &iris->applets.console.open);

        Separator();

        show_timescale_menu(iris);

        if (MenuItem(ICON_MS_SKIP_NEXT " Skip FMVs", NULL, &iris->skip_fmv)) {
            iris_info(&iris->log.ui, "Skip FMVs: {}", iris->skip_fmv);
            ee::set_fmv_skip(iris->ps2->ee, iris->skip_fmv);
        }

        if (MenuItem(ICON_MS_CLOSE " Close all")) {
            iris->applets.ee_control.open = false;
            iris->applets.ee_state.open = false;
            iris->applets.ee_logs.open = false;
            iris->applets.ee_interrupts.open = false;
            iris->applets.ee_dmac.open = false;
            iris->applets.iop_control.open = false;
            iris->applets.iop_state.open = false;
            iris->applets.iop_logs.open = false;
            iris->applets.iop_interrupts.open = false;
            iris->applets.iop_modules.open = false;
            iris->applets.iop_dma.open = false;
            iris->applets.gs_debugger.open = false;
            iris->applets.spu2_debugger.open = false;
            iris->applets.memory_viewer.open = false;
            iris->applets.memory_search.open = false;
            iris->applets.vu_disassembler.open = false;
            iris->ui.show_status_bar = false;
            iris->applets.breakpoints.open = false;
            iris->applets.ee_threads.open = false;
            iris->applets.iop_threads.open = false;
            iris->applets.sysmem_logs.open = false;
            iris->ui.show_imgui_demo = false;
            iris->ui.show_overlay = false;
        }

        if (MenuItem("Gamelist")) {
            iris->ui.show_gamelist = true;
        }

        ImGui::EndMenu();
    }
}

static void show_help_menu(Instance* iris) {
    using namespace ImGui;

    if (BeginMenu("Help")) {
        if (MenuItem(ICON_MS_INFO " About")) {
            iris->applets.about.show();
        }

        if (MenuItem(ICON_MS_EXCLAMATION " Report an issue")) {
            SDL_OpenURL("https://github.com/allkern/iris/issues/new");
        }

        bool disc_loaded = iris->ps2 && iris->ps2->cdvd && iris->ps2->cdvd->disc;

        BeginDisabled(!disc_loaded);
        if (MenuItem(ICON_MS_FACT_CHECK " Report compatibility")) {
            iris->applets.compat_report.show();
            iris->debug.pause = true;
        }
        EndDisabled();

        ImGui::EndMenu();
    }
}

void show_main_menubar(Instance* iris) {
    using namespace ImGui;

    PushFont(iris->ui.font_icons);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 7.0));

    if (BeginMainMenuBar()) {

        show_iris_menu(iris);
        show_settings_menu(iris);
        show_tools_menu(iris);
        show_debug_menu(iris);
        show_help_menu(iris);

        EndMainMenuBar();
    }

    PopStyleVar();
    PopFont();
}

}
