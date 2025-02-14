#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "tfd/tinyfiledialogs.h"

namespace iris {

void show_bios_setting_window(iris::instance* iris) {
    using namespace ImGui;

    OpenPopup("Welcome");

    // Always center this window when appearing
    ImVec2 center = GetMainViewport()->GetCenter();

    SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    static char buf[512];

    if (BeginPopupModal("Welcome", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        PushFont(iris->font_heading);
        Text("Welcome to Iris!");
        PopFont();
        Separator();
        Text(
            "Iris requires a PlayStation 2 BIOS file in order to run.\n\n"
            "Please select one using the box below or provide one using the\n"
            "command line arguments."
        );

        InputTextWithHint("##BIOS", "e.g. scph10000.bin", buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
        SameLine();

        if (Button(ICON_MS_FOLDER)) {
            const char* patterns[1] = { "*.bin" };

            const char* file = tinyfd_openFileDialog(
                "Select BIOS file",
                "",
                1,
                patterns,
                "BIOS files",
                0
            );

            if (file) {
                strncpy(buf, file, 512);
            }
        }

        Separator();

        BeginDisabled(!buf[0]);

        if (Button("Done")) {
            iris->bios_path = buf;
            iris->dump_to_file = true;

            ps2_load_bios(iris->ps2, iris->bios_path.c_str());

            CloseCurrentPopup();

            iris->show_bios_setting_window = false;
        }

        EndDisabled();

        EndPopup();
    }
}

}