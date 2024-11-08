#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ps2_elf.h"

int ps2_elf_load(struct ps2_state* ps2, const char* path) {
    ps2_reset(ps2);

    int ee_ticks = 8;

    while (ps2->ee->pc != 0x00082000) {
        sched_tick(ps2->sched, 1);
        ee_cycle(ps2->ee);

        --ee_ticks;

        if (!ee_ticks) {
            iop_cycle(ps2->iop);

            ee_ticks = 8;
        }
    }

    Elf32_Ehdr ehdr;
    FILE* file = fopen(path, "rb");

    fread(&ehdr, sizeof(Elf32_Ehdr), 1, file);

    ps2->ee->pc = ehdr.e_entry;
    ps2->ee->next_pc = ps2->ee->pc + 4;

    Elf32_Phdr phdr;

    puts("  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align");

    for (int i = 0; i < ehdr.e_phnum; i++) {
        fseek(file, ehdr.e_phoff + (i * ehdr.e_phentsize), SEEK_SET);
        fread(&phdr, sizeof(Elf32_Phdr), 1, file);

        if (phdr.p_type != PT_LOAD)
            continue;

        printf("  LOAD           0x%06x 0x%08x 0x%08x 0x%05x 0x%05x %c%c%c 0x%x\n",
            phdr.p_offset,
            phdr.p_vaddr,
            phdr.p_paddr,
            phdr.p_filesz,
            phdr.p_memsz,
            (phdr.p_flags & 1) ? 'R' : ' ',
            (phdr.p_flags & 1) ? 'W' : ' ',
            (phdr.p_flags & 1) ? 'X' : ' ',
            phdr.p_align
        );

        // Clear p_memsz bytes of EE RAM
        memset(ps2->ee_ram->buf + phdr.p_vaddr, 0, phdr.p_memsz);

        // Read segment binary
        fseek(file, phdr.p_offset, SEEK_SET);
        fread(ps2->ee_ram->buf + phdr.p_vaddr, 1, phdr.p_filesz, file);
    }

    fclose(file);

    return 0;
}