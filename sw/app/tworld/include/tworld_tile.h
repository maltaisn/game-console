
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

#ifndef TWORLD_TWORLD_TILE_H
#define TWORLD_TWORLD_TILE_H

#include "tworld_actor.h"

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TILE_FLOOR = 0x00,
    TILE_TRAP = 0x01,
    TILE_TOGGLE_FLOOR = 0x02,
    TILE_TOGGLE_WALL = 0x03,
    // buttons
    TILE_BUTTON_GREEN = 0x04,
    TILE_BUTTON_RED = 0x05,
    TILE_BUTTON_BROWN = 0x06,
    TILE_BUTTON_BLUE = 0x07,
    // floor acting keys
    TILE_KEY_BLUE = 0x08,
    TILE_KEY_RED = 0x09,
    // thin wall
    TILE_THIN_WALL_N = 0x0c,
    TILE_THIN_WALL_W = 0x0d,
    TILE_THIN_WALL_S = 0x0e,
    TILE_THIN_WALL_E = 0x0f,
    TILE_THIN_WALL_SE = 0x10,
    // ice
    TILE_ICE = 0x13,
    TILE_ICE_CORNER_NW = 0x14,
    TILE_ICE_CORNER_SW = 0x15,
    TILE_ICE_CORNER_SE = 0x16,
    TILE_ICE_CORNER_NE = 0x17,
    // force floor
    TILE_FORCE_FLOOR_N = 0x18,
    TILE_FORCE_FLOOR_W = 0x19,
    TILE_FORCE_FLOOR_S = 0x1a,
    TILE_FORCE_FLOOR_E = 0x1b,
    TILE_FORCE_FLOOR_RANDOM = 0x1c,
    // acting walls for monsters only
    TILE_GRAVEL = 0x1e,
    TILE_EXIT = 0x1f,
    TILE_BOOTS_WATER = 0x20,
    TILE_BOOTS_FIRE = 0x21,
    TILE_BOOTS_ICE = 0x22,
    TILE_BOOTS_FORCE_FLOOR = 0x23,
    // acting walls for monsters and blocks
    TILE_LOCK_BLUE = 0x24,
    TILE_LOCK_RED = 0x25,
    TILE_LOCK_GREEN = 0x26,
    TILE_LOCK_YELLOW = 0x27,
    TILE_KEY_GREEN = 0x2a,
    TILE_KEY_YELLOW = 0x2b,
    TILE_THIEF = 0x2c,
    TILE_CHIP = 0x2d,
    // acting walls for all actors
    TILE_RECESSED_WALL = 0x2e,
    TILE_WALL_BLUE_FAKE = 0x2f,
    TILE_SOCKET = 0x30,
    TILE_DIRT = 0x31,
    TILE_HINT = 0x32,
    TILE_WALL = 0x33,
    TILE_WALL_BLUE_REAL = 0x34,
    TILE_WALL_HIDDEN = 0x35,
    TILE_WALL_INVISIBLE = 0x36,
    TILE_FAKE_EXIT = 0x37,
    TILE_CLONER = 0x38,
    // static
    TILE_STATIC_CLONER = 0x39,
    TILE_STATIC_TRAP = 0x3a,
    // special
    TILE_TELEPORTER = 0x3c,
    TILE_WATER = 0x3d,
    TILE_FIRE = 0x3e,
    TILE_BOMB = 0x3f,
    // internal use only, not encodable
    TILE_BLOCK = 0x40,
    TILE_CHIP_DROWNED = 0x41,
    TILE_CHIP_BURNED = 0x42,
    TILE_CHIP_BOMBED = 0x43,
    TILE_CHIP_SWIMMING_N = 0x44,
    TILE_CHIP_SWIMMING_W = 0x45,
    TILE_CHIP_SWIMMING_S = 0x46,
    TILE_CHIP_SWIMMING_E = 0x47,
} tile_t;

typedef enum {
    BOOT_WATER = 0,
    BOOT_FIRE = 1,
    BOOT_ICE = 2,
    BOOT_SLIDE = 3,
} boot_type_t;

typedef enum {
    KEY_BLUE = 0,
    KEY_RED = 1,
    KEY_GREEN = 2,
    KEY_YELLOW = 3,
} key_type_t;

/** Returns the tile variant 0-3 (key, lock, boot, button, force floor, ice corner).  */
uint8_t tile_get_variant(tile_t tile);

/** Returns true if tile is a key. */
bool tile_is_key(tile_t tile);

/** Returns true if tile is a lock. */
bool tile_is_lock(tile_t tile);

/** Returns true if tile is boots. */
bool tile_is_boots(tile_t tile);

/** Returns true if tile is a button. */
bool tile_is_button(tile_t tile);

/** Returns true if tile is a thin wall. */
bool tile_is_thin_wall(tile_t tile);

/** Returns true if tile is ice, including ice corners. */
bool tile_is_ice(tile_t tile);

/** Returns true if tile is an ice corner. */
bool tile_is_ice_wall(tile_t tile);

/** Returns true if tile is a slide floor (force floor). */
bool tile_is_slide(tile_t tile);

/** Returns true if tile is a wall for monsters. */
bool tile_is_monster_acting_wall(tile_t tile);

/** Returns true if tile is a wall for blocks. */
bool tile_is_block_acting_wall(tile_t tile);

/** Returns true if tile is a wall for chip. */
bool tile_is_chip_acting_wall(tile_t tile);

/** Returns true if tile is a hidden wall or a real blue wall. */
bool tile_is_revealable_wall(tile_t tile);

/** Returns true if tile is static (meaning an actor on top of it will be considered static). */
bool tile_is_static(tile_t tile);

/** Returns true if tile is a toggle wall in either state. */
bool tile_is_toggle_tile(tile_t tile);

/** Returns a toggle tile in another state. */
tile_t tile_with_toggle_state(tile_t tile, uint8_t state);

/** Returns a toggle tile in the opposite state. */
tile_t tile_toggle_state(tile_t tile);

/** Returns a key tile of a particular variant. */
tile_t tile_make_key(key_type_t variant);

/** Returns a boot tile of a particular variant. */
tile_t tile_make_boots(boot_type_t variant);

/** Returns the internal tile used for a chip death (burned, bombed or drowned). */
tile_t tile_make_dead_chip(end_cause_t end_cause);

/** Returns a swimming chip tile for a chip actor. */
tile_t tile_make_swimming_chip(actor_t chip);

#endif //TWORLD_TWORLD_TILE_H
