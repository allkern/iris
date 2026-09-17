#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#include "gs_async.hpp"

#include "profile_counters.hpp"
#include "profile_tag.hpp"

namespace iris::gs::async {

constexpr uint32_t RING_WORDS = 1u << 23;
constexpr uint32_t DIRECT_LIMIT_WORDS = RING_WORDS / 4;
constexpr uint32_t HEADER_WORDS = 3;
constexpr uint32_t WRAP_MARKER = 0xffffffff;
constexpr int SPIN_ITERATIONS = 20000;

struct Async {
    void* backend_udata = nullptr;
    TransferFunc backend_transfer = nullptr;

    std::vector <uint32_t> ring;

    alignas(64) std::atomic <uint32_t> write_pos = 0;
    alignas(64) std::atomic <uint32_t> read_pos = 0;

    alignas(64) std::atomic <bool> sleeping = false;
    std::atomic <bool> stopping = false;
    std::atomic <bool> waiting_for_thread = false;

    bool running = false;

    std::thread thread;
    std::mutex wake_mutex;
    std::condition_variable wake_cv;
    std::mutex idle_mutex;
    std::condition_variable idle_cv;
    fenv_t fp_env;
    int rounding = FE_TONEAREST;
};

static inline void pause_briefly() {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#else
    std::this_thread::yield();
#endif
}

static inline uint32_t words_for_bytes(size_t size) {
    return (uint32_t)((size + 3) / 4);
}

Async* create() {
    Async* async = new Async();

    async->ring.resize(RING_WORDS);

    return async;
}

static inline bool has_work(Async* async) {
    return async->read_pos.load(std::memory_order_relaxed) != async->write_pos.load();
}

bool is_idle(Async* async) {
    return async->read_pos.load() == async->write_pos.load();
}

static inline void call_backend(Async* async, int path, const void* data, size_t size, uint32_t flags) {
    if (!async->backend_transfer) {
        return;
    }

    async->backend_transfer(async->backend_udata, path, data, size, flags);
}

static inline uint32_t process_transfer(Async* async, uint32_t read) {
    const uint32_t* header = &async->ring[read];

    if (header[0] == WRAP_MARKER) {
        return 0;
    }

    int path = (int)header[0];
    size_t size = header[1];
    uint32_t flags = header[2];

    call_backend(async, path, &header[HEADER_WORDS], size, flags);

    return read + HEADER_WORDS + words_for_bytes(size);
}

static void process(Async* async) {
    uint32_t read = async->read_pos.load(std::memory_order_relaxed);
    uint32_t write = async->write_pos.load();

    while (read != write) {
        while (read != write) {
            read = process_transfer(async, read);
        }

        async->read_pos.store(read);

        write = async->write_pos.load();
    }
}

static void notify_waiters(Async* async) {
    if (!async->waiting_for_thread.load()) {
        return;
    }

    std::lock_guard <std::mutex> lock(async->idle_mutex);

    async->idle_cv.notify_all();
}

static bool spin_for_work(Async* async) {
    for (int i = 0; i < SPIN_ITERATIONS; i++) {
        if (has_work(async)) {
            return true;
        }

        pause_briefly();
    }

    return false;
}

static void sleep_until_work(Async* async) {
    std::unique_lock <std::mutex> lock(async->wake_mutex);

    async->sleeping.store(true);

    while (!has_work(async) && !async->stopping.load()) {
        async->wake_cv.wait(lock);
    }

    async->sleeping.store(false);
}

static void thread_main(Async* async) {
    profile::name_this_thread(L"GS thread");

    fesetenv(&async->fp_env);
    fesetround(async->rounding);

    while (!async->stopping.load()) {
        if (has_work(async)) {
            process(async);

            notify_waiters(async);

            continue;
        }

        if (spin_for_work(async)) {
            continue;
        }

        sleep_until_work(async);
    }
}

static void wake_thread(Async* async) {
    if (!async->sleeping.load()) {
        return;
    }

    std::lock_guard <std::mutex> lock(async->wake_mutex);

    async->wake_cv.notify_one();
}

void start(Async* async, const fenv_t& fp_env, int rounding) {
    if (async->running) {
        return;
    }

    async->fp_env = fp_env;
    async->rounding = rounding;
    async->running = true;

    async->thread = std::thread(thread_main, async);
}

static void stop(Async* async) {
    if (!async->running) {
        return;
    }

    async->stopping.store(true);

    std::unique_lock <std::mutex> lock(async->wake_mutex);

    async->wake_cv.notify_one();

    lock.unlock();

    async->thread.join();

    async->running = false;
}

void destroy(Async* async) {
    stop(async);

    delete async;
}

void sync(Async* async) {
    if (is_idle(async)) {
        return;
    }

    wake_thread(async);

    for (int i = 0; i < SPIN_ITERATIONS; i++) {
        if (is_idle(async)) {
            return;
        }

        pause_briefly();
    }

    std::unique_lock <std::mutex> lock(async->idle_mutex);

    async->waiting_for_thread.store(true);

    while (!is_idle(async)) {
        async->idle_cv.wait_for(lock, std::chrono::milliseconds(1));
    }

    async->waiting_for_thread.store(false);
}

void set_backend(Async* async, void* udata, TransferFunc transfer) {
    sync(async);

    async->backend_udata = udata;
    async->backend_transfer = transfer;
}

static void wait_for_space(Async* async) {
    profile::count(profile::GS_ASYNC_RING_FULL_WAITS);

    wake_thread(async);

    for (int i = 0; i < SPIN_ITERATIONS; i++) {
        pause_briefly();
    }

    std::this_thread::yield();
}

static uint32_t reserve_words(Async* async, uint32_t words) {
    while (true) {
        uint32_t write = async->write_pos.load(std::memory_order_relaxed);
        uint32_t read = async->read_pos.load();

        if (write >= read) {
            if (write + words < RING_WORDS) {
                return write;
            }

            if (words < read) {
                async->ring[write] = WRAP_MARKER;
                async->write_pos.store(0);

                return 0;
            }
        } else {
            if (write + words < read) {
                return write;
            }
        }

        wait_for_space(async);
    }
}

void transfer(Async* async, int path, const void* data, size_t size, uint32_t flags) {
    uint32_t words = words_for_bytes(size) + HEADER_WORDS;

    if (!async->running || words > DIRECT_LIMIT_WORDS) {
        sync(async);

        call_backend(async, path, data, size, flags);

        return;
    }

    uint32_t start = reserve_words(async, words);
    uint32_t* header = &async->ring[start];

    header[0] = (uint32_t)path;
    header[1] = (uint32_t)size;
    header[2] = flags;
    header[words - 1] = 0;

    memcpy(&header[HEADER_WORDS], data, size);

    async->write_pos.store(start + words);

    profile::count(profile::GS_ASYNC_TRANSFERS);

    wake_thread(async);
}

}
