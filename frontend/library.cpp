#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#include "iris.hpp"
#include "net.hpp"

#include "iop/disc.hpp"

namespace iris::library {

namespace fs = std::filesystem;

static constexpr const char* GAMEDB_URL = "https://github.com/niemasd/GameDB-PS2/releases/latest/download/PS2.data.tsv";
static constexpr const char* COVERS_URL = "https://raw.githubusercontent.com/xlenore/ps2-covers/refs/heads/main/covers/default/";

static constexpr int WORKER_COUNT = 6;
static constexpr int UPLOADS_PER_FRAME = 3;
static constexpr int TEXTURE_CAP = 160;
static constexpr int TEXTURE_EVICT_TARGET = 128;
static constexpr int COVER_HEIGHT = 400;
static constexpr int URGENT_CAP = 256;
static constexpr int BATCH_SIZE = 64;
static constexpr uint64_t STALE_REQUEST_FRAMES = 90;
static constexpr int64_t MISSING_RETRY_SECONDS = 7 * 24 * 60 * 60;
static constexpr int64_t GAMEDB_REFRESH_SECONDS = 30 * 24 * 60 * 60;
static constexpr double CACHE_SAVE_DELAY = 2.0;
static constexpr double FADE_SECONDS = 0.15;
static constexpr int CACHE_VERSION = 1;

enum : int {
    COVER_NONE,
    COVER_QUEUED,
    COVER_LOADED,
    COVER_MISSING,
    COVER_FAILED
};

enum : int {
    OUTCOME_CACHED,
    OUTCOME_SAVED,
    OUTCOME_MISSING,
    OUTCOME_UNAVAILABLE,
    OUTCOME_FAILED
};

struct Record {
    uint64_t size = 0;
    int64_t mtime = 0;
    int kind = KIND_IGNORED;
    std::string serial;
    std::string name;
    std::string format;
    std::string system;
    std::string subdir;
    int64_t last_played = 0;
    uint32_t seen = 0;
};

struct DbEntry {
    std::string title;
    std::string region;
    std::string developer;
    std::string release_date;
};

using Db = std::unordered_map <std::string, DbEntry>;

struct Cover {
    int state = COVER_NONE;
    Texture texture = {};
    uint64_t last_drawn = 0;
    double loaded_at = 0.0;
};

struct Job {
    std::string key;
    bool load = false;
    bool background = false;
    bool allow_download = false;
    uint64_t frame = 0;
};

struct Pixels {
    int width = 0;
    int height = 0;
    std::vector <uint8_t> data;
};

struct Result {
    std::string key;
    bool wanted = false;
    bool load = false;
    bool background = false;
    int outcome = OUTCOME_FAILED;
    Pixels pixels;
};

struct ScanBatch {
    uint32_t id = 0;
    bool done = false;
    std::vector <std::pair <std::string, Record>> records;
    std::vector <std::string> unavailable;
};

struct LogMessage {
    logger::Level level;
    std::string text;
};

struct State {
    Instance* iris = nullptr;

    fs::path root;
    fs::path covers_dir;
    fs::path cache_path;
    fs::path db_path;

    std::unordered_map <std::string, Record> records;
    std::unordered_map <std::string, int64_t> missing;
    std::unordered_map <std::string, Cover> covers;
    std::unordered_set <std::string> background_queued;
    std::deque <std::pair <std::string, Pixels>> uploads;
    Db db;

    std::vector <Entry> entries;
    uint64_t generation = 0;
    bool entries_dirty = true;
    bool cache_dirty = false;
    double cache_dirty_since = 0.0;

    std::vector <std::string> scanned_dirs;
    bool scanned_recursive = true;
    bool downloads = true;

    int covers_total = 0;
    int covers_done = 0;
    int covers_saved = 0;
    int covers_missing = 0;

    std::mutex mutex;
    std::condition_variable cv;
    std::unordered_map <std::string, Job> jobs;
    std::unordered_map <std::string, Job> deferred;
    std::unordered_set <std::string> in_flight;
    std::deque <std::string> urgent;
    std::deque <std::string> background;
    std::deque <Result> results;
    std::deque <ScanBatch> scan_inbox;
    std::vector <LogMessage> logs;
    std::unique_ptr <Db> db_ready;
    bool db_wanted = false;
    bool db_allow_download = false;
    bool stop = false;

    std::atomic <uint64_t> frame = 0;
    std::atomic <bool> scanning = false;
    std::atomic <bool> scan_stop = false;
    std::atomic <bool> fetching_db = false;
    uint32_t scan_id = 0;

    std::vector <std::thread> workers;
    std::thread scanner;
};

static double now_seconds() {
    using namespace std::chrono;

    return duration <double> (steady_clock::now().time_since_epoch()).count();
}

static int64_t unix_time() {
    return (int64_t)std::time(nullptr);
}

static std::string lowercase(std::string text) {
    for (char& c : text) {
        c = (char)tolower((unsigned char)c);
    }

    return text;
}

static std::string uppercase(std::string text) {
    for (char& c : text) {
        c = (char)toupper((unsigned char)c);
    }

    return text;
}

static std::string normalize_path(const std::string& path) {
    return fs::path(path).lexically_normal().string();
}

static int64_t file_mtime(const fs::path& path) {
    std::error_code ec;

    auto time = fs::last_write_time(path, ec);

    if (ec) {
        return 0;
    }

    return (int64_t)time.time_since_epoch().count();
}

static bool read_file(const fs::path& path, std::string* out) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return false;
    }

    std::ostringstream stream;

    stream << file.rdbuf();

    *out = stream.str();

    return true;
}

