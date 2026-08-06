#include "iris.hpp"
#include "applets.hpp"

namespace iris {

namespace applets {

void create(Instance* iris) {
    Applets& applets = iris->applets;

    applets.all = {
        &applets.compat_report,
        &applets.memory_search,
        &applets.symbols
    };

    for (Applet* a : applets.all)
        a->iris = iris;
}

void init(Instance* iris) {
    for (Applet* a : iris->applets.all)
        a->on_init();
}

void render(Instance* iris) {
    for (Applet* a : iris->applets.all) {
        if (a->open && !a->was_open) {
            a->was_open = true;

            a->on_open();
        }

        if (a->open) {
            if (a->begin())
                a->on_render();

            a->end();
        }

        if (!a->open && a->was_open) {
            a->was_open = false;

            a->on_close();
        }
    }
}

}

}
