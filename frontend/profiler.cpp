#include "profiler.hpp"
#include "profile_tag.hpp"
#include "profile_counters.hpp"

#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

struct SYMBOL_INFO {
    ULONG SizeOfStruct;
    ULONG TypeIndex;
    ULONG64 Reserved[2];
    ULONG Index;
    ULONG Size;
    ULONG64 ModBase;
    ULONG Flags;
    ULONG64 Value;
    ULONG64 Address;
    ULONG Register;
    ULONG Scope;
    ULONG Tag;
    ULONG NameLen;
    ULONG MaxNameLen;
    CHAR Name[1];
};

extern "C" {
    __declspec(dllimport) BOOL __stdcall SymInitialize(HANDLE process, PCSTR search_path, BOOL invade);
    __declspec(dllimport) BOOL __stdcall SymCleanup(HANDLE process);
    __declspec(dllimport) DWORD __stdcall SymSetOptions(DWORD options);
    __declspec(dllimport) BOOL __stdcall SymFromAddr(HANDLE process, DWORD64 address, PDWORD64 displacement, SYMBOL_INFO* symbol);
}

constexpr DWORD SYMOPT_UNDNAME = 0x2;
constexpr DWORD SYMOPT_DEFERRED_LOADS = 0x4;
constexpr DWORD SYMOPT_LOAD_LINES = 0x10;

namespace iris::profiler {

namespace {

constexpr int MAX_CALLERS = 4;
constexpr uint32_t NO_CALLERS = 0xffffffff;
constexpr uint64_t STACK_SCAN_BYTES = 16 * 1024;
constexpr int OTHER_THREAD_TOP = 10;
constexpr double OTHER_THREAD_MIN_SHARE = 2.0;

enum class CodeKind {
    IRIS,
    EXTERNAL,
    GENERATED
};

struct Sample {
    uint64_t ip;
    uint32_t tid;
    int jit;
    uint32_t callers;
};

struct Callers {
    uint64_t frames[MAX_CALLERS];
    int count;
};

struct Target {
    HANDLE handle;
    uint32_t tid;
    uint64_t last_ip;
    uint64_t last_sp;
    uint32_t last_callers;
};

struct CodeRange {
    uint64_t start;
    uint64_t end;
};

struct Symbol {
    std::string name;
    CodeKind kind;
};

struct SymbolKey {
    uint64_t ip;
    int jit;

    bool operator==(const SymbolKey& other) const {
        return ip == other.ip && jit == other.jit;
    }
};

struct SymbolKeyHash {
    size_t operator()(const SymbolKey& key) const {
        return key.ip ^ ((size_t)key.jit << 56);
    }
};

using SymbolCache = std::unordered_map <SymbolKey, Symbol, SymbolKeyHash>;
using CallerNameCache = std::unordered_map <uint64_t, std::string>;
using FunctionCounts = std::unordered_map <std::string, uint64_t>;

struct ThreadProfile {
    uint64_t seen = 0;
    uint64_t busy = 0;
    FunctionCounts functions;
    FunctionCounts external_by_caller;
};

std::atomic <bool> g_running = false;
std::thread g_thread;
std::vector <Sample> g_samples;
std::vector <Callers> g_callers;
std::vector <CodeRange> g_code_ranges;
uint32_t g_sampler_tid = 0;
uint32_t g_emulation_tid = 0;
uint64_t g_image_start = 0;
uint64_t g_image_end = 0;

void find_image_ranges() {
    HMODULE module = GetModuleHandleA(nullptr);

    IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)module;
    IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((uint8_t*)module + dos_header->e_lfanew);

    g_image_start = (uint64_t)module;
    g_image_end = g_image_start + nt_headers->OptionalHeader.SizeOfImage;

    g_code_ranges.clear();

    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt_headers);

    for (int index = 0; index < nt_headers->FileHeader.NumberOfSections; index++) {
        if (section[index].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            uint64_t start = g_image_start + section[index].VirtualAddress;
            uint64_t end = start + section[index].Misc.VirtualSize;

            g_code_ranges.push_back({ start, end });
        }
    }
}

bool is_in_image(uint64_t address) {
    return address >= g_image_start && address < g_image_end;
}

