#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>

#include "ioman.hpp"

#include "../iop_def.hpp"
#include "../bus.hpp"
#include "../iop_export.hpp"

namespace iris::iop::hle::ioman {

#define IOMAN_MAX_OPEN_FILES 512
#define IOMAN_HLE_FD_START 0x7000
#define IOMAN_HLE_FD_END   0x7800

/** Format mask */
#define FIO_SO_IFMT  0x0038
/** Symbolic link */
#define FIO_SO_IFLNK 0x0008
/** Regular file */
#define FIO_SO_IFREG 0x0010
/** Directory */
#define FIO_SO_IFDIR 0x0020

/** read */
#define FIO_SO_IROTH 0x0004
/** write */
#define FIO_SO_IWOTH 0x0002
/** execute */
#define FIO_SO_IXOTH 0x0001

// Legacy ioman (sceIo/io_common.h) mode bits differ from iomanX's FIO_SO_*.
#define FIO_S_IFMT  0xf000
#define FIO_S_IFDIR 0x1000
#define FIO_S_IFREG 0x2000
#define FIO_S_IROTH 0x0004
#define FIO_S_IWOTH 0x0002
#define FIO_S_IXOTH 0x0001

#define FIO_O_RDONLY       0x0001
#define FIO_O_WRONLY       0x0002
#define FIO_O_RDWR         0x0003
#define FIO_O_DIROPEN      0x0008  // Internal use for dopen
#define FIO_O_NBLOCK       0x0010
#define FIO_O_APPEND       0x0100
#define FIO_O_CREAT        0x0200
#define FIO_O_TRUNC        0x0400
#define FIO_O_EXCL         0x0800
#define FIO_O_NOWAIT       0x8000

#define FIO_SEEK_SET       0
#define FIO_SEEK_CUR       1
#define FIO_SEEK_END       2

std::string read_string(iop::Iop* iop, uint32_t addr) {
    std::string str;

    for (int i = 0; i < 256; i++) {
        uint8_t d = iop::read8(iop, addr + i);

        if (!d)
            break;

        str += d;
    }

    return str;
}

void read_ptr(iop::Iop* iop, uint32_t addr, void* buf, int size) {
    unsigned char* ptr = (unsigned char*)buf;

    for (int i = 0; i < size; i++) {
        ptr[i] = iop::read8(iop, addr + i);
    }
}

void write_ptr(iop::Iop* iop, uint32_t addr, const void* buf, int size) {
    const unsigned char* ptr = (const unsigned char*)buf;

    for (int i = 0; i < size; i++) {
        iop::write8(iop, addr + i, ptr[i]);
    }
}

struct iomanx_stat {
    unsigned int mode;
    unsigned int attr;
    unsigned int size;
    unsigned char ctime[8];
    unsigned char atime[8];
    unsigned char mtime[8];
    unsigned int hisize;
    /** Number of subs (main) / subpart number (sub) */
    unsigned int private_0;
    unsigned int private_1;
    unsigned int private_2;
    unsigned int private_3;
    unsigned int private_4;
    /** Sector start.  */
    unsigned int private_5;
};

struct iomanx_dirent {
    iomanx_stat stat;
    char name[256];
    uint32_t privdata;
};

struct dirent {
    std::vector<std::filesystem::directory_entry>* entries;
    int index;
};

struct state {
    FILE* files[IOMAN_MAX_OPEN_FILES] = { nullptr };
    dirent directories[IOMAN_MAX_OPEN_FILES] = {};
} state;

static inline int allocate_file(FILE* file) {
    for (int i = 0; i < IOMAN_MAX_OPEN_FILES; i++) {
        if (!state.files[i]) {
            state.files[i] = file;

            return i;
        }
    }

    // No free file slots
    return -1;
}

static inline int allocate_directory(std::vector<std::filesystem::directory_entry>* entries) {
    for (int i = 0; i < IOMAN_MAX_OPEN_FILES; i++) {
        if (!state.directories[i].entries) {
            state.directories[i].entries = entries;
            state.directories[i].index = 0;

            return i;
        }
    }

    // No free directory slots
    return -1;
}

static std::map<std::string, std::string> g_device_map;

void map_device(const char* device, const char* host_path) {
    if (!device || !host_path)
        return;

    g_device_map[device] = host_path;
}

void unmap_device(const char* device) {
    if (!device)
        return;

    g_device_map.erase(device);
}

void clear_devices() {
    g_device_map.clear();
}

static bool resolve_device_path(const std::string& path, std::filesystem::path& out) {
    auto p = path.find_first_of(':');

    if (p == std::string::npos)
        return false;

    auto it = g_device_map.find(path.substr(0, p));

    if (it == g_device_map.end())
        return false;

    std::string rel = path.substr(p + 1);

    size_t start = rel.find_first_not_of(' ');

    rel = (start == std::string::npos) ? "" : rel.substr(start);

    while (rel.size() && (rel[0] == '/' || rel[0] == '\\'))
        rel = rel.substr(1);

    out = std::filesystem::path(it->second);

    if (rel.size())
        out /= rel;

    return true;
}

static FILE* open_host(const std::filesystem::path& path, int mode) {
    std::error_code ec;

    bool exists = std::filesystem::exists(path, ec);

    bool read = mode & FIO_O_RDONLY;
    bool write = mode & FIO_O_WRONLY;
    bool append = mode & FIO_O_APPEND;
    bool creat = mode & FIO_O_CREAT;
    bool trunc = mode & FIO_O_TRUNC;
    bool excl = mode & FIO_O_EXCL;

    if (creat && excl && exists)
        return nullptr;

    if (!creat && !exists)
        return nullptr;

    const char* m;

    if (append) {
        m = read ? "a+b" : "ab";
    } else if (trunc) {
        m = write ? (read ? "w+b" : "wb") : "w+b";
    } else if (creat && !exists) {
        m = "w+b";
    } else {
        m = write ? "r+b" : "rb";
    }

    return fopen(path.string().c_str(), m);
}

static void fill_time(unsigned char* dst, std::filesystem::file_time_type ft) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
    );

    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);

    std::tm* tm = std::localtime(&tt);

    if (!tm)
        return;

    unsigned int year = (unsigned int)(tm->tm_year + 1900);

    dst[0] = 0;
    dst[1] = (unsigned char)tm->tm_sec;
    dst[2] = (unsigned char)tm->tm_min;
    dst[3] = (unsigned char)tm->tm_hour;
    dst[4] = (unsigned char)tm->tm_mday;
    dst[5] = (unsigned char)(tm->tm_mon + 1);
    dst[6] = (unsigned char)(year & 0xff);
    dst[7] = (unsigned char)((year >> 8) & 0xff);
}

