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

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "imgui_internal.h"

#include "iris.hpp"
#include "imgui.hpp"
#include "TextEditor.h"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"

namespace iris {

enum {
    PENDING_NONE,
    PENDING_NEW,
    PENDING_OPEN
};

struct ApiEntry {
    const char* signature;
    const char* help;
};

static const ApiEntry api_entries[] = {
    { "iris.wait(frames)", "Pause the script body for a number of frames" },
    { "iris.wait_until(fn, timeout)", "Pause until fn returns true or timeout frames pass, returns whether fn did" },
    { "iris.frame()", "Frames since the script started" },
    { "iris.on_frame(fn)", "Call fn at the start of every frame" },
    { "iris.screenshot(name)", "Save a screenshot once this frame is drawn" },
    { "iris.log(...)", "Write to the log, print does the same" },
    { "iris.quit(ok)", "Close Iris after this frame, exiting with 1 when ok is false" },
    { "iris.mem.read8/16/32/64/f(addr)", "Read EE RAM or the scratchpad" },
    { "iris.mem.write8/16/32/64/f(addr, v)", "Write EE RAM or the scratchpad" },
    { "iris.mem.read_string(addr, max)", "Read a NUL-terminated string" },
    { "iris.pad.press(port, ...)", "Hold buttons until they are released" },
    { "iris.pad.release(port, ...)", "Release held buttons" },
    { "iris.pad.tap(port, button, frames)", "Hold a button for a few frames, 4 by default" },
    { "iris.pad.axis(port, axis, value)", "Override lx, ly, rx or ry with 0-255, nil restores it" },
    { "iris.pad.clear(port)", "Release everything on a port" }
};

static const char* const button_list = "select l3 r3 start up right down left l2 r2 l1 r1 triangle circle cross square";

ScriptEditor::ScriptEditor() {
    id = "script_editor";
    title = "Script Editor";
    flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}

ScriptEditor::~ScriptEditor() = default;

static std::string display_name(ScriptEditor* e) {
    if (e->path.empty())
        return "Untitled";

    return std::filesystem::path(e->path).filename().string();
}

static bool is_dirty(ScriptEditor* e) {
    return e->editor->GetUndoIndex() != e->saved_undo_index;
}

static std::string first_line(const std::string& text) {
    return text.substr(0, text.find('\n'));
}

static size_t error_line(const std::string& error) {
    std::string line = first_line(error);

    for (size_t pos = line.find(':'); pos != std::string::npos; pos = line.find(':', pos + 1)) {
        size_t end = pos + 1;

        while (end < line.size() && isdigit((unsigned char)line[end])) {
            end++;
        }

        if (end > pos + 1 && end < line.size() && line[end] == ':') {
            return strtoul(line.c_str() + pos + 1, nullptr, 10);
        }
    }

    return 0;
}

static void reset_document(ScriptEditor* e, const std::string& text, const std::string& path) {
    e->editor->SetText(text);
    e->editor->ClearMarkers();

    e->path = path;
    e->marked_error = "";
    e->saved_undo_index = e->editor->GetUndoIndex();
}

static void new_file(ScriptEditor* e) {
    reset_document(e, "", "");
}

static void open_file(ScriptEditor* e) {
    audio::mute(e->iris);

    auto f = pfd::open_file("Open script", "", {
        "Lua Scripts (*.lua)", "*.lua",
        "All Files (*.*)", "*"
    });

    while (!f.ready());

    audio::unmute(e->iris);

    if (f.result().empty())
        return;

    std::string path = f.result().at(0);
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        push_info(e->iris, "Couldn't open " + path);

        return;
    }

    std::string text((std::istreambuf_iterator <char>(file)), std::istreambuf_iterator <char>());

    reset_document(e, text, path);
}

