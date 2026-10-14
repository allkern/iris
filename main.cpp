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

// Standard includes
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

// Iris includes
#include "iris.hpp"
#include "config.hpp"
#include "ee/ee_def.hpp"
#include "ee/vu_def.hpp"

// ImGui includes
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"

// SDL3 includes
#include <SDL3/SDL.h>

// stb_image stuff
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata("Iris", STR(_IRIS_VERSION), "com.allkern.iris");

    iris::platform::init_console();

    // Check if we got --help or --version in the commandline args
    // if so, don't do anything else.
    if (iris::cli::quick_exit(argc, (const char**)argv)) {
        return SDL_APP_SUCCESS;
    }

    iris::Instance* iris = iris::create();

    if (!iris::cli::parse(iris, argc, (const char**)argv)) {
        return SDL_APP_FAILURE;
    }

    if (!iris::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize instance");

        return SDL_APP_FAILURE;
    }

    // Initialize appstate
    *appstate = iris;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    iris::Instance* iris = (iris::Instance*)appstate;

    return iris::update(iris);
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    iris::Instance* iris = (iris::Instance*)appstate;

    return iris::handle_events(iris, event);
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    iris::Instance* iris = (iris::Instance*)appstate;

    iris::destroy(iris);
}