static void fill_stat(iomanx_stat* st, const std::filesystem::directory_entry& e) {
    memset(st, 0, sizeof(*st));

    std::error_code ec;

    bool is_dir = e.is_directory(ec);

    if (is_dir)
        st->mode = FIO_S_IFDIR | FIO_SO_IFDIR | FIO_SO_IROTH | FIO_SO_IWOTH | FIO_SO_IXOTH;
    else
        st->mode = FIO_S_IFREG | FIO_SO_IFREG | FIO_SO_IROTH | FIO_SO_IWOTH | FIO_SO_IXOTH;

    // Note: mass: is FAT-backed, i.e. the real driver reports te raw FAT attribute byte,
    //       and homebrews use it (attr & 0x10) for directory/loadable-file detection.
    st->attr = is_dir ? 0x10 : 0x20;

    if (!is_dir) {
        uintmax_t size = e.file_size(ec);

        if (!ec) {
            st->size = (uint32_t)size;
            st->hisize = (uint32_t)(size >> 32);
        }
    }

    auto mtime = e.last_write_time(ec);

    if (!ec) {
        fill_time(st->ctime, mtime);
        fill_time(st->atime, mtime);
        fill_time(st->mtime, mtime);
    }
}

static size_t stat_size(int iomanx) {
    return iomanx ? sizeof(iomanx_stat) : offsetof(iomanx_stat, private_0);
}

static bool host_path(iop::Iop* iop, uint32_t addr, std::filesystem::path& out) {
    return resolve_device_path(read_string(iop, addr), out);
}

const std::string WHITESPACE = " \n\r\t\f\v";

// Trim from the start (left)
void ltrim(std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    s.erase(0, start);
}

// Trim from the end (right)
void rtrim(std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    if (end != std::string::npos) {
        s.erase(end + 1);
    } else {
        s.clear(); // String is entirely whitespace
    }
}

// Trim from both ends
void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

