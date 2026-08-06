#pragma once

#include <SDL3/SDL.h>

namespace iris {

struct Instance;

namespace audio {
    bool init(Instance* iris);
    void close(Instance* iris);
    void update(void* udata, SDL_AudioStream* stream, int additional_amount, int total_amount);
    bool mute(Instance* iris);
    void unmute(Instance* iris);
}

}
