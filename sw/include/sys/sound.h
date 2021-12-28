
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

#ifndef SOUND_H
#define SOUND_H

#include <sys/flash.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdbool.h>

// Mostly taken from https://github.com/maltaisn/buzzer-midi, with slight adjustments:
// - Tempo is given in 1/256th of a second slices to work with system time counter.
// - All tracks can be loaded independantly, to support mixing sound effects & music.
// - Data is stored in flash instead of program memory.
// - Data is buffered to limit number of flash accesses.

// Sound data format:
// - 0x00-(end-1):
//       Track data. Tracks with no data are omitted.
// - end: last byte is 0xff
//
// Track data:
// - 0x00:
//       Channel number, 0 to (SOUND_CHANNELS_COUNT-1).
//       Must be greater than the channel number of previous tracks in sound data.
// - 0x01-0x02:
//       Track length, in bytes, including header (little endian).
// - 0x03:
//       Immediate pause duration in 1/16th of a beat.
// - 0x04-<duration_offset-1>:
//       Array of notes
//       - 0x00-0x53: notes from C2 to B8 (*)
//       - 0x54:      pause (*)
//       - 0x55-0xa8: notes from C2 to B8, followed by the immediate pause (*)
//       - 0xa9:      pause, followed by the immediate pause (*)
//       - 0xaa-0xfe: pause of duration (byte - 0xaa)
//       - 0xff:      last byte of note array
//       (*): note byte has an associated duration following it.
// - <duration_offset>-end:
//       Array of note durations
//       - 0x00-0x7f: single byte duration, minus one. (1-128)
//       - 0x80-0xbf: indicates that the current note and the (byte - 0x80) following
//                    notes have the same duration as the previously decoded duration.
//                    Note that single byte pauses are not counted as a previous duration.
//       - 0xc0-0xff: two bytes duration, minus one. (1-16384)
//                    duration = ((byte0 & 0x3f) << 8 | byte1)
//
// Example sound data:
//   0x01, 0x05, 0x00, 0x00, 0xff, 0x03, 0x24, 0x00, 0x04, 0x18, 0x3f, 0x19,
//   0xc1, 0xf3, 0x24, 0x07, 0x25, 0x83, 0x26, 0x27, 0x28, 0x29, 0x82, 0x30,
//   0x31, 0x6d, 0x0f, 0x6e, 0x80, 0x54, 0xc0, 0x83, 0xa9, 0x7e, 0xc0, 0x18,
//   0x18, 0xff, 0xff, 0x00, 0xff, 0xff.
//
// - track 0 data:
//     - 0x01: channel 1
//     - 0x05 0x00: length of track is 5 bytes.
//     - 0x00: immediate pauses have a duration of 1.
//     - 0xff: last byte of note data.
// - track 1 data:
//     - 0x03: channel 2
//     - 0x24 0x00: length of track is 36 bytes.
//     - 0x04: immediate pauses have a duration of 5.
//     - track data (described in <note data> / <duration data> format, not necessarily in the original order):
//         - 0x18 / 0x3f: C4, duration 64.
//         - 0x19 / 0xc1 0xf3: C#4, duration 500.
//         - 0x24 / 0x07: C5, duration 8.
//         - 0x25 0x26 0x27 0x28 / 0x83: C#5, D5, D#5, E5, duration 8 (same as last duration).
//         - 0x29 0x30 0x31 / 0x82: F5, F#5, G5, duration 8 (same as last duration).
//         - 0x6d / 0x0f: C4 duration 16, followed by pause of duration 5 (immediate).
//         - 0x6e / 0x80: C#4 duration 16 (same as last duration), followed by pause of duration 5 (immediate).
//         - 0x54 / 0xc0 0x83: pause, duration 132 (3 bytes encoding).
//         - 0xa9 / 0x7e: pause followed by immediate pause, for total duration of 132 (2 bytes encoding).
//         - 0xc0 /: pause, duration 23.
//         - 0x18 0x18 / 0xff 0xff 0x00: C4, duration 16384+1.
//         - 0xff: last byte of note data.
// - track 2 data: not present hence track is unused.
// - 0xff: last byte of sound data

