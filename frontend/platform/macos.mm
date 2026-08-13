#include "iris.hpp"

#import <AppKit/AppKit.h>

static const NSInteger IRIS_MENU_TAG = 0x1715;

@interface IrisMenuTarget : NSObject
- (void)fire:(id)sender;
@end

@implementation IrisMenuTarget
- (void)fire:(id)sender {
    iris::menu::activate([[(NSMenuItem*)sender representedObject] intValue]);
}
@end

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

static NSString* strip_icon(const std::string& label) {
    const uint8_t* p = (const uint8_t*)label.c_str();

    if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
        uint32_t cp = ((uint32_t)(p[0] & 0x0f) << 12) | ((uint32_t)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);

        if (cp >= 0xe000 && cp <= 0xf8ff) {
            p += 3;

            while (*p == ' ')
                p++;
        }
    }

    return [NSString stringWithUTF8String:(const char*)p];
}

void set_menubar(Instance* iris, const std::vector <menu::Node>& nodes) {
    NSMenu* bar = [NSApp mainMenu];

    if (!bar)
        return;

    static IrisMenuTarget* target = nil;

    if (!target)
        target = [[IrisMenuTarget alloc] init];

    for (NSInteger i = bar.numberOfItems - 1; i >= 0; i--) {
        if ([bar itemAtIndex:i].tag == IRIS_MENU_TAG)
            [bar removeItemAtIndex:i];
    }

    NSInteger insert = bar.numberOfItems ? 1 : 0;

    std::vector <NSMenu*> stack;

    for (size_t i = 0; i < nodes.size(); i++) {
        const menu::Node& node = nodes[i];

        while ((int)stack.size() > node.depth)
            stack.pop_back();

        if (node.separator) {
            if (!stack.empty())
                [stack.back() addItem:[NSMenuItem separatorItem]];

            continue;
        }

        NSString* title = strip_icon(node.label);
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];

        if (node.submenu) {
            NSMenu* submenu = [[NSMenu alloc] initWithTitle:title];

            // Without this Cocoa greys out anything whose target does not
            // implement validation, which is everything we build here
            submenu.autoenablesItems = NO;

            item.submenu = submenu;
        } else {
            item.action = @selector(fire:);
            item.target = target;
            item.representedObject = @((int)i);
            item.state = node.checked ? NSControlStateValueOn : NSControlStateValueOff;
        }

        item.enabled = node.enabled;

        if (stack.empty()) {
            item.tag = IRIS_MENU_TAG;

            [bar insertItem:item atIndex:insert++];
        } else {
            [stack.back() addItem:item];
        }

        if (node.submenu)
            stack.push_back(item.submenu);
    }
}

void destroy(Instance* iris) {}

}
