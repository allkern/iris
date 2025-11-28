#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "../pfd/pfd.h"

namespace iris {

bool hovered = false;
std::string tooltip = "";
int selected_settings = 0;

const char* settings_renderer_names[] = {
    "Null",
    "Software",
    "Software (Threaded)"
};

const char* settings_aspect_mode_names[] = {
    "Native",
    "Stretch",
    "Stretch (Keep aspect ratio)",
    "Force 4:3 (NTSC)",
    "Force 16:9 (Widescreen)",
    "Force 5:4 (PAL)",
    "Auto"
};

const char* settings_fullscreen_names[] = {
    "Windowed",
    "Fullscreen (Desktop)",
};

int settings_fullscreen_flags[] = {
    0,
    SDL_WINDOW_FULLSCREEN
};

const char* settings_buttons[] = {
    " " ICON_MS_MONITOR "  Graphics",
    " " ICON_MS_FOLDER "  Paths",
    " " ICON_MS_SIM_CARD_DOWNLOAD "  Memory cards",
    " " ICON_MS_MORE_HORIZ "  Misc.",
    nullptr
};

void show_graphics_settings(iris::instance* iris) {
    using namespace ImGui;

    // PushFont(iris->font_heading);
    // Text("Graphics");
    // PopFont();

    // Separator();

    static const char* settings_renderer_names[] = {
        "Null",
        "Software",
        "Hardware"
    };

    Text("Renderer");

    if (BeginCombo("##renderer", settings_renderer_names[iris->renderer_backend], ImGuiComboFlags_HeightSmall)) {
        for (int i = 0; i < 3; i++) {
            BeginDisabled(i == RENDERER_BACKEND_SOFTWARE);

            if (Selectable(settings_renderer_names[i], i == iris->renderer_backend)) {
                iris->renderer_backend = i;

                renderer_switch(iris->renderer, i);
            }

            EndDisabled();
        }

        EndCombo();
    }

    Text("Aspect mode");

    if (BeginCombo("##aspectmode", settings_aspect_mode_names[iris->aspect_mode])) {
        for (int i = 0; i < 7; i++) {
            if (Selectable(settings_aspect_mode_names[i], iris->aspect_mode == i)) {
                iris->aspect_mode = i;
            }
        }

        EndCombo();
    }

    BeginDisabled(
        iris->aspect_mode == RENDER_ASPECT_AUTO ||
        iris->aspect_mode == RENDER_ASPECT_STRETCH ||
        iris->aspect_mode == RENDER_ASPECT_STRETCH_KEEP
    );

    Text("Scale");

    char buf[16]; snprintf(buf, 16, "%.1fx", (float)iris->scale);

    if (BeginCombo("##scale", buf, ImGuiComboFlags_HeightSmall)) {
        for (int i = 2; i <= 6; i++) {
            snprintf(buf, 16, "%.1fx", (float)i * 0.5f);

            if (Selectable(buf, ((float)i * 0.5f) == iris->scale)) {
                iris->scale = (float)i * 0.5f;
            }
        }

        EndCombo();
    }

    EndDisabled();

    Text("Scaling");

    if (BeginCombo("##scalingfilter", iris->bilinear ? "Bilinear" : "Nearest")) {
        if (Selectable("Nearest", !iris->bilinear)) {
            iris->bilinear = false;

            // renderer_set_bilinear(iris->ctx, false);
        }

        if (Selectable("Bilinear", iris->bilinear)) {
            iris->bilinear = true;

            // renderer_set_bilinear(iris->ctx, true);
        }

        EndCombo();
    } SameLine();

    Checkbox("Integer scaling", &iris->integer_scaling);

    Text("Window mode");

    if (BeginCombo("##windowmode", settings_fullscreen_names[iris->fullscreen])) {
        for (int i = 0; i < 2; i++) {
            if (Selectable(settings_fullscreen_names[i], iris->fullscreen == i)) {
                iris->fullscreen = i;

                SDL_SetWindowFullscreen(iris->window, settings_fullscreen_flags[i]);
            }
        }

        EndCombo();
    }
}

void show_paths_settings(iris::instance* iris) {
    using namespace ImGui;

    static char buf[512];
    static char dvd_buf[512];
    static char rom2_buf[512];

    Text("BIOS (rom0)");

    if (IsItemHovered()) {
        hovered = true;

        tooltip = ICON_MS_INFO " Select a BIOS file, this is required for the emulator to function properly";
    }

    const char* bios_hint = iris->bios_path.size() ? iris->bios_path.c_str() : "e.g. scph10000.bin";
    const char* rom1_hint = iris->rom1_path.size() ? iris->rom1_path.c_str() : "Not configured";
    const char* rom2_hint = iris->rom2_path.size() ? iris->rom2_path.c_str() : "Not configured";

    SetNextItemWidth(300);

    InputTextWithHint("##rom0", bios_hint, buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom0")) {
        bool mute = iris->mute;

        iris->mute = true;

        auto f = pfd::open_file("Select BIOS file", "", {
            "BIOS dumps (*.bin; *.rom0)", "*.bin *.rom0",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        iris->mute = mute;

        if (f.result().size()) {
            strncpy(buf, f.result().at(0).c_str(), 512);
        }
    }

    Text("DVD Player (rom1)");

    SetNextItemWidth(300);

    InputTextWithHint("##rom1", rom1_hint, dvd_buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom1")) {
        bool mute = iris->mute;

        iris->mute = true;

        auto f = pfd::open_file("Select DVD BIOS file", "", {
            "DVD BIOS dumps (*.bin; *.rom1)", "*.bin *.rom1",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        iris->mute = mute;

        if (f.result().size()) {
            strncpy(dvd_buf, f.result().at(0).c_str(), 512);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##rom1")) {
        memset(dvd_buf, 0, 512);
    }

    Text("Chinese extensions (rom2)");

    SetNextItemWidth(300);

    InputTextWithHint("##rom2", rom2_hint, rom2_buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom2")) {
        bool mute = iris->mute;

        iris->mute = true;

        auto f = pfd::open_file("Select ROM2 file", "", {
            "ROM2 dumps (*.bin; *.rom2)", "*.bin *.rom2",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        iris->mute = mute;

        if (f.result().size()) {
            strncpy(rom2_buf, f.result().at(0).c_str(), 512);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##rom2")) {
        memset(rom2_buf, 0, 512);
    }

    if (Button(ICON_MS_SAVE " Save")) {
        std::string bios_path = buf;

        if (bios_path.size()) iris->bios_path = bios_path;

        iris->rom1_path = dvd_buf;
        iris->rom2_path = rom2_buf;
    } SameLine();

    TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Restart the emulator to apply these changes");
}

void show_memory_card(iris::instance* iris, int slot) {
    using namespace ImGui;

    char label[9] = "##mcard0";

    label[7] = '0' + slot;

    if (BeginChild(label, ImVec2(GetContentRegionAvail().x / (slot ? 1.0 : 2.0), 0))) {
        std::string& path = slot ? iris->mcd1_path : iris->mcd0_path;

        ImVec4 col = GetStyleColorVec4(iris->mcd[slot] ? ImGuiCol_Text : ImGuiCol_TextDisabled);

        col.w = 1.0;

        InvisibleButton("##pad", ImVec2(10, 10));

        int icon_width = iris->ps2_memory_card_icon_width;
        int icon_height = iris->ps2_memory_card_icon_height;
        // SDL_GPUTexture* icon_tex = iris->ps2_memory_card_icon_tex;

        SetCursorPosX((GetContentRegionAvail().x / 2.0) - (icon_width / 2.0));

        // Image(
        //     (ImTextureID)(intptr_t)icon_tex,
        //     ImVec2(icon_width, icon_height),
        //     ImVec2(0, 0), ImVec2(1, 1),
        //     col,
        //     ImVec4(0.0, 0.0, 0.0, 0.0)
        // );

        InvisibleButton("##pad", ImVec2(10, 10));

        if (path.size() && !iris->mcd[slot]) {
            TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Check file");

            if (IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                if (BeginTooltip()) {
                    Text("please check files ");
    
                    EndTooltip();
                }
            }
        }

        PushFont(iris->font_heading);
        Text("Slot %d", slot+1);
        PopFont();

        static char buf[512];
        const char* hint = path.size() ? path.c_str() : "Not configured";

        char it_label[7] = "##mcd0";
        char bt_label[10] = ICON_MS_FOLDER "##mcd0";
        char ed_label[10] = ICON_MS_EDIT "##mcd0";

        it_label[5] = '0' + slot;
        bt_label[8] = '0' + slot;
        ed_label[8] = '0' + slot;

        InputTextWithHint(it_label, hint, buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
        SameLine();

        if (Button(bt_label)) {
            bool mute = iris->mute;

            iris->mute = true;

            auto f = pfd::open_file("Select Memory Card file for Slot 1", iris->pref_path, {
                "Memory Card files (*.ps2; *.mcd; *.bin)", "*.ps2 *.mcd *.bin",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            iris->mute = mute;

            if (f.result().size()) {
                strncpy(buf, f.result().at(0).c_str(), 512);

                path = f.result().at(0);
            }
        }

        SameLine();

        if (Button(ed_label)) {

        }
    } EndChild();
}

void show_memory_card_settings(iris::instance* iris) {
    using namespace ImGui;

    if (Button(ICON_MS_EDIT " Create memory cards...")) {
        // Launch memory card utility
        iris->show_memory_card_tool = true;
    }

    Separator();

    show_memory_card(iris, 0); SameLine(0.0, 10.0);
    show_memory_card(iris, 1);
}

void show_settings(iris::instance* iris) {
    using namespace ImGui;

    hovered = false;

    static const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking;

    if (Begin("Settings", &iris->show_settings, flags)) {
        PushStyleVarX(ImGuiStyleVar_ButtonTextAlign, 0.0);
        PushStyleVarY(ImGuiStyleVar_ItemSpacing, 6.0);

        if (BeginChild("##sidebar", ImVec2(175, GetContentRegionAvail().y - 100.0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border)) {
            for (int i = 0; settings_buttons[i]; i++) {
                if (selected_settings == i) PushStyleColor(ImGuiCol_Button, GetStyle().Colors[ImGuiCol_ButtonHovered]);

                bool pressed = Button(settings_buttons[i], ImVec2(175, 35));
                
                if (selected_settings == i) PopStyleColor();

                if (pressed) {
                    selected_settings = i;
                }
            }
        } EndChild(); SameLine(0.0, 10.0);

        PopStyleVar(2);

        if (BeginChild("##content", ImVec2(0, GetContentRegionAvail().y - 100.0), ImGuiChildFlags_AutoResizeY)) {
            switch (selected_settings) {
                case 0: show_graphics_settings(iris); break;
                case 1: show_paths_settings(iris); break;
                case 2: show_memory_card_settings(iris); break;
            }
        } EndChild();

        Separator();

        if (hovered) {
            TextWrapped(tooltip.c_str());
        } else {
            Text("Hover over an item to get more information");
        }
    } End();
}

}