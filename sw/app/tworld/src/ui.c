
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

#include "ui.h"
#include "game.h"
#include "assets.h"
#include "tworld_level.h"

#include <core/dialog.h>

static const char* const CHOICES_ON_OFF[] = {"OFF", "ON"};

void open_main_menu_dialog(void) {
    dialog_init_hcentered(54, 96, 56);
    if (game.last_state < GAME_SSEP_COVER_BG) {
        // restore previous dialog selection (controls, options).
        dialog.selection = (uint8_t) game.last_state;
    } else {
        dialog.selection = 0;
    }

    dialog_add_item_button("PLAY", RESULT_OPEN_LEVEL_PACKS);
    dialog_add_item_button("HOW TO PLAY", RESULT_OPEN_HELP);
    dialog_add_item_button("OPTIONS", RESULT_OPEN_OPTIONS);
    dialog_add_item_button("EXIT", RESULT_TERMINATE);

    level_read_packs();
}

void open_level_packs_dialog(void) {
    dialog_init_centered(126, 126);
    dialog.title = "LEVEL PACKS";
    dialog.dismiss_result = RESULT_OPEN_MAIN_MENU;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
}

void open_levels_dialog(void) {
    dialog_init_centered(126, 126);
    dialog.title = "LEVELS";
    dialog.dismiss_result = RESULT_OPEN_LEVEL_PACKS;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
}

void open_level_info_dialog(void) {
    dialog_init_centered(126, 73);
    dialog.pos_btn = "START";
    dialog.selection = DIALOG_SELECTION_POS;
    dialog.pos_result = RESULT_START_LEVEL;
}

void open_password_dialog(void) {
    dialog_init_centered(100, 52);
    dialog.title = "GO TO LEVEL";
    dialog.pos_btn = "OK";
    dialog.neg_btn = "Cancel";
    dialog.pos_result = RESULT_ENTER_PASSWORD;
    dialog.neg_result = RESULT_OPEN_LEVEL_PACKS;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
    dialog.selection = 0;

    dialog_add_item_text("LEVEL PASSWORD:", 4, tworld_packs.password_buf);
}

void open_pause_dialog(void) {
    dialog_init_centered(96, 81);
    dialog.title = "GAME PAUSED";
    dialog.dismiss_result = RESULT_RESUME;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
    if (game.last_state == GAME_STATE_HELP_PLAY || game.last_state == GAME_STATE_OPTIONS_PLAY) {
        // restore previous dialog selection (controls, options).
        dialog.selection = (uint8_t) (game.last_state - GAME_STATE_HELP_PLAY) + 2;
    } else {
        dialog.selection = 0;
    }

    dialog_add_item_button("RESUME", RESULT_RESUME);
    dialog_add_item_button("RESTART", RESULT_RESTART_LEVEL);
    dialog_add_item_button("HOW TO PLAY", RESULT_OPEN_HELP_PLAY);
    dialog_add_item_button("OPTIONS", RESULT_OPEN_OPTIONS_PLAY);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
}

void open_hint_dialog(void) {
    dialog_init_centered(120, 87);
    dialog.title = "HINT";
    dialog.pos_btn = "OK";
    dialog.selection = DIALOG_SELECTION_POS;
    dialog.pos_result = RESULT_RESUME;
}

void open_options_dialog(uint8_t result_pos, uint8_t result_neg) {
    // dialog is centered if in game, and at the same position of main menu dialog if in menu.
    dialog_init_hcentered(result_pos == RESULT_SAVE_OPTIONS ? 35 : 17, 108, 80);
    dialog.title = "GAME OPTIONS";
    dialog.pos_btn = "OK";
    dialog.neg_btn = "Cancel";
    dialog.pos_result = result_pos;
    dialog.neg_result = result_neg;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
    dialog.selection = 0;

    const bool music_enabled = (game.options.features & GAME_FEATURE_MUSIC) != 0;
    const bool sound_enabled = (game.options.features & GAME_FEATURE_SOUND_EFFECTS) != 0;

    dialog_add_item_number("SOUND VOLUME", 0, 4, 1, game.options.volume);
    dialog_add_item_choice("GAME MUSIC", music_enabled, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("SOUND EFFECTS", sound_enabled, 2, CHOICES_ON_OFF);
    dialog_add_item_number("DISPLAY CONTRAST", 0, 10, 10, game.options.contrast);
}

void open_controls_dialog(uint8_t result) {
    dialog_init_hcentered(8, 108, 107);
    dialog.title = "HOW TO PLAY";
    dialog.pos_btn = "OK";
    dialog.pos_result = result;
    dialog.dismiss_result = result;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
    dialog.selection = DIALOG_SELECTION_POS;
}

void open_level_fail_dialog(void) {
    dialog_init_centered(120, 76);
    dialog.title = "FAILED";
    dialog.selection = 0;
    dialog.top_margin = 34;

    dialog_add_item_button("TRY AGAIN", RESULT_RESTART_LEVEL);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
}

void open_level_complete_dialog(void) {
    dialog_init_centered(100, 76);
    dialog.title = "COMPLETED";
    dialog.selection = 0;
    dialog.top_margin = 34;

    dialog_add_item_button("NEXT LEVEL", RESULT_NEXT_LEVEL);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
}