static bool save_file(ScriptEditor* e, bool save_as) {
    std::string path = e->path;

    if (path.empty() || save_as) {
        audio::mute(e->iris);

        auto f = pfd::save_file("Save script", display_name(e) == "Untitled" ? "script.lua" : display_name(e), {
            "Lua Scripts (*.lua)", "*.lua",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(e->iris);

        path = f.result();

        if (path.empty())
            return false;
    }

    std::ofstream file(path, std::ios::binary);

    if (!file) {
        push_info(e->iris, "Couldn't save " + path);

        return false;
    }

    std::string text = e->editor->GetText();

    file.write(text.data(), text.size());

    e->path = path;
    e->saved_undo_index = e->editor->GetUndoIndex();

    return true;
}

static void perform(ScriptEditor* e, int action) {
    switch (action) {
        case PENDING_NEW: new_file(e); break;
        case PENDING_OPEN: open_file(e); break;
    }
}

static void request(ScriptEditor* e, int action) {
    if (!is_dirty(e)) {
        perform(e, action);

        return;
    }

    e->pending = action;
    e->open_pending = true;
}

static bool is_ours(ScriptEditor* e) {
    return script::get_name(e->iris) == display_name(e);
}

static void run(ScriptEditor* e) {
    e->editor->ClearMarkers();
    e->marked_error = "";

    script::load_source(e->iris, e->editor->GetText(), display_name(e));
}

static void stop(ScriptEditor* e) {
    script::unload(e->iris);
}

static void update_palette(ScriptEditor* e) {
    ImU32 bg = ImGui::GetColorU32(ImGuiCol_WindowBg);

    if (bg == e->palette_bg)
        return;

    e->palette_bg = bg;

    ImVec4 c = ImGui::ColorConvertU32ToFloat4(bg);

    float luma = 0.299f * c.x + 0.587f * c.y + 0.114f * c.z;

    TextEditor::Palette palette = luma < 0.5f ? TextEditor::GetDarkPalette() : TextEditor::GetLightPalette();

    palette[(size_t)TextEditor::Color::background] = bg;

    e->editor->SetPalette(palette);
}

static void update_error_marker(ScriptEditor* e) {
    if (script::get_state(e->iris) != script::STATE_FAILED || !is_ours(e))
        return;

    std::string error = script::get_error(e->iris);

    if (error == e->marked_error)
        return;

    e->marked_error = error;
    e->editor->ClearMarkers();

    size_t line = error_line(error);

    if (!line)
        return;

    std::string message = first_line(error);

    e->editor->AddMarker(line - 1, IM_COL32(255, 90, 90, 255), IM_COL32(255, 60, 60, 48), message, message);
}

static void handle_shortcuts(ScriptEditor* e) {
    using namespace ImGui;

    if (Shortcut(ImGuiMod_Ctrl | ImGuiKey_N)) request(e, PENDING_NEW);
    if (Shortcut(ImGuiMod_Ctrl | ImGuiKey_O)) request(e, PENDING_OPEN);
    if (Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) save_file(e, false);
    if (Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S)) save_file(e, true);
    if (Shortcut(ImGuiKey_F5)) run(e);
    if (Shortcut(ImGuiMod_Shift | ImGuiKey_F5)) stop(e);
}

static void show_menu_bar(ScriptEditor* e) {
    using namespace ImGui;

    TextEditor* editor = e->editor.get();

    if (!BeginMenuBar())
        return;

    if (imgui::BeginMenu("File")) {
        if (imgui::MenuItem(ICON_MS_NOTE_ADD " New", "Ctrl+N")) request(e, PENDING_NEW);
        if (imgui::MenuItem(ICON_MS_FOLDER_OPEN " Open...", "Ctrl+O")) request(e, PENDING_OPEN);

        Separator();

        if (imgui::MenuItem(ICON_MS_SAVE " Save", "Ctrl+S")) save_file(e, false);
        if (imgui::MenuItem(ICON_MS_SAVE_AS " Save As...", "Ctrl+Shift+S")) save_file(e, true);

        Separator();

        if (imgui::MenuItem(ICON_MS_CLOSE " Close")) e->open = false;

        ImGui::EndMenu();
    }

    if (imgui::BeginMenu("Edit")) {
        if (imgui::MenuItem(ICON_MS_UNDO " Undo", "Ctrl+Z", false, editor->CanUndo())) editor->Undo();
        if (imgui::MenuItem(ICON_MS_REDO " Redo", "Ctrl+Y", false, editor->CanRedo())) editor->Redo();

        Separator();

        if (imgui::MenuItem(ICON_MS_CONTENT_CUT " Cut", "Ctrl+X", false, editor->AnyCursorHasSelection())) editor->Cut();
        if (imgui::MenuItem(ICON_MS_CONTENT_COPY " Copy", "Ctrl+C", false, editor->AnyCursorHasSelection())) editor->Copy();
        if (imgui::MenuItem(ICON_MS_CONTENT_PASTE " Paste", "Ctrl+V")) editor->Paste();
        if (imgui::MenuItem(ICON_MS_SELECT_ALL " Select All", "Ctrl+A")) editor->SelectAll();

        Separator();

        if (imgui::MenuItem(ICON_MS_SEARCH " Find/Replace", "Ctrl+F")) editor->OpenFindReplaceWindow();
        if (imgui::MenuItem(ICON_MS_CODE " Toggle Comment", "Ctrl+/")) editor->ToggleComments();
        if (imgui::MenuItem(ICON_MS_FORMAT_INDENT_INCREASE " Indent", "Ctrl+]")) editor->IndentLines();
        if (imgui::MenuItem(ICON_MS_FORMAT_INDENT_DECREASE " Outdent", "Ctrl+[")) editor->DeindentLines();

        ImGui::EndMenu();
    }

    if (imgui::BeginMenu("View")) {
        bool line_numbers = editor->IsShowLineNumbersEnabled();
        bool whitespace = editor->IsShowWhitespacesEnabled();
        bool mini_map = editor->IsShowMiniMapEnabled();
        bool word_wrap = editor->IsWordWrapEnabled();
        bool folding = editor->IsLineFoldingEnabled();

        if (imgui::MenuItem("Line Numbers", nullptr, &line_numbers)) editor->SetShowLineNumbersEnabled(line_numbers);
        if (imgui::MenuItem("Whitespace", nullptr, &whitespace)) editor->SetShowWhitespacesEnabled(whitespace);
        if (imgui::MenuItem("Mini Map", nullptr, &mini_map)) editor->SetShowMiniMapEnabled(mini_map);
        if (imgui::MenuItem("Word Wrap", nullptr, &word_wrap)) editor->SetWordWrapEnabled(word_wrap);
        if (imgui::MenuItem("Code Folding", nullptr, &folding)) editor->SetLineFoldingEnabled(folding);

        ImGui::EndMenu();
    }

    if (imgui::BeginMenu("Script")) {
        bool running = script::get_state(e->iris) == script::STATE_RUNNING && is_ours(e);

        if (imgui::MenuItem(running ? ICON_MS_RESTART_ALT " Restart" : ICON_MS_PLAY_ARROW " Run", "F5")) run(e);
        if (imgui::MenuItem(ICON_MS_STOP " Stop", "Shift+F5", false, script::is_loaded(e->iris))) stop(e);

        Separator();

        if (imgui::MenuItem(ICON_MS_TERMINAL " Show Console")) e->iris->applets.console.show();

        ImGui::EndMenu();
    }

    if (imgui::BeginMenu("Help")) {
        if (imgui::MenuItem(ICON_MS_HELP " Scripting API")) e->open_api = true;

        ImGui::EndMenu();
    }

    EndMenuBar();
}

static void show_toolbar(ScriptEditor* e) {
    using namespace ImGui;

    bool running = script::get_state(e->iris) == script::STATE_RUNNING && is_ours(e);

    if (Button(running ? ICON_MS_RESTART_ALT : ICON_MS_PLAY_ARROW, ImVec2(60.0f, 0.0f))) {
        run(e);
    }

    SameLine();

    BeginDisabled(!script::is_loaded(e->iris));

    if (Button(ICON_MS_STOP)) {
        stop(e);
    }

    EndDisabled();
}

static void show_script_state(ScriptEditor* e) {
    using namespace ImGui;

    Instance* iris = e->iris;

    switch (script::get_state(iris)) {
        case script::STATE_IDLE: {
            TextDisabled(ICON_MS_CIRCLE " Idle");
        } break;

        case script::STATE_RUNNING: {
            if (is_ours(e)) {
                TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ICON_MS_PLAY_ARROW " Running");
            } else {
                std::string name = std::filesystem::path(script::get_name(iris)).filename().string();

                TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ICON_MS_PLAY_ARROW " Running %s", name.c_str());
            }

            Text("Frame %llu", (unsigned long long)script::get_frame(iris));
        } break;

        case script::STATE_DONE: {
            TextDisabled(ICON_MS_CHECK_CIRCLE " Finished at frame %llu", (unsigned long long)script::get_frame(iris));
        } break;

        case script::STATE_FAILED: {
            std::string error = script::get_error(iris);

            PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

            Text(ICON_MS_ERROR);

            bool clicked = Selectable(first_line(error).c_str(), false, 0, CalcTextSize(first_line(error).c_str()));

            PopStyleColor();

            SetItemTooltip("%s", error.c_str());

            size_t line = error_line(error);

            if (clicked && line && is_ours(e)) {
                e->editor->SetCursor(TextEditor::DocPos(line - 1, 0));
                e->editor->SetFocus();
            }
        } break;
    }
}

