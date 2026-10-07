#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "iris.hpp"

#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "portable-file-dialogs.h"
#include "res/IconsMaterialSymbols.h"

namespace iris {

using library::Entry;

struct LibraryView {
    std::string search;
    std::string applied_search;
    std::vector <int> order;
    uint64_t generation = UINT64_MAX;
    int applied_sort = -1;
    int applied_filter = -1;
    std::string focused_path;
    int focused = -1;
    bool scroll_to_focused = false;
    int columns = 1;
    float row_height = 0.0f;
};

static LibraryView library_view;

static const char* const sort_names[] = {
    "Title",
    "Recently played",
    "Region",
    "System"
};

static const char* const filter_names[] = {
    "All",
    "PS2",
    "Arcade"
};

static const float grid_sizes[] = {
    130.0f,
    155.0f,
    180.0f,
    215.0f,
    255.0f
};

static const char* const grid_size_names[] = {
    "Tiny",
    "Small",
    "Medium",
    "Large",
    "Huge"
};

static const char* const view_names[] = {
    ICON_MS_VIEW_LIST,
    ICON_MS_GRID_VIEW
};

static std::string to_lower(std::string text) {
    for (char& c : text) {
        c = (char)tolower((unsigned char)c);
    }

    return text;
}

static ImVec4 with_alpha(ImVec4 color, float alpha) {
    color.w *= alpha;

    return color;
}

static ImVec4 accent_color() {
    return ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
}

static ImVec4 region_color(const std::string& region) {
    if (region == "NTSC-U") return imgui::BADGE_BLUE;
    if (region == "PAL") return imgui::BADGE_EMERALD;
    if (region == "NTSC-J") return imgui::BADGE_RED;
    if (region == "NTSC-K") return imgui::BADGE_AMBER;

    return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
}

static ImVec4 format_color(const std::string& format) {
    if (format == "ISO") return imgui::BADGE_GREEN;
    if (format == "CHD") return imgui::BADGE_VIOLET;
    if (format == "CSO" || format == "ZSO") return imgui::BADGE_ORANGE;
    if (format == "BIN" || format == "CUE") return imgui::BADGE_SKY;

    return imgui::BADGE_PINK;
}

static float draw_badge(ImDrawList* dl, float right, float center_y, const char* text, const ImVec4& color) {
    ImVec2 size = imgui::badge_size(text);
    ImVec2 pos = ImVec2(right - size.x, center_y - size.y * 0.5f);

    imgui::badge(dl, pos, text, color);

    return pos.x;
}

static uint32_t hash_text(const std::string& text) {
    uint32_t hash = 2166136261u;

    for (char c : text) {
        hash ^= (uint8_t)c;
        hash *= 16777619u;
    }

    return hash;
}

static std::string initials(const std::string& title) {
    std::string result;

    bool word_start = true;

    for (char c : title) {
        if (isalnum((unsigned char)c)) {
            if (word_start) {
                result += (char)toupper((unsigned char)c);

                if (result.size() == 2) {
                    break;
                }
            }

            word_start = false;
        } else if (c == ' ' || c == '-' || c == ':') {
            word_start = true;
        }
    }

    return result.size() ? result : "?";
}

static std::string release_year(const Entry& entry) {
    if (entry.release_date.size() < 4) {
        return "";
    }

    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)entry.release_date[i])) {
            return "";
        }
    }

    return entry.release_date.substr(0, 4);
}

static std::string played_text(int64_t time) {
    if (!time) {
        return "";
    }

    int64_t delta = (int64_t)std::time(nullptr) - time;

    if (delta < 60) {
        return "Played just now";
    }

    auto plural = [](int64_t count, const char* unit) {
        return fmt::format("Played {} {}{} ago", count, unit, count == 1 ? "" : "s");
    };

    if (delta < 60 * 60) {
        return plural(delta / 60, "minute");
    }

    if (delta < 24 * 60 * 60) {
        return plural(delta / (60 * 60), "hour");
    }

    if (delta < 7 * 24 * 60 * 60) {
        return plural(delta / (24 * 60 * 60), "day");
    }

    std::time_t t = (std::time_t)time;
    std::tm* tm = std::localtime(&t);

    if (!tm) {
        return "";
    }

    char buf[64];

    std::strftime(buf, sizeof(buf), "Played %b %d, %Y", tm);

    return buf;
}

static std::string subtitle(const Entry& entry) {
    std::string text = entry.system;

    if (entry.serial.size()) {
        text += " \xe2\x80\xa2 " + entry.serial;
    }

    std::string year = release_year(entry);

    if (year.size()) {
        text += " \xe2\x80\xa2 " + year;
    }

    std::string played = played_text(entry.last_played);

    if (played.size()) {
        text += " \xe2\x80\xa2 " + played;
    }

    return text;
}

static bool matches(const Entry& entry, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }

    if (to_lower(entry.title).find(needle) != std::string::npos) {
        return true;
    }

    if (to_lower(entry.serial).find(needle) != std::string::npos) {
        return true;
    }

    std::string filename = std::filesystem::path(entry.path).filename().string();

    return to_lower(filename).find(needle) != std::string::npos;
}

