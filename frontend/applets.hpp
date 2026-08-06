#pragma once

#include <vector>

#include "applet.hpp"

#include "ui/compat_report.hpp"
#include "ui/memory_search.hpp"
#include "ui/symbols.hpp"

namespace iris {

struct Applets {
    CompatReport compat_report;
    MemorySearch memory_search;
    Symbols symbols;

    std::vector <Applet*> all = {};
};

namespace applets {

void create(Instance* iris);
void init(Instance* iris);
void render(Instance* iris);

}

}
