#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "logger.hpp"

namespace jitdiff {

using iris::logger::Logger;

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

// Divergences are aggregated by signature ("iop mtc0 cop0.CAUSE") rather than
// dumped one per line. A run on a second host is then a diff of signatures:
// anything new is specific to that backend.
struct Report {
    struct Bucket {
        int count = 0;
        std::string example;
    };

    const char* core = "";
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

// Anything the cores log at FATAL_ERROR: a JIT that fails to compile, or an
// unimplemented opcode, must not be mistaken for a passing test.
struct FatalSink {
    int count = 0;
    std::vector <std::string> lines;
};

void install_fatal_sink(Logger* logger, FatalSink* sink);

int run_iop_tests(Logger* logger, uint64_t seed, int iterations, Report* out);
int run_ee_tests(Logger* logger, uint64_t seed, int iterations, Report* out);

}
