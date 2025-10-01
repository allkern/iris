#include <filesystem>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "pfd/pfd.h"

#include "ps2_elf.h"
#include "ps2_iso9660.h"

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
    "Software (Threaded)"
};

const char* fullscreen_names[] = {
    "Windowed",
    "Fullscreen"
};

int fullscreen_flags[] = {
    0,
    SDL_WINDOW_FULLSCREEN
};

void show_main_menubar(iris::instance* iris) {
    using namespace ImGui;

    PushFont(iris->font_icons);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 7.0));

    if (BeginMainMenuBar()) {
        ImVec2 size = GetWindowSize();

        iris->menubar_height = size.y;

        if (BeginMenu("Iris")) {
            if (MenuItem(ICON_MS_DRIVE_FILE_MOVE " Open...")) {
                int prev_mute = iris->mute;

                iris->mute = true;

                auto f = pfd::open_file("Select a file to load", "", {
                    "All File Types (*.iso; *.bin; *.cue; *.elf)", "*.iso *.bin *.cue *.elf",
                    "Disc Images (*.iso; *.bin; *.cue)", "*.iso *.bin *.cue",
                    "ELF Executables (*.elf)", "*.elf",
                    "All Files (*.*)", "*"
                });

                while (!f.ready());

                iris->mute = prev_mute;

                if (f.result().size()) {
                    open_file(iris, f.result().at(0));
                    add_recent(iris, f.result().at(0));
                }
            }

            if (BeginMenu(ICON_MS_HISTORY " Open Recent", iris->recents.size())) {
                for (const std::string& s : iris->recents) {
                    if (MenuItem(s.c_str())) {
                        open_file(iris, s);
                        add_recent(iris, s);
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

            // if (MenuItem(ICON_MS_DRIVE_FILE_MOVE " Load disc...")) {
            //     const char* patterns[3] = { "*.iso", "*.bin", "*.cue" };

            //     const char* file = tinyfd_openFileDialog(
            //         "Select CD/DVD image",
            //         "",
            //         3,
            //         patterns,
            //         "Disc images",
            //         0
            //     );

            //     if (file) {
            //         struct iso9660_state* iso = iso9660_open(file);

            //         if (!iso) {
            //             printf("iris: Couldn't open disc image \"%s\"\n", file);

            //             exit(1);

            //             return;
            //         }

            //         char* boot_file = iso9660_get_boot_path(iso);

            //         if (!boot_file)
            //             return;

            //         // Temporarily disable window updates
            //         struct gs_callback cb = *ps2_gs_get_callback(iris->ps2->gs, GS_EVENT_VBLANK);

            //         ps2_gs_remove_callback(iris->ps2->gs, GS_EVENT_VBLANK);
            //         ps2_boot_file(iris->ps2, boot_file);

            //         // Re-enable window updates
            //         ps2_gs_init_callback(iris->ps2->gs, GS_EVENT_VBLANK, cb.func, cb.udata);

            //         ps2_cdvd_open(iris->ps2->cdvd, file);

            //         iso9660_close(iso);

            //         iris->loaded = file;
            //     }
            // }

            // if (MenuItem(ICON_MS_DRAFT " Load executable...")) {
            //     const char* patterns[3] = { "*.elf" };

            //     const char* file = tinyfd_openFileDialog(
            //         "Select ELF executable",
            //         "",
            //         1,
            //         patterns,
            //         "ELF executables",
            //         0
            //     );

            //     if (file) {
            //         std::string str(file);

            //         str = "host:" + str;

            //         // Temporarily disable window updates
            //         struct gs_callback cb = *ps2_gs_get_callback(iris->ps2->gs, GS_EVENT_VBLANK);

            //         ps2_gs_remove_callback(iris->ps2->gs, GS_EVENT_VBLANK);

            //         ps2_boot_file(iris->ps2, str.c_str());

            //         // ps2_elf_load(iris->ps2, file);

            //         // Re-enable window updates
            //         ps2_gs_init_callback(iris->ps2->gs, GS_EVENT_VBLANK, cb.func, cb.udata);

            //         iris->loaded = file;
            //     }
            // }

            Separator();

            if (MenuItem(iris->pause ? ICON_MS_PLAY_ARROW " Run" : ICON_MS_PAUSE " Pause", "Space")) {
                iris->pause = !iris->pause;
            }

            // To-do: Show confirm dialog maybe?
            if (MenuItem(ICON_MS_REFRESH " Reset")) {
                ps2_reset(iris->ps2);
            }

            if (MenuItem(ICON_MS_FOLDER " Change disc...")) {
                bool mute = iris->mute;

                iris->mute = true;

                auto f = pfd::open_file("Select CD/DVD image", "", {
                    "Disc Images (*.iso; *.bin; *.cue)", "*.iso *.bin *.cue",
                    "All Files (*.*)", "*"
                });

                while (!f.ready());

                iris->mute = mute;

                if (f.result().size()) {
                    // 2-second delay to allow the disc to spin up
                    if (!ps2_cdvd_open(iris->ps2->cdvd, f.result().at(0).c_str(), 38860800*2)) {
                        iris->loaded = f.result().at(0);
                    }
                }
            }

            if (MenuItem(ICON_MS_EJECT " Eject disc")) {
                iris->loaded = "";

                ps2_cdvd_close(iris->ps2->cdvd);
            }

            ImGui::EndMenu();
        }
        if (BeginMenu("Settings")) {
            if (BeginMenu(ICON_MS_MONITOR " Display")) {
                if (BeginMenu(ICON_MS_BRUSH " Renderer")) {
                    for (int i = 0; i < 3; i++) {
                        if (Selectable(renderer_names[i], i == iris->renderer_backend)) {
                            iris->renderer_backend = i;

                            renderer_init_backend(iris->ctx, iris->ps2->gs, iris->window, iris->device, i);
                            renderer_set_scale(iris->ctx, iris->scale);
                            renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
                            renderer_set_bilinear(iris->ctx, iris->bilinear);
                            renderer_set_integer_scaling(iris->ctx, iris->integer_scaling);
                            renderer_set_size(iris->ctx, 0, 0);
                        }
                    }

                    ImGui::EndMenu();
                }

                if (BeginMenu(ICON_MS_CROP " Scale")) {
                    for (int i = 2; i <= 6; i++) {
                        char buf[16]; snprintf(buf, 16, "%.1fx", (float)i * 0.5f);

                        if (Selectable(buf, ((float)i * 0.5f) == iris->scale)) {
                            iris->scale = (float)i * 0.5f;

                            renderer_set_scale(iris->ctx, iris->scale);
                        }
                    }

                    ImGui::EndMenu();
                }

                if (BeginMenu(ICON_MS_ASPECT_RATIO " Aspect mode")) {
                    for (int i = 0; i < 7; i++) {
                        if (Selectable(aspect_mode_names[i], iris->aspect_mode == i)) {
                            iris->aspect_mode = i;

                            renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
                        }
                    }

                    ImGui::EndMenu();
                }

                if (BeginMenu(ICON_MS_FILTER " Scaling filter")) {
                    if (Selectable("Nearest", !iris->bilinear)) {
                        iris->bilinear = false;

                        renderer_set_bilinear(iris->ctx, false);
                    }

                    if (Selectable("Bilinear", iris->bilinear)) {
                        iris->bilinear = true;

                        renderer_set_bilinear(iris->ctx, true);
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
                        "1280x800"
                    };

                    int widths[] = {
                        640, 800, 960, 1024, 1280, 1280
                    };

                    int heights[] = {
                        480, 600, 720, 768, 720, 800
                    };

                    for (int i = 0; i < 6; i++) {
                        bool selected = iris->window_width == widths[i] && iris->window_height == heights[i];

                        if (MenuItem(sizes[i], nullptr, selected)) {
                            iris->window_width = widths[i];
                            iris->window_height = heights[i];

                            SDL_SetWindowSize(iris->window, iris->window_width, iris->window_height);
                        }
                    }

                    ImGui::EndMenu();
                }

                if (MenuItem(ICON_MS_SPEED_2X " Integer scaling", nullptr, &iris->integer_scaling)) {
                    renderer_set_integer_scaling(iris->ctx, iris->integer_scaling);
                }

                if (MenuItem(ICON_MS_FULLSCREEN " Fullscreen", "F11", &iris->fullscreen)) {
                    SDL_SetWindowFullscreen(iris->window, iris->fullscreen);
                }

                ImGui::EndMenu();
            }

            if (BeginMenu(ICON_MS_MUSIC_NOTE " Audio")) {
                PushStyleVarY(ImGuiStyleVar_FramePadding, 0.0f);
                AlignTextToFramePadding();

                const char* icon = ICON_MS_VOLUME_UP;

                if (iris->volume == 0.0f) {
                    icon = ICON_MS_VOLUME_MUTE;
                } else if (iris->volume <= 0.5f) {
                    icon = ICON_MS_VOLUME_DOWN;
                }

                Text(icon); SameLine();
                
                SetNextItemWidth(100.0f);
                SliderFloat("Volume", &iris->volume, 0.0f, 1.0f, "%.1f");
                PopStyleVar();
 
                MenuItem(ICON_MS_VOLUME_OFF " Mute", nullptr, &iris->mute);

                ImGui::EndMenu();
            }

            MenuItem(ICON_MS_DOCK_TO_BOTTOM " Show status bar", nullptr, &iris->show_status_bar);

            if (MenuItem(ICON_MS_CONTENT_COPY " Copy data path to clipboard")) {
                SDL_SetClipboardText(iris->pref_path.c_str());
            }

            Separator();

            if (MenuItem(ICON_MS_MANUFACTURING " Settings...")) {
                iris->show_settings = true;
            }

            ImGui::EndMenu();
        }
        if (BeginMenu("Tools")) {
            if (MenuItem(ICON_MS_BUILD " ImGui Demo", NULL, &iris->show_imgui_demo));
            if (MenuItem(ICON_MS_PHOTO_CAMERA " Take screenshot...", "F9")) {
                bool mute = iris->mute;

                iris->mute = true;

                std::string filename = get_default_screenshot_filename(iris);

                auto f = pfd::save_file("Save screenshot", filename, {
                    "PNG (*.png)", "*.png",
                    "All Files (*.*)", "*"
                });

                while (!f.ready());

                iris->mute = mute;

                if (f.result().size()) {
                    save_screenshot(iris, f.result());
                }
            }

            ImGui::EndMenu();
        }
        if (BeginMenu("Debug")) {
            SeparatorText("EE");
            // if (BeginMenu(ICON_MS_BUG_REPORT " EE")) {
                if (MenuItem(ICON_MS_SETTINGS " Control##ee", NULL, &iris->show_ee_control));
                if (MenuItem(ICON_MS_EDIT_NOTE " State##ee", NULL, &iris->show_ee_state));
                if (MenuItem(ICON_MS_TERMINAL " Logs##ee", NULL, &iris->show_ee_logs));
                if (MenuItem(ICON_MS_BOLT " Interrupts##ee", NULL, &iris->show_ee_interrupts));

                BeginDisabled(iris->symbols.empty());
                if (MenuItem(ICON_MS_CODE " Symbols##ee", NULL, &iris->show_symbols));
                EndDisabled();

                if (MenuItem(ICON_MS_ACCOUNT_TREE " Threads##ee", NULL, &iris->show_threads));

                // ImGui::EndMenu();
            // }

            SeparatorText("IOP");
            // if (BeginMenu(ICON_MS_BUG_REPORT " IOP")) {
                if (MenuItem(ICON_MS_SETTINGS " Control##iop", NULL, &iris->show_iop_control));
                if (MenuItem(ICON_MS_EDIT_NOTE " State##iop", NULL, &iris->show_iop_state));
                if (MenuItem(ICON_MS_TERMINAL " Logs##iop", NULL, &iris->show_iop_logs));
                if (MenuItem(ICON_MS_BOLT " Interrupts##iop", NULL, &iris->show_iop_interrupts));
                if (MenuItem(ICON_MS_EXTENSION " Modules##iop", NULL, &iris->show_iop_modules));

            //     ImGui::EndMenu();
            // }

            Separator();

            if (MenuItem(ICON_MS_BUG_REPORT " Breakpoints", NULL, &iris->show_breakpoints));
            if (MenuItem(ICON_MS_BRUSH " GS debugger", NULL, &iris->show_gs_debugger));
            if (MenuItem(ICON_MS_MUSIC_NOTE " SPU2 debugger", NULL, &iris->show_spu2_debugger));
            if (MenuItem(ICON_MS_MEMORY " Memory viewer", NULL, &iris->show_memory_viewer));
            if (MenuItem(ICON_MS_VIEW_IN_AR " VU disassembler", NULL, &iris->show_vu_disassembler));
            if (MenuItem(ICON_MS_GAMEPAD " DualShock debugger", NULL, &iris->show_pad_debugger));
            if (MenuItem(ICON_MS_BUG_REPORT " Performance overlay", NULL, &iris->show_overlay));

            Separator();

            if (BeginMenu(ICON_MS_MORE_TIME " Timescale")) {
                for (int i = 0; i < 9; i++) {
                    char buf[16]; snprintf(buf, 16, "%dx", 1 << i);

                    if (Selectable(buf, iris->timescale == (1 << i))) {
                        iris->timescale = (1 << i);

                        ps2_set_timescale(iris->ps2, iris->timescale);
                    }
                }

                ImGui::EndMenu();
            }

            if (MenuItem(ICON_MS_SKIP_NEXT " Skip FMVs", NULL, &iris->skip_fmv)) {
                printf("Skip FMVs: %d\n", iris->skip_fmv);
                ee_set_fmv_skip(iris->ps2->ee, iris->skip_fmv);
            }

            if (MenuItem(ICON_MS_CLOSE " Close all")) {
                iris->show_ee_control = false;
                iris->show_ee_state = false;
                iris->show_ee_logs = false;
                iris->show_ee_interrupts = false;
                iris->show_ee_dmac = false;
                iris->show_iop_control = false;
                iris->show_iop_state = false;
                iris->show_iop_logs = false;
                iris->show_iop_interrupts = false;
                iris->show_iop_modules = false;
                iris->show_iop_dma = false;
                iris->show_gs_debugger = false;
                iris->show_spu2_debugger = false;
                iris->show_memory_viewer = false;
                iris->show_vu_disassembler = false;
                iris->show_pad_debugger = false;
                iris->show_symbols = false;
                iris->show_threads = false;
                iris->show_breakpoints = false;
                iris->show_overlay = false;
            }
            
            ImGui::EndMenu();
        }
        if (BeginMenu("Help")) {
            if (MenuItem(ICON_MS_INFO " About")) {
                iris->show_about_window = true;
            }

            ImGui::EndMenu();
        }

        EndMainMenuBar();
    }

    PopStyleVar();
    PopFont();
}

}
