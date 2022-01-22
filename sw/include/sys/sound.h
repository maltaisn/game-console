
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

#include <stdint.h>
#include <stdbool.h>

// The number of sound channels supported
#define SOUND_CHANNELS_COUNT 3

#define SOUND_PWM_MAX 32

// Note value indicating nothing is being played
#define SOUND_NO_NOTE 0x54

typedef enum {
    SOUND_VOLUME_0,
    SOUND_VOLUME_1,
    SOUND_VOLUME_2,
    SOUND_VOLUME_3,
    SOUND_VOLUME_OFF = 0xff,
} sound_volume_t;

typedef enum {
    SOUND_CHANNEL0_VOLUME0 = (1 << 2),
    SOUND_CHANNEL1_VOLUME0 = (1 << 3),
    SOUND_CHANNEL2_VOLUME0 = (1 << 4),
    SOUND_CHANNEL2_VOLUME1 = (1 << 5),
} sound_channel_volume_t;

/**
 * Enable or disable buzzer output.
 */
void sound_set_output_enabled(bool enabled);

/**
 * Play current note of track on sound channel.
 */
void sound_play_note(uint8_t note, uint8_t channel);

/**
 * Low-level implementation for setting global volume.
 * The core wrapper `sound_set_volume` must be used instead.
 * This implementation does not handle SOUND_VOLUME_OFF.
 */
void sound_set_volume_impl(sound_volume_t volume);

/**
 * Low-level implementation for getting volume.
 * The core wrapper `sound_get_volume` should be used instead.
 */
sound_volume_t sound_get_volume_impl(void);

/**
 * Low-level implementation for setting channel volume.
 * The core wrapper `sound_set_channel_volume` should be used instead.
 */
void sound_set_channel_volume_impl(uint8_t channel, sound_channel_volume_t volume);

/**
 * Low-level implementation for getting channel volume.
 * The core wrapper `sound_get_channel_volume` should be used instead.
 */
sound_channel_volume_t sound_get_channel_volume_impl(uint8_t channel);

#endif //SYS_SOUND_H
