#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"

namespace iris {

bool hovered = false;
std::string tooltip = "";
int selected_settings = 0;
int saved = 0;

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
    " " ICON_MS_DEPLOYED_CODE "  System",
    " " ICON_MS_MONITOR "  Graphics",
    " " ICON_MS_BRUSH "  Shaders",
    " " ICON_MS_FOLDER "  Paths",
    " " ICON_MS_SD_CARD "  Memory cards",
    " " ICON_MS_MORE_HORIZ "  Misc.",
    nullptr
};

const char* system_names[] = {
    "Auto",
    "Retail (Fat)",
    "Retail (Slim)",
    "PSX DESR",
    "TEST unit (DTL-H)",
    "TOOL unit (DTL-T)",
    "Konami Python",
    "Konami Python 2",
    "Namco System 147",
    "Namco System 148",
    "Namco System 246",
    "Namco System 256"
};

const char* mechacon_model_names[] = {
    "SPC970",
    "Dragon"
};

void show_system_settings(iris::instance* iris) {
    using namespace ImGui;

    Text("Model");

    if (BeginCombo("##combo", system_names[iris->system])) {
        for (int i = 0; i < 6; i++) {
            if (Selectable(system_names[i], i == iris->system)) {
                iris->system = i;

                ps2_set_system(iris->ps2, i);
            }
        }

        EndCombo();
    }

    if (BeginTable("##specs-table", 2, ImGuiTableFlags_SizingFixedSame)) {
        TableNextRow();

        if (iris->system == 0) {
            TableSetColumnIndex(0);
            TextDisabled("Detected system");
            TableSetColumnIndex(1);
            Text("%s", system_names[iris->ps2->detected_system]);
            TableNextRow();
        }

        TableSetColumnIndex(0);
        TextDisabled("Main RAM");
        TableSetColumnIndex(1);
        Text("%d MB", iris->ps2->ee_ram->size / (1024 * 1024));

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("IOP RAM");
        TableSetColumnIndex(1);
        Text("%d MB", iris->ps2->iop_ram->size / (1024 * 1024));

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("MechaCon Model");
        TableSetColumnIndex(1);
        Text("%s", mechacon_model_names[iris->ps2->cdvd->mechacon_model]);

        EndTable();
    }

    Text("\nTimescale");

    char buf[16];

    sprintf(buf, "%dx", iris->timescale);

    if (BeginCombo("##timescale", buf)) {
        for (int i = 0; i < 9; i++) {
            char buf[16]; snprintf(buf, 16, "%dx", 1 << i);

            if (Selectable(buf, iris->timescale == (1 << i))) {
                iris->timescale = (1 << i);

                ps2_set_timescale(iris->ps2, iris->timescale);
            }
        }

        EndCombo();
    }

    if (BeginTable("##effective-clock", 2, ImGuiTableFlags_SizingFixedSame)) {
        TableNextRow();

        TableSetColumnIndex(0);
        TextDisabled("Effective frequency");
        TableSetColumnIndex(1);
        Text("%.3f MHz", 294.912f / iris->timescale);

        EndTable();
    }
}

const char* ssaa_names[] = {
    "Disabled",
    "2x",
    "4x",
    "8x",
    "16x"
};

