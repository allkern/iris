#include "iris.hpp"

// Note: This is a stub implementation for platforms that
//       do not need special initialization

namespace iris::platform {

bool init(instance* iris) {
    return true;
}

bool apply_settings(instance* iris) {
    return true;
}

void destroy(instance* iris) {}

}