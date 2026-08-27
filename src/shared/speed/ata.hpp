#pragma once

/*
    SPEED ATA docs
    --------------

    This is basically just a standard ATA interface connected to the SPEED chip.
    It's mapped starting at 10000040 (EE side 14000040).

    Registers (X=0 IOP side, X=4 EE side):
    1X000040 - DATA
    1X000042 - ERROR
             - FEATURE
    1X000044 - NSECTOR
    1X000046 - SECTOR
    1X000048 - LCYL
    1X00004a - HCYL
    1X00004c - SELECT
    1X00004e - STATUS
             - COMMAND
    1X000050 - 1X00004B - unused
    1X00005c - CONTROL
*/


#include "logger.hpp"
#include "scheduler.hpp"

namespace iris::speed { struct Speed; }

namespace iris::speed::ata {

inline constexpr auto SECTOR_SIZE = 512;
inline constexpr auto PIO_MODE = 4;
inline constexpr auto NUM_HEADS = 16;
inline constexpr auto SECTORS_PER_TRACK = 63;

inline constexpr auto ATAPI_DVD_SECTOR_SIZE = 2048;
inline constexpr auto SCE_SECURITY_DATA_SIZE = 0x80;

inline constexpr auto ERR_ABORT = 0x04;
inline constexpr auto CONTROL_NIEN = 0x02;
inline constexpr auto CONTROL_SRST = 0x04;

inline constexpr auto STAT_ERR = 0x01;
inline constexpr auto STAT_INDEX = 0x02;
inline constexpr auto STAT_CORR = 0x04;
inline constexpr auto STAT_DRQ = 0x08;
inline constexpr auto STAT_SEEK = 0x10;
inline constexpr auto STAT_WRERR = 0x20;
inline constexpr auto STAT_READY = 0x40;
inline constexpr auto STAT_BUSY = 0x80;

// Sony ATA commands
enum {
    C_NOP                             = 0x00,
    C_CFA_REQUEST_EXTENDED_ERROR_CODE = 0x03,
    C_DATA_SET_MANAGEMENT             = 0x06,
    C_DATA_SET_MANAGEMENT_XL          = 0x07,
    C_DEVICE_RESET                    = 0x08,
    C_REQUEST_SENSE_DATA_EXT          = 0x0B,
    C_RECALIBRATE                     = 0x10,
    C_RECALIBRATE_11H                 = 0x11,
    C_RECALIBRATE_12H                 = 0x12,
    C_RECALIBRATE_13H                 = 0x13,
    C_RECALIBRATE_14H                 = 0x14,
    C_RECALIBRATE_15H                 = 0x15,
    C_RECALIBRATE_16H                 = 0x16,
    C_RECALIBRATE_17H                 = 0x17,
    C_RECALIBRATE_18H                 = 0x18,
    C_RECALIBRATE_19H                 = 0x19,
    C_RECALIBRATE_1AH                 = 0x1A,
    C_RECALIBRATE_1BH                 = 0x1B,
    C_RECALIBRATE_1CH                 = 0x1C,
    C_RECALIBRATE_1DH                 = 0x1D,
    C_RECALIBRATE_1EH                 = 0x1E,
    C_RECALIBRATE_1FH                 = 0x1F,
    C_READ_SECTOR                     = 0x20,
    C_READ_SECTOR_WITHOUT_RETRY       = 0x21,
    C_READ_LONG                       = 0x22,
    C_READ_LONG_WITHOUT_RETRY         = 0x23,
    C_READ_SECTOR_EXT                 = 0x24,
    C_READ_DMA_EXT,
    C_READ_DMA_QUEUED_EXT         = 0x26,
    C_READ_NATIVE_MAX_ADDRESS_EXT = 0x27,
    C_READ_MULTIPLE_EXT           = 0x29,
    C_READ_STREAM_DMA             = 0x2A,
    C_READ_STREAM_EXT             = 0x2B,
    C_READ_LOG_EXT                = 0x2F,
    C_WRITE_SECTOR                = 0x30,
    C_WRITE_SECTOR_WITHOUT_RETRY  = 0x31,
    C_WRITE_LONG                  = 0x32,
    C_WRITE_LONG_WITHOUT_RETRY    = 0x33,
    C_WRITE_SECTOR_EXT            = 0x34,
    C_WRITE_DMA_EXT,
    C_WRITE_DMA_QUEUED_EXT            = 0x36,
    C_SET_MAX_ADDRESS_EXT             = 0x37,
    C_CFA_WRITE_SECTORS_WITHOUT_ERASE = 0x38,
    C_WRITE_MULTIPLE_EXT              = 0x39,
    C_WRITE_STREAM_DMA                = 0x3A,
    C_WRITE_STREAM_EXT                = 0x3B,
    C_WRITE_VERIFY                    = 0x3C,
    C_WRITE_DMA_FUA_EXT               = 0x3D,
    C_WRITE_DMA_QUEUED_FUA_EXT        = 0x3E,
    C_WRITE_LOG_EXT                   = 0x3F,
    C_READ_VERIFY_SECTOR              = 0x40,
    C_READ_VERIFY_SECTOR_WITHOUT_RETRY = 0x41,
    C_READ_VERIFY_SECTOR_EXT          = 0x42,
    C_ZERO_EXT                        = 0x44,
    C_WRITE_UNCORRECTABLE_EXT         = 0x45,
    C_READ_LOG_DMA_EXT                = 0x47,
    C_ZAC_MANAGEMENT_IN               = 0x4A,
    C_FORMAT_TRACK                    = 0x50,
    C_CONFIGURE_STREAM                = 0x51,
    C_WRITE_LOG_DMA_EXT               = 0x57,
    C_TRUSTED_NON_DATA                = 0x5B,
    C_TRUSTED_RECEIVE                 = 0x5C,
    C_TRUSTED_RECEIVE_DMA             = 0x5D,
    C_TRUSTED_SEND                    = 0x5E,
    C_TRUSTED_SEND_DMA                = 0x5F,
    C_READ_FPDMA_QUEUED               = 0x60,
    C_WRITE_FPDMA_QUEUED              = 0x61,
    C_SATA_62H                        = 0x62,
    C_NCQ_NON_DATA                    = 0x63,
    C_SEND_FPDMA_QUEUED               = 0x64,
    C_RECEIVE_FPDMA_QUEUED            = 0x65,
    C_SATA_66H                        = 0x66,
    C_SATA_67H                        = 0x67,
    C_SATA_68H                        = 0x68,
    C_SATA_69H                        = 0x69,
    C_SATA_6AH                        = 0x6A,
    C_SATA_6BH                        = 0x6B,
    C_SATA_6CH                        = 0x6C,
    C_SATA_6DH                        = 0x6D,
    C_SATA_6EH                        = 0x6E,
    C_SATA_6FH                        = 0x6F,
    C_SEEK                            = 0x70,
    C_SEEK_71H                        = 0x71,
    C_SEEK_72H                        = 0x72,
    C_SEEK_73H                        = 0x73,
    C_SEEK_74H                        = 0x74,
    C_SEEK_75H                        = 0x75,
    C_SEEK_76H                        = 0x76,
    C_SET_TIME_DATA_EXT               = 0x77,
    C_ACCESSIBLE_MAX_ADDRESS_CONFIGURATION = 0x78,
    C_SEEK_79H                        = 0x79,
    C_SEEK_7AH                        = 0x7A,
    C_SEEK_7BH                        = 0x7B,
    C_REMOVE_ELEMENT_AND_TRUNCATE     = 0x7C,
    C_RESTORE_ELEMENTS_AND_REBUILD    = 0x7D,
    C_SEEK_7EH                        = 0x7E,
    C_SEEK_7FH                        = 0x7F,
    C_CFA_TRANSLATE_SECTOR            = 0x87,
    C_SCE_SECURITY_CONTROL            = 0x8e,
    C_EXECUTE_DEVICE_DIAGNOSTIC       = 0x90,
    C_INITIALIZE_DEVICE_PARAMETERS    = 0x91,
    C_DOWNLOAD_MICROCODE              = 0x92,
    C_DOWNLOAD_MICROCODE_DMA          = 0x93,
    C_STANDBY_IMMEDIATE_94H           = 0x94,
    C_IDLE_IMMEDIATE_95H              = 0x95,
    C_MUTATE                          = 0x96,
    C_IDLE_97H                        = 0x97,
    C_CHECK_POWER_MODE_98H            = 0x98,
    C_SLEEP_99H                       = 0x99,
    C_ZAC_MANAGEMENT_OUT              = 0x9F,
    C_PACKET                          = 0xa0,
    C_IDENTIFY_PACKET_DEVICE,
    C_SERVICE,
    C_SMART             = 0xb0,
    C_DEVICE_CONFIGURATION,
    C_SET_SECTOR_CONFIGURATION_EXT,
    C_SANATIZE_DEVICE = 0xb4,
    C_NV_CACHE = 0xb6,
    C_CFA_KEY_MANAGEMENT = 0xb9,
    C_CFA_ERASE_SECTORS = 0xc0,
    C_READ_MULTIPLE     = 0xc4,
    C_WRITE_MULTIPLE,
    C_SET_MULTIPLE_MODE,
    C_READ_DMA_QUEUED,
    C_READ_DMA,
    C_READ_DMA_WITHOUT_RETRIES,
    C_WRITE_DMA        = 0xca,
    C_WRITE_DMA_WITHOUT_RETRIES,
    C_WRITE_DMA_QUEUED = 0xcc,
    C_CFA_WRITE_MULTIPLE_WITHOUT_ERASE,
    C_WRITE_MULTIPLE_FUA_EXT,
    C_CHECK_MEDIA_CARD_TYPE = 0xd1,
    C_GET_MEDIA_STATUS = 0xda,
    C_ACKNOWLEDGE_MEDIA_CHANGE = 0xdb,
    C_BOOT_POST_BOOT = 0xdc,
    C_BOOT_PRE_BOOT = 0xdd,
    C_MEDIA_LOCK       = 0xde,
    C_MEDIA_UNLOCK,
    C_STANDBY_IMMEDIATE = 0xe0,
    C_IDLE_IMMEDIATE,
    C_STANDBY,
    C_IDLE,
    C_READ_BUFFER,
    C_CHECK_POWER_MODE,
    C_SLEEP,
    C_FLUSH_CACHE,
    C_WRITE_BUFFER,
    C_WRITE_SAME_READ_BUFFER_DMA = 0xe9,
    C_FLUSH_CACHE_EXT = 0xea,
    C_WRITE_BUFFER_DMA = 0xeb,
    C_IDENTIFY_DEVICE = 0xec,
    C_MEDIA_EJECT,
    C_IDENTIFY_DEVICE_DMA = 0xee,

