
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

void load_from_eeprom(void);

void save_to_eeprom(void);

void save_dialog_options(void);

void update_display_contrast(uint8_t value);

void update_sound_volume(uint8_t volume);

void update_music_enabled(void);

#endif //TWORLD_SAVE_H
