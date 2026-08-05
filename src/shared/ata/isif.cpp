
#include "isif.hpp"

namespace iris::ata::isif {

#ifdef _MSC_VER
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#elif defined(_WIN32)
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

static inline int get_bat_entry_size(Isif* isif) {
    if (isif->hdr.block_mode & 1) {
        return 8;
    } else {
        return 4;
    }
}

Isif* open(logger::Logger* logger, const char* path) {
    Isif* isif = new Isif();

    isif->logger = logger;
    isif->logger_id = logger::register_source(logger, "isif");

    isif->file = fopen(path, "r+b");

    if (!isif->file) {
        iris_error(isif, "Unable to open file");

        delete isif;

        return nullptr;
    }

    fread(&isif->hdr, sizeof(Header), 1, isif->file);

    if (isif->hdr.magic != MAGIC) {
        iris_debug(isif, "Invalid ISIF header");

        fclose(isif->file);
        delete isif;

        return nullptr;
    }

    if (isif->hdr.version != 1) {
        iris_debug(isif, "Unsupported ISIF version {}", isif->hdr.version);

        fclose(isif->file);
        delete isif;

        return nullptr;
    }

    if (isif->hdr.block_mode >= 2) {
        iris_debug(isif, "Unsupported block mode {}", isif->hdr.block_mode);

        fclose(isif->file);
        delete isif;

        return nullptr;
    }

    // Determine the size of each BAT entry based on the block mode
    int bat_entry_size = get_bat_entry_size(isif);

    // Cache BAT
    isif->bat = malloc(isif->hdr.block_count * bat_entry_size);

    fseek64(isif->file, isif->hdr.bat_offset, SEEK_SET);
    fread(isif->bat, bat_entry_size, isif->hdr.block_count, isif->file);

    return isif;
}

uint32_t get_version(Isif* isif) {
    return isif->hdr.version;
}

uint64_t get_block_count(Isif* isif) {
    return isif->hdr.block_count;
}

uint32_t get_block_size(Isif* isif) {
    return isif->hdr.block_size;
}

uint16_t get_block_mode(Isif* isif) {
    return isif->hdr.block_mode;
}

uint16_t get_block_compression(Isif* isif) {
    return isif->hdr.block_compression;
}

uint64_t get_total_size(Isif* isif) {
    return isif->hdr.block_count * (uint64_t)isif->hdr.block_size;
}

uint64_t get_allocated_size(Isif* isif) {
    return isif->hdr.bat_block_count * (uint64_t)isif->hdr.block_size;
}

uint64_t read_bat(Isif* isif, uint64_t index) {
    if (isif->hdr.block_mode & 1) {
        return ((uint64_t*)isif->bat)[index];
    } else {
        return ((uint32_t*)isif->bat)[index];
    }
}

void write_bat(Isif* isif, uint64_t index, uint64_t value) {
    if (isif->hdr.block_mode & 1) {
        ((uint64_t*)isif->bat)[index] = value;
    } else {
        ((uint32_t*)isif->bat)[index] = value;
    }
}

int write_is_empty(Isif* isif, uint8_t* data) {
    for (int i = 0; i < isif->hdr.block_size; i++) {
        if (data[i]) return 0;
    }

    return 1;
}

void allocate_block(Isif* isif, uint64_t index) {
    fseek64(isif->file, 0, SEEK_END);

    uint64_t block_offset = ftell64(isif->file) - isif->hdr.data_offset;

    write_bat(isif, index, block_offset);

    isif->hdr.bat_block_count++;
}

// To-do: Implement shrinking when deallocating blocks
void deallocate_block(Isif* isif, uint64_t index) {
    write_bat(isif, index, 1ull << 63);

    isif->hdr.bat_block_count--;
}

void read_extension(Isif* isif, void* buffer) {
    fseek64(isif->file, isif->hdr.extension_offset, SEEK_SET);
    fread(buffer, 1, isif->hdr.data_offset - isif->hdr.extension_offset, isif->file);
}

void read_block(Isif* isif, uint64_t index, void* buf) {
    if (index >= isif->hdr.block_count) {
        iris_debug(isif, "Block index out of range");

        return;
    }

    uint64_t bat_entry = read_bat(isif, index);

    if (bat_entry == (1ull << 63)) {
        memset(buf, 0, isif->hdr.block_size);

        return;
    }

    // Block is unallocated
    if (!bat_entry) {
        memset(buf, 0, isif->hdr.block_size);

        return;
    }

    uint64_t block_offset = isif->hdr.data_offset + bat_entry;

    fseek64(isif->file, block_offset, SEEK_SET);
    fread(buf, 1, isif->hdr.block_size, isif->file);
}

void write_block(Isif* isif, uint64_t index, const void* buf) {
    if (index >= isif->hdr.block_count) {
        iris_debug(isif, "Block index out of range");

        return;
    }

    uint64_t bat_entry = read_bat(isif, index);
    int is_empty = write_is_empty(isif, (uint8_t*)buf);
    int is_deallocated = bat_entry == (1ull << 63);

    bat_entry &= ~(1ull << 63);

    // Entry is unallocated
    if (!bat_entry) {
        // Nothing to do here
        if (is_empty)
            return;

        // Allocate new block
        allocate_block(isif, index);

        // Write block data
        fwrite(buf, 1, isif->hdr.block_size, isif->file);

        return;
    }

    // Entry is allocated
    if (is_empty) {
        // Deallocate block
        deallocate_block(isif, index);

        return;
    }

    // Update block
    uint64_t block_offset = isif->hdr.data_offset + bat_entry;

    fseek64(isif->file, block_offset, SEEK_SET);
    fwrite(buf, 1, isif->hdr.block_size, isif->file);

    return;
}

void close(Isif* isif) {
    // Write back BAT
    int bat_entry_size = get_bat_entry_size(isif);
    
    fseek64(isif->file, isif->hdr.bat_offset, SEEK_SET);
    fwrite(isif->bat, bat_entry_size, isif->hdr.block_count, isif->file);

    // Write back header
    fseek64(isif->file, 0, SEEK_SET);
    fwrite(&isif->hdr, sizeof(Header), 1, isif->file);

    free(isif->bat);
    fclose(isif->file);
    delete isif;
}

int create_image(logger::Logger* logger, const char* path, uint64_t block_count, uint32_t block_size, uint16_t block_mode, uint16_t block_compression, void* ext, uint64_t ext_size) {
    size_t logger_id = logger::register_source(logger, "isif");

    FILE* file = fopen(path, "wb");

    if (!file) {
        logger::log(logger, logger::Level::ERROR, logger_id, "Unable to create '{}'", path);

        return 1;
    }

    Header hdr;

    hdr.magic = MAGIC;
    hdr.version = 1;
    hdr.block_count = block_count;
    hdr.block_size = block_size;
    hdr.block_mode = block_mode;
    hdr.block_compression = block_compression;
    hdr.bat_block_count = 0;
    hdr.extension_offset = 0;
    hdr.reserved = 0;

    fwrite(&hdr, sizeof(Header), 1, file);

    hdr.bat_offset = ftell64(file);

    // Write empty BAT
    int bat_entry_size = (block_mode & 1) ? 8 : 4;

    uint64_t bat_size = block_count * bat_entry_size;


    uint64_t* empty_bat = (uint64_t *)calloc(block_count, bat_entry_size);
    fwrite(empty_bat, bat_entry_size, block_count, file);
    free(empty_bat);

    if (ext && ext_size > 0) {
        hdr.extension_offset = ftell64(file);

        fwrite(ext, 1, ext_size, file);
    }

    hdr.data_offset = ftell64(file);

    // Update header
    fseek64(file, 0, SEEK_SET);
    fwrite(&hdr, sizeof(Header), 1, file);

    fclose(file);

    return 0;
}

void ata_read_sector(void* udata, uint64_t lba, uint8_t* buf) {
    Isif* isif = (Isif*)udata;

    read_block(isif, lba, buf);
}

void ata_write_sector(void* udata, uint64_t lba, const uint8_t* buf) {
    Isif* isif = (Isif*)udata;

    write_block(isif, lba, buf);
}

int ata_get_identify(void* udata, uint8_t* buf) {
    Isif* isif = (Isif*)udata;

    if (isif->hdr.extension_offset) {
        read_extension(isif, buf);

        return 1;
    }

    memset(buf, 0, 512);

    return 0;
}

uint64_t ata_get_sector_count(void* udata) {
    Isif* isif = (Isif*)udata;

    return isif->hdr.block_count;
}

void ata_close(void* udata) {
    Isif* isif = (Isif*)udata;

    close(isif);
}

}
