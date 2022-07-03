
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

#define ACTIVE_COLOR(cond) ((cond) ? 12 : 6)

/**
 * Format a number under 1000 to a buffer, right-aligned to a width of 3 chars, padded with zeroes.
 */
void uint16_to_str_zero_pad(char buf[static 4], uint16_t n);

/**
 * Format the time left in in-game seconds to a buffer from the specified time in game ticks.
 * The result is right aligned (3 chars width) and padded with zeroes.
 * Creates a "---" string if the time is `TIME_LEFT_NONE` (e.g. if untimed or unknown).
 */
void format_time_left(char buf[static 4], time_left_t time);

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

/**
 * Draw a tile composed of a bottom and up to 2 top tiles for the given tile and actor.
 * Y position must be greater or equal to display page start and less than display page end.
 * X position must be even. This handles special cases like block, swimming chip, etc.
 */
void draw_game_tile(disp_x_t x, disp_y_t y, tile_t tile, actor_t actor0, actor_t actor1);

/**
 * Draw text in a box at top left coordinates and with specified dimensions,
 * wrapping text at the end of lines. The text can be left aligned or centered.
 * The text is drawn in the 5x7 font with current color.
 *
 * Differences from `graphics_text_wrap`:
 * - Text is stored in flash instead of RAM.
 * - Text has a maximum height.
 * - Ability to center align text.
 * - Measurement utilities provided (line start & line count).
 */
void draw_text_wrap(disp_x_t x, disp_y_t y, uint8_t width, uint8_t max_lines,
                    flash_t text, bool centered);

/**
 * Returns the address in flash for the start of a text line to be drawn in a box.
 */
flash_t find_text_line_start(flash_t text, uint8_t width, uint8_t line);

/**
 * Returns the number of lines for a text stored in flash, to be drawn with `draw_text_wrap`.
 */
uint8_t find_text_line_count(flash_t text, uint8_t width);

/**
 * Draw up and down navigation arrows for current dialog, at specified positions.
 */
void draw_vertical_navigation_arrows(uint8_t top_y, uint8_t bottom_y);

#endif //TWORLD_RENDER_UTILS_H
