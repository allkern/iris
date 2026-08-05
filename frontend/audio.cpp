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

void update_adma(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    instance* iris = (instance*)userdata;

    if (iris->pause)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (iris->pause || !additional_amount)
        return;

    spu2::Spu2* spu2 = iris->ps2->spu2;

    uint32_t frames = (uint32_t)additional_amount / sizeof(spu2::Sample);

    iris->audio_buf.resize(frames);

    uint32_t read[2], avail[2];

    for (int c = 0; c < 2; c++) {
        uint32_t w = __atomic_load_n(&spu2->c[c].adma_write, __ATOMIC_ACQUIRE);

        read[c] = spu2->c[c].adma_read;
        avail[c] = w - read[c];
    }

    uint32_t mask = spu2->c[0].adma_buffer_max_size - 1;

    for (uint32_t i = 0; i < frames; i++) {
        int32_t l = 0, r = 0;

        for (int c = 0; c < 2; c++) {
            if (i < avail[c]) {
                spu2::Sample s = spu2->c[c].adma_buffer[(read[c] + i) & mask];

                l += s.s16[0];
                r += s.s16[1];
            }
        }

        iris->audio_buf[i].s16[0] = iris->mute_adma ? 0 : clamp_s16(l * iris->volume);
        iris->audio_buf[i].s16[1] = iris->mute_adma ? 0 : clamp_s16(r * iris->volume);
    }

    for (int c = 0; c < 2; c++) {
        uint32_t consumed = (avail[c] < frames) ? avail[c] : frames;

        __atomic_store_n(&spu2->c[c].adma_read, read[c] + consumed, __ATOMIC_RELEASE);
    }

    SDL_PutAudioStreamData(stream, (void*)iris->audio_buf.data(), frames * sizeof(spu2::Sample));
}

static std::vector <spu2::Sample> samples;

void update_spu2(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    instance* iris = (instance*)userdata;

    additional_amount /= sizeof(spu2::Sample);

    if (iris->pause)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (iris->pause || !additional_amount)
        return;

    // printf("audio: iop cycles elapsed since last sync: %llu\n", iris->ps2->iop->total_cycles - prev_iop_cycles);

    spu2::Spu2* spu2 = iris->ps2->spu2;

    // FILE* file = fopen("audio.raw", "ab");

    samples.resize(additional_amount);

    for (int i = 0; i < additional_amount; i++) {
        spu2::Sample s;

#if SPU2_SYNC
        if (!spu2::pop_sample(spu2, &s))
            s.u32 = 0;
#else
        s = spu2::get_sample(spu2, !iris->mute_adma);
#endif

        samples[i].s16[0] = iris->mute ? 0 : clamp_s16(s.s16[0] * iris->volume);
        samples[i].s16[1] = iris->mute ? 0 : clamp_s16(s.s16[1] * iris->volume);
    }

    SDL_PutAudioStreamData(stream, (void*)samples.data(), samples.size() * sizeof(spu2::Sample));
}

bool init(instance* iris) {
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

    SDL_ResumeAudioStreamDevice(iris->streams[0]);
    SDL_ResumeAudioStreamDevice(iris->streams[1]);

    return true;
}

void close(instance* iris) {
    for (int i = 0; i < 2; i++) {
        if (!iris->streams[i])
            continue;

        SDL_PauseAudioStreamDevice(iris->streams[i]);
        SDL_DestroyAudioStream(iris->streams[i]);

        iris->streams[i] = nullptr;
    }
}

bool mute(instance* iris) {
    iris->prev_mute = iris->mute;

    iris->mute = true;

    SDL_PauseAudioStreamDevice(iris->streams[0]);
    SDL_PauseAudioStreamDevice(iris->streams[1]);

    return iris->prev_mute;
}

void unmute(instance* iris) {
    iris->mute = iris->prev_mute;

    if (!iris->mute) {
        SDL_ResumeAudioStreamDevice(iris->streams[0]);
        SDL_ResumeAudioStreamDevice(iris->streams[1]);
    }
}

}