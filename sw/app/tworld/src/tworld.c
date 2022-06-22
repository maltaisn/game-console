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

#include "tworld.h"
#include "tworld_level.h"
#include "defs.h"

#include <string.h>
#include <core/trace.h>

#define CHIP_REST_DIRECTION DIR_SOUTH
// Number of game ticks before chip moves to rest position.
#define CHIP_REST_TICKS 15

enum {
    // Indicates toggle floor/wall (=0x1, this is important).
    FLAG_TOGGLE_STATE = 1 << 0,
    // Indicates that there may be "reverse tanks" on the grid.
    FLAG_TURN_TANKS = 1 << 1,
    // Indicates that chip has moved by himself.
    FLAG_CHIP_SELF_MOVED = 1 << 2,
    // Indicates that chip move has been forced.
    FLAG_CHIP_FORCE_MOVED = 1 << 3,
    // Indicates that chip can override force floor direction.
    FLAG_CHIP_CAN_UNSLIDE = 1 << 4,
    // Indicates that chip is stuck on a teleporter.
    FLAG_CHIP_STUCK = 1 << 5,
    // Indicates that the level is untimed.
    FLAG_NO_TIME_LIMIT = 1 << 7,
};

typedef enum {
    // Default state.
    ACTOR_STATE_NONE = 0x0 << 5,

    // Hidden state, when the actor is dead
    // Hidden actor entries are skipped and are reused when spawning a new actor.
    ACTOR_STATE_HIDDEN = 0x1 << 5,

    // Moved state, when the actor has chosen a move during stepping (vs. not moving).
    // This also applies when a move is forced on an actor.
    ACTOR_STATE_MOVED = 0x2 << 5,

    // Teleported state, when the actor has just been teleported.
    ACTOR_STATE_TELEPORTED = 0x3 << 5,

    // TODO
    ACTOR_STATE_GHOST = 0x4 << 5,

    // Temporary extra state used to indicate that the actor has died and it's tile
    // Should be replaced by an animation tile. (note: STATE_DIED & 0x3 == STATE_HIDDEN)
    ACTOR_STATE_DIED = 0x4 << 5,
} actor_state_t;

#define ACTOR_STATE_MASK (0x3 << 5)

enum {
    BOOTS_WATER = 1 << 0,
    BOOTS_FIRE = 1 << 1,
    BOOTS_ICE = 1 << 2,
    BOOTS_SLIDE = 1 << 3,
};

/**
 * An actor step indicates how many ticks before actor makes a move.
 * This type represents values between -3 and 12 inclusively.
 */
typedef int8_t step_t;

#define STEP_BIAS 3

/**
 * Container used during step processing to store information about an actor for fast access.
 * There is always one or two instances of this container so size is not an issue.
 */
typedef struct {
    actor_idx_t index;
    grid_pos_t x;
    grid_pos_t y;
    step_t step;
    actor_state_t state;
    entity_t entity;
    direction_t direction;
} moving_actor_t;

/** A position or the grid, or outside of it. */
typedef struct {
    int8_t x;
    int8_t y;
} sposition_t;

SHARED_DISP_BUF links_t trap_links;
SHARED_DISP_BUF links_t cloner_links;

// ============== All utility functions ====================

// there are 4 tiles per block of 3 bytes in the layer data arrays
#define TILES_PER_BLOCK 4

#define nibble_swap(x) ((uint8_t) ((x) >> 4 | (x) << 4))

