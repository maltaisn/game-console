
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
 * outside of the display area, RAM corruption will happen when checks are disabled.
 * There are no protections against invalid draw calls when targeting the game console,
 * only in the simulator, or if GRAPHICS_CHECKS is explicitly defined.
 * This is enables various runtime checks for other functions.
 */
#define GRAPHICS_CHECKS
#endif

/**
 * A font is just an address to the font data in the unified data space.
 *
 * The font format is a 5-byte header followed by glyph data (by bit position):
 * [0]: glyph count
 * [8]: bytes per char (1-33)
 * [16]: glyph width, minus one (0-15)
 * [20]: glyph height, minus one (0-15)
 * [24]: bits of Y offset per glyph (0-4)
 * [28]: max Y offset (0-15)
 * [32]: line spacing (0-32)
 * [40+]: glyph data
 *
 * Remarks:
 * - Each glyph data is byte aligned and takes a number of bytes. From most significant bit to
 *     least significant bit and from most significant byte to least significant byte, glyph data
 *     first encodes the bits for Y offset, then 0 or 1 for set pixels, from left to right, and
 *     from top to bottom. Glyph data bytes are in little endian order.
 * - The glyph offset bits are not reversed. For example if using 3 bits of Y offset and
 *     the first byte of a glyph is 0b00100000, then the offset is 1, not 4.
 * - Glyphs are placed in order, within two ranges: 0x21-0x7f and 0xa0-0xff.
 *     Chars outside these ranges cannot be encoded.
 *     If glyph count is less or equal to 95 (0x5f), only the 0x21-0x7f range is present.
 *     Maximum glyph count is 191 (0xc0)
 * - Maximum glyph size is 16x16, using size byte 0xff.
 * - A glyph can have at most 33 bytes per char, in other words a 16x16 glyph with 7 bit Y offset.
 * - 1 pixel is always left between glyphs when drawing a string, this space is not encoded.
 * - Y offset can only be positive (Y positive goes down).
 * - Glyphs are drawn from their top left corner.
 * - Characters that are not encoded in font will appear blank (a notable example is the space).
 */
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
 * Draw a single glyph using the current font and color.
 * The glyph must fit within the screen.
 */
void graphics_glyph(int8_t x, int8_t y, char c);

/**
 * Draw text using the current font and color.
 * Text that would be drawn past the end of display is not drawn.
 */
void graphics_text(int8_t x, int8_t y, const char* text);

/**
 * Draw text using the current font and color.
 * The text will wrap to next line on space separators whenever possible.
 * `wrap_x` must be at most equal to DISPLAY_WIDTH and must be greater or equal to `x`.
 */
void graphics_text_wrap(int8_t x, int8_t y, uint8_t wrap_x, const char* text);

/**
 * Measure text width with current font (no wrapping).
 * If text size exceeds display width, DISPLAY_WIDTH is returned.
 */
uint8_t graphics_text_width(const char* text);

/**
 * Draw decimal number using the current font and color.
 * The drawn text is not wrapped.
 */
void graphics_text_num(int8_t x, int8_t y, int32_t num);

#endif //CORE_GRAPHICS_H
