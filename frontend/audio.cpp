#include <chrono>
#include <thread>
#include <cmath>

#include "iris.hpp"
#include "iop/iop_def.hpp"

namespace iris::audio {

static inline int16_t clamp_s16(float v) {
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;

    return (int16_t)v;
}

static uint64_t prev_iop_cycles = 0;

void update_adma(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    iris::instance* iris = (iris::instance*)userdata;

    if (iris->pause)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (iris->pause || !additional_amount)
        return;

    uint64_t elapsed = iris->ps2->iop->total_cycles - prev_iop_cycles;

    // printf("audio: elapsed=%llu samples=%lld remainder=%lld required=%d\n", elapsed, elapsed / 768, elapsed % 768, additional_amount);

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

            iris->audio_buf[i].s16[0] = iris->mute_adma ? 0 : clamp_s16(s.s16[0] * iris->volume);
            iris->audio_buf[i].s16[1] = iris->mute_adma ? 0 : clamp_s16(s.s16[1] * iris->volume);
        }

        spu2->c[c].adma_buffer_size = 0;

        break;
    }

    // printf("audio: Outputting %d samples (%d required)\n", iris->audio_buf.size(), additional_amount);

    SDL_PutAudioStreamData(stream, (void*)iris->audio_buf.data(), iris->audio_buf.size() * sizeof(spu2_sample));
}

static std::vector <spu2_sample> samples;

void update_spu2(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    iris::instance* iris = (iris::instance*)userdata;

    if (iris->pause)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (iris->pause || !additional_amount)
        return;

    // printf("audio: iop cycles elapsed since last sync: %llu\n", iris->ps2->iop->total_cycles - prev_iop_cycles);

    struct ps2_spu2* spu2 = iris->ps2->spu2;

    // FILE* file = fopen("audio.raw", "ab");

    samples.resize(additional_amount);

    for (int i = 0; i < additional_amount; i++) {
        struct spu2_sample s;

#if SPU2_SYNC
        if (!ps2_spu2_pop_sample(spu2, &s))
            s.u32 = 0;
#else
        s = ps2_spu2_get_sample(spu2, !iris->mute_adma);
#endif

        samples[i].s16[0] = iris->mute ? 0 : clamp_s16(s.s16[0] * iris->volume);
        samples[i].s16[1] = iris->mute ? 0 : clamp_s16(s.s16[1] * iris->volume);
    }

    SDL_PutAudioStreamData(stream, (void*)samples.data(), samples.size() * sizeof(spu2_sample));
}

bool init(iris::instance* iris) {
    SDL_AudioSpec spec;

    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;

    iris->audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);

    iris->streams[0] = SDL_OpenAudioDeviceStream(iris->audio_device, &spec, update_spu2, iris);
    iris->streams[1] = SDL_OpenAudioDeviceStream(iris->audio_device, &spec, update_adma, iris);

    if (!iris->streams[0] || !iris->streams[1]) {
        fprintf(stderr, "audio: Failed to open audio device\n");

        return false;
    }

    // SDL_BindAudioStreams(iris->audio_device, iris->streams, 2);

    /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
    SDL_ResumeAudioStreamDevice(iris->streams[0]);
    SDL_ResumeAudioStreamDevice(iris->streams[1]);

    return true;
}

void close(iris::instance* iris) {
    for (int i = 0; i < 2; i++) {
        if (!iris->streams[i])
            continue;

        SDL_PauseAudioStreamDevice(iris->streams[i]);
        SDL_DestroyAudioStream(iris->streams[i]);

        iris->streams[i] = nullptr;
    }
}

bool mute(iris::instance* iris) {
    iris->prev_mute = iris->mute;

    iris->mute = true;

    SDL_PauseAudioStreamDevice(iris->streams[0]);
    SDL_PauseAudioStreamDevice(iris->streams[1]);

    return iris->prev_mute;
}

void unmute(iris::instance* iris) {
    iris->mute = iris->prev_mute;

    if (!iris->mute) {
        SDL_ResumeAudioStreamDevice(iris->streams[0]);
        SDL_ResumeAudioStreamDevice(iris->streams[1]);
    }
}

}