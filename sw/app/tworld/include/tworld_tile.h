
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

#include <stdint.h>

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
} tile_t;

#endif //TWORLD_TWORLD_TILE_H
