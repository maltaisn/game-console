
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

#include <core/sound.h>
#include <core/checks.h>

#define TRACK_DATA_END_MASK ((flash_t) 0x800000)

#define IMMEDIATE_PAUSE_OFFSET 0x55
#define SHORT_PAUSE_OFFSET 0xaa
#define IMMEDIATE_PAUSE_MASK 0x8000

#define TRACK_END 0xff

// Minimum number of bytes left in buffer required to not refresh.
// This corresponds to the worst case of <note> + <2 bytes duration>.
#define TRACK_BUFFER_MIN_SIZE 3

// The size in bytes of the track header in sound data.
#define TRACK_HEADER_SIZE 4

#define TRACK0_ACTIVE (TRACK0_STARTED | TRACK0_PLAYING)
#define TRACK1_ACTIVE (TRACK1_STARTED | TRACK1_PLAYING)
#define TRACK2_ACTIVE (TRACK2_STARTED | TRACK2_PLAYING)

typedef struct {
    // Current position in note data array, in flash data space.
    // TRACK_DATA_END_MASK is set when all data has been read for the track.
    flash_t data;
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
    uint8_t buffer[TRACK_BUFFER_SIZE];
    // Current position in buffer.
    uint8_t buffer_pos;
} track_t;

// Sound tracks, one per channel.
static track_t tracks[SOUND_CHANNELS_COUNT];
// Bitfield indicating which tracks are currently started and playing.
// - 0:2 indicate whether tracks have been started.
// - 3:5 indicate whether tracks have finished playing.
// The following states are possible:
// 1. Not started & not playing: track is stopped and wasn't playing before being stopped -> no sound
// 2. Not started & playing: track is stopped and was playing before being stopped -> no sound
// 3. Started & not playing: track is started, but has no data or is finished -> no sound
// 4. Started & playing: track is started and playing --> sound produced (aka "active")
static volatile uint8_t tracks_on;
// Current tempo value.
static uint8_t tempo;
// Delay in system ticks until next 1/16th of a beat is played on all tracks (minus one).
static uint8_t delay;

/**
 * Fill track data buffer starting from a position by reading from flash.
 */
static void track_fill_buffer(track_t* track, uint8_t start) {
    const uint8_t read_len = TRACK_BUFFER_SIZE - start;
    flash_read(track->data, read_len, &track->buffer[start]);
    track->data += read_len;

    // Look for end of track marker byte
    for (uint8_t i = start ; i < TRACK_BUFFER_SIZE ; ++i) {
        if (track->buffer[i] == TRACK_END) {
            track->data |= TRACK_DATA_END_MASK;
            break;
        }
    }
}

/**
 * Enable or disable buzzer output depending on volume level and whether there's any track playing.
 * This allows to save CPU time (no timer interrupts) and reduce current consumption.
 */
static void update_output_state(void) {
    uint8_t tracks_state = tracks_on;
    bool any_tracks_active = (tracks_state & TRACK0_ACTIVE) == TRACK0_ACTIVE ||
                             (tracks_state & TRACK1_ACTIVE) == TRACK1_ACTIVE ||
                             (tracks_state & TRACK2_ACTIVE) == TRACK2_ACTIVE;
    sound_set_output_enabled(sound_get_volume() != SOUND_VOLUME_OFF && any_tracks_active);
}

/**
 * Read the next note in track data and set it as current note with its duration.
 * Preconditions: track->duration_left == 0, track is playing.
 * This is called within an interrupt in actual implementation.
 */
static void track_seek_note(track_t* track, uint8_t track_playing_mask) {
    if (track->duration_total & IMMEDIATE_PAUSE_MASK) {
        // note is followed by an immediate pause.
        track->note = SOUND_NO_NOTE;
        track->duration_left = track->immediate_pause;
        track->duration_total &= ~IMMEDIATE_PAUSE_MASK;
        return;
    }

    uint8_t note = track->buffer[track->buffer_pos++];
    if (note == TRACK_END) {
        // no more notes in track, done playing.
        track->note = SOUND_NO_NOTE;
        tracks_on &= ~track_playing_mask;
        update_output_state();
        return;
    }

    if (note >= SHORT_PAUSE_OFFSET) {
        // single byte encoding for pause, no associated duration.
        // note that this doesn't update duration_total!
        track->duration_left = note - SHORT_PAUSE_OFFSET;
        track->note = SOUND_NO_NOTE;
        return;
    }

    const uint8_t duration = track->buffer[track->buffer_pos];
    if (track->duration_repeat) {
        // last duration continues to be repeated.
        --track->duration_repeat;
    } else if (duration & 0x80) {
        if (duration & 0x40) {
            // two bytes duration encoding.
            track->duration_total = (duration & 0x3f) << 8 | track->buffer[track->buffer_pos + 1];
            track->buffer_pos += 2;
        } else {
            // last duration will be repeated a number of times.
            track->duration_repeat = duration - 0x80;
            ++track->buffer_pos;
        }
    } else {
        // single byte duration encoding.
        track->duration_total = duration;
        ++track->buffer_pos;
    }
    track->duration_left = track->duration_total;
    if (note >= IMMEDIATE_PAUSE_OFFSET) {
        note -= IMMEDIATE_PAUSE_OFFSET;
        track->duration_total |= IMMEDIATE_PAUSE_MASK;
    }
    track->note = note;
}

