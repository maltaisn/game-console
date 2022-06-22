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

#define INDEX_POS_TITLE 10
#define INDEX_POS_HINT 12
#define INDEX_POS_TRAP_LINKS 14
#define INDEX_POS_CLONER_LINKS 16

level_data_t tworld_data;

static flash_t get_level_pack_addr(level_pack_idx_t pack) {
    return asset_level_packs(pack);
}

void level_read_packs(void) {
    uint16_t pos = 0;
    level_pack_info_t* info = tworld_packs;
    for (level_pack_idx_t i = 0; i < LEVEL_PACK_COUNT; ++i) {
        flash_t addr = get_level_pack_addr(i);
        uint8_t header[3];
        flash_read(addr, sizeof header, header);
        if (header[0] != 0x54 || header[1] != 0x57) {
            // invalid signature
            info->total_levels = 0;
            info->completed_levels = 0;
            return;
        }
        uint8_t count = header[2] + 1;
        addr += count * 2 + 3;
        info->total_levels = count;

        flash_read(addr, LEVEL_PACK_NAME_MAX_LENGTH, &info->name);

        fill_completed_levels_array(pos, count, info);
        pos += count;
        ++info;
    }
}

void level_read_level(void) {
    // Read level pack index to get the start address of the current level.
    flash_t addr = get_level_pack_addr(game.current_pack);
    flash_t index_addr = addr + 3;  // skip signature & level count field
    for (level_idx_t i = 0; i <= game.current_level; ++i) {
        uint16_t offset;
        flash_read(index_addr, 2, &offset);
        addr += offset;
        index_addr += 2;
    }
    tworld.addr = addr;

    // Read data from flash
    uint8_t buf[6];
    flash_read(addr, sizeof buf, buf);
    tworld.time_limit = (buf[0] | buf[1] << 8) * TICKS_PER_SECOND;
    tworld.chips_left = buf[2] | buf[3] << 8;

    // Layer data is encoded in the same format as used at runtime, 6 bits per tile,
    // bottom layer before top layer, row-major order and little-endian.
    // We only need to decompress it.
    uint16_t layer_data_size = buf[4] | buf[5] << 8;
    lzss_decode(addr + 18, layer_data_size, tworld.bottom_layer);

    tworld_init();
}

static flash_t get_metadata_address(uint8_t index_pos) {
    uint16_t offset;
    flash_read(tworld.addr + index_pos, 2, &offset);
    return tworld.addr + offset;
}

void level_get_password(char password[LEVEL_PASSWORD_LENGTH]) {
    flash_read(tworld.addr + 4, LEVEL_PASSWORD_LENGTH, password);
}

void level_get_title(char title[LEVEL_TITLE_MAX_LENGTH]) {
    flash_t addr = get_metadata_address(INDEX_POS_TITLE);
    flash_read(addr, LEVEL_TITLE_MAX_LENGTH, title);
}

flash_t level_get_hint(void) {
    return get_metadata_address(INDEX_POS_HINT);
}

static void get_links(links_t *links, uint8_t index_pos) {
    flash_t addr = get_metadata_address(index_pos);
    uint8_t size;
    flash_read(addr, 1, &size);
    links->size = size;
    if (size > 0) {
        flash_read(addr + 1, size * sizeof(link_t), links->links);
    }
}

void level_get_links(void) {
    get_links(&trap_links, INDEX_POS_TRAP_LINKS);
    get_links(&cloner_links, INDEX_POS_CLONER_LINKS);
}

bool level_use_password(const char password[5]) {
    // TODO
    return true;
}
