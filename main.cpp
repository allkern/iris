// Standard includes
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

// Iris includes
#include "iris.hpp"
#include "config.hpp"
#include "ee/ee_def.hpp"

// ImGui includes
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"

// SDL3 includes
#include <SDL3/SDL.h>

// stb_image stuff
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata("Iris", STR(_IRIS_VERSION), "com.allkern.iris");

    // Check if we got --help or --version in the commandline args
    // if so, don't do anything else.
    if (iris::settings::check_for_quick_exit(argc, (const char**)argv)) {
        return SDL_APP_SUCCESS;
    }

    iris::instance* iris = iris::create();

    if (!iris::init(iris, argc, (const char**)argv)) {
        fprintf(stderr, "iris: Failed to initialize instance\n");

        return SDL_APP_FAILURE;
    }

    // Initialize appstate
    *appstate = iris;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    iris::instance* iris = (iris::instance*)appstate;

    return iris::update(iris);
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    iris::instance* iris = (iris::instance*)appstate;

    return iris::handle_events(iris, event);
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    iris::instance* iris = (iris::instance*)appstate;

    iris::destroy(iris);
}