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
#include <save.h>
#include <music.h>
#include <sound.h>
#include <input.h>
#include <led.h>

#include <sys/main.h>
#include <sys/display.h>
#include <sys/time.h>
#include <sys/power.h>
#include <sys/led.h>

#include <core/graphics.h>
#include <core/sound.h>
#include <core/dialog.h>
#include <core/random.h>

#include <sim/flash.h>
#include <sim/eeprom.h>

#include <string.h>
#include <stdio.h>

game_t game;

static systime_t last_draw_time;
static systime_t last_tick_time;

const game_header_t GAME_HEADER = (game_header_t) {0xa5, GAME_VERSION_MAJOR, GAME_VERSION_MINOR};

static game_state_t game_state_update(uint8_t dt);

void setup(void) {
#ifdef SIMULATION
    FILE* flash = fopen("assets.dat", "rb");
    flash_load_file(ASSET_OFFSET, flash);
    fclose(flash);

    FILE* eeprom = fopen("eeprom.dat", "rb");
    if (eeprom) {
        eeprom_load_file(eeprom);
        fclose(eeprom);
    }
#endif

    // check header in flash to make sure flash has been written
    uint8_t header[3];
    flash_read(ASSET_RAW_HEADER, sizeof GAME_HEADER, header);
    if (memcmp(header, &GAME_HEADER, sizeof GAME_HEADER) != 0) {
        // wrong header, don't start game.
        led_set();
        for (;;);
    }

    sound_set_tempo(encode_bpm_tempo(ASSET_SOUND_TEMPO));
    sound_set_channel_volume(2, SOUND_CHANNEL2_VOLUME1);
    dialog_set_font(ASSET_FONT_7X7, ASSET_FONT_5X7, GRAPHICS_BUILTIN_FONT);

    // load saved (or default) settings and apply them.
    load_from_eeprom();
    update_sound_volume(game.options.volume);
    update_display_contrast(game.options.contrast);
    update_music_enabled();
    sound_start(SOUND_TRACKS_STARTED);
}

void loop(void) {
    // wait until at least one game tick has passed since last loop
    // if display was refreshed, dt will be greater than 1, otherwise it will probably be 1.
    systime_t time;
    uint8_t dt;
    do {
        time = time_get();
        dt = (time - last_tick_time) / GAME_TICK;
    } while (dt == 0);
    if (dt > MAX_DELTA_TIME) {
        dt = MAX_DELTA_TIME;
    }
    last_tick_time = time;

    game_led_update(dt);
    game_music_update(dt);
    game_sound_update(dt);
    game.state = game_state_update(dt);

    if (time - last_draw_time > millis_to_ticks(1000.0 / DISPLAY_MAX_FPS)) {
        last_draw_time = time;
        display_first_page();
        do {
            draw();
        } while (display_next_page());
    }
}

static game_state_t update_leaderboard_for_score(void) {
    // check if new high score achieved and find its position.
    int8_t pos = -1;
    int8_t end = (int8_t) game.leaderboard.size;
    if (end < LEADERBOARD_MAX_SIZE) {
        ++end;
    }
    for (int8_t i = 0; i < end; ++i) {
        if (i == game.leaderboard.size || game.leaderboard.entries[i].score < tetris.score) {
            // found lower score in leaderboard, save position.
            // this makes sure to insert the new score after any score that are equal.
            pos = i;
            break;
        }
    }
    if (pos != -1) {
        // shift existing high scores and insert new one
        uint8_t last_pos = game.leaderboard.size == LEADERBOARD_MAX_SIZE ?
                           LEADERBOARD_MAX_SIZE - 1 : game.leaderboard.size;
        for (uint8_t i = last_pos; i > pos; --i) {
            game.leaderboard.entries[i] = game.leaderboard.entries[i - 1];
        }
        game.leaderboard.size = end;
        game.leaderboard.entries[pos] = (game_highscore_t) {tetris.score, "(UNNAMED)"};
        game.new_highscore_pos = pos;
        save_to_eeprom();
        game_music_loop_next(ASSET_MUSIC_HIGH_SCORE);
        return GAME_STATE_HIGH_SCORE;
    }
    return GAME_STATE_GAME_OVER;
}

static game_state_t update_tetris_state(uint8_t dt) {
    game_state_t new_state = game_handle_input_tetris();
    if (new_state != GAME_STATE_PLAY) {
        return new_state;
    }

    tetris_update(dt);

    if (tetris.flags & TETRIS_FLAG_GAME_OVER) {
        game_led_start(32, 128);
        game_music_start(ASSET_MUSIC_GAME_OVER, MUSIC_FLAG_DELAYED);
        return update_leaderboard_for_score();
    }

    return GAME_STATE_PLAY;
}

static game_state_t game_state_update(uint8_t dt) {
    game_state_t s = game.state;
    if (s == GAME_STATE_PLAY) {
        return update_tetris_state(dt);
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
            open_options_dialog(RESULT_SAVE_OPTIONS, RESULT_CANCEL_OPTIONS);
        } else if (s == GAME_STATE_OPTIONS_PLAY) {
            open_options_dialog(RESULT_SAVE_OPTIONS_PLAY, RESULT_CANCEL_OPTIONS_PLAY);
        } else if (s == GAME_STATE_OPTIONS_EXTRA) {
            open_extra_options_dialog();
        } else if (s == GAME_STATE_CONTROLS) {
            open_controls_dialog(RESULT_OPEN_MAIN_MENU);
        } else if (s == GAME_STATE_CONTROLS_PLAY) {
            open_controls_dialog(RESULT_PAUSE_GAME);
        } else if (s == GAME_STATE_LEADERBOARD_PLAY) {
            open_leaderboard_dialog(RESULT_GAME_OVER);
        } else { // s == GAME_STATE_LEADERBOARD
            open_leaderboard_dialog(RESULT_OPEN_MAIN_MENU);
        }
        game.dialog_shown = true;
    }
    return game_handle_input_dialog();
}

void game_start(void) {
    random_seed(time_get());
    tetris_init();

    game_ignore_current_input();
    game_led_stop();

    game_music_start(ASSET_MUSIC_THEME, MUSIC_FLAG_LOOP | MUSIC_FLAG_DELAYED);
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
    game_ignore_current_input();
    // last tick has probably happened very long ago, reset last tick time.
    last_tick_time = time_get();
}
