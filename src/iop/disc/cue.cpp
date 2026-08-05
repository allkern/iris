#include <cctype>

#include "cue.hpp"
#include "../disc.hpp"

namespace iris::iop::disc::cue {

static const char* keywords[] = {
    "CUE_4CH",
    "CUE_AIFF",
    "CUE_AUDIO",
    "CUE_BINARY",
    "CUE_CATALOG",
    "CUE_CDG",
    "CDI/2336",
    "CDI/2352",
    "CUE_CDTEXTFILE",
    "CUE_DCP",
    "CUE_FILE",
    "CUE_FLAGS",
    "CUE_INDEX",
    "CUE_ISRC",
    "MODE1/2048",
    "MODE1/2352",
    "MODE2/2336",
    "MODE2/2352",
    "CUE_MOTOROLA",
    "CUE_MP3",
    "CUE_PERFORMER",
    "CUE_POSTGAP",
    "CUE_PRE",
    "CUE_PREGAP",
    "CUE_REM",
    "CUE_SCMS",
    "CUE_SONGWRITER",
    "CUE_TITLE",
    "CUE_TRACK",
    "CUE_WAVE",
    0
};

char* strapp(char* dst, const char* a, const char* b) {
    char* d = dst;

    while (*a)
        *dst++ = *a++;

    while (*b)
        *dst++ = *b++;

    *dst = '\0';

    return d;
}

const char* find_last_slash(const char* a) {
    if (!a)
        return NULL;

    const char* b = a;

    while (*a) {
        if (*a == '/' || *a == '\\')
            b = a + 1;

        ++a;
    }

    return b;
}

char* get_root_path(char* dst, const char* a) {
    if (!a) {
        *dst = '\0';

        return dst;
    }

    const char* b = a;
    const char* c = a;
    char* d = dst;

    while (*a) {
        if (*a == '/' || *a == '\\')
            b = a + 1;

        ++a;
    }

    while (c != b)
        *dst++ = *c++;

    *dst = '\0';

    return d;
}

int parse_keyword(Cue* cue) {
    char buf[256];
    char* ptr = buf;

    while (isalpha(cue->c) || isdigit(cue->c) || cue->c == '/') {
        *ptr++ = cue->c;

        cue->c = fgetc(cue->file);
    }

    *ptr = '\0';

    int i = 0;

    const char* keyword = keywords[i];

    while (keyword) {
        if (!strcmp(keyword, buf)) {
            return i;
        } else {
            keyword = keywords[++i];
        }
    }

    return -1;
}

int parse_number(Cue* cue) {
    if (!isdigit(cue->c))
        return 0;

    char buf[4];

    char* ptr = buf;

    while (isdigit(cue->c)) {
        *ptr++ = cue->c;

        cue->c = fgetc(cue->file);
    }

    *ptr = '\0';

    return atoi(buf);
}

uint64_t parse_msf(Cue* cue) {
    int m = 0;
    int s = 0;
    int f = 0;

    if (!isdigit(cue->c))
        return 0;

    m = parse_number(cue);

    if (cue->c != ':')
        return 0;

    cue->c = fgetc(cue->file);

    s = parse_number(cue);

    if (cue->c != ':')
        return 0;

    cue->c = fgetc(cue->file);

    f = parse_number(cue);

    // 1 second = 75 frames (sectors)
    // 1 minute = 60 seconds = 4500 frames
    return f + (s * 75) + (m * 4500);
}

void parse_index(Cue* cue) {
    Track* track = cue->tracks.back();

    while (isspace(cue->c))
        cue->c = fgetc(cue->file);

    if (!isdigit(cue->c))
        return;

    int i = parse_number(cue);

    while (isspace(cue->c))
        cue->c = fgetc(cue->file);

    if (i > 1)
        return;

    track->index[i] = parse_msf(cue);
}

Track* parse_track(Cue* cue) {
    while (isspace(cue->c))
        cue->c = fgetc(cue->file);

    if (!isdigit(cue->c))
        return NULL;

    Track* track = new Track();

    track->end = 0;
    track->start = 0;
    track->pregap = 0;
    track->index[0] = -1;
    track->index[1] = -1;
    track->file = cue->files.back();
    track->number = parse_number(cue);

    while (isspace(cue->c))
        cue->c = fgetc(cue->file);

    track->mode = parse_keyword(cue);

    return track;
}

File* parse_file(Cue* cue, const char* p, const char* s) {
    while (isspace(cue->c))
        cue->c = fgetc(cue->file);

    if (cue->c != '\"')
        return NULL;

    File* file = new File();

    file->name = new char[512];

    // Append root path to track file path
    char* ptr = file->name;

    while (p != s)
        *ptr++ = *p++;

    cue->c = fgetc(cue->file);

    while (cue->c != '\"') {
        *ptr++ = cue->c;

        cue->c = fgetc(cue->file);
    }

    *ptr = '\0';

    cue->c = fgetc(cue->file);

    // Ignore file type
    while (isspace(cue->c))
        cue->c = fgetc(cue->file);

    while (isalpha(cue->c))
        cue->c = fgetc(cue->file);

    return file;
}

Cue* create(logger::Logger* logger) {
    Cue* cue = new Cue();

    cue->logger = logger;
    cue->logger_id = logger::register_source(logger, "cue");

    return cue;
}

int parse(Cue* cue, const char* path) {
    cue->file = fopen(path, "rb");

    if (!cue->file)
        return CUE_FILE_NOT_FOUND;

    const char* s = find_last_slash(path);

    cue->c = fgetc(cue->file);

    while (isspace(cue->c))
        cue->c = fgetc(cue->file);

    while (!feof(cue->file)) {
        int kw = parse_keyword(cue);

        switch (kw) {
            case CUE_FILE: {
                cue->files.push_back(parse_file(cue, path, s));
            } break;

            case CUE_TRACK: {
                Track* track = parse_track(cue);
                File* file = cue->files.back();

                cue->tracks.push_back(track);
                file->tracks.push_back(track);
            } break;

            case CUE_INDEX: {
                parse_index(cue);
            } break;

            case CUE_REM: case CUE_PREGAP: case CUE_FLAGS: case CUE_POSTGAP: {
                // Ignore everything until a newline (handle CRLF and LF)
                while ((cue->c != '\n') && (cue->c != '\r'))
                    cue->c = fgetc(cue->file);

                while ((cue->c == '\n') && (cue->c == '\r'))
                    cue->c = fgetc(cue->file);
            } break;

            default: {
                iris_debug(cue, "Unknown keyword: {} ({})", keywords[kw], kw);

                return 1;
            } break;
        }

        while (isspace(cue->c))
            cue->c = fgetc(cue->file);
    }

    return 0;
}

size_t get_file_size(FILE* file) {
    fseek64(file, 0, SEEK_END);

    size_t size = ftell64(file);

    fseek64(file, 0, SEEK_SET);

    return size;
}

/*
(0   - 0  )                   = 150
(0   - 0  ) + 150    + 315000 = 315150
(0   - 0  ) + 315150 + 375    = 315525
(150 - 0  ) + 315525 + 390    = 316065
(195 - 150) + 316065 + 390    = 316500
(155 - 190) + 316500 + 390    = 
*/

int prev_pregap = 0;

int init_tracks(File* file, uint64_t* lba) {
    // 1 track per file case
    if (file->tracks.size() == 1) {
        Track* data = file->tracks.front();

        data->pregap = 0;

        if ((data->index[0] != -1) && (data->index[1] != -1))
            data->pregap = data->index[1];

        data->start = *lba + data->pregap;
        data->end = data->start + (file->size / 0x930);

        *lba = data->end;

        return 0;
    }

    // Multiple tracks per file
    for (size_t i = 0; i < file->tracks.size(); i++) {
        Track* data = file->tracks[i];

        // If this is the last track
        if (i + 1 == file->tracks.size()) {
            data->pregap = 0;
            data->start = data->index[1] + 150;
            data->end = file->size / 0x930;

            return 0;
        }

        Track* next = file->tracks[i + 1];

        data->start = data->index[1] + 150;
        data->end = (next->index[1] + 150) - 1;
        data->pregap = 0;
    }

    return 0;
}

int load(Cue* cue, int mode) {
    // 00:02:00
    uint64_t lba = 2 * 75;

    for (File* data : cue->files) {

        FILE* file = fopen(data->name, "rb");

        if (!file)
            return CUE_TRACK_FILE_NOT_FOUND;

        data->buf_mode = mode;
        data->size = get_file_size(file);

        // iris_debug(cue, "Loaded \'{}\': size={:x}, sectors={}", //     data->name,
        //     data->size,
        //     data->size / 0x930
        //);

        if (data->buf_mode == LD_BUFFERED) {
            data->buf = malloc(data->size);

            fseek64(file, 0, SEEK_SET);

            if (!fread(data->buf, 1, data->size, file))
                return CUE_TRACK_READ_ERROR;

            fclose(file);
        } else {
            data->buf = file;
        }

        data->start = lba;

        init_tracks(data, &lba);
    }

    return CUE_OK;
}

void destroy(Cue* cue) {
    for (File* file : cue->files) {
        if (file->buf_mode == LD_BUFFERED) {
            free(file->buf);
        } else {
            fclose((FILE*)file->buf);
        }

        delete[] file->name;
        delete file;
    }

    // Tracks are owned here; File::tracks only borrows them
    for (Track* track : cue->tracks)
        delete track;

    delete cue;
}

Track* get_sector_track(Cue* cue, uint64_t lba) {
    for (Track* track : cue->tracks) {
        if ((lba >= track->start) && (lba < track->end))
            return track;
    }

    return nullptr;
}

Track* get_sector_track_in_pregap(Cue* cue, uint64_t lba) {
    for (size_t i = 0; i < cue->tracks.size(); i++) {
        Track* track = cue->tracks[i];

        if (i + 1 == cue->tracks.size())
            return track;

        Track* next = cue->tracks[i + 1];

        // Ignore sector number
        int curr_start = track->start - (track->start % 75);
        int next_start = next->start - (next->start % 75);

        if ((lba >= curr_start) && (lba < next_start))
            return track;
    }

    return nullptr;
}

int query(Cue* cue, uint64_t lba) {
    if (lba >= cue->tracks.back()->end)
        return TS_FAR;

    Track* track = get_sector_track(cue, lba);

    // If the LBA isn't too far but the track wasn't found
    // then we are being requested a pregap sector. Clear buffer
    // and initialize sync data (not actually needed)
    if (!track)
        return TS_PREGAP;

    return (track->mode != CUE_AUDIO) ? TS_DATA : TS_AUDIO;
}

int read(Cue* cue, uint64_t lba, void* buf, int* sector_size) {
    if (lba >= cue->tracks.back()->end)
        return TS_FAR;

    Track* track = get_sector_track(cue, lba);

    *sector_size = 2352;

    switch (track->mode) {
        case CUE_MODE1_2048: *sector_size = 2048; break;
        case CUE_MODE2_2336: *sector_size = 2336; break;
        case CUE_MODE2_2352: *sector_size = 2352; break;
    }

    // If the LBA isn't too far but the track wasn't found
    // then we are being requested a pregap sector. Clear buffer
    // and initialize sync data (not actually needed)
    if (!track) {
        memset((uint8_t*)buf, 0, *sector_size);
        memset((uint8_t*)buf + 1, 255, 10);

        return TS_PREGAP;
    }

    File* file = track->file;

    // iris_debug(cue, "Reading sector {} at track {}, file={} ({}), offset={} ({:08x})", //     lba,
    //     track->number,
    //     track->file->name,
    //     file->start,
    //     lba - file->start,
    //     (lba - file->start) * 2352
    //);

    if (file->buf_mode == LD_BUFFERED) {
        uint8_t* ptr = (uint8_t*)file->buf + ((lba - file->start) * (*sector_size));

        memcpy(buf, ptr, *sector_size);
    } else {
        fseek64((FILE*)file->buf, (lba - file->start) * (*sector_size), SEEK_SET);

        // Should always succeed, ignore result for speed
        (void)fread(buf, 1, *sector_size, (FILE*)file->buf);
    }

    return (track->mode != CUE_AUDIO) ? TS_DATA : TS_AUDIO;
}

int get_track_number_impl(Cue* cue, uint64_t lba) {
    Track* track = get_sector_track_in_pregap(cue, lba);

    return track->number;
}

int get_track_count_impl(Cue* cue) {
    return cue->tracks.size();
}

int get_track_lba(Cue* cue, int track) {
    if (!track)
        return cue->tracks.back()->end;

    if ((size_t)track > cue->tracks.size())
        return TS_FAR;

    Track* data = cue->tracks[track - 1];

    return data->start;
}

int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    Cue* cue = (Cue*)udata;

