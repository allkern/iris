#include "iris.hpp"

#undef main

int main(int argc, const char* argv[]) {
    iris::instance instance;

    iris::init(&instance, argc, argv);

    while (iris::is_open(&instance)) {
        iris::update(&instance);
    }

    iris::close(&instance);
}