static bool write_file_atomic(const fs::path& path, const std::string& data) {
    fs::path tmp = path;

    tmp += ".tmp" + std::to_string(std::hash <std::thread::id> {}(std::this_thread::get_id()));

    {
        std::ofstream file(tmp, std::ios::binary | std::ios::trunc);

        if (!file) {
            return false;
        }

        file.write(data.data(), (std::streamsize)data.size());

        if (!file) {
            return false;
        }
    }

    std::error_code ec;

    fs::rename(tmp, path, ec);

    if (ec) {
        fs::remove(tmp, ec);

        return false;
    }

    return true;
}

static void post_log(State* s, logger::Level level, std::string text) {
    std::lock_guard <std::mutex> lock(s->mutex);

    s->logs.push_back({ level, std::move(text) });
}

static std::string region_from_serial(const std::string& serial) {
    if (serial.size() < 4) {
        return "";
    }

    std::string prefix = serial.substr(0, 4);

    static const std::unordered_map <std::string, const char*> regions = {
        { "SLUS", "NTSC-U" }, { "SCUS", "NTSC-U" }, { "LAUS", "NTSC-U" },
        { "SLES", "PAL" }, { "SCES", "PAL" }, { "SCED", "PAL" },
        { "SLPM", "NTSC-J" }, { "SLPS", "NTSC-J" }, { "SCPS", "NTSC-J" },
        { "SCAJ", "NTSC-J" }, { "SLAJ", "NTSC-J" }, { "PAPX", "NTSC-J" },
        { "PCPX", "NTSC-J" }, { "SCPM", "NTSC-J" }, { "SLKA", "NTSC-K" },
        { "SCKA", "NTSC-K" }
    };

    auto it = regions.find(prefix);

    if (it == regions.end()) {
        return "";
    }

    return it->second;
}

static bool is_downloadable_serial(const std::string& serial) {
    if (serial.size() != 10 || serial[4] != '-') {
        return false;
    }

    for (size_t i = 0; i < serial.size(); i++) {
        if (i == 4) {
            continue;
        }

        if (!isalnum((unsigned char)serial[i])) {
            return false;
        }
    }

    return true;
}

static std::string cover_key(int kind, const std::string& serial) {
    if (serial.empty()) {
        return "";
    }

    if (kind == KIND_ARCADE) {
        return "arcade/" + serial;
    }

    return serial;
}

static bool is_arcade_key(const std::string& key) {
    return key.starts_with("arcade/");
}

static fs::path find_cover_file(const State* s, const std::string& key) {
    static const char* const extensions[] = { ".png", ".jpg", ".jpeg" };

    std::error_code ec;

    for (const char* ext : extensions) {
        fs::path path = s->covers_dir / (key + ext);

        if (fs::is_regular_file(path, ec)) {
            return path;
        }
    }

    return {};
}

static bool decode_cover(const fs::path& path, Pixels* out) {
    std::string data;

    if (!read_file(path, &data)) {
        return false;
    }

    int width, height, channels;

    stbi_uc* pixels = stbi_load_from_memory((const stbi_uc*)data.data(), (int)data.size(), &width, &height, &channels, 4);

    if (!pixels) {
        return false;
    }

    if (height > COVER_HEIGHT) {
        int scaled_width = std::max(1, (int)((double)width * COVER_HEIGHT / height + 0.5));

        out->width = scaled_width;
        out->height = COVER_HEIGHT;
        out->data.resize((size_t)scaled_width * COVER_HEIGHT * 4);

        stbir_resize_uint8_srgb(
            pixels, width, height, 0,
            out->data.data(), scaled_width, COVER_HEIGHT, 0,
            STBIR_RGBA
        );
    } else {
        out->width = width;
        out->height = height;
        out->data.assign(pixels, pixels + (size_t)width * height * 4);
    }

    stbi_image_free(pixels);

    return true;
}

static std::vector <std::string> split_tabs(const std::string& line) {
    std::vector <std::string> fields;

    size_t start = 0;

    while (true) {
        size_t end = line.find('\t', start);

        if (end == std::string::npos) {
            fields.push_back(line.substr(start));

            break;
        }

        fields.push_back(line.substr(start, end - start));

        start = end + 1;
    }

    return fields;
}

