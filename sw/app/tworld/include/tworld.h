
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

#ifndef TWORLD_TWORLD_H
#define TWORLD_TWORLD_H

#include "tworld_actor.h"
#include "tworld_tile.h"

#include <core/flash.h>

#include <stdint.h>

#define LEVEL_LAYER_SIZE (6 * 32 * 32 / 8)
#define LEVEL_KEY_COUNT 4

// these maximum string lengths include the nul terminator
#define LEVEL_TITLE_MAX_LENGTH 40
#define LEVEL_HINT_MAX_LENGTH 128
#define LEVEL_PASSWORD_LENGTH 5

#define LEVEL_LINKAGE_MAX_SIZE 32

typedef enum {
    END_CAUSE_NONE,
    END_CAUSE_COMPLETE,
    END_CAUSE_DROWNED,
    END_CAUSE_BURNED,
    END_CAUSE_BOMBED,
    END_CAUSE_OUTOFTIME,
    END_CAUSE_COLLIDED,
} end_cause_t;

/** Position on the grid (X or Y), between 0 and 31. */
typedef uint8_t grid_pos_t;

/**
 * Data structure for the current level state.
 */
typedef struct {
    // Address of level in external flash.
    flash_t addr;
    // Next direction for random force floor.
    direction_t random_slide_dir;

    // Top and bottom layers, 6 bits per tile, row-major order and little-endian.
    uint8_t bottom_layer[LEVEL_LAYER_SIZE];
    uint8_t top_layer[LEVEL_LAYER_SIZE];

    // Time limit for level.
    uint16_t time_limit;
    // Number of required chips left.
    uint16_t chips_left;

    // This zero length field is only used to mark the start of zero-initialized field on init.
    uint8_t zero_init_start[0];

    // Actor list, bounded size.
    uint16_t actors[MAX_ACTORS_COUNT];
    // size of the actor buffer (some actors may be hidden).
    uint8_t actors_size;

    // Game flags (FLAG_* constants below).
    uint8_t flags;
    // Current time elapsed since start, in ticks.
    uint24_t current_time;
    // Number of keys held (in order: blue, red, green, yellow).
    uint8_t keys[LEVEL_KEY_COUNT];
    // Boots held (bitfield on bits 0 to 3).
    uint8_t boots;
    // Position after chip moved (cached).
    grid_pos_t chip_new_pos_x;
    grid_pos_t chip_new_pos_y;
    // Index of actor that collided with chip, or INDEX_NONE if none.
    actor_idx_t collided_with;
    // Actor that collision occured with.
    actor_t collided_actor;
    // Ticks since chip last move (used to go back to rest position).
    uint8_t ticks_since_move;
    // Cause of death (or NONE if not dead).
    end_cause_t end_cause;
    // Last chip direction (since last start_movement call).
    direction_t last_chip_dir;
    // Saved chip tile in special teleporter case to prevent chip from disappearing.
    actor_t teleported_chip;
    // Index of actor currently springing a trap, or INDEX_NONE if none.
    actor_idx_t actor_springing_trap;

} level_t;

/**
 * Link for traps and cloners.
 */
typedef struct {
    grid_pos_t btn_x;
    grid_pos_t btn_y;
    grid_pos_t link_x;
    grid_pos_t link_y;
} link_t;

/**
 * Initialize game state after some fields have been loaded from flash
 * (address, layer data, time limit, chips needed).
 */
void tworld_init(void);

/**
 * TODO
 */
void tworld_update(void);

#endif //TWORLD_TWORLD_H
