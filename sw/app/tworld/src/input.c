
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
            if (level <= info->last_unlocked ||
                    info->completed_array[level / 8] & (1 << (level % 8))) {
                game.current_level = level;
                game.current_level_pos = info->pos + level;
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

static void setup_level_selection(void) {
    level_pack_info_t* info = &tworld_packs.packs[game.current_pack];

    // Find the last level in the consecutive streak of completed levels starting from the first.
    // The level after that is unlocked and will be selected by default.
    // Other levels may be completed with a password but that doesn't unlock the level after
    uint8_t byte = 0;
    uint8_t bits = 0;
    const uint8_t* completed = info->completed_array;
    level_idx_t i = 0;
    for (; i < info->total_levels; ++i) {
        if (bits == 0) {
            byte = *completed++;
        }
        if (!(byte & 1)) {
            break;
        }
        byte >>= 1;
        bits = (bits + 1) % 8;
    }

    game.pos_selection_x = i % LEVELS_PER_SCREEN_H;
    game.pos_selection_y = i / LEVELS_PER_SCREEN_H;
    game.pos_max_x = LEVELS_PER_SCREEN_H - 1;
    game.pos_max_y = (info->total_levels - 1) / LEVELS_PER_SCREEN_H;
    game.pos_last_x = (info->total_levels - 1) % LEVELS_PER_SCREEN_H;
    game.pos_shown_y = LEVELS_PER_SCREEN_V;

    game.pos_first_y = game.pos_selection_y;
    uint8_t max_first_y = game.pos_max_y - LEVELS_PER_SCREEN_V + 1;
    if (game.pos_selection_y > max_first_y) {
        game.pos_selection_y = max_first_y;
    }
}

static bool show_hint_if_needed(void) {
    const position_t pos = tworld_get_current_position();
    if (tworld_get_bottom_tile(pos.x, pos.y) != TILE_HINT) {
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

static void start_level(void) {
    level_read_level();
    game_ignore_current_input();
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
        start_level();
        return GAME_STATE_LEVEL_INFO;

    } else if (res == RESULT_START_LEVEL) {
        start_level();
        return GAME_STATE_PLAY;

    } else if (res == RESULT_NEXT_LEVEL) {
        // TODO
        return GAME_STATE_PLAY;

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
            start_level();
            return GAME_STATE_LEVEL_INFO;
        }
        return GAME_STATE_LEVEL_PACKS;

    } else if (res == RESULT_OPEN_LEVEL_PACKS) {
        setup_level_packs_selection();
        return GAME_STATE_LEVEL_PACKS;

    } else if (res == RESULT_OPEN_LEVELS) {
        setup_level_selection();
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

    return GAME_STATE_MAIN_MENU;
}

game_state_t game_handle_input_tworld(void) {
    const uint8_t curr_state = preprocess_input_state();

    // Create input direction bitfield.
    uint8_t dir = 0;
    if (curr_state & BUTTON_UP) {
        dir |= DIR_NORTH_MASK;
    }
    if (curr_state & BUTTON_LEFT) {
        dir |= DIR_WEST_MASK;
    }
    if (curr_state & BUTTON_DOWN) {
        dir |= DIR_SOUTH_MASK;
    }
    if (curr_state & BUTTON_RIGHT) {
        dir |= DIR_EAST_MASK;
    }
    tworld.input_state = dir;

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

void game_ignore_current_input(void) {
    input_wait_released = input_get_state();
}
