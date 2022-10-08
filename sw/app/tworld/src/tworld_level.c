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

#include "tworld_level.h"
#include "tworld.h"
#include "assets.h"
#include "save.h"
#include "lzss.h"

#include <core/flash.h>

#include <string.h>

// Field positions in level pack data.
#define POS_LEVEL_COUNT 2
#define POS_FIRST_SECRET_LEVEL 3
#define POS_LEVEL_INDEX 4

// Field positions in level data.
#define POS_PASSWORD 7
#define POS_INDEX_TITLE 11
#define POS_INDEX_HINT 13
#define POS_INDEX_TRAP_LINKS 15
#define POS_INDEX_CLONER_LINKS 17
#define POS_LAYER_DATA 19

// Unlock threshold in ratio of previous level pack completed levels.
// Format is UQ0.8 (divide by 256 to get actual value)
// This corresponds to level 100 completed if there are 149 levels in previous pack.
#define LEVEL_PACK_UNLOCK_THRESHOLD (uint8_t) ((100 * 256 + 128) / 149)

level_data_t tworld_data;

static flash_t get_level_pack_addr(level_pack_idx_t pack) {
    return asset_level_packs(pack);
}

void level_read_packs(void) {
    uint16_t pos = 0;
    level_pack_info_t* info = tworld_packs.packs;
    bool next_is_unlocked = true;
    for (level_pack_idx_t i = 0; i < LEVEL_PACK_COUNT; ++i) {
        flash_t addr = get_level_pack_addr(i);

        uint8_t header[4];
        flash_read(addr, sizeof header, header);
        if (header[0] != 0x54 || header[1] != 0x57) {
            // invalid signature, should not happen.
            info->total_levels = 0;
            info->completed_levels = 0;
            info->flags = 0;
            return;
        }

        uint8_t count = header[POS_LEVEL_COUNT];
        info->first_secret_level = header[POS_FIRST_SECRET_LEVEL];
        info->flags = 0;
        info->pos = pos;
        info->total_levels = count;

        if (next_is_unlocked) {
            info->flags |= LEVEL_PACK_FLAG_UNLOCKED;
        }

        addr += count * 2 + POS_LEVEL_INDEX;
        flash_read(addr, LEVEL_PACK_NAME_MAX_LENGTH, &info->name);
        fill_completed_levels_array(pos, info);
        pos += count;

        next_is_unlocked = (info->completed_levels >= (uint8_t) ((uint16_t)
                (info->total_levels * LEVEL_PACK_UNLOCK_THRESHOLD) >> 8));

        ++info;
    }
}

void level_read_level(void) {
    // Read level pack index to get the start address of the current level.
    flash_t addr = get_level_pack_addr(game.current_pack);
    flash_t index_addr = addr + POS_LEVEL_INDEX;
    for (level_idx_t i = 0; i <= game.current_level; ++i) {
        uint16_t offset;
        flash_read(index_addr, 2, &offset);
        addr += offset;
        index_addr += 2;
    }
    tworld.addr = addr;

    // Read data from flash
    uint8_t buf[7];
    flash_read(addr, sizeof buf, buf);

    tworld.level_flags = buf[0];
    tworld.time_left = buf[1] | buf[2] << 8;
    tworld.chips_left = buf[3] | buf[4] << 8;

    // Layer data is encoded in the same format as used at runtime, 6 bits per tile,
    // bottom layer before top layer, row-major order and little-endian.
    // We only need to decompress it.
    uint16_t layer_data_size = buf[5] | buf[6] << 8;
    lzss_decode(addr + POS_LAYER_DATA, layer_data_size, tworld.bottom_layer);

    tworld_init();
}

static flash_t get_metadata_address(uint8_t index_pos) {
    uint16_t offset;
    flash_read(tworld.addr + index_pos, 2, &offset);
    return tworld.addr + offset;
}

void level_get_password(char password[static LEVEL_PASSWORD_LENGTH]) {
    flash_read(tworld.addr + POS_PASSWORD, LEVEL_PASSWORD_LENGTH, password);
    password[LEVEL_PASSWORD_LENGTH - 1] = '\0';
}

flash_t level_get_title(void) {
    return get_metadata_address(POS_INDEX_TITLE);
}

flash_t level_get_hint(void) {
    return get_metadata_address(POS_INDEX_HINT);
}

static void get_links(links_t* links, uint8_t index_pos) {
    flash_t addr = get_metadata_address(index_pos);
    uint8_t size;
    flash_read(addr, 1, &size);
    links->size = size;
    if (size > 0) {
        flash_read(addr + 1, size * sizeof(link_t), links->links);
    }
}

void level_get_links(void) {
    get_links(&trap_links, POS_INDEX_TRAP_LINKS);
    get_links(&cloner_links, POS_INDEX_CLONER_LINKS);
}

bool level_use_password(void) {
    // Iterate over unlocked level packs and iterate over levels in the pack to
    // find a level with a password matching the given password.
    char buf[4];
    for (level_pack_idx_t i = 0; i < LEVEL_PACK_COUNT; ++i) {
        const level_pack_info_t* info = &tworld_packs.packs[i];
        if (info->flags & LEVEL_PACK_FLAG_UNLOCKED) {
            flash_t addr = get_level_pack_addr(i);
            flash_t index_addr = addr + POS_LEVEL_INDEX;
            addr += POS_PASSWORD;

            for (level_idx_t j = 0; j <= info->total_levels; ++j) {
                uint16_t offset;
                flash_read(index_addr, 2, &offset);
                addr += offset;
                index_addr += 2;
                flash_read(addr, sizeof buf, buf);
                if (memcmp(tworld_packs.password_buf, buf, sizeof buf) == 0) {
                    // Level found matching password, go to it.
                    game.current_pack = i;
                    game.current_level = j;
                    game.current_level_pos = info->pos + j;
                    game.flags |= FLAG_PASSWORD_USED;
                    return true;
                }
            }
        }
    }

    return false;
}

bool level_is_completed(const level_pack_info_t* info, level_idx_t level) {
    return info->completed_array[level / 8] & (1 << (level % 8));
}

bool level_is_unlocked(const level_pack_info_t* info, level_idx_t level) {
    return level <= info->last_unlocked || level_is_completed(info, level) ||
           ((info->flags & LEVEL_PACK_FLAG_SECRET_UNLOCKED) && level >= info->first_secret_level);
}

bool level_is_secret_locked(const level_pack_info_t* info, level_idx_t level) {
    return level >= info->first_secret_level && !(info->flags & LEVEL_PACK_FLAG_SECRET_UNLOCKED);
}
