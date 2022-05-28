
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

#include <save.h>
#include <assets.h>
#include <game.h>
#include <tetris.h>
#include <music.h>

#include <core/eeprom.h>
#include <core/dialog.h>

#include <string.h>

#ifdef SIMULATION
#include <stdio.h>
#endif

static void set_default_options(void) {
    game.options = (game_options_t) {
            .features = GAME_FEATURE_MUSIC | GAME_FEATURE_SOUND_EFFECTS,
            .volume = SOUND_VOLUME_2,
            .contrast = 6,
    };
    tetris.options = (tetris_options_t) {
            .features = TETRIS_FEATURE_HOLD | TETRIS_FEATURE_GHOST |
                        TETRIS_FEATURE_WALL_KICKS | TETRIS_FEATURE_TSPINS,
            .preview_pieces = 5,
    };
    game.leaderboard.size = 0;
}

#define EEPROM_SAVE_SIZE (1 + sizeof game.options + sizeof tetris.options + sizeof game.leaderboard)
#define EEPROM_GUARD_BYTE 0x55

static SHARED_DISP_BUF uint8_t save_buf[EEPROM_SAVE_SIZE];

void load_from_eeprom(void) {
    // use display buffer as temporary memory to write data
    eeprom_read(0, EEPROM_SAVE_SIZE, save_buf);
    uint8_t* buf = save_buf;

    // if first launch, guard byte won't be set: set defaults
    if (*buf++ != EEPROM_GUARD_BYTE) {
        set_default_options();
        return;
    }

    memcpy(&game.options, buf, sizeof game.options);
    buf += sizeof game.options;
    memcpy(&tetris.options, buf, sizeof tetris.options);
    buf += sizeof tetris.options;

    memcpy(&game.leaderboard, buf, sizeof game.leaderboard);
    //buf += sizeof game.leaderboard;
}

void save_to_eeprom(void) {
    // use display buffer as temporary memory to write data
    uint8_t* buf = save_buf;
    *buf++ = EEPROM_GUARD_BYTE;

    // options
    memcpy(buf, &game.options, sizeof game.options);
    buf += sizeof game.options;
    memcpy(buf, &tetris.options, sizeof tetris.options);
    buf += sizeof tetris.options;

    // leaderboard
    memcpy(buf, &game.leaderboard, sizeof game.leaderboard);
    //buf += sizeof game.leaderboard;

    eeprom_write(0, EEPROM_SAVE_SIZE, save_buf);

#ifdef SIMULATION
    sim_eeprom_save();
#endif
}

game_state_t save_highscore(void) {
    const char* name = dialog.items[0].text.text;
    if (!*name) {
        // name is empty, don't hide dialog.
        return GAME_STATE_HIGH_SCORE;
    }

    strcpy(game.leaderboard.entries[game.new_highscore_pos].name, dialog.items[0].text.text);
    save_to_eeprom();

    return GAME_STATE_LEADERBOARD_PLAY;
}

void save_dialog_options(void) {
    uint8_t features = 0;
    if (dialog.items[1].choice.selection) {
        features |= GAME_FEATURE_MUSIC;
    }
    if (dialog.items[2].choice.selection) {
        features |= GAME_FEATURE_SOUND_EFFECTS;
    }

    uint8_t volume = dialog.items[0].number.value;
    uint8_t contrast = dialog.items[3].number.value;

    game.options = (game_options_t) {
            .features = features,
            .volume = volume,
            .contrast = contrast,
    };

    // contrast, volume and music enabled were already changed during preview

    save_to_eeprom();
}

void save_dialog_extra_options(void) {
    uint8_t preview_pieces = dialog.items[0].number.value;
    tetris.options.preview_pieces = preview_pieces;

    uint8_t features = 0;
    if (dialog.items[1].choice.selection) {
        features |= TETRIS_FEATURE_GHOST;
    }
    if (dialog.items[2].choice.selection) {
        features |= TETRIS_FEATURE_HOLD;
    }
    if (dialog.items[3].choice.selection) {
        features |= TETRIS_FEATURE_WALL_KICKS;
    }
    if (dialog.items[4].choice.selection) {
        features |= TETRIS_FEATURE_TSPINS;
    }
    tetris.options.features = features;

    save_to_eeprom();
}

void update_display_contrast(uint8_t value) {
    display_set_contrast(value * 15);
}

void update_sound_volume(uint8_t volume) {
    sound_volume_t v;
    if (volume == 0) {
        v = SOUND_VOLUME_OFF;
    } else {
        v = volume - 1;
    }
    sound_set_volume(v);
}

void update_music_enabled(void) {
    if (game.options.features & GAME_FEATURE_MUSIC) {
        game_music_start(game.state == GAME_STATE_OPTIONS_PLAY ?
                         ASSET_MUSIC_THEME : ASSET_MUSIC_MENU, MUSIC_FLAG_LOOP);
    } else {
        game_music_stop();
    }
}
