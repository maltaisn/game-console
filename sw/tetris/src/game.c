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

#include <game.h>
#include <assets.h>
#include <tetris.h>
#include <ui.h>
#include <render.h>

#include <sys/main.h>
#include <sys/input.h>
#include <sys/display.h>
#include <sys/time.h>
#include <sys/power.h>

#include <core/graphics.h>
#include <core/sound.h>
#include <core/trace.h>
#include <core/dialog.h>
#include <core/random.h>

#include <sim/flash.h>

game_t game;

static systime_t last_draw_time;
static systime_t last_tick_time;

static sleep_cause_t last_sleep_cause;
static uint8_t last_input_state;
// number of game ticks that buttons has been held.
static uint8_t click_processed;
static uint8_t button_hold_time[BUTTONS_COUNT];

void setup(void) {
#ifdef SIMULATION
    FILE* file = fopen("assets.dat", "r");
    flash_load_file(file);
    fclose(file);
#endif

    dialog_set_font(ASSET_FONT_7X7, ASSET_FONT_5X7, GRAPHICS_BUILTIN_FONT);

    // TODO load options

    // default options
    game.options = (game_options_t) {
        .features = GAME_FEATURE_MUSIC | GAME_FEATURE_SOUND_EFFECTS,
        .volume = SOUND_VOLUME_2,
        .contrast = 7,
    };
    tetris.options = (tetris_options_t) {
        .features = TETRIS_FEATURE_HOLD | TETRIS_FEATURE_GHOST |
                TETRIS_FEATURE_WALL_KICKS | TETRIS_FEATURE_TSPINS,
        .preview_pieces = 5,
    };
}

void loop(void) {
    systime_t time;
    do {
        time = time_get();
    } while (time - last_tick_time < GAME_TICK);
    last_tick_time = time;

    // sleep detection
    if (power_get_scheduled_sleep_cause() != last_sleep_cause) {
        last_sleep_cause = power_get_scheduled_sleep_cause();
        on_sleep_scheduled();
    }

    game.state = main_loop();

    last_input_state = input_get_state();

    if (last_draw_time - time > millis_to_ticks(1000.0 / DISPLAY_FPS)) {
        last_draw_time = time;
        display_first_page();
        do {
            draw();
        } while (display_next_page());
    }
}

game_state_t main_loop(void) {
    game_state_t s = game.state;
    if (s == GAME_STATE_PLAY) {
        return game_loop();
    } else if (!game.dialog_shown) {
        // all other states show a dialog, and it wasn't initialized yet.
        if (s == GAME_STATE_MAIN_MENU) {
            open_main_menu_dialog();
        } else if (s == GAME_STATE_PAUSE) {
            open_pause_dialog();
        } else if (s == GAME_STATE_HIGH_SCORE) {
            open_high_score_dialog();
        } else if (s == GAME_STATE_GAME_OVER) {
            open_game_over_dialog();
        } else if (s == GAME_STATE_OPTIONS) {
            open_options_dialog();
        } else if (s == GAME_STATE_OPTIONS_EXTRA) {
            open_extra_options_dialog();
        } else { // s == GAME_STATE_LEADERBOARD
            open_leaderboard_dialog();
        }
        game.dialog_shown = true;
    }
    return handle_dialog_input();
}

game_state_t game_loop(void) {
    game_state_t new_state = handle_game_input();
    if (new_state != GAME_STATE_PLAY) {
        return new_state;
    }

    tetris_update();
    if (tetris.flags & GAME_STATE_GAME_OVER) {
        // TODO check highscore and show dialog
        return GAME_STATE_GAME_OVER;
    }

    return GAME_STATE_PLAY;
}

