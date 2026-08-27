#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

#include "arcade.hpp"

namespace iris::arcade {

static constexpr uint64_t HASH_SIZE_LIMIT = 64ull * 1024 * 1024;

static constexpr auto CHD_TAG_SIZE = 8;
static constexpr auto CHD_HEADER_SIZE = 124;
static constexpr auto CHD_LOGICAL_BYTES_OFFSET = 32;
static constexpr auto CHD_SHA1_OFFSET = 84;
static constexpr auto SHA1_SIZE = 20;

static uint32_t g_crc_table[256];
static bool g_crc_table_ready = false;

static void build_crc_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t value = i;

        for (int bit = 0; bit < 8; bit++) {
            value = (value & 1) ? (0xedb88320 ^ (value >> 1)) : (value >> 1);
        }

        g_crc_table[i] = value;
    }

    g_crc_table_ready = true;
}

std::string normalize_key(const std::string& text) {
    std::string out;

    for (char c : text) {
        if (isalnum((unsigned char)c)) {
            out += (char)tolower((unsigned char)c);
        }
    }

    return out;
}

static uint64_t read_be64(const uint8_t* data) {
    uint64_t value = 0;

    for (int i = 0; i < 8; i++)
        value = (value << 8) | data[i];

    return value;
}

static std::string to_hex(const uint8_t* data, int size) {
    static const char* digits = "0123456789abcdef";

    std::string out;

    for (int i = 0; i < size; i++) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0xf];
    }

    return out;
}

static bool read_chd_header(FILE* file, Fingerprint* out) {
    uint8_t header[CHD_HEADER_SIZE];

    if (fread(header, 1, sizeof(header), file) != sizeof(header))
        return false;

    if (memcmp(header, "MComprHD", CHD_TAG_SIZE) != 0)
        return false;

    out->size = read_be64(header + CHD_LOGICAL_BYTES_OFFSET);
    out->sha1 = to_hex(header + CHD_SHA1_OFFSET, SHA1_SIZE);

    return true;
}

static uint32_t hash_file(FILE* file, uint64_t size) {
    if (!g_crc_table_ready)
        build_crc_table();

    std::vector <uint8_t> buffer(1024 * 1024);

    uint32_t crc = 0xffffffff;
    uint64_t left = size;

    while (left) {
        size_t want = left < buffer.size() ? (size_t)left : buffer.size();
        size_t got = fread(buffer.data(), 1, want, file);

        if (!got)
            break;

        for (size_t i = 0; i < got; i++)
            crc = g_crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);

        left -= got;
    }

    return crc ^ 0xffffffff;
}

bool probe_size(uint64_t size, Fingerprint* out) {
    *out = {};

    out->size = size;

    return size != 0;
}

bool probe_file(const std::filesystem::path& path, Fingerprint* out) {
    *out = {};

    std::error_code ec;

    uint64_t size = std::filesystem::file_size(path, ec);

    if (ec)
        return false;

    out->size = size;

    FILE* file = fopen(path.string().c_str(), "rb");

    if (!file)
        return false;

    if (read_chd_header(file, out)) {
        fclose(file);

        return true;
    }

    if (size <= HASH_SIZE_LIMIT) {
        fseek(file, 0, SEEK_SET);

        out->crc = hash_file(file, size);
        out->has_crc = true;
    }

    fclose(file);

    return true;
}

namespace {

struct Dump {
    std::string id;
    std::vector <std::string> names;
    std::string sha1;
    uint32_t crc;
    uint64_t size;
    int role;
};

}

static const std::vector <Dump>& dumps() {
    static std::vector <Dump> cached;

    if (cached.size())
        return cached;

    for (auto&& [key, value] : g_arcade_definitions) {
        const toml::table* table = value.as_table();

        if (!table)
            continue;

        for (int role = ROLE_DONGLE; role <= ROLE_MEDIA; role++) {
            std::string prefix = role == ROLE_DONGLE ? "dongle_" : "media_";

            auto sha1 = (*table)[prefix + "sha1"].value<std::string>();

            if (!sha1) {
                continue;
            }

            Dump dump;

            dump.id = std::string(key.str());

            std::string role_key = role == ROLE_DONGLE ? "dongle" : "media";

            if (const toml::array* listed = (*table)[role_key].as_array()) {
                for (const toml::node& element : *listed) {
                    if (auto value = element.value<std::string>()) {
                        dump.names.push_back(*value);
                    }
                }
            }

            dump.sha1 = *sha1;
            dump.crc = (uint32_t)(*table)[prefix + "crc"].value_or<int64_t>(0);
            dump.size = role == ROLE_DONGLE
                ? (uint64_t)(*table)["dongle_size"].value_or<int64_t>(DONGLE_SIZE)
                : (uint64_t)(*table)["media_size"].value_or<int64_t>(0);
            dump.role = role;

            cached.push_back(dump);
        }
    }

    return cached;
}

bool is_known_set(const std::string& id) {
    return resolve_set_name(id).size() != 0;
}

std::string resolve_set_name(const std::string& name) {
    if (name.empty())
        return "";

    if (g_arcade_definitions.contains(name))
        return name;

    std::string key = normalize_key(name);

    for (auto&& [id, value] : g_arcade_definitions) {
        const toml::table* table = value.as_table();

        if (!table)
            continue;

        const toml::array* aliases = (*table)["alias"].as_array();

        if (!aliases)
            continue;

        for (const toml::node& element : *aliases) {
            std::optional <std::string> text = element.value<std::string>();

            if (text && normalize_key(*text) == key)
                return std::string(id.str());
        }
    }

    return "";
}

void collect_candidates(const Fingerprint& print, const std::string& name, Candidates* out) {
    std::string key = normalize_key(name);

    for (const Dump& dump : dumps()) {
        int evidence = 0;

        if (print.sha1.size() && dump.sha1.size() && print.sha1 == dump.sha1) {
            evidence = EVIDENCE_HASH;
        } else if (print.has_crc && dump.crc && print.crc == dump.crc && print.size == dump.size) {
            evidence = EVIDENCE_HASH;
        } else if (print.size && print.size == dump.size) {
            evidence = EVIDENCE_SIZE;
        }

        int named = 0;

        for (const std::string& candidate : dump.names) {
            if (key.size() && normalize_key(candidate) == key) {
                named = EVIDENCE_NAME;

                break;
            }
        }

        if (!evidence && !named)
            continue;

        Candidate& candidate = (*out)[dump.id];

        candidate.score += evidence + named;

        if (evidence + named > candidate.strongest) {
            candidate.strongest = evidence + named;
            candidate.role = dump.role;
        }
    }
}

Match identify(const Fingerprint& print, const std::string& name) {
    Candidates candidates;

    collect_candidates(print, name, &candidates);

    Match match;

    int best = 0;

    for (const auto& [id, candidate] : candidates) {
        if (candidate.score <= best)
            continue;

        best = candidate.score;

        match.id = id;
        match.role = candidate.role;
    }

    if (best < EVIDENCE_ACCEPT)
        return {};

    match.confidence = best >= EVIDENCE_HASH ? CONFIDENCE_STRONG : CONFIDENCE_WEAK;

    return match;
}

Match identify_file(const std::filesystem::path& path) {
    Fingerprint print;

    if (!probe_file(path, &print))
        return {};

    return identify(print, path.filename().string());
}

}
