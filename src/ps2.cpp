#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstring>

#include "ps2.hpp"
#include "rom.hpp"
#include "iop/hle/ioman.hpp"

namespace iris::ps2 {

Ps2* create(logger::Logger* logger) {
    Ps2* ps2 = new Ps2();

    ps2->logger = logger;
    ps2->logger_id = logger::register_source(logger, "ps2");

    return ps2;
}

void init(Ps2* ps2) {
    ps2->sched = scheduler::create(ps2->logger);

    // Components take their dependencies in create(), so this list is ordered.
    // everything a component needs must already exist above it.
    ps2->ee = ee::create(ps2->logger, (int)ram::Size::_32MB);
    ps2->vu0 = vu::create(ps2->logger, 0);
    ps2->vu1 = vu::create(ps2->logger, 1);
    ps2->iop = iop::create(ps2->logger);
    ps2->ee_bus = ee::bus::create(ps2->logger);
    ps2->iop_bus = iop::bus::create(ps2->logger);

    ps2->ee_ram = ram::create(ps2->logger, ram::Size::_32MB);
    ps2->iop_ram = ram::create(ps2->logger, ram::Size::_2MB);
    ps2->iop_spr = ram::create(ps2->logger, ram::Size::_1KB);
    ps2->bios = bios::create(ps2->logger);
    ps2->rom1 = bios::create(ps2->logger);
    ps2->rom2 = bios::create(ps2->logger);
    ps2->sif = sif::create(ps2->logger);
    ps2->dev9 = dev9::create(ps2->logger, dev9::Model::EXPBAY);

    ps2->iop_intc = iop::intc::create(ps2->logger, ps2->iop);
    ps2->ee_intc = ee::intc::create(ps2->logger, ps2->sched);
    ps2->iop_timers = iop::timers::create(ps2->logger, ps2->iop_intc, ps2->sched);
    ps2->ee_timers = ee::timers::create(ps2->logger, ps2->sched);
    ps2->sbus = sbus::create(ps2->logger, ps2->ee_intc, ps2->iop_intc, ps2->sched);
    ps2->fw = fw::create(ps2->logger, ps2->iop_intc);
    ps2->speed = speed::create(ps2->logger, ps2->iop_intc, ps2->sched);
    ps2->usb = usb::create(ps2->logger, ps2->iop_intc, ps2->iop_bus, ps2->sched);
    ps2->gs = gs::create(ps2->logger, ps2->iop_intc, ps2->iop_timers, ps2->sched);
    ps2->cdvd = cdvd::create(ps2->logger, ps2->iop_intc, ps2->sched);
    ps2->sio2 = sio2::create(ps2->logger, ps2->iop_intc, ps2->sched);
    ps2->spu2 = spu2::create(ps2->logger, ps2->iop_intc, ps2->sched);

    ps2->ee_dma = ee::dmac::create(ps2->logger, ps2->sched, ps2->ee_bus, ps2->sif);
    ps2->iop_dma = iop::dma::create(ps2->logger, ps2->iop_intc, ps2->sif, ps2->speed, ps2->sched, ps2->iop, ps2->iop_bus);
    ps2->ipu = ipu::create(ps2->logger, ps2->ee_dma, ps2->ee_intc);
    ps2->gif = gif::create(ps2->logger);
    ps2->vif0 = vif::create(ps2->logger, 0, ps2->sched, ps2->ee_bus);
    ps2->vif1 = vif::create(ps2->logger, 1, ps2->sched, ps2->ee_bus);

    // Initialize EE

    ee::BusInterface bus_data;
    bus_data.read8 = ee::bus::read8;
    bus_data.read16 = ee::bus::read16;
    bus_data.read32 = ee::bus::read32;
    bus_data.read64 = ee::bus::read64;
    bus_data.read128 = ee::bus::read128;
    bus_data.write8 = ee::bus::write8;
    bus_data.write16 = ee::bus::write16;
    bus_data.write32 = ee::bus::write32;
    bus_data.write64 = ee::bus::write64;
    bus_data.write128 = ee::bus::write128;
    bus_data.udata = ps2->ee_bus;

    ee::connect(ps2->ee, ps2->vu0, ps2->vu1, bus_data);
    // Initialize IOP

    iop::bus::Iface iop_bus_data;
    iop_bus_data.read8 = iop::bus::read8;
    iop_bus_data.read16 = iop::bus::read16;
    iop_bus_data.read32 = iop::bus::read32;
    iop_bus_data.write8 = iop::bus::write8;
    iop_bus_data.write16 = iop::bus::write16;
    iop_bus_data.write32 = iop::bus::write32;
    iop_bus_data.udata = ps2->iop_bus;

    iop::connect(ps2->iop, iop_bus_data);

    // Wire the components that reference each other
    ee::dmac::connect(ps2->ee_dma, ps2->gif, ps2->vif0, ps2->vif1, ps2->ipu, ps2->iop_dma, ps2->ee);
    iop::dma::connect(ps2->iop_dma, ps2->cdvd, ps2->ee_dma, ps2->sio2, ps2->spu2, ps2->s2x6_acata);
    gif::connect(ps2->gif, ps2->ee_dma, ps2->vu1, ps2->gs);
    vif::connect(ps2->vif0, ps2->vu0, ps2->gif, ps2->ee_intc, ps2->ee_dma);
    vif::connect(ps2->vif1, ps2->vu1, ps2->gif, ps2->ee_intc, ps2->ee_dma);
    vu::connect(ps2->vu0, ps2->gif, ps2->vif0, ps2->vu1);
    vu::connect(ps2->vu1, ps2->gif, ps2->vif1, ps2->vu1);
    gs::connect(ps2->gs, ps2->ee_intc, ps2->ee_timers);
    ee::intc::connect(ps2->ee_intc, ps2->ee);
    ee::timers::connect(ps2->ee_timers, ps2->ee_intc);
    cdvd::connect(ps2->cdvd, ps2->iop_dma);
    sio2::connect(ps2->sio2, ps2->iop_dma);
    spu2::connect(ps2->spu2, ps2->iop_dma);

    // Components still on the old two-phase shape
    sif::connect(ps2->sif, ps2->iop_intc);

    // Initialize bus pointers
    ps2->iop_bus->bios = ps2->bios;
    ps2->iop_bus->rom1 = ps2->rom1;
    ps2->iop_bus->rom2 = ps2->rom2;
    ps2->iop_bus->iop_ram = ps2->iop_ram;
    ps2->iop_bus->iop_spr = ps2->iop_spr;
    ps2->iop_bus->sif = ps2->sif;
    ps2->iop_bus->dma = ps2->iop_dma;
    ps2->iop_bus->intc = ps2->iop_intc;
    ps2->iop_bus->timers = ps2->iop_timers;
    ps2->iop_bus->cdvd = ps2->cdvd;
    ps2->iop_bus->sio2 = ps2->sio2;
    ps2->iop_bus->spu2 = ps2->spu2;
    ps2->iop_bus->usb = ps2->usb;
    ps2->iop_bus->fw = ps2->fw;
    ps2->iop_bus->sbus = ps2->sbus;
    ps2->iop_bus->dev9 = ps2->dev9;
    ps2->iop_bus->speed = ps2->speed;
    ps2->ee_bus->bios = ps2->bios;
    ps2->ee_bus->rom1 = ps2->rom1;
    ps2->ee_bus->rom2 = ps2->rom2;
    ps2->ee_bus->iop_ram = ps2->iop_ram;
    ps2->ee_bus->sif = ps2->sif;
    ps2->ee_bus->dmac = ps2->ee_dma;
    ps2->ee_bus->intc = ps2->ee_intc;
    ps2->ee_bus->timers = ps2->ee_timers;
    ps2->ee_bus->gif = ps2->gif;
    ps2->ee_bus->vif0 = ps2->vif0;
    ps2->ee_bus->vif1 = ps2->vif1;
    ps2->ee_bus->gs = ps2->gs;
    ps2->ee_bus->ipu = ps2->ipu;
    ps2->ee_bus->vu0 = ps2->vu0;
    ps2->ee_bus->vu1 = ps2->vu1;
    ps2->ee_bus->cdvd = ps2->cdvd;
    ps2->ee_bus->usb = ps2->usb;
    ps2->ee_bus->sbus = ps2->sbus;
    ps2->ee_bus->dev9 = ps2->dev9;
    ps2->ee_bus->speed = ps2->speed;
    ps2->ee_bus->ee_ram = ps2->ee_ram;
    ps2->ee_bus->iop = ps2->iop;

    iop::dma::set_dev9_mode(ps2->iop_dma, iop::dma::DEV9_MODE_RETAIL);

    ipu::reset(ps2->ipu);

    ps2->ee_cycles = 0;
    ps2->timescale = 1;
}

void init_tty_handler(Ps2* ps2, TtyType tty, void (*handler)(void*, char), void* udata) {
    switch (tty) {
        case EE:  
            ee::bus::init_kputchar(ps2->ee_bus, handler, udata);
            break;
        case IOP:
            iop::init_kputchar(ps2->iop, handler, udata);
            break;
        case SYSMEM:
            iop::init_sm_putchar(ps2->iop, handler, udata);
            break;
    }
}

void iop_map_device(Ps2* ps2, const char* device, const char* host_path) {
    iop::hle::ioman::map_device(device, host_path);
}

void iop_unmap_device(Ps2* ps2, const char* device) {
    iop::hle::ioman::unmap_device(device);
}

void iop_clear_device_maps(Ps2* ps2) {
    iop::hle::ioman::clear_devices();
}

void boot_file(Ps2* ps2, const char* path) {
    reset(ps2);

    while (ee::get_pc(ps2->ee) != 0x00082000) {
        while (ps2->ee_cycles < 16*64) {
            ps2->ee_cycles += ee::run_block(ps2->ee, 1);

            if (ee::get_pc(ps2->ee) == 0x00082000)
                break;

            ipu::run(ps2->ipu);
        }

        scheduler::tick(ps2->sched, ps2->timescale * ps2->ee_cycles);

        // The timer runs at BUSCLK speed, that is 1 BUSCLK cycle every 2 EE instructions
        ee::timers::tick_cycles(ps2->ee_timers, ps2->ee_cycles);

        ps2->iop_cycles += ps2->ee_cycles / 8;

        // printf("ee: cycles=%d iop cycles=%d\n", ps2->ee_cycles, ps2->iop_cycles);

        while (ps2->iop_cycles > 0) {
            int cycles = iop::run_block(ps2->iop, 16);

            iop::timers::tick_cycles(ps2->iop_timers, cycles / 2);

            // for (int i = 0; i < cycles; i++)
            //     iop::timers::tick(ps2->iop_timers);

            ps2->iop_cycles -= cycles;
            ps2->ee_cycles -= cycles * 8;
        }
    }

    uint32_t i;

    // Find rom0:OSDSYS string
    for (i = 0; i < (uint32_t)ram::Size::_32MB; i += 0x10) {
        char* ptr = (char*)&ps2->ee_ram->buf[i];

        if (!strncmp(ptr, "rom0:OSDSYS", 12)) {
            iris_debug(ps2, "Found OSDSYS path at {:08x}", i);

            sprintf(ptr, "%s", path);
        }
    }
}

int load_bios(Ps2* ps2, const char* path) {
    if (!bios::load(ps2->bios, path))
        return 0;

    ee::bus::init_fastmem(ps2->ee_bus, ps2->ee_ram->size, ps2->iop_ram->size);
    iop::bus::init_fastmem(ps2->iop_bus, ps2->iop_ram->size);

    ee::flush_cache(ps2->ee);

    if (ps2->system == AUTO) {
        ps2->rom0_info = rom::search(rom::Type::ROM0, ps2->bios->buf, ps2->bios->size + 1);

        set_system(ps2, ps2->rom0_info.system);

        ps2->detected_system = ps2->rom0_info.system;
    }

    return 1;
}

int load_rom1(Ps2* ps2, const char* path) {
    if (!bios::load(ps2->rom1, path))
        return 0;

    ps2->rom1_info = rom::search(rom::Type::ROM1, ps2->rom1->buf, ps2->rom1->size + 1);

    return 1;
}

int load_rom2(Ps2* ps2, const char* path) {
    if (!bios::load(ps2->rom2, path))
        return 0;

    return 1;
}

void reset(Ps2* ps2) {
    scheduler::reset(ps2->sched);

    int iop_dev9_mode = ps2->iop_dma->dev9_mode;

    ee::reset(ps2->ee);
    iop::reset(ps2->iop);
    vu::reset(ps2->vu0);
    vu::reset(ps2->vu1);
    ee::dmac::reset(ps2->ee_dma);
    vif::reset(ps2->vif0);
    vif::reset(ps2->vif1);
    ee::intc::reset(ps2->ee_intc);
    ee::timers::reset(ps2->ee_timers);
    iop::dma::reset(ps2->iop_dma);
    iop::intc::reset(ps2->iop_intc);
    iop::timers::reset(ps2->iop_timers);
    spu2::reset(ps2->spu2);
    usb::reset(ps2->usb);
    fw::reset(ps2->fw);
    sbus::reset(ps2->sbus);
    cdvd::reset(ps2->cdvd);

    gif::reset(ps2->gif);
    gs::reset(ps2->gs);
    ram::reset(ps2->ee_ram);
    ram::reset(ps2->iop_ram);

    ipu::reset(ps2->ipu);

    // Restore mode
    iop::dma::set_dev9_mode(ps2->iop_dma, iop_dev9_mode);

    ps2->ee_cycles = 0;
}

void cycle(Ps2* ps2) {
    int64_t next = scheduler::cycles_to_next(ps2->sched);

    int64_t max_block_cycles = next < 16*64 ? next : 16*64;

    if (max_block_cycles < 16*16) {
        max_block_cycles = 16*16;
    }

    ipu::run(ps2->ipu);

    while (ps2->ee_cycles < max_block_cycles) {
        ps2->ee_cycles += ee::run_block(ps2->ee, (int)(max_block_cycles - ps2->ee_cycles));

        if (ee::breakpoint_hit(ps2->ee))
            break;
    }

    scheduler::tick(ps2->sched, ps2->timescale * ps2->ee_cycles);

    ipu::run(ps2->ipu);

    // The timer runs at BUSCLK speed, that is 1 BUSCLK cycle every 2 EE instructions
    ee::timers::tick_cycles(ps2->ee_timers, ps2->ee_cycles);

    ps2->iop_cycles += ps2->ee_cycles / 8;

    while (ps2->iop_cycles > 0) {
        int cycles = iop::run_block(ps2->iop, 64);

        iop::timers::tick_cycles(ps2->iop_timers, cycles / 2);

#if SPU2_SYNC
        spu2_tick(ps2->spu2, ps2->timescale * cycles);
#else
        ps2->spu2->emu_cycle += (uint64_t)ps2->timescale * cycles;
#endif

        ps2->iop_cycles -= cycles;
        ps2->ee_cycles -= cycles * 8;

        if (iop::breakpoint_hit(ps2->iop))
            break;
    }
}

void step_ee(Ps2* ps2) {
    ee::step(ps2->ee);
    scheduler::tick(ps2->sched, 1);
    ee::timers::tick(ps2->ee_timers);

    ipu::run(ps2->ipu);

    ps2->ee_cycles++; 

    if (ps2->ee_cycles == 8) {
        iop::cycle(ps2->iop);
        iop::timers::tick(ps2->iop_timers);

        ps2->ee_cycles = 0;
    }
}

void step_iop(Ps2* ps2) {
    for (int i = 0; i < 8; i++) {
        ee::timers::tick(ps2->ee_timers);
        ee::step(ps2->ee);
    }

    scheduler::tick(ps2->sched, 8);
    iop::cycle(ps2->iop);
    iop::timers::tick(ps2->iop_timers);

    ipu::run(ps2->ipu);
}

void set_timescale(Ps2* ps2, int timescale) {
    ps2->timescale = timescale;
}

void destroy(Ps2* ps2) {
    cdvd::destroy(ps2->cdvd);
    scheduler::destroy(ps2->sched);
    ee::destroy(ps2->ee);
    vu::destroy(ps2->vu0);
    vu::destroy(ps2->vu1);
    iop::destroy(ps2->iop);
    ee::bus::destroy(ps2->ee_bus);
    iop::bus::destroy(ps2->iop_bus);
    gif::destroy(ps2->gif);
    gs::destroy(ps2->gs);
    ipu::destroy(ps2->ipu);
    vif::destroy(ps2->vif0);
    vif::destroy(ps2->vif1);
    ee::dmac::destroy(ps2->ee_dma);
    ram::destroy(ps2->ee_ram);
    ee::intc::destroy(ps2->ee_intc);
    ee::timers::destroy(ps2->ee_timers);
    iop::dma::destroy(ps2->iop_dma);
    ram::destroy(ps2->iop_spr);
    iop::intc::destroy(ps2->iop_intc);
    iop::timers::destroy(ps2->iop_timers);
    ram::destroy(ps2->iop_ram);
    sio2::destroy(ps2->sio2);
    spu2::destroy(ps2->spu2);
    usb::destroy(ps2->usb);
    fw::destroy(ps2->fw);
    sbus::destroy(ps2->sbus);
    dev9::destroy(ps2->dev9);
    speed::destroy(ps2->speed);
    bios::destroy(ps2->bios);
    bios::destroy(ps2->rom1);
    bios::destroy(ps2->rom2);
    sif::destroy(ps2->sif);

    // Destroy optional hardware
    if (ps2->s14x_nand) { s14x::nand::destroy(ps2->s14x_nand); ps2->s14x_nand = NULL; }
    if (ps2->s14x_syscon) { s14x::syscon::destroy(ps2->s14x_syscon); ps2->s14x_syscon = NULL; }
    if (ps2->s14x_sram) { s14x::sram::destroy(ps2->s14x_sram); ps2->s14x_sram = NULL; }
    if (ps2->s14x_link) { s14x::link::destroy(ps2->s14x_link); ps2->s14x_link = NULL; }
    if (ps2->s14x_ioboard) { s14x::ioboard::destroy(ps2->s14x_ioboard); ps2->s14x_ioboard = NULL; }
    if (ps2->s14x_aiboard) { s14x::aiboard::destroy(ps2->s14x_aiboard); ps2->s14x_aiboard = NULL; }

    if (ps2->s2x6_acata) { s2x6::acata::destroy(ps2->s2x6_acata); ps2->s2x6_acata = NULL; }
    if (ps2->s2x6_acjv) { s2x6::acjv::destroy(ps2->s2x6_acjv); ps2->s2x6_acjv = NULL; }

    delete ps2;
}

void set_system(Ps2* ps2, int system) {
    int mechacon_model;
    ram::Size ee_ram_size, iop_ram_size;

    iop::dma::set_dev9_mode(ps2->iop_dma, iop::dma::DEV9_MODE_RETAIL);
    speed::set_dvrp_enabled(ps2->speed, 0);
    iop::bus::set_usb_disabled(ps2->iop_bus, 0);

    // Destroy optional hardware
    if (ps2->s14x_nand) { s14x::nand::destroy(ps2->s14x_nand); ps2->s14x_nand = NULL; }
    if (ps2->s14x_syscon) { s14x::syscon::destroy(ps2->s14x_syscon); ps2->s14x_syscon = NULL; }
    if (ps2->s14x_sram) { s14x::sram::destroy(ps2->s14x_sram); ps2->s14x_sram = NULL; }
    if (ps2->s14x_link) { s14x::link::destroy(ps2->s14x_link); ps2->s14x_link = NULL; }
    if (ps2->s14x_ioboard) { s14x::ioboard::destroy(ps2->s14x_ioboard); ps2->s14x_ioboard = NULL; }
    if (ps2->s14x_aiboard) { s14x::aiboard::destroy(ps2->s14x_aiboard); ps2->s14x_aiboard = NULL; }

    if (ps2->s2x6_acata) { s2x6::acata::destroy(ps2->s2x6_acata); ps2->s2x6_acata = NULL; }
    if (ps2->s2x6_acjv) { s2x6::acjv::destroy(ps2->s2x6_acjv); ps2->s2x6_acjv = NULL; }

    switch (system) {
        case AUTO: {
            ps2->rom0_info = rom::search(rom::Type::ROM0, ps2->bios->buf, ps2->bios->size + 1);

            set_system(ps2, ps2->rom0_info.system);

            ps2->detected_system = ps2->rom0_info.system;

            return;
        } break;
        case RETAIL: {
            ee_ram_size = ram::Size::_32MB;
            iop_ram_size = ram::Size::_2MB;
            mechacon_model = cdvd::MECHACON_SPC970;
        } break;

        case RETAIL_DRAGON: {
            ee_ram_size = ram::Size::_32MB;
            iop_ram_size = ram::Size::_2MB;
            mechacon_model = cdvd::MECHACON_DRAGON;

            speed::set_smap_enabled(ps2->speed, 1);
        } break;

        case PSX_DESR: {
            ee_ram_size = ram::Size::_64MB;
            iop_ram_size = ram::Size::_8MB;
            mechacon_model = cdvd::MECHACON_DRAGON;

            speed::set_dvrp_enabled(ps2->speed, 1);
            speed::set_smap_enabled(ps2->speed, 1);
        } break;

        case TEST:
        case TOOL: {
            ee_ram_size = ram::Size::_128MB;
            iop_ram_size = ram::Size::_8MB;

            // To-do: Separate mechacon model for TOOL/TEST
            mechacon_model = cdvd::MECHACON_DRAGON;
        } break;

        case KONAMI_PYTHON: {
            ee_ram_size = ram::Size::_32MB;
            iop_ram_size = ram::Size::_2MB;
            mechacon_model = cdvd::MECHACON_SPC970;
        } break;

        case KONAMI_PYTHON2: {
            ee_ram_size = ram::Size::_32MB;
            iop_ram_size = ram::Size::_2MB;
            mechacon_model = cdvd::MECHACON_DRAGON;
        } break;

        case NAMCO_SYSTEM_147:
        case NAMCO_SYSTEM_148: {
            ee_ram_size = system == NAMCO_SYSTEM_148 ? ram::Size::_64MB : ram::Size::_32MB;
            iop_ram_size = ram::Size::_2MB;

            // This board actually has no MechaCon
            mechacon_model = cdvd::MECHACON_DRAGON;

            // Wire up System 147/148 hardware
            ps2->s14x_nand = s14x::nand::create(ps2->logger);
            ps2->s14x_syscon = s14x::syscon::create(ps2->logger);
            ps2->s14x_sram = s14x::sram::create(ps2->logger, &ps2->s14x_syscon->sram_write_flag);
            ps2->s14x_link = s14x::link::create(ps2->logger, ps2->iop_intc, ps2->sched);
            ps2->s14x_ioboard = s14x::ioboard::create(ps2->logger, 0);
            ps2->s14x_aiboard = s14x::aiboard::create(ps2->logger);

            ps2->iop_bus->s14x_nand = ps2->s14x_nand;
            ps2->iop_bus->s14x_syscon = ps2->s14x_syscon;
            ps2->iop_bus->s14x_sram = ps2->s14x_sram;
            ps2->iop_bus->s14x_link = ps2->s14x_link;

            // Note: Pac-Man Arcade Party (System 147) drops input if USB is enabled
            //       for whatever reason.
            iop::bus::set_usb_disabled(ps2->iop_bus, 1);

            s14x::link::register_node(ps2->s14x_link, 2, s14x::ioboard::handle_packet, ps2->s14x_ioboard);
            s14x::link::register_node(ps2->s14x_link, 3, s14x::aiboard::handle_packet, ps2->s14x_aiboard);

            iop::dma::set_dev9_mode(ps2->iop_dma, iop::dma::DEV9_MODE_NAND);
        } break;

        case NAMCO_SYSTEM_246:
        case NAMCO_SYSTEM_256: {
            ee_ram_size = system == NAMCO_SYSTEM_246 ? ram::Size::_32MB : ram::Size::_64MB;
            iop_ram_size = system == NAMCO_SYSTEM_246 ? ram::Size::_2MB : ram::Size::_4MB;
            mechacon_model = cdvd::MECHACON_DRAGON;

            ps2->s2x6_acata = s2x6::acata::create(ps2->logger, ps2->iop_intc, ps2->sched);
            ps2->s2x6_acjv = s2x6::acjv::create(ps2->logger);

            ps2->iop_bus->s2x6_acata = ps2->s2x6_acata;

            // The IOP DMA was wired before this board existed
            iop::dma::connect(ps2->iop_dma, ps2->cdvd, ps2->ee_dma, ps2->sio2, ps2->spu2, ps2->s2x6_acata);
            ps2->iop_bus->s2x6_acjv = ps2->s2x6_acjv;

            iop::dma::set_dev9_mode(ps2->iop_dma, iop::dma::DEV9_MODE_ACATA);
        } break;

        default: {
            iris_error(ps2, "Unknown system {}", system);

            ee_ram_size = ram::Size::_32MB;
            iop_ram_size = ram::Size::_2MB;
            mechacon_model = cdvd::MECHACON_DRAGON;
        } break;
    }

    ps2->detected_system = system;

    ram::destroy(ps2->ee_ram);
    ram::destroy(ps2->iop_ram);

    ps2->ee_ram = ram::create(ps2->logger, ee_ram_size);
    ps2->iop_ram = ram::create(ps2->logger, iop_ram_size);

    cdvd::set_mechacon_model(ps2->cdvd, mechacon_model);

    ee::BusInterface bus_data;
    bus_data.read8 = ee::bus::read8;
    bus_data.read16 = ee::bus::read16;
    bus_data.read32 = ee::bus::read32;
    bus_data.read64 = ee::bus::read64;
    bus_data.read128 = ee::bus::read128;
    bus_data.write8 = ee::bus::write8;
    bus_data.write16 = ee::bus::write16;
    bus_data.write32 = ee::bus::write32;
    bus_data.write64 = ee::bus::write64;
    bus_data.write128 = ee::bus::write128;
    bus_data.udata = ps2->ee_bus;

    ee::set_ram_size(ps2->ee, (int)ee_ram_size);

    ps2->ee_bus->ee_ram = ps2->ee_ram;
    ps2->ee_bus->iop_ram = ps2->iop_ram;
    ps2->iop_bus->iop_ram = ps2->iop_ram;

    ee::bus::init_fastmem(ps2->ee_bus, ps2->ee_ram->size, ps2->iop_ram->size);
    iop::bus::init_fastmem(ps2->iop_bus, ps2->iop_ram->size);

    ee::flush_cache(ps2->ee);
}

void set_mac_address(Ps2* ps2, const uint8_t* mac) {
    speed::set_mac_address(ps2->speed, mac);
}

}