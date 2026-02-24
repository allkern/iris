#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <string>

#include "ioman.h"

#include "../iop.h"
#include "../bus.h"
#include "../iop_export.h"

#define IOMAN_MAX_OPEN_FILES 64

struct ioman_hle_state {
    FILE* files[IOMAN_MAX_OPEN_FILES] = { nullptr };
} state;

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

static inline int ioman_get_device(std::string path) {
    auto p = path.find_first_of(':');

    if (p == std::string::npos)
        return 0;

    std::string device = path.substr(0, p);

    if (device == "rom0") {
        return IOMAN_DEV_ROM0;
    } else if (device == "rom1") {
        return IOMAN_DEV_ROM1;
    } else if (device == "cdrom0") {
        return IOMAN_DEV_CDROM0;
    } else if (device == "host") {
        return IOMAN_DEV_HOST;
    } else if (device == "host0") {
        return IOMAN_DEV_HOST;        
    } else if (device == "mc0") {
        return IOMAN_DEV_MC0;
    } else if (device == "mc1") {
        return IOMAN_DEV_MC1;
    } else if (device == "mass") {
        return IOMAN_DEV_MASS;
    }

    // To-do: There's probably some ATA/DEV9/HDD device
    // but I have no idea how it's used

    return IOMAN_DEV_UNKNOWN;
}

extern "C" int ioman_open(struct iop_state* iop, int iomanx) {
    char buf[256];

    for (int i = 0; i < 256; i++) {
        uint8_t d = iop_read8(iop, iop->r[4] + i);

        buf[i] = d;

        if (!d)
            break;
    }

    std::string path(buf);

    int device = ioman_get_device(path);

    // printf("path=%s\n", path.c_str());

    // Only hook host files
    if (device != IOMAN_DEV_HOST && device != IOMAN_DEV_MASS)
        return 0;

    auto p = path.find_first_of(':');

    if (p == std::string::npos) return 0;

    // Get access path
    std::string str = path.substr(p + 1);

    if (!str.size()) return 0;

    if (str[0] == '/' || str[0] == '\\')
        str = str.substr(1);

    p = str.find_first_not_of(' ');

    if (p != std::string::npos)
        str = str.substr(p);

    std::filesystem::path absolute = std::filesystem::absolute(str);

    FILE* file = fopen(absolute.string().c_str(), "rb");

    if (!file)
        return 0;

    printf("ioman: Opened \'%s\'\n", absolute.string().c_str());

    int slot = ioman_allocate_file(file);

    // Return file handle
    iop_return(iop, 0x100 + slot);

    return 1;
}
extern "C" int ioman_close(struct iop_state* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= 0x100 && fd < 0x140))
        return 0;

    fd -= 0x100;

    if (state.files[fd])
        fclose(state.files[fd]);

    state.files[fd] = nullptr;

    iop_return(iop, 0);

    return 1;
}
extern "C" int ioman_read(struct iop_state* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= 0x100 && fd < 0x140))
        return 0;

    fd -= 0x100;

    if (!state.files[fd])
        return 0;
    
    uint32_t ptr = iop->r[5];
    uint32_t size = iop->r[6];

    uint8_t* buf = (uint8_t*)malloc(size);

    int ret = fread(buf, 1, size, state.files[fd]);

    for (int i = 0; i < size; i++) {
        iop_write8(iop, ptr + i, buf[i]);
    }

    free(buf);

    iop_return(iop, ret);

    return 1;
}
extern "C" int ioman_write(struct iop_state* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    // We only use this to HLE IOMAN stdout writes
    if (fd != 1)
        return 0;

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
extern "C" int ioman_lseek(struct iop_state* iop, int iomanx) {
    uint32_t fd = iop->r[4];

    if (!(fd >= 0x100 && fd < 0x140))
        return 0;

    fd -= 0x100;

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
extern "C" int ioman_remove(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_mkdir(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_rmdir(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_dopen(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_dclose(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_dread(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_getstat(struct iop_state* iop, int iomanx) {
    char buf[256];

    for (int i = 0; i < 256; i++) {
        uint8_t d = iop_read8(iop, iop->r[4] + i);

        buf[i] = d;

        if (!d)
            break;
    }

    fprintf(stderr, "%s: getstat(%s)\n", iomanx ? "iomanx" : "ioman", buf);

    iop_return(iop, 0);

    return 1;
}
extern "C" int ioman_chstat(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_format(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_adddrv(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_deldrv(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_stdioinit(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_rename(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_chdir(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_sync(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_mount(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_umount(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_lseek64(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_devctl(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_symlink(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_readlink(struct iop_state* iop, int iomanx) { return 0; }
extern "C" int ioman_ioctl2(struct iop_state* iop, int iomanx) { return 0; }