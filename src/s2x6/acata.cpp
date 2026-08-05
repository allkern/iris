#include <new>

#include "acata.hpp"

namespace iris::s2x6::acata {

Acata* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched) {
    Acata* acata = new Acata();

    acata->logger = logger;
    acata->logger_id = logger::register_source(logger, "acata");

    acata->intc = intc;
    acata->sched = sched;

    acata->nsector = 1;
    acata->sector = 1;

    acata->status = speed::ata::STAT_READY | speed::ata::STAT_SEEK;

    acata->dvd.file = fopen("NM00007 SC21 DVD0D (DVD-ROM).iso", "rb");

    fseek(acata->dvd.file, 0, SEEK_END);
    acata->dvd.num_sectors = ftell(acata->dvd.file) / speed::ata::ATAPI_DVD_SECTOR_SIZE;
    fseek(acata->dvd.file, 0, SEEK_SET);

    return acata;
}

static const uint16_t IDENTIFY_PACKET_DEVICE[256] = {
    0x8500,
    0,0,0,0,0,0,0,0,0,
    /* serial [10]*/
    0x2020,0x2020,0x2020,0x2020,0x2020,
    /* firmware [15]*/
    0x312E,0x3030,0x2020,0x2020,
    /* model [19]*/
    0x4E41,0x4D43,0x4F20,0x4456,//[22
    0x442D,0x524F,0x4D20,0x4452,//[26
    0x4956,0x4520,0x2020,0x2020,//[30
    0x2020,0x2020,0x2020,0x2020,//[34
    0x2020,0x2020,0x2020,0x2020,//[38
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //[50
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,// [61
    0x0007, //Word 62: Single-word DMA
    0x0007, //Word 63: Multiword DMA
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,//[80]
    0,0,0,0,0,0,0,//[87]
    0x0000,//Word 88 — Ultra DMA
};

void reset_busy_event(void* udata, int overshoot) {
    Acata* acata = (Acata*)udata;

    acata->status &= ~speed::ata::STAT_BUSY;
}

const char* get_register_name(uint32_t addr, int rw) {
    switch (addr) {
        case 0: return "DATA";
        case 1: return rw ? "FEATURE" : "ERROR";
        case 2: return "NSECTOR";
        case 3: return "SECTOR";
        case 4: return "LCYL";
        case 5: return "HCYL";
        case 6: return "SELECT";
        case 7: return rw ? "COMMAND" : "STATUS";
        case 22: return "CONTROL";
    }

    return "UNKNOWN";
}

void create_identify(uint8_t* buf, uint64_t sectors) {
	memset(buf, 0, speed::ata::SECTOR_SIZE);

    speed::ata::ata_identify* identify = (speed::ata::ata_identify*)buf;

	// Default CHS translation
	uint16_t default_cyls = (sectors > 16514064 ? 16514064 : sectors) / speed::ata::NUM_HEADS / speed::ata::SECTORS_PER_TRACK;
    uint16_t current_cyls = default_cyls;

    identify->general_configuration = 0x0040; // Non-removable
    identify->num_cylinders = default_cyls;
    identify->specific_configuration = 0xc837; // taken from a real PS2 HDD
    identify->num_heads = speed::ata::NUM_HEADS;
    identify->bytes_per_track = speed::ata::SECTOR_SIZE * speed::ata::SECTORS_PER_TRACK;
    identify->bytes_per_sector = speed::ata::SECTOR_SIZE;
    identify->num_sectors_per_track = speed::ata::SECTORS_PER_TRACK;

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
    identify->obsolete_words51[0] = speed::ata::PIO_MODE << 8;

    // CHS, PIO/MWDMA, UDMA fields valid
    identify->translation_fields_free_fall = 7;
    identify->num_current_cylinders = current_cyls;
    identify->num_current_heads = speed::ata::NUM_HEADS;
    identify->num_current_sectors_per_track = speed::ata::SECTORS_PER_TRACK;
    identify->current_sector_capacity = current_cyls * speed::ata::NUM_HEADS * speed::ata::SECTORS_PER_TRACK;

    // Multi-sector setting is valid
    identify->multi_sector_capabilities = 0x0080;
    identify->user_addressable_sectors = sectors;

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
    // SMART error logging
    // SMART self-test
    identify->feature_sets_supported[0] = 0x4021;
    identify->feature_sets_supported[1] = 0x7000;
    identify->feature_sets_supported[2] = 0x0003;
    identify->feature_sets_active[0] = 0x4021;
    identify->feature_sets_active[1] = 0x3000;
    identify->feature_sets_active[2] = 0x0003;

    identify->udma_support_active = 0x007f;

    // if (lba48_supported)
    //    identify->feature_sets_supported |= (1 << 26); // LBA48

    // Drive 0 passed diagnostic
    identify->hardware_reset_result = 0x4009;

    // if (lba48_supported) {
    //     identify->max_48bit_lba[0] = sectors & 0xffffffff;
    //     identify->max_48bit_lba[1] = (sectors >> 32) & 0xffffffff;
    // }

    identify->physical_logical_sector_size = 0x4000;

    identify->signature = 0xa5;
    identify->checksum = 0;

    for (int i = 0; i < 511; i++) {
        identify->checksum += ((uint8_t*)buf)[i];
    }
}