static void rebuild_order(Instance* iris) {
    const std::vector <Entry>& entries = library::entries(iris);

    uint64_t generation = library::generation(iris);

    bool unchanged =
        generation == library_view.generation &&
        library_view.search == library_view.applied_search &&
        iris->library.sort == library_view.applied_sort &&
        iris->library.filter == library_view.applied_filter;

    if (unchanged) {
        return;
    }

    library_view.generation = generation;
    library_view.applied_search = library_view.search;
    library_view.applied_sort = iris->library.sort;
    library_view.applied_filter = iris->library.filter;

    std::string needle = to_lower(library_view.search);

    std::vector <std::string> titles(entries.size());

    library_view.order.clear();

    for (int i = 0; i < (int)entries.size(); i++) {
        const Entry& entry = entries[i];

        bool arcade = entry.kind == library::KIND_ARCADE;

        if (iris->library.filter == library::FILTER_PS2 && arcade) {
            continue;
        }

        if (iris->library.filter == library::FILTER_ARCADE && !arcade) {
            continue;
        }

        if (!matches(entry, needle)) {
            continue;
        }

        titles[i] = to_lower(entry.title);

        library_view.order.push_back(i);
    }

    int sort = iris->library.sort;

    std::stable_sort(library_view.order.begin(), library_view.order.end(), [&](int a, int b) {
        const Entry& ea = entries[a];
        const Entry& eb = entries[b];

        if (sort == library::SORT_RECENT && ea.last_played != eb.last_played) {
            return ea.last_played > eb.last_played;
        }

        if (sort == library::SORT_REGION && ea.region != eb.region) {
            return ea.region < eb.region;
        }

        if (sort == library::SORT_SYSTEM && ea.system != eb.system) {
            return ea.system < eb.system;
        }

        if (titles[a] != titles[b]) {
            return titles[a] < titles[b];
        }

        return ea.path < eb.path;
    });

    library_view.focused = -1;

    for (int i = 0; i < (int)library_view.order.size(); i++) {
        if (entries[library_view.order[i]].path == library_view.focused_path) {
            library_view.focused = i;

            break;
        }
    }
}

static void set_focused(Instance* iris, int index) {
    if (library_view.order.empty()) {
        library_view.focused = -1;

        return;
    }

    index = std::clamp(index, 0, (int)library_view.order.size() - 1);

    library_view.focused = index;
    library_view.focused_path = library::entries(iris)[library_view.order[index]].path;
    library_view.scroll_to_focused = true;
}

static void launch(Instance* iris, const Entry& entry) {
    if (entry.kind == library::KIND_ARCADE) {
        if (!emu::load_arcade(iris, entry.path)) {
            push_info(iris, "Failed to boot arcade: " + entry.title);

            return;
        }

        add_recent(iris, entry.path, RecentType::ARCADE);

        return;
    }

    if (emu::open_file(iris, entry.path)) {
        push_info(iris, "Failed to open file: " + entry.path);

        return;
    }

    add_recent(iris, entry.path, RecentType::PS2);
}

static void add_folder_dialog(Instance* iris) {
    audio::mute(iris);

    auto f = pfd::select_folder("Select a folder with games", "", pfd::opt::none);

    while (!f.ready());

    audio::unmute(iris);

    std::string result = f.result();

    if (result.empty()) {
        return;
    }

    auto& dirs = iris->library.dirs;

    if (std::find(dirs.begin(), dirs.end(), result) == dirs.end()) {
        dirs.push_back(result);
    }
}

static void open_file_dialog(Instance* iris) {
    audio::mute(iris);

    auto f = pfd::open_file("Select a file to load", "", {
        "All File Types (*.iso; *.bin; *.cue; *.chd; *.cso; *.zso; *.elf; *.zip)", "*.iso *.bin *.cue *.chd *.cso *.zso *.elf *.zip",
        "All Files (*.*)", "*"
    });

    while (!f.ready());

    audio::unmute(iris);

    if (f.result().empty() || f.result().at(0).empty()) {
        return;
    }

    std::string path = f.result().at(0);

    if (emu::open_file(iris, path)) {
        push_info(iris, "Failed to open file: " + path);
    } else {
        add_recent(iris, path, RecentType::PS2);
    }
}

static void open_containing_folder(const Entry& entry) {
    std::filesystem::path path(entry.path);

    std::string folder = std::filesystem::is_directory(path) ? path.string() : path.parent_path().string();

    SDL_OpenURL(folder.c_str());
}

