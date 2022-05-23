
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

#include <core/display.h>
#include <core/flash.h>
#include <core/data.h>

#include <stdint.h>

/**
 * Spacing in pixels between every glyph when text is drawn.
 */
#define GRAPHICS_GLYPH_SPACING 1

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

/**
 * An image is just an address to the image data in unified data space.
 *
 * The supported bit depths are 1-bit and 4-bit.
 * Two encodings are supported for each:
 * - Raw: only packed pixel data.
 * - Mixed: mix of raw data and run-length encoded data.
 *
 * Mixed encoding limits the worst case overhead (an image with no run lengths, only raw data) and
 * allows significant size savings on images with long run lengths.
 * "Real" compression is not really an option here since the image must be indexed and compression
 * algorithms often require continuous decoding, with a window for back references.
 *
 * Raw encoding is provided mainly for 4-bit images, since the mixed decoding algorithm is
 * heavier than for 1-bit images and raw encoding will almost always outperform mixed encoding
 * in terms of speed, even when reading more data from external memory.
 * Raw encoding for 1-bit images is provided for completeness but doesn't provide significant
 * improvements over mixed encoding since the mixed decoding algorithm is very fast.
 *
 * Built-in images (dialog arrows, battery icons) use mixed 1-bit encoding.
 *
 * INDEXING
 *
 * Both formats can have an index after the header to skip parts of the image quickly.
 * Indeed, since the display is refreshed in many pages, there would be a huge performance
 * loss having to iterate over an image to skip parts that are not drawn. This is especially
 * important with images stored in external memory since reading each byte takes several cycles.
 * With indexing, the overhead added by pages is limited to reading at most 256 extra bytes.
 * This can be reduced further by choosing an appropriate index granularity.
 *
 * The index granularity is the number of rows between each entry in the index.
 * For example, if the granularity is 3 rows for an image with 128 rows, the index will contain
 * 42 entries. This will increase the size of the image by at least 42 bytes.
 * The current state of the encoder and decoder is also reset on an index bound, to make it
 * possible to jump to the location pointed by the index without knowing the prior state.
 * This fact may also increase the image size slightly.
 *
 * The raw encoding cannot have an index. In fact, the encoder state is reset on each row for the
 * raw encoding, which makes any raw image implicitly indexed. The indexed flag will be ignored.
 *
 * FORMAT DESCRIPTION
 *
 * The header format is:
 * [0]: signature byte, 0xf1
 * [1]: image flags:
 *      - [0..3]: alpha color
 *      - [4]: bit depth    (0: 4-bit, 1: 4-bit)
 *      - [5]: encoding     (0: mixed, 1: raw)
 *      - [6]: transparency (0: no, 1: yes)
 *      - [7]: indexed      (0: no, 1: yes)
 * [2]: image width, -1 (width is from 1 to 256)
 * [3]: image height, -1 (height is from 1 to 256)
 *
 * The alpha color is the color to be treated as transparent if transparency is enabled.
 * This means an image with transparency cannot use all gray levels.
 * Transparency only works with 4-bit images, not 1-bit.
 *
 * If not indexed, index section is omitted completely.
 * If indexed index format is:
 * [4]: index granularity. The 0x00 value is prohibited.
 * [5..(n+4)]: index entries, n = floor(image_height / granularity)
 *             The first index entry is the offset from position 5 to the start of image data.
 *             The following entries are offsets from the previous entry.
 *             If granularity is greater or equal to image height, n = 0 and there are no entries.
 *             1 is subtracted from all offset so that offsets can span 1 to 256 bytes.
 *
 * The image data starts at position 4 if not indexed and at position (n+5) if indexed.
 * The data is encoded differently depending on the encoding indicated by the flag byte.
 * Important: the state of the encoder and decoder is reset on index boundaries!
 *
 * 1-bit raw encoding:
 * Each byte encode 8 pixels, with the LSB being the leftmost pixel.
 * Pixel data in a byte is not allowed to cross an index boundary.
 *
 * 1-bit mixed encoding:
 * The MSB of each byte determines its type:
 * - Raw byte: MSB is not set. The remaining 7 bits is raw data for the next pixels.
 *             The data is encoded from MSB (bit 6) to LSB (bit 0). (0=transparent, 1=color).
 * - RLE byte: MSB is set. Bit 6 indicates the run-length color (0=transparent, 1=color).
 *             The remaining 6 bits indicate the run length, minus 8.
 *             Thus the run length can be from 8 to 71.
 * Neither sequence type is allowed to cross an index boundary.
 *
 * 4-bit raw encoding:
 * Each byte encode 2 pixels, with the least significant nibble being the leftmost pixel.
 * Pixel data in a byte is not allowed to cross an index boundary.
 *
 * 4-bit mixed encoding:
 * - Raw sequence: starts with a byte indicating the number of "raw" color bytes that follow.
 *     The MSB on the length byte is 0. The remaining bits indicated the length, minus one.
 *     Thus a raw sequence can have a length of 1 to 128.
 * - RLE length: the MSB is 1, the remaining bits indicate the run length, minus 4.
 *     Thus the run length can be 3 to 130. The color for the run is defined by the
 *     second nibble if a RLE color byte has already been parsed, or the first nibble
 *     if no color byte has been parsed.
 * - RLE color: two nibbles indicating the color for the RLE immediately before and
 *     the next one encountered in the stream.
 * The first byte can be either a raw sequence length byte or a RLE byte.
 * No sequence can cross an index boundary. The RLE color byte is reset on index boundary.
 * A raw sequence can exceptionnally encode an odd number of pixels if crossing an index boundary.
 *
 * BENCHMARKS
 *
 * Very rough benchmark done with tiger128.png & tiger-bin128.png, stored in external flash memory.
 * Clearing only the display takes 21 ms per frame, which was subtracted from the measured time.
 *
 * - 1-bit raw:   56 ms  (33 cycles/px)
 * - 1-bit mixed: 62 ms  (37 cycles/px)
 * - 4-bit raw:   101 ms (61 cycles/px)
 * - 4-bit mixed: 119 ms (72 cycles/px)
 */
