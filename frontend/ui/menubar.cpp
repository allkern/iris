#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"

namespace lunar {

void show_main_menubar(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_icons);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 7.0));

    if (BeginMainMenuBar()) {
        if (BeginMenu("Lunar")) {
            // Load disc
            // Load executable
            // Change disc
            // Pause
            MenuItem(ICON_MS_DRIVE_FILE_MOVE " Load disc...");
            MenuItem(ICON_MS_DRAFT " Load executable...");
            MenuItem(ICON_MS_FOLDER " Change disc...");

            EndMenu();
        }
        if (BeginMenu("Settings")) {
            EndMenu();
        }
        if (BeginMenu("Debug")) {
            if (BeginMenu(ICON_MS_BUG_REPORT " EE")) {
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Control", NULL, &lunar->show_ee_control));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " State", NULL, &lunar->show_ee_state));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Logs", NULL, &lunar->show_ee_logs));

                EndMenu();
            }

            if (BeginMenu(ICON_MS_BUG_REPORT " IOP")) {
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Control", NULL, &lunar->show_iop_control));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " State", NULL, &lunar->show_iop_state));
                if (MenuItem(ICON_MS_LINE_START_CIRCLE " Logs", NULL, &lunar->show_iop_logs));

                EndMenu();
            }

            if (MenuItem(ICON_MS_LINE_START_CIRCLE " GS debugger", NULL, &lunar->show_gs_debugger));
            
            EndMenu();
        }

        EndMainMenuBar();
    }

    PopStyleVar();
    PopFont();
}

}