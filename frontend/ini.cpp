#include "ini.hpp"

#include <cctype>
#include <fstream>

namespace iris::ini {

static std::string trim(std::string text) {
    size_t start = text.find_first_not_of(" \t\r\n");

    if (start == std::string::npos)
        return "";

    size_t end = text.find_last_not_of(" \t\r\n");

    return text.substr(start, end - start + 1);
}

static std::string fold(std::string text) {
    for (char& c : text)
        c = tolower(c);

    return text;
}

bool load(const std::filesystem::path& path, File* file) {
    std::ifstream stream(path);

    if (!stream.is_open())
        return false;

    std::string section;
    std::string line;

    while (std::getline(stream, line)) {
        line = trim(line);

        if (!line.size() || line[0] == ';' || line[0] == '#')
            continue;

        if (line.front() == '[') {
            size_t end = line.find(']');

            if (end == std::string::npos)
                continue;

            section = fold(trim(line.substr(1, end - 1)));

            continue;
        }

        size_t separator = line.find('=');

        if (separator == std::string::npos)
            continue;

        std::string key = fold(trim(line.substr(0, separator)));

        if (!key.size())
            continue;

        (*file)[section][key] = trim(line.substr(separator + 1));
    }

    return true;
}

std::string value(const File& file, std::string section, std::string key, std::string fallback) {
    auto section_it = file.find(fold(std::move(section)));

    if (section_it == file.end())
        return fallback;

    auto key_it = section_it->second.find(fold(std::move(key)));

    if (key_it == section_it->second.end())
        return fallback;

    return key_it->second;
}

}