static bool parse_db(const std::string& text, Db* out) {
    if (!text.starts_with("ID\ttitle")) {
        return false;
    }

    std::istringstream stream(text);
    std::string line;

    std::getline(stream, line);

    std::vector <std::string> header = split_tabs(line);

    auto column = [&](const char* name) {
        for (size_t i = 0; i < header.size(); i++) {
            std::string field = header[i];

            if (field.size() && field.back() == '\r') {
                field.pop_back();
            }

            if (field == name) {
                return (int)i;
            }
        }

        return -1;
    };

    int id_col = column("ID");
    int title_col = column("title");
    int developer_col = column("developer");
    int region_col = column("region");
    int date_col = column("release_date");
    int serial_col = column("serial");

    if (id_col < 0 || title_col < 0) {
        return false;
    }

    auto field = [](const std::vector <std::string>& fields, int col) {
        if (col < 0 || col >= (int)fields.size()) {
            return std::string();
        }

        std::string value = fields[col];

        if (value.size() && value.back() == '\r') {
            value.pop_back();
        }

        if (value == "N/A") {
            return std::string();
        }

        return value;
    };

    while (std::getline(stream, line)) {
        std::vector <std::string> fields = split_tabs(line);

        DbEntry entry;

        entry.title = field(fields, title_col);
        entry.developer = field(fields, developer_col);
        entry.region = field(fields, region_col);
        entry.release_date = field(fields, date_col);

        if (entry.title.empty()) {
            continue;
        }

        std::string id = field(fields, id_col);
        std::string serial = field(fields, serial_col);

        if (serial.size() && serial != id && !out->contains(serial)) {
            (*out)[serial] = entry;
        }

        if (id.size()) {
            (*out)[id] = std::move(entry);
        }
    }

    return out->size() != 0;
}

static void run_db_fetch(State* s, bool allow_download, net::Session** session) {
    s->fetching_db = true;

    std::error_code ec;

    bool exists = fs::is_regular_file(s->db_path, ec);
    bool stale = !exists;

    if (exists) {
        auto age = fs::file_time_type::clock::now() - fs::last_write_time(s->db_path, ec);

        stale = std::chrono::duration_cast <std::chrono::seconds> (age).count() > GAMEDB_REFRESH_SECONDS;
    }

    std::unique_ptr <Db> db = std::make_unique <Db> ();

    if (stale && allow_download) {
        if (!*session) {
            *session = net::open_session();
        }

        net::DownloadResult result = net::download(*session, GAMEDB_URL);

        if (result.status == 200 && parse_db(result.body, db.get())) {
            write_file_atomic(s->db_path, result.body);

            post_log(s, logger::LEVEL_INFO, "Downloaded game database (" + std::to_string(db->size()) + " titles)");
        } else {
            db->clear();

            post_log(s, logger::LEVEL_WARNING, "Couldn't download game database (HTTP " + std::to_string(result.status) + ")");
        }
    }

    if (db->empty() && exists) {
        std::string text;

        if (!read_file(s->db_path, &text) || !parse_db(text, db.get())) {
            db->clear();

            post_log(s, logger::LEVEL_WARNING, "Couldn't parse cached game database");
        }
    }

    {
        std::lock_guard <std::mutex> lock(s->mutex);

        if (db->size()) {
            s->db_ready = std::move(db);
        }
    }

    s->fetching_db = false;
}

static Result run_job(State* s, const Job& job, net::Session** session) {
    Result result;

    result.key = job.key;
    result.wanted = job.load;
    result.background = job.background;
    result.load = job.load && (s->frame.load() - job.frame) <= STALE_REQUEST_FRAMES;

    fs::path path = find_cover_file(s, job.key);

    if (path.empty()) {
        if (is_arcade_key(job.key) || !job.allow_download || !is_downloadable_serial(job.key)) {
            result.outcome = OUTCOME_UNAVAILABLE;
            result.load = false;

            return result;
        }

        if (!*session) {
            *session = net::open_session();
        }

        net::DownloadResult download = net::download(*session, COVERS_URL + job.key + ".jpg");

        if (download.status == 404) {
            result.outcome = OUTCOME_MISSING;
            result.load = false;

            return result;
        }

        int width, height, channels;

        bool valid = download.status == 200 && stbi_info_from_memory(
            (const stbi_uc*)download.body.data(), (int)download.body.size(),
            &width, &height, &channels
        );

        if (!valid) {
            result.outcome = OUTCOME_FAILED;
            result.load = false;

            return result;
        }

        path = s->covers_dir / (job.key + ".jpg");

        if (!write_file_atomic(path, download.body)) {
            result.outcome = OUTCOME_FAILED;
            result.load = false;

            return result;
        }

        result.outcome = OUTCOME_SAVED;
    } else {
        result.outcome = OUTCOME_CACHED;
    }

    if (result.load && !decode_cover(path, &result.pixels)) {
        std::error_code ec;

        if (path.extension() == ".jpg" && !is_arcade_key(job.key)) {
            fs::remove(path, ec);
        }

        result.outcome = OUTCOME_FAILED;
    }

    return result;
}

static void add_job_locked(State* s, Job job, bool urgent) {
    if (s->in_flight.contains(job.key)) {
        auto [it, inserted] = s->deferred.try_emplace(job.key, job);

        if (!inserted) {
            it->second.load |= job.load;
            it->second.background |= job.background;
            it->second.allow_download |= job.allow_download;
            it->second.frame = std::max(it->second.frame, job.frame);
        }

        return;
    }

    auto [it, inserted] = s->jobs.try_emplace(job.key, job);

    if (inserted) {
        s->background.push_back(job.key);
    } else {
        it->second.load |= job.load;
        it->second.background |= job.background;
        it->second.allow_download |= job.allow_download;
        it->second.frame = std::max(it->second.frame, job.frame);
    }

    if (urgent) {
        s->urgent.push_back(job.key);

        if (s->urgent.size() > URGENT_CAP) {
            s->urgent.pop_front();
        }
    }

    s->cv.notify_one();
}

static void add_job(State* s, Job job, bool urgent) {
    std::lock_guard <std::mutex> lock(s->mutex);

    add_job_locked(s, std::move(job), urgent);
}

