
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
#include <sys/defs.h>

#include <avr/io.h>

#ifdef BOOTLOADER
#include <boot/defs.h>
#include <avr/interrupt.h>
#endif  //BOOTLOADER

#define GLOBAL_VOLUME_MASK 0x03

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

#define TCA_ENABLE() (TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc | TCA_SPLIT_ENABLE_bm)
#define TCA_DISABLE() (TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc)

#define TCB_ENABLE(tcb) ((tcb).CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm)
#define TCB_DISABLE(tcb) ((tcb).CTRLA = TCB_CLKSEL_CLKDIV2_gc)

#ifdef BOOTLOADER

BOOTLOADER_KEEP sound_volume_t sys_sound_global_volume; // = SOUND_VOLUME_0
BOOTLOADER_KEEP sound_channel_volume_t sys_sound_channel2_volume;  // initialized elsewhere

// Timer counts for TCA PWM timer.
// The value corresponds to the number of bits set in the 2-5 position in out_level,
// multiplied by an arbitrary constant to account for the global volume and channel volume.
// The maximum value in array must be SOUND_MAX_PWM.
// The condition where both masks for channel 2 are set must never happen (no associated values).
static uint8_t _PWM_LEVELS[] = {
        0, 0, 0, 0,
        1, 2, 4, 8,
        1, 2, 4, 8,
        2, 4, 8, 16,
        1, 2, 4, 8,
        2, 4, 8, 16,
        2, 4, 8, 16,
        3, 6, 12, 24,
        2, 4, 8, 16,
        3, 6, 12, 24,
        3, 6, 12, 24,
        4, 8, 16, 32,
};

/*
 * Timer counts for TCB channel timers, for each playable note.
 *
 * Counts are calculated using the following formula:
 *   [count] = round([f_cpu] / [prescaler] / [note frequency] / 2) - 1
 *
 * Maximum error is about 0.01 semitone (= 1 ct).
 * Timer counts can also be computed using the error_analysis.py script in the buzzer-midi project,
 * using the channel specification "0,72,5e6;-;-". The output is reproduced here:
 *
 *   Number   Note   Target freq (Hz)   Actual freq (Hz)   Timer count   Error (Hz)   Error (ct)
 *     0      C2           65.4               65.4            38222         -0.0         -0.0
 *     1      C#2          69.3               69.3            36076         +0.0         +0.0
 *     2      D2           73.4               73.4            34051         +0.0         +0.0
 *     3      D#2          77.8               77.8            32140         +0.0         +0.0
 *     4      E2           82.4               82.4            30336         +0.0         +0.0
 *     5      F2           87.3               87.3            28634         -0.0         -0.0
 *     6      F#2          92.5               92.5            27026         +0.0         +0.0
 *     7      G2           98.0               98.0            25510         -0.0         -0.0
 *     8      G#2         103.8              103.8            24078         -0.0         -0.0
 *     9      A2          110.0              110.0            22726         +0.0         +0.0
 *     10     A#2         116.5              116.5            21451         -0.0         -0.0
 *     11     B2          123.5              123.5            20247         -0.0         -0.0
 *     12     C3          130.8              130.8            19110         +0.0         +0.0
 *     13     C#3         138.6              138.6            18038         -0.0         -0.0
 *     14     D3          146.8              146.8            17025         +0.0         +0.0
 *     15     D#3         155.6              155.6            16070         -0.0         -0.0
 *     16     E3          164.8              164.8            15168         -0.0         -0.0
 *     17     F3          174.6              174.6            14316         +0.0         +0.0
 *     18     F#3         185.0              185.0            13513         -0.0         -0.0
 *     19     G3          196.0              196.0            12754         +0.0         +0.0
 *     20     G#3         207.7              207.7            12038         +0.0         +0.1
 *     21     A3          220.0              220.0            11363         -0.0         -0.1
 *     22     A#3         233.1              233.1            10725         -0.0         -0.0
 *     23     B3          246.9              246.9            10123         -0.0         -0.0
 *     24     C4          261.6              261.6            9555          -0.0         -0.1
 *     25     C#4         277.2              277.2            9018          +0.0         +0.1
 *     26     D4          293.7              293.7            8512          +0.0         +0.0
 *     27     D#4         311.1              311.1            8034          +0.0         +0.1
 *     28     E4          329.6              329.6            7583          +0.0         +0.1
 *     29     F4          349.2              349.2            7158          -0.0         -0.1
 *     30     F#4         370.0              370.0            6756          -0.0         -0.0
 *     31     G4          392.0              392.0            6377          -0.0         -0.1
 *     32     G#4         415.3              415.3            6019          -0.0         -0.1
 *     33     A4          440.0              440.0            5681          -0.0         -0.1
 *     34     A#4         466.2              466.2            5362          -0.0         -0.0
 *     35     B4          493.9              493.9            5061          -0.0         -0.0
 *     36     C5          523.3              523.2            4777          -0.0         -0.1
 *     37     C#5         554.4              554.3            4509          -0.0         -0.1
 *     38     D5          587.3              587.3            4256          -0.1         -0.2
 *     39     D#5         622.3              622.2            4017          -0.1         -0.1
 *     40     E5          659.3              659.3            3791          +0.0         +0.1
 *     41     F5          698.5              698.5            3578          +0.1         +0.2
 *     42     F#5         740.0              740.1            3377          +0.1         +0.2
 *     43     G5          784.0              783.9            3188          -0.0         -0.1
 *     44     G#5         830.6              830.6            3009          -0.0         -0.1
 *     45     A5          880.0              880.0            2840          -0.0         -0.1
 *     46     A#5         932.3              932.5            2680          +0.2         +0.3
 *     47     B5          987.8              987.8            2530          -0.0         -0.0
 *     48     C6          1046.5             1046.5           2388          -0.0         -0.1
 *     49     C#6         1108.7             1108.6           2254          -0.1         -0.1
 *     50     D6          1174.7             1174.8           2127          +0.2         +0.2
 *     51     D#6         1244.5             1244.4           2008          -0.1         -0.1
 *     52     E6          1318.5             1318.6           1895          +0.1         +0.1
 *     53     F6          1396.9             1396.6           1789          -0.3         -0.3
 *     54     F#6         1480.0             1480.2           1688          +0.2         +0.2
 *     55     G6          1568.0             1568.4           1593          +0.4         +0.4
 *     56     G#6         1661.2             1661.1           1504          -0.1         -0.1
 *     57     A6          1760.0             1760.6           1419          +0.6         +0.6
 *     58     A#6         1864.7             1864.3           1340          -0.4         -0.3
 *     59     B6          1975.5             1976.3           1264          +0.8         +0.7
 *     60     C7          2093.0             2093.8           1193          +0.8         +0.7
 *     61     C#7         2217.5             2218.3           1126          +0.8         +0.6
 *     62     D7          2349.3             2349.6           1063          +0.3         +0.2
 *     63     D#7         2489.0             2490.0           1003          +1.0         +0.7
 *     64     E7          2637.0             2637.1            947          +0.1         +0.1
 *     65     F7          2793.8             2793.3            894          -0.5         -0.3
 *     66     F#7         2960.0             2958.6            844          -1.4         -0.8
 *     67     G7          3136.0             3136.8            796          +0.8         +0.4
 *     68     G#7         3322.4             3324.5            751          +2.0         +1.1
 *     69     A7          3520.0             3521.1            709          +1.1         +0.6
 *     70     A#7         3729.3             3731.3            669          +2.0         +0.9
 *     71     B7          3951.1             3949.4            632          -1.6         -0.7
 *     72     C8          4186.0             4187.6            596          +1.6         +0.7
 */
