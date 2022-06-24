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

#ifndef TWORLD_GAME_H
#define TWORLD_GAME_H

#include "assets.h"
#include "tworld_level.h"

#include <stdbool.h>
#include <core/sound.h>

// display maximum number of FPS, during gameplay.
#define DISPLAY_MAX_FPS_GAME 16
// same, but in all other game states, which don't require as high FPS rate.
#define DISPLAY_MAX_FPS 8

// game tick in number of system ticks, on which a state update is made and input is read.
// this equals to 16 ticks per second, or 62.5 ms per tick.
#define GAME_TICK 16

// maximum delta time in game ticks
#define MAX_DELTA_TIME 4

typedef enum {
    // states with art background
    GAME_STATE_MAIN_MENU = 0,
    GAME_STATE_CONTROLS = 1,
    GAME_STATE_OPTIONS = 2,
    // states with no background
    GAME_STATE_PASSWORD,
    GAME_STATE_LEVEL_PACKS,  // no battery indicator (first)
    GAME_STATE_LEVELS,  // no battery indicator
    // states with game background
    GAME_STATE_PLAY,  // no battery indicator (last)
    GAME_STATE_LEVEL_FAIL,
    GAME_STATE_LEVEL_COMPLETE,
    GAME_STATE_OPTIONS_PLAY,
    GAME_STATE_CONTROLS_PLAY,
    GAME_STATE_PAUSE,
} game_state_t;

// all dialog result codes
enum {
    RESULT_START_LEVEL,
    RESULT_NEXT_LEVEL,
    RESULT_PAUSE,
    RESULT_RESUME,
    RESULT_LEVEL_FAIL,
    RESULT_LEVEL_COMPLETE,
    RESULT_ENTER_PASSWORD,
    RESULT_OPEN_LEVEL_PACKS,
    RESULT_OPEN_LEVELS,
    RESULT_OPEN_PASSWORD,
    RESULT_OPEN_OPTIONS,
    RESULT_OPEN_OPTIONS_PLAY,
    RESULT_OPEN_CONTROLS,
    RESULT_OPEN_CONTROLS_PLAY,
    RESULT_OPEN_MAIN_MENU,
    RESULT_SAVE_OPTIONS,
    RESULT_SAVE_OPTIONS_PLAY,
    RESULT_CANCEL_OPTIONS,
    RESULT_CANCEL_OPTIONS_PLAY,
    RESULT_TERMINATE,
};

enum {
    GAME_FEATURE_MUSIC = 1 << 0,
};

enum {
    FLAG_DIALOG_SHOWN = 1 << 0,
    FLAG_LINKS_CACHED = 1 << 1,
    FLAG_INVENTORY_SHOWN = 1 << 2,
};

typedef struct {
    uint8_t features;
    sound_volume_t volume; // 0-4
    uint8_t contrast;  // 0-10
    uint8_t unlocked_packs;
} game_options_t;

typedef struct {
    // options
    game_options_t options;
    uint8_t old_features;

    // general state handling
    uint8_t flags;
    game_state_t state;
    game_state_t last_state;
    uint8_t state_delay;

    // used for level pack and level selection dialogs.
    uint8_t pos_selection_x;
    uint8_t pos_selection_y;
    uint8_t pos_first_y;
    uint8_t pos_max_x;
    uint8_t pos_max_y;
    uint8_t pos_shown_y;
    uint8_t pos_last_x;

    // game-related
    level_pack_idx_t current_pack;
    level_idx_t current_level;
} game_t;

extern game_t game;

#endif //TWORLD_GAME_H
