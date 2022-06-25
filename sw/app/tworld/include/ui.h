
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

#ifndef TWORLD_UI_H
#define TWORLD_UI_H

#include "game.h"

void open_main_menu_dialog(void);

void open_level_packs_dialog(void);

void open_levels_dialog(void);

void open_password_dialog(void);

void open_level_info_dialog(void);

void open_pause_dialog(void);

void open_options_dialog(uint8_t result_pos, uint8_t result_neg);

void open_controls_dialog(uint8_t result);

void open_level_fail_dialog(void);

void open_level_complete_dialog(void);

#endif //TWORLD_UI_H
