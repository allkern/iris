#include <cstdio>
#include <cstring>

#include "fs/ps1mcd.hpp"

namespace iris::fs::ps1mcd {

inline constexpr uint32_t FRAME_SIZE = 128;
inline constexpr uint32_t BLOCK_SIZE = 8192;
inline constexpr uint32_t BLOCK_COUNT = 16;
inline constexpr uint32_t CARD_SIZE = BLOCK_SIZE * BLOCK_COUNT;
inline constexpr uint32_t NAME_LENGTH = 20;

// Block 0 frame N describes data block N
enum : uint32_t {
    STATE_FIRST = 0x51,
    STATE_MIDDLE = 0x52,
    STATE_LAST = 0x53,
    STATE_FREE = 0xa0,
    STATE_DELETED_FIRST = 0xa1,
    STATE_DELETED_MIDDLE = 0xa2,
    STATE_DELETED_LAST = 0xa3
};

struct Ps1Mcd {
    blk::Device* dev = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

struct Slot {
    uint32_t state = STATE_FREE;
    uint32_t size = 0;
    uint16_t next = 0xffff;
    char name[NAME_LENGTH + 1] = {};
};

struct FileHandle {
    std::vector<uint32_t> blocks;
    uint64_t size = 0;
};

static uint32_t rd32(const uint8_t* p, uint32_t off) {
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) | ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
}

static uint16_t rd16(const uint8_t* p, uint32_t off) {
    return (uint16_t)(p[off] | (p[off + 1] << 8));
}

static int read_frame(Ps1Mcd* card, uint32_t block, uint32_t frame, uint8_t* out) {
    uint64_t offset = (uint64_t)block * BLOCK_SIZE + (uint64_t)frame * FRAME_SIZE;

    return blk::read(card->dev, offset, out, FRAME_SIZE) < 0 ? FS_ERR_IO : FS_OK;
}

static int read_slot(Ps1Mcd* card, uint32_t block, Slot* out) {
    uint8_t frame[FRAME_SIZE];

    int r = read_frame(card, 0, block, frame);

    if (r < 0)
        return r;

    out->state = rd32(frame, 0x00);
    out->size = rd32(frame, 0x04);
    out->next = rd16(frame, 0x08);

    memcpy(out->name, frame + 0x0a, NAME_LENGTH);

    out->name[NAME_LENGTH] = '\0';

    return FS_OK;
}

static bool is_first(uint32_t state) {
    return state == STATE_FIRST || state == STATE_DELETED_FIRST;
}

static bool is_deleted(uint32_t state) {
    return state >= STATE_DELETED_FIRST && state <= STATE_DELETED_LAST;
}

static int collect_blocks(Ps1Mcd* card, uint32_t first, std::vector<uint32_t>* out) {
    out->clear();

    uint32_t block = first;

    for (uint32_t step = 0; step < BLOCK_COUNT; step++) {
        out->push_back(block);

        Slot slot;

        int r = read_slot(card, block, &slot);

        if (r < 0)
            return r;

        if (slot.next == 0xffff)
            return FS_OK;

        uint32_t next = (uint32_t)slot.next + 1;

        if (next < 1 || next >= BLOCK_COUNT)
            return FS_ERR_CORRUPT;

        for (uint32_t seen : *out) {
            if (seen == next)
                return FS_ERR_CORRUPT;
        }

        block = next;
    }

    return FS_ERR_CORRUPT;
}

static int find_save(Ps1Mcd* card, const char* path, uint32_t* block, Slot* out) {
    std::vector<std::string> parts = path_split(path);

    if (parts.size() != 1)
        return parts.empty() ? FS_ERR_IS_DIRECTORY : FS_ERR_NOT_FOUND;

    for (uint32_t i = 1; i < BLOCK_COUNT; i++) {
        Slot slot;

        int r = read_slot(card, i, &slot);

        if (r < 0)
            return r;

        if (!is_first(slot.state))
            continue;

        if (parts[0] == slot.name) {
            *block = i;
            *out = slot;

            return FS_OK;
        }
    }

    return FS_ERR_NOT_FOUND;
}

static uint64_t clamp_size(const Slot& slot, size_t blocks) {
    uint64_t limit = (uint64_t)blocks * BLOCK_SIZE;

    return slot.size && slot.size <= limit ? slot.size : limit;
}

static int card_list(void* udata, const char* path, std::vector<Entry>* out) {
    Ps1Mcd* card = (Ps1Mcd*)udata;

    if (!path_split(path).empty())
        return FS_ERR_NOT_DIRECTORY;

    for (uint32_t i = 1; i < BLOCK_COUNT; i++) {
        Slot slot;

        int r = read_slot(card, i, &slot);

        if (r < 0)
            return r;

        if (!is_first(slot.state) || !slot.name[0])
            continue;

        std::vector<uint32_t> blocks;

        if (collect_blocks(card, i, &blocks) < 0)
            blocks.assign(1, i);

        Entry entry;

        snprintf(entry.name, sizeof(entry.name), "%s", slot.name);

        entry.size = clamp_size(slot, blocks.size());
        entry.cookie = i;

        if (is_deleted(slot.state))
            entry.flags |= ENTRY_DELETED;

        out->push_back(entry);
    }

    return FS_OK;
}

static int card_stat(void* udata, const char* path, Entry* out) {
    Ps1Mcd* card = (Ps1Mcd*)udata;

    *out = Entry();

    if (path_split(path).empty()) {
        out->flags = ENTRY_DIRECTORY;

        snprintf(out->name, sizeof(out->name), "/");

        return FS_OK;
    }

    uint32_t block;
    Slot slot;

    int r = find_save(card, path, &block, &slot);

    if (r < 0)
        return r;

    std::vector<uint32_t> blocks;

    if (collect_blocks(card, block, &blocks) < 0)
        blocks.assign(1, block);

    snprintf(out->name, sizeof(out->name), "%s", slot.name);

    out->size = clamp_size(slot, blocks.size());
    out->cookie = block;

    if (is_deleted(slot.state))
        out->flags |= ENTRY_DELETED;

    return FS_OK;
}

static int card_open(void* udata, const char* path, Handle** out) {
    Ps1Mcd* card = (Ps1Mcd*)udata;

    uint32_t block;
    Slot slot;

    int r = find_save(card, path, &block, &slot);

    if (r < 0)
        return r;

    FileHandle* h = new FileHandle();

    r = collect_blocks(card, block, &h->blocks);

    if (r < 0) {
        delete h;

        return r;
    }

    h->size = clamp_size(slot, h->blocks.size());

    *out = (Handle*)h;

    return FS_OK;
}

static int64_t card_read(void* udata, Handle* handle, uint64_t offset, void* buf, uint64_t size) {
    Ps1Mcd* card = (Ps1Mcd*)udata;
    FileHandle* h = (FileHandle*)handle;

    if (offset >= h->size)
        return 0;

    if (size > h->size - offset)
        size = h->size - offset;

    uint8_t* out = (uint8_t*)buf;
    uint64_t left = size;

    while (left) {
        uint64_t index = offset / BLOCK_SIZE;
        uint32_t in_block = offset % BLOCK_SIZE;
        uint64_t chunk = BLOCK_SIZE - in_block;

        if (chunk > left)
            chunk = left;

        if (index >= h->blocks.size())
            return -1;

        uint64_t at = (uint64_t)h->blocks[index] * BLOCK_SIZE + in_block;

        if (blk::read(card->dev, at, out, chunk) < 0)
            return -1;

        offset += chunk;
        out += chunk;
        left -= chunk;
    }

    return (int64_t)size;
}

static void card_close_handle(void*, Handle* handle) {
    delete (FileHandle*)handle;
}

static void card_close(void* udata) {
    delete (Ps1Mcd*)udata;
}

Fs* open(logger::Logger* logger, blk::Device* dev, bool take_ownership) {
    if (blk::get_size(dev) != CARD_SIZE)
        return nullptr;

    uint8_t header[FRAME_SIZE];

    if (blk::read(dev, 0, header, sizeof(header)) < 0)
        return nullptr;

    if (header[0] != 'M' || header[1] != 'C')
        return nullptr;

    Ps1Mcd* card = new Ps1Mcd();

    card->dev = dev;
    card->logger = logger;
    card->logger_id = get_logger_id(logger);

    uint64_t used = 0;

    for (uint32_t i = 1; i < BLOCK_COUNT; i++) {
        Slot slot;

        if (read_slot(card, i, &slot) < 0)
            break;

        if (slot.state >= STATE_FIRST && slot.state <= STATE_LAST)
            used += BLOCK_SIZE;
    }

    Fs* fs = new Fs();

    fs->list = card_list;
    fs->stat = card_stat;
    fs->open = card_open;
    fs->read = card_read;
    fs->close_handle = card_close_handle;
    fs->close = card_close;
    fs->udata = card;
    fs->dev = dev;
    fs->owns_dev = take_ownership;
    fs->type = FS_PS1_MCD;
    fs->total_bytes = (uint64_t)(BLOCK_COUNT - 1) * BLOCK_SIZE;
    fs->free_bytes = fs->total_bytes - used;
    fs->logger = logger;
    fs->logger_id = card->logger_id;

    snprintf(fs->label, sizeof(fs->label), "PS1 memory card");

    return fs;
}

static uint32_t sjis_codepoint(uint16_t c) {
    if (c >= 0x20 && c <= 0x7e)
        return c == 0x5c ? 0x00a5 : c;

    if (c >= 0xa1 && c <= 0xdf)
        return 0xff61 + (c - 0xa1);

    if (c == 0x8140) return 0x3000;
    if (c == 0x8141) return 0x3001;
    if (c == 0x8142) return 0x3002;
    if (c == 0x815b) return 0x30fc;
    if (c == 0x8160) return 0x301c;

    // uppercase and lowercase
    if (c >= 0x824f && c <= 0x8258) return 0xff10 + (c - 0x824f);
    if (c >= 0x8260 && c <= 0x8279) return 0xff21 + (c - 0x8260);
    if (c >= 0x8281 && c <= 0x829a) return 0xff41 + (c - 0x8281);

    // Hiragana/katakana
    if (c >= 0x829f && c <= 0x82f1) return 0x3041 + (c - 0x829f);
    if (c >= 0x8340 && c <= 0x837e) return 0x30a1 + (c - 0x8340);
    if (c >= 0x8380 && c <= 0x8396) return 0x30e0 + (c - 0x8380);

    return '?';
}

static void sjis_to_utf8(const uint8_t* in, size_t size, char* out, size_t out_size) {
    size_t o = 0;

    for (size_t i = 0; i < size && o + 4 < out_size; i++) {
        uint16_t c = in[i];

        if (!c)
            break;

        bool lead = (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xef);

        if (lead && i + 1 < size) {
            c = (uint16_t)((c << 8) | in[++i]);
        } else if (lead) {
            break;
        }

        uint32_t cp = sjis_codepoint(c);

        if (cp < 0x80) {
            out[o++] = (char)cp;
        } else if (cp < 0x800) {
            out[o++] = (char)(0xc0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3f));
        } else {
            out[o++] = (char)(0xe0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[o++] = (char)(0x80 | (cp & 0x3f));
        }
    }

    while (o && out[o - 1] == ' ')
        o--;

    out[o] = '\0';
}

int get_save_info(Fs* fs, const char* path, SaveInfo* out) {
    if (!fs || fs->type != FS_PS1_MCD)
        return FS_ERR_UNSUPPORTED;

    Ps1Mcd* card = (Ps1Mcd*)fs->udata;

    *out = SaveInfo();

    uint32_t block;
    Slot slot;

    int r = find_save(card, path, &block, &slot);

    if (r < 0)
        return r;

    uint8_t frame[FRAME_SIZE];

    r = read_frame(card, block, 0, frame);

    if (r < 0)
        return r;

    if (frame[0] != 'S' || frame[1] != 'C')
        return FS_ERR_CORRUPT;

    sjis_to_utf8(frame + 0x04, 64, out->title, sizeof(out->title));

    // 0x11, 0x12 and 0x13 mean one, two and three animation frames
    uint8_t display = frame[0x02];

    out->icon_frames = display >= 0x11 && display <= 0x13 ? display - 0x10 : 0;

    uint16_t clut[16];

    for (int i = 0; i < 16; i++)
        clut[i] = rd16(frame, 0x60 + i * 2);

    for (int f = 0; f < out->icon_frames; f++) {
        uint8_t bitmap[FRAME_SIZE];

        if (read_frame(card, block, 1 + f, bitmap) < 0) {
            out->icon_frames = f;

            break;
        }

        for (int p = 0; p < 16 * 16; p++) {
            uint8_t index = (p & 1) ? (bitmap[p / 2] >> 4) : (bitmap[p / 2] & 0x0f);
            uint16_t colour = clut[index];

            uint8_t* px = out->icon[f] + p * 4;

            px[0] = (uint8_t)((colour & 0x1f) << 3);
            px[1] = (uint8_t)(((colour >> 5) & 0x1f) << 3);
            px[2] = (uint8_t)(((colour >> 10) & 0x1f) << 3);
            px[3] = (index || (colour & 0x8000)) ? 0xff : 0x00;
        }
    }

    return FS_OK;
}

}
