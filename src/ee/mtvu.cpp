#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fenv.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#include "mtvu.hpp"
#include "vu.hpp"
#include "vu_def.hpp"
#include "vif.hpp"
#include "gif.hpp"
#include "bus.hpp"
#include "gs/gs.hpp"
#include "gs/gs_async.hpp"

#include "profile_counters.hpp"
#include "profile_tag.hpp"

namespace iris::mtvu {

constexpr uint32_t RING_WORDS = 1u << 22;
constexpr uint32_t CHUNK_WORDS = 1u << 16;
constexpr uint32_t STAGE_LIMIT_WORDS = 1u << 12;
constexpr uint32_t STAGE_SPAN_WORDS = 64;
constexpr int SPIN_ITERATIONS = 20000;

enum Op : uint32_t {
    OP_WRAP,
    OP_VIF_WORDS,
    OP_VIF_FBRST,
    OP_GIF_QWORDS,
    OP_GIF_WRITE32,
    OP_VU1_EXECUTE,
    OP_VU1_RESET
};

enum Stage : int {
    STAGE_NONE,
    STAGE_VIF,
    STAGE_GIF
};

struct GsEvent {
    int type;
    uint64_t data;
};

struct LogEntry {
    logger::Level level;
    size_t source;
    std::string text;
};

struct Mtvu {
    struct {
        vu::Vu* vu0;
        vu::Vu* vu1;
        vif::Vif* vif1;
        gif::Gif* gif;
        gs::Gs* gs;
        ee::bus::Bus* bus;
    } hw;

    int mode;

    vif::Vif* vif;
    gif::Gif* gif;
    gs::async::Async* gs_async = nullptr;

    void* backend_udata = nullptr;
    void (*backend_transfer)(void*, int, const void*, size_t) = nullptr;
    void (*backend_readback)(void*, void*, size_t) = nullptr;

    bool front_scan = false;
    bool processing_front_gif = false;
    std::atomic <uint32_t> events_kept = 1;

    std::vector <uint32_t> ring;

    alignas(64) std::atomic <uint32_t> write_pos;
    bool published;
    int staged_kind;
    int staged_path;
    uint32_t staged_words;
    uint32_t staged[STAGE_LIMIT_WORDS];

    alignas(64) std::atomic <uint32_t> read_pos;

    alignas(64) std::atomic <bool> sleeping;
    std::atomic <bool> stopping;
    std::atomic <bool> waiting_for_worker;

    std::thread worker;
    std::mutex wake_mutex;
    std::condition_variable wake_cv;
    std::mutex idle_mutex;
    std::condition_variable idle_cv;
    fenv_t fp_env;

    std::mutex events_mutex;
    std::vector <GsEvent> events;
    std::vector <GsEvent> applying;
    std::atomic <uint32_t> events_queued;

