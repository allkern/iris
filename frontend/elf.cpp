#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "iris.hpp"

#ifdef __linux__
#include <elf.h>
#else
#include "elf.h"
#endif

#include "ps2.hpp"

namespace iris::elf {

void load_symbols_from_memory(Instance* iris, char* buf) {
    if (!buf)
        return;

    // Clear previous symbols
    iris->debug.symbols.clear();
    iris->debug.strtab.clear();

    Elf32_Ehdr* ehdr = (Elf32_Ehdr*)buf;

    // Parse ELF header
    if (strncmp((char*)ehdr->e_ident, "\x7f" "ELF", 4) != 0) {
        iris_error(&iris->log.elf, "Invalid ELF magic number");

        return;
    }

    // Read symbol table header
    Elf32_Shdr* symtab = nullptr;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf32_Shdr* shdr = (Elf32_Shdr*)(buf + ehdr->e_shoff + (i * ehdr->e_shentsize));

        if ((shdr->sh_type == SHT_STRTAB) && (i != ehdr->e_shstrndx)) {
            iris_info(&iris->log.elf, "Loading string table size={:x} offset={:x}", shdr->sh_size, shdr->sh_offset);

            // Get string table
            iris->debug.strtab.resize(shdr->sh_size);

            memcpy(iris->debug.strtab.data(), buf + shdr->sh_offset, shdr->sh_size);
        }

        if (shdr->sh_type == SHT_SYMTAB) {
            symtab = shdr;

            iris_info(&iris->log.elf, "Found symbol table size={:x} offset={:x}", symtab->sh_size, symtab->sh_offset);
        }
    }

    // No symbol table present
    if (!symtab) {
        iris_warning(&iris->log.elf, "No symbol table found");

        return;
    }

    if (!symtab->sh_entsize) {
        iris_error(&iris->log.elf, "Invalid symbol table entry size");

        return;
    }

    if (!symtab->sh_size) {
        iris_warning(&iris->log.elf, "Symbol table is empty");

        return;
    }

    size_t symbol_count = symtab->sh_size / symtab->sh_entsize;

    iris_info(&iris->log.elf, "Found symbol table with {} symbols", symbol_count);

    // Read symbol table
    Elf32_Sym* sym;

    for (int i = 0; i < symbol_count; i++) {
        sym = (Elf32_Sym*)(buf + symtab->sh_offset + (i * symtab->sh_entsize));

        if (ELF32_ST_TYPE(sym->st_info) != STT_FUNC)
            continue;

        Symbol symbol;

        symbol.name = (char*)(iris->debug.strtab.data() + sym->st_name);
        symbol.addr = sym->st_value;
        symbol.size = sym->st_size;

        // printf("symbol: %s at 0x%08x\n", symbol.name, symbol.addr);

        iris->debug.symbols.push_back(symbol);
    }
}

bool load_symbols_from_disc(Instance* iris) {
    if (!iris->ps2 || !iris->ps2->cdvd || !iris->ps2->cdvd->disc) {
        iris_error(&iris->log.elf, "No disc loaded");

        return false;
    }

    char* elf = iop::disc::read_boot_elf(iris->ps2->cdvd->disc, 0);

    load_symbols_from_memory(iris, elf);

    free(elf);

    return true;
}

bool load_symbols_from_file(Instance* iris, std::string path) {
    if (path.empty()) {
        iris_error(&iris->log.elf, "No file path provided");

        return false;
    }

    FILE* file = fopen(path.c_str(), "rb");

    if (!file) {
        iris_error(&iris->log.elf, "Failed to open file {}", path.c_str());

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