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

#include "tworld_dir.h"
#include "tworld_actor.h"
#include "tworld_tile.h"

#include <core/flash.h>
#include <core/trace.h>

#include <stdint.h>
#include <stdbool.h>

// How many game ticks per in-game "second", that is, the unit of time left.
// There are 16 ticks per actual second, making each in-game second actually 1.25 s.
#define TICKS_PER_SECOND 20

#define LEVEL_LAYER_SIZE (6 * 32 * 32 / 8)
#define LEVEL_KEY_COUNT 4

#define LEVEL_LINKS_MAX_SIZE 32

// Maximum teleporters in prebuilt list (if SCAN_TELEPORTERS flag is not set).
#define LEVEL_MAX_TELEPORTERS 128

#define GRID_WIDTH 32
#define GRID_HEIGHT 32
#define GRID_SIZE (GRID_WIDTH * GRID_HEIGHT)

#define TIME_LEFT_NONE 0xffff

typedef enum {
    BOOT_MASK_WATER = 1 << BOOT_WATER,
    BOOT_MASK_FIRE = 1 << BOOT_FIRE,
    BOOT_MASK_ICE = 1 << BOOT_ICE,
    BOOT_MASK_SLIDE = 1 << BOOT_SLIDE,
} boot_mask_t;

/** Time left in a level in game ticks, or `TIME_LEFT_NONE` if untimed or not applicable. */
typedef uint16_t time_left_t;

/** Convert a game time in ticks to a game time in seconds, rounding up. */
#define time_left_to_seconds(time) \
        (uint16_t) ((uint16_t) ((time) + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND)

enum {
    EVENT_KEY_TAKEN = 1 << 0,
    EVENT_BOOT_TAKEN = 1 << 1,
    EVENT_CHIP_TAKEN = 1 << 2,
    EVENT_LAST_CHIP_TAKEN = 1 << 3,
};

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
    // Time left for level (time limit initially).
    time_left_t time_left;
    // Number of required chips left.
    uint16_t chips_left;
    // Constant level flags.
    uint8_t level_flags;

    // This zero length field is only used to mark the start of zero-initialized field on init.
    uint8_t zero_init_start[0];
    // Actor list, bounded size.
    active_actor_t actors[MAX_ACTORS_COUNT];
    // Size of the actor buffer (some actors may be hidden).
    uint8_t actors_size;
    // Last byte of current time, used with stepping.
    uint24_t current_time;
    // Game flags (FLAG_* constants in tworld.c).
    uint8_t flags;
    // Game events bitfield (EVENT_* constants).
    uint8_t events;
    // Number of keys held (in order: blue, red, green, yellow).
    uint8_t keys[LEVEL_KEY_COUNT];
    // Boots held (bitfield on bits 0 to 3).
    uint8_t boots;
    // Position after chip moved (cached).
    sposition_t chip_new_pos;
    // Last direction assumed by Chip.
    direction_t chip_last_dir;
    // Index of actor that collided with Chip, or INDEX_NONE if none.
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
    // currently active input directions
    direction_mask_t input_state;
    // used to accumulate active input directions in-between moves.
    direction_mask_t input_since_move;

#ifdef RUNTIME_CHECKS
    bool error;
#endif

#ifdef TESTING
    // stepping value 0-7 (affects teeth only)
    uint8_t stepping;
    // PRNG state
    uint32_t prng_value0;
    uint8_t prng_value1;
    uint8_t prng_value2;
#endif //TESTING
} level_t;

/**
 * Link for traps and cloners.
 */
typedef struct {
    position_t btn;
    position_t link;
} PACK_STRUCT link_t;

typedef struct {
    uint8_t size;
    link_t links[LEVEL_LINKS_MAX_SIZE];
} links_t;

typedef struct {
    uint8_t size;
    position_t teleporters[LEVEL_MAX_TELEPORTERS];
} teleporters_t;

extern links_t trap_links;
extern links_t cloner_links;
extern teleporters_t teleporters;

/**
 * Initialize game state after some fields have been loaded from flash
 * (address, layer data, time limit, chips needed).
 */
void tworld_init(void);

/**
 * Advance the game state by a single tick (or step).
 * The level state must have been initialized first, and link data must be cached.
 */
void tworld_update(void);

/**
 * Scan the grid and cache teleporter positions.
 */
void tworld_cache_teleporters(void);

/**
 * Returns true if game is over (failed or completed).
 */
bool tworld_is_game_over(void);

/**
 * Returns the current position of Chip.
 */
position_t tworld_get_current_position(void);

/**
 * Returns the tile at a position in the game grid.
 */
tile_t tworld_get_bottom_tile(position_t pos);


/**
 * Returns the actor at a position in the game grid (or ACTOR_NONE if none).
 */
actor_t tworld_get_top_tile(position_t pos);

/**
 * Returns true if the end cause is a collision.
 */
bool tworld_has_collided(void);

#endif //TWORLD_TWORLD_H
