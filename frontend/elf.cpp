#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "iris.hpp"

#ifdef __linux__
#include <elf.h>
#else
#include "elf.h"
#endif

namespace iris::elf {

void load_symbols_from_memory(iris::instance* iris, char* buf) {
    if (!buf)
        return;

    // Clear previous symbols
    iris->symbols.clear();
    iris->strtab.clear();

    Elf32_Ehdr* ehdr = (Elf32_Ehdr*)buf;

    // Parse ELF header
    if (strncmp((char*)ehdr->e_ident, "\x7f" "ELF", 4) != 0) {
        printf("elf: Invalid ELF magic number\n");

        return;
    }

    // Read symbol table header
    Elf32_Shdr* symtab = nullptr;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf32_Shdr* shdr = (Elf32_Shdr*)(buf + ehdr->e_shoff + (i * ehdr->e_shentsize));

        if ((shdr->sh_type == SHT_STRTAB) && (i != ehdr->e_shstrndx)) {
            printf("elf: Loading string table size=%x offset=%x\n", shdr->sh_size, shdr->sh_offset);

            // Get string table
            iris->strtab.resize(shdr->sh_size);

            memcpy(iris->strtab.data(), buf + shdr->sh_offset, shdr->sh_size);
        }

        if (shdr->sh_type == SHT_SYMTAB) {
            symtab = shdr;

            printf("elf: Found symbol table size=%x offset=%x\n", symtab->sh_size, symtab->sh_offset);
        }
    }

    // No symbol table present
    if (!symtab) {
        printf("elf: No symbol table found\n");

        return;
    }

    if (!symtab->sh_entsize) {
        printf("elf: Invalid symbol table entry size\n");

        return;
    }

    if (!symtab->sh_size) {
        printf("elf: Symbol table is empty\n");

        return;
    }

    size_t symbol_count = symtab->sh_size / symtab->sh_entsize;

    printf("elf: Found symbol table with %d symbols\n", symbol_count);

    // Read symbol table
    Elf32_Sym* sym;

    for (int i = 0; i < symbol_count; i++) {
        sym = (Elf32_Sym*)(buf + symtab->sh_offset + (i * symtab->sh_entsize));

        if (ELF32_ST_TYPE(sym->st_info) != STT_FUNC)
            continue;

        elf_symbol symbol;

        symbol.name = (char*)(iris->strtab.data() + sym->st_name);
        symbol.addr = sym->st_value;
        symbol.size = sym->st_size;

        // printf("symbol: %s at 0x%08x\n", symbol.name, symbol.addr);

        iris->symbols.push_back(symbol);
    }
}

bool load_symbols_from_disc(iris::instance* iris) {
    if (!iris->ps2 || !iris->ps2->cdvd || !iris->ps2->cdvd->disc) {
        printf("elf: No disc loaded\n");

        return false;
    }

    char* elf = disc_read_boot_elf(iris->ps2->cdvd->disc, 0);

    load_symbols_from_memory(iris, elf);

    free(elf);

    return true;
}

bool load_symbols_from_file(iris::instance* iris, std::string path) {
    if (path.empty()) {
        printf("elf: No file path provided\n");

        return false;
    }

    FILE* file = fopen(path.c_str(), "rb");

    if (!file) {
        printf("elf: Failed to open file %s\n", path.c_str());

        return false;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buf = new char[size];

    fread(buf, 1, size, file);
    fclose(file);

    load_symbols_from_memory(iris, buf);

    delete[] buf;

    return true;
}

}