static void draw_shadow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, float size, float strength) {
    if (getenv("IRIS_NO_SHADOW")) return;
    constexpr int CORNER_SEGMENTS = 8;
    constexpr int RINGS = 6;
    constexpr int POINTS = 4 * (CORNER_SEGMENTS + 1);

    float offset = size * 0.35f;

    min.y += offset;
    max.y += offset;

    float radius = std::min(rounding, std::min(max.x - min.x, max.y - min.y) * 0.5f);

    const ImVec2 centers[4] = {
        ImVec2(max.x - radius, max.y - radius),
        ImVec2(min.x + radius, max.y - radius),
        ImVec2(min.x + radius, min.y + radius),
        ImVec2(max.x - radius, min.y + radius)
    };

    ImVec2 base[POINTS];
    ImVec2 normals[POINTS];

    for (int corner = 0; corner < 4; corner++) {
        for (int i = 0; i <= CORNER_SEGMENTS; i++) {
            float angle = IM_PI * 0.5f * ((float)corner + (float)i / CORNER_SEGMENTS);

            int point = corner * (CORNER_SEGMENTS + 1) + i;

            normals[point] = ImVec2(std::cos(angle), std::sin(angle));
            base[point] = ImVec2(
                centers[corner].x + normals[point].x * radius,
                centers[corner].y + normals[point].y * radius
            );
        }
    }

    int vtx_count = POINTS * (RINGS + 1);
    int idx_count = (POINTS - 2) * 3 + POINTS * RINGS * 6;

    dl->PrimReserve(idx_count, vtx_count);

    ImVec2 uv = dl->_Data->TexUvWhitePixel;

    unsigned int first = dl->_VtxCurrentIdx;

    for (int ring = 0; ring <= RINGS; ring++) {
        float t = (float)ring / RINGS;
        float falloff = (1.0f - t) * (1.0f - t);
        float distance = size * t;

        ImU32 color = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, strength * falloff));

        for (int point = 0; point < POINTS; point++) {
            ImVec2 pos = ImVec2(
                base[point].x + normals[point].x * distance,
                base[point].y + normals[point].y * distance
            );

            dl->PrimWriteVtx(pos, uv, color);
        }
    }

    for (int point = 1; point < POINTS - 1; point++) {
        dl->PrimWriteIdx((ImDrawIdx)first);
        dl->PrimWriteIdx((ImDrawIdx)(first + point));
        dl->PrimWriteIdx((ImDrawIdx)(first + point + 1));
    }

    for (int ring = 0; ring < RINGS; ring++) {
        for (int point = 0; point < POINTS; point++) {
            unsigned int next = (point + 1) % POINTS;

            unsigned int a = first + ring * POINTS + point;
            unsigned int b = first + ring * POINTS + next;
            unsigned int c = a + POINTS;
            unsigned int d = b + POINTS;

            dl->PrimWriteIdx((ImDrawIdx)a);
            dl->PrimWriteIdx((ImDrawIdx)b);
            dl->PrimWriteIdx((ImDrawIdx)d);
            dl->PrimWriteIdx((ImDrawIdx)a);
            dl->PrimWriteIdx((ImDrawIdx)d);
            dl->PrimWriteIdx((ImDrawIdx)c);
        }
    }
}

static void draw_placeholder(Instance* iris, ImDrawList* dl, const Entry& entry, ImVec2 min, ImVec2 max, float rounding, float alpha) {
    using namespace ImGui;

    if (alpha <= 0.0f) {
        return;
    }

    uint32_t hash = hash_text(entry.serial.size() ? entry.serial : entry.title);

    float hue = (float)(hash % 360) / 360.0f;

    ImVec4 fill = ImVec4(0.0f, 0.0f, 0.0f, alpha);
    ImVec4 band = ImVec4(0.0f, 0.0f, 0.0f, alpha);

    ColorConvertHSVtoRGB(hue, 0.42f, 0.36f, fill.x, fill.y, fill.z);
    ColorConvertHSVtoRGB(hue, 0.50f, 0.26f, band.x, band.y, band.z);

    float width = max.x - min.x;
    float height = max.y - min.y;

    dl->AddRectFilled(min, max, GetColorU32(fill), rounding);
    dl->AddRectFilled(ImVec2(min.x, max.y - height * 0.3f), max, GetColorU32(band), rounding, ImDrawFlags_RoundCornersBottom);

    std::string text = initials(entry.title);

    float size = std::clamp(width * 0.34f, 12.0f, 64.0f);

    PushFont(iris->ui.font_black, size);

    ImVec2 text_size = CalcTextSize(text.c_str());

    dl->AddText(
        ImVec2(min.x + (width - text_size.x) * 0.5f, min.y + (height * 0.85f - text_size.y) * 0.5f),
        GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.82f * alpha)),
        text.c_str()
    );

    PopFont();

    if (width < 80.0f) {
        return;
    }

    const char* icon = entry.kind == library::KIND_ARCADE ? ICON_MS_JOYSTICK : ICON_MS_ALBUM;

    ImVec2 icon_size = CalcTextSize(icon);

    dl->AddText(
        ImVec2(min.x + (width - icon_size.x) * 0.5f, max.y - height * 0.15f - icon_size.y * 0.5f),
        GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.55f * alpha)),
        icon
    );
}

