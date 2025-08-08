#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "pfd/pfd.h"

namespace iris {

int stage = 0;

void show_memory_card_stage(iris::instance* iris) {
    using namespace ImGui;

    PushFont(iris->font_heading);
    Text("Set up memory cards");
    PopFont();

    Separator();

    if (Button("Done")) {
        CloseCurrentPopup();

        iris->show_bios_setting_window = false;
    }
}

void show_bios_stage(iris::instance* iris) {
    using namespace ImGui;

    static char buf[512];

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
        bool mute = iris->mute;

        iris->mute = true;

        auto f = pfd::open_file("Select BIOS file", "", {
            "All File Types (*.bin; *.rom0)", "*.bin *.rom0",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        iris->mute = mute;

        if (f.result().size()) {
            strncpy(buf, f.result().at(0).c_str(), 512);
        }
    }

    Separator();

    BeginDisabled(!buf[0]);

    if (Button("Done")) {
        iris->bios_path = buf;
        iris->dump_to_file = true;

        ps2_load_bios(iris->ps2, iris->bios_path.c_str());

        stage = 1;
    }

    EndDisabled();
}

void show_bios_setting_window(iris::instance* iris) {
    using namespace ImGui;

    OpenPopup("Welcome");

    // Always center this window when appearing
    ImVec2 center = GetMainViewport()->GetCenter();

    SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (BeginPopupModal("Welcome", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        switch (stage) {
            case 0: show_bios_stage(iris); break;
            case 1: show_memory_card_stage(iris); break;
        }

        EndPopup();
    }
}

}