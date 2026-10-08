#include <cstring>

#include "iris.hpp"
#include "ps2.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

static constexpr uint32_t NO_ACTION = 0xffffffff;
static constexpr float REMOTE_WIDTH = 13.8f;
static constexpr float REMOTE_HEIGHT = 45.2f;
static constexpr float REMOTE_HEIGHT_DESR = 52.6f;

static constexpr float C0 = 1.0f;
static constexpr float C1 = 5.1f;
static constexpr float C2 = 9.2f;
static constexpr float CW = 3.6f;
static constexpr float RH = 1.6f;

enum Shape {
    SHAPE_RECT,
    SHAPE_ROUND
};

struct RemoteButton {
    InputAction action;
    float x, y, w, h;
    int shape;
    const char* label;
    const char* name;
    ImVec4 color;
};

static const RemoteButton g_buttons[] = {
    { IRIS_RM_EJECT, C0, 1.0f, CW, RH, SHAPE_RECT, ICON_MS_EJECT, "Eject", {} },
    { IRIS_RM_RESET, C1, 1.0f, CW, RH, SHAPE_RECT, ICON_MS_RESTART_ALT, "Reset", {} },
    { IRIS_RM_POWER, C2, 1.0f, CW, RH, SHAPE_RECT, ICON_MS_POWER_SETTINGS_NEW, "Power", {} },

    { IRIS_RM_DIGIT_1, C0, 3.6f, CW, RH, SHAPE_RECT, "1", "1", {} },
    { IRIS_RM_DIGIT_2, C1, 3.6f, CW, RH, SHAPE_RECT, "2", "2", {} },
    { IRIS_RM_DIGIT_3, C2, 3.6f, CW, RH, SHAPE_RECT, "3", "3", {} },
    { IRIS_RM_DIGIT_4, C0, 5.7f, CW, RH, SHAPE_RECT, "4", "4", {} },
    { IRIS_RM_DIGIT_5, C1, 5.7f, CW, RH, SHAPE_RECT, "5", "5", {} },
    { IRIS_RM_DIGIT_6, C2, 5.7f, CW, RH, SHAPE_RECT, "6", "6", {} },
    { IRIS_RM_DIGIT_7, C0, 7.8f, CW, RH, SHAPE_RECT, "7", "7", {} },
    { IRIS_RM_DIGIT_8, C1, 7.8f, CW, RH, SHAPE_RECT, "8", "8", {} },
    { IRIS_RM_DIGIT_9, C2, 7.8f, CW, RH, SHAPE_RECT, "9", "9", {} },
    { IRIS_RM_CLEAR, C0, 9.9f, CW, RH, SHAPE_RECT, "Clear", "Clear", {} },
    { IRIS_RM_DIGIT_0, C1, 9.9f, CW, RH, SHAPE_RECT, "0", "0", {} },
    { IRIS_RM_TIME, C2, 9.9f, CW, RH, SHAPE_RECT, "Time", "Time", {} },

    { IRIS_RM_AUDIO, C0, 12.8f, CW, RH, SHAPE_RECT, "Audio", "Audio", {} },
    { IRIS_RM_ANGLE, C1, 12.8f, CW, RH, SHAPE_RECT, "Angle", "Angle", {} },
    { IRIS_RM_SUBTITLE, C2, 12.8f, CW, RH, SHAPE_RECT, "Subtitle", "Subtitle", {} },
    { IRIS_RM_PROGRAM, C0, 14.9f, CW, RH, SHAPE_RECT, "Program", "Program", {} },
    { IRIS_RM_REPEAT, C1, 14.9f, CW, RH, SHAPE_RECT, "Repeat", "Repeat", {} },
    { IRIS_RM_SHUFFLE, C2, 14.9f, CW, RH, SHAPE_RECT, "Shuffle", "Shuffle", {} },
    { IRIS_RM_ATOB, C0, 17.0f, CW, RH, SHAPE_RECT, "A-B", "A-B Repeat", {} },
    { IRIS_RM_TITLE, C1, 17.0f, CW, RH, SHAPE_RECT, "Title", "Title", {} },
    { IRIS_RM_MENU, C2, 17.0f, CW, RH, SHAPE_RECT, "Menu", "Menu", {} },

    { IRIS_RM_DISPLAY, C0, 20.0f, CW, RH, SHAPE_RECT, "Display", "Display", {} },
    { IRIS_RM_RETURN, C2, 20.0f, CW, RH, SHAPE_RECT, "Return", "Return", {} },
    { IRIS_RM_UP, 5.8f, 20.55f, 2.2f, 1.7f, SHAPE_RECT, "", "Up", {} },
    { IRIS_RM_LEFT, 3.85f, 22.5f, 1.7f, 2.2f, SHAPE_RECT, "", "Left", {} },
    { IRIS_RM_ENTER, 5.75f, 22.45f, 2.3f, 2.3f, SHAPE_ROUND, "Enter", "Enter", {} },
    { IRIS_RM_RIGHT, 8.25f, 22.5f, 1.7f, 2.2f, SHAPE_RECT, "", "Right", {} },
    { IRIS_RM_DOWN, 5.8f, 24.95f, 2.2f, 1.7f, SHAPE_RECT, "", "Down", {} },

    { IRIS_RM_PREV, C0, 27.7f, CW, RH, SHAPE_RECT, ICON_MS_SKIP_PREVIOUS, "Previous", {} },
    { IRIS_RM_PLAY, C1, 27.7f, CW, RH, SHAPE_RECT, ICON_MS_PLAY_ARROW, "Play", {} },
    { IRIS_RM_NEXT, C2, 27.7f, CW, RH, SHAPE_RECT, ICON_MS_SKIP_NEXT, "Next", {} },
    { IRIS_RM_SCAN_BACK, C0, 29.8f, CW, RH, SHAPE_RECT, ICON_MS_FAST_REWIND, "Scan Back", {} },
    { IRIS_RM_PAUSE, C1, 29.8f, CW, RH, SHAPE_RECT, ICON_MS_PAUSE, "Pause", {} },
    { IRIS_RM_SCAN_FORW, C2, 29.8f, CW, RH, SHAPE_RECT, ICON_MS_FAST_FORWARD, "Scan Forward", {} },
    { IRIS_RM_SLOW_BACK, C0, 31.9f, CW, RH, SHAPE_RECT, ICON_MS_ARROW_LEFT "Slow", "Slow Back", {} },
    { IRIS_RM_STOP, C1, 31.9f, CW, RH, SHAPE_RECT, ICON_MS_STOP, "Stop", {} },
    { IRIS_RM_SLOW_FORW, C2, 31.9f, CW, RH, SHAPE_RECT, "Slow" ICON_MS_ARROW_RIGHT, "Slow Forward", {} },

    { IRIS_RM_L2, 1.0f, 34.9f, 2.6f, RH, SHAPE_RECT, "L2", "L2", {} },
    { IRIS_RM_L1, 4.067f, 34.9f, 2.6f, RH, SHAPE_RECT, "L1", "L1", {} },
    { IRIS_RM_R1, 7.133f, 34.9f, 2.6f, RH, SHAPE_RECT, "R1", "R1", {} },
    { IRIS_RM_R2, 10.2f, 34.9f, 2.6f, RH, SHAPE_RECT, "R2", "R2", {} },
    { IRIS_RM_SELECT, C0, 37.4f, CW, RH, SHAPE_RECT, "Select", "Select", {} },
    { IRIS_RM_START, C0, 39.5f, CW, RH, SHAPE_RECT, "Start", "Start", {} },
    { IRIS_RM_TRIANGLE, 8.2f, 36.75f, 1.6f, 1.6f, SHAPE_ROUND, ICON_MS_CHANGE_HISTORY, "Triangle", imgui::BADGE_EMERALD },
    { IRIS_RM_SQUARE, 6.45f, 38.5f, 1.6f, 1.6f, SHAPE_ROUND, ICON_MS_CROP_SQUARE, "Square", imgui::BADGE_PINK },
    { IRIS_RM_CIRCLE, 9.95f, 38.5f, 1.6f, 1.6f, SHAPE_ROUND, ICON_MS_CIRCLE, "Circle", imgui::BADGE_RED },
    { IRIS_RM_CROSS, 8.2f, 40.25f, 1.6f, 1.6f, SHAPE_ROUND, ICON_MS_CLOSE, "Cross", imgui::BADGE_BLUE },
    { IRIS_RM_L3, 1.0f, 42.6f, 5.65f, RH, SHAPE_RECT, "L3", "L3", {} },
    { IRIS_RM_R3, 7.15f, 42.6f, 5.65f, RH, SHAPE_RECT, "R3", "R3", {} }
};

