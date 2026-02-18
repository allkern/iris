#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"

#include "rom.h"

namespace iris {

int stage = 0;

bool bios_checked = false;
int bios_valid = false;

int is_valid(const char* path) {
    FILE* f = fopen(path, "rb");

    if (!f)
        return 0;

    fseek(f, 0, SEEK_END);

    size_t size = ftell(f);

    fseek(f, 0, SEEK_SET);

    uint8_t* buf = new uint8_t[size];

    fread(buf, 1, size, f);

    int valid = ps2_rom0_is_valid(buf, size);

    fclose(f);

    delete[] buf;

    return valid ? 2 : 1;
}

void show_memory_card_stage(iris::instance* iris) {
    using namespace ImGui;

    if (BeginChild("##iconchild", ImVec2(100.0, 0.0), ImGuiChildFlags_AutoResizeY)) {
        Image((ImTextureID)(intptr_t)iris->iris_icon.descriptor_set, ImVec2(100.0, 100.0));
    } EndChild(); SameLine(0.0, 10.0);

    if (BeginChild("##textchild", ImVec2(360.0, 0.0), ImGuiChildFlags_AutoResizeY)) {
        PushFont(iris->font_heading);
        Text("Done!");
        PopFont();

        Separator();

        Text("You may now close this window and start using Iris.\n\n"
            "Additionally you might want to check out the settings\n"
            "menu to configure memory cards, display, audio, and\n"
            "input options.\n\n"
        );

        Text(ICON_MS_WARNING " Please note that Iris is a work-in-progress emulator.\n"
            "You might encounter bugs or experience poor performance\n"
            "in general. We will appreciate any feedback or bug reports\n"
            "you may provide.\n\n"
        );

        Text("You can report issues or provide feedback at:");

        TextLinkOpenURL("https://github.com/allkern/iris/issues");

        Separator();

        static bool open_settings = true;

        if (Button("Done")) {
            CloseCurrentPopup();

            iris->show_bios_setting_window = false;

            if (open_settings) {
                iris->show_settings = true;
            }
        } SameLine();

        Checkbox("Open settings menu", &open_settings);
    } EndChild();
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

    if (InputTextWithHint("##BIOS", "e.g. scph10000.bin", buf, 512, ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
        bios_checked = true;
        bios_valid = is_valid(buf);
    }

    SameLine();

    if (Button(ICON_MS_FOLDER)) {
        audio::mute(iris);

        auto f = pfd::open_file("Select BIOS file", "", {
            "All File Types (*.bin; *.rom0)", "*.bin *.rom0",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            strncpy(buf, f.result().at(0).c_str(), 512);

            bios_checked = true;
            bios_valid = is_valid(buf);
        }
    }

    if (bios_checked) {
        ImVec4 col = bios_valid ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        const char* text = nullptr;

        switch (bios_valid) {
            case 0: {
                col = ImVec4(0.86f, 0.19f, 0.18f, 1.0f);
                text = ICON_MS_CLOSE " Couldn't open the specified file.";
            } break;

            case 1: {
                col = ImVec4(0.90f, 0.73f, 0.2f, 1.0f);
                text = ICON_MS_WARNING " BIOS is unknown, it might not work as expected.";
            } break;

            case 2: {
                col = ImVec4(0.42f, 0.85f, 0.1f, 1.0f);
                text = ICON_MS_CHECK " BIOS is known!";
            } break;
        }

        TextColored(col, text);
    }

    // To-do: Add file validation

    Separator();

    BeginDisabled(!bios_valid || !buf[0]);

    if (Button("Next")) {
        iris->bios_path = buf;
        iris->dump_to_file = true;

        if (!ps2_load_bios(iris->ps2, iris->bios_path.c_str())) {
            push_info(iris, "Couldn't load BIOS");

            stage = 0;
        } else {
            stage = 1;
        }
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