    C_SET_FEATURES = 0xef,

    C_SECURITY_SET_PASSWORD = 0xf1,
    C_SECURITY_UNLOCK,
    C_SECURITY_ERASE_PREPARE,
    C_SECURITY_ERASE_UNIT,
    C_SECURITY_FREEZE_LOCK,
    C_SECURITY_DISABLE_PASSWORD,

    C_READ_NATIVE_MAX_ADDRESS = 0xf8,
    C_SET_MAX_ADDRESS,
};

struct __attribute__((packed)) ata_identify {
    uint16_t general_configuration;
    uint16_t num_cylinders;
    uint16_t specific_configuration;
    uint16_t num_heads;
    uint16_t bytes_per_track;
    uint16_t bytes_per_sector;
    uint16_t num_sectors_per_track;
    uint16_t vendor_unique1[3];
    char serial_number[20];
    uint16_t retired2[2];
    uint16_t obsolete1;
    char firmware_revision[8];
    char model_number[40];
    uint16_t max_block_transfer;
    uint16_t trusted_computing;
    uint32_t capabilities;
    uint16_t obsolete_words51[2];
    uint16_t translation_fields_free_fall;
    uint16_t num_current_cylinders;
    uint16_t num_current_heads;
    uint16_t num_current_sectors_per_track;
    uint32_t current_sector_capacity;
    uint16_t multi_sector_capabilities;
    uint32_t user_addressable_sectors;
    uint16_t obsolete_word62;
    uint16_t mwdma_support_active;
    uint16_t pio_support_active;
    uint16_t minimum_mw_xfer_cycle_time;
    uint16_t recommended_mw_xfer_cycle_time;
    uint16_t minimum_pio_cycle_time;
    uint16_t minimum_pio_cycle_time_iordy;
    uint16_t additional_supported_capabilities;
    uint16_t reserved_words70[5];
    uint16_t queue_depth;
    uint32_t sata_capabilities;
    uint16_t sata_features_supported;
    uint16_t sata_features_enabled;
    uint16_t major_revision;
    uint16_t minor_revision;
    uint16_t feature_sets_supported[3];
    uint16_t feature_sets_active[3];
    uint16_t udma_support_active;
    uint16_t normal_security_erase_unit;
    uint16_t enhanced_security_erase_unit;
    uint16_t current_apm_level;
    uint16_t master_password_id;
    uint16_t hardware_reset_result;
    uint16_t acoustic_value;
    uint16_t stream_min_request_size;
    uint16_t streaming_transfer_time_dma;
    uint16_t streaming_access_latency_dma_pio;
    uint32_t streaming_perf_granularity;
    uint32_t max_48bit_lba[2];
    uint16_t streaming_transfer_time;
    uint16_t dsm_cap;
    uint16_t physical_logical_sector_size;
    uint16_t inter_seek_delay;
    uint16_t world_wide_name[4];
    uint16_t reserved_for_world_wide_name128[4];
    uint16_t reserved_for_tlc_technical_report;
    uint16_t words_per_logical_sector[2];
    uint16_t command_sets_supported_ext;
    uint16_t command_sets_active_ext;
    uint16_t reserved_for_expanded_support_and_active[6];
    uint16_t msn_support;
    uint16_t security_status;
    uint16_t reserved_word129[31];
    uint16_t cfa_power_mode1;
    uint16_t reserved_for_cfa_word161[7];
    uint16_t nominal_form_factor;
    uint16_t data_set_management_feature;
    uint16_t additional_productid[4];
    uint16_t reserved_for_cfa_word174[2];
    uint16_t current_media_serial_number[30];
    uint16_t sct_command_transport;
    uint16_t reserved_word207[2];
    uint16_t block_alignment;
    uint16_t write_read_verify_sector_count_mode3_only[2];
    uint16_t write_read_verify_sector_count_mode2_only[2];
    uint16_t nv_cache_capabilities;
    uint16_t nv_cache_sizelsw;
    uint16_t nv_cache_sizemsw;
    uint16_t nominal_media_rotation_rate;
    uint16_t reserved_word218;
    uint16_t nv_cache_options;
    uint16_t write_read_verify_sector_count_mode;
    uint16_t reserved_word221;
    uint16_t transport_major_version;
    uint16_t transport_minor_version;
    uint16_t reserved_word224[6];
    uint32_t extended_number_of_user_addressable_sectors[2];
    uint16_t min_blocks_per_download_microcode_mode03;
    uint16_t max_blocks_per_download_microcode_mode03;
    uint16_t reserved_word236[19];
    uint8_t signature;
    uint8_t checksum;
};

struct Hdd {
    void (*write_sector)(void* udata, uint64_t lba, const uint8_t* data);
    void (*read_sector)(void* udata, uint64_t lba, uint8_t* data);
    int (*get_identify)(void* udata, uint8_t* buf);
    void (*close)(void* udata);
    uint64_t (*get_sector_count)(void* udata);

