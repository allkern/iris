#include <cstdio>

#include "ps2.h"

int main(int argc, const char* argv[]) {
    struct ps2_state* ps2 = ps2_create();

    ps2_init(ps2);
    ps2_load_bios(ps2, argv[1]);

    while (true) {
        // if (ps2->ee->pc == 0x00082000) {
        //     FILE* file = fopen("3stars.elf", "rb");

        //     fseek(file, 0x1000, SEEK_SET);
        //     fread(ps2->ee_bus->ee_ram->buf + 0x400000, 1, 0x2a200, file);

        //     ps2->ee->pc = 0x400000;
        //     ps2->ee->next_pc = ps2->ee->pc + 4;

        //     fclose(file);
        // }

        ps2_cycle(ps2);
    }

    ps2_destroy(ps2);

    return 0;
}