static AVR_OPTIMIZE uint8_t get_tile_in_tile_block(grid_pos_t x, grid_pos_t y,
                                                   const uint8_t* layer) {
    // note: this function was hand optimized to produce the best assembly output,
    // since it may be called a several thousand times per second.
    // execution time starting from get_x_tile: between 35 and 42 cycles.
    const uint8_t* block = &layer[(uint8_t) ((y << 3) + (x >> 2)) * 3];
    switch (x % TILES_PER_BLOCK) {
        case 0:
            // andi
            return block[0] & 0x3f;
        case 1:
            // lsl, rol, lsl, rol, andi
            return (((uint16_t) (block[0] | block[1] << 8) << 2) >> 8) & 0x3f;
        case 2:
            // swap, andi, swap, andi, or
            return (nibble_swap(block[1]) & 0xf) | (nibble_swap(block[2]) & 0x30);
        case 3:
            // lsr, lsr
            return block[2] >> 2;
    }
    return 0;
}

/** Returns the tile on the bottom layer at a position. */
static tile_t get_bottom_tile(grid_pos_t x, grid_pos_t y) {
    return get_tile_in_tile_block(x, y, tworld.bottom_layer);
}

/** Returns the actor on the top layer at a position. */
static actor_t get_top_tile(grid_pos_t x, grid_pos_t y) {
    return get_tile_in_tile_block(x, y, tworld.top_layer);
}

static AVR_OPTIMIZE void set_tile_in_tile_block(grid_pos_t x, grid_pos_t y,
                                                uint8_t value, uint8_t* layer) {
    // execution time starting from set_x_tile: between 38 and 48 cycles.
    uint8_t* block = &layer[(uint8_t) ((y << 3) + (x >> 2)) * 3];
    switch (x % TILES_PER_BLOCK) {
        case 0: {
            // andi, or
            block[0] = (block[0] & ~0x3f) | value;
            break;
        }
        case 1: {
            // lsr, ror, lsr, ror, andi, or, andi, or
            uint16_t s = (value << 8) >> 2;
            block[0] = (block[0] & ~0xc0) | (s & 0xff);
            block[1] = (block[1] & ~0x0f) | (s >> 8);
            break;
        }
        case 2: {
            // swap, andi, andi, or, andi, andi, or
            uint8_t s = nibble_swap(value);
            block[1] = (block[1] & ~0xf0) | (s & 0xf0);
            block[2] = (block[2] & ~0x03) | (s & 0x03);
            break;
        }
        case 3: {
            block[2] = (block[2] & ~0xfc) | (value << 2);
            break;
        }
    }
}

/** Set the actor on the bottom layer at a position. */
static void set_bottom_tile(grid_pos_t x, grid_pos_t y, tile_t tile) {
    set_tile_in_tile_block(x, y, tile, tworld.bottom_layer);
}

/** Set the actor on the top layer at a position. */
static void set_top_tile(grid_pos_t x, grid_pos_t y, actor_t tile) {
    set_tile_in_tile_block(x, y, tile, tworld.top_layer);
}

static bool has_water_boots(void) {
    return (tworld.boots & BOOTS_WATER) != 0;
}

static bool has_fire_boots(void) {
    return (tworld.boots & BOOTS_FIRE) != 0;
}

static bool has_ice_boots(void) {
    return (tworld.boots & BOOTS_ICE) != 0;
}

static bool has_slide_boots(void) {
    return (tworld.boots & BOOTS_SLIDE) != 0;
}

static active_actor_t act_actor_with_pos(grid_pos_t x, grid_pos_t y) {
    return x | (uint16_t) (y << 7);
}

static active_actor_t act_actor_create(grid_pos_t x, grid_pos_t y,
                                       step_t step, actor_state_t state) {
    return x | state | (uint16_t) (y << 7) | (uint16_t) ((step + STEP_BIAS) << 12);
}

static grid_pos_t act_actor_get_x(active_actor_t a) {
    return a & 0x1f;
}

static grid_pos_t act_actor_get_y(active_actor_t a) {
    return (a >> 7) & 0x1f;
}

static step_t act_actor_get_step(active_actor_t a) {
    return (step_t) ((step_t) (a >> 12) - STEP_BIAS);
}

static actor_state_t act_actor_get_state(active_actor_t a) {
    return (uint8_t) a & ACTOR_STATE_MASK;
}

