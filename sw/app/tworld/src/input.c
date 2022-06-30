
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

#include "input.h"
#include "assets.h"
#include "save.h"
#include "render.h"
#include "render_utils.h"
#include "music.h"

#include <core/app.h>
#include <core/dialog.h>

#include <string.h>

// mask indicating buttons which should be considered not pressed until released.
static uint8_t input_wait_released;
// indicates pressed buttons for which click event has been processed.
static uint8_t click_processed;
// time since buttons was pressed, in game ticks.
static uint8_t button_hold_time[BUTTONS_COUNT];

static uint8_t preprocess_input_state() {
    uint8_t state = input_get_state();
    // if any button was released, update wait mask.
    input_wait_released &= state;
    // consider any buttons on wait mask as not pressed.
    state &= ~input_wait_released;
    return state;
}

static void apply_options_dialog_changes(void) {
    // this will have to be undone if options dialog is cancelled.
    update_sound_volume(dialog.items[0].number.value);
    update_display_contrast(dialog.items[2].number.value);
    if (dialog.items[1].choice.selection == 0) {
        game.options.features &= ~GAME_FEATURE_MUSIC;
    } else {
        game.options.features |= GAME_FEATURE_MUSIC;
    }
    update_music_enabled();
}

static dialog_result_t handle_vertical_navigation_input(void) {
    uint8_t clicked = input_get_clicked();
    if (clicked & BUTTON_LEFT) {
        if (game.pos_selection_x > 0) {
            --game.pos_selection_x;
        }

    } else if (clicked & BUTTON_RIGHT) {
        if (game.pos_selection_x < game.pos_max_x) {
            ++game.pos_selection_x;
            if (game.pos_selection_y == game.pos_max_y && game.pos_selection_x > game.pos_last_x) {
                // the last grid row may be incomplete, restrict the maximum X position.
                game.pos_selection_x = game.pos_last_x;
            }
        }

    } else if (clicked & BUTTON_UP) {
        if (game.pos_selection_y > 0) {
            --game.pos_selection_y;
            if (game.pos_first_y > game.pos_selection_y) {
                // scroll up
                --game.pos_first_y;
            }
        }

    } else if (clicked & BUTTON_DOWN) {
        if (game.pos_selection_y < game.pos_max_y) {
            ++game.pos_selection_y;
            if ((uint8_t) (game.pos_selection_y - game.pos_first_y) >= game.pos_shown_y) {
                // scroll down
                ++game.pos_first_y;
            }
            if (game.pos_selection_y == game.pos_max_y && game.pos_selection_x > game.pos_last_x) {
                // the last grid row may be incomplete, restrict the maximum X position.
                game.pos_selection_x = game.pos_last_x;
            }
        }

    } else if (clicked & DIALOG_BUTTON_ENTER) {
        // select level or level pack.
        if (game.state == GAME_STATE_LEVEL_PACKS) {
            if (game.pos_selection_y == LEVEL_PACK_COUNT) {
                return RESULT_OPEN_PASSWORD;
            } else if (game.options.unlocked_packs & (1 << game.pos_selection_y)) {
                // pack is unlocked, select it and go to level selection.
                game.current_pack = game.pos_selection_y;
                return RESULT_OPEN_LEVELS;
            }

        } else if (game.state == GAME_STATE_LEVELS) {
            // only start level if unlocked or previously completed.
            const level_pack_info_t* info = &tworld_packs.packs[game.current_pack];
            level_idx_t level = game.pos_selection_y * LEVELS_PER_SCREEN_H + game.pos_selection_x;
            if (level_is_unlocked(info, level)) {
                game.current_level = level;
                game.current_level_pos = info->pos + level;
                game.flags &= ~FLAG_PASSWORD_USED;
                return RESULT_LEVEL_INFO;
            }
        } // else game.state == GAME_STATE_HINT
    }

    return DIALOG_RESULT_NONE;
}

static void setup_level_packs_selection(void) {
    game.pos_selection_x = 0;
    game.pos_selection_y = 0;
    game.pos_first_y = 0;
    game.pos_max_x = 0;
    game.pos_max_y = LEVEL_PACK_COUNT;
    game.pos_shown_y = LEVEL_PACKS_PER_SCREEN;
}