    logger::Logger* worker_logger = nullptr;
    std::mutex logs_mutex;
    std::vector <LogEntry> logs;
    std::vector <LogEntry> flushing;
    std::atomic <uint32_t> logs_queued;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

static const char* mode_name(int mode) {
    switch (mode) {
        case MODE_INLINE: return "inline";
        case MODE_THREAD: return "thread";
        case MODE_STRICT: return "strict";
    }

    return "off";
}

Mtvu* create(logger::Logger* logger) {
    Mtvu* mtvu = new Mtvu();

    mtvu->logger = logger;
    mtvu->logger_id = logger::register_source(logger, "mtvu");

    mtvu->mode = MODE_OFF;

    if (mtvu->mode == MODE_OFF) {
        return mtvu;
    }

    mtvu->vif = vif::create(logger, 1, nullptr, nullptr);
    mtvu->gif = gif::create(logger);

    mtvu->ring.resize(RING_WORDS);

    if (mtvu->mode == MODE_THREAD || mtvu->mode == MODE_STRICT) {
        mtvu->gs_async = gs::async::create();
    }

    return mtvu;
}

static inline void pause_briefly() {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#else
    std::this_thread::yield();
#endif
}

static void queue_gs_event(void* udata, int type, uint64_t data) {
    Mtvu* mtvu = (Mtvu*)udata;

    if (!mtvu->events_kept.load(std::memory_order_relaxed)) {
        return;
    }

    std::lock_guard <std::mutex> lock(mtvu->events_mutex);

    mtvu->events.push_back({ type, data });
    mtvu->events_queued.fetch_add(1);
}

static void apply_gs_events(Mtvu* mtvu) {
    if (!mtvu->events_queued.load()) {
        return;
    }

    std::unique_lock <std::mutex> lock(mtvu->events_mutex);

    mtvu->applying.swap(mtvu->events);
    mtvu->events_queued.store(0);

    lock.unlock();

    for (const GsEvent& event : mtvu->applying) {
        switch (event.type) {
            case gs::SIGNAL_EVENT_SIGNAL: {
                gs::apply_signal(mtvu->hw.gs, event.data);
            } break;

            case gs::SIGNAL_EVENT_FINISH: {
                gs::apply_finish(mtvu->hw.gs, event.data);
            } break;

            case gs::SIGNAL_EVENT_LABEL: {
                gs::apply_label(mtvu->hw.gs, event.data);
            } break;
        }
    }

    mtvu->applying.clear();
}

static void queue_worker_log(void* udata, logger::Level level, const logger::Source& source, const std::string& text) {
    Mtvu* mtvu = (Mtvu*)udata;

    std::lock_guard <std::mutex> lock(mtvu->logs_mutex);

    mtvu->logs.push_back({ level, source.id, text });
    mtvu->logs_queued.fetch_add(1);
}

static void flush_worker_logs(Mtvu* mtvu) {
    if (!mtvu->logs_queued.load()) {
        return;
    }

    std::unique_lock <std::mutex> lock(mtvu->logs_mutex);

    mtvu->flushing.swap(mtvu->logs);
    mtvu->logs_queued.store(0);

    lock.unlock();

    const std::vector <logger::Source>& sources = logger::get_sources(mtvu->logger);
    const std::vector <logger::Callback>& callbacks = logger::get_callbacks(mtvu->logger);

    for (const LogEntry& entry : mtvu->flushing) {
        for (const logger::Callback& callback : callbacks) {
            callback.func(callback.udata, entry.level, sources[entry.source], entry.text);
        }
    }

    mtvu->flushing.clear();
}

static inline uint32_t process_op(Mtvu* mtvu, uint32_t read) {
    const uint32_t* op = &mtvu->ring[read];

    switch (op[0]) {
        case OP_WRAP: {
            return 0;
        }

        case OP_VIF_WORDS: {
            uint32_t words = op[1];

            vif::write_words(mtvu->vif, (const uint8_t*)&op[2], words);

            return read + 2 + words;
        }

        case OP_VIF_FBRST: {
            vif::reset_command_state(mtvu->vif, op[1]);

            return read + 2;
        }

        case OP_GIF_QWORDS: {
            int path = (int)op[1];
            uint32_t qwords = op[2];

            mtvu->processing_front_gif = true;

            gif::fifo_write_qwords(mtvu->gif, (const uint8_t*)&op[3], qwords, path);

            mtvu->processing_front_gif = false;

            return read + 3 + qwords * 4;
        }

        case OP_GIF_WRITE32: {
            gif::write32(mtvu->gif, op[1], op[2]);

            return read + 3;
        }

        case OP_VU1_EXECUTE: {
            vu::execute_program(mtvu->hw.vu1, op[1]);

            return read + 2;
        }

        case OP_VU1_RESET: {
            vu::reset_registers(mtvu->hw.vu1);

            return read + 1;
        }
    }

    return read + 1;
}

static void process(Mtvu* mtvu) {
    uint32_t read = mtvu->read_pos.load(std::memory_order_relaxed);
    uint32_t write = mtvu->write_pos.load();

    while (read != write) {
        while (read != write) {
            read = process_op(mtvu, read);
        }

        mtvu->read_pos.store(read);

        write = mtvu->write_pos.load();
    }
}

static inline bool has_work(Mtvu* mtvu) {
    return mtvu->read_pos.load(std::memory_order_relaxed) != mtvu->write_pos.load();
}

static inline bool worker_is_idle(Mtvu* mtvu) {
    return mtvu->read_pos.load() == mtvu->write_pos.load();
}

static void notify_waiters(Mtvu* mtvu) {
    if (!mtvu->waiting_for_worker.load()) {
        return;
    }

    std::lock_guard <std::mutex> lock(mtvu->idle_mutex);

    mtvu->idle_cv.notify_all();
}

static bool spin_for_work(Mtvu* mtvu) {
    for (int i = 0; i < SPIN_ITERATIONS; i++) {
        if (has_work(mtvu)) {
            return true;
        }

        pause_briefly();
    }

    return false;
}

static void sleep_until_work(Mtvu* mtvu) {
    std::unique_lock <std::mutex> lock(mtvu->wake_mutex);

    mtvu->sleeping.store(true);

    while (!has_work(mtvu) && !mtvu->stopping.load()) {
        mtvu->wake_cv.wait(lock);
    }

    mtvu->sleeping.store(false);
}

static void worker_main(Mtvu* mtvu) {
    profile::name_this_thread(L"MTVU worker");

    fesetenv(&mtvu->fp_env);
    fesetround(FE_TOWARDZERO);

    while (!mtvu->stopping.load()) {
        if (has_work(mtvu)) {
            process(mtvu);

            notify_waiters(mtvu);

            continue;
        }

        if (spin_for_work(mtvu)) {
            continue;
        }

        sleep_until_work(mtvu);
    }
}

static void wake_worker(Mtvu* mtvu) {
    if (!mtvu->sleeping.load()) {
        return;
    }

    std::lock_guard <std::mutex> lock(mtvu->wake_mutex);

    mtvu->wake_cv.notify_one();
}

static void wake_if_published(Mtvu* mtvu) {
    if (!mtvu->published) {
        return;
    }

    mtvu->published = false;

    wake_worker(mtvu);
}

static void wait_for_worker(Mtvu* mtvu) {
    mtvu->published = false;

    if (worker_is_idle(mtvu)) {
        return;
    }

    wake_worker(mtvu);

    for (int i = 0; i < SPIN_ITERATIONS; i++) {
        if (worker_is_idle(mtvu)) {
            return;
        }

        pause_briefly();
    }

    std::unique_lock <std::mutex> lock(mtvu->idle_mutex);

    mtvu->waiting_for_worker.store(true);

    while (!worker_is_idle(mtvu)) {
        mtvu->idle_cv.wait_for(lock, std::chrono::milliseconds(1));
    }

    mtvu->waiting_for_worker.store(false);
}

static void catch_up_published(Mtvu* mtvu) {
    if (mtvu->mode == MODE_INLINE) {
        process(mtvu);

        return;
    }

    wait_for_worker(mtvu);
}

static uint32_t reserve_words(Mtvu* mtvu, uint32_t words) {
    while (true) {
        uint32_t write = mtvu->write_pos.load(std::memory_order_relaxed);
        uint32_t read = mtvu->read_pos.load();

        if (write >= read) {
            if (write + words < RING_WORDS) {
                return write;
            }

            if (words < read) {
                mtvu->ring[write] = OP_WRAP;
                mtvu->write_pos.store(0);

                return 0;
            }
        } else {
            if (write + words < read) {
                return write;
            }
        }

        profile::count(profile::MTVU_RING_FULL_WAITS);

        catch_up_published(mtvu);
    }
}

static void publish(Mtvu* mtvu, uint32_t start, uint32_t words) {
    mtvu->write_pos.store(start + words);

    mtvu->published = true;

    profile::count(profile::MTVU_OPS_PUBLISHED);
}

static void write_vif_op(Mtvu* mtvu, const uint8_t* data, uint32_t words) {
    while (words) {
        uint32_t chunk = words;

        if (chunk > CHUNK_WORDS) {
            chunk = CHUNK_WORDS;
        }

        uint32_t start = reserve_words(mtvu, chunk + 2);
        uint32_t* op = &mtvu->ring[start];

        op[0] = OP_VIF_WORDS;
        op[1] = chunk;

        memcpy(&op[2], data, (size_t)chunk * 4);

        publish(mtvu, start, chunk + 2);

        data += (size_t)chunk * 4;
        words -= chunk;
    }
}

static void write_gif_op(Mtvu* mtvu, int path, const uint8_t* data, uint32_t qwords) {
    uint32_t chunk_qwords = CHUNK_WORDS / 4;

    while (qwords) {
        uint32_t chunk = qwords;

        if (chunk > chunk_qwords) {
            chunk = chunk_qwords;
        }

        uint32_t start = reserve_words(mtvu, chunk * 4 + 3);
        uint32_t* op = &mtvu->ring[start];

        op[0] = OP_GIF_QWORDS;
        op[1] = (uint32_t)path;
        op[2] = chunk;

        memcpy(&op[3], data, (size_t)chunk * 16);

        publish(mtvu, start, chunk * 4 + 3);

        data += (size_t)chunk * 16;
        qwords -= chunk;
    }
}

static void flush_staged(Mtvu* mtvu) {
    switch (mtvu->staged_kind) {
        case STAGE_VIF: {
            write_vif_op(mtvu, (const uint8_t*)mtvu->staged, mtvu->staged_words);
        } break;

        case STAGE_GIF: {
            write_gif_op(mtvu, mtvu->staged_path, (const uint8_t*)mtvu->staged, mtvu->staged_words / 4);
        } break;
    }

    mtvu->staged_kind = STAGE_NONE;
    mtvu->staged_words = 0;
}

static void stage_words(Mtvu* mtvu, int kind, int path, const uint8_t* data, uint32_t words) {
    bool same_stream = mtvu->staged_kind == kind && mtvu->staged_path == path;

    if (!same_stream || mtvu->staged_words + words > STAGE_LIMIT_WORDS) {
        flush_staged(mtvu);
    }

    memcpy(&mtvu->staged[mtvu->staged_words], data, (size_t)words * 4);

    mtvu->staged_words += words;
    mtvu->staged_kind = kind;
    mtvu->staged_path = path;
}

static void drain(Mtvu* mtvu) {
    flush_staged(mtvu);

    catch_up_published(mtvu);

    if (mtvu->gs_async) {
        gs::async::sync(mtvu->gs_async);
    }

    apply_gs_events(mtvu);
    flush_worker_logs(mtvu);
}

static void finish_push(Mtvu* mtvu) {
    switch (mtvu->mode) {
        case MODE_INLINE: {
            drain(mtvu);
        } break;

        case MODE_THREAD: {
            wake_if_published(mtvu);
        } break;

        case MODE_STRICT: {
            drain(mtvu);
        } break;
    }
}

void push_vif_words(Mtvu* mtvu, const uint8_t* data, uint32_t words) {
    if (words < STAGE_SPAN_WORDS) {
        stage_words(mtvu, STAGE_VIF, 0, data, words);
    } else {
        flush_staged(mtvu);

        write_vif_op(mtvu, data, words);
    }

    finish_push(mtvu);
}

void push_vif_fbrst(Mtvu* mtvu, uint32_t data) {
    flush_staged(mtvu);

    uint32_t start = reserve_words(mtvu, 2);
    uint32_t* op = &mtvu->ring[start];

    op[0] = OP_VIF_FBRST;
    op[1] = data;

    publish(mtvu, start, 2);

    finish_push(mtvu);
}

void push_gif_qwords(Mtvu* mtvu, int path, const uint8_t* data, uint32_t qwords) {
    if (qwords * 4 < STAGE_SPAN_WORDS) {
        stage_words(mtvu, STAGE_GIF, path, data, qwords * 4);
    } else {
        flush_staged(mtvu);

        write_gif_op(mtvu, path, data, qwords);
    }

    finish_push(mtvu);
}

void push_gif_write32(Mtvu* mtvu, uint32_t addr, uint32_t data) {
    flush_staged(mtvu);

    uint32_t start = reserve_words(mtvu, 3);
    uint32_t* op = &mtvu->ring[start];

    op[0] = OP_GIF_WRITE32;
    op[1] = addr;
    op[2] = data;

    publish(mtvu, start, 3);

    finish_push(mtvu);
}

void push_vu1_execute(Mtvu* mtvu, uint32_t addr) {
    flush_staged(mtvu);

    uint32_t start = reserve_words(mtvu, 2);
    uint32_t* op = &mtvu->ring[start];

    op[0] = OP_VU1_EXECUTE;
    op[1] = addr;

    publish(mtvu, start, 2);

    finish_push(mtvu);
}

void push_vu1_reset(Mtvu* mtvu) {
    flush_staged(mtvu);

    uint32_t start = reserve_words(mtvu, 1);
    uint32_t* op = &mtvu->ring[start];

    op[0] = OP_VU1_RESET;

    publish(mtvu, start, 1);

    finish_push(mtvu);
}

static profile::Counter sync_wait_counter(SyncReason reason) {
    switch (reason) {
        case SYNC_FRAME: return profile::MTVU_FRAME_SYNC_WAITS;
        case SYNC_VU1_MEMORY: return profile::MTVU_VU1_MEMORY_SYNC_WAITS;
        case SYNC_VU1_REGISTERS: return profile::MTVU_VU1_REGISTER_SYNC_WAITS;
        case SYNC_VIF1_ROW: return profile::MTVU_VIF1_ROW_SYNC_WAITS;
        case SYNC_GS_REGISTERS: return profile::MTVU_GS_REGISTER_SYNC_WAITS;
        case SYNC_EE_IDLE: return profile::MTVU_EE_IDLE_SYNC_WAITS;
        case SYNC_OTHER: return profile::MTVU_OTHER_SYNC_WAITS;
    }

    return profile::MTVU_OTHER_SYNC_WAITS;
}

static bool pipeline_is_idle(Mtvu* mtvu) {
    if (!worker_is_idle(mtvu)) {
        return false;
    }

    if (mtvu->gs_async && !gs::async::is_idle(mtvu->gs_async)) {
        return false;
    }

    return true;
}

void sync(Mtvu* mtvu, SyncReason reason) {
    if (mtvu->mode == MODE_OFF) {
        return;
    }

    flush_staged(mtvu);

    if (mtvu->mode != MODE_INLINE && !pipeline_is_idle(mtvu)) {
        profile::count(sync_wait_counter(reason));
    }

    drain(mtvu);
}

void poll(Mtvu* mtvu) {
    if (mtvu->mode == MODE_OFF) {
        return;
    }

    if (mtvu->mode == MODE_THREAD) {
        flush_staged(mtvu);

        wake_if_published(mtvu);
    }

    apply_gs_events(mtvu);
    flush_worker_logs(mtvu);
}

uint32_t read_vif1_row(Mtvu* mtvu, int index) {
    sync(mtvu, SYNC_VIF1_ROW);

    return mtvu->vif->r[index];
}

uint32_t take_gif_fifo_activity(Mtvu* mtvu) {
    return mtvu->gif->fifo_activity.exchange(0);
}

static bool transfer_keeps_events(Mtvu* mtvu, int path) {
    if (!mtvu->front_scan) {
        return true;
    }

    if (gif::flushing_deferred_path3(mtvu->gif)) {
        return true;
    }

    return path == gif::PATH1 && !mtvu->processing_front_gif;
}

static void run_backend_transfer(Mtvu* mtvu, int path, const void* data, size_t size, uint32_t flags) {
    mtvu->events_kept.store(flags, std::memory_order_relaxed);

    if (!mtvu->backend_transfer) {
        return;
    }

    mtvu->backend_transfer(mtvu->backend_udata, path, data, size);
}

static void gs_thread_transfer(void* udata, int path, const void* data, size_t size, uint32_t flags) {
    run_backend_transfer((Mtvu*)udata, path, data, size, flags);
}

static void worker_gif_transfer(void* udata, int path, const void* data, size_t size) {
    Mtvu* mtvu = (Mtvu*)udata;

    uint32_t flags = transfer_keeps_events(mtvu, path) ? 1 : 0;

    if (mtvu->gs_async) {
        gs::async::transfer(mtvu->gs_async, path, data, size, flags);

        return;
    }

    run_backend_transfer(mtvu, path, data, size, flags);
}

static void worker_gif_readback(void* udata, void* data, size_t size) {
    Mtvu* mtvu = (Mtvu*)udata;

    if (!mtvu->backend_readback) {
        return;
    }

    mtvu->backend_readback(mtvu->backend_udata, data, size);
}

void update_gif_backend(Mtvu* mtvu) {
    sync(mtvu, SYNC_OTHER);

    gif::Gif* front = mtvu->hw.gif;

    mtvu->backend_udata = front->udata;
    mtvu->backend_transfer = front->transfer;
    mtvu->backend_readback = front->readback;

    if (mtvu->gs_async) {
        gs::async::set_backend(mtvu->gs_async, mtvu, gs_thread_transfer);
    }

    gif::set_backend(mtvu->gif, mtvu, worker_gif_transfer, worker_gif_readback);
    gif::set_dump_tap(mtvu->gif, front->dump_udata, front->dump_transfer);
}

static void start_worker(Mtvu* mtvu) {
    if (mtvu->logger) {
        mtvu->worker_logger = logger::create();
        mtvu->worker_logger->sources = logger::get_sources(mtvu->logger);
        mtvu->worker_logger->level = logger::get_level(mtvu->logger);

        logger::register_callback(mtvu->worker_logger, queue_worker_log, mtvu);

        mtvu->vif->logger = mtvu->worker_logger;
        mtvu->gif->logger = mtvu->worker_logger;
        mtvu->hw.vu1->logger = mtvu->worker_logger;
    }

    fegetenv(&mtvu->fp_env);

    if (mtvu->gs_async) {
        gs::async::start(mtvu->gs_async, mtvu->fp_env, FE_TOWARDZERO);
    }

    mtvu->worker = std::thread(worker_main, mtvu);
}

static void stop_worker(Mtvu* mtvu) {
    if (!mtvu->worker.joinable()) {
        return;
    }

    mtvu->stopping.store(true);

    std::unique_lock <std::mutex> lock(mtvu->wake_mutex);

    mtvu->wake_cv.notify_one();

    lock.unlock();

    mtvu->worker.join();
}

void connect(Mtvu* mtvu, vu::Vu* vu0, vu::Vu* vu1, vif::Vif* vif1, gif::Gif* gif, gs::Gs* gs, ee::bus::Bus* bus) {
    mtvu->hw.vu0 = vu0;
    mtvu->hw.vu1 = vu1;
    mtvu->hw.vif1 = vif1;
    mtvu->hw.gif = gif;
    mtvu->hw.gs = gs;
    mtvu->hw.bus = bus;

    if (mtvu->mode == MODE_OFF) {
        return;
    }

    if (gif->p3_stall_enable) {
        iris_warning(mtvu, "IRIS_PATH3_STALL does not work with MTVU, turning it off");

        gif->p3_stall_enable = 0;
    }

    mtvu->vif->hw.vu = vu1;
    mtvu->vif->hw.gif = mtvu->gif;
    mtvu->vif->role = vif::VIF_ROLE_WORKER;

    mtvu->gif->hw.gs = gs;
    mtvu->gif->hw.vu1 = vu1;
    mtvu->gif->p3_stall_enable = 0;
    mtvu->gif->report_fifo_activity = true;

    vif1->hw.mtvu = mtvu;
    vif1->role = vif::VIF_ROLE_FRONT;

    gif->hw.mtvu = mtvu;

    vu0->mtvu = mtvu;
    vu1->gif = mtvu->gif;
    vu1->vif = mtvu->vif;

    bus->mtvu = mtvu;

    gs::set_event_sink(gs, queue_gs_event, mtvu);

    if (mtvu->mode == MODE_THREAD) {
        mtvu->front_scan = true;

        gif::enable_front_scan(gif);
    }

    update_gif_backend(mtvu);

    if (mtvu->mode == MODE_THREAD || mtvu->mode == MODE_STRICT) {
        start_worker(mtvu);
    }

    iris_info(mtvu, "MTVU enabled in {} mode", mode_name(mtvu->mode));

    if (mtvu->gs_async) {
        iris_info(mtvu, "GS thread enabled");
    }
}

void reset(Mtvu* mtvu) {
    if (mtvu->mode == MODE_OFF) {
        return;
    }

    sync(mtvu, SYNC_OTHER);

    vif::reset(mtvu->vif);
    gif::reset(mtvu->gif);

    std::lock_guard <std::mutex> lock(mtvu->events_mutex);

    mtvu->events.clear();
    mtvu->events_queued.store(0);
}

void destroy(Mtvu* mtvu) {
    stop_worker(mtvu);

    if (mtvu->gs_async) {
        gs::async::destroy(mtvu->gs_async);
    }

    if (mtvu->vif) {
        vif::destroy(mtvu->vif);
    }

    if (mtvu->gif) {
        gif::destroy(mtvu->gif);
    }

    if (mtvu->worker_logger) {
        logger::destroy(mtvu->worker_logger);
    }

    delete mtvu;
}

}