/**
 * Create a 'moving actor' container for the actor at an index in the actor list.
 * Any changes to this container must be persisted through `destroy_moving_actor`.
 */
static void create_moving_actor(moving_actor_t* mact, actor_idx_t idx) {
    active_actor_t act = tworld.actors[idx];
    mact->index = idx;
    mact->x = act_actor_get_x(act);
    mact->y = act_actor_get_y(act);
    mact->step = act_actor_get_step(act);
    mact->state = act_actor_get_state(act);
    actor_t tile = get_top_tile(mact->x, mact->y);
    mact->entity = actor_get_entity(tile);
    mact->direction = actor_get_direction(tile);
}

/**
 * Persist any changes to a moving actor container to the actor list and the top layer.
 */
static void destroy_moving_actor(const moving_actor_t* mact) {
    tworld.actors[mact->index] = act_actor_create(
            mact->x, mact->y, mact->step, mact->state & ACTOR_STATE_MASK);
    actor_t tile = ACTOR_NONE;
    if (mact->state == ACTOR_STATE_DIED) {
        tile = ACTOR_ANIMATION;
    } else if (mact->state != ACTOR_STATE_HIDDEN) {
        tile = actor_create(mact->entity, mact->direction);
    }
    set_top_tile(mact->x, mact->y, tile);
}

/**
 * Returns the position taken by the moving actor when moved in a direction.
 */
static sposition_t get_new_actor_position(const moving_actor_t* mact, direction_t dir) {
    switch (dir) {
        case DIR_NORTH:
            return (sposition_t) {.x = (int8_t) mact->x, .y = (int8_t) (mact->y - 1)};
        case DIR_WEST:
            return (sposition_t) {.x = (int8_t) (mact->x + 1), .y = (int8_t) mact->y};
        case DIR_SOUTH:
            return (sposition_t) {.x = (int8_t) mact->x, .y = (int8_t) (mact->y + 1)};
        case DIR_EAST:
        default:
            return (sposition_t) {.x = (int8_t) (mact->x - 1), .y = (int8_t) mact->y};
    }
}

/**
 * Creates a moving actor container for the actor at a position.
 * Returns true, if an actor was found at the position.
 * Any changes must be persisted through `destroy_moving_actor`.
 * Animated actors may be included in the search or not.
 */
static bool lookup_actor(moving_actor_t* mact, grid_pos_t x, grid_pos_t y, bool include_animated) {
    for (actor_idx_t i = 0; i < tworld.actors_size; ++i) {
        active_actor_t act = tworld.actors[i];
        if (act_actor_get_x(act) == x && act_actor_get_y(act) == y) {
            if (act_actor_get_state(act) != ACTOR_STATE_HIDDEN ||
                (include_animated && act_actor_get_step(act) != 0)) {
                create_moving_actor(mact, i);
                return true;
            }
        }
    }
    // no actor was found.
    return false;
}

/**
 * Returns the index for a new actor, or returns `ACTOR_INDEX_NONE` if the actor list is full.
 * The active actor at that index must be set afterwards.
 */
static actor_idx_t spawn_actor(void) {
    // Reuse a hidden (dead) actor in the list if possible
    uint8_t count = tworld.actors_size;
    for (actor_idx_t i = 0; i < count; ++i) {
        active_actor_t act = tworld.actors[i];
        if (act_actor_get_state(act) == ACTOR_STATE_HIDDEN && act_actor_get_step(act) == 0) {
            return i;
        }
    }

    if (count >= MAX_ACTORS_COUNT) {
        // Can't create a new actor, list is full! Levels should be made so that this never happens.
#ifdef RUNTIME_CHECKS
        trace("can't spawn actor, actor list is full");
#endif
        return false;
    }

    // Add a new actor at the end of the list.
    tworld.actors[count] = ACTOR_STATE_HIDDEN;
    tworld.actors_size = count + 1;
    return count;
}