static size_t count_glyphs(const std::string& text) {
    size_t count = 0;

    for (unsigned char c : text) {
        if ((c & 0xc0) != 0x80)
            count++;
    }

    return count;
}

static void show_status_bar(ScriptEditor* e) {
    using namespace ImGui;

    TextEditor* editor = e->editor.get();

    if (imgui::BeginStatusBar()) {
        ImVec4 col = GetStyleColorVec4(ImGuiCol_Text);

        col.w = 0.2f;

        PushStyleColor(ImGuiCol_Separator, col);
        PushStyleVarX(ImGuiStyleVar_ItemSpacing, 4.0f);

        float right_x = GetWindowWidth() - e->status_width - 5.0f;
        ImVec2 window_pos = GetWindowPos();

        PushClipRect(window_pos, ImVec2(window_pos.x + right_x - 8.0f, window_pos.y + GetWindowHeight()), true);

        SetCursorPosX(5.0f);

        show_script_state(e);

        PopClipRect();

        SetCursorPosX(right_x);

        TextEditor::DocPos pos = editor->GetMainCursorPosition();

        size_t column = editor->IsWordWrapEnabled() ? pos.index : editor->DocPos2VisPos(pos).column;

        if (editor->GetNumberOfCursors() > 1) {
            Text("%zu cursors", editor->GetNumberOfCursors());
        } else if (editor->MainCursorHasSelection()) {
            Text("Ln %zu, Col %zu (%zu selected)", pos.line + 1, column + 1, count_glyphs(editor->GetCursorText(0)));
        } else {
            Text("Ln %zu, Col %zu", pos.line + 1, column + 1);
        }

        SeparatorEx(ImGuiSeparatorFlags_Vertical);

        if (editor->IsInsertSpacesOnTabs()) {
            Text("Spaces: %zu", editor->GetTabSize());
        } else {
            Text("Tab Size: %zu", editor->GetTabSize());
        }

        SeparatorEx(ImGuiSeparatorFlags_Vertical);
        Text("UTF-8");
        SeparatorEx(ImGuiSeparatorFlags_Vertical);
        Text("Lua");

        e->status_width = GetItemRectMax().x - (window_pos.x + right_x);

        PopStyleVar();
        PopStyleColor();

        imgui::EndStatusBar();
    }
}

