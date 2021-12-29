
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

#include <sys/flash.h>
#include <sys/time.h>

#include <core/sound.h>

#include <stdint.h>
#include <stdbool.h>

/**
 * Enable or disable buzzer output.
 */
void sound_set_output_enabled(bool enabled);

/**
 * Play current note of track on sound channel.
 */
void sound_play_note(const track_t* track, uint8_t channel);

/**
 * Low-level implementation for setting volume.
 * The core wrapper `sound_set_volume` should be used instead.
 */
void sound_set_volume_impl(sound_volume_t volume);

/**
 * Low-level implementation for getting volume.
 * The core wrapper `sound_get_volume` should be used instead.
 */
sound_volume_t sound_get_volume_impl(void);

#endif //SYS_SOUND_H
