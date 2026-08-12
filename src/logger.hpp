#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>

// <wingdi.h> defines ERROR as a macro, which would mangle Level::ERROR in any
// translation unit that reaches a Windows header before this one.
#ifdef ERROR
#undef ERROR
#endif

namespace iris::logger {

// Ordered by severity so log() can drop a message before formatting it
enum class Level {
    DEBUG,
    INFO,
    OK,
    WARNING,
    ERROR,
    FATAL_ERROR
};

struct Source {
    std::string name;
    size_t id;
};

typedef void (*CallbackFunc)(void* udata, Level level, const Source& source, const std::string& text);

struct Callback {
    CallbackFunc func;
    void* udata;
    size_t id;
};

struct Logger {
    std::vector <Source> sources;
    std::vector <Callback> callbacks;

    Level level = Level::INFO;

    Logger* logger = nullptr;
    size_t logger_id = 0;
};

Logger* create();
size_t register_source(Logger* logger, const std::string& name);
size_t register_callback(Logger* logger, CallbackFunc func, void* udata);
void set_level(Logger* logger, Level level);
Level get_level(Logger* logger);
void destroy(Logger* logger);

const std::vector <Source>& get_sources(const Logger* logger);
const std::vector <Callback>& get_callbacks(const Logger* logger);

template <typename... Args>
void log(Logger* logger, Level level, size_t source, fmt::format_string<Args...> fmt, Args&&... args) {
    if (!logger) return;
    if (level < logger->level) return;

    std::string text = fmt::format(fmt, std::forward<Args>(args)...);

    for (const auto& callback : logger->callbacks)
        callback.func(callback.udata, level, logger->sources[source], text);
}

inline constexpr Level LEVEL_DEBUG = Level::DEBUG;
inline constexpr Level LEVEL_INFO = Level::INFO;
inline constexpr Level LEVEL_OK = Level::OK;
inline constexpr Level LEVEL_WARNING = Level::WARNING;
inline constexpr Level LEVEL_ERROR = Level::ERROR;
inline constexpr Level LEVEL_FATAL_ERROR = Level::FATAL_ERROR;

#define iris_log(src, level, fmt, ...) ::iris::logger::log((src)->logger, level, (src)->logger_id, fmt, ##__VA_ARGS__)
#define iris_info(src, fmt, ...) ::iris::logger::log((src)->logger, ::iris::logger::LEVEL_INFO, (src)->logger_id, fmt, ##__VA_ARGS__)
#define iris_warning(src, fmt, ...) ::iris::logger::log((src)->logger, ::iris::logger::LEVEL_WARNING, (src)->logger_id, fmt, ##__VA_ARGS__)
#define iris_error(src, fmt, ...) ::iris::logger::log((src)->logger, ::iris::logger::LEVEL_ERROR, (src)->logger_id, fmt, ##__VA_ARGS__)
#define iris_fatal_error(src, fmt, ...) ::iris::logger::log((src)->logger, ::iris::logger::LEVEL_FATAL_ERROR, (src)->logger_id, fmt, ##__VA_ARGS__)
#define iris_debug(src, fmt, ...) ::iris::logger::log((src)->logger, ::iris::logger::LEVEL_DEBUG, (src)->logger_id, fmt, ##__VA_ARGS__)
#define iris_ok(src, fmt, ...) ::iris::logger::log((src)->logger, ::iris::logger::LEVEL_OK, (src)->logger_id, fmt, ##__VA_ARGS__)

}
