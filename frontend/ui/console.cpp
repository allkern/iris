#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

static const logger::Level level_values[] = {
    logger::LEVEL_DEBUG,
    logger::LEVEL_INFO,
    logger::LEVEL_OK,
    logger::LEVEL_WARNING,
    logger::LEVEL_ERROR,
    logger::LEVEL_FATAL_ERROR
};

static bool contains_ci(const std::string& haystack, const char* needle) {
    if (!needle[0])
        return true;

    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle, needle + strlen(needle),
        [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); }
    );

    return it != haystack.end();
}

void Console::rebuild_sources() {
    const std::vector <logger::Source>& list = logger::get_sources(iris->logger);

    sources.resize(list.size(), true);

    std::vector <size_t> order(list.size());

    for (size_t i = 0; i < list.size(); i++)
        order[i] = i;

    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return list[a].name < list[b].name;
    });

    source_entries.clear();

    const std::vector <size_t>& frontend = iris->frontend_log_sources;

    for (size_t i : order) {
        if (source_entries.empty() || source_entries.back().name != list[i].name)
            source_entries.push_back({ list[i].name, {}, false });

        source_entries.back().ids.push_back(i);

        if (std::find(frontend.begin(), frontend.end(), i) != frontend.end())
            source_entries.back().frontend = true;
    }
}