    // Adjust for lead-in
    lba += 150;

    char temp[2352];

    int sector_size;

    if (read(cue, lba, temp, &sector_size) == TS_FAR) {
        if (size == DISC_SS_DATA) {
            memset(buf, 0, 2048);
        } else {
            memset(buf, 0, 2352);
        }

        return 0;
    }

    if (size == DISC_SS_DATA) {
        if (sector_size == 2048) {
            memcpy(buf, temp, 2048);
        } else {
            memcpy(buf, temp + 0x18, 2048);
        }
    } else {
        memcpy(buf, temp, sector_size);
    }

    return 1;
}

uint64_t get_size(void* udata) {
    Cue* cue = (Cue*)udata;

    unsigned int size = 0;

    for (File* file : cue->files)
        size += file->size;

    return size;
}

int get_sector_size(void* udata) {
    return 2352;
}

int init(Cue* cue, const char* path) {
    if (parse(cue, path) != CUE_OK) {
        iris_debug(cue, "Failed to parse CUE file '{}'", path);

        return 0;
    }

    if (load(cue, LD_FILE) != CUE_OK) {
        iris_debug(cue, "Failed to load CUE file '{}'", path);

        return 0;
    }

    return 1;
}

int get_track_count(void* udata) {
    Cue* cue = (Cue*)udata;

    return get_track_count_impl(cue);
}

int get_track_info(void* udata, int track, TrackInfo* info) {
    Cue* cue = (Cue*)udata;

    if ((size_t)track > cue->tracks.size())
        return 0;

    Track* data = cue->tracks[track - 1];

    info->number = data->number;
    info->type = (data->mode != CUE_AUDIO) ? TS_DATA : TS_AUDIO;
    info->lba = data->start;

    return 1;
}

int get_track_number(void* udata, uint64_t lba) {
    Cue* cue = (Cue*)udata;

    return get_track_number_impl(cue, lba + 150);
}

}
