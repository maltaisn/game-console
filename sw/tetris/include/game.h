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

#ifndef TETRIS_H
#define TETRIS_H

// display frames per second
#include <stdbool.h>

#define DISPLAY_FPS 5
// game tick in milliseconds, on which a state update is made and input is read.
#define GAME_TICK 50

typedef enum {
    // states with art background
    GAME_STATE_MAIN_MENU,
    GAME_STATE_OPTIONS,
    GAME_STATE_OPTIONS_EXTRA,
    GAME_STATE_LEADERBOARD,
    GAME_STATE_PLAY,
    // states with game background
    GAME_STATE_PAUSE,
    GAME_STATE_GAME_OVER,
    GAME_STATE_HIGH_SCORE,
} game_state_t;

typedef struct {
    game_state_t state;
    bool dialog_shown;
} game_t;

extern game_t game;

game_state_t main_loop(void);

game_state_t game_loop(void);

game_state_t handle_dialog_input(void);

game_state_t handle_game_input(void);

void start_game(void);

void save_highscore(void);

void save_options(void);

void save_extra_options(void);

void open_main_menu_dialog(void);

void open_pause_dialog(void);

void open_options_dialog(void);

void open_extra_options_dialog(void);

void open_leaderboard_dialog(void);

void open_high_score_dialog(void);

void open_game_over_dialog(void);

void draw(void);

void draw_game(void);

void on_sleep_scheduled(void);

#endif //TETRIS_H
