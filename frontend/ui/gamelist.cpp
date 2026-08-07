#include <filesystem>
#include <vector>
#include <string>
#include <cctype>
#include <atomic>
#include <thread>
#include <regex>

#include "iris.hpp"
#include "config.hpp"
#include "net.hpp"

#include "iop/disc.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

enum : int {
    GAMELIST_CACHE_NOT_READY,
    GAMELIST_CACHE_PARSING,
    GAMELIST_CACHE_DOWNLOADING_ASSETS,
    GAMELIST_CACHE_READY,
    GAMELIST_CACHE_LOADED_COVERS
};

struct gamelist_cache_entry_hdr {
    char serial[10]; // SCUS-12345
    char region[16]; // NTSC-U, PAL, etc.
    int type; // CD, DVD, etc.
    int title_len; // Length of title string
};

std::atomic <int> gamelist_cache_status = GAMELIST_CACHE_NOT_READY;
std::atomic <int> gamelist_cache_progress = 0;
std::atomic <int> gamelist_cache_total = 0;
bool gamelist_cache_thread_started = false;
bool gamelib_autosearch = true;
bool gamelib_regex = false;
bool gamelib_case_sensitive = false;

struct gamelist_entry {
    std::string name;
    std::string path;
    std::string format;
    std::string type;
    std::string region;
    std::string serial;
    std::string cover;
    Texture* cover_texture = nullptr;
};

std::vector <gamelist_entry> gamelist_cache = {};
std::vector <gamelist_entry> gamelist_cache_full = {};

std::string lower(const std::string& str) {
    std::string result = str;

    for (char& c : result) {
        c = std::tolower(c);
    }

    return result;
}

std::string upper(const std::string& str) {
    std::string result = str;

    for (char& c : result) {
        c = std::toupper(c);
    }

    return result;
}

void parse_disc(iop::disc::Disc* disc, gamelist_entry* entry) {
    int type = iop::disc::get_type(disc);

    switch (type) {
        case iop::disc::CDVD_DISC_PSX_CD: entry->type = "PlayStation CD"; break; 
        case iop::disc::CDVD_DISC_PSX_CDDA: entry->type = "PlayStation CDDA"; break; 
        case iop::disc::CDVD_DISC_PS2_CD: entry->type = "PlayStation 2 CD"; break; 
        case iop::disc::CDVD_DISC_PS2_CDDA: entry->type = "PlayStation 2 CDDA"; break; 
        case iop::disc::CDVD_DISC_PS2_DVD: entry->type = "PlayStation 2 DVD"; break; 
        case iop::disc::CDVD_DISC_CDDA: entry->type = "CD Audio"; break; 
        case iop::disc::CDVD_DISC_DVD_VIDEO: entry->type = "DVD Video"; break; 
    }

    char serial[128];

    if (!iop::disc::get_serial(disc, serial)) {
        serial[0] = '\0';
    }

    entry->serial = std::string(serial);

    std::replace(entry->serial.begin(), entry->serial.end(), '_', '-');
    entry->serial.erase(std::remove(entry->serial.begin(), entry->serial.end(), '.'), entry->serial.end());

    if (entry->serial.length() > 10) {
        entry->serial = entry->serial.substr(0, 10);
    } else if (!entry->serial.length()) {
        entry->serial = "Unknown";
    }
}

