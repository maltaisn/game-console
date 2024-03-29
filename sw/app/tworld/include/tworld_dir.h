
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

#ifndef TWORLD_TWORLD_DIR_H
#define TWORLD_TWORLD_DIR_H

#include <stdint.h>

typedef enum {
    DIR_NORTH = 0x0,
    DIR_WEST = 0x1,
    DIR_SOUTH = 0x2,
    DIR_EAST = 0x3,
} direction_t;

typedef enum {
    DIR_MASK_NONE = 0,
    DIR_NORTH_MASK = 1 << DIR_NORTH,
    DIR_WEST_MASK = 1 << DIR_WEST,
    DIR_SOUTH_MASK = 1 << DIR_SOUTH,
    DIR_EAST_MASK = 1 << DIR_EAST,
    DIR_NORTHWEST_MASK = DIR_NORTH_MASK | DIR_WEST_MASK,
    DIR_SOUTHWEST_MASK = DIR_SOUTH_MASK | DIR_WEST_MASK,
    DIR_NORTHEAST_MASK = DIR_NORTH_MASK | DIR_EAST_MASK,
    DIR_SOUTHEAST_MASK = DIR_SOUTH_MASK | DIR_EAST_MASK,
    DIR_VERTICAL_MASK = DIR_NORTH_MASK | DIR_SOUTH_MASK,
    DIR_HORIZONTAL_MASK = DIR_WEST_MASK | DIR_EAST_MASK,
    // typedef is also used for any other combination of the above.
} direction_mask_t;

/** Returns the direction opposite to the given direction. */
direction_t direction_back(direction_t dir);

/** Returns the direction on the right (clockwise) to the given direction. */
direction_t direction_right(direction_t dir);

/** Returns the direction on the left (counterclockwise) to the given direction. */
direction_t direction_left(direction_t dir);

/** Returns the direction mask for a direction. */
direction_mask_t direction_to_mask(direction_t dir);

/** Returns the direction for a direction mask. */
direction_t direction_from_mask(direction_mask_t mask);

#endif //TWORLD_TWORLD_DIR_H
