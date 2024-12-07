#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ps2_elf.h"

int ps2_elf_load(struct ps2_state* ps2, const char* path) {
    ps2_reset(ps2);

    while (ps2->ee->pc != 0x00082000)
        ps2_cycle(ps2);

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
            (phdr.p_flags & 2) ? 'W' : ' ',
            (phdr.p_flags & 4) ? 'X' : ' ',
            phdr.p_align
        );

        // Clear p_memsz bytes of EE RAM
        memset(ps2->ee_ram->buf + phdr.p_vaddr, 0, phdr.p_memsz);

        // Read segment binary
        fseek(file, phdr.p_offset, SEEK_SET);
        fread(ps2->ee_ram->buf + phdr.p_vaddr, 1, phdr.p_filesz, file);
    }

    // Read symbol table header
    Elf32_Shdr symtab;

    char* buf;

    for (int i = 0; i < ehdr.e_shnum; i++) {
        Elf32_Shdr shdr;

        fseek(file, ehdr.e_shoff + (i * ehdr.e_shentsize), SEEK_SET);
        fread(&shdr, sizeof(Elf32_Shdr), 1, file);

        if ((shdr.sh_type == SHT_STRTAB) && (i != ehdr.e_shstrndx)) {
            printf("elf: Loading string table size=%x offset=%x\n", shdr.sh_size, shdr.sh_offset);

            ps2->strtab = malloc(shdr.sh_size);

            fseek(file, shdr.sh_offset, SEEK_SET);
            fread(ps2->strtab, 1, shdr.sh_size, file);
        }

        if (shdr.sh_type == SHT_SYMTAB)
            symtab = shdr;
    }

    // No symbol table present
    if (symtab.sh_type != SHT_SYMTAB) {
        fclose(file);

        return 0;
    }

    if (!symtab.sh_entsize) {
        fclose(file);

        return 0;
    }

    size_t nsyms = symtab.sh_size / symtab.sh_entsize;

    // Read symbol table
    Elf32_Sym sym;

    for (int i = 0; i < nsyms; i++) {
        fseek(file, symtab.sh_offset + (i * symtab.sh_entsize), SEEK_SET);
        fread(&sym, sizeof(Elf32_Sym), 1, file);

        if (ELF32_ST_TYPE(sym.st_info) != STT_FUNC)
            continue;

        ++ps2->nfuncs;
    }

    ps2->func = malloc(ps2->nfuncs * sizeof(struct ps2_elf_function));

    int index = 0;

    for (int i = 0; i < nsyms; i++) {
        fseek(file, symtab.sh_offset + (i * symtab.sh_entsize), SEEK_SET);
        fread(&sym, sizeof(Elf32_Sym), 1, file);

        if (ELF32_ST_TYPE(sym.st_info) != STT_FUNC)
            continue;

        ps2->func[index].addr = sym.st_value;
        ps2->func[index++].name = ps2->strtab + sym.st_name;
    }
 
    fclose(file);

    return 0;
}