static bool take_job_locked(State* s, Job* out) {
    auto take = [&](const std::string& key) {
        auto it = s->jobs.find(key);

        if (it == s->jobs.end()) {
            return false;
        }

        *out = std::move(it->second);

        s->jobs.erase(it);
        s->in_flight.insert(out->key);

        return true;
    };

    while (s->urgent.size()) {
        std::string key = std::move(s->urgent.back());

        s->urgent.pop_back();

        if (take(key)) {
            return true;
        }
    }

    while (s->background.size()) {
        std::string key = std::move(s->background.front());

        s->background.pop_front();

        if (take(key)) {
            return true;
        }
    }

    return false;
}

static void worker_main(State* s) {
    net::Session* session = nullptr;

    while (true) {
        Job job;

        bool fetch_db = false;
        bool allow_download = false;

        {
            std::unique_lock <std::mutex> lock(s->mutex);

            s->cv.wait(lock, [s] {
                return s->stop || s->db_wanted || s->jobs.size();
            });

            if (s->stop) {
                break;
            }

            if (s->db_wanted) {
                s->db_wanted = false;

                fetch_db = true;
                allow_download = s->db_allow_download;
            } else if (!take_job_locked(s, &job)) {
                continue;
            }
        }

        if (fetch_db) {
            run_db_fetch(s, allow_download, &session);

            continue;
        }

        Result result = run_job(s, job, &session);

        std::lock_guard <std::mutex> lock(s->mutex);

        s->in_flight.erase(job.key);

        auto it = s->deferred.find(job.key);

        if (it != s->deferred.end()) {
            Job next = std::move(it->second);

            s->deferred.erase(it);

            bool urgent = next.load;

            add_job_locked(s, std::move(next), urgent);
        }

        s->results.push_back(std::move(result));
    }

    net::close_session(session);
}

struct Scanner {
    State* s;
    uint32_t id;
    std::unordered_map <std::string, Record> known;
    std::vector <std::pair <std::string, Record>> batch;
    std::chrono::steady_clock::time_point last_flush = std::chrono::steady_clock::now();
    size_t found = 0;
};

static void flush(Scanner* scanner, bool done, std::vector <std::string> unavailable = {}) {
    ScanBatch batch;

    batch.id = scanner->id;
    batch.done = done;
    batch.records = std::move(scanner->batch);
    batch.unavailable = std::move(unavailable);

    scanner->batch.clear();
    scanner->last_flush = std::chrono::steady_clock::now();

    std::lock_guard <std::mutex> lock(scanner->s->mutex);

    scanner->s->scan_inbox.push_back(std::move(batch));
}

static void emit(Scanner* scanner, const std::string& path, Record record) {
    record.seen = scanner->id;

    if (record.kind != KIND_IGNORED) {
        scanner->found++;
    }

    scanner->batch.emplace_back(path, std::move(record));

    auto elapsed = std::chrono::steady_clock::now() - scanner->last_flush;

    if (scanner->batch.size() >= BATCH_SIZE || elapsed > std::chrono::milliseconds(100)) {
        flush(scanner, false);
    }
}

static bool reuse(Scanner* scanner, const std::string& path, uint64_t size, int64_t mtime, Record* out) {
    auto it = scanner->known.find(path);

    if (it == scanner->known.end()) {
        return false;
    }

    if (it->second.size != size || it->second.mtime != mtime) {
        return false;
    }

    *out = it->second;

    return true;
}

static bool is_disc_extension(const std::string& ext) {
    return ext == ".iso" || ext == ".bin" || ext == ".cue" ||
           ext == ".chd" || ext == ".cso" || ext == ".zso";
}

static Record probe_disc(const fs::path& path) {
    Record record;

    record.name = path.stem().string();
    record.format = uppercase(path.extension().string().substr(1));

    iop::disc::Disc* disc = iop::disc::open(nullptr, path.string().c_str());

    if (!disc) {
        return record;
    }

    switch (iop::disc::get_type(disc)) {
        case iop::disc::CDVD_DISC_PS2_CD:
        case iop::disc::CDVD_DISC_PS2_CDDA: {
            record.kind = KIND_PS2_CD;
            record.system = "PlayStation 2 CD";
        } break;

        case iop::disc::CDVD_DISC_PS2_DVD: {
            record.kind = KIND_PS2_DVD;
            record.system = "PlayStation 2 DVD";
        } break;
    }

    if (record.kind != KIND_IGNORED) {
        char serial[64] = {};

        if (iop::disc::get_serial(disc, serial)) {
            record.serial = patches::get_serial(serial);
        }
    }

    iop::disc::close(disc);

    return record;
}

static Record probe_arcade(const fs::path& path, bool directory) {
    Record record;

    record.name = directory ? path.filename().string() : path.stem().string();

    std::optional <ArcadeInfo> info = emu::describe_arcade(path.string());

    if (!info) {
        return record;
    }

    record.kind = KIND_ARCADE;
    record.serial = info->id;
    record.name = info->name;
    record.system = emu::get_system_name(nullptr, info->system);
    record.subdir = info->subdir.size() ? normalize_path(info->subdir) : "";
    record.format = directory ? "Folder" : uppercase(path.extension().string().substr(1));

    return record;
}

