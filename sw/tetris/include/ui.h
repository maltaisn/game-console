
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

#ifndef TETRIS_UI_H
#define TETRIS_UI_H

#include <game.h>

void open_main_menu_dialog(void);

void open_pause_dialog(void);

void open_options_dialog(void);

void open_extra_options_dialog(void);

void open_controls_dialog(game_state_t result);

void open_leaderboard_dialog(void);

void open_high_score_dialog(void);

void open_game_over_dialog(void);

#endif //TETRIS_UI_H
