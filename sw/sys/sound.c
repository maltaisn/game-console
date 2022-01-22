
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

#define GLOBAL_VOLUME_MASK 0x03

// Minimum volume at which the H-bridge driver is used.
// Using the H-bridge for lower volumes is power inefficient.
#define HBRIDGE_MIN_VOLUME SOUND_VOLUME_3

// As a whole this register indicates an index in the PWM_LEVELS array.
// - 0:1 indicate the global volume level (sound_volume_t enum).
// - 2:5 indicate the current level of the output for each channel (sound_channel_volume_t enum).
//       These bits are set and cleared to create the waveform for each channel.
//   If volume is SOUND_VOLUME_OFF, the PWM_LEVELS array is not accessed.
// To slightly reduce interrupt latency, a general purpose I/O register is used
// since it allows single cycle access.
//
// Note A: TCB interrupts are triggered frequently enough (500-1000x per second) that
// zeroing the state of all tracks as an optimization won't make any perceptible difference.
#define out_level GPIOR0

static sound_volume_t global_volume; // = SOUND_VOLUME_0
static sound_channel_volume_t channel2_volume = SOUND_CHANNEL2_VOLUME0;

// Timer counts for TCA PWM timer.
// The value corresponds to the number of bits set in the 2-5 position in out_level,
// multiplied by an arbitrary constant to account for the global volume and channel volume.
// The maximum value in array must be SOUND_MAX_PWM.
// The condition where both masks for channel 2 are set must never happen (no associated values).
static uint8_t PWM_LEVELS[] = {
        0, 0, 0, 0,
        2, 4, 8, 8,
        2, 4, 8, 8,
        4, 8, 16, 16,
        2, 4, 8, 8,
        4, 8, 16, 16,
        4, 8, 16, 16,
        6, 12, 24, 24,
        4, 8, 16, 16,
        6, 12, 24, 24,
        6, 12, 24, 24,
        8, 16, 32, 32,
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

#define TCA_ENABLE() (TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc | TCA_SPLIT_ENABLE_bm)
#define TCA_DISABLE() (TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc)

#define TCB_ENABLE(tcb) ((tcb).CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm)
#define TCB_DISABLE(tcb) ((tcb).CTRLA = TCB_CLKSEL_CLKDIV2_gc)

#define HBRIDGE_ENABLE() PORTA.PIN2CTRL = PORT_INVEN_bm; \
                         EVSYS.USEREVOUTA = EVSYS_CHANNEL_CHANNEL0_gc
#define HBRIDGE_DISABLE() PORTA.PIN2CTRL = 0; \
                          EVSYS.USEREVOUTA = EVSYS_CHANNEL_OFF_gc

void sound_set_output_enabled(bool enabled) {
    if (enabled) {
        TCA_ENABLE();
        if (global_volume >= HBRIDGE_MIN_VOLUME) {
            HBRIDGE_ENABLE();
        }
    } else {
        TCB_DISABLE(TCB0);
        TCB_DISABLE(TCB1);
        TCB_DISABLE(TCB2);
        TCA_DISABLE();
        HBRIDGE_DISABLE();
        VPORTA.OUT &= ~(PIN2_bm | PIN3_bm);
    }
}

void sound_play_note(uint8_t note, uint8_t channel) {
    bool has_note = note != SOUND_NO_NOTE;
    TCB_t* tcb = &TCB0 + channel;
    if (has_note) {
        tcb->CCMP = TIMER_NOTES[note];
        TCB_ENABLE(*tcb);
    } else {
        TCB_DISABLE(*tcb);
        // see Note A
        out_level &= GLOBAL_VOLUME_MASK;
    }
}

void sound_set_volume_impl(sound_volume_t volume) {
    if (volume != SOUND_VOLUME_OFF) {
        out_level = volume;  // see Note A
    }
    if (volume >= HBRIDGE_MIN_VOLUME) {
        HBRIDGE_ENABLE();
    } else {
        HBRIDGE_DISABLE();
    }
    global_volume = volume;
}

sound_volume_t sound_get_volume_impl(void) {
    return global_volume;
}

void sound_set_channel_volume_impl(uint8_t channel, sound_channel_volume_t volume) {
    if (channel == 2) {
        channel2_volume = volume;
    }
    // only one supported level for other channels
}

sound_channel_volume_t sound_get_channel_volume_impl(uint8_t channel) {
    if (channel == 0) {
        return SOUND_CHANNEL0_VOLUME0;
    } else if (channel == 1) {
        return SOUND_CHANNEL1_VOLUME0;
    } else {
        return channel2_volume;
    }
}

// TCB interrupts:
// - update channel output level bit field
// - update TCA0 PWM duty cycle

ISR(TCB0_INT_vect) {
    uint8_t level = out_level;
    level ^= SOUND_CHANNEL0_VOLUME0;
    TCA0.SPLIT.HCMP0 = PWM_LEVELS[level];
    out_level = level;
    TCB0.INTFLAGS = TCB_CAPT_bm;
}

ISR(TCB1_INT_vect) {
    uint8_t level = out_level;
    level ^= SOUND_CHANNEL1_VOLUME0;
    TCA0.SPLIT.HCMP0 = PWM_LEVELS[level];
    out_level = level;
    TCB1.INTFLAGS = TCB_CAPT_bm;
}

ISR(TCB2_INT_vect) {
    uint8_t level = out_level;
    level ^= channel2_volume;
    TCA0.SPLIT.HCMP0 = PWM_LEVELS[level];
    out_level = level;
    TCB2.INTFLAGS = TCB_CAPT_bm;
}