static bool is_arcade_directory_candidate(const std::vector <fs::path>& files) {
    if (files.empty() || files.size() > 48) {
        return false;
    }

    for (const fs::path& file : files) {
        std::string ext = lowercase(file.extension().string());

        if (ext == ".iso" || ext == ".cso" || ext == ".zso" || ext == ".cue") {
            return false;
        }
    }

    return true;
}

static void walk(Scanner* scanner, const fs::path& dir, bool recursive) {
    std::vector <fs::path> files;
    std::vector <fs::path> subdirs;

    std::error_code ec;

    auto options = fs::directory_options::skip_permission_denied;

    for (const fs::directory_entry& entry : fs::directory_iterator(dir, options, ec)) {
        if (scanner->s->scan_stop) {
            return;
        }

        std::error_code entry_ec;

        if (entry.is_directory(entry_ec)) {
            subdirs.push_back(entry.path());
        } else if (entry.is_regular_file(entry_ec)) {
            files.push_back(entry.path());
        }
    }

    if (is_arcade_directory_candidate(files)) {
        std::string key = normalize_path(dir.string());
        int64_t mtime = file_mtime(dir);

        Record record;

        if (!reuse(scanner, key, 0, mtime, &record)) {
            record = probe_arcade(dir, true);
            record.mtime = mtime;
        }

        bool arcade = record.kind == KIND_ARCADE;

        emit(scanner, key, std::move(record));

        if (arcade) {
            return;
        }
    }

    std::unordered_set <std::string> cue_stems;
    std::unordered_set <std::string> skip_dirs;

    for (const fs::path& file : files) {
        if (lowercase(file.extension().string()) == ".cue") {
            cue_stems.insert(lowercase(file.stem().string()));
        }
    }

    for (const fs::path& file : files) {
        if (scanner->s->scan_stop) {
            return;
        }

        std::string ext = lowercase(file.extension().string());

        bool disc = is_disc_extension(ext);
        bool arcade = ext == ".acgame" || ext == ".zip";

        if (!disc && !arcade) {
            continue;
        }

        if (ext == ".bin" && cue_stems.contains(lowercase(file.stem().string()))) {
            continue;
        }

        std::string key = normalize_path(file.string());

        std::error_code size_ec;

        uint64_t size = fs::file_size(file, size_ec);
        int64_t mtime = file_mtime(file);

        Record record;

        if (!reuse(scanner, key, size, mtime, &record)) {
            record = arcade ? probe_arcade(file, false) : probe_disc(file);
            record.size = size;
            record.mtime = mtime;
        }

        if (record.subdir.size()) {
            skip_dirs.insert(record.subdir);
        }

        emit(scanner, key, std::move(record));
    }

    if (!recursive) {
        return;
    }

    for (const fs::path& subdir : subdirs) {
        if (scanner->s->scan_stop) {
            return;
        }

        if (skip_dirs.contains(normalize_path(subdir.string()))) {
            continue;
        }

        walk(scanner, subdir, true);
    }
}

static void scan_main(State* s, uint32_t id, std::vector <std::string> dirs, bool recursive, std::unordered_map <std::string, Record> known) {
    Scanner scanner = { s, id, std::move(known) };

    auto start = std::chrono::steady_clock::now();

    std::vector <std::string> unavailable;

    for (const std::string& dir : dirs) {
        fs::path root = fs::path(dir).lexically_normal();

        if (!root.has_filename() && root.has_relative_path()) {
            root = root.parent_path();
        }

        std::error_code ec;

        if (!fs::is_directory(root, ec)) {
            unavailable.push_back(root.string());

            continue;
        }

        walk(&scanner, root, recursive);

        if (s->scan_stop) {
            break;
        }
    }

    if (!s->scan_stop) {
        auto elapsed = std::chrono::duration_cast <std::chrono::milliseconds> (std::chrono::steady_clock::now() - start);

        post_log(s, logger::LEVEL_INFO, "Found " + std::to_string(scanner.found) + " games in " + std::to_string(elapsed.count()) + " ms");

        flush(&scanner, true, std::move(unavailable));
    }

    s->scanning = false;
}

static void stop_scan(State* s) {
    if (!s->scanner.joinable()) {
        return;
    }

    s->scan_stop = true;
    s->scanner.join();
    s->scan_stop = false;
    s->scanning = false;
}

static void start_scan(State* s) {
    stop_scan(s);

    Instance* iris = s->iris;

    s->scanned_dirs = iris->library.dirs;
    s->scanned_recursive = iris->library.recursive;
    s->scan_id++;
    s->scanning = true;

    {
        std::lock_guard <std::mutex> lock(s->mutex);

        s->scan_inbox.clear();
    }

    s->scanner = std::thread(scan_main, s, s->scan_id, s->scanned_dirs, s->scanned_recursive, s->records);
}

