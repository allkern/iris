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

#include "ioman.h"

#include "../iop_def.hpp"
#include "../bus.h"
#include "../iop_export.h"

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

std::string ioman_read_string(struct iop_state* iop, uint32_t addr) {
    std::string str;

    for (int i = 0; i < 256; i++) {
        uint8_t d = iop_read8(iop, addr + i);

        if (!d)
            break;

        str += d;
    }

    return str;
}

void ioman_read_ptr(struct iop_state* iop, uint32_t addr, void* buf, int size) {
    unsigned char* ptr = (unsigned char*)buf;

    for (int i = 0; i < size; i++) {
        ptr[i] = iop_read8(iop, addr + i);
    }
}

void ioman_write_ptr(struct iop_state* iop, uint32_t addr, const void* buf, int size) {
    const unsigned char* ptr = (const unsigned char*)buf;

    for (int i = 0; i < size; i++) {
        iop_write8(iop, addr + i, ptr[i]);
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

struct ioman_dirent {
    std::vector<std::filesystem::directory_entry>* entries;
    int index;
};

struct ioman_hle_state {
    FILE* files[IOMAN_MAX_OPEN_FILES] = { nullptr };
    ioman_dirent directories[IOMAN_MAX_OPEN_FILES] = {};
} state;

static inline int ioman_allocate_file(FILE* file) {
    for (int i = 0; i < IOMAN_MAX_OPEN_FILES; i++) {
        if (!state.files[i]) {
            state.files[i] = file;

            return i;
        }
    }

    // No free file slots
    return -1;
}

static inline int ioman_allocate_directory(std::vector<std::filesystem::directory_entry>* entries) {
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

extern "C" void ioman_hle_map_device(const char* device, const char* host_path) {
    if (!device || !host_path)
        return;

    g_device_map[device] = host_path;
}

extern "C" void ioman_hle_unmap_device(const char* device) {
    if (!device)
        return;

    g_device_map.erase(device);
}

extern "C" void ioman_hle_clear_devices(void) {
    g_device_map.clear();
}

static bool ioman_resolve_device_path(const std::string& path, std::filesystem::path& out) {
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

static FILE* ioman_open_host(const std::filesystem::path& path, int mode) {
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

static void ioman_fill_time(unsigned char* dst, std::filesystem::file_time_type ft) {
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

static void ioman_fill_stat(iomanx_stat* st, const std::filesystem::directory_entry& e) {
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
        ioman_fill_time(st->ctime, mtime);
        ioman_fill_time(st->atime, mtime);
        ioman_fill_time(st->mtime, mtime);
    }
}

static size_t ioman_stat_size(int iomanx) {
    return iomanx ? sizeof(iomanx_stat) : offsetof(iomanx_stat, private_0);
}

static bool ioman_host_path(struct iop_state* iop, uint32_t addr, std::filesystem::path& out) {
    return ioman_resolve_device_path(ioman_read_string(iop, addr), out);
}

extern "C" int ioman_open(struct iop_state* iop, int iomanx) {
    int mode = iop->r[5];

    std::filesystem::path absolute;

    if (!ioman_host_path(iop, iop->r[4], absolute))
        return 0;

    FILE* file = ioman_open_host(absolute, mode);

    if (!file)
        return 0;

    int slot = ioman_allocate_file(file);

    if (slot == -1) {
        fclose(file);

        return 0;
    }

    printf("ioman: open %s -> fd=%d\n", absolute.string().c_str(), IOMAN_HLE_FD_START + slot);

    // Return file handle
    iop_return(iop, IOMAN_HLE_FD_START + slot);

    return 1;
}
extern "C" int ioman_close(struct iop_state* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= IOMAN_HLE_FD_START && fd < IOMAN_HLE_FD_START + IOMAN_MAX_OPEN_FILES))
        return 0;

    fd -= IOMAN_HLE_FD_START;

    if (state.files[fd])
        fclose(state.files[fd]);

    state.files[fd] = nullptr;

    iop_return(iop, 0);

    return 1;
}
extern "C" int ioman_read(struct iop_state* iop, int iomanx) {
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
        iop_return(iop, -1);

        return 1;
    }

    size_t ret = fread(buf, 1, size, state.files[fd]);

    for (size_t i = 0; i < ret; i++) {
        iop_write8(iop, ptr + i, buf[i]);
    }

    free(buf);

    iop_return(iop, (int)ret);

    return 1;
}
extern "C" int ioman_write(struct iop_state* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    // We only use this to HLE IOMAN stdout writes
    // if (fd != 1)
    //     return 0;

    // printf("%s: write fd=%d\n", iomanx ? "iomanx" : "ioman", fd);

    if (fd >= IOMAN_HLE_FD_START && fd < IOMAN_HLE_FD_START + IOMAN_MAX_OPEN_FILES) {
        fd -= IOMAN_HLE_FD_START;

        if (!state.files[fd])
            return 0;

        uint32_t ptr = iop->r[5];
        uint32_t size = iop->r[6];

        uint8_t* buf = (uint8_t*)malloc(size);

        for (int i = 0; i < size; i++) {
            buf[i] = iop_read8(iop, ptr + i);
        }

        int ret = fwrite(buf, 1, size, state.files[fd]);

        free(buf);

        iop_return(iop, ret);

        return 1;
    } else if (fd == 1) {
        // HLE IOMAN stdout writes
        uint32_t ptr = iop->r[5];
        uint32_t size = iop->r[6] & 0xfff;

        char c = iop_read8(iop, ptr++);
        int cnt = 0;

        while (c && ((cnt++) != size)) {
            iop->kputchar(iop->kputchar_udata, c);

            c = iop_read8(iop, ptr++);
        }

        fflush(stdout);

        iop_return(iop, size);

        return 1;
    }

    return 0;
}
extern "C" int ioman_lseek(struct iop_state* iop, int iomanx) {
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

    iop_return(iop, ret);

    return 1;
}
extern "C" int ioman_ioctl(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_remove(struct iop_state* iop, int iomanx) {
    std::filesystem::path path;

    if (!ioman_host_path(iop, iop->r[4], path))
        return 0;

    std::error_code ec;
    bool ok = std::filesystem::remove(path, ec);

    iop_return(iop, (ok && !ec) ? 0 : -1);

    return 1;
}
extern "C" int ioman_mkdir(struct iop_state* iop, int iomanx) {
    std::filesystem::path path;

    if (!ioman_host_path(iop, iop->r[4], path))
        return 0;

    std::error_code ec;

    if (std::filesystem::exists(path, ec)) {
        iop_return(iop, -1);

        return 1;
    }

    std::filesystem::create_directory(path, ec);

    iop_return(iop, ec ? -1 : 0);

    return 1;
}
extern "C" int ioman_rmdir(struct iop_state* iop, int iomanx) {
    std::filesystem::path path;

    if (!ioman_host_path(iop, iop->r[4], path))
        return 0;

    std::error_code ec;

    if (!std::filesystem::is_directory(path, ec)) {
        iop_return(iop, -1);

        return 1;
    }

    bool ok = std::filesystem::remove(path, ec);

    iop_return(iop, (ok && !ec) ? 0 : -1);

    return 1;
}
extern "C" int ioman_dopen(struct iop_state* iop, int iomanx) {
    std::filesystem::path absolute;

    if (!ioman_host_path(iop, iop->r[4], absolute))
        return 0;

    std::error_code ec;

    if (!std::filesystem::is_directory(absolute, ec)) {
        fprintf(stderr, "ioman: Directory \'%s\' does not exist!\n", absolute.string().c_str());

        return 0;
    }

    auto* entries = new std::vector<std::filesystem::directory_entry>();

    for (const auto& e : std::filesystem::directory_iterator(absolute, ec))
        entries->push_back(e);

    int slot = ioman_allocate_directory(entries);

    if (slot == -1) {
        delete entries;

        return 0;
    }

    iop_return(iop, IOMAN_HLE_FD_END + slot);

    return 1;
}
extern "C" int ioman_dclose(struct iop_state* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= IOMAN_HLE_FD_END && fd < IOMAN_HLE_FD_END + IOMAN_MAX_OPEN_FILES))
        return 0;

    fd -= IOMAN_HLE_FD_END;

    if (state.directories[fd].entries)
        delete state.directories[fd].entries;

    state.directories[fd].entries = nullptr;
    state.directories[fd].index = 0;

    iop_return(iop, 0);

    return 1;
}
extern "C" int ioman_dread(struct iop_state* iop, int iomanx) {
    uint32_t fd = iop->r[4];
    uint32_t ptr = iop->r[5];

    if (!(fd >= IOMAN_HLE_FD_END && fd < IOMAN_HLE_FD_END + IOMAN_MAX_OPEN_FILES))
        return 0;

    fd -= IOMAN_HLE_FD_END;

    ioman_dirent* dir = &state.directories[fd];

    if (!dir->entries)
        return 0;

    if (dir->index >= (int)dir->entries->size()) {
        iop_return(iop, 0);

        return 1;
    }

    const std::filesystem::directory_entry& entry = (*dir->entries)[dir->index];

    size_t stat_size = ioman_stat_size(iomanx);

    uint8_t buf[sizeof(iomanx_stat) + sizeof(iomanx_dirent::name) + sizeof(uint32_t)];
    memset(buf, 0, sizeof(buf));

    iomanx_stat st;
    ioman_fill_stat(&st, entry);
    memcpy(buf, &st, stat_size);

    std::string name = entry.path().filename().string();
    strncpy((char*)(buf + stat_size), name.c_str(), 255);

    size_t total = stat_size + sizeof(iomanx_dirent::name) + sizeof(uint32_t);
    ioman_write_ptr(iop, ptr, buf, (int)total);

    dir->index++;

    iop_return(iop, 1);

    return 1;
}
extern "C" int ioman_getstat(struct iop_state* iop, int iomanx) {
    std::filesystem::path absolute;

    if (!ioman_host_path(iop, iop->r[4], absolute))
        return 0;

    uint32_t stat_ptr = iop->r[5];

    std::error_code ec;

    if (!std::filesystem::exists(absolute, ec)) {
        iop_return(iop, -1);

        return 1;
    }

    iomanx_stat st;
    ioman_fill_stat(&st, std::filesystem::directory_entry(absolute, ec));

    ioman_write_ptr(iop, stat_ptr, &st, (int)ioman_stat_size(iomanx));

    iop_return(iop, 0);

    return 1;
}
extern "C" int ioman_chstat(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_format(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_adddrv(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_deldrv(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_stdioinit(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_rename(struct iop_state* iop, int iomanx) {
    std::filesystem::path from, to;

    if (!ioman_host_path(iop, iop->r[4], from))
        return 0;

    if (!ioman_host_path(iop, iop->r[5], to))
        return 0;

    std::error_code ec;
    std::filesystem::rename(from, to, ec);

    iop_return(iop, ec ? -1 : 0);

    return 1;
}
extern "C" int ioman_chdir(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_sync(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_mount(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_umount(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_lseek64(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_devctl(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_symlink(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_readlink(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_ioctl2(struct iop_state* iop, int iomanx) { return 0; }

extern "C" void ioman_hle_reset(void) {
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