
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

#include <core/dialog.h>

static const char* CHOICES_ON_OFF[] = {"OFF", "ON"};

void open_main_menu_dialog(void) {
    dialog_init_hcentered(69, 96, 43);
    dialog.selection = 0;

    dialog_add_item_button("NEW GAME", RESULT_NEW_GAME);
    dialog_add_item_button("OPTIONS", RESULT_OPEN_OPTIONS);
    dialog_add_item_button("LEADERBOARD", RESULT_OPEN_LEADERBOARD);
}

void open_pause_dialog(void) {
    dialog_init_centered(96, 55);
    dialog.title = "GAME PAUSED";
    dialog.dismiss_result = RESULT_RESUME_GAME;
    dialog.dismissable = true;
    dialog.selection = 0;

    dialog_add_item_button("RESUME", RESULT_RESUME_GAME);
    dialog_add_item_button("NEW GAME", RESULT_NEW_GAME);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
}

void open_options_dialog(void) {
    dialog_init_hcentered(11, 108, 107);
    dialog.title = "GAME OPTIONS";
    dialog.pos_btn = "OK";
    dialog.neg_btn = "Cancel";
    dialog.pos_result = RESULT_SAVE_OPTIONS;
    dialog.neg_result = RESULT_OPEN_MAIN_MENU;
    dialog.dismissable = true;
    dialog.selection = 0;

    const bool music_enabled = (game.options.features & GAME_FEATURE_MUSIC) != 0;
    const bool sound_enabled = (game.options.features & GAME_FEATURE_SOUND_EFFECTS) != 0;

    dialog_add_item_number("SOUND VOLUME", 0, 4, 1, game.options.volume >> SOUND_CHANNELS_COUNT);
    dialog_add_item_choice("GAME MUSIC", music_enabled, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("SOUND EFFECTS", sound_enabled, 2, CHOICES_ON_OFF);
    dialog_add_item_number("DISPLAY CONTRAST", 0, 100, 10, game.options.contrast * 10);
    dialog_add_item_number("PREVIEW PIECES", 0, 5, 1, tetris.options.preview_pieces);
    dialog_add_item_button("MORE OPTIONS", RESULT_OPEN_OPTIONS_EXTRA);
}

void open_extra_options_dialog(void) {
    dialog_init_hcentered(38, 108, 80);
    dialog.title = "EXTRA OPTIONS";
    dialog.pos_btn = "OK";
    dialog.neg_btn = "Cancel";
    dialog.pos_result = RESULT_SAVE_OPTIONS_EXTRA;
    dialog.neg_result = RESULT_OPEN_OPTIONS;
    dialog.dismissable = true;
    dialog.selection = 0;

    const bool feature_ghost = (tetris.options.features & TETRIS_FEATURE_GHOST) != 0;
    const bool feature_hold = (tetris.options.features & TETRIS_FEATURE_HOLD) != 0;
    const bool feature_kicks = (tetris.options.features & TETRIS_FEATURE_WALL_KICKS) != 0;
    const bool feature_tspins = (tetris.options.features & TETRIS_FEATURE_TSPINS) != 0;

    dialog_add_item_choice("GHOST PIECE", feature_ghost, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("HOLD PIECE", feature_hold, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("WALL KICKS", feature_kicks, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("T-SPIN BONUS", feature_tspins, 2, CHOICES_ON_OFF);
}

void open_leaderboard_dialog(void) {
    dialog_init_centered(108, 108);
    dialog.title = "LEADERBOARD";
    dialog.pos_btn = "OK";
    dialog.pos_result = RESULT_OPEN_MAIN_MENU;
    dialog.dismiss_result = RESULT_OPEN_MAIN_MENU;
    dialog.dismissable = true;
    dialog.selection = DIALOG_SELECTION_POS;
}

void open_high_score_dialog(void) {
    dialog_init_centered(108, 50);
    dialog.title = "NEW HIGHSCORE";
    dialog.pos_btn = "OK";
    dialog.pos_result = RESULT_SAVE_HIGHSCORE;
    dialog.selection = DIALOG_SELECTION_POS;

    // TODO text item type
//    dialog.selection = 0;
}

void open_game_over_dialog(void) {
    dialog_init_centered(96, 42);
    dialog.title = "GAME OVER";
    dialog.selection = 0;

    dialog_add_item_button("PLAY AGAIN", RESULT_NEW_GAME);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
}

