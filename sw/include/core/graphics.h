
/*
 * Copyright 2021 Nicolas Maltais
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

#ifndef CORE_GRAPHICS_H
#define CORE_GRAPHICS_H

#include <sys/display.h>
#include <sys/flash.h>

#include <stdint.h>
#include "sys/data.h"

/*
 * The coordinate system used by the graphics functions is Y down, X right.
 * The top left corner corresponds to the (0,0) coordinates.
 *
 * Updating the display should be done like this:
 *
 *     display_first_page();
 *     do {
 *         graphics_clear(<COLOR>);
 *         // Update display buffer.
 *         // This code must be the exactly the same for each iteration!
 *     } while (display_next_page());
 */

#ifdef SIMULATION
/**
 * If any drawing functions is used in such a way that drawing would happen
 * outside of the display area, corruption will happen when checks are disabled.
 * There are no protections against invalid draw calls when targeting the game console,
 * only in the simulator, or if GRAPHICS_CHECKS is explicitly defined.
 */
#define GRAPHICS_CHECKS
#endif

/** A font is just an address to the font data in flash data space. */
typedef data_ptr_t graphics_font_t;

/**
 * Set the current graphics color.
 * The default color is black.
 */
void graphics_set_color(disp_color_t color);

/**
 * Get the current graphics color.
 */
disp_color_t graphics_get_color(void);

/**
 * Set the current font. The default font is a 3x5 font that encodes 0x20-0x5a
 */
void graphics_set_font(graphics_font_t font);

/**
 * Get the current font.
 */
graphics_font_t graphics_get_font(void);

/**
 * Clear buffer with a color.
 */
void graphics_clear(disp_color_t color);

/**
 * Set pixel at (x, y) to current color.
 */
void graphics_pixel(disp_x_t x, disp_y_t y);

/**
 * Draw a one pixel thick horizontal line between x1 and x2 (inclusive), at y.
 * x1 may be smaller than y0.
 */
void graphics_hline(disp_x_t x1, disp_x_t x2, disp_y_t y);

/**
 * Draw a one pixel thick horizontal line between y0 and y1 (inclusive), at x.
 * y1 may be smaller than y0.
 */
void graphics_vline(disp_y_t y0, disp_y_t y1, disp_x_t x);

/**
 * Draw a one pixel thick diagonal line between (x1, y0) and (x2, y1) (inclusive).
 */
void graphics_line(disp_x_t x1, disp_y_t y0, disp_x_t x2, disp_y_t y1);

/**
 * Draw a one pixel thick rectangle with top left corner at (x, y),
 * with a width and a height in pixels.
 */
void graphics_rect(disp_x_t x, disp_y_t y, uint8_t w, uint8_t h);

/**
 * Draw a filled rectangle with top left corner at (x, y),
 * with a width and a height in pixels.
 */
void graphics_fill_rect(disp_x_t x, disp_y_t y, uint8_t w, uint8_t h);

/**
 * Draw a one pixel thick circle centered at (x, y) with radius r.
 */
void graphics_circle(disp_x_t x, disp_y_t y, uint8_t r);

/**
 * Draw a filled circle centered at (x, y) with radius r.
 */
void graphics_filled_circle(disp_x_t x, disp_y_t y, uint8_t r);

/**
 * Draw a one pixel thick ellipse centered at (x, y) with radii (rx, ry).
 */
void graphics_ellipse(disp_x_t x, disp_y_t y, uint8_t rx, uint8_t ry);

/**
 * Draw a filled ellipse centered at (x, y) with radii (rx, ry).
 */
void graphics_filled_ellipse(disp_x_t x, disp_y_t y, uint8_t rx, uint8_t ry);

/**
 * Draw an image from unified data space.
 */
void graphics_image(data_ptr_t data, uint8_t w, uint8_t h);

/**
 * Draw a portion of an image from unified data space.
 * The portion drawn is from the top left position (x, y), with a width and a height in pixels.
 */
void graphics_image_part(data_ptr_t data, uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/**
 * Draw text from buffer using the current font and color.
 */
void graphics_text(disp_x_t x, disp_y_t y, const char* text);

/**
 * Draw decimal number using the current font and color.
 */
void graphics_text_num(disp_x_t x, disp_y_t y, uint32_t num);

#endif //CORE_GRAPHICS_H
