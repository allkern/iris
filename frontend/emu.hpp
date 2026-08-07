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

bool init(Instance* iris);
void destroy(Instance* iris);
int open_file(Instance* iris, std::string path);

bool is_disc_image(const std::string& path);

// Swaps the image in the drive without rebooting, cycling the tray so the
// running game notices. Only valid for disc images
int insert_disc(Instance* iris, std::string path);
void start_pending_load(Instance* iris);
void finalize_load(Instance* iris);
bool load_arcade(Instance* iris, std::string path);
int attach_memory_card(Instance* iris, int slot, const char* path);
void detach_memory_card(Instance* iris, int slot);
const char* get_system_name(Instance* iris, int system);
const char* get_current_system_name(Instance* iris);
int get_system_count(Instance* iris);
bool load_rom_files(Instance* iris);

}

}
