#pragma once

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
bool is_arcade_file(Instance* iris, std::string path);
bool load_arcade(Instance* iris, std::string path);
bool load_arcade_files(Instance* iris, std::string path);
int attach_memory_card(Instance* iris, int slot, const char* path);
void detach_memory_card(Instance* iris, int slot);
int format_memory_card(Instance* iris, int slot);
const char* get_system_name(Instance* iris, int system);
const char* get_current_system_name(Instance* iris);
int get_system_count(Instance* iris);
bool load_rom_files(Instance* iris);

}

}