static void show_unsaved_popup(ScriptEditor* e) {
    using namespace ImGui;

    if (e->open_pending) {
        OpenPopup("Unsaved changes##script_editor");

        e->open_pending = false;
    }

    if (!BeginPopupModal("Unsaved changes##script_editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    Text("Save changes to %s?", display_name(e).c_str());

    Spacing();

    int action = e->pending;

    if (Button("Save")) {
        CloseCurrentPopup();

        e->pending = PENDING_NONE;

        if (save_file(e, false)) {
            perform(e, action);
        }
    }

    SameLine();

    if (Button("Don't Save")) {
        CloseCurrentPopup();

        e->pending = PENDING_NONE;

        perform(e, action);
    }

    SameLine();

    if (Button("Cancel") || IsKeyPressed(ImGuiKey_Escape)) {
        CloseCurrentPopup();

        e->pending = PENDING_NONE;
    }

    EndPopup();
}

static void show_api_popup(ScriptEditor* e) {
    using namespace ImGui;

    if (e->open_api) {
        OpenPopup("Scripting API##script_editor");

        e->open_api = false;
    }

    bool open = true;

    if (!BeginPopupModal("Scripting API##script_editor", &open, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (BeginTable("##api", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        for (const ApiEntry& entry : api_entries) {
            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(e->iris->ui.font_code);
            Text("%s", entry.signature);
            PopFont();

            TableSetColumnIndex(1);
            Text("%s", entry.help);
        }

        EndTable();
    }

    Spacing();

    TextDisabled("Ports are 1 and 2. Buttons: %s", button_list);

    EndPopup();
}

void ScriptEditor::on_init() {
    editor = std::make_unique <TextEditor>();

    editor->SetLanguage(TextEditor::Language::Lua());
    editor->SetTabSize(4);
    editor->SetInsertSpacesOnTabs(true);

    saved_undo_index = editor->GetUndoIndex();
}

bool ScriptEditor::begin() {
    window_title = display_name(this) + (is_dirty(this) ? " *" : "") + " - Script Editor###script_editor";

    return imgui::BeginEx(window_title.c_str(), &open, flags);
}

void ScriptEditor::on_render() {
    using namespace ImGui;

    update_palette(this);
    update_error_marker(this);
    handle_shortcuts(this);

    show_menu_bar(this);
    show_toolbar(this);

    float status_top = GetWindowPos().y + GetWindowHeight() - GetFrameHeight();
    float editor_height = status_top - GetStyle().ItemSpacing.y - GetCursorScreenPos().y;

    PushFont(iris->ui.font_code);

    editor->Render("##editor", ImVec2(0.0f, ImMax(editor_height, 1.0f)));

    PopFont();

    show_status_bar(this);
    show_unsaved_popup(this);
    show_api_popup(this);
}

}
