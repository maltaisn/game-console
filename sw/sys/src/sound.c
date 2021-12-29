
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

#define TCA_ENABLE() (TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc | TCA_SPLIT_ENABLE_bm)
#define TCA_DISABLE() (TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc)

#define TCB_ENABLE(tcb) ((tcb).CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm)
#define TCB_DISABLE(tcb) ((tcb).CTRLA = TCB_CLKSEL_CLKDIV2_gc)

void sound_set_output_enabled(bool enabled) {
    // see Note A
    TCB_DISABLE(TCB0);
    TCB_DISABLE(TCB1);
    TCB_DISABLE(TCB2);

    if (enabled) {
        TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP0EN_bm;
        PORTA.PIN2CTRL = PORT_INVEN_bm;
        TCA_ENABLE();
    } else {
        TCA0.SPLIT.CTRLB = 0;
        PORTA.PIN2CTRL = 0;
        VPORTA.OUT |= PIN2_bm | PIN3_bm;
        TCA_DISABLE();
    }
}

void sound_play_note(const track_t* track, uint8_t channel) {
    bool has_note = track->note != TRACK_NO_NOTE;
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

void sound_set_volume_impl(sound_volume_t volume) {
    out_level = (out_level & CHANNELS_ALL_ON) | volume;
}

sound_volume_t sound_get_volume_impl(void) {
    return out_level & ~CHANNELS_ALL_ON;
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
