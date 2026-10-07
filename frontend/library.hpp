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
