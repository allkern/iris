#ifndef ATA_H
#define ATA_H

#ifdef __cplusplus
extern "C" {
#endif

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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "../speed.h"

#define ATA_SECTOR_SIZE 512
#define ATA_PIO_MODE 4
#define ATA_NUM_HEADS 16
#define ATA_SECTORS_PER_TRACK 63

// Sony ATA commands
enum {
    ATA_C_NOP                             = 0x00,
    ATA_C_CFA_REQUEST_EXTENDED_ERROR_CODE = 0x03,
    ATA_C_DATA_SET_MANAGEMENT             = 0x06,
    ATA_C_DATA_SET_MANAGEMENT_XL          = 0x07,
    ATA_C_DEVICE_RESET                    = 0x08,
    ATA_C_REQUEST_SENSE_DATA_EXT          = 0x0B,
    ATA_C_RECALIBRATE                     = 0x10,
    ATA_C_RECALIBRATE_11H                 = 0x11,
    ATA_C_RECALIBRATE_12H                 = 0x12,
    ATA_C_RECALIBRATE_13H                 = 0x13,
    ATA_C_RECALIBRATE_14H                 = 0x14,
    ATA_C_RECALIBRATE_15H                 = 0x15,
    ATA_C_RECALIBRATE_16H                 = 0x16,
    ATA_C_RECALIBRATE_17H                 = 0x17,
    ATA_C_RECALIBRATE_18H                 = 0x18,
    ATA_C_RECALIBRATE_19H                 = 0x19,
    ATA_C_RECALIBRATE_1AH                 = 0x1A,
    ATA_C_RECALIBRATE_1BH                 = 0x1B,
    ATA_C_RECALIBRATE_1CH                 = 0x1C,
    ATA_C_RECALIBRATE_1DH                 = 0x1D,
    ATA_C_RECALIBRATE_1EH                 = 0x1E,
    ATA_C_RECALIBRATE_1FH                 = 0x1F,
    ATA_C_READ_SECTOR                     = 0x20,
    ATA_C_READ_SECTOR_WITHOUT_RETRY       = 0x21,
    ATA_C_READ_LONG                       = 0x22,
    ATA_C_READ_LONG_WITHOUT_RETRY         = 0x23,
    ATA_C_READ_SECTOR_EXT                 = 0x24,
    ATA_C_READ_DMA_EXT,
    ATA_C_READ_DMA_QUEUED_EXT         = 0x26,
    ATA_C_READ_NATIVE_MAX_ADDRESS_EXT = 0x27,
    ATA_C_READ_MULTIPLE_EXT           = 0x29,
    ATA_C_READ_STREAM_DMA             = 0x2A,
    ATA_C_READ_STREAM_EXT             = 0x2B,
    ATA_C_READ_LOG_EXT                = 0x2F,
    ATA_C_WRITE_SECTOR                = 0x30,
    ATA_C_WRITE_SECTOR_WITHOUT_RETRY  = 0x31,
    ATA_C_WRITE_LONG                  = 0x32,
    ATA_C_WRITE_LONG_WITHOUT_RETRY    = 0x33,
    ATA_C_WRITE_SECTOR_EXT            = 0x34,
    ATA_C_WRITE_DMA_EXT,
    ATA_C_WRITE_DMA_QUEUED_EXT            = 0x36,
    ATA_C_SET_MAX_ADDRESS_EXT             = 0x37,
    ATA_C_CFA_WRITE_SECTORS_WITHOUT_ERASE = 0x38,
    ATA_C_WRITE_MULTIPLE_EXT              = 0x39,
    ATA_C_WRITE_STREAM_DMA                = 0x3A,
    ATA_C_WRITE_STREAM_EXT                = 0x3B,
    ATA_C_WRITE_VERIFY                    = 0x3C,
    ATA_C_WRITE_DMA_FUA_EXT               = 0x3D,
    ATA_C_WRITE_DMA_QUEUED_FUA_EXT        = 0x3E,
    ATA_C_WRITE_LOG_EXT                   = 0x3F,
    ATA_C_READ_VERIFY_SECTOR              = 0x40,
    ATA_C_READ_VERIFY_SECTOR_WITHOUT_RETRY = 0x41,
    ATA_C_READ_VERIFY_SECTOR_EXT          = 0x42,
    ATA_C_ZERO_EXT                        = 0x44,
    ATA_C_WRITE_UNCORRECTABLE_EXT         = 0x45,
    ATA_C_READ_LOG_DMA_EXT                = 0x47,
    ATA_C_ZAC_MANAGEMENT_IN               = 0x4A,
    ATA_C_FORMAT_TRACK                    = 0x50,
    ATA_C_CONFIGURE_STREAM                = 0x51,
    ATA_C_WRITE_LOG_DMA_EXT               = 0x57,
    ATA_C_TRUSTED_NON_DATA                = 0x5B,
    ATA_C_TRUSTED_RECEIVE                 = 0x5C,
    ATA_C_TRUSTED_RECEIVE_DMA             = 0x5D,
    ATA_C_TRUSTED_SEND                    = 0x5E,
    ATA_C_TRUSTED_SEND_DMA                = 0x5F,
    ATA_C_READ_FPDMA_QUEUED               = 0x60,
    ATA_C_WRITE_FPDMA_QUEUED              = 0x61,
    ATA_C_SATA_62H                        = 0x62,
    ATA_C_NCQ_NON_DATA                    = 0x63,
    ATA_C_SEND_FPDMA_QUEUED               = 0x64,
    ATA_C_RECEIVE_FPDMA_QUEUED            = 0x65,
    ATA_C_SATA_66H                        = 0x66,
    ATA_C_SATA_67H                        = 0x67,
    ATA_C_SATA_68H                        = 0x68,
    ATA_C_SATA_69H                        = 0x69,
    ATA_C_SATA_6AH                        = 0x6A,
    ATA_C_SATA_6BH                        = 0x6B,
    ATA_C_SATA_6CH                        = 0x6C,
    ATA_C_SATA_6DH                        = 0x6D,
    ATA_C_SATA_6EH                        = 0x6E,
    ATA_C_SATA_6FH                        = 0x6F,
    ATA_C_SEEK                            = 0x70,
    ATA_C_SEEK_71H                        = 0x71,
    ATA_C_SEEK_72H                        = 0x72,
    ATA_C_SEEK_73H                        = 0x73,
    ATA_C_SEEK_74H                        = 0x74,
    ATA_C_SEEK_75H                        = 0x75,
    ATA_C_SEEK_76H                        = 0x76,
    ATA_C_SET_TIME_DATA_EXT               = 0x77,
    ATA_C_ACCESSIBLE_MAX_ADDRESS_CONFIGURATION = 0x78,
    ATA_C_SEEK_79H                        = 0x79,
    ATA_C_SEEK_7AH                        = 0x7A,
    ATA_C_SEEK_7BH                        = 0x7B,
    ATA_C_REMOVE_ELEMENT_AND_TRUNCATE     = 0x7C,
    ATA_C_RESTORE_ELEMENTS_AND_REBUILD    = 0x7D,
    ATA_C_SEEK_7EH                        = 0x7E,
    ATA_C_SEEK_7FH                        = 0x7F,
    ATA_C_CFA_TRANSLATE_SECTOR            = 0x87,
    ATA_C_SCE_SECURITY_CONTROL            = 0x8e,
    ATA_C_EXECUTE_DEVICE_DIAGNOSTIC       = 0x90,
    ATA_C_INITIALIZE_DEVICE_PARAMETERS    = 0x91,
    ATA_C_DOWNLOAD_MICROCODE              = 0x92,
    ATA_C_DOWNLOAD_MICROCODE_DMA          = 0x93,
    ATA_C_STANDBY_IMMEDIATE_94H           = 0x94,
    ATA_C_IDLE_IMMEDIATE_95H              = 0x95,
    ATA_C_MUTATE                          = 0x96,
    ATA_C_IDLE_97H                        = 0x97,
    ATA_C_CHECK_POWER_MODE_98H            = 0x98,
    ATA_C_SLEEP_99H                       = 0x99,
    ATA_C_ZAC_MANAGEMENT_OUT              = 0x9F,
    ATA_C_PACKET                          = 0xa0,
    ATA_C_IDENTIFY_PACKET_DEVICE,
    ATA_C_SERVICE,
    ATA_C_SMART             = 0xb0,
    ATA_C_DEVICE_CONFIGURATION,
    ATA_C_SET_SECTOR_CONFIGURATION_EXT,
    ATA_C_SANATIZE_DEVICE = 0xb4,
    ATA_C_NV_CACHE = 0xb6,
    ATA_C_CFA_KEY_MANAGEMENT = 0xb9,
    ATA_C_CFA_ERASE_SECTORS = 0xc0,
    ATA_C_READ_MULTIPLE     = 0xc4,
    ATA_C_WRITE_MULTIPLE,
    ATA_C_SET_MULTIPLE_MODE,
    ATA_C_READ_DMA_QUEUED,
    ATA_C_READ_DMA,
    ATA_C_READ_DMA_WITHOUT_RETRIES,
    ATA_C_WRITE_DMA        = 0xca,
    ATA_C_WRITE_DMA_WITHOUT_RETRIES,
    ATA_C_WRITE_DMA_QUEUED = 0xcc,
    ATA_C_CFA_WRITE_MULTIPLE_WITHOUT_ERASE,
    ATA_C_WRITE_MULTIPLE_FUA_EXT,
    ATA_C_CHECK_MEDIA_CARD_TYPE = 0xd1,
    ATA_C_GET_MEDIA_STATUS = 0xda,
    ATA_C_ACKNOWLEDGE_MEDIA_CHANGE = 0xdb,
    ATA_C_BOOT_POST_BOOT = 0xdc,
    ATA_C_BOOT_PRE_BOOT = 0xdd,
    ATA_C_MEDIA_LOCK       = 0xde,
    ATA_C_MEDIA_UNLOCK,
    ATA_C_STANDBY_IMMEDIATE = 0xe0,
    ATA_C_IDLE_IMMEDIATE,
    ATA_C_STANDBY,
    ATA_C_IDLE,
    ATA_C_READ_BUFFER,
    ATA_C_CHECK_POWER_MODE,
    ATA_C_SLEEP,
    ATA_C_FLUSH_CACHE,
    ATA_C_WRITE_BUFFER,
    ATA_C_WRITE_SAME_READ_BUFFER_DMA = 0xe9,
    ATA_C_FLUSH_CACHE_EXT = 0xea,
    ATA_C_WRITE_BUFFER_DMA = 0xeb,
    ATA_C_IDENTIFY_DEVICE = 0xec,
    ATA_C_MEDIA_EJECT,
    ATA_C_IDENTIFY_DEVICE_DMA = 0xee,

    ATA_C_SET_FEATURES = 0xef,

    ATA_C_SECURITY_SET_PASSWORD = 0xf1,
    ATA_C_SECURITY_UNLOCK,
    ATA_C_SECURITY_ERASE_PREPARE,
    ATA_C_SECURITY_ERASE_UNIT,
    ATA_C_SECURITY_FREEZE_LOCK,
    ATA_C_SECURITY_DISABLE_PASSWORD,

    ATA_C_READ_NATIVE_MAX_ADDRESS = 0xf8,
    ATA_C_SET_MAX_ADDRESS,
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

struct ps2_ata {
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

    uint8_t* buf;
    uint32_t buf_index;
    uint32_t buf_size;

    uint8_t identify[ATA_SECTOR_SIZE];

    struct ps2_speed* speed;
};

struct ps2_ata* ps2_ata_create(void);
void ps2_ata_init(struct ps2_ata* ata, struct ps2_speed* speed);
int ps2_ata_load(struct ps2_ata* ata, const char* path);
void ps2_ata_destroy(struct ps2_ata* ata);
uint64_t ps2_ata_read16(struct ps2_ata* ata, uint32_t addr);
uint64_t ps2_ata_read32(struct ps2_ata* ata, uint32_t addr);
void ps2_ata_write16(struct ps2_ata* ata, uint32_t addr, uint64_t data);
void ps2_ata_write32(struct ps2_ata* ata, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif