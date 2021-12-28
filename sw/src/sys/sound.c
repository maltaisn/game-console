
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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "sys/led.h"

#define NO_NOTE 0x54

#define TRACK_DATA_END_MASK ((flash_t) 0x800000)

#define IMMEDIATE_PAUSE_OFFSET 0x55
#define SHORT_PAUSE_OFFSET 0xaa
#define IMMEDIATE_PAUSE_MASK 0x8000

#define TRACK_END 0xff

// The size of each track buffer, to avoid reading from flash one byte at a time.
// A buffer size of 16 should give about 1-3 seconds of equivalent playback time.
#define TRACK_BUFFER_SIZE 16

// Minimum number of bytes left in buffer required to not refresh.
// This corresponds to the worst case of <note> + <2 bytes duration>.
#define TRACK_BUFFER_MIN_SIZE 3

// The size in bytes of the track header in sound data.
#define TRACK_HEADER_SIZE 4

typedef struct {
    // Current position in note data array, in flash data space.
    // TRACK_DATA_END if channel isn't used or when all data has been read for track.
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

#define TRACK0_ACTIVE (TRACK0_STARTED | TRACK0_PLAYING)
#define TRACK1_ACTIVE (TRACK1_STARTED | TRACK1_PLAYING)
#define TRACK2_ACTIVE (TRACK2_STARTED | TRACK2_PLAYING)

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

#define CHANNEL0_ON (1 << 0)
#define CHANNEL1_ON (1 << 1)
#define CHANNEL2_ON (1 << 2)
#define CHANNELS_ALL_ON (CHANNEL0_ON | CHANNEL1_ON | CHANNEL2_ON)

// As a whole this register indicates an index in the PWM_LEVELS array.
// - 0:2 indicate the current level of the output for each channel (CHANNELn_ON masks).
// - 3:4 indicate the current volume level (sound_volume_t enum).
//   If volume is SOUND_VOLUME_OFF, the PWM_LEVELS array is not accessed.
// To slightly reduce interrupt latency, a general purpose I/O register is used
// since it allows single cycle access.
//
// Note A: TCB interrupts are triggered frequently enough (500-1000x per second) that
// zeroing the state of all tracks as an optimization won't make any perceptible difference.
#define out_level GPIOR0

// Timer counts for TCA PWM timer.
// The number corresponds to the number of bits set in the 0-7 position,
// multiplied by an arbitrary constant to account for the volume.
// Maximum value in array must be SOUND_MAX_PWM.
static uint8_t PWM_LEVELS[] = {
        0, 1, 1, 2, 1, 2, 2, 3,     // volume = 0, duty cycle 0 to 12%
        0, 2, 2, 4, 2, 4, 4, 6,     // volume = 1, duty cycle 0 to 24%
        0, 4, 4, 8, 4, 8, 8, 12,    // volume = 2, duty cycle 0 to 48%
        0, 8, 8, 16, 8, 16, 16, 24, // volume = 3, duty cycle 0 to 96%
};

// Timer counts for TCB channel timers, for each playable note.
// Counts are calculated using the following formula:
//   [count] = round([f_cpu] / [prescaler] / [note frequency] / 2) - 1
// Maximum error is about 0.01 semitone.
static uint16_t TIMER_NOTES[] = {
        38222, 36076, 34051, 32140, 30336, 28634, 27026, 25510, 24078, 22726, 21451, 20247,
        19110, 18038, 17025, 16070, 15168, 14316, 13513, 12754, 12038, 11363, 10725, 10123,
        9555, 9018, 8512, 8034, 7583, 7158, 6756, 6377, 6019, 5681, 5362, 5061,
        4777, 4509, 4256, 4017, 3791, 3578, 3377, 3188, 3009, 2840, 2680, 2530,
        2388, 2254, 2127, 2008, 1895, 1789, 1688, 1593, 1504, 1419, 1340, 1264,
        1193, 1126, 1063, 1003, 947, 894, 844, 796, 751, 709, 669, 632, 596,
};

static void track_fill_buffer(track_t* track, uint8_t start) {
    // Fill the rest of the buffer with data from flash.
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

#define TCA_ENABLE() (TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc | TCA_SPLIT_ENABLE_bm)
#define TCA_DISABLE() (TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc)

#define TCB_ENABLE(tcb) ((tcb).CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm)
#define TCB_DISABLE(tcb) ((tcb).CTRLA = TCB_CLKSEL_CLKDIV2_gc)

/**
 * Enable or disable buzzer output depending on volume level and whether there's any track playing.
 * This allows to save CPU time (no timer interrupts) and reduce current consumption.
 */
static void update_buzzer_output(void) {
    // see Note A
    TCB_DISABLE(TCB0);
    TCB_DISABLE(TCB1);
    TCB_DISABLE(TCB2);

    uint8_t tracks_state = tracks_on;
    bool any_tracks_active = (tracks_state & TRACK0_ACTIVE) == TRACK0_ACTIVE ||
                             (tracks_state & TRACK1_ACTIVE) == TRACK1_ACTIVE ||
                             (tracks_state & TRACK2_ACTIVE) == TRACK2_ACTIVE;
    if (sound_get_volume() != SOUND_VOLUME_OFF && any_tracks_active) {
        TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP0EN_bm;
        PORTA.PIN2CTRL = PORT_INVEN_bm;
        TCA_ENABLE();
        led_set();
    } else {
        TCA0.SPLIT.CTRLB = 0;
        PORTA.PIN2CTRL = 0;
        VPORTA.OUT |= PIN2_bm | PIN3_bm;
        TCA_DISABLE();
        led_clear();
    }
}

/**
 * Read the next note in track data and set it as current note with its duration.
 * Preconditions: track->duration_left == 0, track is playing.
 */
static void track_seek_note(track_t* track, uint8_t track_playing_mask) {
    if (track->duration_total & IMMEDIATE_PAUSE_MASK) {
        // note is followed by an immediate pause.
        track->note = NO_NOTE;
        track->duration_left = track->immediate_pause;
        track->duration_total &= ~IMMEDIATE_PAUSE_MASK;
        return;
    }

    uint8_t note = track->buffer[track->buffer_pos++];
    if (note == TRACK_END) {
        // no more notes in track, done playing.
        tracks_on &= ~track_playing_mask;
        update_buzzer_output();
        return;
    }

    if (note >= SHORT_PAUSE_OFFSET) {
        // single byte encoding for pause, no associated duration.
        // note that this doesn't update duration_total!
        track->duration_left = note - SHORT_PAUSE_OFFSET;
        track->note = NO_NOTE;
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
 * Play current note of track on sound channel.
 */
static void play_note(const track_t* track, uint8_t channel) {
    bool has_note = track->note != NO_NOTE;
    TCB_t* tcb = &TCB0 + channel;
    if (has_note) {
        tcb->CCMP = TIMER_NOTES[track->note];
        TCB_ENABLE(*tcb);
    } else {
        TCB_DISABLE(*tcb);
        // see Note A
        out_level &= ~CHANNELS_ALL_ON;
    }
}

/**
 * Update the current note for all playing tracks.
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
                play_note(track, channel);
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
    for (int i = 0 ; i < SOUND_CHANNELS_COUNT ; ++i) {
        track_t* track = &tracks[i];
        flash_read(address, TRACK_HEADER_SIZE, header);
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
            tracks_on |= track_playing_mask;
            address += (flash_t) track_length;
        }
        track_playing_mask <<= 1;
    }
    update_buzzer_output();
}

void sound_start(uint8_t t) {
    tracks_on |= t;
    update_buzzer_output();
}

void sound_stop(uint8_t t) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        tracks_on = (tracks_on & ~TRACKS_STARTED_ALL) | ~t;
    }
    update_buzzer_output();
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
    out_level = (out_level & CHANNELS_ALL_ON) | volume;
    update_buzzer_output();
}

sound_volume_t sound_get_volume(void) {
    return out_level & ~CHANNELS_ALL_ON;
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

// TCB interrupts:
// - update channel output level bit field
// - update TCA0 PWM duty cycle

ISR(TCB0_INT_vect) {
    uint8_t level = out_level;
    level ^= CHANNEL0_ON;
    TCA0.SPLIT.HCMP0 = PWM_LEVELS[level - SOUND_VOLUME_INCREMENT];
    out_level = level;
    TCB0.INTFLAGS = TCB_CAPT_bm;
}

ISR(TCB1_INT_vect) {
    uint8_t level = out_level;
    level ^= CHANNEL1_ON;
    TCA0.SPLIT.HCMP0 = PWM_LEVELS[level - SOUND_VOLUME_INCREMENT];
    out_level = level;
    TCB1.INTFLAGS = TCB_CAPT_bm;
}

ISR(TCB2_INT_vect) {
    uint8_t level = out_level;
    level ^= CHANNEL2_ON;
    TCA0.SPLIT.HCMP0 = PWM_LEVELS[level - SOUND_VOLUME_INCREMENT];
    out_level = level;
    TCB2.INTFLAGS = TCB_CAPT_bm;
}
