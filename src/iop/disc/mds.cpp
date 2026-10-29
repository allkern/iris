/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <algorithm>
#include <cstring>
#include <string>

#include "mds.hpp"

namespace iris::iop::disc::mds {

static constexpr const char* MDS_SIGNATURE = "MEDIA DESCRIPTOR";
static constexpr size_t MDS_SIGNATURE_SIZE = 16;
static constexpr uint8_t MDS_SUPPORTED_VERSION = 1;

static constexpr uint64_t MDS_HEADER_SIZE = 0x58;
static constexpr uint64_t MDS_SESSION_SIZE = 0x18;
static constexpr uint64_t MDS_TRACK_SIZE = 0x50;
static constexpr uint64_t MDS_EXTRA_SIZE = 0x08;
static constexpr uint64_t MDS_FOOTER_SIZE = 0x10;
static constexpr uint64_t MDS_DVD_COPYRIGHT_SIZE = 4;
static constexpr uint64_t MDS_DVD_STRUCTURE_SIZE = 2048;

static constexpr uint16_t MDS_MEDIUM_CD = 0x00;
static constexpr uint16_t MDS_MEDIUM_CD_R = 0x01;
static constexpr uint16_t MDS_MEDIUM_CD_RW = 0x02;
static constexpr uint16_t MDS_MEDIUM_DVD = 0x10;
static constexpr uint16_t MDS_MEDIUM_DVD_R = 0x12;

static constexpr uint8_t MDS_MODE_AUDIO = 0x01;
static constexpr uint8_t MDS_MODE_MODE1 = 0x02;

static constexpr uint8_t MDS_SUBCHANNEL_PW_INTERLEAVED = 0x08;
static constexpr int MDS_SUBCHANNEL_SIZE = 96;

static constexpr int CD_RAW_SECTOR_SIZE = 2352;
static constexpr int CD_MODE2_FORMLESS_SECTOR_SIZE = 2336;
static constexpr int CD_DATA_SECTOR_SIZE = 2048;
static constexpr int CD_LEAD_IN_SECTORS = 150;

static bool in_bounds(const std::vector <uint8_t>& data, uint64_t offset, uint64_t size) {
    if (offset > data.size())
        return false;

    return size <= data.size() - offset;
}

static uint16_t read_u16(const uint8_t* ptr) {
    uint16_t value;

    memcpy(&value, ptr, sizeof(value));

    return value;
}

static uint32_t read_u32(const uint8_t* ptr) {
    uint32_t value;

    memcpy(&value, ptr, sizeof(value));

    return value;
}

static uint64_t read_u64(const uint8_t* ptr) {
    uint64_t value;

    memcpy(&value, ptr, sizeof(value));

    return value;
}

static uint32_t read_u24_be(const uint8_t* ptr) {
    return (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
}

static bool read_file(const std::string& path, std::vector <uint8_t>& data) {
    FILE* file = fopen(path.c_str(), "rb");

    if (!file)
        return false;

    fseek64(file, 0, SEEK_END);

    int64_t size = ftell64(file);

    fseek64(file, 0, SEEK_SET);

    if (size <= 0) {
        fclose(file);

        return false;
    }

    data.resize(size);

    size_t read = fread(data.data(), 1, data.size(), file);

    fclose(file);

    return read == data.size();
}

static std::string get_descriptor_path(const char* path) {
    std::string descriptor(path);

    if (get_extension(path) != DISC_EXT_MDF)
        return descriptor;

    if (descriptor.back() == 'F') {
        descriptor.back() = 'S';
    } else {
        descriptor.back() = 's';
    }

    return descriptor;
}

static size_t find_name_start(const std::string& path) {
    size_t slash = path.find_last_of("/\\");

    if (slash == std::string::npos)
        return 0;

    return slash + 1;
}

static std::string resolve_data_path(const std::string& descriptor_path, const std::string& name) {
    size_t name_start = find_name_start(descriptor_path);

    bool is_wildcard = (name.size() > 1) && (name[0] == '*') && (name[1] == '.');

    if (!is_wildcard) {
        return descriptor_path.substr(0, name_start) + name;
    }

    std::string extension = name.substr(1);
    size_t dot = descriptor_path.find_last_of('.');

    if ((dot == std::string::npos) || (dot < name_start)) {
        return descriptor_path + extension;
    }

    return descriptor_path.substr(0, dot) + extension;
}

static void append_utf8(std::string& out, uint32_t codepoint) {
    if (codepoint < 0x80) {
        out += (char)codepoint;
    } else if (codepoint < 0x800) {
        out += (char)(0xc0 | (codepoint >> 6));
        out += (char)(0x80 | (codepoint & 0x3f));
    } else if (codepoint < 0x10000) {
        out += (char)(0xe0 | (codepoint >> 12));
        out += (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out += (char)(0x80 | (codepoint & 0x3f));
    } else {
        out += (char)(0xf0 | (codepoint >> 18));
        out += (char)(0x80 | ((codepoint >> 12) & 0x3f));
        out += (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out += (char)(0x80 | (codepoint & 0x3f));
    }
}

static bool read_narrow_filename(const std::vector <uint8_t>& data, uint64_t offset, std::string& name) {
    while (in_bounds(data, offset, 1)) {
        char c = (char)data[offset];

        if (!c)
            return true;

        name += c;
        offset += 1;
    }

    return false;
}

static bool read_wide_filename(const std::vector <uint8_t>& data, uint64_t offset, std::string& name) {
    while (in_bounds(data, offset, 2)) {
        uint32_t codepoint = read_u16(&data[offset]);

        offset += 2;

        if (!codepoint)
            return true;

        bool is_high_surrogate = (codepoint >= 0xd800) && (codepoint <= 0xdbff);

        if (is_high_surrogate && in_bounds(data, offset, 2)) {
            uint32_t low = read_u16(&data[offset]);

            if ((low >= 0xdc00) && (low <= 0xdfff)) {
                codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);

                offset += 2;
            }
        }

        append_utf8(name, codepoint);
    }

    return false;
}

static int get_track_type(uint8_t mode) {
    uint8_t kind = mode & 0x07;

    if (kind == MDS_MODE_AUDIO)
        return CDVD_TRACK_AUDIO;

    if (kind == MDS_MODE_MODE1)
        return CDVD_TRACK_MODE1;

    return CDVD_TRACK_MODE2;
}

static int get_data_offset(int type, int sector_size) {
    if (sector_size == CD_RAW_SECTOR_SIZE) {
        if (type == CDVD_TRACK_MODE1)
            return 0x10;

        if (type == CDVD_TRACK_MODE2)
            return 0x18;

        return 0;
    }

    if ((sector_size == CD_MODE2_FORMLESS_SECTOR_SIZE) && (type == CDVD_TRACK_MODE2))
        return 0x08;

    return 0;
}

static void parse_layer_break(Mds* mds, const std::vector <uint8_t>& data, uint32_t structures_offset) {
    if (!structures_offset)
        return;

    uint64_t physical_offset = structures_offset + MDS_DVD_COPYRIGHT_SIZE + MDS_DVD_STRUCTURE_SIZE;

    if (!in_bounds(data, physical_offset, MDS_DVD_STRUCTURE_SIZE)) {
        iris_warning(mds, "DVD structures are out of bounds, ignoring them");

        return;
    }

    const uint8_t* physical = &data[physical_offset];

    int layers = ((physical[2] >> 5) & 3) + 1;

    if (layers != 2)
        return;

    uint32_t start = read_u24_be(physical + 5);
    uint32_t layer0_end = read_u24_be(physical + 13);

    if (layer0_end < start) {
        iris_warning(mds, "Invalid layer 0 end sector {:06x} (start {:06x}), ignoring it", layer0_end, start);

        return;
    }

    mds->layer_break = layer0_end - start + 1;
}

static bool parse_track(Mds* mds, const std::vector <uint8_t>& data, const uint8_t* block, const std::string& descriptor_path, std::string& data_path) {
    Track track;

    track.number = block[4];
    track.type = get_track_type(block[0]);
    track.stride = read_u16(block + 0x10);
    track.sector_size = track.stride;
    track.start = read_u32(block + 0x24);
    track.offset = read_u64(block + 0x28);
    track.pregap = 0;
    track.length = 0;

    if (block[1] == MDS_SUBCHANNEL_PW_INTERLEAVED) {
        track.sector_size -= MDS_SUBCHANNEL_SIZE;
    }

    if ((track.sector_size < CD_DATA_SECTOR_SIZE) || (track.sector_size > CD_RAW_SECTOR_SIZE)) {
        iris_error(mds, "Track {} has an unsupported sector size of {} bytes", track.number, track.stride);

        return false;
    }

    track.data_offset = get_data_offset(track.type, track.sector_size);

    uint32_t extra_offset = read_u32(block + 0x0c);

    if ((mds->media == DISC_MEDIA_CD) && extra_offset) {
        if (!in_bounds(data, extra_offset, MDS_EXTRA_SIZE)) {
            iris_error(mds, "Track {} extra block is out of bounds", track.number);

            return false;
        }

        track.pregap = read_u32(&data[extra_offset]);
        track.length = read_u32(&data[extra_offset + 4]);
    }

    uint32_t file_count = read_u32(block + 0x30);
    uint32_t footer_offset = read_u32(block + 0x34);

    if (file_count != 1) {
        iris_error(mds, "Track {} is split across {} data files, which is not supported", track.number, file_count);

        return false;
    }

    if (!in_bounds(data, footer_offset, MDS_FOOTER_SIZE)) {
        iris_error(mds, "Track {} footer is out of bounds", track.number);

        return false;
    }

    uint32_t filename_offset = read_u32(&data[footer_offset]);
    bool is_widechar = read_u32(&data[footer_offset + 4]) != 0;

    std::string filename;
    bool filename_ok;

    if (is_widechar) {
        filename_ok = read_wide_filename(data, filename_offset, filename);
    } else {
        filename_ok = read_narrow_filename(data, filename_offset, filename);
    }

    if (!filename_ok || filename.empty()) {
        iris_error(mds, "Track {} has an invalid data file name", track.number);

        return false;
    }

    std::string path = resolve_data_path(descriptor_path, filename);

    if (data_path.empty()) {
        data_path = path;
    } else if (data_path != path) {
        iris_error(mds, "Track {} uses a different data file ('{}'), which is not supported", track.number, path);

        return false;
    }

    mds->tracks.push_back(track);

    return true;
}

static bool parse_session(Mds* mds, const std::vector <uint8_t>& data, uint64_t session_offset, const std::string& descriptor_path, std::string& data_path) {
    if (!in_bounds(data, session_offset, MDS_SESSION_SIZE)) {
        iris_error(mds, "Session block is out of bounds");

        return false;
    }

    const uint8_t* session = &data[session_offset];

    int block_count = session[0x0a];
    uint32_t blocks_offset = read_u32(session + 0x14);

    for (int i = 0; i < block_count; i++) {
        uint64_t block_offset = blocks_offset + (i * MDS_TRACK_SIZE);

        if (!in_bounds(data, block_offset, MDS_TRACK_SIZE)) {
            iris_error(mds, "Track block {} is out of bounds", i);

            return false;
        }

        const uint8_t* block = &data[block_offset];

        int point = block[4];

        if ((point < 1) || (point > 99))
            continue;

        if (!parse_track(mds, data, block, descriptor_path, data_path))
            return false;
    }

    return true;
}

static bool compute_track_lengths(Mds* mds, uint64_t file_size) {
    std::sort(mds->tracks.begin(), mds->tracks.end(), [](const Track& a, const Track& b) {
        return a.start < b.start;
    });

    for (size_t i = 0; i < mds->tracks.size(); i++) {
        Track& track = mds->tracks[i];

        if (track.offset > file_size) {
            iris_error(mds, "Track {} starts past the end of the data file", track.number);

            return false;
        }

        uint64_t available = (file_size - track.offset) / track.stride;

        if (!track.length) {
            bool is_last = (i + 1) == mds->tracks.size();

            if (is_last) {
                track.length = available;
            } else {
                const Track& next = mds->tracks[i + 1];

                int64_t gap = (int64_t)next.start - next.pregap - track.start;

                track.length = (gap > 0) ? gap : available;
            }
        }

        if (track.length > available) {
            iris_warning(mds, "Track {} is truncated, only {} of {} sectors are present", track.number, available, track.length);

            track.length = available;
        }
    }

    return true;
}

static bool open_data_file(Mds* mds, const std::string& data_path) {
    mds->file = fopen(data_path.c_str(), "rb");

    if (!mds->file) {
        iris_error(mds, "Couldn't open data file '{}'", data_path);

        return false;
    }

    fseek64(mds->file, 0, SEEK_END);

    int64_t file_size = ftell64(mds->file);

    if (file_size < 0) {
        iris_error(mds, "Couldn't get the size of data file '{}'", data_path);

        return false;
    }

    return compute_track_lengths(mds, file_size);
}

static bool parse(Mds* mds, const std::vector <uint8_t>& data, const std::string& descriptor_path) {
    if (!in_bounds(data, 0, MDS_HEADER_SIZE)) {
        iris_error(mds, "MDS descriptor is too small");

        return false;
    }

    const uint8_t* header = data.data();

    if (memcmp(header, MDS_SIGNATURE, MDS_SIGNATURE_SIZE)) {
        iris_error(mds, "Invalid MDS signature");

        return false;
    }

    if (header[0x10] != MDS_SUPPORTED_VERSION) {
        iris_error(mds, "Unsupported MDS version {}.{}, only Alcohol 120% (v1) descriptors are supported", header[0x10], header[0x11]);

        return false;
    }

    uint16_t medium = read_u16(header + 0x12);

    switch (medium) {
        case MDS_MEDIUM_CD:
        case MDS_MEDIUM_CD_R:
        case MDS_MEDIUM_CD_RW: {
            mds->media = DISC_MEDIA_CD;
        } break;

        case MDS_MEDIUM_DVD:
        case MDS_MEDIUM_DVD_R: {
            mds->media = DISC_MEDIA_DVD;
        } break;

        default: {
            iris_error(mds, "Unsupported MDS medium type {:02x}", medium);

            return false;
        }
    }

    if (mds->media == DISC_MEDIA_DVD) {
        parse_layer_break(mds, data, read_u32(header + 0x40));
    }

    uint16_t session_count = read_u16(header + 0x14);
    uint32_t sessions_offset = read_u32(header + 0x50);

    std::string data_path;

    for (int i = 0; i < session_count; i++) {
        uint64_t session_offset = sessions_offset + (i * MDS_SESSION_SIZE);

        if (!parse_session(mds, data, session_offset, descriptor_path, data_path))
            return false;
    }

    if (mds->tracks.empty()) {
        iris_error(mds, "MDS descriptor contains no tracks");

        return false;
    }

    if (!open_data_file(mds, data_path))
        return false;

    for (const Track& track : mds->tracks) {
        iris_debug(mds, "Track {}: type={:02x} start={} length={} pregap={} offset={:x} stride={} sector_size={}",
            track.number,
            track.type,
            track.start,
            track.length,
            track.pregap,
            track.offset,
            track.stride,
            track.sector_size
        );
    }

    return true;
}

static const Track* find_track(Mds* mds, uint64_t lba) {
    for (const Track& track : mds->tracks) {
        uint64_t end = (uint64_t)track.start + track.length;

        if ((lba >= track.start) && (lba < end)) {
            return &track;
        }
    }

    return nullptr;
}

static uint64_t get_end_lba(Mds* mds) {
    const Track& last = mds->tracks.back();

    return (uint64_t)last.start + last.length;
}

Mds* create(logger::Logger* logger) {
    Mds* mds = new Mds();

    mds->logger = logger;
    mds->logger_id = logger::register_source(logger, "mds");

    return mds;
}

int init(Mds* mds, const char* path) {
    std::string descriptor_path = get_descriptor_path(path);
    std::vector <uint8_t> data;

    if (!read_file(descriptor_path, data)) {
        iris_error(mds, "Couldn't read MDS descriptor '{}'", descriptor_path);

        destroy(mds);

        return 0;
    }

    if (!parse(mds, data, descriptor_path)) {
        destroy(mds);

        return 0;
    }

    return 1;
}

void destroy(Mds* mds) {
    if (mds->file) {
        fclose(mds->file);
    }

    delete mds;
}

int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    Mds* mds = (Mds*)udata;

    const Track* track = find_track(mds, lba);

    if (!track) {
        bool is_data_size = (size == DISC_SS_DATA) || (mds->media == DISC_MEDIA_DVD);

        memset(buf, 0, is_data_size ? CD_DATA_SECTOR_SIZE : CD_RAW_SECTOR_SIZE);

        if (lba < get_end_lba(mds))
            return 1;

        return 0;
    }

    unsigned char sector[CD_RAW_SECTOR_SIZE];

    uint64_t position = track->offset + ((lba - track->start) * track->stride);

    if (fseek64(mds->file, position, SEEK_SET))
        return 0;

    if (fread(sector, 1, track->sector_size, mds->file) != (size_t)track->sector_size)
        return 0;

    if (size == DISC_SS_DATA) {
        memcpy(buf, sector + track->data_offset, CD_DATA_SECTOR_SIZE);
    } else {
        memcpy(buf, sector, track->sector_size);
    }

    return 1;
}

uint64_t get_size(void* udata) {
    Mds* mds = (Mds*)udata;

    uint64_t size = 0;

    for (const Track& track : mds->tracks) {
        size += (uint64_t)track.length * track.sector_size;
    }

    return size;
}

int get_sector_size(void* udata) {
    Mds* mds = (Mds*)udata;

    return mds->tracks.front().sector_size;
}

int get_track_count(void* udata) {
    Mds* mds = (Mds*)udata;

    return mds->tracks.size();
}

int get_track_info(void* udata, int track, TrackInfo* info) {
    Mds* mds = (Mds*)udata;

    if ((track < 1) || ((size_t)track > mds->tracks.size()))
        return 0;

    const Track& data = mds->tracks[track - 1];

    info->number = data.number;
    info->type = data.type;
    info->lba = data.start + CD_LEAD_IN_SECTORS;

    return 1;
}

int get_track_number(void* udata, uint64_t lba) {
    Mds* mds = (Mds*)udata;

    int index = 0;

    for (size_t i = 0; i < mds->tracks.size(); i++) {
        const Track& track = mds->tracks[i];

        int64_t track_begin = (int64_t)track.start - track.pregap;

        if ((int64_t)lba >= track_begin) {
            index = i;
        }
    }

    return index + 1;
}

#undef fseek64
#undef ftell64

}
