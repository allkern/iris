/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <filesystem>
#include <string>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

struct ExitPrompt {
    bool pending = false;
    bool was_paused = false;
    bool dont_ask = false;
};

struct UnknownFilePrompt {
    bool pending = false;
    bool dont_ask = false;

    std::string path = "";
};

static ExitPrompt exit_prompt;
static UnknownFilePrompt unknown_file_prompt;

void request_exit_game(Instance* iris) {
    if (!iris->ui.confirm_exit || (iris->loaded.empty() && iris->arcade_id.empty())) {
        emu::close_game(iris);

        return;
    }

    exit_prompt.was_paused = iris->debug.pause;
    exit_prompt.dont_ask = false;
    exit_prompt.pending = true;

    iris->debug.pause = true;
}

void request_open_unknown_file(Instance* iris, std::string path) {
    unknown_file_prompt.path = std::move(path);
    unknown_file_prompt.dont_ask = false;
    unknown_file_prompt.pending = true;
}

static bool begin_prompt(const char* title) {
    using namespace ImGui;

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    if (GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable && !GetIO().ConfigViewportsNoDecoration)
        flags |= ImGuiWindowFlags_NoTitleBar;

    ImGuiWindowClass window_class;

    window_class.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;

    SetNextWindowClass(&window_class);
    SetNextWindowPos(GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    return BeginPopupModal(title, nullptr, flags);
}

static void focus_default_button() {
    using namespace ImGui;

    SetItemDefaultFocus();

    if (IsWindowAppearing())
        SetNavCursorVisible(true);
}

static bool cancel_pressed() {
    using namespace ImGui;

    return !IsWindowAppearing() && IsKeyPressed(ImGuiKey_Escape, false);
}

static void draw_exit_prompt(Instance* iris) {
    using namespace ImGui;

    if (exit_prompt.pending) {
        OpenPopup("###exitprompt");

        exit_prompt.pending = false;
    }

    if (!begin_prompt("Exit game###exitprompt"))
        return;

    PushFont(iris->ui.font_heading);
    TextUnformatted("Exit game?");
    PopFont();

    Spacing();

    TextUnformatted(iris->library.show_on_startup || iris->ui.show_library_saved ?
        "The game will be closed and you'll return to the game library." :
        "The game will be closed."
    );

    Spacing();
    TextDisabled(ICON_MS_WARNING " Any unsaved progress will be lost.");
    Spacing();

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Don't ask me again", &exit_prompt.dont_ask);
    PopStyleVar();

    Spacing();
    Separator();
    Spacing();

    if (Button("Exit")) {
        if (exit_prompt.dont_ask)
            iris->ui.confirm_exit = false;

        emu::close_game(iris);

        CloseCurrentPopup();
    }

    focus_default_button(); SameLine();

    if (Button("Cancel") || cancel_pressed()) {
        iris->debug.pause = exit_prompt.was_paused;

        CloseCurrentPopup();
    }

    EndPopup();
}

static void draw_unknown_file_prompt(Instance* iris) {
    using namespace ImGui;

    if (unknown_file_prompt.pending) {
        OpenPopup("###unknownfileprompt");

        unknown_file_prompt.pending = false;
    }

    if (!begin_prompt("Unrecognized file###unknownfileprompt"))
        return;

    PushFont(iris->ui.font_heading);
    TextUnformatted("Unrecognized file");
    PopFont();

    Spacing();

    TextUnformatted(std::filesystem::path(unknown_file_prompt.path).filename().string().c_str());

    Spacing();

    TextUnformatted("Iris doesn't recognize the format of this file and will treat it as a plain binary file.");

    Spacing();

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Don't ask me again", &unknown_file_prompt.dont_ask);
    PopStyleVar();

    Spacing();
    Separator();
    Spacing();

    if (Button("Open anyway")) {
        if (unknown_file_prompt.dont_ask)
            iris->ui.warn_unknown_files = false;

        if (emu::open_file_unchecked(iris, unknown_file_prompt.path))
            push_info(iris, "Failed to open file: " + unknown_file_prompt.path);

        CloseCurrentPopup();
    }

    focus_default_button(); SameLine();

    if (Button("Cancel") || cancel_pressed())
        CloseCurrentPopup();

    EndPopup();
}

void show_prompts(Instance* iris) {
    draw_exit_prompt(iris);
    draw_unknown_file_prompt(iris);
}

}