// The number of sound channels supported
#define SOUND_CHANNELS_COUNT 3

// Maximum supported subdivision of a beat (1/16th of a beat)
#define SOUND_RESOLUTION 16

// Masks used to check whether a track is started.
#define TRACK0_STARTED (1 << 0)
#define TRACK1_STARTED (1 << 1)
#define TRACK2_STARTED (1 << 2)
// Masks used to check whether a track is playing.
#define TRACK0_PLAYING (1 << 3)
#define TRACK1_PLAYING (1 << 4)
#define TRACK2_PLAYING (1 << 5)

#define TRACKS_STARTED_ALL (TRACK0_STARTED | TRACK1_STARTED | TRACK2_STARTED)
#define TRACKS_PLAYING_ALL (TRACK0_PLAYING | TRACK1_PLAYING | TRACK2_PLAYING)

#define SOUND_PWM_MAX 24
#define SOUND_VOLUME_INCREMENT (1 << SOUND_CHANNELS_COUNT)

#define encode_bpm_tempo(bpm) ((uint8_t) ((60.0 * SYSTICK_FREQUENCY) / (bpm * SOUND_RESOLUTION) - 0.5))

typedef enum {
    SOUND_VOLUME_OFF = 0 * SOUND_VOLUME_INCREMENT,
    SOUND_VOLUME_0 = 1 * SOUND_VOLUME_INCREMENT,
    SOUND_VOLUME_1 = 2 * SOUND_VOLUME_INCREMENT,
    SOUND_VOLUME_2 = 3 * SOUND_VOLUME_INCREMENT,
    SOUND_VOLUME_3 = 4 * SOUND_VOLUME_INCREMENT,
} sound_volume_t;

/**
 * Load and initialize tracks from data contained in flash.
 * Can be used to reinitialize tracks state for looping.
 * Tracks not present in the loaded data are not changed.
 * The "playing" state of the track is set for loaded tracks.
 */
void sound_load(flash_t address);

/**
 * Start playback for given tracks. (using TRACKn_ACTIVE masks)
 * The "started" state of the tracks is set.
 */
void sound_start(uint8_t tracks);

/**
 * Stop playback for given tracks. (using TRACKn_ACTIVE masks)
 * The "started" state of the tracks is unset.
 * Must not be called from an interrupt.
 */
void sound_stop(uint8_t tracks);

/**
 * Returns true if the given track flags are set.
 * This includes "started" flags and "playing" flags.
 * A track produces sound if it is both started and playing.
 */
bool sound_check_tracks(uint8_t tracks);

/**
 * Set the sound tempo value.
 *
 * Tempo encoded as the number of system time counter ticks in 1/16th of a beat, minus one.
 * The `encode_bpm_tempo(<bpm>)` macro can be used to calculate this value.
 * Higher values result in slower tempo and lower values result is faster tempo.
 * Minimum tempo is 3.75 BPM (=255) and maximum tempo is 960 BPM (=0).
 * There's less than 10% error under 200 BPM.
 */
void sound_set_tempo(uint8_t tempo);

/**
 * Returns the sound tempo value.
 */
uint8_t sound_get_tempo(void);

/**
 * Set the sound volume value.
 */
void sound_set_volume(sound_volume_t volume);

/**
 * Returns the sound volume value.
 * If the volume is off, playback will continue but no sound will be produced.
 */
sound_volume_t sound_get_volume(void);

/**
 * Increase volume by one level, wrapping around to OFF after maximum.
 */
void sound_increase_volume(void);

/**
 * Decrease volume by one level, wrapping around to maximum after OFF.
 */
void sound_decrease_volume(void);

/**
 * Update sound state after one system time counter tick.
 * This is called automatically during the interrupt.
 */
void sound_update(void);


#endif //SOUND_H
