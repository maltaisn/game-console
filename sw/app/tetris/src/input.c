
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

#include <input.h>
#include <assets.h>
#include <save.h>
#include <music.h>
#include <tetris.h>

#include <core/dialog.h>

static uint8_t last_input_state;
// mask indicating buttons which should be considered not pressed until released.
static uint8_t input_wait_released;
// number of game ticks that buttons has been held.
static uint8_t click_processed;
// time since buttons was pressed, in game ticks.
static uint8_t button_hold_time[BUTTONS_COUNT];
// mask indicating for which buttons DAS is currently enabled.
static uint8_t delayed_auto_shift;

static uint8_t preprocess_input_state() {
    uint8_t state = input_get_state();
    // if any button was released, update wait mask.
    input_wait_released &= state;
    // consider any buttons on wait mask as not pressed.
    state &= ~input_wait_released;
    return state;
}

game_state_t game_handle_input_dialog(void) {
    dialog_result_t res = dialog_handle_input(last_input_state, preprocess_input_state());
    last_input_state = input_get_state();

    if (game.state == GAME_STATE_OPTIONS || game.state == GAME_STATE_OPTIONS_PLAY) {
        // apply options as they are changed
        // this will have to be undone if options dialog is cancelled.
        update_sound_volume(dialog.items[0].number.value);
        update_display_contrast(dialog.items[3].number.value);
        if (dialog.items[1].choice.selection == 0) {
            game.options.features &= ~GAME_FEATURE_MUSIC;
        } else {
            game.options.features |= GAME_FEATURE_MUSIC;
        }
        update_music_enabled();
    }

    if (res == DIALOG_RESULT_NONE) {
        return game.state;
    }
    game.dialog_shown = false;

    if (res == RESULT_NEW_GAME) {
        game_start();
        return GAME_STATE_PLAY;

    } else if (res == RESULT_RESUME_GAME) {
        game_ignore_current_input();
        return GAME_STATE_PLAY;

    } else if (res == RESULT_PAUSE_GAME) {
        return GAME_STATE_PAUSE;

    } else if (res == RESULT_GAME_OVER) {
        return GAME_STATE_GAME_OVER;

    } else if (res == RESULT_OPEN_OPTIONS) {
        game.old_features = game.options.features;
        return GAME_STATE_OPTIONS;

    } else if (res == RESULT_OPEN_OPTIONS_PLAY) {
        game.old_features = game.options.features;
        return GAME_STATE_OPTIONS_PLAY;

    } else if (res == RESULT_OPEN_OPTIONS_EXTRA) {
        save_dialog_options();  // save changes or they will be lost!
        return GAME_STATE_OPTIONS_EXTRA;

    } else if (res == RESULT_OPEN_CONTROLS) {
        return GAME_STATE_CONTROLS;

    } else if (res == RESULT_OPEN_CONTROLS_PLAY) {
        return GAME_STATE_CONTROLS_PLAY;

    } else if (res == RESULT_OPEN_LEADERBOARD) {
        return GAME_STATE_LEADERBOARD;

    } else if (res == RESULT_SAVE_OPTIONS_EXTRA) {
        save_dialog_extra_options();
        return GAME_STATE_OPTIONS;

    } else if (res == RESULT_SAVE_HIGHSCORE) {
        return save_highscore();

    } else if (res == RESULT_SAVE_OPTIONS) {
        save_dialog_options();

    } else if (res == RESULT_SAVE_OPTIONS_PLAY) {
        save_dialog_options();
        return GAME_STATE_PAUSE;

    } else if (res == RESULT_CANCEL_OPTIONS || res == RESULT_CANCEL_OPTIONS_PLAY) {
        // restore old options changed by preview feature
        game.options.features = game.old_features;
        update_sound_volume(game.options.volume);
        update_display_contrast(game.options.contrast);
        update_music_enabled();
        if (res == RESULT_CANCEL_OPTIONS_PLAY) {
            return GAME_STATE_PAUSE;
        }
    }

    game_music_start(ASSET_MUSIC_MENU, MUSIC_FLAG_LOOP | MUSIC_FLAG_DELAYED);
    return GAME_STATE_MAIN_MENU;
}

