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

#include <sys/main.h>
#include <sys/input.h>
#include <sys/display.h>
#include <sys/time.h>
#include <sys/power.h>

#include <core/graphics.h>
#include <core/sound.h>
#include <core/sysui.h>
#include <core/trace.h>
#include <core/dialog.h>

#include <sim/flash.h>

game_t game;

static systime_t last_draw_time;
static systime_t last_tick_time;

static sleep_cause_t last_sleep_cause;
static uint8_t last_input_state;

static const char* CHOICES_ON_OFF[] = {"OFF", "ON"};

// all dialog result codes
enum {
    RESULT_NEW_GAME,
    RESULT_RESUME_GAME,
    RESULT_OPEN_OPTIONS,
    RESULT_OPEN_OPTIONS_EXTRA,
    RESULT_OPEN_LEADERBOARD,
    RESULT_OPEN_MAIN_MENU,
    RESULT_SAVE_OPTIONS,
    RESULT_SAVE_OPTIONS_EXTRA,
    RESULT_SAVE_HIGHSCORE,
};

void setup(void) {
#ifdef SIMULATION
    FILE* file = fopen("assets.dat", "r");
    flash_load_file(file);
    fclose(file);
#endif

    dialog_set_font(ASSET_FONT_7X7, ASSET_FONT_5X7, GRAPHICS_BUILTIN_FONT);
}

void loop(void) {
    systime_t time;
    do {
        time = time_get();
    } while (last_tick_time - time < millis_to_ticks(GAME_TICK));
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

    // TODO

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
    // TODO
    // temp
    uint8_t clicked = ~last_input_state & input_get_state();
    if (clicked & BUTTON0) {
        return GAME_STATE_PAUSE;
    }
    return GAME_STATE_PLAY;
}

void start_game(void) {
    // TODO
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

void open_main_menu_dialog(void) {
    if (game.dialog_shown) {
        return;
    }
    game.dialog_shown = true;
    dialog_init(16, 68, 96, 44);
    dialog_add_item_button("NEW GAME", RESULT_NEW_GAME);
    dialog_add_item_button("OPTIONS", RESULT_OPEN_OPTIONS);
    dialog_add_item_button("LEADERBOARD", RESULT_OPEN_LEADERBOARD);
    dialog.selection = 0;
}

void open_pause_dialog(void) {
    dialog_init(16, 36, 96, 55);
    dialog.title = "GAME OVER";
    dialog.dismiss_result = RESULT_RESUME_GAME;
    dialog_add_item_button("RESUME", RESULT_RESUME_GAME);
    dialog_add_item_button("NEW GAME", RESULT_NEW_GAME);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
    dialog.dismissable = true;
    dialog.selection = 0;
}

void open_options_dialog(void) {
    dialog_init(10, 10, 108, 108);
    dialog.title = "GAME OPTIONS";
    dialog.pos_btn = "OK";
    dialog.neg_btn = "Cancel";
    dialog.pos_result = RESULT_SAVE_OPTIONS;
    dialog.neg_result = RESULT_OPEN_MAIN_MENU;
    // TODO use initial value from options
    dialog_add_item_choice("GHOST PIECE", 0, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("HOLD PIECE", 0, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("WALL KICKS", 0, 2, CHOICES_ON_OFF);
    dialog_add_item_choice("T-SPIN BONUS", 0, 2, CHOICES_ON_OFF);
    dialog_add_item_number("PREVIEW PIECES", 0, 5, 1, 0);
    dialog_add_item_button("MORE OPTIONS", RESULT_OPEN_OPTIONS_EXTRA);
    dialog.dismissable = true;
    dialog.selection = 0;
}

void __attribute__((noinline)) open_extra_options_dialog(void) {
    dialog_init(10, 51, 108, 67);
    dialog.title = "EXTRA OPTIONS";
    dialog.pos_btn = "OK";
    dialog.neg_btn = "Cancel";
    dialog.pos_result = RESULT_SAVE_OPTIONS_EXTRA;
    dialog.neg_result = RESULT_OPEN_OPTIONS;
    // TODO use initial value from options
    dialog_add_item_number("MUSIC VOLUME", 0, 4, 1, 0);
    dialog_add_item_choice("SOUND EFFECTS", 0, 2, CHOICES_ON_OFF);
    dialog_add_item_number("DISPLAY CONTRAST", 0, 100, 10, 70);
    dialog.dismissable = true;
    dialog.selection = 0;
}

void open_leaderboard_dialog(void) {
    dialog_init(10, 10, 108, 108);
    dialog.title = "LEADERBOARD";
    dialog.pos_btn = "OK";
    dialog.pos_result = RESULT_OPEN_MAIN_MENU;
    dialog.dismiss_result = RESULT_OPEN_MAIN_MENU;
    dialog.dismissable = true;
    dialog.selection = DIALOG_SELECTION_POS;
}

void open_high_score_dialog(void) {
    dialog_init(10, 39, 108, 50);
    dialog.title = "NEW HIGHSCORE";
    dialog.pos_btn = "OK";
    dialog.pos_result = RESULT_SAVE_HIGHSCORE;
    // TODO text item type
//    dialog.selection = 0;
    dialog.selection = DIALOG_SELECTION_POS;
}

void open_game_over_dialog(void) {
    dialog_init(16, 43, 96, 42);
    dialog.title = "GAME OVER";
    dialog_add_item_button("PLAY AGAIN", RESULT_NEW_GAME);
    dialog_add_item_button("MAIN MENU", RESULT_OPEN_MAIN_MENU);
    dialog.selection = 0;
}

void draw(void) {
    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        // low power sleep scheduled, show low battery UI before sleeping.
        sysui_battery_sleep();
        return;
    }

    graphics_clear(DISPLAY_COLOR_BLACK);

    if (game.state < GAME_STATE_PLAY) {
        // TODO draw main menu image
    } else {
        draw_game();
    }

    if (game.dialog_shown) {
        dialog_draw();
    }
}

void draw_game(void) {
    // TODO draw game state
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_set_font(ASSET_FONT_7X7);
    graphics_text_wrap(10, 10, 118, "TETRIS NOT IMPLEMENTED");
}

void on_sleep_scheduled(void) {
    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        sound_set_output_enabled(false);
    }
}
