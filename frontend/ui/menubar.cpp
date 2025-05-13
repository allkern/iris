#include <filesystem>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "tfd/tinyfiledialogs.h"
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

void add_recent(iris::instance* iris, std::string file) {
    auto it = std::find(iris->recents.begin(), iris->recents.end(), file);

    if (it != iris->recents.end()) {
        iris->recents.erase(it);
        iris->recents.push_front(file);

        return;
    }

    iris->recents.push_front(file);

    if (iris->recents.size() == 11)
        iris->recents.pop_back();
}

int open_file(iris::instance* iris, std::string file) {
    std::filesystem::path path(file);
    std::string ext = path.extension().string();

    for (char& c : ext)
        c = tolower(c);

    // Load disc image
    if (ext == ".iso" || ext == ".bin" || ext == ".cue") {
        if (ps2_cdvd_open(iris->ps2->cdvd, file.c_str()))
            return 1;

        char* boot_file = disc_get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file)
            return 2;

        // Temporarily disable window updates
        struct gs_callback cb = *ps2_gs_get_callback(iris->ps2->gs, GS_EVENT_VBLANK);

        ps2_gs_remove_callback(iris->ps2->gs, GS_EVENT_VBLANK);
        ps2_boot_file(iris->ps2, boot_file);

        // Re-enable window updates
        ps2_gs_init_callback(iris->ps2->gs, GS_EVENT_VBLANK, cb.func, cb.udata);

        iris->loaded = file;

        return 0;
    }

    // Note: We need the trailing whitespaces here because of IOMAN HLE
    // Load executable
    file = "host:  " + file;

    // Temporarily disable window updates
    struct gs_callback cb = *ps2_gs_get_callback(iris->ps2->gs, GS_EVENT_VBLANK);

    ps2_gs_remove_callback(iris->ps2->gs, GS_EVENT_VBLANK);

    ps2_boot_file(iris->ps2, file.c_str());

    // ps2_elf_load(iris->ps2, file);

    // Re-enable window updates
    ps2_gs_init_callback(iris->ps2->gs, GS_EVENT_VBLANK, cb.func, cb.udata);

    iris->loaded = file;

    return 0;
}

void show_main_menubar(iris::instance* iris) {
    using namespace ImGui;

    PushFont(iris->font_icons);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 7.0));

    if (BeginMainMenuBar()) {
        ImVec2 size = GetWindowSize();

        iris->menubar_height = size.y;

        if (BeginMenu("Iris")) {
            if (MenuItem(ICON_MS_DRIVE_FILE_MOVE " Open...")) {
                iris->mute = true;

                auto f = pfd::open_file("Select a file to load", "", {
                    "All File Types (*.iso; *.bin; *.cue; *.elf)", "*.iso *.bin *.cue *.elf",
                    "Disc Images (*.iso; *.bin; *.cue)", "*.iso *.bin *.cue",
                    "ELF Executables (*.elf)", "*.elf",
                    "All Files (*.*)", "*"
                });

                iris->mute = false;

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
                iris->mute = true;

                auto f = pfd::open_file("Select CD/DVD image", "", {
                    "Disc Images (*.iso; *.bin; *.cue)", "*.iso *.bin *.cue",
                    "All Files (*.*)", "*"
                });

                iris->mute = false;

                if (f.result().size()) {
                    if (!ps2_cdvd_open(iris->ps2->cdvd, f.result().at(0).c_str())) {
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

                            renderer_init_backend(iris->ctx, iris->ps2->gs, iris->window, i);
                            renderer_set_scale(iris->ctx, iris->scale);
                            renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
                            renderer_set_bilinear(iris->ctx, iris->bilinear);
                            renderer_set_integer_scaling(iris->ctx, iris->integer_scaling);
                            renderer_set_size(iris->ctx, 0, 0);
                        }
                    }

                    ImGui::EndMenu();
                }

                if (BeginMenu("Scale")) {
                    for (int i = 2; i <= 6; i++) {
                        char buf[16]; snprintf(buf, 16, "%.1fx", (float)i * 0.5f);

                        if (Selectable(buf, ((float)i * 0.5f) == iris->scale)) {
                            iris->scale = (float)i * 0.5f;

                            renderer_set_scale(iris->ctx, iris->scale);
                        }
                    }

                    ImGui::EndMenu();
                }

                if (BeginMenu("Aspect mode")) {
                    for (int i = 0; i < 7; i++) {
                        if (Selectable(aspect_mode_names[i], iris->aspect_mode == i)) {
                            iris->aspect_mode = i;

                            renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
                        }
                    }

                    ImGui::EndMenu();
                }

                if (BeginMenu("Scaling filter")) {
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

                if (MenuItem("Integer scaling", nullptr, &iris->integer_scaling)) {
                    renderer_set_integer_scaling(iris->ctx, iris->integer_scaling);
                }

                if (MenuItem("Fullscreen", "F11", &iris->fullscreen)) {
                    SDL_SetWindowFullscreen(iris->window, iris->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                }

                ImGui::EndMenu();
            }

            MenuItem(ICON_MS_DOCK_TO_BOTTOM " Show status bar", nullptr, &iris->show_status_bar);

            if (MenuItem(ICON_MS_CONTENT_COPY " Copy data path to clipboard")) {
                SDL_SetClipboardText(iris->pref_path.c_str());
            }


            ImGui::EndMenu();
        }
        if (BeginMenu("Tools")) {
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " ImGui Demo", NULL, &iris->show_imgui_demo));

            ImGui::EndMenu();
        }
        if (BeginMenu("Debug")) {
            SeparatorText("EE");
            // if (BeginMenu(ICON_MS_BUG_REPORT " EE")) {
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Control##ee", NULL, &iris->show_ee_control));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " State##ee", NULL, &iris->show_ee_state));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Logs##ee", NULL, &iris->show_ee_logs));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Interrupts##ee", NULL, &iris->show_ee_interrupts));

                // ImGui::EndMenu();
            // }

            SeparatorText("IOP");
            // if (BeginMenu(ICON_MS_BUG_REPORT " IOP")) {
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Control##iop", NULL, &iris->show_iop_control));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " State##iop", NULL, &iris->show_iop_state));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Logs##iop", NULL, &iris->show_iop_logs));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Interrupts##iop", NULL, &iris->show_iop_interrupts));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Modules##iop", NULL, &iris->show_iop_modules));

            //     ImGui::EndMenu();
            // }

            Separator();

            if (MenuItem(ICON_MS_LINE_START_CIRCLE " Breakpoints", NULL, &iris->show_breakpoints));
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " GS debugger", NULL, &iris->show_gs_debugger));
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " SPU2 debugger", NULL, &iris->show_spu2_debugger));
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " Memory viewer", NULL, &iris->show_memory_viewer));
            
            ImGui::EndMenu();
        }
        if (BeginMenu("Help")) {
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " About")) {
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