bool is_in_code(uint64_t address) {
    for (const CodeRange& range : g_code_ranges) {
        if (address >= range.start + 8 && address < range.end) {
            return true;
        }
    }

    return false;
}

bool follows_call(uint64_t address) {
    if (!is_in_code(address)) {
        return false;
    }

    const uint8_t* code = (const uint8_t*)address;

    if (code[-5] == 0xe8) {
        return true;
    }

    for (int length = 2; length <= 7; length++) {
        uint8_t opcode = code[-length];
        uint8_t modrm = code[-length + 1];

        if (opcode == 0xff && (modrm & 0x38) == 0x10) {
            return true;
        }
    }

    return false;
}

bool is_readable_stack(const MEMORY_BASIC_INFORMATION& region) {
    if (region.State != MEM_COMMIT) {
        return false;
    }

    if (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) {
        return false;
    }

    return (region.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READWRITE)) != 0;
}

int scan_callers(uint64_t sp, uint64_t* frames) {
    MEMORY_BASIC_INFORMATION region;

    if (!VirtualQuery((LPCVOID)sp, &region, sizeof(region))) {
        return 0;
    }

    if (!is_readable_stack(region)) {
        return 0;
    }

    uint64_t region_end = (uint64_t)region.BaseAddress + region.RegionSize;
    uint64_t end = sp + STACK_SCAN_BYTES;

    if (end > region_end) {
        end = region_end;
    }

    int count = 0;

    for (uint64_t slot = sp & ~7ull; slot + 8 <= end; slot += 8) {
        uint64_t value = *(const uint64_t*)slot;

        if (!follows_call(value)) {
            continue;
        }

        frames[count] = value;
        count++;

        if (count == MAX_CALLERS) {
            break;
        }
    }

    return count;
}

std::vector <Target> snapshot_threads() {
    std::vector <Target> targets;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return targets;
    }

    DWORD pid = GetCurrentProcessId();

    THREADENTRY32 entry;

    entry.dwSize = sizeof(entry);

    if (!Thread32First(snapshot, &entry)) {
        CloseHandle(snapshot);

        return targets;
    }

    do {
        if (entry.th32OwnerProcessID != pid) {
            continue;
        }

        if (entry.th32ThreadID == g_sampler_tid) {
            continue;
        }

        DWORD access = THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION;

        HANDLE handle = OpenThread(access, FALSE, entry.th32ThreadID);

        if (handle) {
            targets.push_back({ handle, entry.th32ThreadID, 0, 0, NO_CALLERS });
        }
    } while (Thread32Next(snapshot, &entry));

    CloseHandle(snapshot);

    return targets;
}

void close_threads(std::vector <Target>& targets) {
    for (Target& target : targets) {
        CloseHandle(target.handle);
    }

    targets.clear();
}

void sample_thread(Target& target) {
    if (SuspendThread(target.handle) == (DWORD)-1) {
        return;
    }

    CONTEXT context;

    context.ContextFlags = CONTEXT_CONTROL;

    bool captured = GetThreadContext(target.handle, &context);

    int jit = profile::active_jit;

    Callers callers = {};

    bool same_as_last = false;
    bool scanned = false;

    if (captured && !is_in_image(context.Rip)) {
        if (context.Rip == target.last_ip && context.Rsp == target.last_sp) {
            same_as_last = true;
        } else {
            callers.count = scan_callers(context.Rsp, callers.frames);

            scanned = true;
        }
    }

    ResumeThread(target.handle);

    if (!captured) {
        return;
    }

    uint32_t callers_index = NO_CALLERS;

    if (same_as_last) {
        callers_index = target.last_callers;
    }

    if (scanned && callers.count) {
        callers_index = (uint32_t)g_callers.size();

        g_callers.push_back(callers);
    }

    target.last_ip = context.Rip;
    target.last_sp = context.Rsp;
    target.last_callers = callers_index;

    g_samples.push_back({ (uint64_t)context.Rip, target.tid, jit, callers_index });
}

