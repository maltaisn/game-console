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

#include <core/trace.h>
#include <core/random.h>
#include <core/time.h>

#include <string.h>

#ifdef RUNTIME_CHECKS
#define tworld_error(...) do{trace(__VA_ARGS__); tworld.error = true;} while (0)
#define tworld_assert(cond, ...) do{if (!(cond)) tworld_error(__VA_ARGS__);} while (0)
#else
#define tworld_error(...)
#define tworld_assert(cond, ...)
#endif //RUNTIME_CHECKS

#define CHIP_REST_DIRECTION DIR_SOUTH
// Number of game ticks before Chip moves to rest position.
#define CHIP_REST_TICKS 15

enum {
    // Indicates toggle floor/wall (=0x1, this is important).
    FLAG_TOGGLE_STATE = 1 << 0,
    // Indicates that there may be "reverse tanks" on the grid.
    FLAG_TURN_TANKS = 1 << 1,
    // Indicates that Chip has moved by himself.
    FLAG_CHIP_SELF_MOVED = 1 << 2,
    // Indicates that Chip move has been forced.
    FLAG_CHIP_FORCE_MOVED = 1 << 3,
    // Indicates that Chip can override force floor direction.
    FLAG_CHIP_CAN_UNSLIDE = 1 << 4,
    // Indicates that Chip is stuck on a teleporter.
    FLAG_CHIP_STUCK = 1 << 5,
    // Indicates that inventory is currently shown.
    FLAG_INVENTORY_SHOWN = 1 << 6,
    // Indicates that the level is untimed.
    FLAG_NO_TIME_LIMIT = 1 << 7,
};

// Temporary extra state used to indicate that the actor has died and it's tile
// Should be replaced by an animation tile.
// Note: (ACTOR_STATE_DIED & ACTOR_STATE_MASK) == ACTOR_STATE_HIDDEN
#define ACTOR_STATE_DIED ((actor_state_t) (ACTOR_STATE_HIDDEN + 1))

// Temporary extra state used to indicate a ghost block to be removed from the actor list.
// unlike STATE_DIED, the actor is not replaced by an animation.
// Note: (ACTOR_STATE_GHOST & ACTOR_STATE_MASK) == ACTOR_STATE_HIDDEN
#define ACTOR_STATE_GHOST ((actor_state_t) (ACTOR_STATE_HIDDEN + 2))

/**
 * Container used during step processing to store information about an actor for fast access.
 * There is always one or two instances of this container so size is not an issue.
 */
typedef struct {
    actor_idx_t index;
    position_t pos;
    step_t step;
    actor_state_t state;
    entity_t entity;
    direction_t direction;
} moving_actor_t;

SHARED_DISP_BUF links_t trap_links;
SHARED_DISP_BUF links_t cloner_links;

static direction_mask_t THIN_WALL_DIR_FROM[] = {
        DIR_NORTH_MASK,  // thin wall north
        DIR_WEST_MASK,  // thin wall west
        DIR_SOUTH_MASK,  // thin wall south
        DIR_EAST_MASK,  // thin wall east
        DIR_SOUTHEAST_MASK,  // thin wall south east
};

static direction_mask_t ICE_WALL_DIR_FROM[] = {
        DIR_NORTHWEST_MASK,  // ice wall north west
        DIR_SOUTHWEST_MASK,  // ice wall south west
        DIR_SOUTHEAST_MASK,  // ice wall south east
        DIR_NORTHEAST_MASK,  // ice wall north east
};

static direction_mask_t THIN_WALL_DIR_TO[] = {
        DIR_SOUTH_MASK,  // thin wall north
        DIR_EAST_MASK,  // thin wall west
        DIR_NORTH_MASK,  // thin wall south
        DIR_WEST_MASK,  // thin wall east
        DIR_NORTHWEST_MASK,  // thin wall south east
};

static direction_mask_t ICE_WALL_DIR_TO[] = {
        DIR_SOUTHEAST_MASK,  // ice wall north west
        DIR_NORTHEAST_MASK,  // ice wall south west
        DIR_NORTHWEST_MASK,  // ice wall south east
        DIR_SOUTHWEST_MASK,  // ice wall north east
};

#define DIR_NONE 0xff

// Turn direction as function of ice wall type and incoming direction.
static direction_t ICE_WALL_TURN[] = {
        // north, west, south, east
        DIR_EAST, DIR_SOUTH, DIR_NONE, DIR_NONE,  // ice wall north west
        DIR_NONE, DIR_NORTH, DIR_EAST, DIR_NONE,  // ice wall south west
        DIR_NONE, DIR_NONE, DIR_WEST, DIR_NORTH,  // ice wall south east
        DIR_WEST, DIR_NONE, DIR_NONE, DIR_SOUTH,  // ice wall north east
};

// Return value for `start_movement` and `perform_move`.
typedef enum {
    // Failed to move, still alive
    MOVE_RESULT_FAIL,
    // Move successfully or remained stationary successfully
    MOVE_RESULT_SUCCESS,
    // Died as result of move
    MOVE_RESULT_DIED,
} move_result_t;

// flags used by `can_move` (CM).
enum {
    // was called as part of `start_movement`.
    CM_START_MOVEMENT = 1 << 0,
    // blocks should be pushed (i.e. change the direction but not position)
    CM_PUSH_BLOCKS = 1 << 1,
    // blocks should be moved
    CM_PUSH_BLOCKS_NOW = 1 << 2,
    // was called as result of trap or cloner release
    CM_RELEASING = 1 << 3,
    // animation on target tile should be cleared
    CM_CLEAR_ANIM = 1 << 4,
    // push & move blocks
    CM_PUSH_BLOCKS_ALL = CM_PUSH_BLOCKS | CM_PUSH_BLOCKS_NOW
};

#define chip_actor() tworld.actors[0]

#define CHIP_NEW_POS_NONE 0xff

// walker and blob turn is lazy evaluated to avoid changing PRNG state
#define WALKER_TURN 0xfe
#define BLOB_TURN 0xfd

// ============== Testing functions ====================

#ifdef TESTING
#define stepping() (tworld.stepping)
#else
#define stepping() 0
#endif //TEST

// ============== All utility functions ====================

// there are 4 tiles per block of 3 bytes in the layer data arrays
#define TILES_PER_BLOCK 4

#define nibble_swap(x) ((uint8_t) ((x) >> 4 | (x) << 4))

