#include <filesystem>
#include <thread>
#include <atomic>

#include "iris.hpp"

namespace iris::gamelist {

// void make_gamelist_cache(instance* iris, std::string path) {
//     for (const auto& entry : std::filesystem::directory_iterator(path)) {
//         if (entry.is_regular_file()) {
//             std::string ext = entry.path().extension().string();

//             for (char& c : ext) {
//                 c = std::tolower(c);
//             }

//             if (ext == ".iso" || ext == ".bin" || ext == ".cue" || ext == ".chd" || ext == ".cso" || ext == ".zso") {
//                 iop::disc::Disc* disc = iop::disc::open(entry.path().string().c_str());

//                 gamelist_entry e;

//                 e.name = entry.path().stem().string();
//                 e.path = entry.path().string();
//                 e.format = upper(ext.substr(1));
//                 e.type = "Unknown";
//                 e.region = "Unknown";
//                 e.serial = "Unknown";

//                 if (disc) {
//                     parse_disc(disc, &e);
//                     iop::disc::close(disc);
//                 }

//                 gamelist_cache.push_back(e);
//             }
//         }
//     }
// }

bool init(instance* iris) {
    return true;
}

void destroy(instance* iris) {
}

}