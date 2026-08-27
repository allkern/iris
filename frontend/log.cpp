#include "iris.hpp"

namespace iris {

const char* log_level_name(logger::Level level) {
    switch (level) {
        case logger::LEVEL_DEBUG:       return "debug";
        case logger::LEVEL_INFO:        return "info";
        case logger::LEVEL_OK:          return "ok";
        case logger::LEVEL_WARNING:     return "warn";
        case logger::LEVEL_ERROR:       return "error";
        case logger::LEVEL_FATAL_ERROR: return "fatal";
    }

    return "?";
}

const char* log_level_ansi(logger::Level level) {
    switch (level) {
        case logger::LEVEL_DEBUG:       return "\x1b[38;5;244m";
        case logger::LEVEL_INFO:        return "\x1b[38;5;110m";
        case logger::LEVEL_OK:          return "\x1b[38;5;114m";
        case logger::LEVEL_WARNING:     return "\x1b[38;5;179m";
        case logger::LEVEL_ERROR:       return "\x1b[38;5;167m";
        case logger::LEVEL_FATAL_ERROR: return "\x1b[1;38;5;197m";
    }

    return "";
}

ImVec4 log_level_color(logger::Level level) {
    switch (level) {
        case logger::LEVEL_DEBUG:       return ImVec4(0.48f, 0.48f, 0.53f, 1.00f);
        case logger::LEVEL_INFO:        return ImVec4(0.51f, 0.68f, 0.92f, 1.00f);
        case logger::LEVEL_OK:          return ImVec4(0.42f, 0.80f, 0.51f, 1.00f);
        case logger::LEVEL_WARNING:     return ImVec4(0.92f, 0.72f, 0.35f, 1.00f);
        case logger::LEVEL_ERROR:       return ImVec4(0.91f, 0.44f, 0.44f, 1.00f);
        case logger::LEVEL_FATAL_ERROR: return ImVec4(1.00f, 0.36f, 0.51f, 1.00f);
    }

    return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
}

logger::Level log_level_from_name(const std::string& name) {
    for (int i = (int)logger::LEVEL_DEBUG; i <= (int)logger::LEVEL_FATAL_ERROR; i++) {
        if (name == log_level_name((logger::Level)i))
            return (logger::Level)i;
    }

    return logger::LEVEL_INFO;
}

void log_write_file(Instance* iris, const LogEntry& entry) {
    if (!iris->log_file)
        return;

    const std::vector <logger::Source>& sources = logger::get_sources(iris->logger);

    const char* name = entry.source < sources.size() ? sources[entry.source].name.c_str() : "?";

    fmt::print(iris->log_file, "{:<5} {:>8}  {}\n", log_level_name(entry.level), name, entry.text);

    fflush(iris->log_file);
}

void log_close_file(Instance* iris) {
    std::lock_guard <std::mutex> lock(iris->log_mutex);

    if (!iris->log_file)
        return;

    fclose(iris->log_file);

    iris->log_file = nullptr;
    iris->log_file_path.clear();
}

void log_apply_settings(Instance* iris) {
    logger::set_level(iris->logger, iris->log_level);

    if (!iris->log_to_file || iris->paths.log_path.empty()) {
        log_close_file(iris);

        return;
    }

    if (iris->log_file && iris->log_file_path == iris->paths.log_path)
        return;

    log_close_file(iris);

    std::lock_guard <std::mutex> lock(iris->log_mutex);

    iris->log_file = fopen(iris->paths.log_path.c_str(), "w");

    if (!iris->log_file) {
        fmt::print(stderr, "log: couldn't open \"{}\" for writing\n", iris->paths.log_path);

        return;
    }

    iris->log_file_path = iris->paths.log_path;

    for (const LogEntry& entry : iris->log_history)
        log_write_file(iris, entry);

    fflush(iris->log_file);
}

}