    void* udata;
};

struct Ata {
    Hdd hdd;

    uint16_t data;
    uint16_t error;
    uint16_t feature;
    uint16_t nsector;
    uint16_t sector;
    uint16_t lcyl;
    uint16_t hcyl;
    uint16_t select;
    uint16_t status;
    uint16_t command;
    uint16_t control;

    uint8_t buf[SECTOR_SIZE];
    uint32_t buf_index;
    uint32_t buf_size;

    uint64_t pending_sectors;
    uint64_t pending_lba;

    uint8_t identify[SECTOR_SIZE];

    int last_unhandled_command;
    uint8_t sce_security_data[512];

    Speed* speed;
    scheduler::Scheduler* sched;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Ata* create(logger::Logger* logger);
void init(Ata* ata, Speed* speed, scheduler::Scheduler* sched);
int load(Ata* ata, const char* path);
int load_security_data(Ata* ata, const char* path);
void destroy(Ata* ata);
uint64_t read16(Ata* ata, uint32_t addr);
uint64_t read32(Ata* ata, uint32_t addr);
void write16(Ata* ata, uint32_t addr, uint64_t data);
void write32(Ata* ata, uint32_t addr, uint64_t data);
uint16_t ata_read(Ata* ata, uint32_t addr);
void ata_write(Ata* ata, uint32_t addr, uint64_t data);

}
