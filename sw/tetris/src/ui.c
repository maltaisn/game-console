
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

static void init_empty_dialog(game_state_t result) {
    dialog.pos_btn = "OK";
    dialog.pos_result = result;
    dialog.dismiss_result = result;
    dialog.dismissable = true;
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
    dialog_init_centered(96, 68);
    dialog.title = "GAME PAUSED";
    dialog.dismiss_result = RESULT_RESUME_GAME;
    dialog.dismissable = true;
    dialog.selection = 0;

    dialog_add_item_button("RESUME", RESULT_RESUME_GAME);
    dialog_add_item_button("NEW GAME", RESULT_NEW_GAME);
    dialog_add_item_button("HOW TO PLAY", RESULT_OPEN_CONTROLS_PLAY);
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

    dialog_add_item_number("SOUND VOLUME", 0, 4, 1, game.options.volume);
    dialog_add_item_choice("GAME MUSIC", music_enabled, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("SOUND EFFECTS", sound_enabled, 2, CHOICES_ON_OFF);
    dialog_add_item_number("DISPLAY CONTRAST", 0, 10, 1, game.options.contrast);
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

void open_controls_dialog(game_state_t result) {
    dialog_init_centered(108, 110);
    dialog.title = "HOW TO PLAY";
    init_empty_dialog(result);
}

void open_leaderboard_dialog(void) {
    dialog_init_centered(108, 108);
    dialog.title = "LEADERBOARD";
    init_empty_dialog(RESULT_OPEN_MAIN_MENU);
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

void draw_leaderboard_overlay(void) {
    // TODO
}

#define CONTROLS_COUNT 8

static const char* CONTROL_NAMES[CONTROLS_COUNT] = {
        "Pause",
        "Move left",
        "Move right",
        "Rotate left",
        "Rotate right",
        "Soft drop",
        "Hard drop",
        "Hold/swap",
};
static const uint8_t CONTROL_BUTTONS[CONTROLS_COUNT] = {
        BUTTON_PAUSE,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_ROT_CCW,
        BUTTON_ROT_CW,
        BUTTON_DOWN,
        BUTTON_HARD_DROP,
        BUTTON_HOLD,
};

void draw_controls_overlay(void) {
    graphics_set_font(ASSET_FONT_5X7);
    disp_y_t y = 25;
    for (uint8_t i = 0; i < CONTROLS_COUNT; ++i) {
        // control name text
        graphics_set_color(DISPLAY_COLOR_WHITE);
        graphics_text(30, (int8_t) y, CONTROL_NAMES[i]);

        // illustrate the 6 buttons with the one used by the control highlighted.
        uint8_t buttons = CONTROL_BUTTONS[i];
        uint8_t mask = BUTTON0;
        disp_x_t button_x = 15;
        for (uint8_t j = 0; j < 3; ++j) {
            disp_y_t button_y = y;
            for (uint8_t k = 0; k < 2; ++k) {
                graphics_set_color(buttons & mask ? DISPLAY_COLOR_WHITE : 6);
                graphics_fill_rect(button_x, button_y, 3, 3);
                button_y += 4;
                mask <<= 1;
            }
            button_x += 4;
        }
        y += 10;
    }
}