static void draw_cover(Instance* iris, ImDrawList* dl, const Entry& entry, ImVec2 min, ImVec2 max, float rounding) {
    float alpha = 0.0f;

    const Texture* texture = library::request_cover(iris, entry, &alpha);

    draw_placeholder(iris, dl, entry, min, max, rounding, texture ? 1.0f - alpha : 1.0f);

    if (texture && texture->height) {
        float box_aspect = (max.x - min.x) / (max.y - min.y);
        float tex_aspect = (float)texture->width / (float)texture->height;

        ImVec2 uv0 = ImVec2(0.0f, 0.0f);
        ImVec2 uv1 = ImVec2(1.0f, 1.0f);

        if (tex_aspect > box_aspect) {
            float crop = (1.0f - box_aspect / tex_aspect) * 0.5f;

            uv0.x = crop;
            uv1.x = 1.0f - crop;
        } else {
            float crop = (1.0f - tex_aspect / box_aspect) * 0.5f;

            uv0.y = crop;
            uv1.y = 1.0f - crop;
        }

        dl->AddImageRounded(
            (ImTextureID)(intptr_t)texture->descriptor_set,
            min, max, uv0, uv1,
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)),
            rounding
        );
    }

    dl->AddRect(min, max, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.08f)), rounding);
}

static void draw_entry_tooltip(Instance* iris, const Entry& entry) {
    using namespace ImGui;

    if (!BeginTooltip()) {
        return;
    }

    PushFont(iris->ui.font_heading);
    TextUnformatted(entry.title.c_str());
    PopFont();

    if (entry.developer.size()) {
        TextDisabled("%s", entry.developer.c_str());
    }

    if (entry.release_date.size()) {
        TextDisabled("Released %s", entry.release_date.c_str());
    }

    TextDisabled("%s", entry.path.c_str());

    EndTooltip();
}

static void draw_context_menu(Instance* iris, const Entry& entry) {
    using namespace ImGui;

    PushFont(iris->ui.font_icons);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));

    if (!BeginPopupContextItem("##context")) {
        PopStyleVar();
        PopFont();

        return;
    }

    PushFont(iris->ui.font_label);
    TextDisabled("%s", entry.title.c_str());
    PopFont();

    Separator();

    if (imgui::MenuItem(ICON_MS_PLAY_ARROW " Launch")) {
        launch(iris, entry);
    }

    if (imgui::MenuItem(ICON_MS_FOLDER_OPEN " Open containing folder")) {
        open_containing_folder(entry);
    }

    Separator();

    if (imgui::MenuItem(ICON_MS_CONTENT_COPY " Copy serial", nullptr, false, entry.serial.size())) {
        SetClipboardText(entry.serial.c_str());
    }

    if (imgui::MenuItem(ICON_MS_CONTENT_COPY " Copy path")) {
        SetClipboardText(entry.path.c_str());
    }

    if (imgui::MenuItem(ICON_MS_REFRESH " Re-download cover", nullptr, false, entry.kind != library::KIND_ARCADE && entry.serial.size())) {
        library::refresh_cover(iris, entry);
    }

    EndPopup();

    PopStyleVar();
    PopFont();
}

static bool draw_list_row(Instance* iris, const Entry& entry, int index, float height) {
    using namespace ImGui;

    float scale = iris->ui.ui_scale;

    ImDrawList* dl = GetWindowDrawList();

    ImVec2 min = GetCursorScreenPos();
    ImVec2 max = ImVec2(min.x + GetContentRegionAvail().x, min.y + height);

    PushID(index);

    bool clicked = InvisibleButton("##row", ImVec2(max.x - min.x, height));
    bool hovered = IsItemHovered();
    bool focused = library_view.focused == index;

    if (hovered && IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
        draw_entry_tooltip(iris, entry);
    }

    draw_context_menu(iris, entry);

    PopID();

    float rounding = 8.0f * scale;

    if (hovered || focused) {
        ImVec4 accent = accent_color();

        dl->AddRectFilled(min, max, GetColorU32(with_alpha(accent, hovered ? 0.12f : 0.08f)), rounding);
        dl->AddRectFilled(
            ImVec2(min.x, min.y + height * 0.25f),
            ImVec2(min.x + 3.0f * scale, max.y - height * 0.25f),
            GetColorU32(accent),
            2.0f * scale
        );
    }

    float pad = 14.0f * scale;
    float cover_height = height - 14.0f * scale;
    float cover_width = cover_height * 0.7f;

    ImVec2 cover_min = ImVec2(min.x + pad, min.y + (height - cover_height) * 0.5f);
    ImVec2 cover_max = ImVec2(cover_min.x + cover_width, cover_min.y + cover_height);

    draw_shadow(dl, cover_min, cover_max, 4.0f * scale, 4.0f * scale, 0.35f);
    draw_cover(iris, dl, entry, cover_min, cover_max, 4.0f * scale);

    float center_y = (min.y + max.y) * 0.5f;
    float right = max.x - pad;

    right = draw_badge(dl, right, center_y, entry.format.c_str(), format_color(entry.format)) - 10.0f * scale;

    if (entry.region.size()) {
        right = draw_badge(dl, right, center_y, entry.region.c_str(), region_color(entry.region)) - 10.0f * scale;
    }

    float text_x = cover_max.x + 16.0f * scale;
    float text_right = std::max(text_x + 40.0f * scale, right - 16.0f * scale);

    PushFont(iris->ui.font_heading);

    float title_height = GetFontSize();

    PopFont();

    float subtitle_height = GetFontSize();
    float gap = 3.0f * scale;
    float top = center_y - (title_height + gap + subtitle_height) * 0.5f;

    PushFont(iris->ui.font_heading);

    RenderTextEllipsis(
        dl,
        ImVec2(text_x, top),
        ImVec2(text_right, top + title_height),
        text_right,
        entry.title.c_str(), nullptr, nullptr
    );

    PopFont();

    std::string sub = subtitle(entry);

    PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled));

    RenderTextEllipsis(
        dl,
        ImVec2(text_x, top + title_height + gap),
        ImVec2(text_right, top + title_height + gap + subtitle_height),
        text_right,
        sub.c_str(), nullptr, nullptr
    );

    PopStyleColor();

    return clicked;
}

