#include "iris.hpp"

// Note: This is a stub implementation for platforms that
//       do not need special initialization

namespace iris::platform {

void init_console() {}

bool init(Instance* iris) {
    return true;
}

bool apply_settings(Instance* iris) {
    return true;
}

void destroy(Instance* iris) {}

}