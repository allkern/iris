#include "iris.hpp"

namespace iris {

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

    FILE* stream = level >= logger::Level::WARNING ? stderr : stdout;

    fmt::print(stream, "{}: {}\n", source.name, text);
    fflush(stream);

    if (level != logger::Level::FATAL_ERROR)
        return;

    // Keep the first fatal: later ones are usually fallout from this one
    if (iris->fatal_error)
        return;

    iris->fatal_error = true;
    iris->fatal_error_text = fmt::format("{}: {}", source.name, text);

    iris->debug.pause = true;
}

void init_logger(Instance* iris) {
    iris->logger = logger::create();

    logger::register_callback(iris->logger, handle_log_event, iris);

    auto reg = [iris](LogSource& src, const std::string& name) {
        src.logger = iris->logger;
        src.logger_id = logger::register_source(iris->logger, name);
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