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

#define VARIANT2_MASK 0x1
#define VARIANT4_MASK 0x3
#define TYPE2_MASK (~VARIANT2_MASK)
#define TYPE4_MASK (~VARIANT4_MASK)

uint8_t tile_get_variant(tile_t tile) {
    return tile & VARIANT4_MASK;
}

bool tile_is_key(tile_t tile) {
    return (tile & 0x1c) == 0x08;
}

bool tile_is_lock(tile_t tile) {
    return (tile & TYPE4_MASK) == TILE_LOCK_BLUE;
}

bool tile_is_boots(tile_t tile) {
    return (tile & TYPE4_MASK) == TILE_BOOTS_WATER;
}

bool tile_is_button(tile_t tile) {
    return (tile & TYPE4_MASK) == TILE_BUTTON_GREEN;
}

bool tile_is_thin_wall(tile_t tile) {
    return tile >= TILE_THIN_WALL_N && tile <= TILE_THIN_WALL_SE;
}

bool tile_is_ice(tile_t tile) {
    return tile >= TILE_ICE && tile <= TILE_ICE_CORNER_NE;
}

bool tile_is_ice_wall(tile_t tile) {
    return (tile & TYPE4_MASK) == TILE_ICE_CORNER_NW;
}

bool tile_is_slide(tile_t tile) {
    return tile >= TILE_FORCE_FLOOR_N && tile <= TILE_FORCE_FLOOR_RANDOM;
}

bool tile_is_monster_acting_wall(tile_t tile) {
    return tile >= 0x1e && tile <= 0x3a;
}

bool tile_is_block_acting_wall(tile_t tile) {
    return tile >= 0x1f && tile <= 0x3a;
}

bool tile_is_chip_acting_wall(tile_t tile) {
    return tile >= 0x33 && tile <= 0x3a;
}

bool tile_is_revealable_wall(tile_t tile) {
    return (tile & TYPE2_MASK) == 0x34;
}

bool tile_is_static(tile_t tile) {
    return (tile & TYPE2_MASK) == 0x3a;
}

bool tile_is_toggle_tile(tile_t tile) {
    return (tile & TYPE2_MASK) == 0x02;
}

tile_t tile_with_toggle_state(tile_t tile, bool state) {
    return tile ^ state;
}

tile_t tile_toggle_state(tile_t tile) {
    return tile_with_toggle_state(tile, true);
}

tile_t tile_make_key(key_type_t variant) {
    return (variant < 2 ? TILE_KEY_BLUE : TILE_KEY_GREEN) | variant;
}

tile_t tile_make_boots(boot_type_t variant) {
    return TILE_BOOTS_WATER | variant;
}

tile_t tile_make_dead_chip(end_cause_t end_cause) {
    return end_cause + 0x40;
}

tile_t tile_make_swimming_chip(actor_t chip) {
    return chip + (uint8_t) (TILE_CHIP_SWIMMING_N - ENTITY_CHIP);
}
