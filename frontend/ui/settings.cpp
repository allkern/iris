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
    "Fullscreen"
};

int settings_fullscreen_flags[] = {
    0,
    SDL_WINDOW_FULLSCREEN_DESKTOP,
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

    Text("Renderer");

    if (BeginCombo("##renderer", renderer_get_name(iris->ctx), ImGuiComboFlags_HeightSmall)) {
        for (int i = 0; i < 3; i++) {
            if (Selectable(settings_renderer_names[i], i == iris->renderer_backend)) {
                iris->renderer_backend = i;

                renderer_init_backend(iris->ctx, iris->ps2->gs, iris->window, i);
                renderer_set_scale(iris->ctx, iris->scale);
                renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
                renderer_set_bilinear(iris->ctx, iris->bilinear);
                renderer_set_integer_scaling(iris->ctx, iris->integer_scaling);
                renderer_set_size(iris->ctx, 0, 0);
            }
        }

        EndCombo();
    }

    Text("Aspect mode");

    if (BeginCombo("##aspectmode", settings_aspect_mode_names[iris->aspect_mode])) {
        for (int i = 0; i < 7; i++) {
            if (Selectable(settings_aspect_mode_names[i], iris->aspect_mode == i)) {
                iris->aspect_mode = i;

                renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
            }
        }

        EndCombo();
    }

    BeginDisabled(
        iris->aspect_mode == RENDERER_ASPECT_AUTO ||
        iris->aspect_mode == RENDERER_ASPECT_STRETCH ||
        iris->aspect_mode == RENDERER_ASPECT_STRETCH_KEEP
    );

    Text("Scale");

    char buf[16]; snprintf(buf, 16, "%.1fx", (float)iris->scale);

    if (BeginCombo("##scale", buf, ImGuiComboFlags_HeightSmall)) {
        for (int i = 2; i <= 6; i++) {
            snprintf(buf, 16, "%.1fx", (float)i * 0.5f);

            if (Selectable(buf, ((float)i * 0.5f) == iris->scale)) {
                iris->scale = (float)i * 0.5f;

                renderer_set_scale(iris->ctx, iris->scale);
            }
        }

        EndCombo();
    }

    EndDisabled();

    Text("Scaling");

    if (BeginCombo("##scalingfilter", iris->bilinear ? "Bilinear" : "Nearest")) {
        if (Selectable("Nearest", !iris->bilinear)) {
            iris->bilinear = false;

            renderer_set_bilinear(iris->ctx, false);
        }

        if (Selectable("Bilinear", iris->bilinear)) {
            iris->bilinear = true;

            renderer_set_bilinear(iris->ctx, true);
        }

        EndCombo();
    } SameLine();

    Checkbox("Integer scaling", &iris->integer_scaling);

    Text("Window mode");

    if (BeginCombo("##windowmode", settings_fullscreen_names[iris->fullscreen])) {
        for (int i = 0; i < 3; i++) {
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
        iris->mute = true;

        auto f = pfd::open_file("Select BIOS file", "", {
            "BIOS dumps (*.bin)", "*.bin",
            "All Files (*.*)", "*"
        });

        iris->mute = false;

        if (f.result().size()) {
            strncpy(buf, f.result().at(0).c_str(), 512);
        }
    }

    Text("DVD Player (rom1)");

    SetNextItemWidth(300);

    InputTextWithHint("##rom1", rom1_hint, dvd_buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom1")) {
        iris->mute = true;

        auto f = pfd::open_file("Select DVD BIOS file", "", {
            "DVD BIOS dumps (*.bin)", "*.bin",
            "All Files (*.*)", "*"
        });

        iris->mute = false;

        if (f.result().size()) {
            strncpy(dvd_buf, f.result().at(0).c_str(), 512);
        }
    }

    Text("Chinese extensions (rom2)");

    SetNextItemWidth(300);

    InputTextWithHint("##rom2", rom2_hint, rom2_buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom2")) {
        iris->mute = true;

        auto f = pfd::open_file("Select ROM2 file", "", {
            "ROM2 dumps (*.bin)", "*.bin",
            "All Files (*.*)", "*"
        });

        iris->mute = false;

        if (f.result().size()) {
            strncpy(rom2_buf, f.result().at(0).c_str(), 512);
        }
    }

    TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Restart the emulator to apply these changes");
}

void show_memory_card_settings(iris::instance* iris) {
    using namespace ImGui;

    if (Button(ICON_MS_EDIT " Create memory cards...")) {
        // Launch memory card utility
        iris->show_memory_card_tool = true;
    }

    Separator();

    if (BeginChild("##mcard0", ImVec2(GetContentRegionAvail().x / 2.0, 0))) {
        TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Check file");

        if (IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {        
            if (BeginTooltip()) {
                Text("Sex");

                EndTooltip();
            }
        }

        PushFont(iris->font_heading);
        Text("Slot 1");
        PopFont();

        static char buf[512];
        const char* hint = iris->mcd0_path.size() ? iris->mcd0_path.c_str() : "Not configured";

        InputTextWithHint("##mcd0", hint, buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
        SameLine();

        if (Button(ICON_MS_FOLDER "##mcd0")) {
            iris->mute = true;

            auto f = pfd::open_file("Select Memory Card file for Slot 1", iris->pref_path, {
                "Memory Card files (*.mcd; *.bin)", "*.mcd *.bin",
                "All Files (*.*)", "*"
            });

            iris->mute = false;

            if (f.result().size()) {
                strncpy(buf, f.result().at(0).c_str(), 512);
            }
        }
    } EndChild(); SameLine(0.0, 10.0);

    if (BeginChild("##mcard1", ImVec2(GetContentRegionAvail().x, 0))) {
        TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Check file");

        if (IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {        
            if (BeginTooltip()) {
                Text("Sex");

                EndTooltip();
            }
        }

        PushFont(iris->font_heading);
        Text("Slot 2");
        PopFont();

        static char buf[512];
        const char* hint = iris->mcd1_path.size() ? iris->mcd1_path.c_str() : "Not configured";

        InputTextWithHint("##mcd1", hint, buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
        SameLine();

        if (Button(ICON_MS_FOLDER "##mcd1")) {
            iris->mute = true;

            auto f = pfd::open_file("Select Memory Card file for Slot 2", iris->pref_path, {
                "Memory Card files (*.mcd; *.bin)", "*.mcd *.bin",
                "All Files (*.*)", "*"
            });

            iris->mute = false;

            if (f.result().size()) {
                strncpy(buf, f.result().at(0).c_str(), 512);
            }
        }
    } EndChild();
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