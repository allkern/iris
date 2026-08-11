#include <cctype>
#include <cstring>
#include <filesystem>

#include "fs/exfat.hpp"
#include "fs/fat.hpp"
#include "fs/fs.hpp"
#include "fs/iso9660.hpp"
#include "fs/ps1mcd.hpp"
#include "fs/ps2mcd.hpp"

namespace iris::fs {

Fs* probe(logger::Logger* logger, blk::Device* dev, bool take_ownership) {
    if (!dev)
        return nullptr;

    if (Fs* fs = ps2mcd::open(logger, dev, take_ownership))
        return fs;

    if (Fs* fs = ps1mcd::open(logger, dev, take_ownership))
        return fs;

    if (Fs* fs = exfat::open(logger, dev, take_ownership))
        return fs;

    if (Fs* fs = fat::open(logger, dev, take_ownership))
        return fs;

    if (Fs* fs = iso9660::open(logger, dev, take_ownership))
        return fs;

    return nullptr;
}

int list(Fs* fs, const char* path, std::vector <Entry>* out) {
    out->clear();

    if (!fs)
        return FS_ERR_IO;

    return fs->list(fs->udata, path, out);
}

int stat(Fs* fs, const char* path, Entry* out) {
    return fs ? fs->stat(fs->udata, path, out) : FS_ERR_IO;
}

int open(Fs* fs, const char* path, Handle** out) {
    *out = nullptr;

    return fs ? fs->open(fs->udata, path, out) : FS_ERR_IO;
}

int64_t read(Fs* fs, Handle* handle, uint64_t offset, void* buf, uint64_t size) {
    if (!fs || !handle)
        return -1;

    if (!size)
        return 0;

    return fs->read(fs->udata, handle, offset, buf, size);
}

void close_handle(Fs* fs, Handle* handle) {
    if (fs && handle)
        fs->close_handle(fs->udata, handle);
}

void close(Fs* fs) {
    if (!fs)
        return;

    fs->close(fs->udata);

    if (fs->owns_dev)
        blk::close(fs->dev);

    delete fs;
}

const char* type_name(const Fs* fs) {
    if (fs && fs->variant[0])
        return fs->variant;

    return type_name(fs ? fs->type : FS_NONE);
}

const char* type_name(int type) {
    switch (type) {
        case FS_PS2_MCD: return "PS2 memory card";
        case FS_PS1_MCD: return "PS1 memory card";
        case FS_FAT: return "FAT";
        case FS_EXFAT: return "exFAT";
        case FS_ISO9660: return "ISO 9660";
        case FS_APA: return "APA";
        case FS_PFS: return "PFS";
    }

    return "Unknown";
}

const char* error_name(int error) {
    switch (error) {
        case FS_OK: return "OK";
        case FS_ERR_NOT_FOUND: return "No such file or directory";
        case FS_ERR_NOT_DIRECTORY: return "Not a directory";
        case FS_ERR_IS_DIRECTORY: return "Is a directory";
        case FS_ERR_IO: return "Read error";
        case FS_ERR_CORRUPT: return "Image is corrupt";
        case FS_ERR_UNSUPPORTED: return "Unsupported";
    }

    return "Unknown error";
}

bool sanitize_name(const char* in, std::string* out) {
    out->clear();

    if (!in || !*in || !strcmp(in, ".") || !strcmp(in, ".."))
        return false;

    for (const char* p = in; *p; p++) {
        unsigned char c = (unsigned char)*p;

        if (c < 0x20 || strchr("/\\:*?\"<>|", c))
            return false;

        *out += (char)c;
    }

    // Windows drops these silently, which would let "a. " land on top of "a"
    while (out->size() && (out->back() == '.' || out->back() == ' '))
        out->pop_back();

    if (out->empty())
        return false;

    static const char* const reserved[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };

    std::string stem;

    for (char c : *out) {
        if (c == '.')
            break;

        stem += (char)toupper((unsigned char)c);
    }

    for (const char* r : reserved) {
        if (stem == r)
            return false;
    }

    return true;
}

bool path_is_inside(const std::string& root, const std::string& path) {
    std::error_code ec;

    std::filesystem::path a = std::filesystem::weakly_canonical(std::filesystem::path(root), ec);

    if (ec)
        return false;

    std::filesystem::path b = std::filesystem::weakly_canonical(std::filesystem::path(path), ec);

    if (ec)
        return false;

    auto ai = a.begin();
    auto bi = b.begin();

    for (; ai != a.end(); ++ai, ++bi) {
        if (bi == b.end() || *ai != *bi)
            return false;
    }

    return true;
}

std::string path_join(const std::string& dir, const std::string& name) {
    if (dir.empty() || dir == "/")
        return "/" + name;

    return dir.back() == '/' ? dir + name : dir + "/" + name;
}

std::string path_parent(const std::string& path) {
    size_t slash = path.find_last_of('/');

    if (slash == std::string::npos || slash == 0)
        return "/";

    return path.substr(0, slash);
}

const char* path_basename(const char* path) {
    const char* slash = strrchr(path, '/');

    return slash ? slash + 1 : path;
}

std::vector <std::string> path_split(const std::string& path) {
    std::vector <std::string> out;

    size_t i = 0;

    while (i < path.size()) {
        size_t slash = path.find('/', i);

        if (slash == std::string::npos)
            slash = path.size();

        if (slash > i)
            out.push_back(path.substr(i, slash - i));

        i = slash + 1;
    }

    return out;
}

void utf16_to_utf8(const uint16_t* in, size_t count, char* out, size_t out_size, bool* truncated) {
    size_t o = 0;

    if (truncated)
        *truncated = false;

    for (size_t i = 0; i < count; i++) {
        uint32_t cp = in[i];

        if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < count && in[i + 1] >= 0xdc00 && in[i + 1] <= 0xdfff) {
            cp = 0x10000 + ((cp - 0xd800) << 10) + (in[++i] - 0xdc00);
        } else if (cp >= 0xd800 && cp <= 0xdfff) {
            cp = 0xfffd;
        }

        size_t need = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;

        if (o + need >= out_size) {
            if (truncated)
                *truncated = true;

            break;
        }

        if (cp < 0x80) {
            out[o++] = (char)cp;
        } else if (cp < 0x800) {
            out[o++] = (char)(0xc0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3f));
        } else if (cp < 0x10000) {
            out[o++] = (char)(0xe0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[o++] = (char)(0x80 | (cp & 0x3f));
        } else {
            out[o++] = (char)(0xf0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3f));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[o++] = (char)(0x80 | (cp & 0x3f));
        }
    }

    out[o] = '\0';
}

}
