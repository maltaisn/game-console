
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

#include "save.h"
#include "assets.h"
#include "game.h"
#include "music.h"

#include <core/eeprom.h>
#include <core/dialog.h>
#include <core/trace.h>

#include <string.h>

#define EEPROM_SAVE_SIZE (1 + sizeof(game.options))
#define SAVE_TIME_SLOTS 600
#define SAVE_TIME_POS EEPROM_SAVE_SIZE
#define SAVE_TIME_SIZE ((SAVE_TIME_SLOTS * 10 + 7) / 8)
#define EEPROM_TOTAL_SIZE (EEPROM_SAVE_SIZE + SAVE_TIME_SIZE)

#define EEPROM_GUARD_BYTE 0x43

#define save_time_block_address(pos) (SAVE_TIME_POS + (pos) / 4 * 5)

static SHARED_DISP_BUF uint8_t save_buf[255];

static void save_to_eeprom(void) {
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

static void set_default_options(void) {
    game.options = (game_options_t) {
            .features = GAME_FEATURE_MUSIC | GAME_FEATURE_SOUND_EFFECTS,
            .volume = SOUND_VOLUME_2,
            .contrast = 6,
    };

    // write all level times to "not completed"
    eeprom_t addr = SAVE_TIME_POS;
    uint16_t size = SAVE_TIME_SIZE;
    memset(save_buf, 0xff, sizeof save_buf);
    while (size) {
        uint8_t block_size = sizeof save_buf;
        if (block_size > size) {
            block_size = size;
        }
        eeprom_write(addr, block_size, save_buf);
        addr += block_size;
        size -= block_size;
    }

    save_to_eeprom();
}

void load_from_eeprom(void) {
#ifdef RUNTIME_CHECKS
    if (EEPROM_TOTAL_SIZE != EEPROM_RESERVED_SPACE) {
        trace("EEPROM total size doesn't match with reserved size.");
        return;
    }
#endif

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

    game.options.features = features;
    game.options.volume = volume;
    game.options.contrast = contrast;

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
    if (game.options.features & GAME_FEATURE_MUSIC) {
        if (game.state >= GAME_SSEP_LEVEL_BG) {
            game_music_start_level_music(MUSIC_FLAG_LOOP);
        } else {
            game_music_start(ASSET_MUSIC_MENU, MUSIC_FLAG_LOOP);
        }
    } else {
        game_music_stop();
    }
}

struct time_block {
    uint16_t time0 : 10;
    uint16_t time1 : 10;
    uint16_t time2 : 10;
    uint16_t time3 : 10;
} PACK_STRUCT;

static void read_level_time_block(eeprom_t addr, uint16_t times[4]) {
    struct time_block block;
    eeprom_read(addr, sizeof block, &block);
    times[0] = block.time0;
    times[1] = block.time1;
    times[2] = block.time2;
    times[3] = block.time3;
}

time_left_t get_best_level_time(uint16_t pos) {
    const eeprom_t addr = save_time_block_address(pos);
    uint16_t times[4];
    read_level_time_block(addr, times);

    const uint16_t time = times[pos % 4];
    if (time >= 1000) {
        // Either unknown (not completed), or untimed.
        return TIME_LEFT_NONE;
    } else {
        return time * TICKS_PER_SECOND;
    }
}

void set_best_level_time(void) {
    // Get new time in in-game seconds, rounded up.
    const uint16_t pos = game.current_level_pos;
    time_left_t new_time = tworld.time_left;
    if (new_time == TIME_LEFT_NONE) {
        // Level is untimed; still save something to unlock the level.
        new_time = SAVE_TIME_UNTIMED;
    } else {
        new_time += TICKS_PER_SECOND - 1;
        const uint16_t best_time = get_best_level_time(pos);
        if (best_time != TIME_LEFT_NONE && new_time <= best_time) {
            // New not better than old time, don't save it.
            return;
        }
        new_time /= TICKS_PER_SECOND;
    }

    // Save new time to EEPROM. Write the whole block, it's easier.
    const eeprom_t addr = save_time_block_address(pos);
    struct time_block block;
    eeprom_read(addr, sizeof block, &block);
    switch (pos % 4) {
        case 0:
            block.time0 = new_time;
            break;
        case 1:
            block.time1 = new_time;
            break;
        case 2:
            block.time2 = new_time;
            break;
        case 3:
            block.time3 = new_time;
            break;
    }
    eeprom_write(addr, sizeof block, &block);

#ifdef SIMULATION
    sim_eeprom_save();
#endif
}

void fill_completed_levels_array(uint16_t pos, level_pack_info_t *info) {
    info->last_unlocked = 0;
    uint8_t *arr = info->completed_array - 1;
    eeprom_t addr = save_time_block_address(pos);
    uint16_t times[4];
    uint8_t bit = 0;
    uint8_t mask = 1;
    uint8_t block_pos = pos % 4;
    uint8_t completed = 0;
    level_idx_t i = 0;
    goto start;
    for (; i < info->total_levels; ++i) {
        if (block_pos == 0) {
start:
            read_level_time_block(addr, times);
            addr += 5;
        }
        if (bit == 0) {
            *(++arr) = 0;
            mask = 1;
        }
        if (times[block_pos] != SAVE_TIME_UNKNOWN) {
            *arr |= mask;
            ++completed;
            if (i >= info->first_secret_level) {
                info->flags |= LEVEL_PACK_FLAG_SECRET_UNLOCKED;
            }
        } else if (i == completed) {
            // first level not completed since start, this level is unlocked.
            info->last_unlocked = i;
        }
        block_pos = (block_pos + 1) % 4;
        bit = (bit + 1) % 8;
        mask <<= 1;
    }

    info->completed_levels = completed;
    if (info->last_unlocked >= info->first_secret_level) {
        // Secret levels can't be unlocked simply by completing the previous level.
        info->last_unlocked = info->first_secret_level - 1;
    }
}
