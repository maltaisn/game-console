
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

#include "tworld_dir.h"

#include <stdint.h>
#include <stdbool.h>

#define MAX_ACTORS_COUNT 128
#define ACTOR_INDEX_NONE ((actor_idx_t) 0xff)

// An empty top layer tile, no actor.
#define ACTOR_NONE actor_create(ENTITY_NONE, 0)
// A top layer tile where an animation is occurring.
#define ACTOR_ANIMATION actor_create(ENTITY_NONE, 1)

// Static actors that don't appear on the actor list and can't move.
#define ACTOR_STATIC_BLOCK actor_create(ENTITY_STATIC, 0)
#define ACTOR_STATIC_FIREBALL actor_create(ENTITY_STATIC, 1)
#define ACTOR_STATIC_BALL actor_create(ENTITY_STATIC, 2)
#define ACTOR_STATIC_BLOB actor_create(ENTITY_STATIC, 3)

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

/**
 * A top layer tile. An actor is a bitfield defined as follows:
 * - [7:2]: entity (entity_t enum)
 * - [1:0]: direction (direction_t enum)
 */
typedef uint8_t actor_t;

/**
 * An index in the actor list between 0 and `MAX_ACTORS_COUNT-1`,
 * or `ACTOR_INDEX_NONE` special value.
 */
typedef uint8_t actor_idx_t;

/**
 * An actor in the actor list. This is a bitfield defined as follows:
 * - [0:4]: X position (grid_pos_t)
 * - [5:6]: internal state
 * - [7:11]: Y position (grid_pos_t)
 * - [12:15]: step, with bias of +3.
 */
typedef uint16_t active_actor_t;

/**
 * Create an actor from an entity and a direction.
 */
actor_t actor_create(entity_t entity, direction_t direction);

/**
 * Returns the entity of an actor.
 */
entity_t actor_get_entity(actor_t actor);

/**
 * Returns the direction of an actor.
 */
direction_t actor_get_direction(actor_t actor);

/**
 * Returns a new actor in a different direction.
 */
actor_t actor_with_direction(actor_t actor, direction_t direction);

/**
 * Returns a new actor with a different entity.
 */
actor_t actor_with_entity(actor_t actor, entity_t entity);

/**
 * Assuming the actor is a tank, reverse its direction.
 */
actor_t actor_reverse_tank(actor_t actor);

/**
 * Returns true if an actor is a tank or a reversed tank.
 */
bool actor_is_tank(actor_t actor);

/**
 * Returns true if an actor is a block or a ghost block.
 */
bool actor_is_block(actor_t actor);

/**
 * Returns true if an actor is a monster.
 */
bool actor_is_monster(actor_t actor);

/**
 * Returns true if an actor is a block (including ghost) or a monster.
 */
bool actor_is_monster_or_block(actor_t actor);

/**
 * Returns true if an actor should be added to the actor list.
 * This excludes static actors, ghost block, and incorrectly returns false for chip,
 * which is expected to be on the actor list.
 */
bool actor_is_on_actor_list(actor_t actor);

#endif //TWORLD_TWORLD_ACTOR_H