static void draw_list(Instance* iris) {
    using namespace ImGui;

    const std::vector <Entry>& entries = library::entries(iris);

    float scale = iris->ui.ui_scale;
    float height = 74.0f * scale;
    float spacing = 4.0f * scale;

    library_view.columns = 1;
    library_view.row_height = height + spacing;

    PushStyleVarY(ImGuiStyleVar_ItemSpacing, spacing);

    ImGuiListClipper clipper;

    clipper.Begin((int)library_view.order.size(), height + spacing);

    if (library_view.focused >= 0) {
        clipper.IncludeItemByIndex(library_view.focused);
    }

    int launched = -1;

    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            if (draw_list_row(iris, entries[library_view.order[i]], i, height)) {
                launched = i;
            }
        }
    }

    PopStyleVar();

    if (launched >= 0) {
        set_focused(iris, launched);
        launch(iris, entries[library_view.order[launched]]);
    }
}

static bool draw_grid_card(Instance* iris, const Entry& entry, int index, ImVec2 origin, float width, float cover_height, float text_height) {
    using namespace ImGui;

    float scale = iris->ui.ui_scale;

    ImDrawList* dl = GetWindowDrawList();

    SetCursorScreenPos(origin);

    PushID(index);

    bool clicked = InvisibleButton("##card", ImVec2(width, cover_height + text_height));
    bool hovered = IsItemHovered();
    bool focused = library_view.focused == index;

    if (hovered && IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
        draw_entry_tooltip(iris, entry);
    }

    draw_context_menu(iris, entry);

    PopID();

    float lift = hovered ? 4.0f * scale : 0.0f;
    float rounding = 8.0f * scale;

    ImVec2 cover_min = ImVec2(origin.x, origin.y - lift);
    ImVec2 cover_max = ImVec2(origin.x + width, origin.y + cover_height - lift);

    draw_shadow(dl, cover_min, cover_max, rounding, (hovered ? 14.0f : 8.0f) * scale, hovered ? 0.55f : 0.40f);
    draw_cover(iris, dl, entry, cover_min, cover_max, rounding);

    if (hovered || focused) {
        float thickness = 2.0f * scale;

        dl->AddRect(
            ImVec2(cover_min.x - thickness, cover_min.y - thickness),
            ImVec2(cover_max.x + thickness, cover_max.y + thickness),
            GetColorU32(with_alpha(accent_color(), hovered ? 1.0f : 0.7f)),
            rounding + thickness,
            0,
            thickness
        );
    }

    float y = origin.y + cover_height + 8.0f * scale;
    float line_height = GetFontSize();

    const char* text = entry.title.c_str();
    const char* text_end = text + entry.title.size();
    const char* wrap = ImFontCalcWordWrapPositionEx(GetFont(), GetFontSize(), text, text_end, width);

    if (wrap <= text) {
        wrap = text_end;
    }

    ImU32 text_color = GetColorU32(ImGuiCol_Text);

    float lines = wrap >= text_end ? 1.0f : 2.0f;

    if (wrap >= text_end) {
        RenderTextEllipsis(dl, ImVec2(origin.x, y), ImVec2(origin.x + width, y + line_height), origin.x + width, text, text_end, nullptr);
    } else {
        dl->AddText(ImVec2(origin.x, y), text_color, text, wrap);

        while (wrap < text_end && *wrap == ' ') {
            wrap++;
        }

        RenderTextEllipsis(dl, ImVec2(origin.x, y + line_height), ImVec2(origin.x + width, y + line_height * 2.0f), origin.x + width, wrap, text_end, nullptr);
    }

    float meta_y = y + line_height * lines + 4.0f * scale;

    PushFont(iris->ui.font_small);

    std::string meta = entry.region.size() ? entry.region : entry.system;

    if (entry.format.size()) {
        meta += " \xe2\x80\xa2 " + entry.format;
    }

    ImVec4 meta_color = GetStyleColorVec4(ImGuiCol_TextDisabled);

    PushStyleColor(ImGuiCol_Text, meta_color);
    RenderTextEllipsis(dl, ImVec2(origin.x, meta_y), ImVec2(origin.x + width, meta_y + GetFontSize()), origin.x + width, meta.c_str(), nullptr, nullptr);
    PopStyleColor();

    PopFont();

    return clicked;
}

