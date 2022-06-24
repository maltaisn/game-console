
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

#ifndef TWORLD_RENDER_UTILS_H
#define TWORLD_RENDER_UTILS_H

#include "tworld_tile.h"
#include "tworld_actor.h"
#include "tworld.h"

#include <core/graphics.h>

#define tworld_time_left_in_seconds() \
    ((uint16_t) (tworld.time_left + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND)

/**
 * Format the time left in in-game seconds to a buffer.
 * The result is right aligned (3 chars width) and padded with spaces.
 * Creates a "---" string if level is untimed.
 */
void format_time_left(char buf[static 4]);

/**
 * From Chip's position on the grid (X or Y), return the first shown tile in that axis.
 */
grid_pos_t get_camera_pos(grid_pos_t pos);

/**
 * Draw a 16x14 bottom tile at a position. Tiles must be drawn left to right.
 * Y position must be greater or equal to display page start and less than display page end.
 * X position must be even.
 */
void draw_bottom_tile(disp_x_t x, disp_y_t y, tile_t tile);

/**
 * Draw a 16x14 top tile at a position.
 * Y position must be greater or equal to display page start and less than display page end.
 * X position must be even.
 */
void draw_top_tile(disp_x_t x, disp_y_t y, actor_t tile);

#endif //TWORLD_RENDER_UTILS_H
