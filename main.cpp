#include <cstdio>

#include "ps2.h"

int main(int argc, const char* argv[]) {
    struct ps2_state* ps2 = ps2_create();

    ps2_init(ps2);
    ps2_load_bios(ps2, argv[1]);

    while (true)
        ps2_cycle(ps2);

    ps2_destroy(ps2);

    return 0;
}