void make_gamelist_cache(Instance* iris, std::string path) {
    gamelist_cache_status = GAMELIST_CACHE_PARSING;

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();

            for (char& c : ext) {
                c = std::tolower(c);
            }

            if (ext == ".iso" || ext == ".bin" || ext == ".cue" || ext == ".chd" || ext == ".cso" || ext == ".zso") {
                iop::disc::Disc* disc = iop::disc::open(entry.path().string().c_str());

                gamelist_entry e;

                e.name = entry.path().stem().string();
                e.path = entry.path().string();
                e.format = upper(ext.substr(1));
                e.type = "Unknown";
                e.region = "Unknown";
                e.serial = "Unknown";

                if (disc) {
                    parse_disc(disc, &e);
                    iop::disc::close(disc);
                }

                gamelist_cache.push_back(e);
            }
        }
    }

    gamelist_cache_status = GAMELIST_CACHE_DOWNLOADING_ASSETS;
    gamelist_cache_total = gamelist_cache.size();
    gamelist_cache_progress = 0;

    const std::string gamedb_url = "https://raw.githubusercontent.com/niemasd/GameDB-PS2/refs/heads/main/games/";
    const std::string game_covers_url = "https://raw.githubusercontent.com/xlenore/ps2-covers/refs/heads/main/covers/default/";
    const int thread_pool_size = 32;

    if (gamelist_cache.size() < thread_pool_size) {
        for (gamelist_entry& entry : gamelist_cache) {
            net::DownloadResult region = net::download(gamedb_url + entry.serial + "/region.txt");
            net::DownloadResult title = net::download(gamedb_url + entry.serial + "/title.txt");
            net::DownloadResult cover = net::download(game_covers_url + entry.serial + ".jpg");

            if (region.status == 200) {
                entry.region = region.body;
            }

            if (title.status == 200) {
                entry.name = title.body;
            }

            if (cover.status != 200) {
                iris_error(&iris->log.gamelist, "Failed to download cover for {} ({})", entry.name.c_str(), entry.serial.c_str());
                // entry.cover = cover.body;
            }

            gamelist_cache_progress++;
        }

        gamelist_cache_status = GAMELIST_CACHE_READY;

        return;
    }

    std::thread thread_pool[thread_pool_size];
    std::array <std::atomic <bool>, thread_pool_size> thread_free;

    for (int i = 0; i < thread_free.size(); i++) {
        thread_free[i] = true;
    }

    int j = 0;

    while (gamelist_cache_progress != gamelist_cache_total) {
        // No more tasks to assign, wait for threads to finish
        if (j == gamelist_cache.size()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            continue;
        }

        for (int i = 0; i < thread_pool_size; i++) {
            int r = rand() % thread_pool_size;

            if (!thread_free[r]) continue;

            thread_free[r] = false;

            thread_pool[r] = std::thread([&thread_free, iris](int id, int index){
                const std::string gamedb_url = "https://raw.githubusercontent.com/niemasd/GameDB-PS2/refs/heads/main/games/";
                const std::string game_covers_url = "https://raw.githubusercontent.com/xlenore/ps2-covers/refs/heads/main/covers/default/";

                std::string serial = gamelist_cache[index].serial;

                net::DownloadResult region = net::download(gamedb_url + serial + "/region.txt");
                net::DownloadResult title = net::download(gamedb_url + serial + "/title.txt");
                net::DownloadResult cover = net::download(game_covers_url + serial + ".jpg");

                if (region.status == 200) gamelist_cache[index].region = region.body;
                if (title.status == 200) gamelist_cache[index].name = title.body;

                if (cover.status == 200) {
                    gamelist_cache[index].cover = cover.body;
                } else {
                    iris_error(&iris->log.gamelist, "Failed to download cover for {} ({})",
                        gamelist_cache[index].name.c_str(), serial.c_str()
                    );
                }

                gamelist_cache_progress++;

                thread_free[id].store(true);
            }, r, j);

            thread_pool[r].detach();

            j++;

            if (j == gamelist_cache.size()) {
                break;
            }
        }
    }

    // if (gamelist_cache.size()) {
    //     FILE* cache_file = fopen((iris->paths.pref_path + "gamelist_cache.txt").c_str(), "w");

    //     for (const auto& entry : gamelist_cache) {
    //         fprintf(cache_file, "%s|%s|%s|%s|%s|%s\n", entry.name.c_str(), entry.path.c_str(), entry.format.c_str(), entry.type.c_str(), entry.region.c_str(), entry.serial.c_str());
    //     }

    //     fclose(cache_file);
    // }

    // for (const auto& entry : gamelist_cache) {
    //     if (!entry.cover.empty()) {
    //         FILE* f = fopen((iris->paths.pref_path + "covers/" + entry.serial + ".jpg").c_str(), "wb");

    //         if (f) {
    //             fwrite(entry.cover.data(), 1, entry.cover.size(), f);
    //             fclose(f);
    //         }
    //     }
    // }

    gamelist_cache_full = gamelist_cache;
    gamelist_cache_status = GAMELIST_CACHE_READY;
}

void draw_badge(const char* text, ImU32 color, float bg_alpha = 0.5f, float rounding = 5.0f, int x_pad = 5, int y_pad = 5) {
    using namespace ImGui;

    auto cursor = GetCursorScreenPos();
    auto size = CalcTextSize(text);
    auto text_color = ColorConvertU32ToFloat4(color);
    auto bg_color = text_color;

    text_color.w = 1.0f;
    bg_color.w = bg_alpha;

    cursor.y -= y_pad;

    GetForegroundDrawList()->AddRectFilled(
        cursor,
        ImVec2(
            cursor.x + size.x + x_pad*2,
            cursor.y + size.y + y_pad*2
        ),
        ColorConvertFloat4ToU32(bg_color),
        rounding
    );

    SetCursorPosX(GetCursorPosX() + x_pad);

    TextColored(text_color, "%s", text);
}