game_state_t game_handle_input_tetris(void) {
    uint8_t curr_state = preprocess_input_state();
    last_input_state = input_get_state();

    // update buttons hold time
    // use hold time to determine which buttons were recently clicked.
    uint8_t mask = BUTTON0;
    uint8_t clicked = 0;        // clicked buttons (pressed and click wasn't processed)
    uint8_t das_triggered = 0;  // DAS triggered on this frame
    uint8_t pressed_count = 0;  // number of pressed buttons
    uint8_t last_hold_time = 0;
    for (uint8_t i = 0; i < BUTTONS_COUNT; ++i) {
        uint8_t hold_time = button_hold_time[i];
        if (curr_state & mask) {
            // button pressed or held
            if (hold_time != UINT8_MAX) {
                ++hold_time;

                // delayed auto shift: enable if button is pressed long enough, then
                // holding button will result in a repeated click at a fixed interval.
                if (delayed_auto_shift & mask) {
                    // DAS enabled for that button, trigger it if auto repeat delay passed.
                    if (hold_time >= DAS_DELAY + AUTO_REPEAT_RATE) {
                        hold_time = DAS_DELAY;
                        das_triggered |= mask;
                    }
                } else if (hold_time >= DAS_DELAY && (DAS_MASK & mask)) {
                    delayed_auto_shift |= mask;
                    if ((delayed_auto_shift & DAS_DISALLOWED) == DAS_DISALLOWED) {
                        // disallowed combination of active DAS, disable them all.
                        delayed_auto_shift = 0;
                    } else {
                        das_triggered |= mask;
                        hold_time = DAS_DELAY;
                    }
                } else if (!(click_processed & mask)) {
                    // button is pressed and click wasn't processed yet: trigger a click.
                    last_hold_time = hold_time;
                    clicked |= mask;
                }
            }
            ++pressed_count;
        } else {
            // button released
            hold_time = 0;
            click_processed &= ~mask;
            delayed_auto_shift &= ~mask;
        }
        button_hold_time[i] = hold_time;
        mask <<= 1;
    }

    if (!das_triggered && pressed_count == 1 && last_hold_time <= BUTTON_COMBINATION_DELAY) {
        // Single button pressed, wait minimum time for other button to be pressed and
        // create a two buttons combination. After that delay, treat as single button click.
        return GAME_STATE_PLAY;
    }

    uint8_t clicked_or_das = clicked | das_triggered;
    if (clicked_or_das) {
        // process the click action
        if ((clicked & BUTTON_PAUSE) == BUTTON_PAUSE) {
            click_processed |= BUTTON_PAUSE;
            return GAME_STATE_PAUSE;

        } else if ((clicked & BUTTON_HARD_DROP) == BUTTON_HARD_DROP) {
            tetris_hard_drop();
            click_processed |= BUTTON_HARD_DROP;

        } else if ((clicked_or_das & BUTTON_LEFT) == BUTTON_LEFT) {
            // if DAS is enabled for right button, don't move left, it looks weird.
            if (!(delayed_auto_shift & BUTTON_RIGHT)) {
                tetris_move_left();
            }
            click_processed |= BUTTON_LEFT;

        } else if ((clicked_or_das & BUTTON_RIGHT) == BUTTON_RIGHT) {
            // if DAS is enabled for left button, don't move right, it looks weird.
            if (!(delayed_auto_shift & BUTTON_LEFT)) {
                tetris_move_right();
            }
            click_processed |= BUTTON_RIGHT;

        } else if ((clicked_or_das & BUTTON_DOWN) == BUTTON_DOWN) {
            tetris_move_down();
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

void game_ignore_current_input(void) {
    input_wait_released = input_get_state();
}
