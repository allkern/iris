#include "iris.hpp"

namespace iris {

void handle_ee_tty_event(void* udata, char c) {
    instance* iris = (instance*)udata;

    if (c == '\r')
        return;

    if (c == '\n') {
        iris->ee_log.push_back("");
    } else {
        iris->ee_log.back().push_back(c);
    }
}

void handle_iop_tty_event(void* udata, char c) {
    instance* iris = (instance*)udata;

    if (c == '\r')
        return;

    if (c == '\n') {
        iris->iop_log.push_back("");
    } else {
        iris->iop_log.back().push_back(c);
    }
}

void handle_log_event(void* udata, logger::Level level, const logger::Source& source, const std::string& text) {
    instance* iris = (instance*)udata;

    if (level != logger::Level::FATAL_ERROR)
        return;

    // Keep the first fatal: later ones are usually fallout from this one
    if (iris->fatal_error)
        return;

    iris->fatal_error = true;
    iris->fatal_error_text = fmt::format("{}: {}", source.name, text);

    iris->pause = true;
}

void handle_sysmem_tty_event(void* udata, char c) {
    instance* iris = (instance*)udata;

    if (c == '\r')
        return;

    if (c == '\n') {
        iris->sysmem_log.push_back("");
    } else {
        iris->sysmem_log.back().push_back(c);
    }
}

}