static void load_cache(State* s) {
    std::string text;

    if (!read_file(s->cache_path, &text)) {
        return;
    }

    toml::parse_result result = toml::parse(text);

    if (!result) {
        iris_warning(&s->iris->log.library, "Couldn't parse library cache, rebuilding it");

        return;
    }

    toml::table table = std::move(result).table();

    if (table["version"].value_or(0) != CACHE_VERSION) {
        return;
    }

    if (toml::table* games = table["games"].as_table()) {
        for (auto&& [key, node] : *games) {
            toml::table* game = node.as_table();

            if (!game) {
                continue;
            }

            Record record;

            record.size = (uint64_t)(*game)["size"].value_or<int64_t>(0);
            record.mtime = (*game)["mtime"].value_or<int64_t>(0);
            record.kind = (int)(*game)["kind"].value_or<int64_t>(KIND_IGNORED);
            record.serial = (*game)["serial"].value_or("");
            record.name = (*game)["name"].value_or("");
            record.format = (*game)["format"].value_or("");
            record.system = (*game)["system"].value_or("");
            record.subdir = (*game)["subdir"].value_or("");
            record.last_played = (*game)["last_played"].value_or<int64_t>(0);

            s->records[std::string(key.str())] = std::move(record);
        }
    }

    if (toml::table* missing = table["missing"].as_table()) {
        for (auto&& [key, node] : *missing) {
            s->missing[std::string(key.str())] = node.value_or<int64_t>(0);
        }
    }
}

static void save_cache(State* s) {
    toml::table games;

    for (const auto& [path, record] : s->records) {
        toml::table game {
            { "size", (int64_t)record.size },
            { "mtime", record.mtime },
            { "kind", record.kind }
        };

        if (record.serial.size()) game.insert("serial", record.serial);
        if (record.name.size()) game.insert("name", record.name);
        if (record.format.size()) game.insert("format", record.format);
        if (record.system.size()) game.insert("system", record.system);
        if (record.subdir.size()) game.insert("subdir", record.subdir);
        if (record.last_played) game.insert("last_played", record.last_played);

        games.insert(path, std::move(game));
    }

    toml::table missing;

    for (const auto& [serial, time] : s->missing) {
        missing.insert(serial, time);
    }

    toml::table table {
        { "version", CACHE_VERSION },
        { "games", std::move(games) },
        { "missing", std::move(missing) }
    };

    std::ostringstream stream;

    stream << table;

    if (!write_file_atomic(s->cache_path, stream.str())) {
        iris_error(&s->iris->log.library, "Couldn't write library cache \"{}\"", s->cache_path.string());
    }

    s->cache_dirty = false;
}

static void mark_cache_dirty(State* s) {
    if (!s->cache_dirty) {
        s->cache_dirty_since = now_seconds();
    }

    s->cache_dirty = true;
}

static bool recently_missing(const State* s, const std::string& key) {
    auto it = s->missing.find(key);

    if (it == s->missing.end()) {
        return false;
    }

    return unix_time() - it->second < MISSING_RETRY_SECONDS;
}

static void queue_background(State* s) {
    if (!s->downloads) {
        return;
    }

    std::vector <Job> jobs;

    for (const auto& [path, record] : s->records) {
        if (record.kind != KIND_PS2_CD && record.kind != KIND_PS2_DVD) {
            continue;
        }

        if (!is_downloadable_serial(record.serial)) {
            continue;
        }

        if (s->background_queued.contains(record.serial) || recently_missing(s, record.serial)) {
            continue;
        }

        s->background_queued.insert(record.serial);

        Job job;

        job.key = record.serial;
        job.background = true;
        job.allow_download = true;

        jobs.push_back(std::move(job));
    }

    if (jobs.empty()) {
        return;
    }

    s->covers_total += (int)jobs.size();

    std::lock_guard <std::mutex> lock(s->mutex);

    for (Job& job : jobs) {
        add_job_locked(s, std::move(job), false);
    }
}

static void request_db(State* s) {
    std::lock_guard <std::mutex> lock(s->mutex);

    s->db_wanted = true;
    s->db_allow_download = s->downloads;
    s->fetching_db = true;
    s->cv.notify_one();
}

static void rebuild_entries(State* s) {
    s->entries.clear();
    s->entries.reserve(s->records.size());

    for (const auto& [path, record] : s->records) {
        if (record.kind == KIND_IGNORED) {
            continue;
        }

        Entry entry;

        entry.path = path;
        entry.kind = record.kind;
        entry.serial = record.serial;
        entry.format = record.format;
        entry.system = record.system;
        entry.title = record.name;
        entry.last_played = record.last_played;

        if (record.kind != KIND_ARCADE) {
            auto it = s->db.find(record.serial);

            if (it != s->db.end()) {
                entry.title = it->second.title;
                entry.region = it->second.region;
                entry.developer = it->second.developer;
                entry.release_date = it->second.release_date;
            }

            if (entry.region.empty()) {
                entry.region = region_from_serial(record.serial);
            }
        }

        s->entries.push_back(std::move(entry));
    }

    s->entries_dirty = false;
    s->generation++;
}

static void merge_scan(State* s, ScanBatch& batch) {
    if (batch.id != s->scan_id) {
        return;
    }

    for (auto& [path, record] : batch.records) {
        auto it = s->records.find(path);

        if (it != s->records.end()) {
            record.last_played = it->second.last_played;

            it->second = std::move(record);
        } else {
            s->records.emplace(path, std::move(record));
        }
    }

    if (batch.done) {
        std::erase_if(s->records, [&](const auto& item) {
            if (item.second.seen == batch.id) {
                return false;
            }

            for (const std::string& root : batch.unavailable) {
                if (item.first.starts_with(root)) {
                    return false;
                }
            }

            return true;
        });
    }

    s->entries_dirty = true;

    mark_cache_dirty(s);
}

