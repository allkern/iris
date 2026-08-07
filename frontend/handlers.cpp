#include "iris.hpp"

#ifdef _WIN32
#include <io.h>
#define iris_isatty _isatty
#define iris_fileno _fileno
#else
#include <unistd.h>
#define iris_isatty isatty
#define iris_fileno fileno
#endif

namespace iris {

static bool console_is_tty() {
    static const bool tty = iris_isatty(iris_fileno(stdout)) != 0;

    return tty;
}

void handle_ee_tty_event(void* udata, char c) {
    Instance* iris = (Instance*)udata;

    if (c == '\r')
        return;

    if (c == '\n') {
        iris->debug.ee_log.push_back("");
    } else {
        iris->debug.ee_log.back().push_back(c);
    }
}

void handle_iop_tty_event(void* udata, char c) {
    Instance* iris = (Instance*)udata;

    if (c == '\r')
        return;

    if (c == '\n') {
        iris->debug.iop_log.push_back("");
    } else {
        iris->debug.iop_log.back().push_back(c);
    }
}

void handle_log_event(void* udata, logger::Level level, const logger::Source& source, const std::string& text) {
    Instance* iris = (Instance*)udata;

    {
        std::lock_guard <std::mutex> lock(iris->log_mutex);

        iris->log_history.push_back({ level, source.id, text });

        while (iris->log_history.size() > LOG_HISTORY_MAX)
            iris->log_history.pop_front();

        if (iris->log_to_file && iris->log_file) {
            log_write_file(iris, iris->log_history.back());

            fflush(iris->log_file);
        }
    }

    if (iris->log_to_console) {
        FILE* stream = level >= logger::LEVEL_WARNING ? stderr : stdout;

        if (console_is_tty()) {
            fmt::print(stream, "{}{:<5}\x1b[0m \x1b[38;5;244m{:>8}\x1b[0m  {}\n",
                log_level_ansi(level), log_level_name(level), source.name, text);
        } else {
            fmt::print(stream, "{:<5} {:>8}  {}\n", log_level_name(level), source.name, text);
        }

        fflush(stream);
    }

    if (level != logger::LEVEL_FATAL_ERROR)
        return;

    if (iris->fatal_error)
        return;

    iris->fatal_error = true;
    iris->fatal_error_text = fmt::format("{}: {}", source.name, text);

    iris->debug.pause = true;
}

void init_logger(Instance* iris) {
    platform::init_console();

    iris->logger = logger::create();

    logger::register_callback(iris->logger, handle_log_event, iris);

    auto reg = [iris](LogSource& src, const std::string& name) {
        src.logger = iris->logger;
        src.logger_id = logger::register_source(iris->logger, name);

        iris->frontend_log_sources.push_back(src.logger_id);
    };

    reg(iris->log.iris, "iris");
    reg(iris->log.vulkan, "vulkan");
    reg(iris->log.render, "render");
    reg(iris->log.shaders, "shaders");
    reg(iris->log.imgui, "imgui");
    reg(iris->log.elf, "elf");
    reg(iris->log.settings, "settings");
    reg(iris->log.emu, "emu");
    reg(iris->log.input, "input");
    reg(iris->log.audio, "audio");
    reg(iris->log.slirp, "slirp");
    reg(iris->log.net, "net");
    reg(iris->log.gamelist, "gamelist");
    reg(iris->log.platform, "platform");
    reg(iris->log.ui, "ui");
}

void handle_sysmem_tty_event(void* udata, char c) {
    Instance* iris = (Instance*)udata;

    if (c == '\r')
        return;

    if (c == '\n') {
        iris->debug.sysmem_log.push_back("");
    } else {
        iris->debug.sysmem_log.back().push_back(c);
    }
}

}