game_state_t handle_dialog_input(void) {
    dialog_result_t res = dialog_handle_input(last_input_state);
    if (res == DIALOG_RESULT_NONE) {
        return game.state;
    }
    game.dialog_shown = false;

    if (res == RESULT_NEW_GAME) {
        start_game();
        return GAME_STATE_PLAY;

    } else if (res == RESULT_RESUME_GAME) {
        resume_game();
        return GAME_STATE_PLAY;

    } else if (res == RESULT_OPEN_OPTIONS) {
        return GAME_STATE_OPTIONS;

    } else if (res == RESULT_OPEN_OPTIONS_EXTRA) {
        return GAME_STATE_OPTIONS_EXTRA;

    } else if (res == RESULT_OPEN_LEADERBOARD) {
        return GAME_STATE_LEADERBOARD;

    } else if (res == RESULT_SAVE_OPTIONS_EXTRA) {
        save_extra_options();
        return GAME_STATE_OPTIONS;

    } else if (res == RESULT_SAVE_OPTIONS) {
        save_options();

    } else if (res == RESULT_SAVE_HIGHSCORE) {
        save_highscore();
    }
    return GAME_STATE_MAIN_MENU;
}

game_state_t handle_game_input(void) {
    // update buttons hold time
    // use hold time to determine which buttons were recently clicked.
    uint8_t curr_state = input_get_state();
    uint8_t mask = BUTTON0;
    uint8_t clicked = 0;
    uint8_t pressed_count = 0;
    uint8_t last_hold_time = 0;
    for (uint8_t i = 0; i < BUTTONS_COUNT; ++i) {
        uint8_t hold_time = button_hold_time[i];
        if (curr_state & mask) {
            // button still held
            if (hold_time != UINT8_MAX) {
                ++hold_time;
            }
            if (!(click_processed & mask)) {
                last_hold_time = hold_time;
                clicked |= mask;
            }
            ++pressed_count;
        } else {
            // button released
            hold_time = 0;
            click_processed &= ~mask;
        }
        button_hold_time[i] = hold_time;
        mask <<= 1;
    }

    if (pressed_count == 1 && last_hold_time <= BUTTON_COMBINATION_DELAY) {
        // Single button pressed, wait minimum time for other button to be pressed and
        // create a two buttons combination. After that delay, treat as single button click.
        return GAME_STATE_PLAY;
    }

    if (clicked) {
        // process the click action
        if ((clicked & BUTTON_PAUSE) == BUTTON_PAUSE) {
            click_processed |= BUTTON_PAUSE;
            return GAME_STATE_PAUSE;

        } else if ((clicked & BUTTON_HARD_DROP) == BUTTON_HARD_DROP) {
            tetris_hard_drop();
            click_processed |= BUTTON_HARD_DROP;

        } else if ((clicked & BUTTON_LEFT) == BUTTON_LEFT) {
            tetris_move_left();
            click_processed |= BUTTON_LEFT;

        } else if ((clicked & BUTTON_RIGHT) == BUTTON_RIGHT) {
            tetris_move_right();
            click_processed |= BUTTON_RIGHT;

        } else if ((clicked & BUTTON_DOWN) == BUTTON_DOWN) {
            tetris_move_down(true);
            click_processed |= BUTTON_DOWN;

        } else if ((clicked & BUTTON_ROT_CW) == BUTTON_ROT_CW) {
            tetris_rotate_piece(TETRIS_DIR_CW);
            click_processed |= BUTTON_ROT_CW;

        } else if ((clicked & BUTTON_ROT_CCW) == BUTTON_ROT_CCW) {
            tetris_rotate_piece(TETRIS_DIR_CCW);
            click_processed |= BUTTON_ROT_CCW;

        } else if ((clicked & BUTTON_HOLD) == BUTTON_HOLD) {
            tetris_hold_or_swap_piece();
            click_processed |= BUTTON_HOLD;
        }
    }

    return GAME_STATE_PLAY;
}

void start_game(void) {
    random_seed(time_get());
    tetris_init();
    resume_game();
}

void resume_game(void) {
    // reset input state
    click_processed = BUTTONS_ALL;
    for (uint8_t i = 0; i < BUTTONS_COUNT; ++i) {
        button_hold_time[i] = 0;
    }
}

void save_highscore(void) {
    // TODO
}

void save_options(void) {
    // TODO
}

void save_extra_options(void) {
    // TODO
}

void on_sleep_scheduled(void) {
    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        sound_set_output_enabled(false);
    }
}