void sampler() {
    g_sampler_tid = GetCurrentThreadId();

    std::vector <Target> targets = snapshot_threads();

    LARGE_INTEGER frequency;
    LARGE_INTEGER last_snapshot;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last_snapshot);

    timeBeginPeriod(1);

    while (g_running.load(std::memory_order_relaxed)) {
        for (Target& target : targets) {
            sample_thread(target);
        }

        Sleep(1);

        LARGE_INTEGER now;

        QueryPerformanceCounter(&now);

        if (now.QuadPart - last_snapshot.QuadPart > frequency.QuadPart) {
            close_threads(targets);

            targets = snapshot_threads();
            last_snapshot = now;
        }
    }

    timeEndPeriod(1);

    close_threads(targets);
}

std::string thread_name(uint32_t tid) {
    std::string name;

    HANDLE handle = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);

    if (!handle) {
        return name;
    }

    PWSTR description = nullptr;

    if (SUCCEEDED(GetThreadDescription(handle, &description)) && description && description[0]) {
        int size = WideCharToMultiByte(CP_UTF8, 0, description, -1, nullptr, 0, nullptr, nullptr);

        name.resize((size_t)size);

        WideCharToMultiByte(CP_UTF8, 0, description, -1, name.data(), size, nullptr, nullptr);

        name.resize(name.size() - 1);
    }

    if (description) {
        LocalFree(description);
    }

    CloseHandle(handle);

    return name;
}

std::string thread_label(uint32_t tid) {
    std::string name = thread_name(tid);

    if (tid != g_emulation_tid) {
        return name;
    }

    if (name.empty()) {
        return "emulation thread";
    }

    return name + " (emulation thread)";
}

const char* jit_name(int jit) {
    switch (jit) {
        case profile::JIT_EE: return "<jit code: ee>";
        case profile::JIT_IOP: return "<jit code: iop>";
        case profile::JIT_VU0: return "<jit code: vu0>";
        case profile::JIT_VU1: return "<jit code: vu1>";
    }

    return "<jit code>";
}

bool symbol_from_address(uint64_t address, std::string& name) {
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512];

    SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;

    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 512;

    DWORD64 displacement = 0;

    if (!SymFromAddr(GetCurrentProcess(), address, &displacement, symbol)) {
        return false;
    }

    name = symbol->Name;

    return true;
}

bool module_from_address(uint64_t address, std::string& name) {
    HMODULE module = nullptr;

    DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

    if (!GetModuleHandleExA(flags, (LPCSTR)address, &module) || !module) {
        return false;
    }

    char path[MAX_PATH];

    GetModuleFileNameA(module, path, MAX_PATH);

    const char* filename = strrchr(path, '\\');

    if (filename) {
        filename++;
    } else {
        filename = path;
    }

    name = std::string(filename) + "!<no symbols>";

    return true;
}

Symbol symbolize(uint64_t ip, int jit) {
    Symbol symbol;

    if (is_in_image(ip)) {
        symbol.kind = CodeKind::IRIS;
    } else {
        symbol.kind = CodeKind::EXTERNAL;
    }

    if (symbol_from_address(ip, symbol.name)) {
        return symbol;
    }

    if (module_from_address(ip, symbol.name)) {
        return symbol;
    }

    symbol.name = jit_name(jit);
    symbol.kind = CodeKind::GENERATED;

    return symbol;
}

const Symbol& symbol_of(SymbolCache& cache, const Sample& sample) {
    SymbolKey key = { sample.ip, sample.jit };

    auto it = cache.find(key);

    if (it == cache.end()) {
        it = cache.emplace(key, symbolize(sample.ip, sample.jit)).first;
    }

    return it->second;
}

const std::string& caller_name(CallerNameCache& cache, uint64_t return_address) {
    auto it = cache.find(return_address);

    if (it != cache.end()) {
        return it->second;
    }

    std::string name;

    if (!symbol_from_address(return_address - 1, name)) {
        name = "<unknown>";
    }

    return cache.emplace(return_address, name).first->second;
}

std::string caller_chain(CallerNameCache& cache, uint32_t callers_index) {
    if (callers_index == NO_CALLERS) {
        return "  <-  (no Iris caller found)";
    }

    const Callers& callers = g_callers[callers_index];

    std::string chain;
    std::string previous;

    for (int index = 0; index < callers.count; index++) {
        const std::string& name = caller_name(cache, callers.frames[index]);

        if (name == previous) {
            continue;
        }

        chain += "  <-  " + name;
        previous = name;
    }

    return chain;
}

