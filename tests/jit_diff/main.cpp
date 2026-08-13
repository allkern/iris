// Differential test for the EE/IOP recompilers.
//
// The question it answers is "does this backend's codegen agree with another
// backend's". One build records what the recompiler produced for a fixed set of
// cases; another build replays the same cases and compares. Nothing about the
// comparison depends on an interpreter, which matters because the interpreters
// stopped being maintained when the emulator switched to the recompiler.
//
// There is still an --interpreter mode, but it is a fallback for when there is
// only one machine to hand, and it has its own false positives.

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
    if (level < logger::Level::ERROR)
        return;

    FatalSink* sink = (FatalSink*)udata;

    if (level == logger::Level::FATAL_ERROR)
        ++sink->fatal_count;

    // The recompiler logs the whole offending guest block at ERROR before the
    // fatal error that follows it, which is the only way to find out which
    // instruction it could not encode. Keep enough of that to be useful.
    if (sink->lines.size() < 400)
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

void usage(const char* argv0) {
    printf("usage: %s [--seed N] [--iterations N] [--ee|--iop]\n"
           "       [--record PATH | --compare PATH | --interpreter]\n"
           "       [--label NAME] [--other-label NAME] [--signatures-out PATH]\n"
           "\n"
           "  --record PATH    run the recompiler and write what it produced\n"
           "  --compare PATH   run it again elsewhere and diff against that (default)\n"
           "  --interpreter    diff against the in-process interpreter instead\n"
           "\n"
           "exit status: 0 = clean, 1 = divergences, 2 = a core logged a fatal\n"
           "error (failed codegen or unimplemented opcode), 3 = usage\n",
           argv0);
}

}

void install_fatal_sink(Logger* logger, FatalSink* sink) {
    logger::register_callback(logger, fatal_callback, sink);
}

}

int main(int argc, char** argv) {
    using namespace jitdiff;

    Options opt;

    opt.seed = 0x123456789abcdefull;
    opt.iterations = 20000;

    bool do_ee = true;
    bool do_iop = true;
    const char* signatures_out = nullptr;
    const char* blob_path = nullptr;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            opt.seed = strtoull(argv[++i], nullptr, 0);
        } else if (!strcmp(argv[i], "--iterations") && i + 1 < argc) {
            opt.iterations = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--signatures-out") && i + 1 < argc) {
            signatures_out = argv[++i];
        } else if (!strcmp(argv[i], "--record") && i + 1 < argc) {
            opt.mode = Mode::Record;
            blob_path = argv[++i];
        } else if (!strcmp(argv[i], "--compare") && i + 1 < argc) {
            opt.mode = Mode::Compare;
            blob_path = argv[++i];
        } else if (!strcmp(argv[i], "--interpreter")) {
            opt.mode = Mode::Interpreter;
            opt.other_label = "interp";
        } else if (!strcmp(argv[i], "--label") && i + 1 < argc) {
            opt.label = argv[++i];
        } else if (!strcmp(argv[i], "--other-label") && i + 1 < argc) {
            opt.other_label = argv[++i];
        } else if (!strcmp(argv[i], "--ee")) {
            do_iop = false;
        } else if (!strcmp(argv[i], "--iop")) {
            do_ee = false;
        } else {
            usage(argv[0]);

            return 3;
        }
    }

    if (opt.mode != Mode::Interpreter && !blob_path) {
        printf("--record or --compare needs a path (or use --interpreter)\n\n");
        usage(argv[0]);

        return 3;
    }

#if defined(ASMJIT_UJIT_AARCH64) && !defined(__aarch64__) && !defined(_M_ARM64)
#error ujit backend does not match the host architecture
#endif

    if (!strcmp(opt.label, "this"))
        opt.label = host_arch();

    Blob blob;

    if (opt.mode == Mode::Compare && !blob.load(blob_path)) {
        printf("could not read recording %s\n", blob_path);

        return 3;
    }

    opt.blob = &blob;

    printf("iris jit differential test\n");
    printf("  host arch:    %s\n", host_arch());
    printf("  ujit backend: %s\n", ujit_backend());
    printf("  mode:         %s\n",
           opt.mode == Mode::Record      ? "record"
           : opt.mode == Mode::Compare   ? "compare against a recording"
                                         : "compare against the interpreter");
    printf("  iterations:   %d per core\n", opt.iterations);
    printf("  seed:         0x%llx\n", (unsigned long long)opt.seed);

    Logger* logger = logger::create();

    logger::set_level(logger, logger::Level::ERROR);

    FatalSink sink;

    install_fatal_sink(logger, &sink);

    int failures = 0;

    std::vector <std::string> signatures;

    auto collect = [&](const Report& rep) {
        for (const auto& kv : rep.buckets)
            signatures.push_back(kv.first);
    };

    // Each core gets its own seed and its own region of the recording, so
    // --ee and --iop stay usable on their own.
    if (do_iop) {
        Report rep;
        Options o = opt;

        failures += run_iop_tests(logger, o, &rep);

        print_report(rep);
        collect(rep);
    }

    if (do_ee) {
        Report rep;
        Options o = opt;

        o.seed = opt.seed ^ 0xa5a5a5a5a5a5a5a5ull;

        failures += run_ee_tests(logger, o, &rep);

        print_report(rep);
        collect(rep);
    }

    if (opt.mode == Mode::Record) {
        if (!blob.save(blob_path)) {
            printf("\ncould not write recording %s\n", blob_path);

            return 3;
        }

        printf("\nrecorded %zu bytes to %s\n", blob.data.size(), blob_path);
    }

    if (!sink.lines.empty()) {
        printf("\n%d fatal error(s) logged by the cores, with context:\n", sink.fatal_count);

        for (const std::string& line : sink.lines)
            printf("  %s\n", line.c_str());
    }

    if (sink.fatal_count) {
        printf("\nA fatal error here is more serious than a divergence: a block the\n"
               "recompiler cannot encode leaves block->func null, and the core then\n"
               "makes no forward progress at all.\n");
    }

    // One signature per line, sorted, nothing else - so two runs can be
    // compared with comm(1).
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

    bool bad = failures || sink.fatal_count;

    if (opt.mode == Mode::Record)
        printf("\n%s\n", sink.fatal_count ? "FAILED" : "RECORDED");
    else
        printf("\n%s\n", bad ? "FAILED" : "PASSED");

    logger::destroy(logger);

    if (sink.fatal_count) return 2;

    return failures ? 1 : 0;
}