static uint16_t _TIMER_NOTES[] = {
        38222, 36076, 34051, 32140, 30336, 28634, 27026, 25510, 24078, 22726, 21451, 20247,
        19110, 18038, 17025, 16070, 15168, 14316, 13513, 12754, 12038, 11363, 10725, 10123,
        9555, 9018, 8512, 8034, 7583, 7158, 6756, 6377, 6019, 5681, 5362, 5061,
        4777, 4509, 4256, 4017, 3791, 3578, 3377, 3188, 3009, 2840, 2680, 2530,
        2388, 2254, 2127, 2008, 1895, 1789, 1688, 1593, 1504, 1419, 1340, 1264,
        1193, 1126, 1063, 1003, 947, 894, 844, 796, 751, 709, 669, 632, 596,
};

void sys_sound_set_output_enabled(bool enabled) {
    if (enabled) {
        TCA_ENABLE();
    } else {
        TCA_DISABLE();
        VPORTA.OUT &= ~PIN3_bm;
    }
}

BOOTLOADER_NOINLINE
void sys_sound_play_note(uint8_t note, uint8_t channel) {
    bool has_note = note != SYS_SOUND_NO_NOTE;
    TCB_t* tcb = &TCB0 + channel;
    if (has_note) {
        tcb->CCMP = _TIMER_NOTES[note];
        TCB_ENABLE(*tcb);
    } else {
        TCB_DISABLE(*tcb);
        // see Note A
        out_level &= GLOBAL_VOLUME_MASK;
    }
}

// TCB interrupts:
// - update channel output level bit field
// - update TCA0 PWM duty cycle

ISR(TCB0_INT_vect) {
    uint8_t level = out_level;
    level ^= SOUND_CHANNEL0_VOLUME0;
    TCA0.SPLIT.HCMP0 = _PWM_LEVELS[level];
    out_level = level;
    TCB0.INTFLAGS = TCB_CAPT_bm;
}

ISR(TCB1_INT_vect) {
    uint8_t level = out_level;
    level ^= SOUND_CHANNEL1_VOLUME0;
    TCA0.SPLIT.HCMP0 = _PWM_LEVELS[level];
    out_level = level;
    TCB1.INTFLAGS = TCB_CAPT_bm;
}

ISR(TCB2_INT_vect) {
    uint8_t level = out_level;
    level ^= sys_sound_channel2_volume;
    TCA0.SPLIT.HCMP0 = _PWM_LEVELS[level];
    out_level = level;
    TCB2.INTFLAGS = TCB_CAPT_bm;
}

#endif  //BOOTLOADER

void sys_sound_set_volume(sound_volume_t volume) {
    if (volume != SOUND_VOLUME_OFF) {
        out_level = volume;  // see Note A
    }
    sys_sound_global_volume = volume;
}

ALWAYS_INLINE
sound_volume_t sys_sound_get_volume(void) {
    return sys_sound_global_volume;
}

ALWAYS_INLINE
void sys_sound_set_channel_volume(uint8_t channel, sound_channel_volume_t volume) {
    if (channel == 2) {
        sys_sound_channel2_volume = volume;
    }
    // only one supported level for other channels
}

sound_channel_volume_t sys_sound_get_channel_volume(uint8_t channel) {
    if (channel == 0) {
        return SOUND_CHANNEL0_VOLUME0;
    } else if (channel == 1) {
        return SOUND_CHANNEL1_VOLUME0;
    } else {
        return sys_sound_channel2_volume;
    }
}
