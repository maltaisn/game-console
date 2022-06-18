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
#include "level.h"

#include <string.h>

enum {
    // indicates toggle floor/wall (=0x1, this is important)
    FLAG_TOGGLE_STATE = 1 << 0,
    // indicates that there may be "reverse tanks" on the grid
    FLAG_TURN_TANKS = 1 << 1,
    // indicates that chip has moved by himself
    FLAG_CHIP_SELF_MOVED = 1 << 2,
    // indicates that chip move has been forced
    FLAG_CHIP_FORCE_MOVED = 1 << 3,
    // indicates that chip can override force floor direction
    FLAG_CHIP_CAN_UNSLIDE = 1 << 4,
    // indicates that chip is stuck on a teleporter
    FLAG_CHIP_STUCK = 1 << 5,
    // indicates that the level is untimed
    FLAG_NO_TIME_LIMIT = 1 << 7,
};

enum {
    DIR_NORTH_MASK = 1 << DIR_NORTH,
    DIR_WEST_MASK = 1 << DIR_WEST,
    DIR_SOUTH_MASK = 1 << DIR_SOUTH,
    DIR_EAST_MASK = 1 << DIR_EAST,
};

enum {
    ACTOR_STATE_NONE = 0,
    ACTOR_STATE_HIDDEN = 1,
    ACTOR_STATE_MOVED = 2,
    ACTOR_STATE_TELEPORTED = 3,
};

void tworld_init(void) {
    // Most fields are zero-initialized
    memset(&tworld.zero_init_start, 0,
           sizeof tworld - (tworld.zero_init_start - (uint8_t*) &tworld));

    if (tworld.time_limit == 0) {
        tworld.flags = FLAG_NO_TIME_LIMIT;
    }
    tworld.collided_with = ACTOR_INDEX_NONE;
    tworld.actor_springing_trap = ACTOR_INDEX_NONE;

    // TODO build actor list
}

void tworld_update(void) {
    // TODO
}
