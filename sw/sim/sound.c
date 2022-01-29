/*
 * Copyright 2021 Nicolas Maltais
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/sound.h>
#include <sim/sound.h>

#include <core/trace.h>

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdatomic.h>
#include <string.h>

#ifndef SIMULATION_HEADLESS
#include <portaudio.h>

static PaStream* stream;


// The sound playback in the simulator is made using PortAudio.
// The sound_play_note callback is called by the sound module to update the state of each channel.
// When the portaudio callback is called, the output buffer is filled with frames generated using
// the current state of each channel. The phase of each channel square wave is updated each time
// to allow the generation of a square wave of the correct frequency.

#define SAMPLE_RATE 44100

static const float GLOBAL_VOLUME_LEVELS[] = {
        [SOUND_VOLUME_0] = 0.2f,
        [SOUND_VOLUME_1] = 0.4f,
        [SOUND_VOLUME_2] = 0.6f,
        [SOUND_VOLUME_3] = 0.8f,
};

static const float CHANNEL_VOLUME_LEVELS[] = {
        0.0f,
        0.5f,
        1.0f,
};

// flag used to synchronize access to the channels data.
// the audio output stream callback is called from another thread,
// and should not rely on OS mecanisms (mutexes).
// http://www.portaudio.com/docs/v19-doxydocs/writing_a_callback.html
static atomic_flag channels_lock;

#endif //SIMULATION_HEADLESS

static sound_volume_t global_volume;
static bool output_enabled;

typedef struct {
    uint8_t note;
    uint8_t volume;
    uint16_t samples_per_period;
    uint16_t phase;
} channel_t;

static channel_t channels[SOUND_CHANNELS_COUNT];

void sound_set_output_enabled(bool enabled) {
    output_enabled = enabled;
}

bool sound_is_output_enabled(void) {
    return output_enabled;
}

void sound_set_volume_impl(sound_volume_t volume) {
    global_volume = volume;
}

sound_volume_t sound_get_volume_impl(void) {
    return global_volume;
}

void sound_set_channel_volume_impl(uint8_t channel, sound_channel_volume_t volume) {
    if (channel != 2) {
        return;
    }
    uint8_t v = 0;
    if (volume == SOUND_CHANNEL2_VOLUME0) {
        v = 1;
    } else if (volume == SOUND_CHANNEL2_VOLUME1) {
        v = 2;
    }
    channels[channel].volume = v;
}

sound_channel_volume_t sound_get_channel_volume_impl(uint8_t channel) {
    return channels[channel].volume;
}

void sound_play_note(uint8_t note, uint8_t channel) {
    // update channel fields for new note
#ifndef SIMULATION_HEADLESS
    channel_t *ch = &channels[channel];
    if (note == ch->note) {
        // no change
        return;
    } else {
        while (atomic_flag_test_and_set(&channels_lock));
        ch->note = note;
        ch->phase = 0;
        if (note == SOUND_NO_NOTE) {
            ch->phase = 0;
            ch->samples_per_period = 0;
        } else {
            float freq = 440 * powf(2.0f, (float) ((int) note - 33) / 12.0f);
            ch->samples_per_period = lroundf((float) SAMPLE_RATE / freq);
        }
        atomic_flag_clear(&channels_lock);
    }
#endif //SIMULATION_HEADLESS
}

#ifndef SIMULATION_HEADLESS
static bool handle_pa_error(PaError err) {
    if (err != paNoError) {
        trace("error occurred with the portaudio stream: [%d] %s", err, Pa_GetErrorText(err));
        Pa_Terminate();
        stream = 0;
        return true;
    }
    return false;
}

static int patestCallback(const void* input_buffer, void* output_buffer,
                          unsigned long frames_per_buffer,
                          const PaStreamCallbackTimeInfo* time_info,
                          PaStreamCallbackFlags status_flags,
                          void* user_data) {
    float* out = (float*) output_buffer;
    const sound_volume_t vol = global_volume;
    if (vol == SOUND_VOLUME_OFF || !output_enabled) {
        memset(out, 0, frames_per_buffer * sizeof(float));
        return paContinue;
    }

    while (atomic_flag_test_and_set(&channels_lock));
    for (size_t i = 0; i < frames_per_buffer; ++i) {
        // update the phase for all channels and count how many channels are on the
        // high side of the square wave.
        float level = 0;
        for (int channel = 0; channel < SOUND_CHANNELS_COUNT; ++channel) {
            channel_t *ch = &channels[channel];
            float ch_vol = CHANNEL_VOLUME_LEVELS[ch->volume];
            if (ch->note != SOUND_NO_NOTE) {
                if (ch->phase < ch->samples_per_period / 2) {
                    level += ch_vol;
                } else {
                    level -= ch_vol;
                }
                ++ch->phase;
                if (ch->phase == ch->samples_per_period) {
                    ch->phase = 0;
                }
            }
        }
        // normalize level and apply volume
        float sample = level / SOUND_CHANNELS_COUNT * GLOBAL_VOLUME_LEVELS[vol];
        *out++ = sample;
    }

    atomic_flag_clear(&channels_lock);
    return paContinue;
}
#endif //SIMULATION_HEADLESS

void sound_init(void) {
#ifndef SIMULATION_HEADLESS
    // temporarily close stderr because Pa_Initialize outputs lots of stuff.
    int stderr_copy = dup(STDERR_FILENO);
    close(STDERR_FILENO);
    handle_pa_error(Pa_Initialize());
    dup2(stderr_copy, STDERR_FILENO);
#endif //SIMULATION_HEADLESS

    for (int i = 0; i < SOUND_CHANNELS_COUNT; ++i) {
        channels[i].volume = 1;
    }
}

void sound_terminate(void) {
#ifndef SIMULATION_HEADLESS
    if (stream != 0) {
        sound_close_stream();
    }
    handle_pa_error(Pa_Terminate());
    stream = 0;
#endif //SIMULATION_HEADLESS
}

void sound_open_stream(void) {
#ifndef SIMULATION_HEADLESS
    if (stream != 0) {
        return;
    }

    // silence all channels initially
    for (int i = 0; i < SOUND_CHANNELS_COUNT; ++i) {
        channels[i].note = SOUND_NO_NOTE;
    }

    // create portaudio output stream.
    PaStreamParameters params;
    params.device = Pa_GetDefaultOutputDevice();
    if (params.device == paNoDevice) {
        trace("no default output device.");
        return;
    }
    params.channelCount = 1;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowOutputLatency;
    params.hostApiSpecificStreamInfo = 0;
    Pa_OpenStream(&stream, 0, &params, SAMPLE_RATE, paFramesPerBufferUnspecified,
            paClipOff, patestCallback, 0);
    handle_pa_error(Pa_StartStream(stream));
#endif //SIMULATION_HEADLESS
}

void sound_close_stream(void) {
#ifndef SIMULATION_HEADLESS
    if (stream == 0) {
        return;
    }
    if (handle_pa_error(Pa_StopStream(stream))) return;
    if (handle_pa_error(Pa_CloseStream(stream))) return;
    stream = 0;
#endif //SIMULATION_HEADLESS
}