typedef data_ptr_t graphics_image_t;

/**
 * A font is just an address to the font data in the unified data space.
 *
 * The font format is a 6-byte header followed by glyph data (by bit position):
 * [0]: signature byte, 0xf0
 * [8]: glyph count
 * [16]: bytes per char (1-33)
 * [24]: glyph width, minus one (0-15)
 * [28]: glyph height, minus one (0-15)
 * [32]: bits of Y offset per glyph (0-4)
 * [36]: max Y offset (0-15)
 * [40]: line spacing (0-32)
 * [48+]: glyph data
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

typedef struct {
    graphics_font_t addr;  // this is the address of the start of glyph data
    uint8_t glyph_count;
    uint8_t glyph_size;
    uint8_t offset_bits;
    uint8_t offset_max;
    uint8_t line_spacing;
    uint8_t width;
    uint8_t height;
} graphics_font_data_t;

/**
 * The current font. There is no default value.
 */
extern graphics_font_data_t graphics_font;

// 3x5 font, 2 bytes per char, encodes 0x21-0x5a, total size 122 bytes.
// from https://github.com/olikraus/u8g2/wiki/fntgrpx11#micro, u8g2_font_micro_tr, modified
extern const uint8_t GRAPHICS_BUILTIN_FONT_DATA[];
#define GRAPHICS_BUILTIN_FONT data_mcu(GRAPHICS_BUILTIN_FONT_DATA)

// provided for uniformity with defines generated by assets packer.
#define ASSET_FONT_3X5_BUILTIN GRAPHICS_BUILTIN_FONT

/**
 * Set the current graphics color. The default color is black.
 * Note that setting the color is a very inexpensive operation (1 or 2 cycles).
 */
void graphics_set_color(disp_color_t color);

