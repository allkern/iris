#include "instance.hpp"

int main(int argc, const char* argv[]) {
    lunar::instance instance;

    lunar::init(&instance, argc, argv);

    while (lunar::is_open(&instance)) {
        lunar::update(&instance);
    }

    lunar::close(&instance);
}