/**
 * Find the link for the button at a position.
 * Returns a pointer to the link or 0 if the button isn't linked.
 */
static const link_t* find_link_to(grid_pos_t x, grid_pos_t y, const links_t* links) {
    for (uint8_t i = 0; i < links->size; ++i) {
        const link_t* link = &links->links[i];
        if (link->btn_x == x && link->btn_y == y) {
            return link;
        }
    }
    return 0;
}

/**
 * Return the direction to slide to for a slide floor tile.
 * Can optionally advance the direction if slide is a random force floor (otherwise peek only).
 */
static direction_t get_slide_direction(tile_t tile, bool advance) {
    if (tile == TILE_FORCE_FLOOR_RANDOM) {
        if (advance) {
            tworld.random_slide_dir = direction_right(tworld.random_slide_dir);
        }
        return tworld.random_slide_dir;
    }
    return tile_get_variant(tile);
}

/**
 * Build actor list in reading order (monster list is not used).
 * Exclude all actors on static tiles, but don't exchange chip with the first actor
 * if the first actor was made static by the conversion process.
 */
static void build_actor_list(void) {
    actor_idx_t chip_index = ACTOR_INDEX_NONE;
    uint8_t count = 0;
    for (grid_pos_t y = 0; y < GRID_WIDTH; ++y) {
        for (grid_pos_t x = 0; x < GRID_WIDTH; ++x) {
            actor_t actor = actor_get_entity(get_top_tile(x, y));
            if (actor_get_entity(actor) == ENTITY_CHIP) {
                chip_index = count;
            } else if (!actor_is_on_actor_list(actor) || tile_is_static(get_bottom_tile(x, y))) {
                continue;
            }
            tworld.actors[count++] = act_actor_with_pos(x, y);
        }
    }
    tworld.actors_size = count;

#ifdef RUNTIME_CHECKS
    if (chip_index == ACTOR_INDEX_NONE) {
        trace("no chip tile found in level");
    }
    if (count > MAX_ACTORS_COUNT) {
        trace("too many actors (%d)", count);
    }
#endif

    // If needed, swap chip with first actor on the list.
    if (chip_index > 0) {
        uint16_t temp = tworld.actors[0];
        tworld.actors[0] = tworld.actors[chip_index];
        tworld.actors[chip_index] = temp;
    }
}

// ===================================

void tworld_init(void) {
    // Most fields are zero-initialized
    memset(&tworld.zero_init_start, 0,
           sizeof tworld - (tworld.zero_init_start - (uint8_t*) &tworld));

    if (tworld.time_limit == 0) {
        tworld.flags = FLAG_NO_TIME_LIMIT;
    }
    tworld.collided_with = ACTOR_INDEX_NONE;
    tworld.actor_springing_trap = ACTOR_INDEX_NONE;

    build_actor_list();
}

void tworld_update(uint8_t input_state) {
    // TODO
}

bool tworld_is_game_over(void) {
    return tworld.end_cause != END_CAUSE_NONE;
}

position_t tworld_get_current_position(void) {
    active_actor_t chip = tworld.actors[0];
    return (position_t) {.x = act_actor_get_x(chip), .y = act_actor_get_y(chip)};
}

tile_t tworld_get_bottom_tile(grid_pos_t x, grid_pos_t y) {
    return get_bottom_tile(x, y);
}

actor_t tworld_get_top_tile(grid_pos_t x, grid_pos_t y) {
    return get_top_tile(x, y);
}

void tworld_set_current_position(position_t pos, actor_t actor) {
    active_actor_t chip = tworld.actors[0];
    set_top_tile(act_actor_get_x(chip), act_actor_get_y(chip), ACTOR_NONE);
    tworld.actors[0] = act_actor_create(pos.x, pos.y, 0, ACTOR_STATE_NONE);
    set_top_tile(pos.x, pos.y, actor);
}