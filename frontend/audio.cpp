#include "iris.hpp"

namespace iris {

void audio_update(void* ud, uint8_t* buf, int size) {
    iris::instance* iris = (iris::instance*)ud;

    memset(buf, 0, size);

    if (iris->mute || iris->pause)
        return;

    for (int i = 0; i < (size >> 2); i++) {
        struct spu2_sample s;

        struct spu2_sample c0_adma = ps2_spu2_get_adma_sample(iris->ps2->spu2, 0);
        struct spu2_sample c1_adma = ps2_spu2_get_adma_sample(iris->ps2->spu2, 1);
    
        s.s16[0] = c0_adma.s16[0];
        s.s16[1] = c0_adma.s16[1];
        s.s16[0] += c1_adma.s16[0];
        s.s16[1] += c1_adma.s16[1];

        if (iris->core0_solo >= 0) {
            for (int i = 0; i < 24; i++) {
                struct spu2_sample c0 = ps2_spu2_get_voice_sample(iris->ps2->spu2, 0, i);

                bool mute = iris->core0_mute[i] || i != iris->core0_solo;

                s.s16[0] += mute ? 0 : c0.s16[0];
                s.s16[1] += mute ? 0 : c0.s16[1];
            }
        } else {
            for (int i = 0; i < 24; i++) {
                struct spu2_sample c0 = ps2_spu2_get_voice_sample(iris->ps2->spu2, 0, i);

                bool mute = iris->core0_mute[i];

                s.s16[0] += mute ? 0 : c0.s16[0];
                s.s16[1] += mute ? 0 : c0.s16[1];
            }
        }

        if (iris->core1_solo >= 0) {
            for (int i = 0; i < 24; i++) {
                struct spu2_sample c1 = ps2_spu2_get_voice_sample(iris->ps2->spu2, 1, i);

                bool mute = iris->core1_mute[i] || i != iris->core1_solo;

                s.s16[0] += mute ? 0 : c1.s16[0];
                s.s16[1] += mute ? 0 : c1.s16[1];
            }
        } else {
            for (int i = 0; i < 24; i++) {
                struct spu2_sample c1 = ps2_spu2_get_voice_sample(iris->ps2->spu2, 1, i);

                bool mute = iris->core1_mute[i];

                s.s16[0] += mute ? 0 : c1.s16[0];
                s.s16[1] += mute ? 0 : c1.s16[1];
            }
        }

        *(int16_t*)(&buf[(i << 2) + 0]) = s.u16[0];
        *(int16_t*)(&buf[(i << 2) + 2]) = s.u16[1];
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