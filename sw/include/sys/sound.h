
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

#ifndef SYS_SOUND_H
#define SYS_SOUND_H

#include <core/sound.h>

#include <stdint.h>
#include <stdbool.h>

// The number of sound channels supported
#define SYS_SOUND_CHANNELS 3

#define SYS_SOUND_PWM_MAX 32

// Note value indicating nothing is being played
#define SYS_SOUND_NO_NOTE 0x54

// The size of each track buffer, to avoid reading from flash one byte at a time.
// A buffer size of 16 should give about 2-4 seconds of equivalent playback time.
#define SOUND_TRACK_BUFFER_SIZE 20


extern sound_volume_t sys_sound_global_volume;
extern sound_channel_volume_t sys_sound_channel2_volume;

/**
 * Play current note of track on sound channel.
 * This function should never be used by app code.
 */
void sys_sound_play_note(uint8_t note, uint8_t channel);

/**
 * Low-level implementation for setting global volume.
 * The core wrapper `sound_set_volume` must be used instead.
 * This implementation does not handle SOUND_VOLUME_OFF.
 */
void sys_sound_set_volume(sound_volume_t volume);

/**
 * Low-level implementation for getting volume.
 * The core wrapper `sound_get_volume` should be used instead.
 */
sound_volume_t sys_sound_get_volume(void);

/**
 * Low-level implementation for setting channel volume.
 * The core wrapper `sound_set_channel_volume` should be used instead.
 */
void sys_sound_set_channel_volume(uint8_t channel, sound_channel_volume_t volume);

/**
 * Low-level implementation for getting channel volume.
 * The core wrapper `sound_get_channel_volume` should be used instead.
 */
sound_channel_volume_t sys_sound_get_channel_volume(uint8_t channel);


// ==========================================
// The following declarations are not implemented in sys/sound.c but core/sound.c
// This is to avoid exposing the API as part of core but still share implementation with simulator.

typedef struct {
    // Current position in note data array, in unified data space.
    // TRACK_DATA_END_MASK is set when all data has been read for the track.
    sound_t data;
    // Pause duration used after note using immediate pause encoding.
    uint8_t immediate_pause;
    // Note being currently played (0-83).
    uint8_t note;
    // Time left for note currently being played, in 1/16th of a beat, -1.
    uint16_t duration_left;
    // Total duration of note currently being played, in 1/16th of a beat, -1.
    // The MSB of this field indicates whether the current note is followed by the most common pause.
    uint16_t duration_total;
    // Number of times that the current note duration is to be repeated yet.
    uint8_t duration_repeat;
    // Buffer used to store upcoming sound data.
    uint8_t buffer[SOUND_TRACK_BUFFER_SIZE];
    // Current position in buffer.
    uint8_t buffer_pos;
} sound_track_t;

// Sound tracks, one per channel.
extern sound_track_t sys_sound_tracks[SYS_SOUND_CHANNELS];

// Bitfield indicating which tracks are currently started and playing.
// - 0:2 indicate whether tracks have been started.
// - 3:5 indicate whether tracks have finished playing.
// The following states are possible:
// 1. Not started & not playing: track is stopped and wasn't playing before being stopped -> no sound
// 2. Not started & playing: track is stopped and was playing before being stopped -> no sound
// 3. Started & not playing: track is started, but has no data or is finished -> no sound
// 4. Started & playing: track is started and playing --> sound produced (aka "active")
extern volatile uint8_t sys_sound_tracks_on;

// Current tempo value.
extern uint8_t sys_sound_tempo;

/**
 * Fill track buffers with sound data. This must be called periodically
 * to avoid buffer underrun, in which case sound will be cut.
 */
void sys_sound_fill_track_buffers(void);

/**
 * Enable or disable buzzer output depending on volume level and whether there's any track playing.
 * This allows to save CPU time (no timer interrupts) and reduce current consumption.
 * This function should never be used by app code.
 */
void sys_sound_update_output_state(void);

#endif //SYS_SOUND_H
