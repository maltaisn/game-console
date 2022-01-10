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

#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H

#include <sys/input.h>
#include <sys/sound.h>

#include <stdbool.h>

// display frames per second
#define DISPLAY_FPS 5

// Keybindings, can be a single button or two buttons
#define BUTTON_LEFT      (BUTTON1)
#define BUTTON_RIGHT     (BUTTON5)
#define BUTTON_DOWN      (BUTTON3)
#define BUTTON_ROT_CW    (BUTTON4)
#define BUTTON_ROT_CCW   (BUTTON0)
#define BUTTON_HOLD      (BUTTON2)
#define BUTTON_HARD_DROP (BUTTON1 | BUTTON5)
#define BUTTON_PAUSE     (BUTTON0 | BUTTON4)

// If a single button is pressed, wait for this delay in game ticks for a
// second button click to create a two-buttons combination.
// This does introduce a XX ms delay between click and action.
#define BUTTON_COMBINATION_DELAY 2

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

// all dialog result codes
enum {
    RESULT_NEW_GAME,
    RESULT_RESUME_GAME,
    RESULT_OPEN_OPTIONS,
    RESULT_OPEN_OPTIONS_EXTRA,
    RESULT_OPEN_LEADERBOARD,
    RESULT_OPEN_MAIN_MENU,
    RESULT_SAVE_OPTIONS,
    RESULT_SAVE_OPTIONS_EXTRA,
    RESULT_SAVE_HIGHSCORE,
};

enum {
    GAME_FEATURE_MUSIC = 1 << 0,
    GAME_FEATURE_SOUND_EFFECTS = 1 << 1,
};

typedef struct {
    uint8_t features;
    sound_volume_t volume;
    uint8_t contrast;  // 0-10
} game_options_t;

typedef struct {
    game_options_t options;
    game_state_t state;
    bool dialog_shown;
} game_t;

extern game_t game;

game_state_t main_loop(void);

game_state_t game_loop(void);

game_state_t handle_dialog_input(void);

game_state_t handle_game_input(void);

void start_game(void);

void resume_game(void);

void save_highscore(void);

void save_options(void);

void save_extra_options(void);

void on_sleep_scheduled(void);

#endif //TETRIS_GAME_H
