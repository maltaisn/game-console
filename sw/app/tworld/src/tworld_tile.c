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

#include "tworld_tile.h"

uint8_t tile_get_variant(tile_t tile) {
    return tile & 0x3;
}

bool tile_is_key(tile_t tile) {
    return (tile & 0x1c) == 0x08;
}

bool tile_is_lock(tile_t tile) {
    return tile >= 0x24 && tile <= 0x27;
}

bool tile_is_boots(tile_t tile) {
    return tile >= 0x20 && tile <= 0x23;
}

bool tile_is_button(tile_t tile) {
    return tile >= 0x04 && tile <= 0x07;
}

bool tile_is_thin_wall(tile_t tile) {
    return tile >= 0x0c && tile <= 0x10;
}

bool tile_is_ice(tile_t tile) {
    return tile >= 0x13 && tile <= 0x17;
}

bool tile_is_ice_wall(tile_t tile) {
    return tile >= 0x14 && tile <= 0x17;
}

bool tile_is_slide(tile_t tile) {
    return tile >= 0x18 && tile <= 0x1c;
}

bool tile_is_monster_acting_wall(tile_t tile) {
    return tile >= 0x1e && tile <= 0x39;
}

bool tile_is_block_acting_wall(tile_t tile) {
    return tile >= 0x1f && tile <= 0x39;
}

bool tile_is_chip_acting_wall(tile_t tile) {
    return tile >= 0x33 && tile <= 0x39;
}

bool tile_is_revealable_wall(tile_t tile) {
// wall hidden & wall blue real
    return (tile & ~0x1) == 0x34;
}

bool tile_is_static(tile_t tile) {
    return tile >= 0x39 && tile <= 0x3b;
}

bool tile_is_toggle_tile(tile_t tile) {
    return (tile & ~0x1) == 0x2;
}

tile_t tile_with_toggle_state(tile_t tile, uint8_t state) {
    // toggle state only if `state` has bit 0 set
    return tile ^ (state & 0x1);
}

tile_t tile_toggle_state(tile_t tile) {
    return tile ^ 0x1;
}