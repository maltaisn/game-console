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

#include "tworld_dir.h"

static const direction_t DIR_FROM_MASK[] = {
        -1,
        DIR_NORTH,
        DIR_WEST,
        -1,
        DIR_SOUTH,
        -1,
        -1,
        -1,
        DIR_EAST,
};

static const uint8_t DIR_TO_MASK[] = {
        DIR_NORTH_MASK,
        DIR_WEST_MASK,
        DIR_SOUTH_MASK,
        DIR_EAST_MASK,
};

static const uint8_t DIR_BACK[] = {
        DIR_SOUTH,
        DIR_EAST,
        DIR_NORTH,
        DIR_WEST,
};

direction_t direction_back(direction_t dir) {
    return DIR_BACK[dir];
}

direction_t direction_right(direction_t dir) {
    return (uint8_t) (dir - 1) % 4;
}

direction_t direction_left(direction_t dir) {
    return (dir + 1) % 4;
}

direction_mask_t direction_to_mask(direction_t dir) {
    return DIR_TO_MASK[dir];
}

direction_t direction_from_mask(direction_mask_t mask) {
    return DIR_FROM_MASK[mask];
}
