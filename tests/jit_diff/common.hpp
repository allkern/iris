#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "logger.hpp"

namespace jitdiff {

using iris::logger::Logger;

// What the recompiler output is being checked against.
enum class Mode {
    // Against a recording made by another build. This is the one that answers
    // "does the arm64 backend agree with the x86 backend", and it is the
    // default, because the interpreters stopped being maintained when the
    // emulator switched to the recompiler.
    Compare,

    // Produce that recording.
    Record,

    // Against the in-process interpreter. Kept because it needs no second
    // machine, but it answers a weaker question and it has its own failure
    // modes - see the note in the README about FMA contraction.
    Interpreter
};

// Deterministic PRNG so a failure on one machine reproduces on another.
struct Rng {
    uint64_t s;

    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9e3779b97f4a7c15ull) {}

    uint64_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;

        return s;
    }

    uint32_t u32() { return (uint32_t)(next() >> 32); }
    uint64_t u64() { return next(); }

    // Values that stress sign, saturation and overflow, not just the middle of
    // the range.
    uint32_t interesting32() {
        static const uint32_t pool[] = {
            0x00000000, 0x00000001, 0xffffffff, 0x7fffffff, 0x80000000,
            0x0000ffff, 0xffff0000, 0x00008000, 0xffff8000, 0x0000007f,
            0x00000080, 0x000000ff, 0x7f7f7f7f, 0x80808080, 0x0000001f,
            0x00000020, 0x0000003f, 0x00000040, 0xfffffffe, 0x00010000
        };

        uint32_t r = u32();

        if ((r & 3) == 0) return pool[(r >> 8) % (sizeof(pool) / sizeof(*pool))];

        return u32();
    }

    uint64_t interesting64() {
        uint32_t r = u32();

        switch (r & 7) {
            case 0: return 0;
            case 1: return ~0ull;
            case 2: return 0x8000000000000000ull;
            case 3: return 0x7fffffffffffffffull;
            case 4: return (uint64_t)(int64_t)(int32_t)interesting32();
            default: return ((uint64_t)interesting32() << 32) | interesting32();
        }
    }

    // Floats the EE FPU actually cares about: zeroes, denormals it flushes,
    // values it clamps instead of producing inf/NaN, and ordinary numbers.
    uint32_t interesting_f32() {
        static const uint32_t pool[] = {
            0x00000000, 0x80000000,             // +-0
            0x00000001, 0x807fffff,             // denormals (EE flushes these)
            0x7f7fffff, 0xff7fffff,             // +-FLT_MAX (EE clamps here)
            0x7f800000, 0xff800000,             // inf encodings
            0x7fc00000, 0x7fffffff,             // NaN encodings
            0x3f800000, 0xbf800000,             // +-1.0
            0x40000000, 0xc0000000,             // +-2.0
            0x4b000000, 0x4f000000,             // 2^23, 2^31 (cvt.w.s edges)
            0x4effffff, 0x4f7fffff,             // just under / over int32 range
            0x00800000, 0x01000000              // smallest normals
        };

        uint32_t r = u32();

        if ((r & 1) == 0) return pool[(r >> 8) % (sizeof(pool) / sizeof(*pool))];

        return u32();
    }
};

inline uint64_t hash64(const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    uint64_t h = 0xcbf29ce484222325ull;

    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 0x100000001b3ull;
    }

    return h;
}

// Recompiler output from one build, written by --record and read by --compare.
// Fields go in one at a time rather than as a struct copy: the two builds are
// different targets and nothing here should depend on how either lays a struct
// out.
struct Blob {
    std::vector <uint8_t> data;
    size_t pos = 0;
    bool ok = true;

    void put(const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;

        data.insert(data.end(), b, b + n);
    }

    void put_u32(uint32_t v) { put(&v, 4); }
    void put_u64(uint64_t v) { put(&v, 8); }

    void get(void* p, size_t n) {
        if (pos + n > data.size()) {
            ok = false;

            return;
        }

        memcpy(p, data.data() + pos, n);

        pos += n;
    }

    uint32_t get_u32() { uint32_t v = 0; get(&v, 4); return v; }
    uint64_t get_u64() { uint64_t v = 0; get(&v, 8); return v; }

    bool save(const char* path) const {
        FILE* f = fopen(path, "wb");

        if (!f) return false;

        bool wrote = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();

        fclose(f);

        return wrote;
    }

    bool load(const char* path) {
        FILE* f = fopen(path, "rb");

        if (!f) return false;

        fseek(f, 0, SEEK_END);

        long n = ftell(f);

        fseek(f, 0, SEEK_SET);

        data.resize((size_t)(n < 0 ? 0 : n));

        bool read = data.empty() || fread(data.data(), 1, data.size(), f) == data.size();

        fclose(f);

        pos = 0;

        return read;
    }
};

// Divergences are aggregated by signature ("ee psrlw gpr.lo") rather than
// dumped one per line, so two runs can be compared as sets.
struct Report {
    struct Bucket {
        int count = 0;
        std::string example;
    };

    std::string core;
    uint64_t seed = 0;
    int cases = 0;
    int failures = 0;

    std::map <std::string, Bucket> buckets;

    void fail(const std::string& signature, const std::string& detail) {
        ++failures;

        Bucket& b = buckets[signature];

        ++b.count;

        if (b.example.empty())
            b.example = detail;
    }
};

// Anything the cores log at ERROR or worse. A recompiler that cannot encode an
// instruction logs the offending guest block and then a fatal error; both are
// worth keeping, and neither is a divergence.
struct FatalSink {
    int fatal_count = 0;

    std::vector <std::string> lines;
};

void install_fatal_sink(Logger* logger, FatalSink* sink);

// How a case is built.
enum class Shape {
    // One instruction, a nop, and a jump back to the start. Every sub-block
    // holds exactly one instruction, so the register cache is allocated and
    // flushed around each instruction in isolation.
    Single,

    // A run of instructions with branches wired to targets inside it. This is
    // the shape real guest code has, and the only one that puts the register
    // cache, constant folding, register pressure and sub-block chaining under
    // any load at all.
    Block,

    // One instruction that leaves for the exception vector, or ERET coming back
    // from one. Everything here is excluded from the other two shapes because it
    // does not stay inside the case, but the BIOS spends most of its time in
    // exactly these: a syscall out and an eret back. ERET in particular is
    // twenty-odd emitted instructions with branches and a call in the middle,
    // and nothing else was reaching any of it.
    Except
};

struct Options {
    uint64_t seed = 0;
    int iterations = 0;
    Mode mode = Mode::Compare;
    Blob* blob = nullptr;
    const char* label = "this";
    const char* other_label = "recorded";

    Shape shape = Shape::Single;

    // Instructions per case in Shape::Block, before the terminating jump.
    int block_size = 12;

    // Cycles handed to run_block(). One is enough to retire a single-instruction
    // sub-block; a block with internal edges needs enough budget to actually
    // walk them, because every edge is guarded by a cycles_left check that
    // leaves the block when the budget is gone.
    int budget = 1;

    // Populate the bus fastmem tables, so the inline load/store path and the
    // constant-address folding path execute instead of being emitted and never
    // taken. Shipped builds always run with this on.
    bool fastmem = false;

    // Shown in the report and in the pass banner.
    const char* pass = "";
};

int run_iop_tests(Logger* logger, const Options& opt, Report* out);
int run_ee_tests(Logger* logger, const Options& opt, Report* out);

}