static const RemoteButton g_desr_buttons[] = {
    { IRIS_RM_HOME, C0, 45.8f, CW, RH, SHAPE_RECT, ICON_MS_HOME, "Home", {} },
    { IRIS_RM_G_GUIDE, C1, 45.8f, CW, RH, SHAPE_RECT, "G-Guide", "G-Guide", {} },
    { IRIS_RM_QUIT_GAME, C2, 45.8f, CW, RH, SHAPE_RECT, "Quit", "Quit Game", {} },
    { IRIS_RM_REC_MODE, C0, 47.9f, CW, RH, SHAPE_RECT, "Rec Mode", "Recording Mode", {} },
    { IRIS_RM_DELETE, C2, 47.9f, CW, RH, SHAPE_RECT, "Delete", "Delete", {} },
    { IRIS_RM_REC_START, C0, 50.0f, CW, RH, SHAPE_RECT, ICON_MS_FIBER_MANUAL_RECORD, "Record", imgui::BADGE_RED },
    { IRIS_RM_REC_PAUSE, C1, 50.0f, CW, RH, SHAPE_RECT, ICON_MS_PAUSE_CIRCLE, "Record Pause", {} },
    { IRIS_RM_REC_STOP, C2, 50.0f, CW, RH, SHAPE_RECT, ICON_MS_STOP_CIRCLE, "Record Stop", {} }
};

