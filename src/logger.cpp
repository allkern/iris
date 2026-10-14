/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
