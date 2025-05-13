#include "loadcore.h"

#include "../iop.h"

#include <stdlib.h>
#include <string.h>

static unsigned get_module_list(struct iop_state* iop)
{
    /* Loadcore puts a pointer at 0x3f0 to an array in its data section */
    unsigned bootmodes_ptr = iop_read32(iop, 0x3f0);
    unsigned p = bootmodes_ptr - 0x60;
    unsigned found = 0;

    /* see if the string starting with PsIIload is there*/
    while (p < bootmodes_ptr) {
        if (iop_read32(iop, p) == 0x49497350
            && iop_read32(iop, p + 4) == 0x64616F6C) {
            found = p;
            break;
        }

        p += 4;
    }

    /* This seems to have held true for all the versions i've seen */
    unsigned lc_struct;
    if (!found) {
        lc_struct = bootmodes_ptr - 0x20;
    } else {
        lc_struct = p + 0x18;
    }

    return lc_struct + 0x10;
}

static void iop_strncpy(struct iop_state* iop, char* dest, unsigned src, int n)
{
    char c;

    while ((c = iop_read8(iop, src)) && n) {
        *dest = c;
        dest++;
        src++;
        n--;
    }
}

static void cache_loaded_modules(struct iop_state* iop, unsigned list)
{
    unsigned ent = iop_read32(iop, list);
    struct iop_module* mod;
    int count = 0;

    while (ent) {
        ent = iop_read32(iop, ent);
        count++;
    }

    mod = calloc(count, sizeof(*mod));

    ent = iop_read32(iop, list);
    int i = 0;
    while (ent != 0) {
        if (iop_read32(iop, ent + 4)) {
            iop_strncpy(iop, mod[i].name, iop_read32(iop, ent + 4), sizeof(mod[i].name));
        } else {
            strcpy(mod[i].name, "-- MISSING --");
        }
        mod[i].version = iop_read16(iop, ent + 8);
        mod[i].entry = iop_read32(iop, ent + 0x10);
        mod[i].gp = iop_read32(iop, ent + 0x14);
        mod[i].text_addr = iop_read32(iop, ent + 0x18);
        mod[i].text_size = iop_read32(iop, ent + 0x1c);
        mod[i].data_size = iop_read32(iop, ent + 0x20);
        mod[i].bss_size = iop_read32(iop, ent + 0x24);

        ent = iop_read32(iop, ent);
        i++;
    }

    iop->module_count = count;
    iop->module_list = mod;
}

void refresh_module_list(struct iop_state* iop)
{
    struct iop_module* mod = iop->module_list;

    iop->module_count = 0;
    iop->module_list = NULL;
    free(mod);

    cache_loaded_modules(iop, iop->module_list_addr);
}

int loadcore_reg_lib_ent(struct iop_state* iop)
{
    unsigned list = get_module_list(iop);

    iop->module_list_addr = list;

    refresh_module_list(iop);

    return 0;
}
