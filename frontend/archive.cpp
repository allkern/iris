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

#include "archive.hpp"

#include "miniz.h"

#include <cctype>

namespace iris::archive {

static std::string base_name(const std::string& path) {
    size_t sep = path.find_last_of("/\\");

    if (sep == std::string::npos)
        return path;

    return path.substr(sep + 1);
}

bool is_archive(const std::filesystem::path& path) {
    std::string ext = path.extension().string();

    for (char& c : ext)
        c = tolower(c);

    return ext == ".zip";
}

bool list(const std::filesystem::path& archive, std::vector<Entry>* entries) {
    mz_zip_archive zip;

    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, archive.string().c_str(), 0))
        return false;

    mz_uint count = mz_zip_reader_get_num_files(&zip);

    for (mz_uint i = 0; i < count; i++) {
        mz_zip_archive_file_stat stat;

        if (!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;

        Entry entry;

        entry.path = stat.m_filename;
        entry.name = base_name(entry.path);
        entry.size = stat.m_uncomp_size;
        entry.index = i;
        entry.directory = mz_zip_reader_is_file_a_directory(&zip, i);

        entries->push_back(std::move(entry));
    }

    mz_zip_reader_end(&zip);

    return true;
}

bool extract(const std::filesystem::path& archive, const Entry& entry, const std::filesystem::path& dst) {
    mz_zip_archive zip;

    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, archive.string().c_str(), 0))
        return false;

    std::error_code ec;

    std::filesystem::create_directories(dst.parent_path(), ec);

    bool ok = mz_zip_reader_extract_to_file(&zip, entry.index, dst.string().c_str(), 0);

    mz_zip_reader_end(&zip);

    return ok;
}

bool extract_all(const std::filesystem::path& archive, const std::filesystem::path& dst_dir) {
    std::vector<Entry> entries;

    if (!list(archive, &entries))
        return false;

    mz_zip_archive zip;

    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, archive.string().c_str(), 0))
        return false;

    std::error_code ec;

    bool ok = true;

    for (const Entry& entry : entries) {
        std::filesystem::path dst = dst_dir / entry.path;

        if (entry.directory) {
            std::filesystem::create_directories(dst, ec);

            continue;
        }

        std::filesystem::create_directories(dst.parent_path(), ec);

        if (!mz_zip_reader_extract_to_file(&zip, entry.index, dst.string().c_str(), 0))
            ok = false;
    }

    mz_zip_reader_end(&zip);

    return ok;
}

}
