#include <cstdio>

#include "ee.h"
#include "bus.h"

int main() {
    struct ps2_bus* bus = ps2_bus_create();

    ps2_bus_init(bus, "scph10000.bin");

    struct ee_bus cpu_bus;

    cpu_bus.read8 = ps2_bus_read8;
    cpu_bus.read16 = ps2_bus_read16;
    cpu_bus.read32 = ps2_bus_read32;
    cpu_bus.read64 = ps2_bus_read64;
    cpu_bus.read128 = ps2_bus_read128;
    cpu_bus.write8 = ps2_bus_write8;
    cpu_bus.write16 = ps2_bus_write16;
    cpu_bus.write32 = ps2_bus_write32;
    cpu_bus.write64 = ps2_bus_write64;
    cpu_bus.write128 = ps2_bus_write128;
    cpu_bus.udata = bus;

    struct ee_state* ee = ee_create();

    ee_init(ee, cpu_bus);
    ee_cycle(ee);
    ee_destroy(ee);

    return 0;
}