#include <new>

#include "../speed.hpp"
#include "ata.hpp"

#include "shared/ata/isif.hpp"
#include "shared/ata/raw.hpp"
#include "shared/ata/disc.hpp"

namespace iris::speed::ata {

Ata* create(logger::Logger* logger) {
    Ata* ata = new Ata();

    ata->logger = logger;
    ata->logger_id = logger::register_source(logger, "speed_ata");

    return ata;
}

const char* ata_get_register_name(uint32_t addr, int rw) {
    switch (addr) {
        case 0x0040: return "DATA";
        case 0x0042: return rw ? "FEATURE" : "ERROR";
        case 0x0044: return "NSECTOR";
        case 0x0046: return "SECTOR";
        case 0x0048: return "LCYL";
        case 0x004a: return "HCYL";
        case 0x004c: return "SELECT";
        case 0x004e: return rw ? "COMMAND" : "STATUS";
        case 0x005c: return "CONTROL";
    }

    return "UNKNOWN";
}

void ata_create_identify(uint8_t* buf, uint64_t sectors);

void ata_init_security_data(Ata* ata) {
    // Note: Taken from PCSX2
    memcpy(ata->sce_security_data, "Sony Computer Entertainment Inc.", 32); // Always this magic header.
    memcpy(ata->sce_security_data + 0x20, "SCPH-20401", 10); // sometimes this matches HDD model, the rest 6 bytes filles with zeroes, or sometimes with spaces
    memcpy(ata->sce_security_data + 0x30, " 120", 4); // or " 120" for PSX DESR, reference for ps2 area size. The rest bytes filled with zeroes

    ata->sce_security_data[0x40] = 0; // 0x40 - 0x43 - 4-byte HDD internal SCE serial, does not match real HDD serial, currently hardcoded to 0x1000000
    ata->sce_security_data[0x41] = 0;
    ata->sce_security_data[0x42] = 0;
    ata->sce_security_data[0x43] = 0x01;

    // purpose of next 12 bytes is unknown
    ata->sce_security_data[0x44] = 0; // always zero
    ata->sce_security_data[0x45] = 0; // always zero
    ata->sce_security_data[0x46] = 0x1a;
    ata->sce_security_data[0x47] = 0x01;
    ata->sce_security_data[0x48] = 0x02;
    ata->sce_security_data[0x49] = 0x20;
    ata->sce_security_data[0x4a] = 0; // always zero
    ata->sce_security_data[0x4b] = 0; // always zero
    // next 4 bytes always these values
    ata->sce_security_data[0x4c] = 0x01;
    ata->sce_security_data[0x4d] = 0x03;
    ata->sce_security_data[0x4e] = 0x11;
    ata->sce_security_data[0x4f] = 0x01;
}

int load_security_data(Ata* ata, const char* path) {
    if (!path || !path[0])
        return 0;

    FILE* file = fopen(path, "rb");

    if (!file) {
        iris_error(ata, "Failed to open HDD ID file \"{}\"", path);

        return 0;
    }

    uint8_t buf[SCE_SECURITY_DATA_SIZE];

    size_t read = fread(buf, 1, sizeof(buf), file);

    fclose(file);

    if (read != sizeof(buf)) {
        iris_error(ata, "HDD ID file \"{}\" is {} bytes, expected {}", path, read, sizeof(buf));

        return 0;
    }

    memcpy(ata->sce_security_data, buf, sizeof(buf));

    return 1;
}

void init(Ata* ata, Speed* speed, scheduler::Scheduler* sched) {
    logger::Logger* logger = ata->logger;
    size_t logger_id = ata->logger_id;

    new (ata) Ata();

    ata->logger = logger;
    ata->logger_id = logger_id;

    ata->speed = speed;
    ata->sched = sched;

    // Note: See atad ata_device_probe
    ata->nsector = 1;
    ata->sector = 1;

    ata->status = STAT_READY | STAT_SEEK;

    ata_init_security_data(ata);
}

void ata_create_identify(uint8_t* buf, uint64_t sectors) {
	memset(buf, 0, SECTOR_SIZE);

    struct ata_identify* identify = (struct ata_identify*)buf;

	// Default CHS translation
	uint16_t default_cyls = (sectors > 16514064 ? 16514064 : sectors) / NUM_HEADS / SECTORS_PER_TRACK;
    uint16_t current_cyls = default_cyls;


    identify->general_configuration = 0x0040; // Non-removable
    identify->num_cylinders = default_cyls;
    identify->specific_configuration = 0xc837; // taken from a real PS2 HDD
    identify->num_heads = NUM_HEADS;
    identify->bytes_per_track = SECTOR_SIZE * SECTORS_PER_TRACK;
    identify->bytes_per_sector = SECTOR_SIZE;
    identify->num_sectors_per_track = SECTORS_PER_TRACK;

    // Generate a fake serial number (20 ASCII characters)
    for (int i = 0; i < 19; i++) {
        identify->serial_number[i] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"[rand() % 36];
    }

    identify->serial_number[19] = '\0';

    strncpy(identify->firmware_revision, "FIRM100", 8);
    strncpy(identify->model_number, "Iris ATA HDD", 40);

    identify->max_block_transfer = 0x8080;

    // IORDY supported, LBA supported, DMA supported
    identify->capabilities = 0x40000b00;
    identify->obsolete_words51[0] = PIO_MODE << 8;

    // CHS, PIO/MWDMA, UDMA fields valid
    identify->translation_fields_free_fall = 7;
    identify->num_current_cylinders = current_cyls;
    identify->num_current_heads = NUM_HEADS;
    identify->num_current_sectors_per_track = SECTORS_PER_TRACK;
    identify->current_sector_capacity = current_cyls * NUM_HEADS * SECTORS_PER_TRACK;

    // Multi-sector setting is valid
    identify->multi_sector_capabilities = 0x0080;
    identify->user_addressable_sectors = sectors > LBA28_MAX_SECTORS ? LBA28_MAX_SECTORS : sectors;

    // MWDMA modes supported (0,1,2) and active mode (2)
    identify->mwdma_support_active = 0x0407; // or 0x0007

    // PIO 3,4 supported
    identify->pio_support_active = 0x0003;

    identify->minimum_mw_xfer_cycle_time = 120;
    identify->recommended_mw_xfer_cycle_time = 120;
    identify->minimum_pio_cycle_time = 120;
    identify->minimum_pio_cycle_time_iordy = 120;

    identify->major_revision = 0x0070;
    identify->minor_revision = 0x0018;

    // SMART, Write cache, NOP, FLUSH CACHE, FLUSH CACHE EXT
    // 48-bit address feature set (word 83/86 bit 10)
    // SMART error logging
    // SMART self-test
    identify->feature_sets_supported[0] = 0x4021;
    identify->feature_sets_supported[1] = 0x7400;
    identify->feature_sets_supported[2] = 0x0003;
    identify->feature_sets_active[0] = 0x4021;
    identify->feature_sets_active[1] = 0x3400;
    identify->feature_sets_active[2] = 0x0003;

    identify->udma_support_active = 0x007f;

    // Drive 0 passed diagnostic
    identify->hardware_reset_result = 0x4009;

    identify->max_48bit_lba[0] = sectors & 0xffffffff;
    identify->max_48bit_lba[1] = (sectors >> 32) & 0xffffffff;

    identify->physical_logical_sector_size = 0x4000;

    identify->signature = 0xa5;
    identify->checksum = 0;

    for (int i = 0; i < 511; i++) {
        identify->checksum += ((uint8_t*)buf)[i];
    }
}

int load(Ata* ata, const char* path) {
    if (!path || !path[0])
        return 0;

    if (iris::ata::disc::is_compressed(path)) {
        iop::disc::Disc* disc = iris::ata::disc::open(ata->logger, path);

        if (!disc) {
            iris_error(ata, "Failed to open compressed HDD image \"{}\"", path);

            return 0;
        }

        ata->hdd.udata = (void*)disc;

        ata->hdd.read_sector = iris::ata::disc::ata_read_sector;
        ata->hdd.write_sector = iris::ata::disc::ata_write_sector;
        ata->hdd.get_identify = iris::ata::disc::ata_get_identify;
        ata->hdd.get_sector_count = iris::ata::disc::ata_get_sector_count;
        ata->hdd.close = iris::ata::disc::ata_close;

        ata_create_identify(ata->identify, ata->hdd.get_sector_count(ata->hdd.udata));

        return 1;
    }

    iris::ata::isif::Isif* isif = iris::ata::isif::open(ata->logger, path);

    if (!isif) {
        iris::ata::raw::Raw* raw = iris::ata::raw::open(ata->logger, path);

        if (!raw) {
            iris_debug(ata, "Failed to open HDD image");

            return 0;
        }

        ata->hdd.udata = (void*)raw;

        ata->hdd.read_sector = iris::ata::raw::ata_read_sector;
        ata->hdd.write_sector = iris::ata::raw::ata_write_sector;
        ata->hdd.get_identify = iris::ata::raw::ata_get_identify;
        ata->hdd.get_sector_count = iris::ata::raw::ata_get_sector_count;
        ata->hdd.close = iris::ata::raw::ata_close;
    } else {
        ata->hdd.udata = (void*)isif;

        ata->hdd.read_sector = iris::ata::isif::ata_read_sector;
        ata->hdd.write_sector = iris::ata::isif::ata_write_sector;
        ata->hdd.get_identify = iris::ata::isif::ata_get_identify;
        ata->hdd.get_sector_count = iris::ata::isif::ata_get_sector_count;
        ata->hdd.close = iris::ata::isif::ata_close;
    }

    if (ata->hdd.get_identify(ata->hdd.udata, ata->identify) == 0) {
        iris_debug(ata, "Failed to get identify data");

        // Create new identify data based on the size of the image
        // if the image doesn't provide valid identify data.
        ata_create_identify(ata->identify, ata->hdd.get_sector_count(ata->hdd.udata));
    }

    return 1;
}

void destroy(Ata* ata) {
    if (ata->hdd.close) {
        ata->hdd.close(ata->hdd.udata);
    }

    delete ata;
}

int ata_get_drive(Ata* ata) {
    return (ata->select >> 4) & 1;
}

void ata_init_response(Ata* ata, int size) {
    if (size > SECTOR_SIZE)
        size = SECTOR_SIZE;

    ata->status |= STAT_DRQ;

    ata->buf_index = 0;
    ata->buf_size = size;

    memset(ata->buf, 0, size);
}

void ata_handle_data_overflow(Ata* ata) {
    // iris_debug(ata, "Data overflow (pending sectors {}) command={:02x}", ata->pending_sectors, ata->command);

    switch (ata->command) {
        case C_IDENTIFY_DEVICE: {
            ata->status &= ~STAT_DRQ;
        } break;

        case C_WRITE_DMA:
        case C_WRITE_DMA_EXT:
        case C_WRITE_SECTOR:
        case C_WRITE_SECTOR_EXT: {
            ata->hdd.write_sector(ata->hdd.udata, ata->pending_lba++, ata->buf);

            ata->pending_sectors--;

            if (ata->pending_sectors == 0) {
                ata->status &= ~STAT_DRQ;
            } else {
                ata->buf_index = 0;
                ata->buf_size = 512;
            }
        } break;

        case C_READ_DMA:
        case C_READ_DMA_EXT:
        case C_READ_SECTOR:
        case C_READ_SECTOR_EXT: {
            if (ata->pending_sectors == 0) {
                ata->status &= ~STAT_DRQ;

                return;
            }

            ata->hdd.read_sector(ata->hdd.udata, ata->pending_lba++, ata->buf);

            ata->buf_index = 0;
            ata->buf_size = 512;

            ata->pending_sectors--;
        } break;

        case C_SCE_SECURITY_CONTROL: {
            ata->status &= ~STAT_DRQ;
        } break;
    }
}

uint64_t ata_get_lba(Ata* ata) {
    return (uint64_t)(ata->sector & 0xff) |
           ((uint64_t)(ata->lcyl & 0xff) << 8) |
           ((uint64_t)(ata->hcyl & 0xff) << 16) |
           ((uint64_t)(ata->select & 0x0f) << 24);
}

uint64_t ata_get_lba48(Ata* ata) {
    return (uint64_t)(ata->sector & 0xff) |
           ((uint64_t)(ata->lcyl & 0xff) << 8) |
           ((uint64_t)(ata->hcyl & 0xff) << 16) |
           ((uint64_t)(ata->sector_hob & 0xff) << 24) |
           ((uint64_t)(ata->lcyl_hob & 0xff) << 32) |
           ((uint64_t)(ata->hcyl_hob & 0xff) << 40);
}

uint64_t ata_get_nsectors(Ata* ata) {
    if (ata->nsector == 0) {
        return 0x100;
    }

    return ata->nsector;
}

uint64_t ata_get_nsectors48(Ata* ata) {
    uint64_t nsectors = (ata->nsector & 0xff) | ((ata->nsector_hob & 0xff) << 8);

    if (nsectors == 0) {
        return LBA48_MAX_NSECTORS;
    }

    return nsectors;
}

void ata_set_lba28_result(Ata* ata, uint64_t lba) {
    ata->sector = lba & 0xff;
    ata->lcyl = (lba >> 8) & 0xff;
    ata->hcyl = (lba >> 16) & 0xff;
    ata->select = (ata->select & 0xf0) | ((lba >> 24) & 0x0f);
}

void ata_set_lba48_result(Ata* ata, uint64_t lba) {
    ata->sector = lba & 0xff;
    ata->lcyl = (lba >> 8) & 0xff;
    ata->hcyl = (lba >> 16) & 0xff;
    ata->sector_hob = (lba >> 24) & 0xff;
    ata->lcyl_hob = (lba >> 32) & 0xff;
    ata->hcyl_hob = (lba >> 40) & 0xff;
}

void ata_start_read(Ata* ata, uint64_t lba, uint64_t nsectors) {
    ata_init_response(ata, SECTOR_SIZE);

    ata->pending_lba = lba;
    ata->pending_sectors = nsectors - 1;

    ata->hdd.read_sector(ata->hdd.udata, ata->pending_lba++, ata->buf);
}

void ata_start_write(Ata* ata, uint64_t lba, uint64_t nsectors) {
    ata_init_response(ata, SECTOR_SIZE);

    ata->pending_lba = lba;
    ata->pending_sectors = nsectors;
}

void ata_handle_command(Ata* ata, uint16_t cmd) {
    switch (cmd) {
        case C_IDENTIFY_DEVICE: {
            iris_debug(ata, "IDENTIFY DEVICE");

            ata_init_response(ata, 512);

            memcpy(ata->buf, ata->identify, SECTOR_SIZE);
        } break;

        case C_READ_DMA:
        case C_READ_SECTOR: {
            uint64_t lba = ata_get_lba(ata);
            uint64_t nsectors = ata_get_nsectors(ata);

            iris_debug(ata, "{} (LBA {} COUNT {})", cmd == C_READ_DMA ? "READ DMA" : "READ SECTOR", lba, nsectors);

            ata_start_read(ata, lba, nsectors);
        } break;

        case C_READ_DMA_EXT:
        case C_READ_SECTOR_EXT: {
            uint64_t lba = ata_get_lba48(ata);
            uint64_t nsectors = ata_get_nsectors48(ata);

            iris_debug(ata, "{} (LBA {} COUNT {})", cmd == C_READ_DMA_EXT ? "READ DMA EXT" : "READ SECTOR EXT", lba, nsectors);

            ata_start_read(ata, lba, nsectors);
        } break;

        case C_WRITE_DMA:
        case C_WRITE_SECTOR: {
            uint64_t lba = ata_get_lba(ata);
            uint64_t nsectors = ata_get_nsectors(ata);

            iris_debug(ata, "{} (LBA {} COUNT {})", cmd == C_WRITE_DMA ? "WRITE DMA" : "WRITE SECTOR", lba, nsectors);

            ata_start_write(ata, lba, nsectors);
        } break;

        case C_WRITE_DMA_EXT:
        case C_WRITE_SECTOR_EXT: {
            uint64_t lba = ata_get_lba48(ata);
            uint64_t nsectors = ata_get_nsectors48(ata);

            iris_debug(ata, "{} (LBA {} COUNT {})", cmd == C_WRITE_DMA_EXT ? "WRITE DMA EXT" : "WRITE SECTOR EXT", lba, nsectors);

            ata_start_write(ata, lba, nsectors);
        } break;

        case C_READ_NATIVE_MAX_ADDRESS: {
            uint64_t sectors = ata->hdd.get_sector_count(ata->hdd.udata);
            uint64_t max = sectors > LBA28_MAX_SECTORS ? LBA28_MAX_SECTORS : sectors;

            iris_debug(ata, "READ NATIVE MAX ADDRESS ({})", max);

            ata_set_lba28_result(ata, max ? max - 1 : 0);
        } break;

        case C_READ_NATIVE_MAX_ADDRESS_EXT: {
            uint64_t sectors = ata->hdd.get_sector_count(ata->hdd.udata);

            iris_debug(ata, "READ NATIVE MAX ADDRESS EXT ({})", sectors);

            ata_set_lba48_result(ata, sectors ? sectors - 1 : 0);
        } break;

        case C_IDLE: {
            iris_debug(ata, "IDLE");
        } break;

        case C_SMART: {
            iris_debug(ata, "SMART command subcommand {}", ata->feature);
        } break;

        case C_FLUSH_CACHE:
        case C_FLUSH_CACHE_EXT: {
            iris_debug(ata, "FLUSH CACHE");
        } break;

        case C_SET_FEATURES: {
            iris_debug(ata, "SET FEATURES subcommand {}", ata->feature);
        } break;

        case C_SCE_SECURITY_CONTROL: {
            iris_debug(ata, "SCE SECURITY CONTROL subcommand {:02x}", ata->feature);

            if (ata->feature == 0xec) {
                iris_debug(ata, "SCE SECURITY CONTROL - Get security data");

                ata_init_response(ata, 512);

                memcpy(ata->buf, ata->sce_security_data, 512);

                break;
            }

            iris_warning(ata, "Unhandled SCE SECURITY CONTROL subcommand {:02x}", ata->feature);

            ata->error |= ERR_ABORT;
            ata->status |= STAT_ERR;
        } break;

        default: {
            if (cmd != ata->last_unhandled_command) {
                ata->last_unhandled_command = cmd;

                iris_warning(ata, "Unhandled command {:02x}", cmd);
            }
        } break;
    }
}

uint16_t ata_handle_data_read(Ata* ata) {
    if (ata->buf_index >= ata->buf_size) {
        return 0;
    }

    uint16_t value = ata->buf[ata->buf_index] | (ata->buf[ata->buf_index + 1] << 8);

    // iris_debug(ata, "Data read {:04x} (index {} lba {:08x})", value, ata->buf_index, ata->pending_lba);

    ata->buf_index += 2;

    if (ata->buf_index >= ata->buf_size) {
        ata_handle_data_overflow(ata);
    }

    return value;
}

void ata_handle_data_write(Ata* ata, uint16_t value) {
    if (ata->buf_index >= ata->buf_size) {
        return;
    }

    ata->buf[ata->buf_index] = value & 0xff;
    ata->buf[ata->buf_index + 1] = (value >> 8) & 0xff;

    ata->buf_index += 2;

    if (ata->buf_index >= ata->buf_size) {
        ata_handle_data_overflow(ata);
    }
}

uint16_t ata_read(Ata* ata, uint32_t addr) {
    // iris_debug(ata, "Read {} (drive {}, status {:02x}, control {:02x})", ata_get_register_name(addr, 0), ata_get_drive(ata), ata->status, ata->control);
    if (!ata->hdd.udata)
        return 0;

    // Only allow reads from the SELECT reg when slave is selected
    if (ata_get_drive(ata) && addr != 0x4c) return 0;

    // High Order Byte
    int hob = ata->control & CONTROL_HOB;

    switch (addr) {
        case 0x40: return ata_handle_data_read(ata);
        case 0x42: /* iris_debug(ata, "error read {:04x}", ata->error); */ return ata->error;
        case 0x44: return hob ? ata->nsector_hob : ata->nsector;
        case 0x46: return hob ? ata->sector_hob : ata->sector;
        case 0x48: return hob ? ata->lcyl_hob : ata->lcyl;
        case 0x4a: return hob ? ata->hcyl_hob : ata->hcyl;
        case 0x4c: /* iris_debug(ata, "select read {:04x}", ata->select); */ return ata->select;

        // Note: This is the status reg offset, reading from this reg
        //       clears the interrupt flags
        //       Reg 5C reads the same status reg as 4e but without
        //       clearing the interrupt flags
        case 0x4e: { /* iris_debug(ata, "status read {:04x}", ata->status); */
            ata->speed->intr_stat &= INTR_ATA0;
            
            return ata->status;
        }

        case 0x5c: /* iris_debug(ata, "status alt read {:04x}", ata->status); */ return ata->status;
    }

    iris_debug(ata, "read from unknown register {:08x}", addr);

    return 0;
}

void ata_reset_busy_event(void* udata, int overshoot) {
    Ata* ata = (Ata*)udata;

    ata->status &= ~STAT_BUSY;

    send_irq(ata->speed, INTR_ATA0);
}

void ata_write(Ata* ata, uint32_t addr, uint64_t data) {
    if (!ata->hdd.udata)
        return;

    // iris_debug(ata, "Write {} {:08x} (drive {})", ata_get_register_name(addr, 1), data, ata_get_drive(ata));

    if (ata_get_drive(ata) && (addr != 0x4c && addr != 0x5c))
        return;

    switch (addr) {
        case 0x40: ata_handle_data_write(ata, data); return;
        case 0x42: ata->feature = data & 0xff; return;
        case 0x44: ata->nsector_hob = ata->nsector; ata->nsector = data & 0xff; return;
        case 0x46: ata->sector_hob = ata->sector; ata->sector = data & 0xff; return;
        case 0x48: ata->lcyl_hob = ata->lcyl; ata->lcyl = data & 0xff; return;
        case 0x4a: ata->hcyl_hob = ata->hcyl; ata->hcyl = data & 0xff; return;
        case 0x4c: ata->select = data & 0xff; return;
        case 0x4e: {
            ata->command = data;

            ata->status |= STAT_BUSY;

            scheduler::Event event;

            event.callback = ata_reset_busy_event;
            event.udata = ata;
            event.name = "ata reset busy";
            event.cycles = 1000;

            scheduler::schedule(ata->sched, event);

            ata_handle_command(ata, ata->command);

            return;
        } break;
        case 0x005c: {
            ata->control = data & 0xff;

            if (data & CONTROL_SRST) {
                // iris_debug(ata, "Software reset");

                ata->status = STAT_READY | STAT_SEEK;
                ata->error = 1;
                ata->sector = 1;
                ata->nsector = 1;
                ata->lcyl = 0;
                ata->hcyl = 0;
                ata->sector_hob = 0;
                ata->nsector_hob = 0;
                ata->lcyl_hob = 0;
                ata->hcyl_hob = 0;
                ata->buf_index = 0;
                ata->buf_size = 0;
                ata->pending_sectors = 0;
                ata->pending_lba = 0;
            }

            return;
        } break;
    }

    iris_debug(ata, "write to unknown register {:08x}", addr);
}

uint64_t read16(Ata* ata, uint32_t addr) {
    return ata_read(ata, addr);
}

uint64_t read32(Ata* ata, uint32_t addr) {
    return ata_read(ata, addr);
}

void write16(Ata* ata, uint32_t addr, uint64_t data) {
    ata_write(ata, addr, data);
}

void write32(Ata* ata, uint32_t addr, uint64_t data) {
    ata_write(ata, addr, data);
}

}