bool is_kernel_call(const std::string& name) {
    return name.starts_with("Zw") || name.starts_with("Nt");
}

bool is_gpu_wait(const std::string& name) {
    return name.starts_with("NtGdiDdDDI");
}

bool is_idle(const std::string& name) {
    static const char* const waits[] = {
        "Wait",
        "DelayExecution",
        "GetMessage",
        "SleepEx",
        "RemoveIoCompletion",
        "DeviceIoControlFile"
    };

    if (!is_kernel_call(name)) {
        return false;
    }

    if (is_gpu_wait(name)) {
        return false;
    }

    for (const char* wait : waits) {
        if (name.find(wait) != std::string::npos) {
            return true;
        }
    }

    return false;
}

template <typename T>
bool has_more_samples(const std::pair <T, uint64_t>& a, const std::pair <T, uint64_t>& b) {
    return a.second > b.second;
}

std::string executable_directory() {
    char path[MAX_PATH];

    GetModuleFileNameA(nullptr, path, MAX_PATH);

    std::string directory = path;

    directory.resize(directory.find_last_of('\\'));

    return directory;
}

double percent(uint64_t part, uint64_t whole) {
    if (!whole) {
        return 0.0;
    }

    return 100.0 * (double)part / (double)whole;
}

void print_counter_ratio(FILE* out, const char* label, int compiled_counter, int interpreted_counter) {
    uint64_t compiled = profile::counters[compiled_counter];
    uint64_t interpreted = profile::counters[interpreted_counter];

    if (!compiled && !interpreted) {
        return;
    }

    fprintf(out, "counters: %s %.1f%%\n", label, percent(compiled, compiled + interpreted));
}

void print_counters(FILE* out, uint64_t frames) {
    fprintf(out, "counters: per emulated frame over %llu frames, totals in brackets\n", (unsigned long long)frames);

    for (int index = 0; index < profile::COUNTER_COUNT; index++) {
        uint64_t total = profile::counters[index];

        if (!total) {
            continue;
        }

        double per_frame = 0.0;

        if (frames) {
            per_frame = (double)total / (double)frames;
        }

        fprintf(out, "counters: %14.1f  %-42s (%llu)\n",
            per_frame,
            profile::counter_name(index),
            (unsigned long long)total
        );
    }

    print_counter_ratio(out, "vu0 entries run compiled", profile::VU0_ENTRIES_COMPILED_RUN, profile::VU0_ENTRIES_INTERPRETED_RUN);
    print_counter_ratio(out, "vu1 entries run compiled", profile::VU1_ENTRIES_COMPILED_RUN, profile::VU1_ENTRIES_INTERPRETED_RUN);

    fprintf(out, "\n");
}

std::vector <std::pair <uint32_t, uint64_t>> threads_by_busy(const std::unordered_map <uint32_t, ThreadProfile>& threads) {
    std::vector <std::pair <uint32_t, uint64_t>> sorted;

    for (const auto& [tid, thread] : threads) {
        if (thread.busy) {
            sorted.push_back({ tid, thread.busy });
        }
    }

    std::sort(sorted.begin(), sorted.end(), has_more_samples <uint32_t>);

    return sorted;
}

void print_threads(FILE* out, const std::unordered_map <uint32_t, ThreadProfile>& threads, uint64_t busy) {
    for (const auto& [tid, samples] : threads_by_busy(threads)) {
        double share_of_busy = percent(samples, busy);

        if (share_of_busy < 0.5) {
            continue;
        }

        const ThreadProfile& thread = threads.at(tid);

        std::string label = thread_label(tid);

        fprintf(out, "profile: thread %6u %6.2f%% of busy, %5.1f%% of its own time working  %s\n",
            tid,
            share_of_busy,
            percent(samples, thread.seen),
            label.c_str()
        );
    }

    fprintf(out, "\n");
}