int load(Acata* acata, const char* path) {
    ata::isif::Isif* isif = ata::isif::open(nullptr, path);

    if (!isif) {
        ata::raw::Raw* raw = ata::raw::open(nullptr, path);

        if (!raw) {
            iris_debug(acata, "Failed to open HDD image");

            return 0;
        }

        acata->hdd.udata = (void*)raw;

        acata->hdd.read_sector = ata::raw::ata_read_sector;
        acata->hdd.write_sector = ata::raw::ata_write_sector;
        acata->hdd.get_identify = ata::raw::ata_get_identify;
        acata->hdd.get_sector_count = ata::raw::ata_get_sector_count;
        acata->hdd.close = ata::raw::ata_close;
    } else {
        acata->hdd.udata = (void*)isif;

        acata->hdd.read_sector = ata::isif::ata_read_sector;
        acata->hdd.write_sector = ata::isif::ata_write_sector;
        acata->hdd.get_identify = ata::isif::ata_get_identify;
        acata->hdd.get_sector_count = ata::isif::ata_get_sector_count;
        acata->hdd.close = ata::isif::ata_close;
    }

    if (acata->hdd.get_identify(acata->hdd.udata, acata->identify) == 0) {
        iris_debug(acata, "Failed to get identify data");

        // Create new identify data based on the size of the image
        // if the image doesn't provide valid identify data.
        create_identify(acata->identify, acata->hdd.get_sector_count(acata->hdd.udata));
    }

    return 1;
}

void destroy(Acata* acata) {
    if (acata->hdd.close) {
        acata->hdd.close(acata->hdd.udata);
    }

    delete acata;
}

int get_drive(Acata* acata) {
    return (acata->select >> 4) & 1;
}

void init_response(Acata* acata, int size) {
    acata->status |= speed::ata::STAT_DRQ;

    acata->buf_index = 0;
    acata->buf_size = size;
    acata->buf = (uint8_t *)malloc(size);

    memset(acata->buf, 0, acata->buf_size);
}

void atapi_packet_response_event(void* udata, int cycles) {
    Acata* acata = (Acata*)udata;

    // iris_debug(acata, "ATAPI packet response event");

    iop::intc::irq(acata->intc, iop::intc::DEV9);
}

AtapiPacket atapi_process_packet(Acata* acata) {
    AtapiPacket packet;

    packet.cmd = acata->buf[0];
    packet.lba = (acata->buf[2] << 24) | (acata->buf[3] << 16) | (acata->buf[4] << 8) | acata->buf[5];
    packet.len = (acata->buf[7] << 8) | acata->buf[8];

    return packet;
}

