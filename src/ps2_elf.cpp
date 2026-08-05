#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <cstdio>
#include <cstring>

#include "ps2_elf.hpp"

namespace iris::elf {

bool load(ps2::Ps2* ps2, const char* path) {
    ps2::reset(ps2);

    while (ee::get_pc(ps2->ee) != 0x00082000)
        ps2::cycle(ps2);

    Elf32_Ehdr ehdr;
    FILE* file = fopen(path, "rb");

    if (!fread(&ehdr, sizeof(Elf32_Ehdr), 1, file)) {
        iris_debug(ps2, "Couldn't read ELF header");

        return 1;
    }

    Elf32_Phdr phdr;

    puts("  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align");

    for (int i = 0; i < ehdr.e_phnum; i++) {
        fseek(file, ehdr.e_phoff + (i * ehdr.e_phentsize), SEEK_SET);

        if (!fread(&phdr, sizeof(Elf32_Phdr), 1, file)) {
            iris_debug(ps2, "Couldn't read program header");

            return 1;
        }

        if (phdr.p_type != PT_LOAD)
            continue;

        iris_debug(ps2, "LOAD           0x{:06x} 0x{:08x} 0x{:08x} 0x{:05x} 0x{:05x} {}{}{} 0x{:x}", phdr.p_offset,
            phdr.p_vaddr,
            phdr.p_paddr,
            phdr.p_filesz,
            phdr.p_memsz,
            (phdr.p_flags & 1) ? 'R' : ' ',
            (phdr.p_flags & 2) ? 'W' : ' ',
            (phdr.p_flags & 4) ? 'X' : ' ',
            phdr.p_align);

        memset(ps2->ee_ram->buf + phdr.p_vaddr, 0, phdr.p_memsz);

        fseek(file, phdr.p_offset, SEEK_SET);

        if (!fread(ps2->ee_ram->buf + phdr.p_vaddr, 1, phdr.p_filesz, file)) {
            iris_debug(ps2, "Couldn't read segment binary");
        }
    }

    iris_debug(ps2, "Entry: 0x{:08x}", ehdr.e_entry);

    fclose(file);

    return 0;
}

}