/**
 * Update the current note for all playing tracks.
 * This is called within an interrupt in actual implementation.
 */
static void tracks_seek_note(void) {
    uint8_t track_active_mask = TRACK0_ACTIVE;
    for (int channel = 0 ; channel < SOUND_CHANNELS_COUNT ; ++channel) {
        track_t* track = &tracks[channel];
        if ((tracks_on & track_active_mask) == track_active_mask) {
            // Track is started & currently playing.
            if (track->duration_left == 0) {
                // Note ended, go to next note. Make sure buffer has enough data.
                if (!(track->data & TRACK_DATA_END_MASK) &&
                    track->buffer_pos >= TRACK_BUFFER_SIZE - TRACK_BUFFER_MIN_SIZE) {
                    // Not enough data to be guaranteed that next note can be decoded.
                    // Move all remaining data to the start of buffer and fill the rest.
                    uint8_t j = 0;
                    for (uint8_t i = track->buffer_pos ; i < TRACK_BUFFER_SIZE ; ++i, ++j) {
                        track->buffer[j] = track->buffer[i];
                    }
                    track->buffer_pos = 0;
                    track_fill_buffer(track, j);
                }
                track_seek_note(track, track_active_mask & TRACKS_PLAYING_ALL);
                sound_play_note(track->note, channel);
            } else {
                --track->duration_left;
            }
        }
        track_active_mask <<= 1;
    }
}

void sound_load(flash_t address) {
    uint8_t header[TRACK_HEADER_SIZE];
    uint8_t track_playing_mask = TRACK0_PLAYING;
#ifdef RUNTIME_CHECKS
    bool any_tracks = false;
#endif
    for (int i = 0 ; i < SOUND_CHANNELS_COUNT ; ++i) {
        track_t* track = &tracks[i];
        flash_read(address, TRACK_HEADER_SIZE, header);
#ifdef RUNTIME_CHECKS
        if (header[0] >= SOUND_CHANNELS_COUNT) {
            check_message("invalid sound data");
        }
#endif
        if (header[0] == i) {
            // Initialize track from header, fill buffer with first data.
            const uint16_t track_length = header[1] | header[2] << 8;
            track->data = address + TRACK_HEADER_SIZE;
            track->immediate_pause = header[3];
            track->duration_left = 0;
            track->duration_total = 0;
            track->duration_repeat = 0;
            track->buffer_pos = 0;
            track_fill_buffer(track, 0);
            // TODO not atomic
            tracks_on |= track_playing_mask;
            address += (flash_t) track_length;
#ifdef RUNTIME_CHECKS
            any_tracks = true;
#endif
        }
        track_playing_mask <<= 1;
    }
    update_output_state();
#ifdef RUNTIME_CHECKS
    if (!any_tracks) {
        check_message("loaded sound data has no tracks");
    }
#endif
}

void sound_start(uint8_t t) {
#ifdef RUNTIME_CHECKS
    if ((t & ~TRACKS_STARTED_ALL) != 0) {
        check_message("invalid track start flags");
    }
#endif
    // TODO not atomic
    tracks_on |= t;
    update_output_state();
}

void sound_stop(uint8_t t) {
    // TODO not atomic
    tracks_on &= ~TRACKS_STARTED_ALL;
    tracks_on |= ~t;
    update_output_state();
}

bool sound_check_tracks(uint8_t t) {
    return (tracks_on & t) != 0;
}

void sound_set_tempo(uint8_t t) {
    tempo = t;
}

uint8_t sound_get_tempo(void) {
    return tempo;
}

void sound_set_volume(sound_volume_t volume) {
    // should be in atomic block but won't affect sound noticeably.
    sound_set_volume_impl(volume);
    update_output_state();
}

sound_volume_t sound_get_volume(void) {
    return sound_get_volume_impl();
}

void sound_increase_volume(void) {
    sound_volume_t vol = sound_get_volume();
    if (vol != SOUND_VOLUME_3) {
        sound_set_volume(vol + SOUND_VOLUME_INCREMENT);
    }
}

void sound_decrease_volume(void) {
    sound_volume_t vol = sound_get_volume();
    if (vol != SOUND_VOLUME_OFF) {
        sound_set_volume(vol - SOUND_VOLUME_INCREMENT);
    }
}

void sound_update(void) {
    if (delay == 0) {
        delay = tempo;
        tracks_seek_note();
    } else {
        --delay;
    }
}