static void draw_grid(Instance* iris) {
    using namespace ImGui;

    const std::vector <Entry>& entries = library::entries(iris);

    float scale = iris->ui.ui_scale;
    float gap = 16.0f * scale;
    float target = std::clamp(iris->library.grid_size, 120.0f, 280.0f) * scale;
    float avail = GetContentRegionAvail().x;

    int columns = std::max(1, (int)((avail + gap) / (target + gap)));

    float width = std::floor((avail - gap * (columns - 1)) / columns);
    float cover_height = std::floor(width / 0.7f);

    PushFont(iris->ui.font_small);

    float small_height = GetFontSize();

    PopFont();

    float text_height = 8.0f * scale + GetFontSize() * 2.0f + 4.0f * scale + small_height;
    float row_height = cover_height + text_height + gap;

    library_view.columns = columns;
    library_view.row_height = row_height;

    int count = (int)library_view.order.size();
    int rows = (count + columns - 1) / columns;

    PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);

    ImGuiListClipper clipper;

    clipper.Begin(rows, row_height);

    if (library_view.focused >= 0) {
        clipper.IncludeItemByIndex(library_view.focused / columns);
    }

    int launched = -1;

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            ImVec2 origin = GetCursorScreenPos();

            for (int column = 0; column < columns; column++) {
                int index = row * columns + column;

                if (index >= count) {
                    break;
                }

                ImVec2 card = ImVec2(origin.x + column * (width + gap), origin.y + 6.0f * scale);

                if (draw_grid_card(iris, entries[library_view.order[index]], index, card, width, cover_height, text_height)) {
                    launched = index;
                }
            }

            SetCursorScreenPos(origin);
            Dummy(ImVec2(avail, row_height));
        }
    }

    PopStyleVar();

    if (launched >= 0) {
        set_focused(iris, launched);
        launch(iris, entries[library_view.order[launched]]);
    }
}

static void handle_keyboard(Instance* iris) {
    using namespace ImGui;

    if (library_view.order.empty() || IsAnyItemActive()) {
        return;
    }

    if (IsWindowFocused(ImGuiFocusedFlags_AnyWindow) && !IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        return;
    }

    int step = iris->library.view == library::VIEW_GRID ? library_view.columns : 1;
    int page = std::max(1, (int)(GetWindowHeight() / std::max(1.0f, library_view.row_height))) * step;
    int current = library_view.focused;

    auto move = [&](int delta) {
        set_focused(iris, current < 0 ? 0 : current + delta);
    };

    if (IsKeyPressed(ImGuiKey_DownArrow)) move(step);
    if (IsKeyPressed(ImGuiKey_UpArrow)) move(-step);
    if (IsKeyPressed(ImGuiKey_PageDown)) move(page);
    if (IsKeyPressed(ImGuiKey_PageUp)) move(-page);

    if (iris->library.view == library::VIEW_GRID) {
        if (IsKeyPressed(ImGuiKey_RightArrow)) move(1);
        if (IsKeyPressed(ImGuiKey_LeftArrow)) move(-1);
    }

    if (IsKeyPressed(ImGuiKey_Home)) set_focused(iris, 0);
    if (IsKeyPressed(ImGuiKey_End)) set_focused(iris, (int)library_view.order.size() - 1);

    bool enter = IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter);

    if (enter && library_view.focused >= 0) {
        launch(iris, library::entries(iris)[library_view.order[library_view.focused]]);
    }
}

static void scroll_to_focused() {
    using namespace ImGui;

    if (!library_view.scroll_to_focused || library_view.focused < 0) {
        return;
    }

    library_view.scroll_to_focused = false;

    float y = (float)(library_view.focused / std::max(1, library_view.columns)) * library_view.row_height;
    float scroll = GetScrollY();
    float height = GetWindowHeight();

    if (y < scroll) {
        SetScrollY(y);
    } else if (y + library_view.row_height > scroll + height) {
        SetScrollY(y + library_view.row_height - height);
    }
}

static std::string status_text(Instance* iris) {
    library::Progress progress = library::progress(iris);

    if (progress.scanning) {
        return "Scanning folders...";
    }

    if (progress.covers_total) {
        return fmt::format("Fetching covers {}/{}", progress.covers_done, progress.covers_total);
    }

    if (progress.fetching_db) {
        return "Updating game database...";
    }

    return "";
}

static void draw_progress_line(Instance* iris, float y, float x0, float x1) {
    using namespace ImGui;

    library::Progress progress = library::progress(iris);

    ImDrawList* dl = GetWindowDrawList();

    float thickness = 2.0f * iris->ui.ui_scale;

    dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + 1.0f), GetColorU32(ImGuiCol_Border));

    ImU32 accent = GetColorU32(accent_color());

    if (progress.covers_total) {
        float t = (float)progress.covers_done / (float)progress.covers_total;

        dl->AddRectFilled(ImVec2(x0, y), ImVec2(x0 + (x1 - x0) * t, y + thickness), accent, thickness);

        return;
    }

    if (progress.scanning || progress.fetching_db) {
        float width = (x1 - x0) * 0.25f;
        float t = (float)std::fmod(GetTime() * 0.6, 1.0);
        float start = x0 - width + (x1 - x0 + width) * t;

        dl->AddRectFilled(
            ImVec2(std::max(x0, start), y),
            ImVec2(std::min(x1, start + width), y + thickness),
            accent,
            thickness
        );
    }
}

