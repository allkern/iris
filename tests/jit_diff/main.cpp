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

void print_report(const Report& rep, int budget) {
    printf("\n[%s] %d cases, %d divergences in %d signature(s) (seed=0x%llx)\n",
           rep.core.c_str(), rep.cases, rep.failures, (int)rep.buckets.size(),
           (unsigned long long)rep.seed);

    // A case count says how many blocks were built, not how much of each one
    // ran. Without this, a pass whose cases all leave on the first instruction
    // is indistinguishable from one that walks every branch it generated.
    if (rep.cases) {
        printf("  steps/case: min %d mean %.1f max %d, %d of %d used the whole %d-cycle budget\n",
               rep.steps_min, (double)rep.steps_total / rep.cases, rep.steps_max,
               rep.at_budget, rep.cases, budget);

        // Descending by frequency, so the shape of the pass is the first thing
        // on the line. Truncated because a long tail costs width without
        // saying much - min and max above already bound it.
        std::vector <std::pair<int, int>> by_freq(rep.steps_hist.begin(), rep.steps_hist.end());

        std::sort(by_freq.begin(), by_freq.end(),
                  [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                      return a.second != b.second ? a.second > b.second : a.first < b.first;
                  });

        printf("  steps seen (steps x cases):");

        for (int i = 0; i < (int)by_freq.size() && i < 8; i++)
            printf("  %d x%d", by_freq[i].first, by_freq[i].second);

        if ((int)by_freq.size() > 8)
            printf("  (+%d more)", (int)by_freq.size() - 8);

        printf("\n");

        // The failure this is here to catch: a block pass whose cases mostly
        // leave early is not testing the register cache, the spills or the
        // sub-block edges it exists to test, and it reports clean while not
        // testing them.
        if (budget > 1 && rep.at_budget * 2 < rep.cases)
            printf("  NOTE: most cases left the block before the budget ran out, so this\n"
                   "        pass covers less than its case count suggests\n");
    }

    for (const auto& kv : rep.buckets)
        printf("  %-30s x%-6d %s\n", kv.first.c_str(), kv.second.count,
               kv.second.example.c_str());
}

void usage(const char* argv0) {
    printf("usage: %s [--seed N] [--iterations N] [--ee|--iop] [--pass NAME]\n"
           "       [--block-size N]\n"
           "       [--record PATH | --compare PATH | --interpreter]\n"
           "       [--label NAME] [--other-label NAME] [--signatures-out PATH]\n"
           "\n"
           "  --record PATH    run the recompiler and write what it produced\n"
           "  --compare PATH   run it again elsewhere and diff against that (default)\n"
           "  --interpreter    diff against the in-process interpreter instead\n"
           "  --pass NAME      run only this pass (repeatable); default is all of them\n"
           "  --block-size N   instructions per case in the block passes\n"
           "\n"
           "exit status: 0 = clean, 1 = divergences, 2 = a core logged a fatal\n"
           "error (failed codegen or unimplemented opcode), 3 = usage\n",
           argv0);
}

// The passes, in the order both sides have to run them.
//
// A recording is one flat stream, so record and compare have to walk the same
// list in the same order or they line up against the wrong case. Selecting a
// subset with --pass is therefore only useful when both sides select the same
// subset.
//
// The shapes are separate passes rather than a mix inside one because they ask
// different questions. Single tells you whether an instruction computes the
// right value. Block tells you whether the register cache, constant folding and
// sub-block edges still hold once there is more than one instruction to keep
// track of - which is the shape all real guest code has and the one the
// recompiler does most of its work for.
struct Pass {
    const char* name;
    bool ee;
    Shape shape;
    bool fastmem;
    int budget_mul;   // run_block budget, per instruction in the case
    int seed_group;   // passes sharing one generate the same cases
};

// A fastmem pass shares its seed group with its slow-path twin, so the two run
// the same cases. Fastmem is meant to be transparent - same address, same value,
// same architectural state - so the two recordings have to come out byte for
// byte identical on one machine. That is an oracle that needs no second
// architecture, and it covers the inline load/store path, which is what shipped
// builds run and what the old harness never executed at all.
const Pass g_passes[] = {
    { "iop-single",         false, Shape::Single, false, 0, 0 },
    { "iop-block",          false, Shape::Block,  false, 4, 1 },
    { "ee-single",          true,  Shape::Single, false, 0, 2 },
    { "ee-single-fastmem",  true,  Shape::Single, true,  0, 2 },
    { "ee-block",           true,  Shape::Block,  false, 4, 3 },
    { "ee-block-fastmem",   true,  Shape::Block,  true,  4, 3 },
    { "ee-except",          true,  Shape::Except, false, 0, 4 }
};

constexpr int PASS_COUNT = (int)(sizeof(g_passes) / sizeof(*g_passes));

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

    std::vector <std::string> selected;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--pass") && i + 1 < argc) {
            selected.push_back(argv[++i]);
        } else if (!strcmp(argv[i], "--block-size") && i + 1 < argc) {
            opt.block_size = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
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

    // Each pass gets its own seed and its own region of the recording, so a
    // change to one pass does not shift the cases every later pass generates.
    for (int p = 0; p < PASS_COUNT; p++) {
        const Pass& pass = g_passes[p];

        if (pass.ee ? !do_ee : !do_iop)
            continue;

        if (!selected.empty() &&
            std::find(selected.begin(), selected.end(), pass.name) == selected.end())
            continue;

        Report rep;
        Options o = opt;

        o.pass = pass.name;
        o.shape = pass.shape;
        o.fastmem = pass.fastmem;
        o.seed = opt.seed ^ (0xa5a5a5a5a5a5a5a5ull * (uint64_t)(pass.seed_group + 1));

        // A single-instruction case retires in one cycle. A case with edges in
        // it needs enough budget to walk them: every edge is guarded by a
        // cycles_left check that leaves the block when the budget is gone, so
        // too small a budget silently turns a control-flow test back into a
        // straight-line one.
        o.budget = pass.budget_mul ? pass.budget_mul * o.block_size : 1;

        printf("\n-- pass %s --\n", pass.name);

        failures += pass.ee ? run_ee_tests(logger, o, &rep)
                            : run_iop_tests(logger, o, &rep);

        print_report(rep, o.budget);
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