void print_function_list(FILE* out, const FunctionCounts& functions, uint64_t total, int top) {
    std::vector <std::pair <std::string, uint64_t>> sorted(functions.begin(), functions.end());

    std::sort(sorted.begin(), sorted.end(), has_more_samples <std::string>);

    int shown = 0;

    for (const auto& [name, samples] : sorted) {
        if (shown == top) {
            break;
        }

        fprintf(out, "profile: %6.2f%%  %s\n", percent(samples, total), name.c_str());

        shown++;
    }

    fprintf(out, "\n");
}

void print_emulation_thread(FILE* out, const std::unordered_map <uint32_t, ThreadProfile>& threads, int top) {
    auto it = threads.find(g_emulation_tid);

    if (it == threads.end() || !it->second.busy) {
        fprintf(out, "profile: the emulation thread was never seen working\n\n");

        return;
    }

    const ThreadProfile& thread = it->second;

    fprintf(out, "profile: emulation thread %u, top %d functions, %% of its busy samples\n", g_emulation_tid, top);

    print_function_list(out, thread.functions, thread.busy, top);

    if (thread.external_by_caller.empty()) {
        return;
    }

    fprintf(out, "profile: emulation thread %u, system and driver code by nearest Iris callers (stack scan, approximate), %% of its busy samples\n", g_emulation_tid);

    print_function_list(out, thread.external_by_caller, thread.busy, top);
}

void print_other_threads(FILE* out, const std::unordered_map <uint32_t, ThreadProfile>& threads, uint64_t busy) {
    for (const auto& [tid, samples] : threads_by_busy(threads)) {
        if (tid == g_emulation_tid) {
            continue;
        }

        if (percent(samples, busy) < OTHER_THREAD_MIN_SHARE) {
            continue;
        }

        const ThreadProfile& thread = threads.at(tid);

        std::string label = thread_label(tid);

        fprintf(out, "profile: thread %u %s, top %d functions, %% of its busy samples\n", tid, label.c_str(), OTHER_THREAD_TOP);

        print_function_list(out, thread.functions, thread.busy, OTHER_THREAD_TOP);
    }
}

}

void start() {
    if (g_running.load()) {
        return;
    }

    g_emulation_tid = GetCurrentThreadId();

    find_image_ranges();

    profile::reset_counters();

    g_samples.clear();
    g_samples.reserve(1 << 20);

    g_callers.clear();
    g_callers.reserve(1 << 16);

    g_running = true;

    g_thread = std::thread(sampler);
}

bool is_running() {
    return g_running.load();
}

void stop_and_report(FILE* out, int top, uint64_t frames) {
    if (!g_running.load()) {
        return;
    }

    g_running = false;

    g_thread.join();

    print_counters(out, frames);

    if (g_samples.empty()) {
        fprintf(out, "profile: no samples\n");

        return;
    }

    HANDLE process = GetCurrentProcess();

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, executable_directory().c_str(), TRUE);

    SymbolCache symbols;
    CallerNameCache caller_names;

    std::unordered_map <uint32_t, ThreadProfile> threads;

    uint64_t busy = 0;

    for (const Sample& sample : g_samples) {
        const Symbol& symbol = symbol_of(symbols, sample);

        ThreadProfile& thread = threads[sample.tid];

        thread.seen++;

        if (is_idle(symbol.name)) {
            continue;
        }

        busy++;

        thread.busy++;
        thread.functions[symbol.name]++;

        if (symbol.kind == CodeKind::EXTERNAL && sample.tid == g_emulation_tid) {
            std::string key = symbol.name + caller_chain(caller_names, sample.callers);

            thread.external_by_caller[key]++;
        }
    }

    fprintf(out, "profile: %llu samples, %llu busy (%.1f%% of the process was working)\n",
        (unsigned long long)g_samples.size(),
        (unsigned long long)busy,
        percent(busy, g_samples.size())
    );

    if (busy) {
        print_threads(out, threads, busy);
        print_emulation_thread(out, threads, top);
        print_other_threads(out, threads, busy);
    }

    SymCleanup(process);

    g_samples.clear();
    g_samples.shrink_to_fit();

    g_callers.clear();
    g_callers.shrink_to_fit();
}

}

#else

namespace iris::profiler {

void start() {
}

void stop_and_report(FILE* out, int top, uint64_t frames) {
}

bool is_running() {
    return false;
}

}

#endif