static void evict(State* s) {
    int resident = 0;

    for (const auto& [key, cover] : s->covers) {
        if (cover.state == COVER_LOADED) {
            resident++;
        }
    }

    if (resident <= TEXTURE_CAP) {
        return;
    }

    std::vector <std::pair <uint64_t, std::string>> loaded;

    uint64_t frame = s->frame.load();

    for (const auto& [key, cover] : s->covers) {
        if (cover.state == COVER_LOADED && cover.last_drawn + 2 < frame) {
            loaded.emplace_back(cover.last_drawn, key);
        }
    }

    int target = TEXTURE_EVICT_TARGET;

    std::sort(loaded.begin(), loaded.end());

    std::vector <Texture> textures;

    for (const auto& [last_drawn, key] : loaded) {
        if (resident <= target) {
            break;
        }

        Cover& cover = s->covers[key];

        textures.push_back(cover.texture);

        cover.texture = {};
        cover.state = COVER_NONE;

        resident--;
    }

    vulkan::free_textures(s->iris, textures);
}

static void free_all_covers(State* s) {
    std::vector <Texture> textures;

    for (auto& [key, cover] : s->covers) {
        if (cover.state == COVER_LOADED) {
            textures.push_back(cover.texture);
        }
    }

    s->covers.clear();
    s->uploads.clear();

    vulkan::free_textures(s->iris, textures);
}

static void apply_result(State* s, Result& result) {
    Cover& cover = s->covers[result.key];

    if (result.background) {
        s->covers_done++;

        if (result.outcome == OUTCOME_SAVED) {
            s->covers_saved++;
        }
    }

    switch (result.outcome) {
        case OUTCOME_MISSING: {
            s->missing[result.key] = unix_time();

            if (result.background) {
                s->covers_missing++;
            }

            mark_cache_dirty(s);

            if (cover.state != COVER_LOADED) {
                cover.state = COVER_MISSING;
            }
        } break;

        case OUTCOME_UNAVAILABLE: {
            if (result.wanted && cover.state == COVER_QUEUED) {
                cover.state = COVER_MISSING;
            }
        } break;

        case OUTCOME_FAILED: {
            if (result.wanted && cover.state == COVER_QUEUED) {
                cover.state = COVER_FAILED;
            }
        } break;

        default: {
            if (!result.wanted || cover.state != COVER_QUEUED) {
                break;
            }

            if (result.load && result.pixels.data.size()) {
                s->uploads.emplace_back(result.key, std::move(result.pixels));
            } else {
                cover.state = COVER_NONE;
            }
        } break;
    }
}

static void process_uploads(State* s) {
    int budget = UPLOADS_PER_FRAME;

    uint64_t frame = s->frame.load();

    while (budget && s->uploads.size()) {
        auto [key, pixels] = std::move(s->uploads.front());

        s->uploads.pop_front();

        Cover& cover = s->covers[key];

        if (cover.state != COVER_QUEUED) {
            continue;
        }

        if (cover.last_drawn + STALE_REQUEST_FRAMES < frame) {
            cover.state = COVER_NONE;

            continue;
        }

        Texture texture = vulkan::upload_texture(s->iris, pixels.data.data(), pixels.width, pixels.height, 4);

        if (!texture.descriptor_set) {
            cover.state = COVER_FAILED;

            continue;
        }

        cover.texture = texture;
        cover.state = COVER_LOADED;
        cover.loaded_at = now_seconds();

        budget--;
    }

    evict(s);
}

static void flush_logs(State* s, std::vector <LogMessage>& logs) {
    for (const LogMessage& message : logs) {
        iris_log(&s->iris->log.library, message.level, "{}", message.text);
    }
}

bool init(Instance* iris) {
    State* s = new State();

    s->iris = iris;
    s->root = fs::path(iris->paths.pref_path) / "library";
    s->covers_dir = s->root / "covers";
    s->cache_path = s->root / "cache.toml";
    s->db_path = s->root / "gamedb.tsv";
    s->downloads = iris->library.download_covers;
    s->scanned_dirs = iris->library.dirs;
    s->scanned_recursive = iris->library.recursive;

    iris->library.state = s;

    if (iris->headless) {
        return true;
    }

    std::error_code ec;

    fs::create_directories(s->covers_dir / "arcade", ec);

    if (ec) {
        iris_error(&iris->log.library, "Couldn't create library folder \"{}\"", s->covers_dir.string());
    }

    load_cache(s);

    for (int i = 0; i < WORKER_COUNT; i++) {
        s->workers.emplace_back(worker_main, s);
    }

    request_db(s);

    if (iris->library.dirs.size()) {
        start_scan(s);
    }

    queue_background(s);

    return true;
}

void destroy(Instance* iris) {
    State* s = iris->library.state;

    if (!s) {
        return;
    }

    stop_scan(s);

    {
        std::lock_guard <std::mutex> lock(s->mutex);

        s->stop = true;
        s->cv.notify_all();
    }

    for (std::thread& worker : s->workers) {
        worker.join();
    }

    if (s->cache_dirty) {
        save_cache(s);
    }

    free_all_covers(s);

    delete s;

    iris->library.state = nullptr;
}

