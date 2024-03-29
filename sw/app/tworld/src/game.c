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

#include "game.h"
#include "assets.h"
#include "tworld.h"
#include "ui.h"
#include "render.h"
#include "input.h"
#include "save.h"
#include "music.h"
#include "sound.h"

#include <core/callback.h>
#include <core/graphics.h>
#include <core/sound.h>
#include <core/dialog.h>
#include <core/random.h>

#ifdef SIMULATION
#include <core/flash.h>
#include <core/eeprom.h>
#endif

game_t game;

static systime_t last_draw_time;
static systime_t last_tick_time;

static game_state_t game_state_update(uint8_t dt);

void callback_setup(void) {
#ifdef SIMULATION
    sim_flash_load("assets.dat");
    sim_eeprom_load("eeprom.dat");
#endif

    dialog_set_font(ASSET_FONT_7X7, ASSET_FONT_5X7, ASSET_FONT_3X5_BUILTIN);
    sound_set_tempo(encode_bpm_tempo(ASSET_MUSIC_TEMPO));
    sound_set_channel_volume(2, SOUND_CHANNEL2_VOLUME1);

    // load saved (or default) settings and apply them.
    load_from_eeprom();
    update_sound_volume(game.options.volume);
    update_display_contrast(game.options.contrast);
    update_music_enabled();
    sound_start(SOUND_TRACKS_STARTED);
}

bool callback_loop(void) {
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

    input_latch();

    game_music_update(dt);
    game.state = game_state_update(dt);

    uint8_t frame_delay = game.state == GAME_STATE_PLAY ?
                          millis_to_ticks(1000.0 / DISPLAY_MAX_FPS_GAME) :
                          millis_to_ticks(1000.0 / DISPLAY_MAX_FPS);
    if ((systime_t) (time - last_draw_time) >= frame_delay) {
        // links are cached in display buffer memory, drawing will destroy them.
        game.flags &= ~FLAG_CACHE_VALID;
        last_draw_time = time_get();
        return true;
    }
    return false;
}

void callback_draw(void) {
    draw();
}

static game_state_t prepare_level_end(void) {
    game_hide_inventory();

    if (tworld.end_cause == END_CAUSE_COMPLETE) {
        set_best_level_time();
        game.state_delay = LEVEL_COMPLETE_STATE_DELAY;
        game_music_start(ASSET_MUSIC_COMPLETE, MUSIC_FLAG_DELAYED | MUSIC_FLAG_SOUND_EFFECT);
        return GAME_STATE_LEVEL_COMPLETE;
    } else {
        game.state_delay = LEVEL_FAIL_STATE_DELAY;
        game_music_start(ASSET_MUSIC_FAIL, MUSIC_FLAG_DELAYED | MUSIC_FLAG_SOUND_EFFECT);
        return GAME_STATE_LEVEL_FAIL;
    }
}

void callback_sleep_scheduled(void) {
    if (game.state == GAME_STATE_PLAY) {
        game.state = GAME_STATE_PAUSE;
        game_hide_inventory();
    }
}

void callback_wakeup(void) {
    // last tick has probably happened very long ago, reset last tick time.
    last_tick_time = time_get();
}

void game_hide_inventory(void) {
    game.flags &= ~FLAG_INVENTORY_SHOWN;
}

static game_state_t update_tworld_state(uint8_t dt) {
    game_state_t new_state = game_handle_input_tworld();
    if (new_state != GAME_STATE_PLAY) {
        return new_state;
    }

    if (game.flags & FLAG_GAME_STARTED) {
        // cache position data if needed
        if (!(game.flags & FLAG_CACHE_VALID)) {
            level_get_links();
            tworld_cache_teleporters();
            game.flags |= FLAG_CACHE_VALID;
        }

        // do game steps for all ticks
        for (uint8_t i = 0; i < dt; ++i) {
            tworld_update();
            if (tworld_is_game_over()) {
                return prepare_level_end();
            }
        }
    }

    if (tworld.events & EVENT_KEY_TAKEN) {
        // A key has been picked up since last checked.
        game_sound_play(ASSET_SOUND_KEY);

    } else if (tworld.events & EVENT_BOOT_TAKEN) {
        // A key has been picked up since last checked.
        game_sound_play(ASSET_SOUND_BOOT);

    } else if (tworld.events & EVENT_CHIP_TAKEN) {
        game_sound_play(ASSET_SOUND_CHIP);

    } else if (tworld.events & EVENT_LAST_CHIP_TAKEN) {
        game_sound_play(ASSET_SOUND_LASTCHIP);

    } else if (tworld.time_left <= LOW_TIMER_THRESHOLD &&
        (uint8_t) (tworld.time_left % TICKS_PER_SECOND) == 0) {
        // Low timer and last second just ended
        game_sound_play(ASSET_SOUND_TIMER);
    }

    // Clear all sounds.
    tworld.events = 0;

    return GAME_STATE_PLAY;
}

static game_state_t game_state_update(uint8_t dt) {
    game_state_t s = game.state;

    if (game.state_delay > dt) {
        // wait in between state change.
        game.state_delay -= dt;
        return s;
    }

    // Update animation counter for all tiles, except if waiting for user to start level.
    game.anim_state += dt;

    if (s == GAME_STATE_PLAY) {
        return update_tworld_state(dt);
    } else if (!(game.flags & FLAG_DIALOG_SHOWN)) {
        // all other states show a dialog, and it wasn't initialized yet.
        if (s == GAME_STATE_MAIN_MENU) {
            open_main_menu_dialog();
        } else if (s == GAME_STATE_PASSWORD) {
            open_password_dialog();
        } else if (s == GAME_STATE_LEVEL_PACKS) {
            open_level_packs_dialog();
        } else if (s == GAME_STATE_LEVELS) {
            open_levels_dialog();
        } else if (s == GAME_STATE_LEVEL_INFO) {
            open_level_info_dialog();
        } else if (s == GAME_STATE_PAUSE) {
            open_pause_dialog();
        } else if (s == GAME_STATE_HINT) {
            open_hint_dialog();
        } else if (s == GAME_STATE_LEVEL_FAIL) {
            open_level_fail_dialog();
        } else if (s == GAME_STATE_LEVEL_COMPLETE) {
            open_level_complete_dialog();
        } else if (s == GAME_STATE_OPTIONS) {
            open_options_dialog(RESULT_SAVE_OPTIONS, RESULT_CANCEL_OPTIONS);
        } else if (s == GAME_STATE_OPTIONS_PLAY) {
            open_options_dialog(RESULT_SAVE_OPTIONS_PLAY, RESULT_CANCEL_OPTIONS_PLAY);
        } else if (s == GAME_STATE_HELP) {
            open_controls_dialog(RESULT_OPEN_MAIN_MENU);
        } else if (s == GAME_STATE_HELP_PLAY) {
            open_controls_dialog(RESULT_PAUSE);
        }
        game.flags |= FLAG_DIALOG_SHOWN;
    }

    game.last_state = s;
    return game_handle_input_dialog();
}
