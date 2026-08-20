#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace iris::ini {

using Section = std::map<std::string, std::string>;
using File = std::map<std::string, Section>;

bool load(const std::filesystem::path& path, File* file);
std::string value(const File& file, std::string section, std::string key, std::string fallback = "");

}
