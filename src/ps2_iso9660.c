#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

#include "ps2_iso9660.h"

#ifdef _MSC_VER
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#elif defined(_WIN32)
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

#define IGNORE_RETURN ((void)!)

struct iso9660_state* iso9660_open(const char* path) {
    FILE* file = fopen(path, "rb");

    if (!file)
        return NULL;

    struct iso9660_state* iso = malloc(sizeof(struct iso9660_state));

    memset(iso, 0, sizeof(struct iso9660_state));

    iso->file = file;

    return iso;
}

char* iso9660_get_boot_path(struct iso9660_state* iso) {
    // Cache the PVD (Primary Volume Descriptor)
    fseek64(iso->file, 16 * 0x800, SEEK_SET);

    if (!fread(&iso->pvd, sizeof(struct iso9660_pvd), 1, iso->file)) {
        printf("iso9660: Couldn't read PVD\n");

        return NULL;
    }

    if (strncmp(iso->pvd.id, "\1CD001\1", 8)) {
        printf("iso: Unknown format disc %d\n", iso->pvd.id[0]);

        return NULL;
    }

    struct iso9660_dirent* root = (struct iso9660_dirent*)iso->pvd.root;

    fseek64(iso->file, (uint64_t)root->lba_le * 0x800, SEEK_SET);

    if (!fread(iso->buf, 0x800, 1, iso->file)) {
        printf("iso9660: Couldn't read root sector\n");

        return NULL;
    }

    struct iso9660_dirent* dir = (struct iso9660_dirent*)iso->buf;

    while (dir->dr_len) {
        // printf("Entry: lba=%d len=%d size=%d name_len=%d name=\"", dir->lba_le, dir->dr_len, dir->size_le, dir->id_len);

        // if (dir->id == '\0') {
        //     putchar('.');
        // } else if (dir->id == '\1') {
        //     putchar('.');
        //     putchar('.');
        // } else {
        //     for (int j = 0; j < dir->id_len; j++) {
        //         putchar(*(((char*)&dir->id) + j));
        //     }
        // }

        // puts("\"");

        if (dir->id_len == 12) {
            if (!strcmp((char*)&dir->id, "SYSTEM.CNF;1")) {
                break;
            }
        }

        uint8_t* ptr = (uint8_t*)dir;

        dir = (struct iso9660_dirent*)(ptr + dir->dr_len);
    }

    if (!dir->dr_len) {
        printf("iso: SYSTEM.CNF not found! (non-PlayStation disc?)\n");

        return NULL;
    }

    fseek64(iso->file, (uint64_t)dir->lba_le * 0x800, SEEK_SET);

    if (!fread(iso->buf, 0x800, 1, iso->file)) {
        printf("iso9660: Couldn't read SYSTEM.CNF\n");

        return NULL;
    }

    printf("isobuf: %s\n", iso->buf);

    // Parse SYSTEM.CNF
    char* p = iso->buf;
    char key[64];
    
    while (*p) {
        char* kptr = key;

        while (isspace(*p))
            ++p;

        while (isalnum(*p))
            *kptr++ = *p++;

        *kptr = '\0';

        // printf("key: %s\n", key);

        if (!strncmp(key, "BOOT2", 64)) {
            while (isspace(*p)) ++p;

            if (*p != '=') {
                printf("iso: Expected =\n");

                return NULL;
            }

            ++p;

            while (isspace(*p)) ++p;

            int i;

            for (i = 0; i < 255; i++) {
                if (*p == '\n' || *p == '\r')
                    break;

                iso->boot_file[i] = *p++;
            }

            iso->boot_file[i] = '\0';

            return iso->boot_file;
        } else {
            while ((*p != '\n') && (*p != '\0') && (*p != '\r')) ++p;
            while ((*p == '\n') || (*p == '\r')) ++p;
        }
    }

    printf("iso: Couldn't find BOOT2 entry in SYSTEM.CNF (PlayStation disc?)\n");

    return NULL;
}

// void iso9660_load_boot_elf(struct iso9660_state* iso, char* buf);

void iso9660_close(struct iso9660_state* iso) {
    if (iso->file)
        fclose(iso->file);

    free(iso);
}

#undef fseek64
#undef ftell64