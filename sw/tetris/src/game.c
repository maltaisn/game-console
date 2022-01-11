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
#include <sim/eeprom.h>

#include <string.h>

game_t game;

static systime_t last_draw_time;
static systime_t last_tick_time;

static uint8_t last_input_state;
// mask indicating buttons which should be considered not pressed until released.
static uint8_t input_wait_released;
// number of game ticks that buttons has been held.
static uint8_t click_processed;
// time since buttons was pressed, in game ticks.
static uint8_t button_hold_time[BUTTONS_COUNT];
// mask indicating for which buttons DAS is currently enabled.
static uint8_t delayed_auto_shift;

const game_header_t GAME_HEADER = (game_header_t) {0xa5, GAME_VERSION_MAJOR, GAME_VERSION_MINOR};

void setup(void) {
#ifdef SIMULATION
    FILE* flash = fopen("assets.dat", "r");
    flash_load_file(flash);
    fclose(flash);

    FILE* eeprom = fopen("eeprom.dat", "r");
    if (eeprom) {
        eeprom_load_file(eeprom);
        fclose(eeprom);
    }
#endif

    dialog_set_font(ASSET_FONT_7X7, ASSET_FONT_5X7, GRAPHICS_BUILTIN_FONT);

    load_from_eeprom();

    sound_set_volume(game.options.volume << SOUND_CHANNELS_COUNT);
    update_display_contrast(game.options.contrast);
}

void loop(void) {
    systime_t time;
    do {
        time = time_get();
    } while (time - last_tick_time < GAME_TICK);
    last_tick_time = time;

    game.state = main_loop();

    last_input_state = input_get_state();

    if (time - last_draw_time > millis_to_ticks(1000.0 / DISPLAY_FPS)) {
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
        } else if (s == GAME_STATE_CONTROLS) {
            open_controls_dialog(RESULT_OPEN_MAIN_MENU);
        } else if (s == GAME_STATE_CONTROLS_PLAY) {
            open_controls_dialog(RESULT_PAUSE_GAME);
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

    if (tetris.flags & TETRIS_FLAG_GAME_OVER) {
        // check if new high score achieved and find its position.
        int8_t pos = -1;
        int8_t end = (int8_t) game.leaderboard.size;
        if (end < LEADERBOARD_MAX_SIZE) {
            ++end;
        }
        for (int8_t i = 0; i < end; ++i) {
            if (i == game.leaderboard.size || game.leaderboard.entries[i].score < tetris.score) {
                pos = i;
                break;
            }
        }
        if (pos != -1) {
            // shift existing high scores and insert new one
            for (uint8_t i = game.leaderboard.size; i > pos; --i) {
                game.leaderboard.entries[i] = game.leaderboard.entries[i - 1];
            }
            game.leaderboard.size = end;
            game.leaderboard.entries[pos] = (game_highscore_t) {tetris.score, "(UNNAMED)"};
            game.new_highscore_pos = pos;
            save_to_eeprom();
        }
        return GAME_STATE_HIGH_SCORE;
    }

    return GAME_STATE_PLAY;
}

static uint8_t preprocess_input_state() {
    uint8_t state = input_get_state();
    // if any button was released, update wait mask.
    input_wait_released &= state;
    // consider any buttons on wait mask as not pressed.
    state &= ~input_wait_released;
    return state;
}

game_state_t handle_dialog_input(void) {
    dialog_result_t res = dialog_handle_input(last_input_state, preprocess_input_state());

    if (game.state == GAME_STATE_OPTIONS) {
        update_display_contrast(dialog.items[3].number.value);
    }

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

    } else if (res == RESULT_PAUSE_GAME) {
        open_pause_dialog();
        return GAME_STATE_PAUSE;

    } else if (res == RESULT_OPEN_OPTIONS) {
        return GAME_STATE_OPTIONS;

    } else if (res == RESULT_OPEN_OPTIONS_EXTRA) {
        return GAME_STATE_OPTIONS_EXTRA;

    } else if (res == RESULT_OPEN_CONTROLS) {
        return GAME_STATE_CONTROLS;

    } else if (res == RESULT_OPEN_CONTROLS_PLAY) {
        return GAME_STATE_CONTROLS_PLAY;

    } else if (res == RESULT_OPEN_LEADERBOARD) {
        return GAME_STATE_LEADERBOARD;

    } else if (res == RESULT_SAVE_OPTIONS_EXTRA) {
        save_extra_options();
        return GAME_STATE_OPTIONS;

    } else if (res == RESULT_SAVE_OPTIONS) {
        save_options();
        return GAME_STATE_MAIN_MENU;

    } else if (res == RESULT_CANCEL_OPTIONS) {
        // restore old contrast
        update_display_contrast(game.options.contrast);
        return GAME_STATE_MAIN_MENU;

    } else if (res == RESULT_SAVE_HIGHSCORE) {
        return save_highscore();
    }
    // should not happen
    return GAME_STATE_MAIN_MENU;
}

