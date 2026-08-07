#pragma once

#include <cstddef>
#include <string>

#include "imgui.h"

#include "logger.hpp"

namespace iris {

constexpr size_t LOG_HISTORY_MAX = 8192;

struct LogSource {
    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

struct LogEntry {
    logger::Level level;
    size_t source;
    std::string text;
};

const char* log_level_name(logger::Level level);
const char* log_level_ansi(logger::Level level);
ImVec4 log_level_color(logger::Level level);

struct Instance;

void log_apply_settings(Instance* iris);
void log_close_file(Instance* iris);
void log_write_file(Instance* iris, const LogEntry& entry);

}
