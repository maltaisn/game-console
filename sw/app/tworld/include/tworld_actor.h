
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

#ifndef TWORLD_TWORLD_ACTOR_H
#define TWORLD_TWORLD_ACTOR_H

#include <stdint.h>

#define MAX_ACTORS_COUNT 128
#define ACTOR_INDEX_NONE ((actor_idx_t) 0xff)

typedef enum {
    ENTITY_NONE = 0x00,
    ENTITY_CHIP = 0x04,
    ENTITY_STATIC = 0x08,
    ENTITY_BLOCK_GHOST = 0x10,
    ENTITY_BLOCK = 0x14,
    ENTITY_BUG = 0x18,
    ENTITY_PARAMECIUM = 0x1c,
    ENTITY_GLIDER = 0x20,
    ENTITY_FIREBALL = 0x24,
    ENTITY_BALL = 0x28,
    ENTITY_BLOB = 0x2c,
    ENTITY_TANK = 0x30,
    ENTITY_TANK_REVERSED = 0x34,
    ENTITY_WALKER = 0x38,
    ENTITY_TEETH = 0x3c,
} entity_t;

typedef enum {
    DIR_NORTH = 0x0,
    DIR_WEST = 0x1,
    DIR_SOUTH = 0x2,
    DIR_EAST = 0x3,
} direction_t;

/**
 * An actor is a bitfield defined as follows:
 * - [7:2]: entity (entity_t enum)
 * - [1:0]: direction (direction_t enum)
 */
typedef uint8_t actor_t;

/**
 * An index in the actor list between 0 and `MAX_ACTORS_COUNT-1`,
 * or `ACTOR_INDEX_NONE` special value.
 */
typedef uint8_t actor_idx_t;

#endif //TWORLD_TWORLD_ACTOR_H
