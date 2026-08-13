#include "iris.hpp"
#include "menu.hpp"

namespace iris::menu {

#ifdef __APPLE__
inline constexpr bool NATIVE = true;
#else
inline constexpr bool NATIVE = false;
#endif

// Description being built by the current walk
static std::vector <Node> g_nodes;

// What the OS is showing, so the menubar is only rebuilt when it really changed
static std::vector <Node> g_published;

// Items the user picked since the last walk, by index into g_published
static std::vector <int> g_pending;

static int g_depth = 0;

bool native() {
    return NATIVE;
}

void activate(int index) {
    g_pending.push_back(index);
}

static bool take(int index) {
    for (size_t i = 0; i < g_pending.size(); i++) {
        if (g_pending[i] != index)
            continue;

        g_pending.erase(g_pending.begin() + i);

        return true;
    }

    return false;
}

bool begin_bar(Instance* iris) {
    if (!NATIVE) {
        if (iris->fullscreen)
            return false;

        ImGui::PushFont(iris->ui.font_icons);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 7.0));

        if (ImGui::BeginMainMenuBar())
            return true;

        ImGui::PopStyleVar();
        ImGui::PopFont();

        return false;
    }

    g_nodes.clear();

    g_depth = 0;

    return true;
}

void end_bar(Instance* iris) {
    if (!NATIVE) {
        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar();
        ImGui::PopFont();

        return;
    }

    if (g_nodes != g_published) {
        g_published = g_nodes;

        platform::set_menubar(iris, g_published);
    }

    // Anything the walk did not claim was aimed at a menu that has since moved
    g_pending.clear();
}

bool begin(const char* label, bool enabled) {
    if (!NATIVE)
        return imgui::BeginMenu(label, enabled);

    Node node;

    node.label = label;
    node.depth = g_depth;
    node.submenu = true;
    node.enabled = enabled;

    g_nodes.push_back(node);

    g_depth++;

    // Unlike ImGui the body always runs, because a retained menu has to be
    // described whether or not it happens to be open
    return true;
}

void end() {
    if (!NATIVE) {
        ImGui::EndMenu();

        return;
    }

    g_depth--;
}

bool item(const char* label, const char* shortcut, bool selected, bool enabled) {
    if (!NATIVE)
        return imgui::MenuItem(label, shortcut, selected, enabled);

    Node node;

    node.label = label;
    node.shortcut = shortcut ? shortcut : "";
    node.depth = g_depth;
    node.enabled = enabled;
    node.checked = selected;

    int index = (int)g_nodes.size();

    g_nodes.push_back(node);

    return take(index);
}

bool item(const char* label, const char* shortcut, bool* selected, bool enabled) {
    if (!NATIVE)
        return imgui::MenuItem(label, shortcut, selected, enabled);

    bool clicked = item(label, shortcut, selected && *selected, enabled);

    if (clicked && selected)
        *selected = !*selected;

    return clicked;
}

void separator() {
    if (!NATIVE) {
        ImGui::Separator();

        return;
    }

    Node node;

    node.depth = g_depth;
    node.separator = true;

    g_nodes.push_back(node);
}

}
