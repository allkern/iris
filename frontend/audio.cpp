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

    iris->audio_buf.resize(additional_amount);

    for (int i = 0; i < additional_amount; i++) {
        iris->audio_buf[i] = ps2_spu2_get_sample(spu2, !iris->mute_adma);
        iris->audio_buf[i].s16[0] *= iris->mute ? 0.0f : iris->volume;
        iris->audio_buf[i].s16[1] *= iris->mute ? 0.0f : iris->volume;
    }

    FILE* file = fopen("audio.raw", "ab");

    for (int c = 0; c < 2; c++) {
        if (spu2->c[c].adma_buffer_size == 0)
            continue;

        printf("audio: mixing in %d ADMA samples from core%d amount=%d\n", spu2->c[c].adma_buffer_size, c, additional_amount);

        int samples = spu2->c[c].adma_buffer_size > additional_amount ? additional_amount : spu2->c[c].adma_buffer_size;

        fwrite(spu2->c[c].adma_buffer, sizeof(struct spu2_sample), spu2->c[c].adma_buffer_size, file);

        for (int i = 1; i < samples; i++) {
            // struct spu2_sample a = spu2->c[c].adma_buffer[(int)std::roundf(((i-1)*ratio))];
            // struct spu2_sample b = spu2->c[c].adma_buffer[(int)std::roundf((i*ratio))];

            // iris->audio_buf[i].s16[0] += (a.s16[0] + b.s16[0]) / ratio;
            // iris->audio_buf[i].s16[1] += (a.s16[1] + b.s16[1]) / ratio;
            struct spu2_sample s = spu2->c[c].adma_buffer[i];

            iris->audio_buf[i].s16[0] += s.s16[0];
            iris->audio_buf[i].s16[1] += s.s16[1];
        }

        spu2->c[c].adma_buffer_size = 0;
    }

    fclose(file);

    // printf("audio: generated %d adma samples\n", spu2->c[0].adma_buffer_size + spu2->c[1].adma_buffer_size);

    // if (spu2->c[0].adma_buffer_size) {
    //     printf("audio: core0 %d adma samples\n", spu2->c[0].adma_buffer_size);

    //     spu2->c[0].adma_buffer_size = 0;

    //     exit(1);
    // }

    // if (spu2->c[1].adma_buffer_size) {
    //     printf("audio: core1 %d adma samples\n", spu2->c[1].adma_buffer_size);

    //     spu2->c[1].adma_buffer_size = 0;

    //     exit(1);
    // }

    SDL_PutAudioStreamData(stream, (void*)iris->audio_buf.data(), additional_amount * sizeof(spu2_sample));
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