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
