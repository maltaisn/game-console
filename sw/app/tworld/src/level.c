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

#include "level.h"
#include "assets.h"
#include "save.h"

#include <core/flash.h>

level_data_t tworld_data;

static flash_t get_level_pack_addr(level_pack_idx_t pack) {
    return asset_level_packs(pack);
}

void level_read_packs(void) {
    uint16_t pos = 0;
    level_pack_info_t *info = tworld_packs;
    for (level_pack_idx_t i = 0; i < LEVEL_PACK_COUNT; ++i) {
        flash_t addr = get_level_pack_addr(i);
        uint8_t header[3];
        flash_read(addr, sizeof header, header);
        if (*((uint16_t*) header) != 0x5754) {
            // invalid signature
            info->total_levels = 0;
            info->completed_levels = 0;
            return;
        }
        uint8_t count = header[2] + 1;
        addr += count * 2 + 3;
        info->total_levels = count;

        flash_read(addr, LEVEL_PACK_NAME_MAX_LENGTH, &info->name);

        info->completed_levels = fill_completed_levels_array(pos, count, info->completed_array);
        pos += count;
        ++info;
    }
}

void level_read_level(uint8_t pack, uint8_t level) {
    tworld.pack = pack;
    tworld.level = level;
    // TODO
}
