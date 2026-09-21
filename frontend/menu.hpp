#pragma once

#include <string>
#include <vector>

namespace iris {

struct Instance;

namespace menu {

struct Node {
    std::string label;
    std::string shortcut;

    int depth = 0;

    bool submenu = false;
    bool separator = false;
    bool enabled = true;
    bool checked = false;

    bool operator==(const Node& other) const {
        return label == other.label &&
               shortcut == other.shortcut &&
               depth == other.depth &&
               submenu == other.submenu &&
               separator == other.separator &&
               enabled == other.enabled &&
               checked == other.checked;
    }
};

bool native();
bool begin_bar(Instance* iris);
void end_bar(Instance* iris);
bool begin(const char* label, bool enabled = true);
void end();
bool item(const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);
bool item(const char* label, const char* shortcut, bool* selected, bool enabled = true);
void separator();
void activate(int index);

}

}
