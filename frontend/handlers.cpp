#include "iris.hpp"

namespace iris {

void handle_ee_tty_event(void* udata, char c) {
    iris::instance* iris = (iris::instance*)udata;

    if (c == '\r')
        return;

    if (c == '\n') {
        iris->ee_log.push_back("");
    } else {
        iris->ee_log.back().push_back(c);
    }
}

void handle_iop_tty_event(void* udata, char c) {
    iris::instance* iris = (iris::instance*)udata;

    if (c == '\r')
        return;

    if (c == '\n') {
        iris->iop_log.push_back("");
    } else {
        iris->iop_log.back().push_back(c);
    }
}

void handle_scissor_event(void* udata) {
    iris::instance* iris = (iris::instance*)udata;

    software_set_size(iris->ctx, 0, 0);
}

}