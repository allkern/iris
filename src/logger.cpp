#include "logger.hpp"

namespace iris::logger {

Logger* create() {
    Logger* logger = new Logger();

    logger->logger = logger;
    logger->logger_id = register_source(logger, "log");

    return logger;
}

// Modules that haven't been threaded a logger yet pass null, matching log()
size_t register_source(Logger* logger, const std::string& name) {
    if (!logger) return 0;

    logger->sources.push_back({ name, logger->sources.size() });

    return logger->sources.back().id;
}

size_t register_callback(Logger* logger, CallbackFunc func, void* udata) {
    logger->callbacks.push_back({ func, udata, logger->callbacks.size() });

    return logger->callbacks.back().id;
}

void set_level(Logger* logger, Level level) {
    if (!logger) return;

    logger->level = level;
}

Level get_level(Logger* logger) {
    if (!logger) return Level::INFO;

    return logger->level;
}

void destroy(Logger* logger) {
    delete logger;
}

const std::vector <Source>& get_sources(const Logger* logger) {
    return logger->sources;
}

const std::vector <Callback>& get_callbacks(const Logger* logger) {
    return logger->callbacks;
}

}