void draw_table(Instance* iris) {
    using namespace ImGui;

    if (BeginTable("##gamelist_tabs", 4, ImGuiTableFlags_Sortable | ImGuiTableFlags_NoBordersInBody)) {
        TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, 100);
        TableSetupColumn("Title");
        TableSetupColumn("Region", ImGuiTableColumnFlags_WidthFixed, 60);
        TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 60);
        // TableHeadersRow();

        int i = 0;

        int height = 55;
        int height2 = height / 2;
        int padding = 1;

        PushFont(iris->ui.font_heading);
        int title_height = CalcTextSize("Aa").y;
        PopFont();

        int subtitle_height = CalcTextSize("Aa").y;

        //ImGuiListClipper clipper;
        //clipper.Begin(gamelist_cache.size());

        for (const gamelist_entry& entry : gamelist_cache) {
            //TableSetColumnIndex(0);
            TableNextColumn();

            float cursor_pos_y = GetCursorPosY();

            if (Selectable(std::string("##" + std::to_string(i)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, height))) {
                add_recent(iris, entry.path, RecentType::PS2);
                emu::open_file(iris, entry.path);
            }

            SetCursorPosY(cursor_pos_y);

            if (BeginChild(("icon" + std::to_string(i)).c_str(), ImVec2(0, height), 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs)) {
                if (iris->ui.covers.contains(entry.serial)) {
                    Texture& cover = iris->ui.covers[entry.serial];

                    float width = ((float)cover.width / (float)cover.height) * (float)height;

                    SetCursorPosX(GetContentRegionAvail().x / 2 - width / 2);

                    Image(cover.descriptor_set, ImVec2(width, (float)height));
                } else {
                    SetWindowFontScale(2.0f);

                    auto text_size = CalcTextSize(ICON_MS_INDETERMINATE_QUESTION_BOX);

                    SetCursorPosX(GetContentRegionAvail().x / 2 - text_size.x / 2);
                    SetCursorPosY(height2 - text_size.y / 2);

                    TextDisabled(ICON_MS_INDETERMINATE_QUESTION_BOX);
                    SetWindowFontScale(1.0f);
                }
            } EndChild();

            if (IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                if (BeginTooltip()) {
                    if (!iris->ui.covers.contains(entry.serial)) {
                        Text("%s", entry.name.c_str());
                        TextDisabled("%s", entry.path.c_str());
                    } else {
                        Texture& cover = iris->ui.covers[entry.serial];

                        float max_height = 300.0f;
                        float width = ((float)cover.width / (float)cover.height) * max_height;

                        Image(cover.descriptor_set, ImVec2(width, max_height));
                        SetNextItemWidth(width);
                        Separator();
                        Text("%s", entry.name.c_str());
                        TextDisabled("%s", entry.path.c_str());
                    }

                    EndTooltip();
                }
            }

            TableNextColumn();

            if (BeginChild(("title" + std::to_string(i)).c_str(), ImVec2(0, height), 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs)) {
                SetCursorPosY(height2 - title_height);
                PushFont(iris->ui.font_heading);
                Text("%s", entry.name.c_str());
                PopFont();
                SetCursorPosY(height2 + padding * 2);
                TextDisabled("%s", (entry.type + " • " + entry.serial).c_str());
            } EndChild();

            TableNextColumn();
            if (BeginChild(("region" + std::to_string(i)).c_str(), ImVec2(0, height), 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs)) {
                SetCursorPosX(GetContentRegionAvail().x / 2 - CalcTextSize(entry.region.c_str()).x / 2);
                SetCursorPosY(height2 - subtitle_height / 2);
                TextDisabled("%s", entry.region.c_str());
            } EndChild();


            TableNextColumn();
            if (BeginChild(("format" + std::to_string(i)).c_str(), ImVec2(0, height), 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs)) {
                SetCursorPosY(height2 - subtitle_height / 2);
                SetCursorPosX(GetContentRegionAvail().x / 2 - CalcTextSize(entry.format.c_str()).x / 2 - 7);

                draw_badge(entry.format.c_str(), 0x22C55E, 0.3f, 5.0f, 7, 3);
            } EndChild();

            i++;
        }

        EndTable();
    }
}

