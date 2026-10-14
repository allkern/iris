/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
