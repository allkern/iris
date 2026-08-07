#pragma once

#include "applet.hpp"

namespace iris {

struct MemoryViewer : Applet {
    MemoryViewer() {
        id = "memory_viewer";
        title = "Memory";
    }

    void on_render() override;
};

}
