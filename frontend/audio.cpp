#include "iris.hpp"

namespace iris {

void audio_update(void* ud, uint8_t* buf, int size) {
    iris::instance* iris = (iris::instance*)ud;

    memset(buf, 0, size);

    if (iris->mute || iris->pause)
        return;

    for (int i = 0; i < (size >> 2); i++) {
        struct spu2_sample sample = ps2_spu2_get_sample(iris->ps2->spu2);

        *(int16_t*)(&buf[(i << 2) + 0]) = sample.u16[0];
        *(int16_t*)(&buf[(i << 2) + 2]) = sample.u16[1];
    }
}

int init_audio(iris::instance* iris) {
    SDL_AudioDeviceID dev;
    SDL_AudioSpec obtained, desired;

    desired.freq     = 48000;
    desired.format   = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples  = 512;
    desired.callback = &audio_update;
    desired.userdata = iris;

    dev = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);

    if (dev) {
        SDL_PauseAudioDevice(dev, 0);

        return 0;
    }

    return 1;
}

}