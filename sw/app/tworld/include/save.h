
/*
 * Copyright 2022 Nicolas Maltais
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

#ifndef TWORLD_SAVE_H
#define TWORLD_SAVE_H

#include "game.h"
#include "level.h"

#define SAVE_TIME_NONE 0x3ff

void load_from_eeprom(void);

void save_to_eeprom(void);

void save_dialog_options(void);

void update_display_contrast(uint8_t value);

void update_sound_volume(uint8_t volume);

void update_music_enabled(void);

/**
 * Returns the best time for a completed level or `SAVE_TIME_NONE` if not completed.
 * The time is given in number of seconds left when level is completed.
 * Returns the number of completed levels.
 */
uint16_t get_best_level_time(uint16_t pos);

/**
 * For `size` levels starting at `pos`, set a bit
 * in the `arr` bitset to 1 when the level is completed.
 */
uint8_t fill_completed_levels_array(uint16_t pos, uint8_t size, uint8_t* arr);

#endif //TWORLD_SAVE_H