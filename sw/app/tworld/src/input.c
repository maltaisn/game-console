
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

#include <core/app.h>
#include <core/dialog.h>

// mask indicating buttons which should be considered not pressed until released.
static uint8_t input_wait_released;

static uint8_t preprocess_input_state() {
    uint8_t state = input_get_state();
    // if any button was released, update wait mask.
    input_wait_released &= state;
    // consider any buttons on wait mask as not pressed.
    state &= ~input_wait_released;
    return state;
}

game_state_t game_handle_input_dialog(void) {
    dialog_result_t res = dialog_handle_input();

    if (game.state == GAME_STATE_OPTIONS || game.state == GAME_STATE_OPTIONS_PLAY) {
        // apply options as they are changed
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

    if (res == DIALOG_RESULT_NONE) {
        return game.state;
    }
    game.dialog_shown = false;

    if (res == RESULT_START_LEVEL) {
        // TODO
        return GAME_STATE_PLAY;

    } else if (res == RESULT_RESTART_LEVEL) {
        // TODO
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

    } else if (res == RESULT_PASSWORD) {
        // TODO
        return GAME_STATE_LEVELS;

    } else if (res == RESULT_OPEN_LEVEL_PACKS) {
        return GAME_STATE_LEVEL_PACKS;

    } else if (res == RESULT_OPEN_LEVELS) {
        return GAME_STATE_LEVELS;

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
//    uint8_t curr_state = preprocess_input_state();

    // TODO handle hint, inventory and pause input

    return GAME_STATE_PLAY;
}

void game_ignore_current_input(void) {
    input_wait_released = input_get_state();
}