/**
 * Get the current graphics color.
 */
disp_color_t graphics_get_color(void);

/**
 * Set the current font. There is no font set by default, one must be set prior to drawing text.
 * Note that setting the font is a relatively expensive operation (a few hundred cycles).
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
 * Draw a 1-bit raw image from unified data space, with top left corner at position (x, y),
 * using the current color. The image must fully fit within display bounds.
 */
void graphics_image_1bit_raw(graphics_image_t data, disp_x_t x, disp_y_t y);

/**
 * Same as `graphics_image_1bit_raw` but draws a vertical portion of the image.
 * The bottom coordinate is inclusive. The image region must fully fit within display bounds.
 */
void graphics_image_1bit_raw_region(graphics_image_t data, disp_x_t x, disp_y_t y,
                                    uint8_t top, uint8_t bottom);

/**
 * Draw a 1-bit raw image from unified data space, with top left corner at position (x, y),
 * using the current color. The image must fully fit within display bounds.
 */
void graphics_image_1bit_mixed(graphics_image_t data, disp_x_t x, disp_y_t y);

/**
 * Same as `graphics_image_1bit_raw` but draws a vertical portion of the image.
 * The bottom coordinate is inclusive. The image region must fully fit within display bounds.
 */
void graphics_image_1bit_mixed_region(graphics_image_t data, disp_x_t x, disp_y_t y,
                                      uint8_t top, uint8_t bottom);

/**
 * Draw a 1-bit raw image from unified data space, with top left corner at position (x, y),
 * using the current color. The image must fully fit within display bounds.
 */
void graphics_image_4bit_raw(graphics_image_t data, disp_x_t x, disp_y_t y);

/**
 * Same as `graphics_image_1bit_raw` but draws a vertical portion of the image.
 * The bottom coordinate is inclusive. The image region must fully fit within display bounds.
 */
void graphics_image_4bit_raw_region(graphics_image_t data, disp_x_t x, disp_y_t y,
                                    uint8_t top, uint8_t bottom);

/**
 * Draw a 1-bit raw image from unified data space, with top left corner at position (x, y),
 * using the current color. The image must fully fit within display bounds.
 */
void graphics_image_4bit_mixed(graphics_image_t data, disp_x_t x, disp_y_t y);

/**
 * Same as `graphics_image_1bit_raw` but draws a vertical portion of the image.
 * The bottom coordinate is inclusive. The image region must fully fit within display bounds.
 */
void graphics_image_4bit_mixed_region(graphics_image_t data, disp_x_t x, disp_y_t y,
                                      uint8_t top, uint8_t bottom);

/**
 * Draw a single glyph using the current font and color.
 * The glyph can be drawn partially or completely outside of screen bounds.
 */
void graphics_glyph(int8_t x, int8_t y, char c);

/**
 * Draw text using the current font and color.
 * Text that would be drawn past outside of the display is not drawn.
 * Does not handle line breaks.
 */
void graphics_text(int8_t x, int8_t y, const char* text);

/**
 * Draw text using the current font and color.
 * The text will wrap to next line on space separators whenever possible.
 * `wrap_x` must be at most equal to DISPLAY_WIDTH and must be greater or equal to `x`.
 * Does not handle line breaks.
 */
void graphics_text_wrap(int8_t x, int8_t y, uint8_t wrap_x, const char* text);

/**
 * Measure text width with current font (no wrapping).
 * If text size exceeds display width, DISPLAY_WIDTH is returned.
 */
uint8_t graphics_text_width(const char* text);

/**
 * Returns the glyph width for the current font.
 */
uint8_t graphics_glyph_width(void);

/**
 * Get text height with current font. This just returns the glyph height, not taking into
 * account the extra line spacing or maximum Y offset.
 */
uint8_t graphics_text_height(void);

/**
 * Get maximum text height with current font. This is the glyph height plus the maximum Y offset.
 */
uint8_t graphics_text_max_height(void);

#endif //CORE_GRAPHICS_H
