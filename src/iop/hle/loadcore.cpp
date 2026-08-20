#include "loadcore.hpp"

#include "../iop_def.hpp"
#include "thbase.hpp"
#include "sifcmd.hpp"

namespace iris::iop::hle::loadcore {

static unsigned get_module_list(iop::Iop* iop)
{
    /* Loadcore puts a pointer at 0x3f0 to an array in its data section */
    unsigned bootmodes_ptr = iop::read32(iop, 0x3f0);
    unsigned p = bootmodes_ptr - 0x60;
    unsigned found = 0;

    /* see if the string starting with PsIIload is there*/
    while (p < bootmodes_ptr) {
        if (iop::read32(iop, p) == 0x49497350
            && iop::read32(iop, p + 4) == 0x64616F6C) {
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

static unsigned get_thread_list(iop::Iop* iop)
{
    unsigned module_version = iop::read32(iop, iop->r[4] + 8);

    /* Read address of ordinal 3 */
    unsigned func = iop::read32(iop, iop->r[4] + 0x20);

    /* Read lui+ori of address to the thread manager global */
    unsigned th_struct = iop::read32(iop, func) << 16;
    th_struct |= iop::read32(iop, func + 4) & 0xffff;

    unsigned th_list = th_struct + 0x42c;
    if (module_version > 0x101) {
        th_list = th_struct + 0x430;
    }

    return th_list;
}

static void iop_strncpy(iop::Iop* iop, char* dest, unsigned src, int n)
{
    char c;

    while ((c = iop::read8(iop, src)) && n) {
        *dest = c;
        dest++;
        src++;
        n--;
    }
}

static void cache_loaded_modules(iop::Iop* iop, unsigned list)
{
    unsigned ent = iop::read32(iop, list);
    Module* mod;
    int count = 0;

    while (ent) {
        ent = iop::read32(iop, ent);
        count++;
    }

    mod = (Module*)calloc(count, sizeof(*mod));

    ent = iop::read32(iop, list);
    int i = 0;
    while (ent != 0) {
        if (iop::read32(iop, ent + 4)) {
            iop_strncpy(iop, mod[i].name, iop::read32(iop, ent + 4), sizeof(mod[i].name));
        } else {
            strcpy(mod[i].name, "-- MISSING --");
        }
        mod[i].version = iop::read16(iop, ent + 8);
        mod[i].entry = iop::read32(iop, ent + 0x10);
        mod[i].gp = iop::read32(iop, ent + 0x14);
        mod[i].text_addr = iop::read32(iop, ent + 0x18);
        mod[i].text_size = iop::read32(iop, ent + 0x1c);
        mod[i].data_size = iop::read32(iop, ent + 0x20);
        mod[i].bss_size = iop::read32(iop, ent + 0x24);

        ent = iop::read32(iop, ent);
        i++;
    }

    iop->module_count = count;
    iop->module_list = mod;
}

void refresh_module_list(iop::Iop* iop)
{
    Module* mod = iop->module_list;

    iop->module_count = 0;
    iop->module_list = NULL;
    free(mod);

    cache_loaded_modules(iop, iop->module_list_addr);
}

int reg_lib_ent(iop::Iop* iop)
{
    unsigned module_list = get_module_list(iop);
    iop->module_list_addr = module_list;
    refresh_module_list(iop);

    char name[8] = {};

    iop_strncpy(iop, name, iop->r[4] + 0xc, sizeof(name));
    if (strncmp(name, "thbase", 6) == 0) {
        unsigned thread_list = get_thread_list(iop);
        iop->thread_list_addr = thread_list;

        thbase::register_exports(iop, iop->r[4]);
    }

    if (strncmp(name, "sifcmd", 6) == 0)
        sifcmd::register_exports(iop, iop->r[4]);

    return 0;
}

}
