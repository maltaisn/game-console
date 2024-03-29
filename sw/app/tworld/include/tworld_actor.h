
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

#include <core/defs.h>

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

/** Cause of death for an actor, also used to indicate level outcome. */
typedef enum {
    END_CAUSE_NONE = 0,
    END_CAUSE_DROWNED = 1,
    END_CAUSE_BURNED = 2,
    END_CAUSE_BOMBED = 3,
    END_CAUSE_COLLIDED_MONSTER = 4,
    END_CAUSE_COLLIDED_BLOCK = 5,
    END_CAUSE_OUTOFTIME = 6,
    END_CAUSE_COMPLETE = 7,
    // used for testing
    END_CAUSE_ERROR = 8,
} end_cause_t;

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
 * An actor step indicates how many ticks before actor makes a move.
 * This type represents values between -3 and 12 inclusively.
 */
typedef int8_t step_t;

#define STEP_BIAS 3

#define ACTOR_STATE_SHIFT 5

typedef enum {
    // Default state.
    ACTOR_STATE_NONE = 0x0 << ACTOR_STATE_SHIFT,

    // Hidden state, when the actor is dead
    // Hidden actor entries are skipped and are reused when spawning a new actor.
    ACTOR_STATE_HIDDEN = 0x1 << ACTOR_STATE_SHIFT,

    // Moved state, when the actor has chosen a move during stepping (vs. not moving).
    // This also applies when a move is forced on an actor.
    ACTOR_STATE_MOVED = 0x2 << ACTOR_STATE_SHIFT,

    // Teleported state, when the actor has just been teleported.
    ACTOR_STATE_TELEPORTED = 0x3 << ACTOR_STATE_SHIFT,
} actor_state_t;

#define ACTOR_STATE_MASK (0x3 << ACTOR_STATE_SHIFT)

/** Position on the grid (X or Y), between 0 and 31. */
typedef uint8_t grid_pos_t;

/** A position on the game grid. */
typedef struct {
    grid_pos_t x;
    grid_pos_t y;
} PACK_STRUCT position_t;

/** A position or the grid, or outside of it. */
typedef struct {
    int8_t x;
    int8_t y;
} sposition_t;

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

/**
 * Returns the X position of an active actor.
 */
grid_pos_t act_actor_get_x(active_actor_t a);

/**
 * Returns the Y position of an active actor.
 */
grid_pos_t act_actor_get_y(active_actor_t a);

/**
 * Returns the position of an active actor.
 */
position_t act_actor_get_pos(active_actor_t a);

/**
 * Returns the step value of an active actor.
 */
step_t act_actor_get_step(active_actor_t a);

/**
 * Returns the state of an active actor.
 */
actor_state_t act_actor_get_state(active_actor_t a);

#endif //TWORLD_TWORLD_ACTOR_H
