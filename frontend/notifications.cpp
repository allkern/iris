#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

enum {
    STATE_IDLE,
    STATE_MOVING = 1,
    STATE_FADING = 2
};

enum {
    TYPE_INFO = 0,
};

static inline int seconds_to_frames(float s) {
    return s * 60.0f;
}

float ease_in_out(float t) {
    if (t <= 0.5f) return 2.0f * t * t;

    t -= 0.5f;

    return 2.0f * t * (1.0f - t) + 0.5f;
}

void handle_move(iris::notification* notif) {
    float f = (float)notif->move.frames_remaining / (float)notif->move.frames;

    f = ease_in_out(f);

    float fs = f;
    float ft = 1.0 - f;

    notif->move.x = notif->move.source_x * fs + notif->move.target_x * ft;
    notif->move.y = notif->move.source_y * fs + notif->move.target_y * ft;

    if (!notif->move.frames_remaining) {
        notif->state &= ~STATE_MOVING;
    } else {
        notif->move.frames_remaining--;
    }
}

void handle_fade(iris::notification* notif) {
    float f = (float)notif->fade.frames_remaining / (float)notif->fade.frames;

    f = ease_in_out(f);

    float fs = f;
    float ft = 1.0 - f;

    notif->fade.alpha = notif->fade.source_alpha * fs + notif->fade.target_alpha * ft;

    if (!notif->fade.frames_remaining) {
        notif->state &= ~STATE_FADING;
    } else {
        notif->fade.frames_remaining--;
    }
}

void render_notification(iris::instance* iris, iris::notification* notif) {
    using namespace ImGui;

    ImGuiStyle& style = ImGui::GetStyle();

    ImVec4 bg = style.Colors[ImGuiCol_MenuBarBg];
    ImVec4 tc = style.Colors[ImGuiCol_Text];
    ImVec4 td = style.Colors[ImGuiCol_TextDisabled];

    uint32_t alpha = (notif->fade.alpha) << 24;

    uint32_t bg_col =
        ((uint32_t)(bg.x * 255.0) << 0 ) |
        ((uint32_t)(bg.y * 255.0) << 8 ) |
        ((uint32_t)(bg.z * 255.0) << 16) |
        alpha;

    uint32_t text_col =
        ((uint32_t)(tc.x * 255.0) << 0 ) |
        ((uint32_t)(tc.y * 255.0) << 8 ) |
        ((uint32_t)(tc.z * 255.0) << 16) |
        alpha;

    uint32_t icon_col = 0x799fa7 | alpha;

    uint32_t bar_col =
        ((uint32_t)(td.x * 255.0) << 0 ) |
        ((uint32_t)(td.y * 255.0) << 8 ) |
        ((uint32_t)(td.z * 255.0) << 16) |
        alpha;

    float x = notif->move.x;
    float y = notif->move.y;
    float width = notif->width;
    float height = notif->height;

    GetForegroundDrawList()->AddRectFilled(
        ImVec2(x, y),
        ImVec2(x + width, y + height),
        bg_col, 10.0, ImDrawFlags_RoundCornersAll
    );

    GetForegroundDrawList()->AddText(
        ImVec2(x + 10, y + (height / 2) - (notif->text_height / 2)), icon_col, ICON_MS_INFO
    );

    GetForegroundDrawList()->AddText(
        ImVec2(x + 35, y + (height / 2) - (notif->text_height / 2)), text_col, notif->text.c_str()
    );

    int progress_width = width * (1.0 - ((float)notif->frames_remaining / (float)notif->frames));

    GetForegroundDrawList()->AddRectFilled(
        ImVec2(x, y + height - 2),
        ImVec2(x + progress_width, y + height),
        bar_col, 10.0, ImDrawFlags_RoundCornersBottom
    );
}

void remove_notification(iris::instance* iris, int i) {
    iris::notification& n = iris->notifications.at(i);

    for (unsigned int j = i + 1; j < iris->notifications.size(); j++) {
        iris::notification& n1 = iris->notifications.at(j);

        int target_x = n1.state & STATE_MOVING ? n1.move.target_x : n1.move.x;
        int target_y = n1.state & STATE_MOVING ? n1.move.target_y : n1.move.y;

        n1.move.source_x = n1.move.x;
        n1.move.source_y = n1.move.y;
        n1.move.target_x = target_x;
        n1.move.target_y = target_y + n.height + 10;
        n1.move.frames = seconds_to_frames(0.25);
        n1.move.frames_remaining = n.move.frames;
        n1.state |= STATE_MOVING;
    }

    iris->notifications.erase(std::begin(iris->notifications) + i);
}

void handle_animations(iris::instance* iris) {
    for (unsigned int i = 0; i < iris->notifications.size(); i++) {
        iris::notification& n = iris->notifications.at(i);

        render_notification(iris, &n);

        if (n.frames_remaining) {
            n.frames_remaining--;

            if (!n.frames_remaining) {
                n.fade.source_alpha = n.fade.alpha;
                n.fade.target_alpha = 0;
                n.fade.frames = seconds_to_frames(0.25);
                n.fade.frames_remaining = n.fade.frames;
                n.state |= STATE_FADING;
                n.end = true;
            }
        }

        if (n.state == STATE_IDLE) {
            if (n.end) {
                remove_notification(iris, i);
            }

            continue;
        }

        if (n.state & STATE_MOVING) handle_move(&n);
        if (n.state & STATE_FADING) handle_fade(&n);
    }
}

void push_notification(iris::instance* iris, iris::notification notif) {
    for (iris::notification& n : iris->notifications) {
        int target_x = n.state & STATE_MOVING ? n.move.target_x : n.move.x;
        int target_y = n.state & STATE_MOVING ? n.move.target_y : n.move.y;

        n.move.source_x = n.move.x;
        n.move.source_y = n.move.y;
        n.move.target_x = target_x;
        n.move.target_y = target_y - notif.height - 10;
        n.move.frames = seconds_to_frames(0.25);
        n.move.frames_remaining = n.move.frames;
        n.state |= STATE_MOVING;
    }

    iris->notifications.push_front(notif);
}

void push_info(iris::instance* iris, std::string text) {
    using namespace ImGui;

    iris::notification notif;

    int window_width, window_height;
    int statusbar_offset = iris->show_status_bar ? iris->menubar_height : 0;

    SDL_GetWindowSizeInPixels(iris->window, &window_width, &window_height);

    ImVec2 ts = CalcTextSize(text.c_str());

    notif.text = text;
    notif.text_width = ts.x;
    notif.text_height = ts.y;
    notif.width = notif.text_width + 50;
    notif.height = notif.text_height + 25;
    notif.frames = seconds_to_frames(5.0);
    notif.frames_remaining = notif.frames;
    notif.state = STATE_MOVING | STATE_FADING;
    notif.move.source_x = window_width + 5;
    notif.move.source_y = window_height - notif.height - 10 - statusbar_offset;
    notif.move.target_x = window_width - notif.width - 10;
    notif.move.target_y = notif.move.source_y;
    notif.move.frames = seconds_to_frames(0.25);
    notif.move.frames_remaining = notif.move.frames;
    notif.fade.source_alpha = 0;
    notif.fade.target_alpha = 255;
    notif.fade.frames = seconds_to_frames(0.25);
    notif.fade.frames_remaining = notif.fade.frames;
    notif.type = TYPE_INFO;
    notif.end = false;

    push_notification(iris, notif);
}

}