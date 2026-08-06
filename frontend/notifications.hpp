#pragma once

#include <cstddef>
#include <string>

namespace iris {

struct MoveAnimation {
    int frames;
    int frames_remaining;
    float source_x, source_y;
    float target_x, target_y;
    float x, y;
};

struct FadeAnimation {
    int frames;
    int frames_remaining;
    int source_alpha, target_alpha;
    int alpha;
};

struct Notification {
    int type;
    int state;
    int frames;
    int frames_remaining;
    float width, height;
    float text_width, text_height;
    bool end;
    MoveAnimation move;
    FadeAnimation fade;
    std::string text;
};

}
