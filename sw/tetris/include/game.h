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
#define DISPLAY_FPS 24  // faster for debugging
#else
#define DISPLAY_FPS 8
#endif

// Keybindings, can be a single button or two buttons
#define BUTTON_LEFT      (BUTTON1)
#define BUTTON_RIGHT     (BUTTON5)
#define BUTTON_DOWN      (BUTTON3)
#define BUTTON_ROT_CW    (BUTTON4)
#define BUTTON_ROT_CCW   (BUTTON0)
#define BUTTON_HOLD      (BUTTON2)
#define BUTTON_HARD_DROP (BUTTON1 | BUTTON5)
#define BUTTON_PAUSE     (BUTTON0 | BUTTON4)

// Buttons for which delayed auto-shift is enabled.
#define DAS_MASK         (BUTTON1 | BUTTON3 | BUTTON5)
// Disallowed DAS mask (if all bits in mask are set, all DAS are disabled)
#define DAS_DISALLOWED (BUTTON_LEFT | BUTTON_RIGHT)

// If a single button is pressed, wait for this delay in game ticks for a
// second button click to create a two-buttons combination.
// This does introduce a 50 ms delay between click and action.
#define BUTTON_COMBINATION_DELAY 2

#define HIGHSCORE_NAME_MAX_LENGTH 12
#define LEADERBOARD_MAX_SIZE 10

#define MUSIC_NONE 0

typedef enum {
    // states with art background
    GAME_STATE_MAIN_MENU,
    GAME_STATE_OPTIONS,
    GAME_STATE_OPTIONS_EXTRA,
    GAME_STATE_CONTROLS,
    GAME_STATE_LEADERBOARD,
    // states with game background
    GAME_STATE_PLAY,
    GAME_STATE_CONTROLS_PLAY,
    GAME_STATE_LEADERBOARD_PLAY,
    GAME_STATE_PAUSE,
    GAME_STATE_GAME_OVER,
    GAME_STATE_HIGH_SCORE,
} game_state_t;

// all dialog result codes
enum {
    RESULT_NEW_GAME,
    RESULT_PAUSE_GAME,
    RESULT_RESUME_GAME,
    RESULT_GAME_OVER,
    RESULT_OPEN_OPTIONS,
    RESULT_OPEN_OPTIONS_EXTRA,
    RESULT_OPEN_CONTROLS,
    RESULT_OPEN_CONTROLS_PLAY,
    RESULT_OPEN_LEADERBOARD,
    RESULT_OPEN_MAIN_MENU,
    RESULT_SAVE_OPTIONS,
    RESULT_CANCEL_OPTIONS,
    RESULT_SAVE_OPTIONS_EXTRA,
    RESULT_SAVE_HIGHSCORE,
};

enum {
    GAME_FEATURE_MUSIC = 1 << 0,
    GAME_FEATURE_SOUND_EFFECTS = 1 << 1,
};

// note: structs are stored in eeprom in the same layout as in memory.
// if the any of the following structs is changed, the version should be changed:
// - game_header_t
// - game_options_t
// - tetris_options_t
// - game_highscore_t
// - game_leaderboard_t

typedef struct {
    uint8_t signature;
    uint8_t version_major;
    uint8_t version_minor;
} game_header_t;

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
    uint8_t new_highscore_pos;
    uint8_t old_features;
    bool dialog_shown;
    sound_t music;
} game_t;

extern game_t game;

game_state_t update_game_state(void);

game_state_t update_tetris_state(void);

game_state_t handle_dialog_input(void);

game_state_t handle_game_input(void);

void start_music(sound_t music, bool loop);

void stop_music(void);

void update_music(void);

void start_game(void);

void resume_game(void);

game_state_t save_highscore(void);

void update_display_contrast(uint8_t value);

void update_sound_volume(uint8_t volume);

void update_music_enabled(void);

void save_options(void);

void save_extra_options(void);

void set_default_options(void);

void load_from_eeprom(void);

void save_to_eeprom(void);

#endif //TETRIS_GAME_H