int open(iop::Iop* iop, int iomanx) {
    int mode = iop->r[5];

    std::filesystem::path absolute;

    if (!host_path(iop, iop->r[4], absolute))
        return 0;

    std::string str = absolute.string();

    trim(str);

    absolute = std::filesystem::path(str);

    FILE* file = open_host(absolute, mode);

    if (!file) {
        return 0;
    }

    int slot = allocate_file(file);

    if (slot == -1) {
        fclose(file);

        return 0;
    }
    
    // Return file handle
    iop::set_return(iop, IOMAN_HLE_FD_START + slot);

    return 1;
}
int close(iop::Iop* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= IOMAN_HLE_FD_START && fd < IOMAN_HLE_FD_START + IOMAN_MAX_OPEN_FILES))
        return 0;

    fd -= IOMAN_HLE_FD_START;

    if (state.files[fd])
        fclose(state.files[fd]);

    state.files[fd] = nullptr;

    iop::set_return(iop, 0);

    return 1;
}
int read(iop::Iop* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= IOMAN_HLE_FD_START && fd < IOMAN_HLE_FD_START + IOMAN_MAX_OPEN_FILES))
        return 0;

    fd -= IOMAN_HLE_FD_START;

    if (!state.files[fd])
        return 0;
    
    uint32_t ptr = iop->r[5];
    uint32_t size = iop->r[6];

    uint8_t* buf = (uint8_t*)malloc(size ? size : 1);

    if (!buf) {
        iop::set_return(iop, -1);

        return 1;
    }

    size_t ret = fread(buf, 1, size, state.files[fd]);

    for (size_t i = 0; i < ret; i++) {
        iop::write8(iop, ptr + i, buf[i]);
    }

    free(buf);

    iop::set_return(iop, (int)ret);

    return 1;
}
int write(iop::Iop* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    // We only use this to HLE IOMAN stdout writes
    // if (fd != 1)
    //     return 0;

    // iris_debug(ioman, "{}: write fd={}", iomanx ? "iomanx" : "ioman", fd);

    if (fd >= IOMAN_HLE_FD_START && fd < IOMAN_HLE_FD_START + IOMAN_MAX_OPEN_FILES) {
        fd -= IOMAN_HLE_FD_START;

        if (!state.files[fd])
            return 0;

        uint32_t ptr = iop->r[5];
        uint32_t size = iop->r[6];

        uint8_t* buf = (uint8_t*)malloc(size);

        for (int i = 0; i < size; i++) {
            buf[i] = iop::read8(iop, ptr + i);
        }

        int ret = fwrite(buf, 1, size, state.files[fd]);

        free(buf);

        iop::set_return(iop, ret);

        return 1;
    } else if (fd == 1) {
        // HLE IOMAN stdout writes
        uint32_t ptr = iop->r[5];
        uint32_t size = iop->r[6] & 0xfff;

        char c = iop::read8(iop, ptr++);
        int cnt = 0;

        while (c && ((cnt++) != size)) {
            iop->kputchar(iop->kputchar_udata, c);

            c = iop::read8(iop, ptr++);
        }

        fflush(stdout);

        iop::set_return(iop, size);

        return 1;
    }

    return 0;
}
int lseek(iop::Iop* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= IOMAN_HLE_FD_START && fd < IOMAN_HLE_FD_START + IOMAN_MAX_OPEN_FILES))
        return 0;

    fd -= IOMAN_HLE_FD_START;

    if (!state.files[fd])
        return 0;

    int32_t off = iop->r[5];
    uint32_t whence = iop->r[6];

    switch (whence) {
        case 0: fseek(state.files[fd], off, SEEK_SET); break;
        case 1: fseek(state.files[fd], off, SEEK_CUR); break;
        case 2: fseek(state.files[fd], off, SEEK_END); break;
    }

    int ret = ftell(state.files[fd]);

    iop::set_return(iop, ret);

    return 1;
}
int ioctl(iop::Iop* iop, int iomanx) { return 0; }
int remove(iop::Iop* iop, int iomanx) {
    std::filesystem::path path;

    if (!host_path(iop, iop->r[4], path))
        return 0;

    std::error_code ec;
    bool ok = std::filesystem::remove(path, ec);

    iop::set_return(iop, (ok && !ec) ? 0 : -1);

    return 1;
}
int mkdir(iop::Iop* iop, int iomanx) {
    std::filesystem::path path;

    if (!host_path(iop, iop->r[4], path))
        return 0;

    std::error_code ec;

    if (std::filesystem::exists(path, ec)) {
        iop::set_return(iop, -1);

        return 1;
    }

    std::filesystem::create_directory(path, ec);

    iop::set_return(iop, ec ? -1 : 0);

    return 1;
}
int rmdir(iop::Iop* iop, int iomanx) {
    std::filesystem::path path;

    if (!host_path(iop, iop->r[4], path))
        return 0;

    std::error_code ec;

    if (!std::filesystem::is_directory(path, ec)) {
        iop::set_return(iop, -1);

        return 1;
    }

    bool ok = std::filesystem::remove(path, ec);

    iop::set_return(iop, (ok && !ec) ? 0 : -1);

    return 1;
}
int dopen(iop::Iop* iop, int iomanx) {
    std::filesystem::path absolute;

    if (!host_path(iop, iop->r[4], absolute))
        return 0;

    std::error_code ec;

    if (!std::filesystem::is_directory(absolute, ec)) {

        return 0;
    }

    auto* entries = new std::vector<std::filesystem::directory_entry>();

    for (const auto& e : std::filesystem::directory_iterator(absolute, ec))
        entries->push_back(e);

    int slot = allocate_directory(entries);

    if (slot == -1) {
        delete entries;

        return 0;
    }

    iop::set_return(iop, IOMAN_HLE_FD_END + slot);

    return 1;
}
int dclose(iop::Iop* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= IOMAN_HLE_FD_END && fd < IOMAN_HLE_FD_END + IOMAN_MAX_OPEN_FILES))
        return 0;

    fd -= IOMAN_HLE_FD_END;

    if (state.directories[fd].entries)
        delete state.directories[fd].entries;

    state.directories[fd].entries = nullptr;
    state.directories[fd].index = 0;

    iop::set_return(iop, 0);

    return 1;
}
int dread(iop::Iop* iop, int iomanx) {
    uint32_t fd = iop->r[4];
    uint32_t ptr = iop->r[5];

    if (!(fd >= IOMAN_HLE_FD_END && fd < IOMAN_HLE_FD_END + IOMAN_MAX_OPEN_FILES))
        return 0;

    fd -= IOMAN_HLE_FD_END;

    dirent* dir = &state.directories[fd];

    if (!dir->entries)
        return 0;

    if (dir->index >= (int)dir->entries->size()) {
        iop::set_return(iop, 0);

        return 1;
    }

    const std::filesystem::directory_entry& entry = (*dir->entries)[dir->index];

    size_t st_size = stat_size(iomanx);

    uint8_t buf[sizeof(iomanx_stat) + sizeof(iomanx_dirent::name) + sizeof(uint32_t)];
    memset(buf, 0, sizeof(buf));

    iomanx_stat st;
    fill_stat(&st, entry);
    memcpy(buf, &st, st_size);

    std::string name = entry.path().filename().string();
    strncpy((char*)(buf + st_size), name.c_str(), 255);

    size_t total = st_size + sizeof(iomanx_dirent::name) + sizeof(uint32_t);
    write_ptr(iop, ptr, buf, (int)total);

    dir->index++;

    iop::set_return(iop, 1);

    return 1;
}
int getstat(iop::Iop* iop, int iomanx) {
    std::filesystem::path absolute;

    if (!host_path(iop, iop->r[4], absolute))
        return 0;

    uint32_t stat_ptr = iop->r[5];

    std::error_code ec;

    if (!std::filesystem::exists(absolute, ec)) {
        iop::set_return(iop, -1);

        return 1;
    }

    iomanx_stat st;
    fill_stat(&st, std::filesystem::directory_entry(absolute, ec));

    write_ptr(iop, stat_ptr, &st, (int)stat_size(iomanx));

    iop::set_return(iop, 0);

    return 1;
}
int chstat(iop::Iop* iop, int iomanx) { return 0; }
int format(iop::Iop* iop, int iomanx) { return 0; }
int adddrv(iop::Iop* iop, int iomanx) { return 0; }
int deldrv(iop::Iop* iop, int iomanx) { return 0; }
int stdioinit(iop::Iop* iop, int iomanx) { return 0; }
int rename(iop::Iop* iop, int iomanx) {
    std::filesystem::path from, to;

    if (!host_path(iop, iop->r[4], from))
        return 0;

    if (!host_path(iop, iop->r[5], to))
        return 0;

    std::error_code ec;
    std::filesystem::rename(from, to, ec);

    iop::set_return(iop, ec ? -1 : 0);

    return 1;
}
int chdir(iop::Iop* iop, int iomanx) { return 0; }
int sync(iop::Iop* iop, int iomanx) { return 0; }
int mount(iop::Iop* iop, int iomanx) { return 0; }
int umount(iop::Iop* iop, int iomanx) { return 0; }
int lseek64(iop::Iop* iop, int iomanx) { return 0; }
int devctl(iop::Iop* iop, int iomanx) { return 0; }
int symlink(iop::Iop* iop, int iomanx) { return 0; }
int readlink(iop::Iop* iop, int iomanx) { return 0; }
int ioctl2(iop::Iop* iop, int iomanx) { return 0; }

void reset() {
    for (int i = 0; i < IOMAN_MAX_OPEN_FILES; i++) {
        if (state.files[i]) {
            fclose(state.files[i]);

            state.files[i] = nullptr;
        }

        if (state.directories[i].entries) {
            delete state.directories[i].entries;

            state.directories[i].entries = nullptr;
        }

        state.directories[i].index = 0;
    }
}

}
