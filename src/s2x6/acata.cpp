#include <new>

#include "acata.hpp"

namespace iris::s2x6::acata {

Acata* create(logger::Logger* logger, accore::Accore* accore, scheduler::Scheduler* sched) {
    Acata* acata = new Acata();

    acata->logger = logger;
    acata->logger_id = logger::register_source(logger, "acata");

    acata->accore = accore;
    acata->sched = sched;

    acata->nsector = 1;
    acata->sector = 1;

    acata->status = speed::ata::STAT_READY | speed::ata::STAT_SEEK;

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

    if (!acata->irq_on_ready)
        return;

    acata->irq_on_ready = 0;

    accore::irq(acata->accore, accore::IRQ_ATA);
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

static void disc_read_sector(void* udata, uint64_t lba, uint8_t* data) {
    iop::disc::read_sector((iop::disc::Disc*)udata, data, lba, iop::disc::DISC_SS_DATA);
}

static void disc_write_sector(void* udata, uint64_t lba, const uint8_t* data) {
    // Compressed images are read only
}

static int disc_get_identify(void* udata, uint8_t* buf) {
    return 0;
}

static uint64_t disc_get_sector_count(void* udata) {
    iop::disc::Disc* disc = (iop::disc::Disc*)udata;

    int sector_size = iop::disc::get_sector_size(disc);

    if (sector_size <= 0)
        sector_size = speed::ata::SECTOR_SIZE;

    return iop::disc::get_size(disc) / sector_size;
}

static void disc_close(void* udata) {
    iop::disc::close((iop::disc::Disc*)udata);
}

static int load_compressed_hdd(Acata* acata, const char* path) {
    iop::disc::Disc* disc = iop::disc::open(acata->logger, path);

    if (!disc)
        return 0;

    acata->hdd.udata = (void*)disc;

    acata->hdd.read_sector = disc_read_sector;
    acata->hdd.write_sector = disc_write_sector;
    acata->hdd.get_identify = disc_get_identify;
    acata->hdd.get_sector_count = disc_get_sector_count;
    acata->hdd.close = disc_close;

    return 1;
}

int load(Acata* acata, const char* path, int media) {
    acata->media = media;

    if (media != MEDIA_HDD) {
        acata->disc = iop::disc::open(acata->logger, path);

        if (!acata->disc) {
            iris_error(acata, "Failed to open media image \"{}\"", path);

            return 0;
        }

        acata->media_sectors = iop::disc::get_size(acata->disc) / speed::ata::ATAPI_DVD_SECTOR_SIZE;

        return 1;
    }

    int ext = iop::disc::get_extension(path);

    if (ext == iop::disc::DISC_EXT_CHD || ext == iop::disc::DISC_EXT_CSO || ext == iop::disc::DISC_EXT_ZSO) {
        if (!load_compressed_hdd(acata, path)) {
            iris_error(acata, "Failed to open hard disk image \"{}\"", path);

            return 0;
        }

        create_identify(acata->identify, acata->hdd.get_sector_count(acata->hdd.udata));

        return 1;
    }

    ata::isif::Isif* isif = ata::isif::open(acata->logger, path);

    if (!isif) {
        ata::raw::Raw* raw = ata::raw::open(acata->logger, path);

        if (!raw) {
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
        // Create new identify data based on the size of the image
        // if the image doesn't provide valid identify data.
        create_identify(acata->identify, acata->hdd.get_sector_count(acata->hdd.udata));
    }

    return 1;
}

// The board arbitrates the DEV9 channel between the drive and the extra RAM
bool dma_pending(Acata* acata) {
    return (acata->status & speed::ata::STAT_DRQ) != 0;
}

void destroy(Acata* acata) {
    free(acata->buf);

    if (acata->hdd.close) {
        acata->hdd.close(acata->hdd.udata);
    }

    if (acata->disc) {
        iop::disc::close(acata->disc);
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

    int allocated = (size + 1) & ~1;

    free(acata->buf);

    acata->buf = (uint8_t *)malloc(allocated);

    memset(acata->buf, 0, allocated);
}

void atapi_init_response(Acata* acata, int size) {
    acata->atapi_response = 1;

    init_response(acata, size);

    // To-do: Split transfers past this into chunks
    int count = size > 0xfffe ? 0xfffe : size;

    acata->lcyl = count & 0xff;
    acata->hcyl = (count >> 8) & 0xff;
    acata->nsector = ATAPI_PHASE_DATA_IN;
}

void atapi_init_request(Acata* acata, int size) {
    acata->atapi_response = 1;

    init_response(acata, size);

    acata->lcyl = size & 0xff;
    acata->hcyl = (size >> 8) & 0xff;
    acata->nsector = ATAPI_PHASE_DATA_OUT;
}

void atapi_complete(Acata* acata) {
    acata->lcyl = 0;
    acata->hcyl = 0;
    acata->nsector = ATAPI_PHASE_COMPLETE;
}

void atapi_packet_response_event(void* udata, int cycles) {
    Acata* acata = (Acata*)udata;

    accore::irq(acata->accore, accore::IRQ_ATA);
}

void atapi_raise_interrupt(Acata* acata) {
    scheduler::Event event;

    event.callback = atapi_packet_response_event;
    event.cycles = 1000;
    event.name = "ATAPI packet response";
    event.udata = acata;

    scheduler::schedule(acata->sched, event);
}

AtapiPacket atapi_process_packet(Acata* acata) {
    AtapiPacket packet;

    memcpy(packet.raw, acata->buf, sizeof(packet.raw));

    packet.cmd = acata->buf[0];
    packet.lba = (acata->buf[2] << 24) | (acata->buf[3] << 16) | (acata->buf[4] << 8) | acata->buf[5];
    packet.len = (acata->buf[7] << 8) | acata->buf[8];

    return packet;
}

void atapi_read_media(Acata* acata, uint64_t lba, uint64_t count, uint8_t* buf) {
    int unit = iop::disc::get_sector_size(acata->disc);

    uint64_t units = unit > 0 && unit < speed::ata::ATAPI_DVD_SECTOR_SIZE ?
        speed::ata::ATAPI_DVD_SECTOR_SIZE / unit : 1;

    if (units == 1)
        unit = speed::ata::ATAPI_DVD_SECTOR_SIZE;

    for (uint64_t i = 0; i < count * units; i++) {
        iop::disc::read_sector(acata->disc, buf + (i * unit), (lba * units) + i, iop::disc::DISC_SS_DATA);
    }
}

static const char* get_command_name(uint16_t cmd) {
    switch (cmd) {
        case speed::ata::C_NOP: return "NOP";
        case speed::ata::C_DEVICE_RESET: return "DEVICE_RESET";
        case speed::ata::C_READ_SECTOR: return "READ_SECTOR";
        case speed::ata::C_READ_DMA: return "READ_DMA";
        case speed::ata::C_READ_DMA_WITHOUT_RETRIES: return "READ_DMA_NO_RETRY";
        case speed::ata::C_WRITE_DMA: return "WRITE_DMA";
        case speed::ata::C_WRITE_DMA_WITHOUT_RETRIES: return "WRITE_DMA_NO_RETRY";
        case speed::ata::C_IDLE: return "IDLE";
        case speed::ata::C_CHECK_POWER_MODE: return "CHECK_POWER_MODE";
        case speed::ata::C_SMART: return "SMART";
        case speed::ata::C_FLUSH_CACHE: return "FLUSH_CACHE";
        case speed::ata::C_SET_FEATURES: return "SET_FEATURES";
        case speed::ata::C_PACKET: return "PACKET";
        case speed::ata::C_IDENTIFY_PACKET_DEVICE: return "IDENTIFY_PACKET_DEVICE";
        case speed::ata::C_IDENTIFY_DEVICE: return "IDENTIFY_DEVICE";
        case speed::ata::C_SCE_SECURITY_CONTROL: return "SCE_SECURITY_CONTROL";
    }

    return "UNKNOWN";
}

static const char* get_atapi_command_name(uint8_t cmd) {
    switch (cmd) {
        case ATAPI_TEST_UNIT_READY: return "TEST_UNIT_READY";
        case ATAPI_INQUIRY: return "INQUIRY";
        case ATAPI_READ_CAPACITY: return "READ_CAPACITY";
        case ATAPI_READ: return "READ_10";
        case ATAPI_MODE_SELECT: return "MODE_SELECT";
        case ATAPI_MODE_SENSE: return "MODE_SENSE";
        case ATAPI_SET_STREAMING: return "SET_STREAMING";
        case ATAPI_SET_CD_SPEED: return "SET_CD_SPEED";
    }

    return "UNKNOWN";
}

void atapi_handle_command(Acata* acata, AtapiPacket* packet) {
    iris_debug(acata, "ATAPI {} lba:{:08x} sectors:{:02x} dma:{}",
        get_atapi_command_name(packet->cmd), packet->lba, packet->len,
        acata->feature & FEATURE_DMA);

    switch (packet->cmd) {
        case ATAPI_TEST_UNIT_READY: {
            atapi_complete(acata);
        } break;

        case ATAPI_INQUIRY: {
            int size = 36;

            if (packet->raw[4] && packet->raw[4] < size)
                size = packet->raw[4];

            atapi_init_response(acata, size);

            uint8_t inquiry[36] = {};

            inquiry[0] = 0x05; // CD/DVD-ROM
            inquiry[1] = 0x80; // Removable
            inquiry[3] = 0x21;
            inquiry[4] = 0x1f; // 31 bytes follow

            memcpy(inquiry + 8, "Iris    ", 8);
            memcpy(inquiry + 16, "DVD-ROM         ", 16);
            memcpy(inquiry + 32, "1.00", 4);

            memcpy(acata->buf, inquiry, size);
        } break;

        case ATAPI_READ_CAPACITY: {
            atapi_init_response(acata, 8);

            uint32_t last_lba = acata->media_sectors ? acata->media_sectors - 1 : 0;

            acata->buf[0] = last_lba >> 24;
            acata->buf[1] = last_lba >> 16;
            acata->buf[2] = last_lba >> 8;
            acata->buf[3] = last_lba;
            acata->buf[4] = speed::ata::ATAPI_DVD_SECTOR_SIZE >> 24;
            acata->buf[5] = speed::ata::ATAPI_DVD_SECTOR_SIZE >> 16;
            acata->buf[6] = speed::ata::ATAPI_DVD_SECTOR_SIZE >> 8;
            acata->buf[7] = speed::ata::ATAPI_DVD_SECTOR_SIZE;
        } break;

        case ATAPI_MODE_SENSE: {
            int page = packet->raw[2] & 0x3f;

            uint8_t mode[28] = {};
            int size;

            if (page == 0x01) {
                size = 20;

                mode[1] = 0x12;
                mode[2] = acata->media == MEDIA_DVD ? 0x41 : 0x01;
                mode[8] = 0x01;
                mode[9] = 0x0a;
                mode[11] = 0x05;
            } else if (page == 0x2a) {
                size = 28;

                mode[1] = 0x1a;
                mode[8] = 0x2a;
                mode[9] = 0x12;
                mode[10] = 0x03;
                mode[16] = 0x15;
                mode[17] = 0xa4;
                mode[22] = 0x15;
                mode[23] = 0xa4;
            } else {
                size = 8;

                mode[1] = 0x06;
            }

            if (packet->len && packet->len < size)
                size = packet->len;

            atapi_init_response(acata, size);

            memcpy(acata->buf, mode, size);
        } break;

        case ATAPI_READ: {
            if (!packet->len) {
                atapi_complete(acata);

                return;
            }

            atapi_init_response(acata, packet->len * speed::ata::ATAPI_DVD_SECTOR_SIZE);

            atapi_read_media(acata, packet->lba, packet->len, acata->buf);
        } break;

        case ATAPI_MODE_SELECT:
        case ATAPI_SET_STREAMING: {
            int size = packet->cmd == ATAPI_SET_STREAMING ?
                ((packet->raw[9] << 8) | packet->raw[10]) : packet->len;

            if (!size) {
                atapi_complete(acata);

                return;
            }

            atapi_init_request(acata, size);
        } break;

        case ATAPI_SET_CD_SPEED: {
            atapi_complete(acata);
        } break;

        default: {
            if (packet->cmd != acata->last_unhandled_atapi_command) {
                acata->last_unhandled_atapi_command = packet->cmd;

                iris_error(acata, "Unhandled ATAPI command {:02x}", packet->cmd);
            }

            atapi_complete(acata);
        } break;
    }
}

void handle_data_overflow(Acata* acata) {
    switch (acata->command) {
        case speed::ata::C_IDENTIFY_PACKET_DEVICE:
        case speed::ata::C_IDENTIFY_DEVICE: {
            acata->status &= ~speed::ata::STAT_DRQ;
        } break;

        case speed::ata::C_WRITE_DMA:
        case speed::ata::C_WRITE_DMA_WITHOUT_RETRIES: {
            acata->hdd.write_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);

            acata->pending_sectors--;

            if (acata->pending_sectors == 0) {
                acata->status &= ~speed::ata::STAT_DRQ;
            } else {
                acata->buf_index = 0;
                acata->buf_size = 512;
            }
        } break;

        case speed::ata::C_READ_DMA:
        case speed::ata::C_READ_DMA_WITHOUT_RETRIES:
        case speed::ata::C_READ_SECTOR: {
            if (acata->pending_sectors == 0) {
                acata->status &= ~speed::ata::STAT_DRQ;

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

            if (acata->atapi_response) {
                acata->atapi_response = 0;

                atapi_complete(acata);
                atapi_raise_interrupt(acata);

                return;
            }

            AtapiPacket packet = atapi_process_packet(acata);

            atapi_handle_command(acata, &packet);
            atapi_raise_interrupt(acata);
        } break;

        case speed::ata::C_SCE_SECURITY_CONTROL: {
            acata->status &= ~speed::ata::STAT_DRQ;
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

void reset_signature(Acata* acata) {
    acata->status = speed::ata::STAT_READY | speed::ata::STAT_SEEK;
    acata->error = 0;
    acata->sector = 1;
    acata->nsector = 1;

    if (acata->media == MEDIA_HDD) {
        acata->lcyl = 0;
        acata->hcyl = 0;
    } else {
        acata->lcyl = ATAPI_SIGNATURE_LCYL;
        acata->hcyl = ATAPI_SIGNATURE_HCYL;
    }
}

void handle_command(Acata* acata, uint16_t cmd) {
    iris_debug(acata, "{}: FEATURE:{:04x}, R_NSECTOR:{:04x}, R_SECTOR:{:04x}, R_LCYL:{:04x}, R_HCYL:{:04x}, R_SELECT:{:04x}",
        get_command_name(cmd), acata->feature, acata->nsector, acata->sector,
        acata->lcyl, acata->hcyl, acata->select);

    acata->status &= ~speed::ata::STAT_ERR;
    acata->error = 0;

    bool needs_drive = cmd == speed::ata::C_READ_DMA ||
                       cmd == speed::ata::C_READ_DMA_WITHOUT_RETRIES ||
                       cmd == speed::ata::C_WRITE_DMA ||
                       cmd == speed::ata::C_WRITE_DMA_WITHOUT_RETRIES ||
                       cmd == speed::ata::C_READ_SECTOR;

    if (needs_drive && !acata->hdd.read_sector) {
        iris_error(acata, "Command {:02x} needs a hard drive, but the loaded media isn't one", cmd);

        return;
    }

    switch (cmd) {
        case speed::ata::C_IDLE:
        case speed::ata::C_SMART:
        case speed::ata::C_FLUSH_CACHE:
        case speed::ata::C_SET_FEATURES:
        case speed::ata::C_SCE_SECURITY_CONTROL: break;

        case speed::ata::C_NOP: {
            acata->status |= speed::ata::STAT_BUSY;
            acata->busy_until_read = 1;

            return;
        } break;

        case speed::ata::C_DEVICE_RESET: {
            reset_signature(acata);
        } break;

        case speed::ata::C_IDENTIFY_DEVICE: {
            init_response(acata, speed::ata::SECTOR_SIZE);

            memcpy(acata->buf, acata->identify, speed::ata::SECTOR_SIZE);
        } break;

        case speed::ata::C_READ_DMA:
        case speed::ata::C_READ_DMA_WITHOUT_RETRIES: {
            acata->pending_sectors = get_nsectors(acata) - 1;
            acata->pending_lba = get_lba(acata);

            init_response(acata, speed::ata::SECTOR_SIZE);

            acata->hdd.read_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);
        } break;

        case speed::ata::C_WRITE_DMA:
        case speed::ata::C_WRITE_DMA_WITHOUT_RETRIES: {
            acata->pending_sectors = get_nsectors(acata);
            acata->pending_lba = get_lba(acata);

            init_response(acata, speed::ata::SECTOR_SIZE);
        } break;

        case speed::ata::C_READ_SECTOR: {
            init_response(acata, speed::ata::SECTOR_SIZE);

            acata->pending_sectors = get_nsectors(acata) - 1;
            acata->pending_lba = get_lba(acata);

            acata->hdd.read_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);
        } break;

        case speed::ata::C_CHECK_POWER_MODE: {
            acata->nsector = POWER_MODE_ACTIVE;
        } break;

        case speed::ata::C_PACKET: {
            init_response(acata, sizeof(AtapiPacket::raw));
        } break;

        case speed::ata::C_IDENTIFY_PACKET_DEVICE: {
            init_response(acata, speed::ata::SECTOR_SIZE);

            memcpy(acata->buf, IDENTIFY_PACKET_DEVICE, speed::ata::SECTOR_SIZE);
        } break;

        default: {
            if (cmd != acata->last_unhandled_command) {
                acata->last_unhandled_command = cmd;

                iris_error(acata, "Unhandled command {:02x} (feature {:02x}), aborting it", cmd, acata->feature);
            }

            acata->status |= speed::ata::STAT_ERR;
            acata->error = ERR_ABORT;
        } break;
    }

    acata->status |= speed::ata::STAT_BUSY;
    acata->irq_on_ready = cmd != speed::ata::C_PACKET &&
                          cmd != speed::ata::C_DEVICE_RESET;

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

    acata->buf_index += 2;

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
    // if (!acata->hdd.udata)
    //     return 0;

    if (get_drive(acata) && (addr == 7 || addr == 22))
        return 0;

    switch (addr) {
        case 0: return handle_data_read(acata);
        case 1: return acata->error;
        case 2: return acata->nsector & 0xff;
        case 3: return acata->sector;
        case 4: return acata->lcyl;
        case 5: return acata->hcyl;
        case 6: return acata->select;

        // Note: This is the status reg offset, reading from this reg
        //       clears the interrupt flags
        //       Reg 5C reads the same status reg as 4e but without
        //       clearing the interrupt flags
        case 7: {
            uint16_t status = acata->status;

            if (acata->busy_until_read) {
                acata->busy_until_read = 0;

                acata->status &= ~speed::ata::STAT_BUSY;
            }

            return status;
        }

        case 22: return acata->status;
    }

    return 0;
}

void write(Acata* acata, uint32_t addr, uint64_t data) {
    // if (!acata->hdd.udata)
    //     return;

    if (get_drive(acata) && addr == 7)
        return;

    switch (addr) {
        case 0: handle_data_write(acata, data); return;
        case 1: acata->feature = data & 0xff; return;
        case 2: acata->nsector = data & 0xff; return;
        case 3: acata->sector = data & 0xff; return;
        case 4: acata->lcyl = data & 0xff; return;
        case 5: acata->hcyl = data & 0xff; return;
        case 6: acata->select = data & 0xff; return;
        case 7: {
            acata->command = data & 0xff;

            handle_command(acata, acata->command);

            return;
        } break;
        case 22: {
            acata->control = data & 0xff;

            // Bit 1 masks the interrupt, bit 2 resets
            if (data & CONTROL_RESET) {
                reset_signature(acata);
            }

            return;
        } break;
    }
}

uint64_t read16(Acata* acata, uint32_t addr) {
    return read(acata, (addr >> 16) & 0x3f);
}

void write16(Acata* acata, uint32_t addr, uint64_t data) {
    write(acata, (addr >> 16) & 0x3f, data);
}
}