static AVR_OPTIMIZE uint8_t get_tile_in_tile_block(const position_t pos, const uint8_t* layer) {
    // note: this function was hand optimized to produce the best assembly output,
    // since it may be called a several thousand times per second.
    // execution time starting from get_x_tile: between 35 and 42 cycles.
    const uint8_t* block = &layer[(uint8_t) ((pos.y << 3) + (pos.x >> 2)) * 3];
    switch (pos.x % TILES_PER_BLOCK) {
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
static tile_t get_bottom_tile(const position_t pos) {
    return get_tile_in_tile_block(pos, tworld.bottom_layer);
}

/** Returns the actor on the top layer at a position. */
static actor_t get_top_tile(const position_t pos) {
    return get_tile_in_tile_block(pos, tworld.top_layer);
}

static AVR_OPTIMIZE void set_tile_in_tile_block(
        const position_t pos, const uint8_t value, uint8_t* layer) {
    // execution time starting from set_x_tile: between 38 and 48 cycles.
    uint8_t* block = &layer[(uint8_t) ((pos.y << 3) + (pos.x >> 2)) * 3];
    switch (pos.x % TILES_PER_BLOCK) {
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
            // lsl, lsl, andi, or
            block[2] = (block[2] & ~0xfc) | (value << 2);
            break;
        }
    }
}

/** Set the actor on the bottom layer at a position. */
static void set_bottom_tile(const position_t pos, const tile_t tile) {
    set_tile_in_tile_block(pos, tile, tworld.bottom_layer);
}

/** Set the actor on the top layer at a position. */
static void set_top_tile(const position_t pos, const actor_t tile) {
    set_tile_in_tile_block(pos, tile, tworld.top_layer);
}

static bool has_water_boots(void) {
    return (tworld.boots & BOOT_MASK_WATER) != 0;
}

static bool has_fire_boots(void) {
    return (tworld.boots & BOOT_MASK_FIRE) != 0;
}

static bool has_ice_boots(void) {
    return (tworld.boots & BOOT_MASK_ICE) != 0;
}

static bool has_slide_boots(void) {
    return (tworld.boots & BOOT_MASK_SLIDE) != 0;
}

static void receive_boots(uint8_t variant) {
    // reuse direction mask lookup table instead of 1 << variant.
    tworld_assert(variant < 4);
    tworld.boots |= direction_to_mask(variant);
}

static bool position_equals(position_t a, position_t b) {
    return a.x == b.x && a.y == b.y;
}

static active_actor_t act_actor_create(position_t pos, step_t step, actor_state_t state) {
    tworld_assert(pos.x < GRID_WIDTH);
    tworld_assert(pos.y < GRID_HEIGHT);
    tworld_assert(step >= -3 && step <= 12);
    tworld_assert((state & ~ACTOR_STATE_MASK) == 0);

    return pos.x | state | (uint16_t) (pos.y << 7) |
           (uint16_t) ((uint8_t) (step + STEP_BIAS) << 12);
}

static bool act_actor_is_at_pos(active_actor_t a, position_t pos) {
    return act_actor_get_x(a) == pos.x && act_actor_get_y(a) == pos.y;
}

static active_actor_t act_actor_set_step(active_actor_t a, step_t step) {
    tworld_assert(step >= -3 && step <= 12);
    return (a & 0x0fff) | (uint8_t) (step + STEP_BIAS) << 12;
}

static active_actor_t act_actor_set_state(active_actor_t a, actor_state_t state) {
    tworld_assert((state & ~ACTOR_STATE_MASK) == 0);
    return (a & ~ACTOR_STATE_MASK) | state;
}

/**
 * Create a 'moving actor' container for the actor at an index in the actor list.
 * Any changes to this container must be persisted through `destroy_moving_actor`.
 */
static void create_moving_actor(moving_actor_t* mact, const actor_idx_t idx) {
    const active_actor_t act = tworld.actors[idx];
    mact->index = idx;
    mact->pos = act_actor_get_pos(act);
    mact->step = act_actor_get_step(act);
    mact->state = act_actor_get_state(act);
    const actor_t tile = get_top_tile(mact->pos);
    mact->entity = actor_get_entity(tile);
    mact->direction = actor_get_direction(tile);
}

/**
 * Persist any changes to a moving actor container to the actor list and the top layer.
 */
static void destroy_moving_actor(const moving_actor_t* mact) {
    // ACTOR_STATE_DIED and ACTOR_STATE_GHOST become ACTOR_STATE_HIDDEN after masking.
    tworld.actors[mact->index] = act_actor_create(
            mact->pos, mact->step, mact->state & ACTOR_STATE_MASK);

    actor_t tile = ACTOR_NONE;
    if (mact->state == ACTOR_STATE_DIED) {
        tile = ACTOR_ANIMATION;
    } else if (mact->state != ACTOR_STATE_HIDDEN) {
        tile = actor_create(mact->entity, mact->direction);
    }
    set_top_tile(mact->pos, tile);
}

/**
 * Returns the position taken by the moving actor when moved in a direction.
 */
static sposition_t get_new_actor_position(const moving_actor_t* mact, const direction_t dir) {
    tworld_assert(dir >= DIR_NORTH && dir <= DIR_EAST);

    sposition_t pos;
    pos.x = (int8_t) mact->pos.x;
    pos.y = (int8_t) mact->pos.y;
    switch (dir) {
        case DIR_NORTH:
            --pos.y;
            break;
        case DIR_WEST:
            --pos.x;
            break;
        case DIR_SOUTH:
            ++pos.y;
            break;
        case DIR_EAST:
        default:
            ++pos.x;
            break;
    }
    return pos;
}

/**
 * Creates a moving actor container for the actor at a position.
 * Returns true, if an actor was found at the position.
 * Any changes must be persisted through `destroy_moving_actor`.
 * Animated actors may be included in the search or not.
 */
static bool lookup_actor(moving_actor_t* mact, const position_t pos, const bool include_animated) {
    for (actor_idx_t i = 0; i < tworld.actors_size; ++i) {
        const active_actor_t act = tworld.actors[i];
        if (act_actor_is_at_pos(act, pos)) {
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
 * Create a new actor if possible. The actor must be destroyed properly afterwards
 * if it's modified, otherwise it's not necessary since it spawns as hidden.
 * Returns true if actor was successfully spawned, false if maximum actors has been reached.
 */
static bool spawn_actor(moving_actor_t* mact) {
    // Reuse a hidden (dead) actor in the list if possible
    const uint8_t count = tworld.actors_size;
    for (actor_idx_t i = 0; i < count; ++i) {
        const active_actor_t act = tworld.actors[i];
        if (act_actor_get_state(act) == ACTOR_STATE_HIDDEN && act_actor_get_step(act) == 0) {
            create_moving_actor(mact, i);
            return true;
        }
    }

    // Can't create a new actor, list is full! Levels should be made so that this never happens,
    // or so that the level can be completed normally despite this limitation.
    if (count >= MAX_ACTORS_COUNT) {
        trace("can't spawn actor, actor list is full");
        return false;
    }

    // Add a new actor at the end of the list.
    tworld.actors[count] = act_actor_create((position_t) {0, 0}, 0, ACTOR_STATE_HIDDEN);
    tworld.actors_size = count + 1;
    create_moving_actor(mact, count);
    return true;
}

/**
 * Find the link for the button at a position.
 * Returns a pointer to the link or 0 if the button isn't linked.
 */
static const link_t* find_link_to(const position_t pos, const links_t* links) {
    for (uint8_t i = 0; i < links->size; ++i) {
        const link_t* link = &links->links[i];
        if (link->btn.x == pos.x && link->btn.y == pos.y) {
            return link;
        }
    }
    return 0;
}

/**
 * Return the direction to slide to for a slide floor tile.
 * Can optionally advance the direction if slide is a random force floor (otherwise peek only).
 */
static direction_t get_slide_direction(const tile_t tile, const bool advance) {
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
 * Exclude all actors on static tiles, but don't exchange Chip with the first actor
 * if the first actor was made static by the conversion process.
 */
static void build_actor_list(void) {
    actor_idx_t chip_index = ACTOR_INDEX_NONE;
    uint8_t count = 0;
    for (grid_pos_t y = 0; y < GRID_WIDTH; ++y) {
        for (grid_pos_t x = 0; x < GRID_WIDTH; ++x) {
            const position_t pos = {x, y};
            const actor_t actor = get_top_tile(pos);
            if (actor_get_entity(actor) == ENTITY_CHIP) {
                chip_index = count;
            } else if (!actor_is_on_actor_list(actor) || tile_is_static(get_bottom_tile(pos))) {
                continue;
            }
            tworld.actors[count++] = act_actor_create(pos, 0, ACTOR_STATE_NONE);
#ifdef RUNTIME_CHECKS
            if (count == MAX_ACTORS_COUNT) {
                tworld_error("too many actors in level");
                return;
            }
#endif //RUNTIME_CHECKS
        }
    }
    tworld.actors_size = count;

#ifdef RUNTIME_CHECKS
    if (chip_index == ACTOR_INDEX_NONE) {
        tworld_error("no chip tile found in level");
        return;
    }
#endif //RUNTIME_CHECKS

    // If needed, swap Chip with first actor on the list.
    if (chip_index > 0) {
        const uint16_t temp = chip_actor();
        chip_actor() = tworld.actors[chip_index];
        tworld.actors[chip_index] = temp;
    }
}

static direction_t pick_walker_direction(const direction_t curr_dir) {
#ifdef TESTING
    // PRNG used in original Tile World, for testing.
    uint8_t n = ((tworld.prng_value1 >> 2) - tworld.prng_value1) & 0xff;
    if ((tworld.prng_value1 & 0x02) == 0) {
        --n;
    }
    tworld.prng_value1 = ((tworld.prng_value1 >> 1) | (tworld.prng_value2 & 0x80)) & 0xff;
    tworld.prng_value2 = ((tworld.prng_value2 << 1) | (n & 0x01)) & 0xff;
    uint8_t value = tworld.prng_value1 ^ tworld.prng_value2;
    return (uint8_t) (curr_dir - (value & 0x3)) % 4;
#else
    return random8() & 0x3;
#endif //TESTING
}

static direction_t pick_blob_direction(void) {
#ifdef TESTING
    // PRNG used in original Tile World, for testing.
    tworld.prng_value0 = ((tworld.prng_value0 * 1103515245UL) + 12345UL) & 0x7fffffffUL;
    const direction_t cw[] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
    return cw[tworld.prng_value0 >> 29];
#else
    return random8() & 0x3;
#endif //TESTING
}

// ============== State update functions ====================

static bool can_move(const moving_actor_t* act, direction_t direction, uint8_t flags);

static move_result_t perform_move(moving_actor_t* act, uint8_t flags);

/**
 * Clear the animation on the top tile at a position, if any.
 * The actor at the given position should be `ACTOR_ANIMATION`.
 */
static void stop_death_animation(const position_t pos) {
    for (actor_idx_t i = 0; i < tworld.actors_size; ++i) {
        const active_actor_t act = tworld.actors[i];
        if (act_actor_is_at_pos(act, pos)) {
            tworld.actors[i] = act_actor_set_step(act, 0);
            return;
        }
    }
}

/**
 * If actor is currently on an ice wall, turn it in the direction forced by the ice wall.
 */
static void apply_ice_wall_turn(moving_actor_t* act) {
    const tile_t tile = get_bottom_tile(act->pos);
    if (tile_is_ice_wall(tile)) {
        const uint8_t idx = act->direction + (uint8_t) (tile_get_variant(tile) * 4);
        const direction_t new_dir = ICE_WALL_TURN[idx];
        if (new_dir != DIR_NONE) {
            act->direction = new_dir;
        }
    }
}

/**
 * Returns true if the block is allowed to be pushed in the given direction.
 * If flags include `CM_PUSH_BLOCKS`, block direction is changed.
 * If flags include `CM_PUSH_BLOCKS_NOW`, block is moved.
 */
static bool can_push_block(moving_actor_t* block, const direction_t direction,
                           const uint8_t flags) {
    bool can_push = true;
    bool changed = false;
    if (!can_move(block, direction, flags)) {
        // Only change block direction (but only if block wasn't force moved on this step).
        can_push = false;
        if (block->step == 0 && (flags & CM_PUSH_BLOCKS_ALL) && block->state != ACTOR_STATE_MOVED) {
            block->direction = direction;
            changed = true;
        }
    } else if (flags & CM_PUSH_BLOCKS_ALL) {
        // Change block direction and move it.
        block->direction = direction;
        block->state = ACTOR_STATE_MOVED;
        if (flags & CM_PUSH_BLOCKS_NOW) {
            perform_move(block, 0);
        }
        changed = true;
        can_push = true;
        if (block->index == tworld.actor_springing_trap) {
            // Block on trap button was pushed off, so no one is springing it now.
            tworld.actor_springing_trap = ACTOR_INDEX_NONE;
        }
    }
    if (changed) {
        destroy_moving_actor(block);
    }
    return can_push;
}

/**
 * Returns true if the actor is allowed to move in the given direction.
 * Flags can be set to set the context from which the move is performed.
 */
static bool can_move(const moving_actor_t* act, const direction_t direction,
                     const uint8_t flags) {
    const sposition_t spos = get_new_actor_position(act, direction);
    if (spos.x < 0 || spos.x >= GRID_WIDTH || spos.y < 0 || spos.y >= GRID_HEIGHT) {
        // cannot exit map borders
        return false;
    }

    const position_t pos = {spos.x, spos.y};
    const tile_t tile_from = get_bottom_tile(act->pos);
    const tile_t tile_to = get_bottom_tile(pos);

    if ((tile_from == TILE_TRAP || tile_from == TILE_CLONER) && !(flags & CM_RELEASING)) {
        return false;
    }
    if (tile_from == TILE_STATIC_TRAP) {
        return false;
    }

    if (tile_is_toggle_tile(tile_to) &&
        tile_with_toggle_state(tile_to, tworld.flags & FLAG_TOGGLE_STATE) == TILE_TOGGLE_WALL) {
        // Since toggle state can change multiple time per step, only a flag is changed
        // instead of the whole grid for consistent execution time.
        return false;
    }

    if (tile_is_slide(tile_from) && (act->entity != ENTITY_CHIP || !has_slide_boots()) &&
        get_slide_direction(tile_from, false) == direction_back(direction)) {
        // Can't move back on slide floor when overriding forced movement
        return false;
    }

    // Thin wall / ice corner directional handling
    uint8_t thin_wall = 0;
    if (tile_is_thin_wall(tile_from)) {
        thin_wall |= THIN_WALL_DIR_FROM[tile_from - TILE_THIN_WALL_N];
    } else if (tile_is_ice_wall(tile_from)) {
        thin_wall |= ICE_WALL_DIR_FROM[tile_from - TILE_ICE_CORNER_NW];
    }
    if (tile_is_thin_wall(tile_to)) {
        thin_wall |= THIN_WALL_DIR_TO[tile_to - TILE_THIN_WALL_N];
    } else if (tile_is_ice_wall(tile_to)) {
        thin_wall |= ICE_WALL_DIR_TO[tile_to - TILE_ICE_CORNER_NW];
    }
    if (thin_wall & direction_to_mask(direction)) {
        return false;
    }

    if (act->entity == ENTITY_CHIP) {
        if (tile_is_chip_acting_wall(tile_to) && !tile_is_revealable_wall(tile_to)) {
            return false;
        }
        if (tile_to == TILE_SOCKET && tworld.chips_left > 0) {
            return false;
        }
        if (tile_is_lock(tile_to) && tworld.keys[tile_get_variant(tile_to)] == 0) {
            return false;
        }

        // Check if there's another actor on destination tile
        moving_actor_t other;
        bool is_other = lookup_actor(&other, pos, true);

        if (!is_other && actor_get_entity(get_top_tile(pos)) == ENTITY_BLOCK_GHOST) {
            // No actor there but there is a ghost block. Add it to the actor list if possible.
            // Levels should be made so that a ghost block can always be created, otherwise
            // it won't be spawned and won't be moved!
            moving_actor_t new_block;
            if (spawn_actor(&new_block)) {
                new_block.entity = ENTITY_BLOCK_GHOST;
                new_block.pos = pos;
                new_block.state = ACTOR_STATE_NONE;
                other = new_block;
                is_other = true;
            }
        }

        if (is_other) {
            if (other.state == ACTOR_STATE_HIDDEN) {
                if (other.step > 0) {
                    // "animated" actors block Chip
                    return false;
                }
            } else if (actor_is_block(other.entity)) {
                if (!can_push_block(&other, direction, flags & ~CM_RELEASING)) {
                    if (other.entity == ENTITY_BLOCK_GHOST) {
                        // Ghost block just created can't be moved: hide it immediately.
                        tworld.actors[other.index] = act_actor_set_state(
                                tworld.actors[other.index], ACTOR_STATE_HIDDEN);
                    }
                    return false;
                }
            }
        }
        // static blocks are always put on a wall so they are no concern here

        if (tile_is_revealable_wall(tile_to)) {
            if (flags & CM_START_MOVEMENT) {
                // Reveal hidden wall or blue wall
                set_bottom_tile(pos, TILE_WALL);
            }
            return false;
        }

        if (tworld.flags & FLAG_CHIP_STUCK) {
            // Chip is stuck on teleporter forever
            return false;
        }
        return true;

    } else if (actor_is_block(act->entity)) {
        if (act->step > 0) {
            // Block cannot move while in-between moves (when called from _can_push_block).
            return false;
        }
        if (tile_is_block_acting_wall(tile_to)) {
            return false;
        }

    } else {
        if (tile_is_monster_acting_wall(tile_to)) {
            return false;
        }
        if (tile_to == TILE_FIRE && act->entity != ENTITY_FIREBALL) {
            // Fire is treated as a wall by all except fireball
            return false;
        }
    }

    const actor_t other = get_top_tile(pos);
    if (actor_is_monster_or_block(other)) {
        // There's already a monster or a block there (location claimed)
        return false;
    }
    if ((flags & CM_CLEAR_ANIM) && other == ACTOR_ANIMATION) {
        stop_death_animation(pos);
    }

    return true;
}

/**
 * If an actor is forced to move in a direction, apply that direction.
 * The actor state field must not have been reset since last move when this is called!
 * The teleported flag indicates that the actor was teleported on the previous step.
 */
static void apply_forced_move(moving_actor_t* act, const bool teleported) {
    if (tworld.current_time == 0) {
        return;
    }

    const tile_t tile = get_bottom_tile(act->pos);
    const bool is_chip = (act->entity == ENTITY_CHIP);
    if (tile_is_ice(tile)) {
        if (is_chip && has_ice_boots()) {
            return;
        }
        // Continue in same direction

    } else if (tile_is_slide(tile)) {
        if (is_chip && has_slide_boots()) {
            return;
        }
        // Take direction of force floor
        act->direction = get_slide_direction(tile, true);

    } else if (!teleported) {
        // If teleported, continue in same direction
        // In other cases, move is not forced.
        return;
    }

    if (is_chip) {
        tworld.flags |= FLAG_CHIP_FORCE_MOVED;
    }
    act->state = ACTOR_STATE_MOVED;
}

/**
 * Choose a move for Chip given the current input state.
 */
static void choose_chip_move(moving_actor_t* act) {
    direction_mask_t state = tworld.input_state | tworld.input_since_move;
    tworld.input_since_move = 0;

    tworld_assert(!((state & DIR_VERTICAL_MASK) == DIR_VERTICAL_MASK ||
                    (state & DIR_HORIZONTAL_MASK) == DIR_HORIZONTAL_MASK), "bad direction mask");

    if (state == 0) {
        // No keys pressed
        return;
    }

    if ((tworld.flags & FLAG_CHIP_FORCE_MOVED) && !(tworld.flags & FLAG_CHIP_CAN_UNSLIDE)) {
        // Chip was forced moved and is not allowed to "unslide", skip free choice.
        return;
    }

    if ((state & DIR_VERTICAL_MASK) != 0 && (state & DIR_HORIZONTAL_MASK) != 0) {
        // Direction is diagonal
        const direction_t last_dir = tworld.last_chip_dir;
        const direction_mask_t last_dir_mask = direction_to_mask(last_dir);
        if (state & last_dir_mask) {
            // One of the direction is the current one: continue in current direction, and
            // change direction only if current direction is blocked and other is not.
            const direction_t other_dir = direction_from_mask(last_dir_mask ^ state);
            const bool can_move_curr = can_move(act, last_dir, CM_PUSH_BLOCKS);
            const bool can_move_other = can_move(act, other_dir, CM_PUSH_BLOCKS);
            act->direction = (!can_move_curr && can_move_other) ? other_dir : last_dir;
        } else {
            // Neither direction is the current direction: prioritize horizontal movement first.
            if (can_move(act, direction_from_mask(state & DIR_HORIZONTAL_MASK), CM_PUSH_BLOCKS)) {
                state &= DIR_HORIZONTAL_MASK;
            } else {
                state &= DIR_VERTICAL_MASK;
            }
            act->direction = direction_from_mask(state);
        }
    } else {
        // Single direction, apply it.
        act->direction = direction_from_mask(state);
        // Make unused check since it can have side effects (pushing blocks)
        can_move(act, act->direction, CM_PUSH_BLOCKS);
    }

    tworld.flags |= FLAG_CHIP_SELF_MOVED;
    act->state = ACTOR_STATE_MOVED;
}

/**
 * Choose a direction for a monster actor.
 */
static void choose_monster_move(moving_actor_t* act) {
    if (act->state == ACTOR_STATE_MOVED) {
        // Monster was force moved, do not override;
        return;
    }

    const tile_t tile = get_bottom_tile(act->pos);
    if (tile == TILE_CLONER || tile == TILE_TRAP) {
        return;
    }

    direction_t choices[4] = {DIR_NONE, DIR_NONE, DIR_NONE, DIR_NONE};

    const direction_t forward = act->direction;
    if (act->entity == ENTITY_TEETH) {
        // Go towards Chip
        if ((tworld.current_time + stepping()) & 0x4) {
            // Teeth only move at half speed, don't move this time
            return;
        }
        const position_t pos = tworld_get_current_position();
        const int8_t dx = (int8_t) (pos.x - act->pos.x);
        const int8_t dy = (int8_t) (pos.y - act->pos.y);
        if (dx < 0) {
            choices[0] = DIR_WEST;
        } else if (dx > 0) {
            choices[0] = DIR_EAST;
        }
        if (dy < 0) {
            choices[1] = DIR_NORTH;
        } else if (dy > 0) {
            choices[1] = DIR_SOUTH;
        }
        const uint8_t adx = dx < 0 ? -dx : dx;
        const uint8_t ady = dy < 0 ? -dy : dy;
        if (ady >= adx) {
            // Y difference is greater than X difference, give priority to Y move.
            const direction_t temp = choices[0];
            choices[0] = choices[1];
            choices[1] = temp;
        }
        // at this point choices[1] may still be DIR_NONE.
    } else if (act->entity == ENTITY_BLOB) {
        // random direction
        choices[0] = BLOB_TURN;
    } else if (actor_is_tank(act->entity)) {
        choices[0] = forward;
    } else if (act->entity == ENTITY_WALKER) {
        // forward and turn randomly if blocked.
        choices[0] = forward;
        choices[1] = WALKER_TURN;
    } else {
        const direction_t back = direction_back(forward);
        if (act->entity == ENTITY_BALL) {
            choices[0] = forward;
            choices[1] = back;
        } else {
            const direction_t left = direction_left(forward);
            const direction_t right = direction_right(forward);
            if (act->entity == ENTITY_BUG) {
                choices[0] = left;
                choices[1] = forward;
                choices[2] = right;
            } else if (act->entity == ENTITY_PARAMECIUM) {
                choices[0] = right;
                choices[1] = forward;
                choices[2] = left;
            } else if (act->entity == ENTITY_GLIDER) {
                choices[0] = forward;
                choices[1] = left;
                choices[2] = right;
            } else {  // FIREBALL is the only left at this point
                choices[0] = forward;
                choices[1] = right;
                choices[2] = left;
            }
            choices[3] = back;
        }
    }

    // Attempt move choices in order.
    // Even if all directions are blocked, still change direction and indicate actor has moved,
    // in case one direction is freed by another actor moving in the meantime.
    act->state = ACTOR_STATE_MOVED;
    for (uint8_t i = 0; i < 4; ++i) {
        direction_t choice = choices[i];
        if (choice == DIR_NONE) {
            break;
        } else if (choice == WALKER_TURN) {
            choice = pick_walker_direction(forward);
        } else if (choice == BLOB_TURN) {
            choice = pick_blob_direction();
        }
        act->direction = choice;
        if (can_move(act, choice, CM_CLEAR_ANIM)) {
            return;
        }
    }

    if (act->entity == ENTITY_TEETH) {
        // Move failed, but still make teeth face Chip
        act->direction = choices[0];
    }
}

/**
 * Choose a move for an actor. The move is stored by changing the actor's direction.
 * The teleported flag indicates that the actor was teleported on the previous step.
 */
static void choose_move(moving_actor_t* act, const bool teleported) {
    // This will set actor state to MOVED if force move applied.
    apply_forced_move(act, teleported);

    if (act->entity == ENTITY_CHIP) {
        choose_chip_move(act);

        // Last direction assumed by Chip is used to resolve diagonal input correctly.
        tworld.chip_last_dir = act->direction;

        // Save new position for Chip, used later for collision checking.
        tworld.collided_with = ACTOR_INDEX_NONE;
        if (act->state == ACTOR_STATE_MOVED) {
            tworld.ticks_since_move = 0;
            if (!(tworld.flags & FLAG_CHIP_FORCE_MOVED)) {
                // Note: collision case 1 doesn't apply if Chip is subject to a forced move.
                tworld.chip_new_pos = get_new_actor_position(act, act->direction);
            }
        } else {
            // Update rest timer
            if (tworld.ticks_since_move == CHIP_REST_TICKS) {
                act->direction = CHIP_REST_DIRECTION;
            } else if (tworld.ticks_since_move < CHIP_REST_TICKS) {
                ++tworld.ticks_since_move;
            }
        }

    } else if (!actor_is_block(act->entity)) {
        // Choose monster move.
        choose_monster_move(act);

    } else if (act->entity == ENTITY_BLOCK_GHOST) {
        if (act->state == ACTOR_STATE_NONE) {
            // Ghost block hasn't moved, remove it from actor list without removing the tile.
            // Don't touch ghost blocks that have side effect though.
            const tile_t tile = get_bottom_tile(act->pos);
            if (!tile_is_button(tile) && tile != TILE_TRAP) {
                act->state = ACTOR_STATE_GHOST;
            }
        }
    }

    // (blocks never move by themselves)
}

/**
 * Initiate a move by an actor. Flags indicate the context from which the move is performed,
 * for example releasing an actor from a trap/cloner uses the CM_RELEASING flag.
 * Returns a result indicating the outcome of the move initiation.
 */
static move_result_t start_movement(moving_actor_t* act, const uint8_t flags) {
    const tile_t tile_from = get_bottom_tile(act->pos);

    if (act->entity == ENTITY_CHIP) {
        if (!has_slide_boots()) {
            if (tile_is_slide(tile_from) && !(tworld.flags & FLAG_CHIP_SELF_MOVED)) {
                // Chip is on force floor and has not moved by himtworld, award unslide permission.
                tworld.flags |= FLAG_CHIP_CAN_UNSLIDE;
            } else if (!tile_is_ice(tile_from) || has_ice_boots()) {
                // Chip is on non force move slide, reclaim unslide permission.
                tworld.flags &= ~FLAG_CHIP_CAN_UNSLIDE;
            }
        }
        tworld.flags &= ~(FLAG_CHIP_FORCE_MOVED | FLAG_CHIP_SELF_MOVED);
        tworld.last_chip_dir = act->direction;
    }

    if (!can_move(act, act->direction, flags | CM_START_MOVEMENT |
                                       CM_CLEAR_ANIM | CM_PUSH_BLOCKS_NOW)) {
        // Cannot make chosen move: either another actor made the move before,
        // or move is being forced in blocked direction
        if (tile_is_ice(tile_from) && (act->entity != ENTITY_CHIP || !has_ice_boots())) {
            act->direction = direction_back(act->direction);
            apply_ice_wall_turn(act);
        }
        return MOVE_RESULT_FAIL;
    }

    if (tile_from == TILE_CLONER || tile_from == TILE_TRAP) {
        tworld_assert(flags & CM_RELEASING);
    }

    // Check if creature is currently located where Chip intends to move (case 1)
    bool chip_collided = false;
    if (actor_is_monster(act->entity) && (int8_t) act->pos.x == tworld.chip_new_pos.x &&
        (int8_t) act->pos.y == tworld.chip_new_pos.y) {
        // Collision may occur: Chip has moved where a monster was.
        tworld.collided_with = act->index;
        tworld.collided_actor = actor_create(act->entity, act->direction);

    } else if (act->entity == ENTITY_CHIP && tworld.collided_with != ACTOR_INDEX_NONE) {
        const active_actor_t other = tworld.actors[tworld.collided_with];
        if (act_actor_get_state(other) != ACTOR_STATE_HIDDEN) {
            // Collision occured and creature has not died in the meantime.
            // This is a special case since the creature has actually moved by this time,
            // so we need to remove it from the tile where it moved to.
            chip_collided = true;
            set_top_tile(act_actor_get_pos(other), ACTOR_NONE);
        }
    }

    // Check if Chip is moving on a monster (case 2)
    const sposition_t spos = get_new_actor_position(act, act->direction);
    const position_t pos = {spos.x, spos.y};  // conversion is always valid at this point
    if (act->entity == ENTITY_CHIP) {
        const actor_t other = get_top_tile(pos);
        if (actor_get_entity(other) != ENTITY_NONE) {
            chip_collided = true;
            tworld.collided_actor = other;
        }
    }

    // Make move
    if (tile_from != TILE_CLONER) {
        // (leave actor in cloner if releasing)
        set_top_tile(act->pos, ACTOR_NONE);
    }
    act->pos = pos;
    // The new tile in top layer is set later. This is because direction is stored in top layer
    // and direction may be changed without execution reaching this point (e.g. ice wall turn).

    // Check if creature has moved on Chip (case 3)
    if (act->entity != ENTITY_CHIP) {
        const active_actor_t chip = chip_actor();
        const position_t chip_pos = act_actor_get_pos(chip);
        if (position_equals(chip_pos, pos)) {
            chip_collided = true;
            tworld.collided_actor = get_top_tile(chip_pos);
            // If Chip has moved, ignore it. This is important because if death occurs, the
            // rest of the tick is processed as usual, but now the actor that has moved on Chip
            // took its place and may attempt to move again when Chip turn comes.
            // STATE_HIDDEN cannot be used because we want this actor to be shown in collision.
            chip_actor() = act_actor_set_state(chip, ACTOR_STATE_NONE);
        }
    }

    if (chip_collided) {
        if (actor_get_entity(tworld.collided_actor) == ENTITY_BLOCK ||
            act->entity == ENTITY_BLOCK) {
            tworld.end_cause = END_CAUSE_COLLIDED_BLOCK;
        } else {
            tworld.end_cause = END_CAUSE_COLLIDED_MONSTER;
        }
        return MOVE_RESULT_DIED;
    }

    act->step += 8;
    return MOVE_RESULT_SUCCESS;

}

/**
 * Continue an actor's movement. When movement starts, a number of ticks are elapsed
 * before the move ends, during which the move is normally animated, but not here.
 * For this reason and since the top tile is set in _start_movement, there will be a small
 * delay between the move and its side effects (e.g. reaching a key, then picking it up).
 */
static bool continue_movement(moving_actor_t* act) {
    tworld_assert(act->step > 0);

    const tile_t tile = get_bottom_tile(act->pos);

    uint8_t speed = act->entity == ENTITY_BLOB ? 1 : 2;
    // apply x2 multiplifier on sliding tiles
    if ((tile_is_ice(tile) && (act->entity != ENTITY_CHIP || !has_ice_boots())) ||
        (tile_is_slide(tile) && (act->entity != ENTITY_CHIP || !has_slide_boots()))) {
        speed *= 2;
    }

    act->step = (int8_t) (act->step - speed);
    return act->step > 0;
}

/**
 * When a blue button is clicked, all tanks not on ice or a clone machine are marked as
 * "reverse tanks", and will be reversed at the end of this step if a blue button wasn't
 * clicked again in the meantime by another actor. Depending on the actor list order and the
 * presence of ice, there can be some tanks reversed and some not at the end of a step.
 */
static void turn_tanks(moving_actor_t* trigger) {
    tworld.flags |= FLAG_TURN_TANKS;
    if (actor_is_tank(trigger->entity)) {
        // There's a moving actor active for this tank, change it directly
        // it will be destroyed later so changing only the tile it's on would reverse the effect.
        trigger->entity = actor_reverse_tank(trigger->entity);
    }
    for (actor_idx_t i = 0; i < tworld.actors_size; ++i) {
        const active_actor_t actor = tworld.actors[i];
        if (act_actor_get_state(actor) == ACTOR_STATE_HIDDEN) {
            continue;
        }

        const position_t pos = act_actor_get_pos(actor);
        const tile_t top_tile = get_top_tile(pos);
        if (!actor_is_tank(top_tile)) {
            continue;
        }
        const tile_t bot_tile = get_bottom_tile(pos);
        if (bot_tile == TILE_CLONER || tile_is_ice(bot_tile)) {
            continue;
        }

        // Replace tank by reverse tank or inversely
        set_top_tile(pos, actor_with_entity(top_tile, actor_reverse_tank(top_tile)));
    }
}

/**
 * Release actor from cloner controlled by the button at the given position.
 * If maximum number of actors is reached, the parent actor comes out and cloner is empty.
 */
static void activate_cloner(const position_t pos) {
    const link_t* link = find_link_to(pos, &cloner_links);
    if (!link) {
        // Button isn't linked with a cloner.
        return;
    }

    moving_actor_t parent;
    if (!lookup_actor(&parent, link->link, false)) {
        // Cloner is empty.
        return;
    }

    moving_actor_t clone;
    if (!spawn_actor(&clone)) {
        // Max number of actors reached, use parent (cloner becomes empty).
        if (perform_move(&parent, CM_RELEASING) == MOVE_RESULT_SUCCESS) {
            // Parent moved successfully, remove it from cloner and persist it.
            set_top_tile(pos, ACTOR_NONE);
            destroy_moving_actor(&parent);
        }
        return;
    }

    clone.pos = parent.pos;
    clone.step = parent.step;
    clone.state = parent.state;
    clone.entity = parent.entity;
    clone.direction = parent.direction;

    parent.state = ACTOR_STATE_MOVED;
    if (perform_move(&parent, CM_RELEASING) == MOVE_RESULT_SUCCESS) {
        // Parent moved successfully out of cloner, persist it.
        destroy_moving_actor(&parent);
        // Clone takes the place of the parent in cloner, persist it.
        destroy_moving_actor(&clone);
        // If parent move fails, neither is persisted so that parent keeps original position
        // and clone ceases to exist (by virtue of being hidden on spawn)
    }
}

/**
 * Complete the movement for the given actor. Most side effects produced by the move
 * occur at this point. An end cause is returned indicating whether the actor survived,
 * (END_CAUSE_NONE), or otherwise how it died.
 */
static end_cause_t end_movement(moving_actor_t* act) {
    const tile_t tile = get_bottom_tile(act->pos);
    uint8_t variant = tile_get_variant(tile);

    if (act->entity != ENTITY_CHIP || !has_ice_boots()) {
        apply_ice_wall_turn(act);
    }

    tile_t new_tile = tile;  // new bottom tile after movement if it changed
    end_cause_t end_cause = END_CAUSE_NONE;
    if (act->entity == ENTITY_CHIP) {
        if (tile == TILE_WATER) {
            if (!has_water_boots()) {
                end_cause = END_CAUSE_DROWNED;
            }
        } else if (tile == TILE_FIRE) {
            if (!has_fire_boots()) {
                end_cause = END_CAUSE_BURNED;
            }
        } else if (tile == TILE_DIRT || tile == TILE_WALL_BLUE_FAKE || tile == TILE_SOCKET) {
            new_tile = TILE_FLOOR;
        } else if (tile == TILE_RECESSED_WALL) {
            new_tile = TILE_WALL;
        } else if (tile_is_lock(tile)) {
            if (tile != TILE_LOCK_GREEN) {
                --tworld.keys[variant];
            }
            new_tile = TILE_FLOOR;
        } else if (tile_is_key(tile)) {
            if (tworld.keys[variant] < 255) {
                ++tworld.keys[variant];
            }
            new_tile = TILE_FLOOR;
        } else if (tile_is_boots(tile)) {
            receive_boots(variant);
            new_tile = TILE_FLOOR;
        } else if (tile == TILE_THIEF) {
            tworld.boots = 0;
        } else if (tile == TILE_CHIP) {
            if (tworld.chips_left > 0) {
                --tworld.chips_left;
            }
            new_tile = TILE_FLOOR;
        } else if (tile == TILE_EXIT) {
            end_cause = END_CAUSE_COMPLETE;
        }
    } else {
        // block or monster
        if (tile == TILE_WATER) {
            if (actor_is_block(act->entity)) {
                new_tile = TILE_DIRT;
            }
            if (act->entity != ENTITY_GLIDER) {
                end_cause = END_CAUSE_DROWNED;
            }
        } else if (tile == TILE_KEY_BLUE) {
            // monsters and blocks destroy blue keys
            new_tile = TILE_FLOOR;
        }
        // fire is a wall to monsters so they will never end up on it,
        // except fireball and block that survives it
    }

    if (tile == TILE_BOMB) {
        new_tile = TILE_FLOOR;
        end_cause = END_CAUSE_BOMBED;
    } else if (tile == TILE_BUTTON_GREEN) {
        tworld.flags ^= FLAG_TOGGLE_STATE;
    } else if (tile == TILE_BUTTON_BLUE) {
        turn_tanks(act);
    } else if (tile == TILE_BUTTON_RED) {
        activate_cloner(act->pos);
    }

    if (new_tile != tile) {
        set_bottom_tile(act->pos, new_tile);
    }

    return end_cause;
}

/**
 * Release actor from trap controlled by the button at the given position.
 */
static void spring_trap(const position_t pos) {
    const link_t* link = find_link_to(pos, &trap_links);
    if (!link) {
        // Button isn't linked with a trap.
        return;
    }

    moving_actor_t mact;
    if (lookup_actor(&mact, link->link, false)) {
        perform_move(&mact, CM_RELEASING);
        destroy_moving_actor(&mact);
    }
}

/**
 * Teleport an actor on a teleporter to another teleporter, in reverse reading order.
 * If all teleporters are blocked in the actor's direction, the actor becomes stuck.
 * Returns true if teleport is successful.
 */
static void teleport_actor(moving_actor_t* act) {
    if (act->index == 0 && act->entity != ENTITY_CHIP) {
        // Chip tile was destroyed (see below). Restore it.
        act->entity = actor_get_entity(tworld.teleported_chip);
        act->direction = actor_get_direction(tworld.teleported_chip);
    } else {
        // If Chip tile was destroyed then there's two actors on the same position, don't erase
        // the tile because we'll lose information about the other actor.
        // Otherwise erase the tile to unclaim it, since actor is most likely going to move.
        // This is needed so that the current teleporter appears unclaimed later.
        set_top_tile(act->pos, ACTOR_NONE);
    }

    const uint16_t orig_idx = act->pos.x + act->pos.y * GRID_WIDTH;
    uint16_t idx = orig_idx;
    while (true) {
        idx = (uint16_t) (idx - 1) % GRID_SIZE;
        const position_t pos = {idx % GRID_WIDTH, idx / GRID_WIDTH};

        if (get_bottom_tile(pos) == TILE_TELEPORTER) {
            act->pos = pos;
            if (!actor_is_monster_or_block(get_top_tile(pos)) && can_move(act, act->direction, 0)) {
                // Actor teleported successfully. Its position was changed just before so that
                // `can_move` could be called correctly, keep it there so that the tile gets set
                // when the actor is destroyed after this call.
                // Also set the TELEPORTED state to force the move out of the teleporter later.
                act->state = ACTOR_STATE_TELEPORTED;

                const actor_t actor = get_top_tile(pos);
                if (actor_get_entity(actor) == ENTITY_CHIP) {
                    // Oops, teleporting on Chip (this is legal in TW, not a collision)
                    // Chip tile will be lost after destruction, save it temporarily.
                    // This happens when Chip goes in teleporter at the same time or after
                    // a creature, but before that creature moves out of the teleporter.
                    // A bit of a hack, but it only costs 1B of RAM.
                    tworld.teleported_chip = actor;
                }
                return;
            }
        }

        if (idx == orig_idx) {
            // no destination teleporter found, actor is stuck.
            act->pos = pos;
            if (act->entity == ENTITY_CHIP) {
                tworld.flags |= FLAG_CHIP_STUCK;
            }
            return;
        }
    }
}

/**
 * Perform chosen move for the given actor. Flags can be provided for use with
 * the `can_move` function called during this step.
 * Returns a result indicating the outcome of the move.
 */
static move_result_t perform_move(moving_actor_t* act, const uint8_t flags) {
    const bool is_chip = (act->entity == ENTITY_CHIP);

    if (act->step <= 0) {
        direction_t dir_before;
        if (flags & CM_RELEASING) {
            // If releasing Chip from trap, ignore the new direction chosen, use last
            // movement direction. This ensures Chip cannot turn while trap is forcing the move.
            if (is_chip) {
                dir_before = act->direction;
                act->direction = tworld.last_chip_dir;
            }
        } else if (act->state == ACTOR_STATE_NONE) {
            // actor has not moved
            return MOVE_RESULT_SUCCESS;
        }

        const move_result_t result = start_movement(act, flags);
        if (result != MOVE_RESULT_SUCCESS) {
            // There's no need to set state to hidden: actors can only die in start_movement
            // as a result of collision, which ends the game, and we also want to actor tile to
            // be kept to show collision.
            if ((flags & CM_RELEASING) && is_chip) {
                // Restore Chip chosen direction before releasing from trap
                act->direction = dir_before;
                tworld.last_chip_dir = dir_before;
            }
            return result;
        }
    }

    if (!continue_movement(act)) {
        const end_cause_t end_cause = end_movement(act);
        if (end_cause != END_CAUSE_NONE) {
            if (is_chip) {
                tworld.end_cause = end_cause;
            } else {
                // Put actor in the "animated state", with a delay stored in the step field.
                act->state = ACTOR_STATE_DIED;
                act->step = (int8_t) (11 + ((tworld.current_time + stepping()) & 1));
            }
            return MOVE_RESULT_DIED;
        }
    }

    return MOVE_RESULT_SUCCESS;
}

/**
 * Sanity check at the start of a step. Optional, no side effects on state.
 */
static void step_check(void) {
#ifdef RUNTIME_CHECKS
    // Check if corresponding tile for actor has an entity.
    for (actor_idx_t i = 0; i < tworld.actors_size; ++i) {
        const active_actor_t actor = tworld.actors[i];
        const position_t pos = act_actor_get_pos(actor);
        if (actor_get_entity(get_top_tile(pos)) == ENTITY_NONE &&
            act_actor_get_state(actor) != ACTOR_STATE_HIDDEN) {
            if (i == 0 && tworld.teleported_chip != ACTOR_NONE) {
                // Special intermediary case where this is allowed.
                continue;
            }
            tworld_error("actor at (%d, %d) has no entity.", pos.x, pos.y);
        }
    }

    if (tworld.teleported_chip == ACTOR_NONE) {
        const position_t chip_pos = tworld_get_current_position();
        tworld_assert(actor_get_entity(get_top_tile(chip_pos)) == ENTITY_CHIP,
                      "chip is not first in actor list");
    }
#endif //RUNTIME_CHECKS
}

/**
 * Finish applying changes from last step before making a new step.
 */
static void prestep(void) {
    // update toggle wall/floor according to toggle state
    if (tworld.flags & FLAG_TOGGLE_STATE) {
        for (grid_pos_t y = 0; y < GRID_HEIGHT; ++y) {
            for (grid_pos_t x = 0; x < GRID_WIDTH; ++x) {
                const position_t pos = {x, y};
                const tile_t tile = get_bottom_tile(pos);
                if (tile_is_toggle_tile(tile)) {
                    set_bottom_tile(pos, tile_toggle_state(tile));
                }
            }
        }
    }

    // if needed, transform "reverse tanks" to normal tanks in the opposite direction.
    if (tworld.flags & FLAG_TURN_TANKS) {
        for (actor_idx_t i = 0; i < tworld.actors_size; ++i) {
            const active_actor_t actor = tworld.actors[i];
            if (act_actor_get_state(actor) == ACTOR_STATE_HIDDEN) {
                continue;
            }

            moving_actor_t mact;
            create_moving_actor(&mact, i);
            if (mact.entity == ENTITY_TANK_REVERSED) {
                mact.entity = ENTITY_TANK;
                if (mact.step <= 0) {
                    // don't turn tanks in between moves
                    mact.direction = direction_back(mact.direction);
                }
                destroy_moving_actor(&mact);
            }
        }
    }

    tworld.flags &= ~(FLAG_TOGGLE_STATE | FLAG_TURN_TANKS);

    tworld.chip_new_pos.x = CHIP_NEW_POS_NONE;
    tworld.chip_new_pos.y = CHIP_NEW_POS_NONE;
}

static void choose_all_moves(void) {
    for (actor_idx_t i = tworld.actors_size; i-- > 0;) {
        const active_actor_t actor = tworld.actors[i];
        const step_t step = act_actor_get_step(actor);
        const actor_state_t state = act_actor_get_state(actor);

        if (state == ACTOR_STATE_HIDDEN) {
            if (step > 0) {
                // "animated" state delay
                tworld.actors[i] = act_actor_set_step(actor, (step_t) (step - 1));
            }
            continue;
        }

        tworld.actors[i] = act_actor_set_state(actor, ACTOR_STATE_NONE);
        if (step <= 0) {
            moving_actor_t mact;
            create_moving_actor(&mact, i);
            choose_move(&mact, state == ACTOR_STATE_TELEPORTED);
            destroy_moving_actor(&mact);
        }
    }
}

static void perform_all_moves(void) {
    for (actor_idx_t i = tworld.actors_size; i-- > 0;) {
        const active_actor_t actor = tworld.actors[i];
        if (act_actor_get_state(actor) == ACTOR_STATE_HIDDEN) {
            continue;
        }

        moving_actor_t mact;
        create_moving_actor(&mact, i);
        const move_result_t result = perform_move(&mact, 0);
        bool persist = true;
        if (result != MOVE_RESULT_DIED && mact.step <= 0 &&
            get_bottom_tile(mact.pos) == TILE_BUTTON_BROWN) {
            // If a block is on a trap button and Chip pushes it off while springing the trap,
            // the block will be pushed and persisted then. Do not persist it in that case,
            // since the instance of moving_actor_t we have here has an outdated position.
            // Make sure of this by saving the index of the current block and checking it
            // in _can_push_block.
            tworld.actor_springing_trap = i;
            spring_trap(mact.pos);
            persist = (tworld.actor_springing_trap != ACTOR_INDEX_NONE);
            tworld.actor_springing_trap = ACTOR_INDEX_NONE;
        }
        if (persist) {
            destroy_moving_actor(&mact);
        }
    }
}

static void teleport_all(void) {
    if (tworld_is_game_over()) {
        // If collision occured with Chip on teleporter tile, don't teleport monster that caused it.
        return;
    }

    for (actor_idx_t i = tworld.actors_size; i-- > 0;) {
        const active_actor_t actor = tworld.actors[i];
        if (act_actor_get_state(actor) == ACTOR_STATE_HIDDEN || act_actor_get_step(actor) > 0) {
            continue;
        }

        const position_t pos = act_actor_get_pos(actor);
        if (get_bottom_tile(pos) == TILE_TELEPORTER) {
            moving_actor_t mact;
            create_moving_actor(&mact, i);
            teleport_actor(&mact);
            destroy_moving_actor(&mact);
        }
    }
}

// ===================================

void tworld_init(void) {
    // Most fields are zero-initialized
    memset(&tworld.zero_init_start, 0,
           sizeof tworld - (tworld.zero_init_start - (uint8_t*) &tworld));

    if (tworld.time_left == TIME_LEFT_NONE) {
        tworld.flags = FLAG_NO_TIME_LIMIT;
    }
    tworld.collided_with = ACTOR_INDEX_NONE;
    tworld.actor_springing_trap = ACTOR_INDEX_NONE;

    random_seed(time_get());

#ifdef RUNTIME_CHECKS
    tworld.error = false;
#endif //RUNTIME_CHECKS
#ifdef TEST
    tworld.prng_value0 = time_get();
    tworld.prng_value1 = 0;
    tworld.prng_value2 = 0;
#endif //TEST

    build_actor_list();
}

void tworld_update(void) {
    if (tworld.time_left == 0) {
        tworld.end_cause = END_CAUSE_OUTOFTIME;
        return;
    }

    step_check();
    prestep();
    choose_all_moves();
    perform_all_moves();
    teleport_all();

    ++tworld.current_time;
    if (!(tworld.flags & FLAG_NO_TIME_LIMIT)) {
        --tworld.time_left;
    }
}

bool tworld_is_game_over(void) {
    return tworld.end_cause != END_CAUSE_NONE;
}

position_t tworld_get_current_position(void) {
    return act_actor_get_pos(chip_actor());
}

tile_t tworld_get_bottom_tile(const position_t pos) {
    return get_bottom_tile(pos);
}

actor_t tworld_get_top_tile(const position_t pos) {
    return get_top_tile(pos);
}

bool tworld_has_collided(void) {
    return tworld.end_cause == END_CAUSE_COLLIDED_MONSTER ||
           tworld.end_cause == END_CAUSE_COLLIDED_BLOCK;
}
