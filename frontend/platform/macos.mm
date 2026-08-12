#include "iris.hpp"

#import <AppKit/AppKit.h>

namespace iris::platform {

void init_console() {}

bool init(Instance* iris) {
    apply_settings(iris);

    return true;
}

bool apply_settings(Instance* iris) {
    if (!NSApp)
        return false;

    NSAppearanceName name = iris->dark_titlebar ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua;

    NSApp.appearance = [NSAppearance appearanceNamed:name];

    return true;
}

void destroy(Instance* iris) {}

}