static bool primary_button(const char* label, ImVec2 size = ImVec2(0, 0)) {
    using namespace ImGui;

    ImVec4 accent = accent_color();

    PushStyleColor(ImGuiCol_Button, accent);
    PushStyleColor(ImGuiCol_ButtonHovered, with_alpha(accent, 0.85f));
    PushStyleColor(ImGuiCol_ButtonActive, with_alpha(accent, 0.70f));
    PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    bool pressed = Button(label, size);

    PopStyleColor(4);

    return pressed;
}

static void draw_empty_state(Instance* iris, const char* icon, const char* title, const char* text, bool show_add, bool show_settings) {
    using namespace ImGui;

    float scale = iris->ui.ui_scale;
    float width = std::min(GetContentRegionAvail().x, 440.0f * scale);

    ImVec2 avail = GetContentRegionAvail();

    PushFont(iris->ui.font_icons_big);
    float icon_height = GetFontSize();
    PopFont();

    float block_height = icon_height + 140.0f * scale;

    SetCursorPos(ImVec2(
        GetCursorPosX() + (avail.x - width) * 0.5f,
        GetCursorPosY() + std::max(0.0f, (avail.y - block_height) * 0.4f)
    ));

    BeginGroup();

    float left = GetCursorPosX();

    auto centered = [&](float item_width) {
        SetCursorPosX(left + (width - item_width) * 0.5f);
    };

    PushFont(iris->ui.font_icons_big);
    PushStyleColor(ImGuiCol_Text, accent_color());
    centered(CalcTextSize(icon).x);
    TextUnformatted(icon);
    PopStyleColor();
    PopFont();

    PushFont(iris->ui.font_heading);
    centered(CalcTextSize(title).x);
    TextUnformatted(title);
    PopFont();

    PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled));
    PushTextWrapPos(left + width);

    ImVec2 text_size = CalcTextSize(text, nullptr, false, width);

    centered(text_size.x);
    TextWrapped("%s", text);

    PopTextWrapPos();
    PopStyleColor();

    Dummy(ImVec2(0.0f, 8.0f * scale));

    const char* add_label = ICON_MS_CREATE_NEW_FOLDER " Add folder...";
    const char* open_label = ICON_MS_DRIVE_FILE_MOVE " Open file...";
    const char* settings_label = ICON_MS_SETTINGS " Library settings";

    ImGuiStyle& style = GetStyle();

    auto button_width = [&](const char* label) {
        return CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    };

    float total = 0.0f;

    if (show_add) total += button_width(add_label) + style.ItemSpacing.x;
    if (show_settings) total += button_width(settings_label) + style.ItemSpacing.x;

    total += button_width(open_label);

    centered(total);

    if (show_add) {
        if (primary_button(add_label)) {
            add_folder_dialog(iris);
        }

        SameLine();
    }

    if (show_settings) {
        if (Button(settings_label)) {
            iris->applets.settings.selected = 2;
            iris->applets.settings.show();
        }

        SameLine();
    }

    if (Button(open_label)) {
        open_file_dialog(iris);
    }

    EndGroup();
}

static int grid_size_step(float size) {
    int best = 0;

    for (int i = 1; i < IM_ARRAYSIZE(grid_sizes); i++) {
        if (std::fabs(grid_sizes[i] - size) < std::fabs(grid_sizes[best] - size)) {
            best = i;
        }
    }

    return best;
}

