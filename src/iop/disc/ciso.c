#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ciso.h"

#define CISO_DEFLATE 0
#define CISO_LZ4 1

int get_compression_type(const char* path) {
    char* ext = strrchr(path, '.');

    if (!ext) {
        return CISO_DEFLATE;
    }

    if (tolower(ext[1]) == 'c') {
        return CISO_DEFLATE;
    } else if (tolower(ext[1]) == 'z') {
        return CISO_LZ4;
    }

    return CISO_DEFLATE;
}

int realloc_comp_buf(struct disc_ciso* ciso, size_t size) {
    if (size > ciso->comp_buf_cap) {
        uint8_t* new_buf = realloc(ciso->comp_buf, size);

        if (!new_buf) {
            return 0;
        }

        ciso->comp_buf = new_buf;
        ciso->comp_buf_cap = size;
    }

    return 1;
}

struct disc_ciso* ciso_create(void) {
    return malloc(sizeof(struct disc_ciso));
}

int ciso_init(struct disc_ciso* ciso, const char* path) {
    memset(ciso, 0, sizeof(struct disc_ciso));

    ciso->file = fopen(path, "rb");

    if (!ciso->file) {
        free(ciso);

        return 0;
    }

    fread(&ciso->header, sizeof(struct ciso_header), 1, ciso->file);

    if (ciso->header.magic != 0x4f534943 && ciso->header.magic != 0x4f53495a) {
        fprintf(stderr, "ciso: Invalid CISO/ZISO magic\n");

        ciso_destroy(ciso);

        return 0;
    }

    if (ciso->header.version > 1) {
        fprintf(stderr, "ciso: Unsupported CISO version %u\n", ciso->header.version);

        ciso_destroy(ciso);

        return 0;
    }

    if (ciso->header.block_size != 2048) {
        fprintf(stderr, "ciso: Unsupported CISO header size %u\n", ciso->header.header_size);

        ciso_destroy(ciso);

        return 0;
    }

    ciso->compression_type = get_compression_type(path);

    ciso->decompressor = libdeflate_alloc_decompressor();

    if (!ciso->decompressor) {
        fprintf(stderr, "ciso: Failed to allocate decompressor\n");

        ciso_destroy(ciso);

        return 0;
    }

    ciso->index_count = (ciso->header.uncompressed_size / (uint64_t)ciso->header.block_size) + 1;
    ciso->index_table = malloc(ciso->index_count * sizeof(uint32_t));

    if (!ciso->index_table) {
        fprintf(stderr, "ciso: Failed to allocate index table\n");

        ciso_destroy(ciso);

        return 0;
    }

    fread(ciso->index_table, ciso->index_count * sizeof(uint32_t), 1, ciso->file);

    return 1;
}

void ciso_destroy(struct disc_ciso* ciso) {
    if (ciso->file) fclose(ciso->file);
    if (ciso->decompressor) libdeflate_free_decompressor(ciso->decompressor);
    if (ciso->index_table) free(ciso->index_table);
    if (ciso->comp_buf) free(ciso->comp_buf);

    free(ciso);
}

// Disc IF
int ciso_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    struct disc_ciso* ciso = (struct disc_ciso*)udata;

    if (lba * 2048 >= ciso->header.uncompressed_size)
        return 0;

    size = 2048;

    uint32_t index_entry = ciso->index_table[lba];
    uint32_t next_index_entry = ciso->index_table[lba + 1];

    uint32_t offset = index_entry & 0x7fffffff;
    uint32_t next_offset = next_index_entry & 0x7fffffff;

    uint32_t buf_size = next_offset - offset;

    int is_compressed = !(index_entry & 0x80000000);

    fseek(ciso->file, offset, SEEK_SET);

    if (is_compressed) {
        realloc_comp_buf(ciso, buf_size);

        fseek(ciso->file, offset, SEEK_SET);
        fread(ciso->comp_buf, 1, buf_size, ciso->file);

        size_t actual_decomp_size = 0;

        if (ciso->compression_type == CISO_LZ4) {
            actual_decomp_size = LZ4_decompress_safe(
                (const char*)ciso->comp_buf, (char*)buf,
                buf_size, size
            );
        } else {
            libdeflate_deflate_decompress(
                ciso->decompressor,
                ciso->comp_buf, buf_size,
                buf, size,
                &actual_decomp_size
            );
        }

        if (actual_decomp_size != 2048) {
            fprintf(stderr, "ciso: Decompression failed for sector %llu\n", lba);

            return 0;
        }

        return 1;
    }

    if (buf_size != 2048) {
        fprintf(stderr, "ciso: Invalid uncompressed sector size %u for sector %llu\n", buf_size, lba);

        return 0;
    }

    fread(buf, 1, buf_size, ciso->file);

    return 1;
}

uint64_t ciso_get_size(void* udata) {
    struct disc_ciso* ciso = (struct disc_ciso*)udata;

    return ciso->header.uncompressed_size;
}

int ciso_get_sector_size(void* udata) {
    return 2048;
}

int ciso_get_track_count(void* udata) {
    return 1;
}

int ciso_get_track_info(void* udata, int track, struct track_info* info) {
    return 0;
}

int ciso_get_track_number(void* udata, uint64_t lba) {
    return 1;
}