void gamelib_filter_symbols(Instance* iris, const std::string& filter, bool regex, bool case_sensitive) {
    gamelist_cache.clear();

    if (filter[0] == '\0') {
        gamelist_cache = gamelist_cache_full;

        return;
    }

    std::string filter_str(filter);

    for (const gamelist_entry& entry : gamelist_cache_full) {
        if (regex) {
            std::regex r(filter_str, std::regex::ECMAScript | (case_sensitive ? std::regex_constants::syntax_option_type(0) : std::regex::icase));

            if (std::regex_match(entry.name, r)) {
                gamelist_cache.push_back(entry);
            }
        } else {
            std::string str(entry.name);

            if (!case_sensitive) {
                std::transform(str.begin(), str.end(), str.begin(), tolower);
                std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), tolower);
            }

            auto it = str.find(filter_str);

            if (it != std::string::npos) {
                gamelist_cache.push_back(entry);
            }
        }
    }
}

int gamelib_edit_callback(ImGuiInputTextCallbackData* data) {
    Instance* iris = (Instance*)data->UserData;

    gamelib_filter_symbols(iris, data->Buf, gamelib_regex, gamelib_case_sensitive);

    return 0;
}

void show_gamelist(Instance* iris) {
    using namespace ImGui;

    if (!gamelist_cache_thread_started) {
        gamelist_cache_thread_started = true;

        std::thread(make_gamelist_cache, iris, "X:\\Games\\PS2\\dvd").detach();
    }

    if (gamelist_cache_status == GAMELIST_CACHE_READY) {
        iris_info(&iris->log.gamelist, "Loading covers into GPU...");

        for (auto& entry : gamelist_cache) {
            if (entry.cover.empty())
                continue;

            Texture tex = vulkan::load_texture_from_memory(iris, entry.cover.data(), entry.cover.size());

            iris->ui.covers[entry.serial] = tex;
        }

        gamelist_cache_status = GAMELIST_CACHE_LOADED_COVERS;
    }

    if ((gamelist_cache_status != GAMELIST_CACHE_READY) && (gamelist_cache_status != GAMELIST_CACHE_LOADED_COVERS)) {
        std::string status_text;

        switch (gamelist_cache_status) {
            case GAMELIST_CACHE_NOT_READY: status_text = "Initializing..."; break;
            case GAMELIST_CACHE_PARSING: status_text = "Loading games..."; break;
            case GAMELIST_CACHE_DOWNLOADING_ASSETS: status_text = "Downloading assets... (%d/%d)"; break;
        }

        char buf[128];

        sprintf(buf, status_text.c_str(), gamelist_cache_progress.load(), gamelist_cache_total.load());

        auto size = CalcTextSize(buf);

        ImVec2 cursor_pos = GetCursorPos();

        cursor_pos.x += (GetContentRegionAvail().x - size.x) / 2;
        cursor_pos.y += (GetContentRegionAvail().y - size.y) / 2;

        ImGui::SetCursorPos(cursor_pos);

        Text("%s", buf);

        return;
    }

    static char buf[512];

    SetNextItemWidth(200.0f);

    ImGuiInputFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;

    if (gamelib_autosearch) {
        flags |= ImGuiInputTextFlags_CallbackEdit;
    }

    if (InputTextWithHint("##search", "Search symbols...", buf, 512, flags, gamelib_edit_callback, (void*)iris)) {
        gamelib_filter_symbols(iris, buf, gamelib_regex, gamelib_case_sensitive);
    } SameLine();

    if (Button(ICON_MS_SEARCH)) {
        gamelib_filter_symbols(iris, buf, gamelib_regex, gamelib_case_sensitive);
    }

    // if (BeginPopupContextItem("symbols_settings")) {
    //     if (imgui::MenuItem(ICON_MS_REGULAR_EXPRESSION " Regex mode", NULL, &gamelib_regex)) {
    //         gamelib_filter_symbols(iris, buf, gamelib_regex, gamelib_case_sensitive);
    //     }

    //     if (imgui::MenuItem(ICON_MS_MATCH_CASE " Case-sensitive", NULL, &gamelib_case_sensitive)) {
    //         gamelib_filter_symbols(iris, buf, gamelib_regex, gamelib_case_sensitive);
    //     }

    //     EndPopup();
    // }

    if (BeginChild("gamelist_child", ImVec2(0, GetContentRegionAvail().y - 35), false, ImGuiWindowFlags_NoScrollbar)) {
        draw_table(iris);
    } EndChild();
    
    Button("A");
}

}