void update(Instance* iris) {
    State* s = iris->library.state;

    if (!s || iris->headless) {
        return;
    }

    s->frame++;

    if (s->scanned_dirs != iris->library.dirs || s->scanned_recursive != iris->library.recursive) {
        start_scan(s);
    }

    if (s->downloads != iris->library.download_covers) {
        s->downloads = iris->library.download_covers;

        if (s->downloads) {
            request_db(s);
            queue_background(s);
        }
    }

    std::deque <ScanBatch> batches;
    std::deque <Result> results;
    std::vector <LogMessage> logs;
    std::unique_ptr <Db> db;

    {
        std::lock_guard <std::mutex> lock(s->mutex);

        batches.swap(s->scan_inbox);
        results.swap(s->results);
        logs.swap(s->logs);
        db = std::move(s->db_ready);
    }

    flush_logs(s, logs);

    if (db) {
        s->db = std::move(*db);
        s->entries_dirty = true;
    }

    for (ScanBatch& batch : batches) {
        merge_scan(s, batch);
    }

    if (batches.size()) {
        queue_background(s);
    }

    for (Result& result : results) {
        apply_result(s, result);
    }

    if (s->covers_total && s->covers_done >= s->covers_total) {
        if (s->covers_saved || s->covers_missing) {
            iris_info(&iris->log.library, "Downloaded {} covers, {} unavailable", s->covers_saved, s->covers_missing);
        }

        s->covers_total = 0;
        s->covers_done = 0;
        s->covers_saved = 0;
        s->covers_missing = 0;
    }

    if (s->entries_dirty) {
        rebuild_entries(s);
    }

    if (iris->ui.show_library) {
        process_uploads(s);
    }

    if (s->cache_dirty && now_seconds() - s->cache_dirty_since > CACHE_SAVE_DELAY) {
        save_cache(s);
    }
}

void rescan(Instance* iris) {
    State* s = iris->library.state;

    if (s) {
        start_scan(s);
    }
}

void refresh_covers(Instance* iris) {
    State* s = iris->library.state;

    if (!s) {
        return;
    }

    s->missing.clear();
    s->background_queued.clear();

    for (auto& [key, cover] : s->covers) {
        if (cover.state == COVER_MISSING || cover.state == COVER_FAILED) {
            cover.state = COVER_NONE;
        }
    }

    mark_cache_dirty(s);

    request_db(s);
    queue_background(s);
}

void clear_cover_cache(Instance* iris) {
    State* s = iris->library.state;

    if (!s) {
        return;
    }

    free_all_covers(s);

    std::error_code ec;

    for (const fs::directory_entry& entry : fs::directory_iterator(s->covers_dir, ec)) {
        std::error_code entry_ec;

        if (entry.is_regular_file(entry_ec) && lowercase(entry.path().extension().string()) == ".jpg") {
            fs::remove(entry.path(), entry_ec);
        }
    }

    s->missing.clear();
    s->background_queued.clear();

    mark_cache_dirty(s);

    queue_background(s);
}

void refresh_cover(Instance* iris, const Entry& entry) {
    State* s = iris->library.state;

    if (!s) {
        return;
    }

    std::string key = cover_key(entry.kind, entry.serial);

    if (key.empty()) {
        return;
    }

    auto it = s->covers.find(key);

    if (it != s->covers.end()) {
        if (it->second.state == COVER_LOADED) {
            vulkan::free_texture(iris, it->second.texture);
        }

        s->covers.erase(it);
    }

    if (!is_arcade_key(key)) {
        std::error_code ec;

        fs::remove(s->covers_dir / (key + ".jpg"), ec);
    }

    s->missing.erase(key);

    mark_cache_dirty(s);
}

void mark_played(Instance* iris, const std::string& path) {
    State* s = iris->library.state;

    if (!s || path.empty()) {
        return;
    }

    auto it = s->records.find(normalize_path(path));

    if (it == s->records.end()) {
        return;
    }

    it->second.last_played = unix_time();

    s->entries_dirty = true;

    mark_cache_dirty(s);
}

const std::vector <Entry>& entries(Instance* iris) {
    static const std::vector <Entry> empty;

    State* s = iris->library.state;

    return s ? s->entries : empty;
}

uint64_t generation(Instance* iris) {
    State* s = iris->library.state;

    return s ? s->generation : 0;
}

Progress progress(Instance* iris) {
    Progress progress;

    State* s = iris->library.state;

    if (!s) {
        return progress;
    }

    progress.scanning = s->scanning;
    progress.fetching_db = s->fetching_db;
    progress.covers_done = s->covers_done;
    progress.covers_total = s->covers_total;

    return progress;
}

const Texture* request_cover(Instance* iris, const Entry& entry, float* alpha) {
    *alpha = 0.0f;

    State* s = iris->library.state;

    if (!s) {
        return nullptr;
    }

    std::string key = cover_key(entry.kind, entry.serial);

    if (key.empty()) {
        return nullptr;
    }

    Cover& cover = s->covers[key];

    uint64_t frame = s->frame.load();

    cover.last_drawn = frame;

    if (cover.state == COVER_NONE) {
        cover.state = COVER_QUEUED;

        Job job;

        job.key = key;
        job.load = true;
        job.allow_download = s->downloads && !recently_missing(s, key);
        job.frame = frame;

        add_job(s, std::move(job), true);

        return nullptr;
    }

    if (cover.state != COVER_LOADED) {
        return nullptr;
    }

    *alpha = (float)std::clamp((now_seconds() - cover.loaded_at) / FADE_SECONDS, 0.0, 1.0);

    return &cover.texture;
}

}
