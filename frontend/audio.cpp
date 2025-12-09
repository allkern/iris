#include "iris.hpp"

namespace iris::audio {

void update(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    iris::instance* iris = (iris::instance*)userdata;

    if (iris->pause || !additional_amount)
        return;

    iris->audio_buf.resize(additional_amount);

    for (int i = 0; i < additional_amount; i++) {
        iris->audio_buf[i] = ps2_spu2_get_sample(iris->ps2->spu2, !iris->mute_adma);
        iris->audio_buf[i].s16[0] *= iris->mute ? 0.0f : iris->volume;
        iris->audio_buf[i].s16[1] *= iris->mute ? 0.0f : iris->volume;
    }

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