game_state_t handle_game_input(void) {
    // update buttons hold time
    // use hold time to determine which buttons were recently clicked.
    uint8_t curr_state = preprocess_input_state();
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
                    if (hold_time >= DELAYED_AUTO_SHIFT + AUTO_REPEAT_RATE) {
                        hold_time = DELAYED_AUTO_SHIFT;
                        das_triggered |= mask;
                    }
                } else if (hold_time >= DELAYED_AUTO_SHIFT && (DAS_MASK & mask)) {
                    delayed_auto_shift |= mask;
                    if ((delayed_auto_shift & DAS_DISALLOWED) == DAS_DISALLOWED) {
                        // disallowed combination of active DAS, disable them all.
                        delayed_auto_shift = 0;
                    } else {
                        das_triggered |= mask;
                        hold_time = DELAYED_AUTO_SHIFT;
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
    tetris_next_piece();
    resume_game();
}

void resume_game(void) {
    input_wait_released = input_get_state();
    click_processed = BUTTONS_ALL;
}

game_state_t save_highscore(void) {
    const char* name = dialog.items[0].text.text;
    if (!*name) {
        // name is empty, don't hide dialog.
        return GAME_STATE_HIGH_SCORE;
    }

    strcpy(game.leaderboard.entries[game.new_highscore_pos].name, dialog.items[0].text.text);
    save_to_eeprom();

    return GAME_STATE_LEADERBOARD;
}

void update_display_contrast(uint8_t value) {
    display_set_contrast(value * 20);
}

void save_options(void) {
    uint8_t features = 0;
    if (dialog.items[1].choice.selection) {
        features |= GAME_FEATURE_MUSIC;
    }
    if (dialog.items[2].choice.selection) {
        features |= GAME_FEATURE_SOUND_EFFECTS;
    }

    uint8_t volume = dialog.items[0].number.value;
    uint8_t contrast = dialog.items[3].number.value;
    uint8_t preview_pieces = dialog.items[4].number.value;

    game.options = (game_options_t) {
            .features = features,
            .volume = volume,
            .contrast = contrast,
    };
    tetris.options.preview_pieces = preview_pieces;

    sound_set_volume(volume << SOUND_CHANNELS_COUNT);
    update_display_contrast(contrast);

    save_to_eeprom();
}

void save_extra_options(void) {
    uint8_t features = 0;
    if (dialog.items[0].choice.selection) {
        features |= TETRIS_FEATURE_GHOST;
    }
    if (dialog.items[1].choice.selection) {
        features |= TETRIS_FEATURE_HOLD;
    }
    if (dialog.items[2].choice.selection) {
        features |= TETRIS_FEATURE_WALL_KICKS;
    }
    if (dialog.items[3].choice.selection) {
        features |= TETRIS_FEATURE_TSPINS;
    }
    tetris.options.features = features;

    save_to_eeprom();
}

void set_default_options(void) {
    game.options = (game_options_t) {
            .features = GAME_FEATURE_MUSIC | GAME_FEATURE_SOUND_EFFECTS,
            .volume = SOUND_VOLUME_2 >> SOUND_CHANNELS_COUNT,
            .contrast = 6,
    };
    tetris.options = (tetris_options_t) {
            .features = TETRIS_FEATURE_HOLD | TETRIS_FEATURE_GHOST |
                        TETRIS_FEATURE_WALL_KICKS | TETRIS_FEATURE_TSPINS,
            .preview_pieces = 5,
    };
    game.leaderboard.size = 0;
}

void load_from_eeprom(void) {
    // use display buffer as temporary memory to write data
    // TODO use section attribute to share buffer and decouple from display
    uint8_t* buf = display_buffer(0, 0);
    eeprom_read(0, sizeof GAME_HEADER + sizeof game.options +
                   sizeof tetris.options + sizeof game.leaderboard, buf);

    // read header and check version & signature first
    if (memcmp(buf, &GAME_HEADER, sizeof GAME_HEADER) != 0) {
        // header differs, can't load data from eeprom
        set_default_options();
        return;
    }
    buf += sizeof GAME_HEADER;

    memcpy(&game.options, buf, sizeof game.options);
    buf += sizeof game.options;
    memcpy(&tetris.options, buf, sizeof tetris.options);
    buf += sizeof tetris.options;

    memcpy(&game.leaderboard, buf, sizeof game.leaderboard);
}

void save_to_eeprom(void) {
    // use display buffer as temporary memory to write data
    // TODO use section attribute to share buffer and decouple from display
    uint8_t* buf = display_buffer(0, 0);
    uint8_t* buf_start = buf;

    // header
    memcpy(buf, &GAME_HEADER, sizeof GAME_HEADER);
    buf += sizeof GAME_HEADER;

    // options
    memcpy(buf, &game.options, sizeof game.options);
    buf += sizeof game.options;
    memcpy(buf, &tetris.options, sizeof tetris.options);
    buf += sizeof tetris.options;

    // leaderboard
    memcpy(buf, &game.leaderboard, sizeof game.leaderboard);
    buf += sizeof game.leaderboard;

    eeprom_write(0, buf - buf_start, buf_start);

#ifdef SIMULATION
    FILE* eeprom = fopen("eeprom.dat", "w");
    eeprom_save(eeprom);
    fclose(eeprom);
#endif
}

void power_callback_sleep_scheduled(void) {
    sleep_cause_t cause = power_get_scheduled_sleep_cause();

    if (game.state == GAME_STATE_PLAY) {
        game.state = GAME_STATE_PAUSE;
    }
    if (cause == SLEEP_CAUSE_LOW_POWER) {
        sound_set_output_enabled(false);
    }
}

void power_callback_wakeup(void) {
    // ignore whatever button was used to wake up.
    input_wait_released = input_get_state_immediate();
}