static void setup_level_selection(const level_idx_t selection) {
    level_pack_info_t* info = &tworld_packs.packs[game.current_pack];

    game.pos_selection_x = selection % LEVELS_PER_SCREEN_H;
    game.pos_selection_y = selection / LEVELS_PER_SCREEN_H;
    game.pos_max_x = LEVELS_PER_SCREEN_H - 1;
    game.pos_max_y = (info->total_levels - 1) / LEVELS_PER_SCREEN_H;
    game.pos_last_x = (info->total_levels - 1) % LEVELS_PER_SCREEN_H;
    game.pos_shown_y = LEVELS_PER_SCREEN_V;

    game.pos_first_y = game.pos_selection_y;
    uint8_t max_first_y = game.pos_max_y - LEVELS_PER_SCREEN_V + 1;
    if (game.pos_first_y > max_first_y) {
        game.pos_first_y = max_first_y;
    }
}

static bool show_hint_if_needed(void) {
    const position_t pos = tworld_get_current_position();
    if (tworld_get_bottom_tile(pos) != TILE_HINT) {
        return false;
    }

    const flash_t hint = level_get_hint();
    uint8_t lines = find_text_line_count(hint, HINT_TEXT_WIDTH);
    game.pos_selection_x = 0;
    game.pos_selection_y = 0;
    game.pos_first_y = 0;
    game.pos_max_x = 0;
    game.pos_max_y = lines > HINT_LINES_PER_SCREEN ? lines - HINT_LINES_PER_SCREEN : 0;
    game.pos_shown_y = 1;
    return true;
}

static game_state_t start_level(void) {
    level_read_level();

    // don't immediately start updating the game state, wait for first input.
    game_ignore_current_input();
    game.flags &= ~FLAG_GAME_STARTED;

    // start music (will do nothing if already started)
    game_music_start_level_music(MUSIC_FLAG_LOOP | MUSIC_FLAG_DELAYED);

    return GAME_STATE_LEVEL_INFO;
}

static game_state_t next_level(void) {
    level_read_packs();
    const level_pack_info_t *info = &tworld_packs.packs[game.current_pack];

    if ((info->completed_levels == info->total_levels) || (game.flags & FLAG_PASSWORD_USED)) {
        // All levels have been completed, or level was accessed
        // via a password, go back to level selection.
        setup_level_selection(game.current_level);
        return GAME_STATE_LEVELS;
    }
    // If playing on last level but not all levels are completed, then the level
    // was necessarily unlocked by a password.
    // So at this point game.current_level < info->total_levels - 1;

    // Start next level.
    ++game.current_level;
    return start_level();
}

game_state_t game_handle_input_dialog(void) {
    dialog_result_t res = dialog_handle_input();

    if (game.state == GAME_STATE_OPTIONS || game.state == GAME_STATE_OPTIONS_PLAY) {
        apply_options_dialog_changes();
    } else if (res == DIALOG_RESULT_NONE &&
               (game.state >= GAME_SSEP_VERT_NAV_START || game.state <= GAME_SSEP_VERT_NAV_END)) {
        res = handle_vertical_navigation_input();
    }

    if (res == DIALOG_RESULT_NONE) {
        return game.state;
    }
    game.flags &= ~FLAG_DIALOG_SHOWN;

    if (res == RESULT_LEVEL_INFO) {
        return start_level();

    } else if (res == RESULT_START_LEVEL) {
        return GAME_STATE_PLAY;

    } else if (res == RESULT_RESTART_LEVEL) {
        start_level();
        return GAME_STATE_PLAY;

    } else if (res == RESULT_NEXT_LEVEL) {
        return next_level();

    } else if (res == RESULT_RESUME) {
        game_ignore_current_input();
        return GAME_STATE_PLAY;

    } else if (res == RESULT_PAUSE) {
        return GAME_STATE_PAUSE;

    } else if (res == RESULT_LEVEL_FAIL) {
        return GAME_STATE_LEVEL_FAIL;

    } else if (res == RESULT_LEVEL_COMPLETE) {
        return GAME_STATE_LEVEL_COMPLETE;

    } else if (res == RESULT_ENTER_PASSWORD) {
        if (level_use_password()) {
            return start_level();
        }
        return GAME_STATE_LEVEL_PACKS;

    } else if (res == RESULT_OPEN_LEVEL_PACKS) {
        setup_level_packs_selection();
        return GAME_STATE_LEVEL_PACKS;

    } else if (res == RESULT_OPEN_LEVELS) {
        setup_level_selection(tworld_packs.packs[game.current_pack].last_unlocked);
        return GAME_STATE_LEVELS;

    } else if (res == RESULT_OPEN_PASSWORD) {
        memset(tworld_packs.password_buf, 0, LEVEL_PASSWORD_LENGTH);
        return GAME_STATE_PASSWORD;

    } else if (res == RESULT_OPEN_OPTIONS) {
        game.old_features = game.options.features;
        return GAME_STATE_OPTIONS;

    } else if (res == RESULT_OPEN_OPTIONS_PLAY) {
        game.old_features = game.options.features;
        return GAME_STATE_OPTIONS_PLAY;

    } else if (res == RESULT_OPEN_CONTROLS) {
        return GAME_STATE_CONTROLS;

    } else if (res == RESULT_OPEN_CONTROLS_PLAY) {
        return GAME_STATE_CONTROLS_PLAY;

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
    } else if (res == RESULT_TERMINATE) {
        app_terminate();
    }

    game_music_start(ASSET_MUSIC_MENU, MUSIC_FLAG_DELAYED | MUSIC_FLAG_LOOP);
    return GAME_STATE_MAIN_MENU;
}