void Console::on_render() {
    using namespace ImGui;

    const std::vector <logger::Source>& source_list = logger::get_sources(iris->logger);

    if (sources.size() != source_list.size())
        rebuild_sources();

    if (BeginMenuBar()) {
        if (imgui::BeginMenu("View")) {
            imgui::MenuItem(ICON_MS_ARROW_DOWNWARD " Autoscroll", nullptr, &autoscroll);
            imgui::MenuItem(ICON_MS_PALETTE " Colorize", nullptr, &colorize);
            imgui::MenuItem(ICON_MS_WRAP_TEXT " Wrap long lines", nullptr, &wrap);

            Separator();

            imgui::MenuItem(ICON_MS_TERMINAL " Mirror to system console", nullptr, &iris->log_to_console);

            ImGui::EndMenu();
        }

        if (imgui::BeginMenu("Level")) {
            SeparatorText("Show");

            for (int i = 0; i < IM_ARRAYSIZE(level_values); i++) {
                PushStyleColor(ImGuiCol_Text, log_level_color(level_values[i]));

                std::string label = fmt::format("{}##show", log_level_name(level_values[i]));

                if (imgui::MenuItem(label.c_str(), nullptr, min_level == i))
                    min_level = i;

                PopStyleColor();
            }

            SeparatorText("Capture");

            for (logger::Level level : level_values) {
                PushStyleColor(ImGuiCol_Text, log_level_color(level));

                std::string label = fmt::format("{}##capture", log_level_name(level));

                if (imgui::MenuItem(label.c_str(), nullptr, iris->log_level == level)) {
                    iris->log_level = level;

                    logger::set_level(iris->logger, level);
                }

                PopStyleColor();
            }

            ImGui::EndMenu();
        }

        if (imgui::BeginMenu("Sources")) {
            size_t enabled = 0;

            for (const SourceEntry& entry : source_entries)
                enabled += sources[entry.ids[0]] ? 1 : 0;

            SetNextItemWidth(220.0f);
            InputTextWithHint("##srcsearch", ICON_MS_SEARCH " Filter sources...", source_search, sizeof(source_search));

            SameLine();

            if (Button(ICON_MS_SELECT " All"))
                std::fill(sources.begin(), sources.end(), true);

            SameLine();

            if (Button(ICON_MS_REMOVE_SELECTION " None"))
                std::fill(sources.begin(), sources.end(), false);

            SameLine();

            AlignTextToFramePadding();
            TextDisabled("%zu / %zu", enabled, source_entries.size());

            Separator();

            PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 3.0f));
            PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 3.0f));
            PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 1.0f));

            if (BeginChild("##srclist", ImVec2(420.0f, 280.0f))) {
                auto matches = [&](const SourceEntry& entry, bool frontend) {
                    return entry.frontend == frontend && contains_ci(entry.name, source_search);
                };

                auto draw_group = [&](const char* label, bool frontend) {
                    size_t total = 0;
                    size_t on = 0;

                    for (const SourceEntry& entry : source_entries) {
                        if (!matches(entry, frontend))
                            continue;

                        total++;
                        on += sources[entry.ids[0]] ? 1 : 0;
                    }

                    if (!total)
                        return;

                    std::string header = fmt::format("{} ({}/{})###{}", label, on, total, label);

                    if (!TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        return;

                    if (Button(fmt::format("All##{}", label).c_str()))
                        for (const SourceEntry& entry : source_entries)
                            if (matches(entry, frontend))
                                for (size_t id : entry.ids)
                                    sources[id] = true;

                    SameLine();

                    if (Button(fmt::format("None##{}", label).c_str()))
                        for (const SourceEntry& entry : source_entries)
                            if (matches(entry, frontend))
                                for (size_t id : entry.ids)
                                    sources[id] = false;

                    PushFont(iris->ui.font_code);

                    if (BeginTable(label, 2)) {
                        for (const SourceEntry& entry : source_entries) {
                            if (!matches(entry, frontend))
                                continue;

                            std::string name = entry.name;

                            if (entry.ids.size() > 1)
                                name += fmt::format(" ({})", entry.ids.size());

                            bool checked = sources[entry.ids[0]];

                            TableNextColumn();

                            if (Checkbox(name.c_str(), &checked))
                                for (size_t id : entry.ids)
                                    sources[id] = checked;
                        }

                        EndTable();
                    }

                    PopFont();

                    TreePop();
                };

                draw_group("Frontend", true);
                draw_group("Core", false);
            } EndChild();

            PopStyleVar(3);

            ImGui::EndMenu();
        }

        EndMenuBar();
    }

    SetNextItemWidth(240.0f);
    InputTextWithHint("##search", ICON_MS_SEARCH " Filter...", search, sizeof(search));

    SameLine();

    bool copy = Button(ICON_MS_CONTENT_COPY " Copy");

    SameLine();

    bool clear = Button(ICON_MS_DELETE " Clear");

    SameLine();

    std::string copy_buf;

    std::lock_guard <std::mutex> lock(iris->log_mutex);

    if (clear)
        iris->log_history.clear();

    std::vector <size_t> visible;

    visible.reserve(iris->log_history.size());

    for (size_t i = 0; i < iris->log_history.size(); i++) {
        const LogEntry& entry = iris->log_history[i];

        if ((int)entry.level < min_level)
            continue;

        if (entry.source < sources.size() && !sources[entry.source])
            continue;

        if (!contains_ci(entry.text, search))
            continue;

        visible.push_back(i);
    }

    TextDisabled("%zu / %zu", visible.size(), iris->log_history.size());

    if (copy) {
        for (size_t i : visible) {
            const LogEntry& entry = iris->log_history[i];

            copy_buf += log_level_name(entry.level);
            copy_buf += ' ';
            copy_buf += source_list[entry.source].name;
            copy_buf += "  ";
            copy_buf += entry.text;
            copy_buf += '\n';
        }

        SDL_SetClipboardText(copy_buf.c_str());
    }

    ImGuiTableFlags flags =
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingFixedFit;

    if (!wrap)
        flags |= ImGuiTableFlags_ScrollX;

    if (BeginTable("##logtable", 3, flags)) {
        TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed);
        TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed);
        TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

        PushFont(iris->ui.font_code);

        auto draw_row = [&](size_t index) {
            const LogEntry& entry = iris->log_history[index];

            TableNextRow();

            TableSetColumnIndex(0);

            if (colorize) {
                TextColored(log_level_color(entry.level), "%s", log_level_name(entry.level));
            } else {
                TextUnformatted(log_level_name(entry.level));
            }

            TableSetColumnIndex(1);
            TextDisabled("%s", source_list[entry.source].name.c_str());

            TableSetColumnIndex(2);

            bool tint = colorize && entry.level >= logger::LEVEL_WARNING;

            if (tint)
                PushStyleColor(ImGuiCol_Text, log_level_color(entry.level));

            if (wrap) {
                TextWrapped("%s", entry.text.c_str());
            } else {
                TextUnformatted(entry.text.c_str());
            }

            if (tint)
                PopStyleColor();
        };

        // Wrapped rows have no uniform height, so we can't use the clippers
        if (wrap) {
            for (size_t i : visible)
                draw_row(i);
        } else {
            ImGuiListClipper clipper;

            clipper.Begin((int)visible.size());

            while (clipper.Step())
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    draw_row(visible[i]);
        }

        PopFont();

        if (autoscroll && GetScrollY() >= GetScrollMaxY())
            SetScrollHereY(1.0f);

        EndTable();
    }
}

}
