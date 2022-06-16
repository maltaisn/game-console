
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

#include <core/eeprom.h>
#include <core/dialog.h>

#include <string.h>

static void set_default_options(void) {
    game.options = (game_options_t) {
            .features = GAME_FEATURE_MUSIC,
            .volume = SOUND_VOLUME_2,
            .contrast = 6,
    };

    // no need to save to EEPROM now, it will be done when options are changed or leaderboard is
    // updated. If app is launched again without EEPROM having been written, defaults are set again.
}

#define EEPROM_SAVE_SIZE (1 + sizeof game.options)
#define EEPROM_GUARD_BYTE 0x43

static SHARED_DISP_BUF uint8_t save_buf[EEPROM_SAVE_SIZE];

void load_from_eeprom(void) {
    eeprom_read(0, EEPROM_SAVE_SIZE, save_buf);
    uint8_t* buf = save_buf;

    // if first launch, guard byte isn't be set: set defaults, eeprom was never saved.
    if (*buf++ != EEPROM_GUARD_BYTE) {
        set_default_options();
        return;
    }

    memcpy(&game.options, buf, sizeof game.options);
    //buf += sizeof game.options;
}

void save_to_eeprom(void) {
    uint8_t* buf = save_buf;
    *buf++ = EEPROM_GUARD_BYTE;

    // options
    memcpy(buf, &game.options, sizeof game.options);
    //buf += sizeof game.options;

    eeprom_write(0, EEPROM_SAVE_SIZE, save_buf);

#ifdef SIMULATION
    sim_eeprom_save();
#endif
}

void save_dialog_options(void) {
    uint8_t features = 0;
    if (dialog.items[1].choice.selection) {
        features |= GAME_FEATURE_MUSIC;
    }

    uint8_t volume = dialog.items[0].number.value;
    uint8_t contrast = dialog.items[2].number.value;

    game.options = (game_options_t) {
            .features = features,
            .volume = volume,
            .contrast = contrast,
    };

    // contrast, volume and music enabled were already changed during preview

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
    // TODO
}
