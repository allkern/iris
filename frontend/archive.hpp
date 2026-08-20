#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace iris::archive {

struct Entry {
    std::string path;
    std::string name;
    uint64_t size;
    uint32_t index;
    bool directory;
};

bool is_archive(const std::filesystem::path& path);
bool list(const std::filesystem::path& archive, std::vector<Entry>* entries);
bool extract(const std::filesystem::path& archive, const Entry& entry, const std::filesystem::path& dst);
bool extract_all(const std::filesystem::path& archive, const std::filesystem::path& dst_dir);

}
