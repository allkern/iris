#include <chrono>
#include <thread>
#include <cmath>

#include "iris.hpp"
#include "iop/iop_def.hpp"
#include "ps2.hpp"

namespace iris::audio {

static constexpr int OUTPUT_RATE = 48000;

static inline int16_t clamp_s16(float v) {
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;

    return (int16_t)v;
}

void update_adma(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    Instance* iris = (Instance*)userdata;

    if (iris->debug.pause)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (iris->debug.pause || !additional_amount)
        return;

    spu2::Spu2* spu2 = iris->ps2->spu2;

    uint32_t frames = (uint32_t)additional_amount / sizeof(spu2::Sample);

    iris->audio.audio_buf.resize(frames);

    uint32_t read[2], avail[2];

    for (int c = 0; c < 2; c++) {
        uint32_t w = __atomic_load_n(&spu2->c[c].adma_write, __ATOMIC_ACQUIRE);

        read[c] = spu2->c[c].adma_read;
        avail[c] = w - read[c];
    }

    uint32_t mask = spu2->c[0].adma_buffer_max_size - 1;

    double step = (double)spu2->sample_rate / OUTPUT_RATE;
    double position = iris->audio.adma_position;

    for (uint32_t i = 0; i < frames; i++) {
        uint32_t index = (uint32_t)position;
        float weight = (float)(position - index);

        int32_t l = 0, r = 0;

        for (int c = 0; c < 2; c++) {
            if (index >= avail[c])
                continue;

            spu2::Sample a = spu2->c[c].adma_buffer[(read[c] + index) & mask];
            spu2::Sample b = a;

            if (index + 1 < avail[c])
                b = spu2->c[c].adma_buffer[(read[c] + index + 1) & mask];

            l += (int32_t)(a.s16[0] + (b.s16[0] - a.s16[0]) * weight);
            r += (int32_t)(a.s16[1] + (b.s16[1] - a.s16[1]) * weight);
        }

        iris->audio.audio_buf[i].s16[0] = iris->audio.mute_adma ? 0 : clamp_s16(l * iris->audio.volume);
        iris->audio.audio_buf[i].s16[1] = iris->audio.mute_adma ? 0 : clamp_s16(r * iris->audio.volume);

        position += step;
    }

    uint32_t consumed_frames = (uint32_t)position;

    iris->audio.adma_position = position - consumed_frames;

    for (int c = 0; c < 2; c++) {
        uint32_t consumed = (avail[c] < consumed_frames) ? avail[c] : consumed_frames;

        __atomic_store_n(&spu2->c[c].adma_read, read[c] + consumed, __ATOMIC_RELEASE);
    }

    SDL_PutAudioStreamData(stream, (void*)iris->audio.audio_buf.data(), frames * sizeof(spu2::Sample));
}

static std::vector <spu2::Sample> samples;

static spu2::Sample next_voice_sample(Instance* iris, spu2::Spu2* spu2) {
    spu2::Sample s;

#if SPU2_SYNC
    if (!spu2::pop_sample(spu2, &s))
        s.u32 = 0;
#else
    s = spu2::get_sample(spu2, !iris->audio.mute_adma);
#endif

    return s;
}

void update_spu2(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    Instance* iris = (Instance*)userdata;

    additional_amount /= sizeof(spu2::Sample);

    if (iris->debug.pause)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (iris->debug.pause || !additional_amount)
        return;

    // printf("audio: iop cycles elapsed since last sync: %llu\n", iris->ps2->iop->total_cycles - prev_iop_cycles);

    spu2::Spu2* spu2 = iris->ps2->spu2;

    // FILE* file = fopen("audio.raw", "ab");

    samples.resize(additional_amount);

    double step = (double)spu2->sample_rate / OUTPUT_RATE;
    double position = iris->audio.voice_position;

    for (int i = 0; i < additional_amount; i++) {
        while (position >= 1.0) {
            iris->audio.voice_prev = iris->audio.voice_next;
            iris->audio.voice_next = next_voice_sample(iris, spu2);

            position -= 1.0;
        }

        spu2::Sample a = iris->audio.voice_prev;
        spu2::Sample b = iris->audio.voice_next;

        float weight = (float)position;
        float l = a.s16[0] + (b.s16[0] - a.s16[0]) * weight;
        float r = a.s16[1] + (b.s16[1] - a.s16[1]) * weight;

        samples[i].s16[0] = iris->audio.mute ? 0 : clamp_s16(l * iris->audio.volume);
        samples[i].s16[1] = iris->audio.mute ? 0 : clamp_s16(r * iris->audio.volume);

        position += step;
    }

    iris->audio.voice_position = position;

    SDL_PutAudioStreamData(stream, (void*)samples.data(), samples.size() * sizeof(spu2::Sample));
}

bool init(Instance* iris) {
    SDL_AudioSpec spec;

    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;
    spec.freq = OUTPUT_RATE;

    iris->audio.audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);

    iris->audio.streams[0] = SDL_OpenAudioDeviceStream(iris->audio.audio_device, &spec, update_spu2, iris);
    iris->audio.streams[1] = SDL_OpenAudioDeviceStream(iris->audio.audio_device, &spec, update_adma, iris);

    if (!iris->audio.streams[0] || !iris->audio.streams[1]) {
        iris_error(&iris->log.audio, "Failed to open audio device");

        return false;
    }

    // SDL_BindAudioStreams(iris->audio.audio_device, iris->audio.streams, 2);

    SDL_ResumeAudioStreamDevice(iris->audio.streams[0]);
    SDL_ResumeAudioStreamDevice(iris->audio.streams[1]);

    return true;
}

void close(Instance* iris) {
    for (int i = 0; i < 2; i++) {
        if (!iris->audio.streams[i])
            continue;

        SDL_PauseAudioStreamDevice(iris->audio.streams[i]);
        SDL_DestroyAudioStream(iris->audio.streams[i]);

        iris->audio.streams[i] = nullptr;
    }
}

bool mute(Instance* iris) {
    iris->audio.prev_mute = iris->audio.mute;

    iris->audio.mute = true;

    SDL_PauseAudioStreamDevice(iris->audio.streams[0]);
    SDL_PauseAudioStreamDevice(iris->audio.streams[1]);

    return iris->audio.prev_mute;
}

void unmute(Instance* iris) {
    iris->audio.mute = iris->audio.prev_mute;

    if (!iris->audio.mute) {
        SDL_ResumeAudioStreamDevice(iris->audio.streams[0]);
        SDL_ResumeAudioStreamDevice(iris->audio.streams[1]);
    }
}

}