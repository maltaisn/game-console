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

// delay in number of game ticks after level ends before showing dialog.
#define LEVEL_FAIL_STATE_DELAY 16  // 1000 ms
#define LEVEL_COMPLETE_STATE_DELAY 8  // 500 ms

// low timer overlay will be shown if time left is less than this value.
#define LOW_TIMER_THRESHOLD (20 * TICKS_PER_SECOND)

typedef enum {
    GAME_STATE_MAIN_MENU = 0,
    GAME_STATE_HELP = 1,
    GAME_STATE_OPTIONS = 2,
    GAME_SSEP_COVER_BG,
    GAME_STATE_PASSWORD,
    GAME_SSEP_NO_BAT_START,
    GAME_SSEP_VERT_NAV_START,
    GAME_STATE_LEVEL_PACKS,
    GAME_STATE_LEVELS,
    GAME_SSEP_LEVEL_BG,
    GAME_STATE_HINT,
    GAME_SSEP_VERT_NAV_END,
    GAME_STATE_PLAY,
    GAME_STATE_LEVEL_INFO,
    GAME_SSEP_NO_BAT_END,
    GAME_STATE_LEVEL_FAIL,
    GAME_STATE_LEVEL_COMPLETE,
    GAME_STATE_HELP_PLAY,
    GAME_STATE_OPTIONS_PLAY,
    GAME_STATE_PAUSE,
} game_state_t;

// all dialog result codes
enum {
    RESULT_LEVEL_INFO,
    RESULT_START_LEVEL,
    RESULT_RESTART_LEVEL,
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
    RESULT_OPEN_HELP,
    RESULT_OPEN_HELP_PLAY,
    RESULT_OPEN_MAIN_MENU,
    RESULT_SAVE_OPTIONS,
    RESULT_SAVE_OPTIONS_PLAY,
    RESULT_CANCEL_OPTIONS,
    RESULT_CANCEL_OPTIONS_PLAY,
    RESULT_TERMINATE,
};

enum {
    GAME_FEATURE_MUSIC = 1 << 0,
    GAME_FEATURE_SOUND_EFFECTS = 1 << 1,
};

enum {
    /** Set when a dialog is currently shown. */
    FLAG_DIALOG_SHOWN = 1 << 0,
    /** Set when trap and cloner links have been cached to a buffer in RAM. */
    FLAG_LINKS_CACHED = 1 << 1,
    /** Set when inventory overlay is shown. */
    FLAG_INVENTORY_SHOWN = 1 << 2,
    /** Set when game has been started (timer is counting). */
    FLAG_GAME_STARTED = 1 << 3,
    /** Set if current level was unlocked with a password. */
    FLAG_PASSWORD_USED = 1 << 5,
};

typedef struct {
    uint8_t features;
    sound_volume_t volume; // 0-4
    uint8_t contrast;  // 0-10
} PACK_STRUCT game_options_t;

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

    // current level information
    level_pack_idx_t current_pack;
    level_idx_t current_level;
    uint16_t current_level_pos;

    // misc
    uint8_t anim_state;
} game_t;

extern game_t game;

/**
 * Hide the inventory overlay if currently shown.
 */
void game_hide_inventory(void);

#endif //TWORLD_GAME_H
