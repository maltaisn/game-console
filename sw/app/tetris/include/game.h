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

#include <core/sound.h>

#include <stdbool.h>

// display frames per second
#ifdef SIMULATION
#define DISPLAY_MAX_FPS 24  // faster for debugging
#else
#define DISPLAY_MAX_FPS 8
#endif

// game tick in number of system ticks, on which a state update is made and input is read.
// this equals to 64 ticks per second, or roughly 15.6 ms per tick.
#define GAME_TICK 4

// maximum delta time in game ticks
#define MAX_DELTA_TIME 16

#define HIGHSCORE_NAME_MAX_LENGTH 12
// maximum number of entries in leaderboard
#define LEADERBOARD_MAX_SIZE 10

// delay in game ticks to wait before showing dialog after game over.
#define GAME_OVER_DELAY 48

typedef enum {
    // states with art background
    GAME_STATE_MAIN_MENU,
    GAME_STATE_OPTIONS,
    GAME_STATE_OPTIONS_EXTRA,
    GAME_STATE_CONTROLS,
    GAME_STATE_LEADERBOARD,
    // states with game background
    GAME_STATE_GAME_OVER,
    GAME_STATE_HIGH_SCORE,
    GAME_STATE_PLAY,
    GAME_STATE_OPTIONS_PLAY,
    GAME_STATE_CONTROLS_PLAY,
    GAME_STATE_LEADERBOARD_PLAY,
    GAME_STATE_PAUSE,
} game_state_t;

// all dialog result codes
enum {
    RESULT_NEW_GAME,
    RESULT_PAUSE_GAME,
    RESULT_RESUME_GAME,
    RESULT_GAME_OVER,
    RESULT_OPEN_OPTIONS,
    RESULT_OPEN_OPTIONS_PLAY,
    RESULT_OPEN_OPTIONS_EXTRA,
    RESULT_OPEN_CONTROLS,
    RESULT_OPEN_CONTROLS_PLAY,
    RESULT_OPEN_LEADERBOARD,
    RESULT_OPEN_MAIN_MENU,
    RESULT_SAVE_OPTIONS,
    RESULT_SAVE_OPTIONS_PLAY,
    RESULT_CANCEL_OPTIONS,
    RESULT_CANCEL_OPTIONS_PLAY,
    RESULT_SAVE_OPTIONS_EXTRA,
    RESULT_SAVE_HIGHSCORE,
};

enum {
    GAME_FEATURE_MUSIC = 1 << 0,
    GAME_FEATURE_SOUND_EFFECTS = 1 << 1,
};

// note: structs are stored in eeprom in the same layout as in memory.
// if the any of the following structs is changed, the version should be changed:
// - game_options_t
// - tetris_options_t
// - game_highscore_t
// - game_leaderboard_t

typedef struct {
    uint8_t features;
    sound_volume_t volume; // 0-4
    uint8_t contrast;  // 0-10
} game_options_t;

typedef struct {
    uint32_t score;
    char name[HIGHSCORE_NAME_MAX_LENGTH + 1];
} game_highscore_t;

typedef struct {
    uint8_t size;
    game_highscore_t entries[LEADERBOARD_MAX_SIZE];
} game_leaderboard_t;

typedef struct {
    game_options_t options;
    game_leaderboard_t leaderboard;

    game_state_t state;
    uint8_t state_delay;
    uint8_t new_highscore_pos;
    uint8_t old_features;
    bool dialog_shown;
} game_t;

extern game_t game;

void game_start(void);

#endif //TETRIS_GAME_H