void atapi_read_dvd(AtapiDvd* dvd, uint64_t lba, uint64_t count, uint8_t* buf) {
    fseek(dvd->file, lba * speed::ata::ATAPI_DVD_SECTOR_SIZE, SEEK_SET);
    fread(buf, 1, count * speed::ata::ATAPI_DVD_SECTOR_SIZE, dvd->file);
}

void atapi_handle_command(Acata* acata, AtapiPacket* packet) {
    // iris_debug(acata, "Handling ATAPI packet command {:02x}", packet->cmd);

    switch (packet->cmd) {
        // TEST UNIT READY
        case 0x00: {
            iris_debug(acata, "ATAPI TEST UNIT READY");
        } break;

        // READ
        case 0x28: {
            iris_debug(acata, "ATAPI READ (LBA {} COUNT {})", packet->lba, packet->len);

            if (packet->len == 0) {
                iris_fatal_error(acata, "ATAPI READ with length 0, treating as 65536");
            }

            acata->atapi_response = 1;

            init_response(acata, packet->len * speed::ata::ATAPI_DVD_SECTOR_SIZE);

            atapi_read_dvd(&acata->dvd, packet->lba, packet->len, acata->buf);
        } break;

        default: {
            iris_debug(acata, "Unhandled ATAPI packet command {:02x}", packet->cmd);
        } break;
    }
}

void handle_data_overflow(Acata* acata) {
    // iris_debug(acata, "Data overflow (pending sectors {}) command={:02x}", acata->pending_sectors, acata->command);

    switch (acata->command) {
        case speed::ata::C_IDENTIFY_PACKET_DEVICE:
        case speed::ata::C_IDENTIFY_DEVICE: {
            acata->status &= ~speed::ata::STAT_DRQ;
            iris_debug(acata, "DRQ cleared");
        } break;

        case speed::ata::C_WRITE_DMA: {
            acata->hdd.write_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);

            acata->pending_sectors--;

            if (acata->pending_sectors == 0) {
                acata->status &= ~speed::ata::STAT_DRQ;
                iris_debug(acata, "DRQ cleared");
            } else {
                acata->buf_index = 0;
                acata->buf_size = 512;
            }
        } break;

        case speed::ata::C_READ_DMA:
        case speed::ata::C_READ_SECTOR: {
            if (acata->pending_sectors == 0) {
                acata->status &= ~speed::ata::STAT_DRQ;
                iris_debug(acata, "DRQ cleared");

                return;
            }

            acata->hdd.read_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);

            acata->buf_index = 0;
            acata->buf_size = 512;

            acata->pending_sectors--;
        } break;

        case speed::ata::C_PACKET: {
            acata->status &= ~speed::ata::STAT_DRQ;
            acata->status |= speed::ata::STAT_READY;
            iris_debug(acata, "DRQ cleared");

            if (acata->atapi_response) {
                acata->atapi_response = 0;

                return;
            }

            AtapiPacket packet = atapi_process_packet(acata);

            // iris_debug(acata, "ATAPI packet command {:02x} lba {:08x} len {:08x}", packet.cmd, packet.lba, packet.len);

            atapi_handle_command(acata, &packet);

            scheduler::Event event;

            event.callback = atapi_packet_response_event;
            event.cycles = 1000;
            event.name = "ATAPI packet response";
            event.udata = acata;

            scheduler::schedule(acata->sched, event);
        } break;

        case speed::ata::C_SCE_SECURITY_CONTROL: {
            acata->status &= ~speed::ata::STAT_DRQ;
            iris_debug(acata, "DRQ cleared");
        } break;
    }
}

uint64_t get_lba(Acata* acata) {
    return acata->sector | (acata->lcyl << 8) | (acata->hcyl << 16) | ((acata->select & 0x0f) << 24);
}

uint64_t get_nsectors(Acata* acata) {
    if (acata->nsector == 0) {
        return 0x100;
    }

    return acata->nsector;
}

