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

#include <optional>
#include <string>

namespace iris {

struct Instance;

enum class RecentType : int {
    PS2,
    ARCADE
};

struct Recent {
    std::string path;
    RecentType type;
};

struct ArcadeInfo {
    std::string id;
    std::string name;
    std::string subdir;
    int system = 0;
};

namespace emu {

enum ArcadeBios {
    ARCADE_BIOS_147,
    ARCADE_BIOS_148,
    ARCADE_BIOS_246,
    ARCADE_BIOS_256,
    ARCADE_BIOS_PYTHON,
    ARCADE_BIOS_PYTHON2,
    ARCADE_BIOS_COUNT
};

const char* get_arcade_bios_label(int slot);
const char* get_arcade_bios_key(int slot);
int get_arcade_bios_slot(int system);
std::string get_arcade_bios_path(Instance* iris, int system);

bool init(Instance* iris);
void destroy(Instance* iris);
int open_file(Instance* iris, std::string path);
int boot_ps2_path(Instance* iris, std::string path);

bool is_disc_image(const std::string& path);

int insert_disc(Instance* iris, std::string path);
void start_pending_load(Instance* iris);
void finalize_load(Instance* iris);
void clean_arcade_files(Instance* iris);
void clean_tmp_files(Instance* iris);
bool is_arcade_file(Instance* iris, std::string path);
bool load_arcade(Instance* iris, std::string path);
bool load_arcade_files(Instance* iris, std::string path);
std::optional <ArcadeInfo> describe_arcade(const std::string& path);
void close_game(Instance* iris);
int attach_memory_card(Instance* iris, int slot, const char* path);
void detach_memory_card(Instance* iris, int slot);
int format_memory_card(Instance* iris, int slot);
const char* get_system_name(Instance* iris, int system);
const char* get_current_system_name(Instance* iris);
int get_system_count(Instance* iris);
bool load_rom_files(Instance* iris);

}

}
