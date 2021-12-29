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

static sound_volume_t volume;
static bool output_enabled;

void sound_set_output_enabled(bool enabled) {
    output_enabled = enabled;
}

bool sound_is_output_enabled(void) {
    return output_enabled;
}

void sound_set_volume_impl(sound_volume_t vol) {
    volume = vol;
}

sound_volume_t sound_get_volume_impl(void) {
    return volume;
}

void sound_play_note(const track_t* track, uint8_t channel) {
    // TODO
}