void show_hardware_renderer_settings(iris::instance* iris) {
    using namespace ImGui;

    Text("SSAA");

    if (BeginCombo("##ssaa", ssaa_names[iris->hardware_backend_config.super_sampling])) {
        for (int i = 0; i < 5; i++) {
            if (Selectable(ssaa_names[i], iris->hardware_backend_config.super_sampling == i)) {
                iris->hardware_backend_config.super_sampling = i;

                if (i != 0) {
                    iris->hardware_backend_config.force_progressive = true;
                }

                render::refresh(iris);
            }
        }

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    BeginDisabled(iris->hardware_backend_config.super_sampling != 0);
    if (Checkbox(" Force progressive scan", &iris->hardware_backend_config.force_progressive)) {
        render::refresh(iris);
    }
    EndDisabled();

    if (Checkbox(" Overscan", &iris->hardware_backend_config.overscan)) {
        render::refresh(iris);
    }
    PopStyleVar();

    SeparatorText("Advanced");

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    if (Checkbox(" CRTC Offsets", &iris->hardware_backend_config.crtc_offsets)) {
        render::refresh(iris);
    }

    if (Checkbox(" Disable Mipmaps", &iris->hardware_backend_config.disable_mipmaps)) {
        render::refresh(iris);
    }

    if (Checkbox(" Unsynced Readbacks", &iris->hardware_backend_config.unsynced_readbacks)) {
        render::refresh(iris);
    }

    if (Checkbox(" Backbuffer Promotion", &iris->hardware_backend_config.backbuffer_promotion)) {
        render::refresh(iris);
    }

    if (Checkbox(" Allow Blend Demote", &iris->hardware_backend_config.allow_blend_demote)) {
        render::refresh(iris);
    }
    PopStyleVar();

    SeparatorText("Vulkan settings");

    Text("GPU");

    const char* hint;
    const auto& selected_device = iris->vulkan_physical_devices[iris->vulkan_selected_device_index];

    if (iris->vulkan_physical_device < 0) {
        hint = "Auto";
    } else {
        hint = iris->vulkan_physical_devices[iris->vulkan_physical_device].name.c_str();
    }

    PushStyleVarY(ImGuiStyleVar_ItemSpacing, 5.0F);

    if (BeginCombo("##gpu", hint)) {
        if (Selectable("Auto", iris->vulkan_physical_device < 0)) {
            iris->vulkan_physical_device = -1;
        }

        for (int i = 0; i < iris->vulkan_physical_devices.size(); i++) {
            const auto& device = iris->vulkan_physical_devices[i];

            std::string name = device.name;

            if (device.device == selected_device.device) {
                name += " (Current)";
            }

            if (Selectable(name.c_str(), device.device == selected_device.device)) {
                iris->vulkan_physical_device = i;
            }
        }

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox(" Enable validation layers", &iris->vulkan_enable_validation_layers);
    PopStyleVar(2);
}

void show_graphics_settings(iris::instance* iris) {
    using namespace ImGui;

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
                render::switch_backend(iris, i);
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

    const char* filter_names[] = {
        "Nearest",
        "Bilinear",
        "Cubic"
    };

    if (BeginCombo("##scalingfilter", filter_names[iris->filter])) {
        for (int i = 0; i < 3; i++) {
            BeginDisabled(i == 2 && !iris->cubic_supported);
            if (Selectable(filter_names[i], iris->filter == i)) {
                iris->filter = i;
            }
            EndDisabled();
        }

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox(" Integer scaling", &iris->integer_scaling);
    PopStyleVar();

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

    if (iris->renderer_backend == RENDERER_BACKEND_HARDWARE) {
        SeparatorText("Renderer settings");

        show_hardware_renderer_settings(iris);
    }
}

void show_paths_settings(iris::instance* iris) {
    using namespace ImGui;

    static char buf[512];
    static char dvd_buf[512];
    static char rom2_buf[512];
    static char nvram_buf[512];
    static char flash_buf[512];

    Text("BIOS (rom0)");

    if (IsItemHovered()) {
        hovered = true;

        tooltip = ICON_MS_INFO " Select a BIOS file, this is required for the emulator to function properly";
    }

    const char* bios_hint = iris->bios_path.size() ? iris->bios_path.c_str() : "e.g. scph10000.bin";
    const char* rom1_hint = iris->rom1_path.size() ? iris->rom1_path.c_str() : "Not configured";
    const char* rom2_hint = iris->rom2_path.size() ? iris->rom2_path.c_str() : "Not configured";
    const char* nvram_hint = iris->nvram_path.size() ? iris->nvram_path.c_str() : "Not configured";
    const char* flash_hint = iris->flash_path.size() ? iris->flash_path.c_str() : "Not configured";

    SetNextItemWidth(300);

    InputTextWithHint("##rom0", bios_hint, buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom0")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select BIOS file", "", {
            "BIOS dumps (*.bin; *.rom0)", "*.bin *.rom0",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            strncpy(buf, f.result().at(0).c_str(), 512);

            ps2_load_bios(iris->ps2, buf);
        }
    }

    if (BeginTable("##rom-info", 2, ImGuiTableFlags_SizingFixedFit)) {
        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("Model" " ");
        TableSetColumnIndex(1);
        Text("%s", iris->ps2->rom0_info.model);

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("Version" " ");
        TableSetColumnIndex(1);
        Text("%s", iris->ps2->rom0_info.version);

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("Region" " ");
        TableSetColumnIndex(1);
        Text("%s", iris->ps2->rom0_info.region);

        TableNextRow();
        TableSetColumnIndex(0);
        TextDisabled("MD5 hash" " ");
        TableSetColumnIndex(1);
        Text("%s", iris->ps2->rom0_info.md5hash); SameLine();
        if (SmallButton(ICON_MS_CONTENT_COPY)) {
            SDL_SetClipboardText(iris->ps2->rom0_info.md5hash);
        }

        EndTable();
    }

    Separator();

    Text("DVD Player (rom1)");

    SetNextItemWidth(300);

    InputTextWithHint("##rom1", rom1_hint, dvd_buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom1")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select DVD BIOS file", "", {
            "DVD BIOS dumps (*.bin; *.rom1)", "*.bin *.rom1",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            strncpy(dvd_buf, f.result().at(0).c_str(), 512);

            ps2_load_rom1(iris->ps2, dvd_buf);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##rom1")) {
        iris->rom1_path = "";

        memset(dvd_buf, 0, 512);
    }

    if (iris->rom1_path.size()) {
        if (BeginTable("##rom1-info", 2, ImGuiTableFlags_SizingFixedFit)) {
            TableNextRow();
            TableSetColumnIndex(0);
            TextDisabled("Version" " ");
            TableSetColumnIndex(1);
            Text("%s", iris->ps2->rom1_info.version);

            TableNextRow();
            TableSetColumnIndex(0);
            TextDisabled("MD5 hash" " ");
            TableSetColumnIndex(1);
            Text("%s", iris->ps2->rom1_info.md5hash); SameLine();
            if (SmallButton(ICON_MS_CONTENT_COPY)) {
                SDL_SetClipboardText(iris->ps2->rom1_info.md5hash);
            }

            EndTable();
        }

        Separator();
    }

    Text("Chinese extensions (rom2)");

    SetNextItemWidth(300);

    InputTextWithHint("##rom2", rom2_hint, rom2_buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##rom2")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select ROM2 file", "", {
            "ROM2 dumps (*.bin; *.rom2)", "*.bin *.rom2",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            strncpy(rom2_buf, f.result().at(0).c_str(), 512);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##rom2")) {
        iris->rom2_path = "";

        memset(rom2_buf, 0, 512);
    } 

    Text("EEPROM memory (nvram)");

    SetNextItemWidth(300);

    InputTextWithHint("##nvram", nvram_hint, nvram_buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##nvram")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select NVRAM file", "", {
            "NVRAM dumps (*.nvm; *.bin)", "*.nvm *.bin",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            strncpy(nvram_buf, f.result().at(0).c_str(), 512);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##nvram")) {
        iris->nvram_path = "";

        memset(nvram_buf, 0, 512);
    } 

    Text("Flash memory (xfrom)");

    SetNextItemWidth(300);

    InputTextWithHint("##flash", flash_hint, flash_buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
    SameLine();

    if (Button(ICON_MS_FOLDER "##flash")) {
        audio::mute(iris);

        auto f = pfd::open_file("Select Flash/XFROM dump file", "", {
            "XFROM dumps (*.bin)", "*.bin",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            strncpy(flash_buf, f.result().at(0).c_str(), 512);
        }
    } SameLine();

    if (Button(ICON_MS_CLEAR "##xfrom")) {
        iris->flash_path = "";

        memset(flash_buf, 0, 512);
    } 

    if (Button(ICON_MS_SAVE " Save")) {
        std::string bios_path = buf;
        std::string rom1_path = dvd_buf;
        std::string rom2_path = rom2_buf;
        std::string flash_path = flash_buf;
        std::string nvram_path = nvram_buf;

        if (bios_path.size()) iris->bios_path = bios_path;
        if (rom1_path.size()) iris->rom1_path = rom1_path;
        if (rom2_path.size()) iris->rom2_path = rom2_path;
        if (flash_path.size()) iris->flash_path = flash_path;
        if (nvram_path.size()) iris->nvram_path = nvram_path;

        saved = 1;
    } SameLine();

    if (saved) {
        TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Restart the emulator to apply these changes");
    }
}

static char slot0_buf[1024];
static char slot1_buf[1024];

void show_memory_card(iris::instance* iris, int slot) {
    using namespace ImGui;

    char label[9] = "##mcard0";

    label[7] = '0' + slot;

    if (BeginChild(label, ImVec2(GetContentRegionAvail().x / (slot ? 1.0 : 2.0), 0))) {
        std::string& path = slot ? iris->mcd1_path : iris->mcd0_path;

        ImVec4 col = GetStyleColorVec4(iris->mcd_slot_type[slot] ? ImGuiCol_Text : ImGuiCol_TextDisabled);

        col.w = 1.0;

        InvisibleButton("##pad0", ImVec2(10, 10));

        texture* tex = &iris->ps2_memory_card_icon;

        if (iris->mcd_slot_type[slot] == 2) {
            tex = &iris->ps1_memory_card_icon;
        } else if (iris->mcd_slot_type[slot] == 3) {
            tex = &iris->pocketstation_icon;
        }

        SetCursorPosX((GetContentRegionAvail().x / 2.0) - (tex->width / 2.0));

        Image(
            (ImTextureID)(intptr_t)tex->descriptor_set,
            ImVec2(tex->width, tex->height),
            ImVec2(0, 0), ImVec2(1, 1),
            col,
            ImVec4(0.0, 0.0, 0.0, 0.0)
        );

        InvisibleButton("##pad1", ImVec2(10, 10));

        if (path.size() && !iris->mcd_slot_type[slot]) {
            TextColored(ImVec4(211.0/255.0, 167.0/255.0, 30.0/255.0, 1.0), ICON_MS_WARNING " Check file");

            if (IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                if (BeginTooltip()) {
                    Text("Please check files ");
    
                    EndTooltip();
                }
            }
        }

        PushFont(iris->font_heading);
        Text("Slot %d", slot+1);
        PopFont();

        char* buf = slot ? slot1_buf : slot0_buf;
        const char* hint = path.size() ? path.c_str() : "Not configured";

        char it_label[7] = "##mcd0";
        char bt_label[10] = ICON_MS_FOLDER "##mcd0";
        char ed_label[10];

        snprintf(ed_label, 10, "%s##mcd0", iris->mcd_slot_type[slot] ? ICON_MS_ARROW_DOWNWARD : ICON_MS_ARROW_UPWARD);

        it_label[5] = '0' + slot;
        bt_label[8] = '0' + slot;
        ed_label[8] = '0' + slot;

        InputTextWithHint(it_label, hint, buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
        SameLine();

        if (Button(bt_label)) {
            audio::mute(iris);

            auto f = pfd::open_file("Select Memory Card file for Slot 1", iris->pref_path, {
                "Memory Card files (*.ps2; *.mcd; *.bin; *.psm; *.pocket)", "*.ps2 *.mcd *.bin *.psm *.pocket",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                strncpy(buf, f.result().at(0).c_str(), 512);

                path = f.result().at(0);

                emu::attach_memory_card(iris, slot, path.c_str());
            }
        }

        SameLine();

        BeginDisabled((!iris->mcd_slot_type[slot]) && (!path.size()));

        if (Button(ed_label)) {
            if (iris->mcd_slot_type[slot]) {
                emu::detach_memory_card(iris, slot);
            } else {
                emu::attach_memory_card(iris, slot, path.c_str());
            }
        }

        EndDisabled();
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

static const char* const theme_names[] = {
    "Granite",
    "ImGui Dark",
    "ImGui Light",
    "ImGui Classic",
    0
};

void show_misc_settings(iris::instance* iris) {
    using namespace ImGui;

    SeparatorText("Style");

    Text("Theme");

    if (BeginCombo("##theme", theme_names[iris->theme])) {
        for (int i = 0; theme_names[i]; i++) {
            if (Selectable(theme_names[i], iris->theme == i)) {
                iris->theme = i;

                imgui::set_theme(iris, i);
            }
        }

        EndCombo();
    }

    Text("Background color");

    ColorEdit3("##bgcolor", (float*)&iris->clear_value.color);

    Text("UI scale");

    DragFloat("##uiscale", &iris->ui_scale, 0.05f, 0.5f, 1.5f, "%.1f");

    // SliderFloat("##uiscale", &iris->ui_scale, 0.5f, 2.0f, "%.3f");

    GetStyle().FontScaleMain = iris->ui_scale;

    SeparatorText("Screenshots");

    const char* format_names[] = {
        "PNG",
        "BMP",
        "JPG",
        "TGA"
    };

    const char* jpg_quality_names[] = {
        "Minimum", // 1
        "Low", // 25
        "Medium", // 50
        "High", // 90
        "Maximum", // 100
        "Custom..."
    };

    const char* mode_names[] = {
        "Internal",
        "Display"
    };

    Text("Format");

    if (BeginCombo("##screenshotformat", format_names[iris->screenshot_format])) {
        for (int i = 0; i < 4; i++) {
            if (Selectable(format_names[i], iris->screenshot_format == i)) {
                iris->screenshot_format = i;
            }
        }

        EndCombo();
    }

    Text("Resolution mode");

    if (BeginCombo("##screenshotmode", mode_names[iris->screenshot_mode])) {
        for (int i = 0; i < 2; i++) {
            if (Selectable(mode_names[i], iris->screenshot_mode == i)) {
                iris->screenshot_mode = i;
            }
        }

        EndCombo();
    }

    if (iris->screenshot_format == IRIS_SCREENSHOT_FORMAT_JPG) {
        Text("JPG Quality");

        if (BeginCombo("##jpgquality", jpg_quality_names[iris->screenshot_jpg_quality_mode])) {
            for (int i = 0; i < 6; i++) {
                if (Selectable(jpg_quality_names[i], iris->screenshot_jpg_quality_mode == i)) {
                    iris->screenshot_jpg_quality_mode = i;
                }
            }

            EndCombo();
        }

        if (iris->screenshot_jpg_quality_mode == IRIS_SCREENSHOT_JPG_QUALITY_CUSTOM) {
            SliderInt("Quality##jpgqualitycustom", &iris->screenshot_jpg_quality, 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
        }
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox(" Include shader processing", &iris->screenshot_shader_processing);
    PopStyleVar();
}

const char* builtin_shader_names[] = {
    "iris-ntsc-encoder",
    "iris-ntsc-decoder",
    "iris-ntsc-curvature"
};

const char* presets[] = {
    "NTSC codec",
    0
};

void show_shader_settings(iris::instance* iris) {
    using namespace ImGui;

    static const char* selected_shader = "";

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0f);
    Checkbox(" Enable shaders", &iris->enable_shaders);
    PopStyleVar();

    Separator();

    Text("Add shader");
    if (BeginCombo("##combo", selected_shader)) {
        for (int i = 0; i < 3; i++) {
            if (Selectable(builtin_shader_names[i], selected_shader == builtin_shader_names[i])) {
                selected_shader = builtin_shader_names[i];
            }
        }

        EndCombo();
    } SameLine();

    if (Button(ICON_MS_ADD)) {
        if (selected_shader && selected_shader[0]) {
            std::string shader(selected_shader);

            shaders::push(iris, selected_shader);
        }
    } SameLine();

    if (Button(ICON_MS_REMOVE_SELECTION)) {
        shaders::clear(iris);
    }

    // Text("Preset");

    // if (BeginCombo("##presets", selected_shader)) {
    //     for (int i = 0; i < 3; i++) {
    //         if (Selectable(presets[i], selected_shader == builtin_shader_names[i])) {
    //             selected_shader = builtin_shader_names[i];
    //         }
    //     }

    //     EndCombo();
    // }

    if (BeginTable("##shaders", 1, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < shaders::count(iris); i++) {
            TableNextRow();

            char bypass[16];
            char del[16];
            char id[1024];

            sprintf(bypass, "%s##%d", shaders::at(iris, i)->bypass ? ICON_MS_CHECK_BOX_OUTLINE_BLANK : ICON_MS_CHECK_BOX, i);
            sprintf(del, ICON_MS_DELETE "##%d", i);
            sprintf(id, "%s##%d", shaders::at(iris, i)->get_id().c_str(), i);

            TableSetColumnIndex(0);
            if (SmallButton(del)) {
                iris->shader_passes.erase(iris->shader_passes.begin() + i);

                break;
            } SameLine();

            if (SmallButton(bypass)) {
                shaders::at(iris, i)->bypass = !shaders::at(iris, i)->bypass;
            } SameLine();

            Selectable(id, false, ImGuiSelectableFlags_SpanAllColumns);

            if (BeginDragDropSource()) {
                SetDragDropPayload("SHADER_DND_PAYLOAD", &i, sizeof(int));

                EndDragDropSource();
            }

            if (BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = AcceptDragDropPayload("SHADER_DND_PAYLOAD")) {
                    int src = *(int*)payload->Data;

                    shaders::swap(iris, src, i);
                }

                EndDragDropTarget();
            }
        }

        EndTable();
    }
}

void show_settings(iris::instance* iris) {
    using namespace ImGui;

    hovered = false;

    static const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking;

    SetNextWindowSize(ImVec2(650, 560), ImGuiCond_FirstUseEver);
    PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(650, 560));

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
                case 0: show_system_settings(iris); break;
                case 1: show_graphics_settings(iris); break;
                case 2: show_shader_settings(iris); break;
                case 3: show_paths_settings(iris); break;
                case 4: show_memory_card_settings(iris); break;
                case 5: show_misc_settings(iris); break;
            }
        } EndChild();

        Separator();

        if (hovered) {
            TextWrapped(tooltip.c_str());
        } else {
            Text("Hover over an item to get more information");
        }
    } End();

    PopStyleVar();
}

}