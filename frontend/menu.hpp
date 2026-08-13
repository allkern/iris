#pragma once

#include <string>
#include <vector>

namespace iris {

struct Instance;

namespace menu {

// The menubar is described as a flat list: depth says how deep a row sits, and
// the rows following a submenu row up to the next shallower one are its children
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

// True when the OS draws the menubar and ImGui should keep out of it
bool native();

// Wraps one walk of the menu tree. Returns false when there is nothing to draw
bool begin_bar(Instance* iris);
void end_bar(Instance* iris);

bool begin(const char* label, bool enabled = true);
void end();

bool item(const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);
bool item(const char* label, const char* shortcut, bool* selected, bool enabled = true);

void separator();

// Handed back by the platform layer when the user picks a native item. The
// click is replayed on the next walk, so the action still runs on our own
// frame rather than inside the menu's nested event loop
void activate(int index);

}

}
