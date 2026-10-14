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

#include <cstdint>
#include <string>
#include <vector>

namespace iris {

struct Instance;
struct Texture;

namespace library {

enum : int {
    KIND_IGNORED = -1,
    KIND_PS2_CD,
    KIND_PS2_DVD,
    KIND_ARCADE
};

enum : int {
    VIEW_LIST,
    VIEW_GRID
};

enum : int {
    SORT_TITLE,
    SORT_RECENT,
    SORT_REGION,
    SORT_SYSTEM
};

enum : int {
    FILTER_ALL,
    FILTER_PS2,
    FILTER_ARCADE
};

struct Entry {
    std::string path;
    std::string title;
    std::string serial;
    std::string region;
    std::string format;
    std::string system;
    std::string developer;
    std::string release_date;
    int kind = KIND_PS2_DVD;
    int64_t last_played = 0;
};

struct Progress {
    bool scanning = false;
    bool fetching_db = false;
    int covers_done = 0;
    int covers_total = 0;
};

struct State;

bool init(Instance* iris);
void destroy(Instance* iris);
void update(Instance* iris);

void rescan(Instance* iris);
void refresh_covers(Instance* iris);
void clear_cover_cache(Instance* iris);
void refresh_cover(Instance* iris, const Entry& entry);
void mark_played(Instance* iris, const std::string& path);

const std::vector <Entry>& entries(Instance* iris);
uint64_t generation(Instance* iris);
Progress progress(Instance* iris);

const Texture* request_cover(Instance* iris, const Entry& entry, float* alpha);

}

}
