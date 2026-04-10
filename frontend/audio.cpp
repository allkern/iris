#include <chrono>
#include <thread>
#include <cmath>

#include "iris.hpp"
#include "iop/iop_def.hpp"

namespace iris::audio {

static uint64_t prev_iop_cycles = 0;

void update(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    iris::instance* iris = (iris::instance*)userdata;

    if (iris->pause)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (iris->pause || !additional_amount)
        return;

    // printf("audio: iop cycles elapsed since last sync: %llu\n", iris->ps2->iop->total_cycles - prev_iop_cycles);

    prev_iop_cycles = iris->ps2->iop->total_cycles;

    struct ps2_spu2* spu2 = iris->ps2->spu2;

    // FILE* file = fopen("audio.raw", "ab");

    memset(iris->audio_buf.data(), 0, iris->audio_buf.size() * sizeof(spu2_sample));

    for (int c = 0; c < 2; c++) {
        if (spu2->c[c].adma_buffer_size == 0) {
            iris->audio_buf.resize(additional_amount);

            continue;
        }

        iris->audio_buf.resize(spu2->c[c].adma_buffer_size);

        for (int i = 0; i < spu2->c[c].adma_buffer_size; i++) {
            struct spu2_sample s = spu2->c[c].adma_buffer[i];

            iris->audio_buf[i].s16[0] = iris->mute_adma ? 0 : s.s16[0] * iris->volume;
            iris->audio_buf[i].s16[1] = iris->mute_adma ? 0 : s.s16[1] * iris->volume;
        }

        spu2->c[c].adma_buffer_size = 0;

        break;
    }

    for (int i = 0; i < iris->audio_buf.size(); i++) {
        struct spu2_sample s = ps2_spu2_get_sample(spu2, !iris->mute_adma);

        iris->audio_buf[i].s16[0] += iris->mute ? 0.0f : s.s16[0] * iris->volume;
        iris->audio_buf[i].s16[1] += iris->mute ? 0.0f : s.s16[1] * iris->volume;
    }

    // printf("audio: Outputting %d samples (%d required)\n", iris->audio_buf.size(), additional_amount);

    SDL_PutAudioStreamData(stream, (void*)iris->audio_buf.data(), iris->audio_buf.size() * sizeof(spu2_sample));
}

bool init(iris::instance* iris) {
    SDL_AudioSpec spec;

    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;

    iris->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, iris::audio::update, iris);

    if (!iris->stream) {
        fprintf(stderr, "audio: Failed to open audio device\n");

        return false;
    }

    /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
    SDL_ResumeAudioStreamDevice(iris->stream);

    return true;
}

void close(iris::instance* iris) {
    if (!iris->stream) {
        return;
    }

    SDL_PauseAudioStreamDevice(iris->stream);
    SDL_DestroyAudioStream(iris->stream);

    iris->stream = nullptr;
}

bool mute(iris::instance* iris) {
    iris->prev_mute = iris->mute;

    iris->mute = true;

    return iris->prev_mute;
}

void unmute(iris::instance* iris) {
    iris->mute = iris->prev_mute;
}

}