static void draw_header(Instance* iris) {
    using namespace ImGui;

    float scale = iris->ui.ui_scale;

    ImGuiStyle& style = GetStyle();

    float start_y = GetCursorPosY();
    float content_left = GetCursorScreenPos().x;
    float content_width = GetContentRegionAvail().x;
    float left_x = GetCursorPosX();
    float right_edge = left_x + content_width;

    PushFont(iris->ui.font_black, 26.0f);
    AlignTextToFramePadding();
    TextUnformatted("Library");
    float title_height = GetItemRectSize().y;
    PopFont();

    SameLine(0.0f, 12.0f * scale);

    std::string count = fmt::format("{} games", library::entries(iris).size());
    std::string status = status_text(iris);

    if (status.size()) {
        count += " â¢ " + status;
    }

    SetCursorPosY(start_y + (title_height - GetFontSize()) * 0.5f + 2.0f * scale);
    TextDisabled("%s", count.c_str());

    float frame_height = GetFrameHeight();
    float search_width = 240.0f * scale;
    float filter_width = 70.0f * scale;
    float sort_width = 170.0f * scale;
    float slider_width = 110.0f * scale;
    float view_width = frame_height * 1.4f;
    float rescan_width = CalcTextSize(ICON_MS_REFRESH).x + style.FramePadding.x * 2.0f;
    float spacing = style.ItemSpacing.x;

    bool grid = iris->library.view == library::VIEW_GRID;

    float filter_total = filter_width * IM_ARRAYSIZE(filter_names);

    float fixed =
        spacing + sort_width + spacing +
        (grid ? slider_width + spacing : 0.0f) +
        view_width * IM_ARRAYSIZE(view_names) + spacing +
        rescan_width;

    float room = right_edge - (left_x + filter_total + spacing * 4.0f) - fixed;
    float min_search_width = 140.0f * scale;

    bool wrap = room < min_search_width;

    if (!wrap) {
        search_width = std::min(search_width, room);
    }

    imgui::segmented("##filter", &iris->library.filter, filter_names, IM_ARRAYSIZE(filter_names), filter_width);

    SameLine();

    if (wrap) {
        NewLine();
    }

    SetCursorPosX(std::max(left_x, right_edge - fixed - search_width));

    if (IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F)) {
        SetKeyboardFocusHere();
    }

    SetNextItemWidth(search_width);
    InputTextWithHint("##search", ICON_MS_SEARCH " Search games", &library_view.search);

    if (IsItemActive() && IsKeyPressed(ImGuiKey_Escape)) {
        library_view.search.clear();
    }

    SameLine();

    SetNextItemWidth(sort_width);

    std::string sort_preview = std::string(ICON_MS_SORT " ") + sort_names[std::clamp(iris->library.sort, 0, IM_ARRAYSIZE(sort_names) - 1)];

    if (BeginCombo("##sort", sort_preview.c_str())) {
        for (int i = 0; i < IM_ARRAYSIZE(sort_names); i++) {
            if (imgui::Selectable(sort_names[i], iris->library.sort == i)) {
                iris->library.sort = i;
            }
        }

        EndCombo();
    }

    if (grid) {
        SameLine();

        int step = grid_size_step(iris->library.grid_size);

        SetNextItemWidth(slider_width);

        PushStyleColor(ImGuiCol_SliderGrab, with_alpha(accent_color(), 0.45f));
        PushStyleColor(ImGuiCol_SliderGrabActive, with_alpha(accent_color(), 0.65f));

        if (SliderInt("##size", &step, 0, IM_ARRAYSIZE(grid_sizes) - 1, grid_size_names[step], ImGuiSliderFlags_NoInput)) {
            iris->library.grid_size = grid_sizes[step];
        }

        PopStyleColor(2);

        if (IsItemHovered()) {
            SetTooltip("Cover size");
        }
    }

    SameLine();

    imgui::segmented("##view", &iris->library.view, view_names, IM_ARRAYSIZE(view_names), view_width);

    SameLine();

    if (Button(ICON_MS_REFRESH "##rescan", ImVec2(rescan_width, frame_height))) {
        library::rescan(iris);
    }

    if (IsItemHovered()) {
        SetTooltip("Rescan folders");
    }

    float line_y = GetCursorScreenPos().y + 4.0f * scale;

    draw_progress_line(iris, line_y, content_left, content_left + content_width);

    Dummy(ImVec2(0.0f, 8.0f * scale));
}

void show_library(Instance* iris) {
    using namespace ImGui;

    float scale = iris->ui.ui_scale;

    ImGuiViewport* viewport = GetMainViewport();

    SetNextWindowPos(viewport->WorkPos);
    SetNextWindowSize(viewport->WorkSize);
    SetNextWindowViewport(viewport->ID);

    PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 12.0f * scale));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollWithMouse;

    bool open = Begin("##GameLibrary", nullptr, flags);

    PopStyleVar(3);

    if (open) {
        if (IsWindowAppearing()) {
            BringWindowToDisplayBack(GetCurrentWindow());
        }

        rebuild_order(iris);

        draw_header(iris);

        bool empty_library = library::entries(iris).empty();
        bool scanning = library::progress(iris).scanning;

        if (iris->library.dirs.empty() && empty_library) {
            draw_empty_state(
                iris, ICON_MS_SPORTS_ESPORTS,
                "Your library is empty",
                "Add a folder with your PS2 disc images or arcade sets. Iris scans it and fetches covers and titles automatically.",
                true, false
            );
        } else if (empty_library && scanning) {
            draw_empty_state(iris, ICON_MS_SEARCH, "Looking for games...", "This only takes a moment.", false, false);
        } else if (empty_library) {
            draw_empty_state(
                iris, ICON_MS_FOLDER_OPEN,
                "No games found",
                "None of your library folders contain PS2 disc images (ISO, BIN/CUE, CHD, CSO, ZSO) or arcade sets.",
                true, true
            );
        } else if (library_view.order.empty()) {
            std::string text = fmt::format("Nothing matches \"{}\"", library_view.search);

            draw_empty_state(iris, ICON_MS_SEARCH, "No results", text.c_str(), false, false);
        } else {
            float margin = 8.0f * scale;

            SetCursorPosX(GetCursorPosX() - margin);

            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(margin, 0.0f));

            ImVec2 size = ImVec2(GetContentRegionAvail().x + margin, 0.0f);

            bool child = BeginChild("##library_items", size, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);

            PopStyleVar();

            if (child) {
                handle_keyboard(iris);

                if (iris->library.view == library::VIEW_GRID) {
                    draw_grid(iris);
                } else {
                    draw_list(iris);
                }

                scroll_to_focused();
            }

            EndChild();
        }
    }

    End();
}

}
