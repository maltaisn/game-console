
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

#include <ui.h>
#include <game.h>
#include <tetris.h>
#include <assets.h>

#include <core/dialog.h>

static const char* CHOICES_ON_OFF[] = {"OFF", "ON"};

static char text_field_buffer[11];

static void init_empty_dialog(game_state_t result) {
    dialog.pos_btn = "OK";
    dialog.pos_result = result;
    dialog.dismiss_result = result;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
    dialog.selection = DIALOG_SELECTION_POS;
}

void open_main_menu_dialog(void) {
    dialog_init_hcentered(56, 96, 56);
    dialog.selection = 0;

    dialog_add_item_button("NEW GAME", RESULT_NEW_GAME);
    dialog_add_item_button("HOW TO PLAY", RESULT_OPEN_CONTROLS);
    dialog_add_item_button("OPTIONS", RESULT_OPEN_OPTIONS);
    dialog_add_item_button("LEADERBOARD", RESULT_OPEN_LEADERBOARD);
}

void open_pause_dialog(void) {
    dialog_init_centered(96, 81);
    dialog.title = "GAME PAUSED";
    dialog.dismiss_result = RESULT_RESUME_GAME;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
    dialog.selection = 0;

    dialog_add_item_button("RESUME", RESULT_RESUME_GAME);
    dialog_add_item_button("NEW GAME", RESULT_NEW_GAME);
    dialog_add_item_button("HOW TO PLAY", RESULT_OPEN_CONTROLS_PLAY);
    dialog_add_item_button("OPTIONS", RESULT_OPEN_OPTIONS_PLAY);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
}

void open_options_dialog(uint8_t result_pos, uint8_t result_neg) {
    uint8_t height = (result_pos == RESULT_SAVE_OPTIONS ? 94 : 80);
    dialog_init_centered(108, height);
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
    if (result_pos == RESULT_SAVE_OPTIONS) {
        dialog_add_item_button("MORE OPTIONS", RESULT_OPEN_OPTIONS_EXTRA);
    }
}

void open_extra_options_dialog(void) {
    dialog_init_hcentered(18, 108, 93);
    dialog.title = "EXTRA OPTIONS";
    dialog.pos_btn = "OK";
    dialog.neg_btn = "Cancel";
    dialog.pos_result = RESULT_SAVE_OPTIONS_EXTRA;
    dialog.neg_result = RESULT_OPEN_OPTIONS;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
    dialog.selection = 0;

    const bool feature_ghost = (tetris.options.features & TETRIS_FEATURE_GHOST) != 0;
    const bool feature_hold = (tetris.options.features & TETRIS_FEATURE_HOLD) != 0;
    const bool feature_kicks = (tetris.options.features & TETRIS_FEATURE_WALL_KICKS) != 0;
    const bool feature_tspins = (tetris.options.features & TETRIS_FEATURE_TSPINS) != 0;

    dialog_add_item_number("PREVIEW PIECES", 0, 5, 1, tetris.options.preview_pieces);
    dialog_add_item_choice("GHOST PIECE", feature_ghost, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("HOLD PIECE", feature_hold, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("WALL KICKS", feature_kicks, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("T-SPIN BONUS", feature_tspins, 2, CHOICES_ON_OFF);
}

void open_controls_dialog(uint8_t result) {
    dialog_init_centered(108, 110);
    dialog.title = "HOW TO PLAY";
    init_empty_dialog(result);
}

void open_leaderboard_dialog(uint8_t result) {
    dialog_init_centered(108, 109);
    dialog.title = "LEADERBOARD";
    init_empty_dialog(result);
}

void open_high_score_dialog(void) {
    dialog_init_centered(108, 52);
    dialog.title = "NEW HIGHSCORE";
    dialog.pos_btn = "OK";
    dialog.pos_result = RESULT_SAVE_HIGHSCORE;
    dialog.selection = 0;
    dialog.cursor_pos = 0;

    dialog_add_item_text("ENTER YOUR NAME:", HIGHSCORE_NAME_MAX_LENGTH, text_field_buffer);
}

void open_game_over_dialog(void) {
    dialog_init_centered(96, 42);
    dialog.title = "GAME OVER";
    dialog.selection = 0;

    dialog_add_item_button("PLAY AGAIN", RESULT_NEW_GAME);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
}

