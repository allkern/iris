#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"
#include "tfd/tinyfiledialogs.h"

#include "ps2_elf.h"

namespace lunar {

const char* aspect_mode_names[] = {
    "Native",
    "Stretch",
    "Stretch (Keep aspect ratio)",
    "Force 4:3",
    "Force 16:9",
    "Auto"
};

void show_main_menubar(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_icons);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 7.0));

    if (BeginMainMenuBar()) {
        ImVec2 size = GetWindowSize();

        lunar->menubar_height = size.y;

        if (BeginMenu("Lunar")) {
            if (MenuItem(ICON_MS_DRIVE_FILE_MOVE " Load disc...")) {
                const char* patterns[3] = { "*.iso", "*.bin", "*.cue" };

                const char* file = tinyfd_openFileDialog(
                    "Select CD/DVD image",
                    "",
                    3,
                    patterns,
                    "Disc images",
                    0
                );

                if (file) {
                    ps2_reset(lunar->ps2);
                    ps2_cdvd_open(lunar->ps2->cdvd, file);

                    lunar->loaded = file;
                }
            }

            if (MenuItem(ICON_MS_DRAFT " Load executable...")) {
                const char* patterns[3] = { "*.elf" };

                const char* file = tinyfd_openFileDialog(
                    "Select ELF executable",
                    "",
                    1,
                    patterns,
                    "ELF executables",
                    0
                );

                if (file) {
                    // Temporarily disable window updates
                    struct gs_callback cb = *ps2_gs_get_callback(lunar->ps2->gs, GS_EVENT_VBLANK);

                    ps2_gs_remove_callback(lunar->ps2->gs, GS_EVENT_VBLANK);

                    ps2_elf_load(lunar->ps2, file);

                    // Re-enable window updates
                    ps2_gs_init_callback(lunar->ps2->gs, GS_EVENT_VBLANK, cb.func, cb.udata);

                    lunar->loaded = file;
                }
            }

            Separator();

            if (MenuItem(lunar->pause ? ICON_MS_PLAY_ARROW " Run" : ICON_MS_PAUSE " Pause", "Space")) {
                lunar->pause = !lunar->pause;
            }

            if (MenuItem(ICON_MS_FOLDER " Change disc...")) {
                const char* patterns[3] = { "*.iso", "*.bin", "*.cue" };

                const char* file = tinyfd_openFileDialog(
                    "Select CD/DVD image",
                    "",
                    3,
                    patterns,
                    "Disc images",
                    0
                );

                if (file) {
                    ps2_cdvd_open(lunar->ps2->cdvd, file);

                    lunar->loaded = file;
                }
            }

            EndMenu();
        }
        if (BeginMenu("Settings")) {
            if (BeginMenu(ICON_MS_MONITOR " Display")) {
                if (BeginMenu("Scale")) {
                    for (int i = 2; i <= 6; i++) {
                        char buf[16]; sprintf(buf, "%.1fx", (float)i * 0.5f);

                        if (Selectable(buf, ((float)i * 0.5f) == lunar->scale)) {
                            lunar->scale = (float)i * 0.5f;

                            software_set_scale(lunar->ctx, lunar->scale);
                        }
                    }

                    EndMenu();
                }

                if (BeginMenu("Aspect mode")) {
                    for (int i = 0; i < 6; i++) {
                        if (Selectable(aspect_mode_names[i], lunar->aspect_mode == i)) {
                            lunar->aspect_mode = i;

                            software_set_aspect_mode(lunar->ctx, lunar->aspect_mode);
                        }
                    }

                    EndMenu();
                }

                if (BeginMenu("Scaling filter")) {
                    if (Selectable("Nearest", !lunar->bilinear)) {
                        lunar->bilinear = false;

                        software_set_bilinear(lunar->ctx, false);
                    }

                    if (Selectable("Bilinear", lunar->bilinear)) {
                        lunar->bilinear = true;

                        software_set_bilinear(lunar->ctx, true);
                    }

                    EndMenu();
                }

                MenuItem("Integer scaling", nullptr, &lunar->ctx->integer_scaling);

                if (MenuItem("Fullscreen", nullptr, &lunar->fullscreen)) {
                    SDL_SetWindowFullscreen(lunar->window, lunar->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                }

                EndMenu();
            }

            MenuItem(ICON_MS_DOCK_TO_BOTTOM " Show status bar", nullptr, &lunar->show_status_bar);

            if (MenuItem(ICON_MS_CONTENT_COPY " Copy data path to clipboard")) {
                SDL_SetClipboardText(SDL_GetPrefPath("Allkern", "Iris"));
            }


            EndMenu();
        }
        if (BeginMenu("Tools")) {
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " ImGui Demo", NULL, &lunar->show_imgui_demo));

            EndMenu();
        }
        if (BeginMenu("Debug")) {
            SeparatorText("EE");
            // if (BeginMenu(ICON_MS_BUG_REPORT " EE")) {
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Control##ee", NULL, &lunar->show_ee_control));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " State##ee", NULL, &lunar->show_ee_state));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Logs##ee", NULL, &lunar->show_ee_logs));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Interrupts##ee", NULL, &lunar->show_ee_interrupts));

                // EndMenu();
            // }

            SeparatorText("IOP");
            // if (BeginMenu(ICON_MS_BUG_REPORT " IOP")) {
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Control##iop", NULL, &lunar->show_iop_control));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " State##iop", NULL, &lunar->show_iop_state));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Logs##iop", NULL, &lunar->show_iop_logs));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Interrupts##iop", NULL, &lunar->show_iop_interrupts));

            //     EndMenu();
            // }

            Separator();

            if (MenuItem(ICON_MS_LINE_START_CIRCLE " Breakpoints", NULL, &lunar->show_breakpoints));
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " GS debugger", NULL, &lunar->show_gs_debugger));
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " Memory viewer", NULL, &lunar->show_memory_viewer));
            
            EndMenu();
        }
        if (BeginMenu("Help")) {
            if (MenuItem(ICON_MS_LINE_START_CIRCLE " About")) {
                lunar->show_about_window = true;
            }

            EndMenu();
        }

        EndMainMenuBar();
    }

    PopStyleVar();
    PopFont();
}

}