static const float g_dividers[] = {
    3.0f,
    12.2f,
    19.4f,
    27.1f,
    34.3f
};

static inline ImVec2 at(ImVec2 origin, float u, float x, float y) {
    return ImVec2(origin.x + (x * u), origin.y + (y * u));
}

static inline bool is_icon(const char* label) {
    return strlen(label) == 3 && ((unsigned char)label[0] & 0x80);
}

static bool polled(uint32_t count, uint32_t& last, uint64_t& tick) {
    uint64_t now = SDL_GetTicks();

    if (count != last) {
        last = count;
        tick = now;
    }

    return tick && (now - tick) < 1000;
}

static void show_receiver(const char* name, bool active) {
    using namespace ImGui;

    AlignTextToFramePadding();
    Text("%s", name);
    SameLine();

    if (active) {
        imgui::badge(ICON_MS_CHECK "  Polled", imgui::BADGE_GREEN);
    } else {
        imgui::badge(ICON_MS_SCHEDULE "  Not polled", imgui::BADGE_AMBER);
    }
}

static void draw_arrow(ImDrawList* dl, ImVec2 min, ImVec2 max, InputAction action) {
    using namespace ImGui;

    float cx = (min.x + max.x) * 0.5f;
    float cy = (min.y + max.y) * 0.5f;
    float r = GetFontSize() * 0.3f;
    ImU32 col = GetColorU32(ImGuiCol_Text);

    switch (action) {
        case IRIS_RM_UP: dl->AddTriangleFilled(ImVec2(cx, cy - r), ImVec2(cx + r, cy + r * 0.6f), ImVec2(cx - r, cy + r * 0.6f), col); break;
        case IRIS_RM_DOWN: dl->AddTriangleFilled(ImVec2(cx - r, cy - r * 0.6f), ImVec2(cx + r, cy - r * 0.6f), ImVec2(cx, cy + r), col); break;
        case IRIS_RM_LEFT: dl->AddTriangleFilled(ImVec2(cx - r, cy), ImVec2(cx + r * 0.6f, cy - r), ImVec2(cx + r * 0.6f, cy + r), col); break;
        case IRIS_RM_RIGHT: dl->AddTriangleFilled(ImVec2(cx + r, cy), ImVec2(cx - r * 0.6f, cy + r), ImVec2(cx - r * 0.6f, cy - r), col); break;
        default: break;
    }
}

static void draw_label(ImDrawList* dl, ImVec2 min, ImVec2 max, const RemoteButton& b) {
    using namespace ImGui;

    if (!*b.label) {
        draw_arrow(dl, min, max, b.action);

        return;
    }

    ImFont* font = GetFont();
    float base = GetFontSize() * (is_icon(b.label) ? 1.4f : 0.8f);
    float avail = (max.x - min.x) - (GetFontSize() * 0.2f);

    ImVec2 size = font->CalcTextSizeA(base, FLT_MAX, 0.0f, b.label);

    if (size.x > avail) {
        base *= avail / size.x;
        size = font->CalcTextSizeA(base, FLT_MAX, 0.0f, b.label);
    }

    ImVec4 color = b.color.w > 0.0f ? b.color : GetStyleColorVec4(ImGuiCol_Text);

    ImVec2 pos = ImVec2(
        min.x + ((max.x - min.x) - size.x) * 0.5f,
        min.y + ((max.y - min.y) - size.y) * 0.5f
    );

    dl->AddText(font, base, pos, GetColorU32(color), b.label);
}

