// Differential test: run every EE/IOP instruction through the JIT and through
// the interpreter from identical state and compare the architectural result.
//
// The point is portability. Both engines are built from the same source on
// every target, so a divergence that shows up only on one host architecture is
// a code generation bug in that backend, which is exactly what this catches.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <asmjit/ujit.h>

#include "common.hpp"

namespace jitdiff {

using namespace iris;

namespace {

void fatal_callback(void* udata, logger::Level level, const logger::Source& source,
                    const std::string& text) {
    if (level != logger::Level::FATAL_ERROR)
        return;

    FatalSink* sink = (FatalSink*)udata;

    ++sink->count;

    if (sink->lines.size() < 50)
        sink->lines.push_back(source.name + ": " + text);
}

const char* host_arch() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "unknown";
#endif
}

const char* ujit_backend() {
#if defined(ASMJIT_UJIT_AARCH64)
    return "aarch64";
#elif defined(ASMJIT_UJIT_X86)
    return "x86";
#else
    return "none";
#endif
}

void print_report(const Report& rep) {
    printf("\n[%s] %d cases, %d divergences in %d signature(s) (seed=0x%llx)\n",
           rep.core, rep.cases, rep.failures, (int)rep.buckets.size(),
           (unsigned long long)rep.seed);

    for (const auto& kv : rep.buckets)
        printf("  %-30s x%-6d %s\n", kv.first.c_str(), kv.second.count,
               kv.second.example.c_str());
}

}

void install_fatal_sink(Logger* logger, FatalSink* sink) {
    logger::register_callback(logger, fatal_callback, sink);
}

}

int main(int argc, char** argv) {
    using namespace jitdiff;

    uint64_t seed = 0x123456789abcdefull;
    int iterations = 20000;
    bool do_ee = true;
    bool do_iop = true;
    const char* signatures_out = nullptr;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = strtoull(argv[++i], nullptr, 0);
        } else if (!strcmp(argv[i], "--iterations") && i + 1 < argc) {
            iterations = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--signatures-out") && i + 1 < argc) {
            signatures_out = argv[++i];
        } else if (!strcmp(argv[i], "--ee")) {
            do_iop = false;
        } else if (!strcmp(argv[i], "--iop")) {
            do_ee = false;
        } else {
            printf("usage: %s [--seed N] [--iterations N] [--ee|--iop]\n"
                   "       [--signatures-out PATH]\n"
                   "\n"
                   "exit status: 0 = clean, 1 = divergences, 2 = a core logged a\n"
                   "fatal error (failed codegen or unimplemented opcode), 3 = usage\n",
                   argv[0]);

            return 3;
        }
    }

#if defined(ASMJIT_UJIT_AARCH64) && !defined(__aarch64__) && !defined(_M_ARM64)
#error ujit backend does not match the host architecture
#endif

    printf("iris jit differential test\n");
    printf("  host arch:    %s\n", host_arch());
    printf("  ujit backend: %s\n", ujit_backend());
    printf("  iterations:   %d per core\n", iterations);
    printf("  seed:         0x%llx\n", (unsigned long long)seed);

    Logger* logger = logger::create();

    logger::set_level(logger, logger::Level::FATAL_ERROR);

    FatalSink sink;

    install_fatal_sink(logger, &sink);

    int failures = 0;

    std::vector <std::string> signatures;

    auto collect = [&](const Report& rep) {
        for (const auto& kv : rep.buckets)
            signatures.push_back(kv.first);
    };

    if (do_iop) {
        Report rep;

        failures += run_iop_tests(logger, seed, iterations, &rep);

        print_report(rep);
        collect(rep);
    }

    if (do_ee) {
        Report rep;

        failures += run_ee_tests(logger, seed ^ 0xa5a5a5a5a5a5a5a5ull, iterations, &rep);

        print_report(rep);
        collect(rep);
    }

    if (sink.count) {
        printf("\n%d fatal error(s) logged by the cores:\n", sink.count);

        for (const std::string& line : sink.lines)
            printf("  %s\n", line.c_str());

        printf("\nA fatal error here is more serious than a divergence: a block the\n"
               "recompiler cannot encode leaves block->func null, and the core then\n"
               "makes no forward progress at all.\n");
    }

    // One signature per line, sorted, nothing else - so two runs on different
    // architectures can be compared with comm(1).
    if (signatures_out) {
        std::sort(signatures.begin(), signatures.end());

        FILE* f = fopen(signatures_out, "w");

        if (!f) {
            printf("\ncould not write %s\n", signatures_out);

            return 3;
        }

        for (const std::string& s : signatures)
            fprintf(f, "%s\n", s.c_str());

        fclose(f);
    }

    printf("\n%s\n", (failures || sink.count) ? "FAILED" : "PASSED");

    logger::destroy(logger);

    if (sink.count) return 2;

    return failures ? 1 : 0;
}