static void handle_movement_key_down(const direction_mask_t dir) {
    // remove any colinear direction mask to avoid having both set at once.
    if (dir & DIR_VERTICAL_MASK) {
        tworld.input_state &= ~DIR_VERTICAL_MASK;
        tworld.input_since_move &= ~DIR_VERTICAL_MASK;
    } else {
        tworld.input_state &= ~DIR_HORIZONTAL_MASK;
        tworld.input_since_move &= ~DIR_HORIZONTAL_MASK;
    }

    // add new direction to current input state.
    tworld.input_state |= dir;
    tworld.input_since_move |= dir;
}

static void handle_movement_input(const uint8_t curr_state) {
    const uint8_t last_state = input_get_last_state();

    // handle key down events
    const uint8_t key_down = curr_state & ~last_state;
    if (key_down & BUTTON_UP) {
        handle_movement_key_down(DIR_NORTH_MASK);
    } else if (key_down & BUTTON_DOWN) {
        handle_movement_key_down(DIR_SOUTH_MASK);
    }
    if (key_down & BUTTON_LEFT) {
        handle_movement_key_down(DIR_WEST_MASK);
    } else if (key_down & BUTTON_RIGHT) {
        handle_movement_key_down(DIR_EAST_MASK);
    }

    // handle key up events: only remove direction from current input state, but not from input
    // state since move, so that short click in between two moves is still registered.
    const uint8_t key_up = last_state & ~curr_state;
    if (key_up & BUTTON_UP) {
        tworld.input_state &= ~DIR_NORTH_MASK;
    }
    if (key_up & BUTTON_LEFT) {
        tworld.input_state &= ~DIR_WEST_MASK;
    }
    if (key_up & BUTTON_DOWN) {
        tworld.input_state &= ~DIR_SOUTH_MASK;
    }
    if (key_up & BUTTON_RIGHT) {
        tworld.input_state &= ~DIR_EAST_MASK;
    }
}

static game_state_t handle_misc_input(const uint8_t curr_state) {
    // update buttons hold time
    // use hold time to determine which buttons were recently clicked.
    uint8_t mask = BUTTON0;
    uint8_t clicked = 0;  // clicked buttons (pressed and click wasn't processed)
    uint8_t pressed_count = 0;  // number of pressed buttons
    uint8_t last_hold_time = 0;
    for (uint8_t i = 0; i < BUTTONS_COUNT; ++i) {
        uint8_t hold_time = button_hold_time[i];
        if (curr_state & mask) {
            // button pressed or held
            if (hold_time != UINT8_MAX) {
                ++hold_time;
                if (!(click_processed & mask)) {
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
        }
        button_hold_time[i] = hold_time;
        mask <<= 1;
    }

    if (clicked && (pressed_count > 1 || last_hold_time > BUTTON_COMBINATION_DELAY)) {
        // If single button pressed, wait minimum time for other button to be pressed and
        // create a two buttons combination. After that delay, treat as single button click.
        if ((clicked & BUTTON_PAUSE) == BUTTON_PAUSE) {
            click_processed |= BUTTON_PAUSE;
            game.flags &= ~FLAG_INVENTORY_SHOWN;
            return GAME_STATE_PAUSE;

        } else if ((clicked & BUTTON_ACTION) == BUTTON_ACTION) {
            click_processed |= BUTTON_ACTION;
            if (show_hint_if_needed()) {
                game.flags &= ~FLAG_INVENTORY_SHOWN;
                return GAME_STATE_HINT;
            }

        } else if ((clicked & BUTTON_INVENTORY) == BUTTON_INVENTORY) {
            click_processed |= BUTTON_INVENTORY;
            game.flags ^= FLAG_INVENTORY_SHOWN;
        }
    }

    return GAME_STATE_PLAY;
}

game_state_t game_handle_input_tworld(void) {
    const uint8_t curr_state = preprocess_input_state();

    handle_movement_input(curr_state);

    if (tworld.input_state) {
        game.flags |= FLAG_GAME_STARTED;
    }

    return handle_misc_input(curr_state);
}

void game_ignore_current_input(void) {
    input_wait_released = input_get_state();
}