static bool draw_button(Instance* iris, ImDrawList* dl, ImVec2 origin, float u, const RemoteButton& b) {
    using namespace ImGui;

    ImVec2 min = at(origin, u, b.x, b.y);
    ImVec2 max = at(origin, u, b.x + b.w, b.y + b.h);

    SetCursorScreenPos(min);
    PushID((int)b.action);
    InvisibleButton("##button", ImVec2(max.x - min.x, max.y - min.y));
    PopID();

    bool active = IsItemActive();

    SetItemTooltip("%s", b.name);

    bool lit = active || iris->input.remote_action == b.action;

    ImGuiCol fill = ImGuiCol_Button;

    if (lit) {
        fill = ImGuiCol_ButtonActive;
    } else if (IsItemHovered()) {
        fill = ImGuiCol_ButtonHovered;
    }

    if (b.shape == SHAPE_ROUND) {
        ImVec2 center = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        float radius = (max.x - min.x) * 0.5f;

        dl->AddCircleFilled(center, radius, GetColorU32(fill));
        dl->AddCircle(center, radius, GetColorU32(ImGuiCol_Border));
    } else {
        dl->AddRectFilled(min, max, GetColorU32(fill), 0.4f * u);
        dl->AddRect(min, max, GetColorU32(ImGuiCol_Border), 0.4f * u);
    }

    draw_label(dl, min, max, b);

    return active;
}

void DvdRemote::set_held(uint32_t action) {
    if (action == held)
        return;

    if (held != NO_ACTION) {
        input::execute_action(iris, (InputAction)held, 0, 0.0f);
    }

    held = action;

    if (held != NO_ACTION) {
        input::execute_action(iris, (InputAction)held, 0, 1.0f);
    }
}

void DvdRemote::show_status() {
    using namespace ImGui;

    cdvd::Cdvd* cdvd = iris->ps2->cdvd;

    show_receiver("Built-in receiver", polled(cdvd->remote_reads.load(), last_reads, last_read_tick));

    if (iris->ps2->detected_system == ps2::PSX_DESR) {
        speed::dvrp::Dvrp* dvrp = iris->ps2->speed->dvrp;

        show_receiver("PSX receiver", polled(dvrp->remote_reads.load(), last_dvrp_reads, last_dvrp_tick));
    }

    for (int i = 0; i < 2; i++) {
        dev::rm::Rm* rm = iris->input.rm[i];

        if (!rm)
            continue;

        char name[32];

        snprintf(name, sizeof(name), "Slot %d receiver", i + 1);

        show_receiver(name, polled(rm->polls.load(), last_polls[i], last_poll_tick[i]));
    }
}

void DvdRemote::show_remote() {
    using namespace ImGui;

    float u = GetFontSize() * 0.8f;
    float offset = (GetContentRegionAvail().x - (REMOTE_WIDTH * u)) * 0.5f;

    if (offset > 0.0f) {
        SetCursorPosX(GetCursorPosX() + offset);
    }

    bool desr = iris->ps2->detected_system == ps2::PSX_DESR;
    ImVec2 origin = GetCursorScreenPos();
    ImVec2 end = at(origin, u, REMOTE_WIDTH, desr ? REMOTE_HEIGHT_DESR : REMOTE_HEIGHT);
    ImDrawList* dl = GetWindowDrawList();

    dl->AddRectFilled(origin, end, GetColorU32(ImGuiCol_FrameBg), 2.0f * u);
    dl->AddRect(origin, end, GetColorU32(ImGuiCol_Border), 2.0f * u, 0, 2.0f);

    for (float y : g_dividers) {
        dl->AddLine(at(origin, u, 1.5f, y), at(origin, u, REMOTE_WIDTH - 1.5f, y), GetColorU32(ImGuiCol_Separator));
    }

    uint32_t active = NO_ACTION;

    for (const RemoteButton& b : g_buttons) {
        if (draw_button(iris, dl, origin, u, b)) {
            active = b.action;
        }
    }

    if (desr) {
        dl->AddLine(at(origin, u, 1.5f, 44.9f), at(origin, u, REMOTE_WIDTH - 1.5f, 44.9f), GetColorU32(ImGuiCol_Separator));

        for (const RemoteButton& b : g_desr_buttons) {
            if (draw_button(iris, dl, origin, u, b)) {
                active = b.action;
            }
        }
    }

    SetCursorScreenPos(origin);
    Dummy(ImVec2(end.x - origin.x, end.y - origin.y));

    set_held(active);
}

void DvdRemote::on_render() {
    using namespace ImGui;

    show_status();

    Spacing();

    show_remote();
}

void DvdRemote::on_close() {
    set_held(NO_ACTION);
}

}