void handle_command(Acata* acata, uint16_t cmd) {
    switch (cmd) {
        case speed::ata::C_NOP: {
            iris_debug(acata, "NOP");
        } break;

        case speed::ata::C_IDENTIFY_DEVICE: {
            iris_debug(acata, "IDENTIFY DEVICE");

            init_response(acata, 512);

            memcpy(acata->buf, acata->identify, speed::ata::SECTOR_SIZE);
        } break;

        case speed::ata::C_READ_DMA: {
            iris_debug(acata, "READ DMA (LBA {} COUNT {})", acata->sector, acata->nsector);

            acata->pending_sectors = get_nsectors(acata) - 1;
            acata->pending_lba = get_lba(acata);

            acata->status |= speed::ata::STAT_DRQ;
            acata->buf_size = 512;
            acata->buf_index = 0;

            acata->hdd.read_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);
        } break;

        case speed::ata::C_WRITE_DMA: {
            iris_debug(acata, "WRITE DMA (LBA {} COUNT {})", acata->sector, acata->nsector);

            acata->pending_sectors = get_nsectors(acata);
            acata->pending_lba = get_lba(acata);

            acata->status |= speed::ata::STAT_DRQ;
            acata->buf_size = 512;
            acata->buf_index = 0;
        } break;

        case speed::ata::C_READ_SECTOR: {
            iris_debug(acata, "READ SECTOR (LBA {} COUNT {})", acata->sector, acata->nsector);

            init_response(acata, 512);

            acata->pending_sectors = get_nsectors(acata) - 1;
            acata->pending_lba = get_lba(acata);

            acata->status |= speed::ata::STAT_DRQ;
            acata->buf_size = 512;
            acata->buf_index = 0;

            acata->hdd.read_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);
        } break;

        case speed::ata::C_IDLE: {
            iris_debug(acata, "IDLE");
        } break;

        case speed::ata::C_SMART: {
            iris_debug(acata, "SMART command subcommand {}", acata->feature);
        } break;

        case speed::ata::C_FLUSH_CACHE: {
            iris_debug(acata, "FLUSH CACHE");
        } break;

        case speed::ata::C_SET_FEATURES: {
            iris_debug(acata, "SET FEATURES subcommand {}", acata->feature);

            return;
        } break;

        case speed::ata::C_PACKET: {
            iris_debug(acata, "PACKET");

            acata->status |= speed::ata::STAT_DRQ;
            acata->buf_size = 12;
            acata->buf_index = 0;
        } break;

        case speed::ata::C_IDENTIFY_PACKET_DEVICE: {
            iris_debug(acata, "IDENTIFY PACKET DEVICE");

            init_response(acata, 512);

            memcpy(acata->buf, IDENTIFY_PACKET_DEVICE, speed::ata::SECTOR_SIZE);
        } break;

        case speed::ata::C_SCE_SECURITY_CONTROL: {
            iris_debug(acata, "SCE SECURITY CONTROL command subcommand {:02x}", acata->feature);

            // if (acata->feature == 0xec) {
            //     iris_debug(acata, "SCE SECURITY CONTROL - Get security data");

            //     init_response(acata, 512);

            //     memcpy(acata->buf, acata->sce_security_data, 512);
            // } break;
        } break;

        default: {
            iris_fatal_error(acata, "Unhandled command {:02x}", cmd);
        } break;
    }

    acata->status |= speed::ata::STAT_BUSY;

    scheduler::Event event;

    event.callback = reset_busy_event;
    event.udata = acata;
    event.name = "acata reset busy";
    event.cycles = 1000;

    scheduler::schedule(acata->sched, event);
}

