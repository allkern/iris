#include <cstdio>
#include <vector>
#include <cstdint>

#include "ps2.h"

#include <SDL2/SDL.h>

void kputchar_stub(void* udata, char c) {
    putchar(c);
}

int main(int argc, const char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    SDL_Window* window = SDL_CreateWindow(
        "EEGS core",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED
    );

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        640, 480
    );

    std::vector <uint32_t> buf;

    buf.resize(640 * 480);

    bool open = true;

    struct ps2_state* ps2 = ps2_create();

    ps2_init(ps2);
    ps2_load_bios(ps2, argv[1]);
    ps2_gif_init(ps2->gif, buf.data());
    ps2_init_kputchar(ps2, kputchar_stub, NULL, kputchar_stub, NULL);

    while (open) {
        for (int i = 0; i < 0x10000; i++) {
            // if (ps2->ee->pc == 0x00082000) {
            //     FILE* file = fopen("3stars.elf", "rb");

            //     fseek(file, 0x1000, SEEK_SET);
            //     fread(ps2->ee_bus->ee_ram->buf + 0x400000, 1, 0x2a200, file);

            //     ps2->ee->pc = 0x400000;
            //     ps2->ee->next_pc = ps2->ee->pc + 4;

            //     fclose(file);
            // }

            ps2_cycle(ps2);
        }

        SDL_UpdateTexture(texture, NULL, buf.data(), 640 * sizeof(uint32_t));
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: {
                    open = false;
                } break;
            }
        }
    }

    ps2_destroy(ps2);

    return 0;
}