uint16_t handle_data_read(Acata* acata) {
     if (acata->buf_index >= acata->buf_size) {
        return 0;
    }

    uint16_t value = acata->buf[acata->buf_index] | (acata->buf[acata->buf_index + 1] << 8);

    // iris_debug(acata, "Data read {:04x} (index {} lba {:08x})", value, acata->buf_index, acata->pending_lba);

    acata->buf_index += 2;

    // iris_debug(acata, "Data read {:04x} (index {} lba {:08x})", value, acata->buf_index, acata->pending_lba);

    if (acata->buf_index >= acata->buf_size) {
        handle_data_overflow(acata);
    }

    return value;
}

void handle_data_write(Acata* acata, uint16_t value) {
    if (acata->buf_index >= acata->buf_size) {
        return;
    }

    acata->buf[acata->buf_index] = value & 0xff;
    acata->buf[acata->buf_index + 1] = (value >> 8) & 0xff;

    acata->buf_index += 2;

    if (acata->buf_index >= acata->buf_size) {
        handle_data_overflow(acata);
    }
}

uint16_t read(Acata* acata, uint32_t addr) {
    iris_debug(acata, "Read {} (drive {}, status {:02x}, control {:02x})", get_register_name(addr, 0), get_drive(acata), acata->status, acata->control);

    // if (!acata->hdd.udata)
    //     return 0;

    // Only allow reads from the SELECT reg when slave is selected
    if (get_drive(acata) && addr != 6) return 0;

    switch (addr) {
        case 0: return handle_data_read(acata);
        case 1: /* iris_debug(acata, "error read {:04x}", acata->error); */ return acata->error;
        case 2: /* iris_debug(acata, "nsector read {:04x}", acata->nsector); */ return acata->nsector & 0xff;
        case 3: /* iris_debug(acata, "sector read {:04x}", acata->sector); */ return acata->sector;
        case 4: /* iris_debug(acata, "lcyl read {:04x}", acata->lcyl); */ return acata->lcyl;
        case 5: /* iris_debug(acata, "hcyl read {:04x}", acata->hcyl); */ return acata->hcyl;
        case 6: /* iris_debug(acata, "select read {:04x}", acata->select); */ return acata->select;

        // Note: This is the status reg offset, reading from this reg
        //       clears the interrupt flags
        //       Reg 5C reads the same status reg as 4e but without
        //       clearing the interrupt flags
        case 7: { /* iris_debug(acata, "status read {:04x}", acata->status); */
            // acata->speed->intr_stat &= SPD_INTR_ATA0;
            
            return acata->status;
        }

        case 22: /* iris_debug(acata, "status alt read {:04x}", acata->status); */ return acata->status;
    }

    iris_debug(acata, "read from unknown register {:08x}", addr);

    return 0;
}

void write(Acata* acata, uint32_t addr, uint64_t data) {
    // if (!acata->hdd.udata)
    //     return;

    // iris_debug(acata, "Write {} {:08x} (drive {})", get_register_name(addr, 1), data, get_drive(acata));

    if (get_drive(acata) && (addr != 6 && addr != 22))
        return;

    switch (addr) {
        case 0: handle_data_write(acata, data); return;
        case 1: acata->feature = data; return;
        case 2: acata->nsector = data & 0xff; return;
        case 3: acata->sector = data; return;
        case 4: acata->lcyl = data; return;
        case 5: acata->hcyl = data; return;
        case 6: acata->select = data; return;
        case 7: {
            acata->command = data;

            handle_command(acata, acata->command);

            return;
        } break;
        case 22: {
            if (data & 2 || data & 4) {
                iris_debug(acata, "Software reset");

                acata->status = speed::ata::STAT_READY | speed::ata::STAT_SEEK;
                acata->sector = 1;
                acata->nsector = 1;
            }

            return;
        } break;
    }

    iris_debug(acata, "write to unknown register {:08x}", addr);
}

uint64_t read16(Acata* acata, uint32_t addr) {
    return read(acata, (addr >> 16) & 0x3f);
}

void write16(Acata* acata, uint32_t addr, uint64_t data) {
    write(acata, (addr >> 16) & 0x3f, data);
}

}
