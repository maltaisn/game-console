
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

#include <core/graphics.h>
#include <core/trace.h>

#include <sys/data.h>
#include <sys/display.h>

#include <string.h>

/**
 * Important note: when most drawing functions are used in such a way that drawing would happen
 * outside of the display area, RAM corruption will happen when checks are disabled.
 * There are no protections against invalid draw calls when targeting the game console,
 * only in the simulator, or if RUNTIME_CHECKS is explicitly defined.
 */
#ifdef RUNTIME_CHECKS

#include <ctype.h>

#endif

#define FONT_SIGNATURE 0xf0
#define FONT_HEADER_SIZE 6

#define FONT_RANGE0_START 0x21
#define FONT_RANGE0_END 0x7f
#define FONT_RANGE0_LEN (FONT_RANGE0_END - FONT_RANGE0_START + 1)
#define FONT_RANGE1_START 0xa0
#define FONT_RANGE1_END 0xff
#define FONT_RANGE1_LEN (FONT_RANGE1_END - FONT_RANGE1_START + 1)

#define FONT_MAX_GLYPHS (FONT_RANGE0_LEN + FONT_RANGE1_LEN)
#define FONT_MAX_Y_OFFSET_BITS 7
#define FONT_MIN_GLYPH_SIZE 1
#define FONT_MAX_GLYPH_SIZE 33
#define FONT_MAX_LINE_SPACING 32

#define IMAGE_SIGNATURE 0xf1
#define IMAGE_HEADER_SIZE 4
#ifndef GRAPHICS_NO_INDEXED_IMAGE
#define IMAGE_INDEX_HEADER_SIZE 2
#else
#define IMAGE_INDEX_HEADER_SIZE 0
#endif

#define IMAGE_INDEX_BUFFER_SIZE 8
#define IMAGE_BUFFER_SIZE 16

enum {
    IMAGE_FLAG_BINARY = (1 << 0),
    IMAGE_FLAG_INDEXED = (1 << 1),
    IMAGE_FLAG_RAW = (1 << 2),
};

// currently selected color, black by default.
static disp_color_t color;

// font specs of currently selected font
static struct {
    graphics_font_t addr;  // this is the address of the start of glyph data
    uint8_t glyph_count;
    uint8_t glyph_size;
    uint8_t offset_bits;
    uint8_t offset_max;
    uint8_t line_spacing;
    uint8_t width;
    uint8_t height;
} font;

const uint8_t GRAPHICS_BUILTIN_FONT_DATA[] = {
        0xf0, 0x3a, 0x02, 0x42, 0x00, 0x06, 0x0c, 0xdb, 0x00, 0xb4, 0xfa, 0xbe,
        0x3e, 0x5f, 0x4a, 0xa5, 0xe4, 0x4b, 0x00, 0x48, 0x22, 0x29, 0x28, 0x89,
        0xaa, 0xab, 0xa0, 0x0b, 0x2c, 0x00, 0x80, 0x03, 0x6c, 0x00, 0x28, 0x25,
        0xde, 0xf6, 0x2e, 0x59, 0xce, 0xe7, 0x9e, 0xe5, 0x92, 0xb7, 0x9e, 0xf3,
        0xde, 0xf3, 0x92, 0xe4, 0xde, 0xf7, 0x9e, 0xf7, 0x6c, 0xd8, 0x2c, 0xd8,
        0x22, 0x2a, 0x70, 0x1c, 0xa8, 0x88, 0x84, 0xe5, 0x46, 0x77, 0xda, 0xf7,
        0x5e, 0xf7, 0x4e, 0xf2, 0xdc, 0xd6, 0xce, 0xf3, 0xc8, 0xf3, 0xde, 0xf2,
        0xda, 0xb7, 0x2e, 0xe9, 0xde, 0x24, 0x5a, 0xb7, 0x4e, 0x92, 0xda, 0xbe,
        0xda, 0xf6, 0xde, 0xf6, 0xc8, 0xf7, 0xe6, 0xf6, 0x5a, 0xf7, 0x9e, 0xf3,
        0x24, 0xe9, 0xde, 0xb6, 0xd4, 0xb6, 0xfa, 0xb6, 0x7a, 0xbd, 0xa4, 0xb7,
        0x4e, 0xe5
};


#define set_block_left(c, block) ((block) = ((block) & 0xf0) | (c))
#define set_block_right(c, block) ((block) = ((block) & 0xf) | (c) << 4)
#define set_block_both(c, block) ((block) = ((c) | (c) << 4))

// set the pixel in a display buffer row, handling left & right block cases given x.
// increments the buffer if right block was written.
#define set_buffer_pixel(c, x, buffer) do { \
        if ((x) & 1) {                      \
            set_block_right(c, *(buffer));  \
            ++(buffer);                     \
        } else {                            \
            set_block_left(c, *(buffer));   \
        }                                   \
    } while (0);

#define swap(T, a, b) do { \
        T temp = (a);      \
        (a) = (b);         \
        (b) = temp;        \
    } while (0)

#define min(a, b) ((a) >= (b) ? (b) : (a))
#define max(a, b) ((a) <= (b) ? (b) : (a))

#define saturate_sub(a, b) ((a) <= (b) ? 0 : ((a) - (b)))

// nibble swap is implemented as a single instruction on AVR
// can be used as a quick >> 4 when the most significant nibble is not important.
#define nibble_swap(a) ((a) >> 4 | (a) << 4)

void graphics_set_color(disp_color_t c) {
#ifdef RUNTIME_CHECKS
    if (c > DISPLAY_COLOR_WHITE) {
        trace("invalid color");
        return;
    }
#endif
    color = c;
}

disp_color_t graphics_get_color(void) {
    return color;
}

void graphics_set_font(graphics_font_t f) {
    // read font header to get its specs
    uint8_t header[FONT_HEADER_SIZE];
    data_read(f, FONT_HEADER_SIZE, header);
    if (header[0] != FONT_SIGNATURE) {
        trace("invalid font signature");
        return;
    }
    font.addr = f + FONT_HEADER_SIZE;
    font.glyph_count = header[1];
    font.glyph_size = header[2];
    font.width = (header[3] & 0xf) + 1;
    font.height = (header[3] >> 4) + 1;
    font.offset_bits = header[4] & 0xf;
    font.offset_max = header[4] >> 4;
    font.line_spacing = header[5];

#ifdef RUNTIME_CHECKS
    if (font.glyph_count > FONT_MAX_GLYPHS) {
        trace("font has too many glyphs");
    }
    if (font.offset_bits > FONT_MAX_Y_OFFSET_BITS) {
        trace("font Y offset bits out of bounds");
    }
    if (font.glyph_size < FONT_MIN_GLYPH_SIZE || font.glyph_size > FONT_MAX_GLYPH_SIZE) {
        trace("font glyph size out of bounds");
    }
    if (font.offset_max >= (1 << font.offset_bits)) {
        trace("max offset not coherent with offset bits");
    }
    if (font.line_spacing > FONT_MAX_LINE_SPACING) {
        trace("line spacing out of bounds");
    }
#endif
}

graphics_font_t graphics_get_font(void) {
    return font.addr - FONT_HEADER_SIZE;
}

void graphics_clear(disp_color_t c) {
    c = c | c << 4;
    uint8_t* buffer = display_buffer(0, 0);
    memset(buffer, c, page_height() * DISPLAY_NUM_COLS);
}

static void graphics_pixel_fast(const disp_x_t x, const disp_y_t y) {
#ifdef RUNTIME_CHECKS
    if (x >= DISPLAY_WIDTH || y >= page_height()) {
        trace("drawing outside bounds");
        return;
    }
#endif
    uint8_t* buffer = display_buffer(x, y);
    set_buffer_pixel(color, x, buffer);
}

void graphics_pixel(const disp_x_t x, const disp_y_t y) {
#ifdef RUNTIME_CHECKS
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        trace("drawing outside bounds");
        return;
    }
#endif
    if (y >= display_page_ystart && y <= display_page_yend) {
        graphics_pixel_fast(x, y - display_page_ystart);
    }
}

static void graphics_hline_fast(disp_x_t x0, const disp_x_t x1, const disp_y_t y) {
    // preconditions: y must be in current page, 0 <= y < page_height().
#ifdef RUNTIME_CHECKS
    if (x1 < x0 || x0 >= DISPLAY_WIDTH || x1 >= DISPLAY_WIDTH || y >= page_height()) {
        trace("outside of bounds");
        return;
    }
#endif
    uint8_t* buffer = display_buffer(x0, y);
    if (x0 & 1) {
        // handle half block at the start
        set_block_right(color, *buffer);
        ++buffer;
        ++x0;
    }
    for (disp_x_t x = x0; x < x1; x += 2) {
        set_block_both(color, *buffer);
        ++buffer;
    }
    if (!(x1 & 1)) {
        // handle half block at the end
        set_block_left(color, *buffer);
    }
}

void graphics_hline(disp_x_t x0, disp_x_t x1, const disp_y_t y) {
#ifdef RUNTIME_CHECKS
    if (x0 >= DISPLAY_WIDTH || x1 >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        trace("outside of bounds");
        return;
    }
#endif
    if (y < display_page_ystart || y > display_page_yend) {
        return; // completely out of page
    }
    if (x0 > x1) {
        swap(uint8_t, x0, x1);
    }
    graphics_hline_fast(x0, x1, y - display_page_ystart);
}

void graphics_vline(disp_y_t y0, disp_y_t y1, const disp_x_t x) {
#ifdef RUNTIME_CHECKS
    if (y0 >= DISPLAY_HEIGHT || y1 >= DISPLAY_HEIGHT || x >= DISPLAY_WIDTH) {
        trace("outside of bounds");
        return;
    }
#endif
    if (y0 > y1) {
        swap(uint8_t, y0, y1);
    }
    if (y1 < display_page_ystart || y0 > display_page_yend) {
        return;  // completely out of page
    }
    y0 = y0 <= display_page_ystart ? 0 : y0 - display_page_ystart;
    y1 = y1 > display_page_yend ? page_height() - 1 : y1 - display_page_ystart;
    uint8_t* buffer = display_buffer(x, y0);
    if (x & 1) {
        // on the right side of blocks
        for (uint8_t y = y0; y <= y1; ++y) {
            set_block_right(color, *buffer);
            buffer += DISPLAY_NUM_COLS;
        }
    } else {
        // on the left side of blocks
        for (uint8_t y = y0; y <= y1; ++y) {
            set_block_left(color, *buffer);
            buffer += DISPLAY_NUM_COLS;
        }
    }
}

void graphics_line(disp_x_t x0, disp_y_t y0, disp_x_t x1, disp_y_t y1) {
#ifdef RUNTIME_CHECKS
    if (x0 >= DISPLAY_WIDTH || x1 >= DISPLAY_WIDTH ||
        y0 >= DISPLAY_HEIGHT || y1 >= DISPLAY_HEIGHT) {
        trace("outside of bounds");
        return;
    }
#endif
    if (x0 == x1) {
        graphics_vline(y0, y1, x0);
    } else if (y0 == y1) {
        graphics_hline(x0, x1, y0);
    } else {
        // https://en.wikipedia.org/wiki/Bresenham's_line_algorithm#All_cases
        // taken from: https://github.com/olikraus/u8g2/blob/7d1108521680fbb14147a867e0ea15769119b78b/csrc/u8g2_line.c
        // adapted to include out of page fast path.
        int8_t dx = (int8_t) ((int8_t) x0 - x1);
        int8_t dy = (int8_t) ((int8_t) y0 - y1);
        if (dx < 0) dx = (int8_t) -dx;
        if (dy < 0) dy = (int8_t) -dy;
        bool swapxy = false;
        if (dy > dx) {
            swapxy = true;
            swap(uint8_t, dx, dy);
            swap(uint8_t, x0, y0);
            swap(uint8_t, x1, y1);
        }
        if (x0 > x1) {
            swap(uint8_t, x0, x1);
            swap(uint8_t, y0, y1);
        }
        int8_t err = (int8_t) (dx >> 1);
        if (swapxy) {
            // octants 2, 3, 6, 7
            const int8_t ystep = y1 > y0 ? 1 : -1;
            for (; x0 <= x1 && x0 <= display_page_yend; ++x0) {
                if (x0 >= display_page_ystart) {
                    graphics_pixel_fast(y0, x0 - display_page_ystart);
                }
                err = (int8_t) (err - dy);
                if (err < 0) {
                    y0 += ystep;
                    err = (int8_t) (err + dx);
                }
            }
        } else if (y1 > y0) {
            // octants 1, 4
            if (y0 <= display_page_yend) {
                for (; x0 <= x1; ++x0) {
                    if (y0 >= display_page_ystart) {
                        graphics_pixel_fast(x0, y0 - display_page_ystart);
                    }
                    err = (int8_t) (err - dy);
                    if (err < 0) {
                        ++y0;
                        if (y0 > display_page_yend) {
                            break; // out of page
                        }
                        err = (int8_t) (err + dx);
                    }
                }
            }
        } else if (y0 >= display_page_ystart) {
            // octants 5, 8
            for (; x0 <= x1; ++x0) {
                if (y0 <= display_page_yend) {
                    graphics_pixel_fast(x0, y0 - display_page_ystart);
                }
                err = (int8_t) (err - dy);
                if (err < 0) {
                    --y0;
                    if (y0 < display_page_ystart) {
                        break; // out of page
                    }
                    err = (int8_t) (err + dx);
                }
            }
        }
    }
}

void graphics_rect(const disp_x_t x, const disp_y_t y, const uint8_t w, const uint8_t h) {
#ifdef RUNTIME_CHECKS
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT ||
        x + w > DISPLAY_WIDTH || y + h > DISPLAY_HEIGHT) {
        trace("outside of bounds");
        return;
    }
    if (w == 0 || h == 0) {
        trace("dimension cannot be zero.");
        return;
    }
#endif
    const uint8_t right = x + w - 1;
    const uint8_t bottom = y + h - 1;
    graphics_hline(x, right, y);
    graphics_hline(x, right, bottom);
    graphics_vline(y, bottom, x);
    graphics_vline(y, bottom, right);
}

void graphics_fill_rect(const disp_x_t x, const disp_y_t y, const uint8_t w, const uint8_t h) {
#ifdef RUNTIME_CHECKS
    if (x + w > DISPLAY_WIDTH || y + h > DISPLAY_HEIGHT) {
        trace("outside of bounds");
        return;
    }
    if (w == 0 || h == 0) {
        trace("dimension cannot be zero.");
        return;
    }
#endif
    uint8_t y1 = y + h;
    if (y + h <= display_page_ystart) {
        return;  // completely out of page
    }
    // get coordinates within current page
    const uint8_t y0 = y <= display_page_ystart ? 0 : y - display_page_ystart;
    y1 = y1 > display_page_yend ? page_height() : y1 - display_page_ystart;
    const uint8_t x1 = x + w - 1;
    for (uint8_t i = y0; i < y1; ++i) {
        graphics_hline_fast(x, x1, i);
    }
}

// The graphics_image_nbit functions draw a region of an image in current page.
// The region height is at most equals to the page height.
// `y` is a coordinate in the current page (with y=0 being display_page_ystart on display).
// `data` is a pointer in unified data space to the start of image data (can be derived from index).
// `image_width` is the width of the image in pixels, minus one.
//
// The functions decode image data sequentially until the top limit is reached in image,
// then start drawing when within left and right bounds, until bottom limit is reached.
// The decoder state is reset on an index boundary.
// `index_gran` can be set to 0 if image is not indexed.
//
// There are separate functions for raw and mixed encodings, for performance,
// with raw encoding being faster even when reading more data from external memory.

#ifndef GRAPHICS_NO_HORIZONTAL_IMAGE_REGION
#define within_region() (y_img >= top && x_img >= left && x_img <= right)
#else
#define within_region() (y_img >= top)
#endif

static void graphics_image_checks(disp_x_t x, disp_y_t y, uint8_t left, uint8_t top,
                                  uint8_t right, uint8_t bottom, uint8_t image_width) {
#ifdef RUNTIME_CHECKS
    if (x >= DISPLAY_WIDTH || y >= page_height() || left > image_width ||
        right > image_width || bottom < top || (bottom - top) >= page_height()) {
        trace("out of bounds");
        return;
    }
#endif //RUNTIME_CHECKS
}

#ifndef GRAPHICS_NO_1BIT_IMAGE

#ifndef GRAPHICS_NO_1BIT_RAW_IMAGE

static void graphics_image_1bit_raw(graphics_image_t data, const disp_x_t x, const disp_y_t y,
                                    const uint8_t left, const uint8_t top,
                                    const uint8_t right, const uint8_t bottom,
                                    const uint8_t image_width, const uint8_t index_gran) {
    graphics_image_checks(x, y, left, top, right, bottom, image_width);

    uint8_t buf[IMAGE_BUFFER_SIZE];

    // current coordinates within the image
    uint8_t x_img = 0;
    uint8_t y_img = 0;
    // current coordinates within the page
    uint8_t x_page = x;
    uint8_t y_page = y;

#ifndef GRAPHICS_NO_INDEXED_IMAGE
    uint8_t next_index_bound = index_gran;
#endif

    uint8_t* buffer = display_buffer(x_page, y_page);
    while (true) {
        // fill buffer
        data_read(data, sizeof buf, buf);
        data += sizeof buf;

        // draw all pixels in buffer
        uint8_t buf_pos = 0;
        while (buf_pos < sizeof buf) {
            uint8_t shift_reg = buf[buf_pos++];
            for (uint8_t i = 0; i < 8; ++i) {
                if (within_region()) {
                    if (shift_reg & 1) {
                        set_buffer_pixel(color, x_page, buffer);
                    } else if (x_page & 1) {
                        ++buffer;
                    }
                    ++x_page;
                }
                shift_reg >>= 1;
                if (x_img == image_width) {
                    // end of scan line, go to next line
                    x_img = 0;
                    if (y_img >= top) {
                        x_page = x;
                        if (y_img == bottom) {
                            // bottom of image reached.
                            return;
                        }
                        ++y_page;
                        buffer = display_buffer(x_page, y_page);
                    }
                    ++y_img;
#ifndef GRAPHICS_NO_INDEXED_IMAGE
                    if (y_img == next_index_bound) {
                        // Index bound reached, reset decoder state.
                        next_index_bound += index_gran;
                        break;
                    }
#endif //GRAPHICS_NO_INDEXED_IMAGE
                } else {
                    ++x_img;
                }
            }
        }
    }
}

#endif //GRAPHICS_NO_1BIT_RAW_IMAGE

#ifndef GRAPHICS_NO_1BIT_MIXED_IMAGE

static void graphics_image_1bit_mixed(graphics_image_t data, const disp_x_t x, const disp_y_t y,
                                      const uint8_t left, const uint8_t top,
                                      const uint8_t right, const uint8_t bottom,
                                      const uint8_t image_width, const uint8_t index_gran) {
    graphics_image_checks(x, y, left, top, right, bottom, image_width);

    uint8_t buf[IMAGE_BUFFER_SIZE];
    uint8_t buf_pos = sizeof buf;

    // current coordinates within the image
    uint8_t x_img = 0;
    uint8_t y_img = 0;
    // current coordinates within the page
    uint8_t x_page = x;
    uint8_t y_page = y;

#ifndef GRAPHICS_NO_INDEXED_IMAGE
    uint8_t next_index_bound = index_gran;
#endif

    uint8_t* buffer = display_buffer(x_page, y_page);
    while (true) {
        if (buf_pos == sizeof buf) {
            // image data buffer is empty, read more data.
            data_read(data, sizeof buf, buf);
            data += sizeof buf;
            buf_pos = 0;
        }

        const uint8_t byte = buf[buf_pos++];
        // number of pixels to draw depends on the MSB (if byte is raw or RLE)
        uint8_t pixels = byte & 0x80 ? (byte & 0x3f) + 8 : 7;
        uint8_t shift_reg = byte;
        while (pixels--) {
            if (within_region()) {
                if (shift_reg & 0x40) {
                    // 0x40 is both the color bit mask if byte is a RLE byte,
                    // and the next pixel mask if byte is raw byte.
                    set_buffer_pixel(color, x_page, buffer);
                } else if (x_page & 1) {
                    ++buffer;
                }
                ++x_page;
            }
            if (!(byte & 0x80)) {
                shift_reg <<= 1;
            }
            if (x_img == image_width) {
                // end of scan line, go to next line
                x_img = 0;
                if (y_img >= top) {
                    x_page = x;
                    if (y_img == bottom) {
                        // bottom of image reached.
                        return;
                    }
                    ++y_page;
                    buffer = display_buffer(x_page, y_page);
                }
                ++y_img;
#ifndef GRAPHICS_NO_INDEXED_IMAGE
                if (y_img == next_index_bound) {
                    // Index bound reached, reset decoder state. The encoder must make sure this
                    // never happens in a RLE sequence, only in a raw sequence.
#ifdef RUNTIME_CHECKS
                    if ((byte & 0x80) && pixels > 0) {
                        trace("decoding RLE on index bound");
                        return;
                    }
#endif //RUNTIME_CHECKS
                    next_index_bound += index_gran;
                    break;
                }
#endif //GRAPHICS_NO_INDEXED_IMAGE
            } else {
                ++x_img;
            }
        }
    }
}

#endif //GRAPHICS_NO_1BIT_MIXED_IMAGE

#endif //GRAPHICS_NO_1BIT_IMAGE

#ifndef GRAPHICS_NO_4BIT_IMAGE

#ifndef GRAPHICS_NO_4BIT_RAW_IMAGE

static void graphics_image_4bit_raw(graphics_image_t data, const disp_x_t x, const disp_y_t y,
                                    const uint8_t left, const uint8_t top,
                                    const uint8_t right, const uint8_t bottom,
                                    const uint8_t image_width, const uint8_t index_gran) {
    graphics_image_checks(x, y, left, top, right, bottom, image_width);

    uint8_t buf[IMAGE_BUFFER_SIZE];

    // current coordinates within the image
    uint8_t x_img = 0;
    uint8_t y_img = 0;
    // current coordinates within the page
    uint8_t x_page = x;
    uint8_t y_page = y;

#ifndef GRAPHICS_NO_INDEXED_IMAGE
    uint8_t next_index_bound = index_gran;
#endif

    uint8_t* buffer = display_buffer(x_page, y_page);
    while (true) {
        // fill buffer
        data_read(data, sizeof buf, buf);
        data += sizeof buf;

        // draw all pixels in buffer
        uint8_t buf_pos = 0;
        while (buf_pos < sizeof buf) {
            uint8_t byte = buf[buf_pos++];
            for (uint8_t i = 0; i < 2; ++i) {
                if (within_region()) {
                    const uint8_t c = byte & 0xf;
                    set_buffer_pixel(c, x_page, buffer);
                    byte = nibble_swap(byte);
                    ++x_page;
                }
                if (x_img == image_width) {
                    // end of scan line, go to next line
                    x_img = 0;
                    if (y_img >= top) {
                        x_page = x;
                        if (y_img == bottom) {
                            // bottom of image reached.
                            return;
                        }
                        ++y_page;
                        buffer = display_buffer(x_page, y_page);
                    }
                    ++y_img;
#ifndef GRAPHICS_NO_INDEXED_IMAGE
                    if (y_img == next_index_bound) {
                        // Index bound reached, reset decoder state:
                        // if i == 0, ignore second nibble and go to next byte.
                        next_index_bound += index_gran;
                        break;
                    }
#endif //GRAPHICS_NO_INDEXED_IMAGE
                } else {
                    ++x_img;
                }
            }
        }
    }
}

#endif //GRAPHICS_NO_4BIT_RAW_IMAGE

#ifndef GRAPHICS_NO_4BIT_MIXED_IMAGE

static void graphics_image_4bit_mixed(graphics_image_t data, const disp_x_t x, const disp_y_t y,
                                      const uint8_t left, const uint8_t top,
                                      const uint8_t right, const uint8_t bottom,
                                      const uint8_t image_width, const uint8_t index_gran) {
    graphics_image_checks(x, y, left, top, right, bottom, image_width);

    uint8_t buf[IMAGE_BUFFER_SIZE];
    uint8_t buf_pos = sizeof buf;

    // current coordinates within the image
    uint8_t x_img = 0;
    uint8_t y_img = 0;
    // current coordinates within the page
    uint8_t x_page = x;
    uint8_t y_page = y;

#ifndef GRAPHICS_NO_INDEXED_IMAGE
    uint8_t next_index_bound = index_gran;
#endif

    uint8_t raw_seq_length = 0;  // if decoding raw sequence, the number of blocks left (2 px/block), otherwise 0.
    uint8_t rle_seq_length = 0;  // if deocding rle sequence, the number of pixels left, otherwise 0.
    uint8_t rle_color;  // the high nibble has the color of the next rle sequence if HAS_RLE_COLOR is set.
    uint8_t raw_color;  // the high nibble has the color of the next raw sequence if HAS_RAW_COLOR is set.

    enum {
        HAS_RLE_COLOR = 1 << 0,
        HAS_RAW_COLOR = 1 << 1,
    };
    uint8_t state = 0;

    uint8_t* buffer = display_buffer(x_page, y_page);
    while (true) {
        if (buf_pos >= sizeof buf - 1) {
            // Image data buffer is empty, read more data.
            // The buffer is filled even when there's still one byte left,
            // because 2 bytes are read at once at the start of a sequence.
            if (buf_pos == sizeof buf - 1) {
                buf[0] = buf[sizeof buf - 1];
            }
            data_read(data, buf_pos, buf + (sizeof buf - buf_pos));
            data += buf_pos;
            buf_pos = 0;
        }

        // decode logic
        uint8_t curr_color;
        if (state & HAS_RAW_COLOR) {
            // decoding raw sequence: use second color in current byte.
            // note that at this point HAS_RLE_COLOR flag may be set.
            state &= ~HAS_RAW_COLOR;
            curr_color = nibble_swap(raw_color);
            --raw_seq_length;
        } else {
            uint8_t byte = buf[buf_pos++];
            if (raw_seq_length == rle_seq_length) {
                // (if both lengths are equal, this means both are zero)
                // start of sequence: this byte is a length byte, either raw or RLE.
                uint8_t seq_length = byte & 0x7f;
                if (byte & 0x80) {
                    // RLE byte (offset=3, but length is decremented only after 1 iteration, so +2.
                    // note that at this point HAS_RAW_COLOR flag is always cleared.
                    rle_seq_length = seq_length + 2;
                    if (state != 0) {
                        state = 0;
                        curr_color = nibble_swap(rle_color);
                    } else {
                        // no RLE color remaining, use next byte.
                        state = HAS_RLE_COLOR;
                        rle_color = buf[buf_pos++];
                        curr_color = rle_color;
                    }
                    goto draw;
                } else {
                    // raw byte (offset=1)
                    raw_seq_length = seq_length + 1;
                    byte = buf[buf_pos++];
                }
            }
            // decoding raw sequence: use first color in current byte.
            state |= HAS_RAW_COLOR;
            raw_color = byte;
            curr_color = raw_color;
            // note that buffer position is incremented when reading the first nibble in case
            // that the second nibble is never read because of index bound.
        }

draw:
        // draw logic
        curr_color &= 0xf;
        do {
            if (within_region()) {
                set_buffer_pixel(curr_color, x_page, buffer);
                ++x_page;
            }
            if (x_img == image_width) {
                // end of scan line, go to next line
                x_img = 0;
                if (y_img >= top) {
                    x_page = x;
                    if (y_img == bottom) {
                        // bottom of image reached.
                        return;
                    }
                    ++y_page;
                    buffer = display_buffer(x_page, y_page);
                }
                ++y_img;
#ifndef GRAPHICS_NO_INDEXED_IMAGE
                if (y_img == next_index_bound) {
                    // Index bound reached, reset decoder state. The encoder must make sure this
                    // never happens when within a raw or RLE sequence.
#ifdef RUNTIME_CHECKS
                    if ((rle_seq_length != 0 || raw_seq_length != 0) && !(state & HAS_RAW_COLOR)) {
                        // if there's one raw color left that's fine since the raw sequence
                        // length is actually twice the encoded value, and one nibble may be left
                        // unused on an index boundary (hence the !(state & HAS_RAW_COLOR)).
                        trace("index bound in middle of sequence");
                        return;
                    }
#endif //RUNTIME_CHECKS
                    state = 0;
                    raw_seq_length = 0;
                    next_index_bound += index_gran;
                }
#endif //GRAPHICS_NO_INDEXED_IMAGE
            } else {
                ++x_img;
            }
        } while (rle_seq_length-- > 0);
        rle_seq_length = 0;  // if it was already 0, undo overflow.
    }
}

#endif //GRAPHICS_NO_4BIT_MIXED_IMAGE

#endif //GRAPHICS_NO_4BIT_IMAGE

static void graphics_image_internal(graphics_image_t data, const disp_x_t x, const disp_y_t y,
                                    const uint8_t left, const uint8_t top, uint8_t right,
                                    uint8_t bottom, const bool full) {
#ifdef RUNTIME_CHECKS
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        trace("out of bounds");
        return;
    }
#endif

    // read image header and index header (even if not indexed)
    uint8_t header[IMAGE_HEADER_SIZE + IMAGE_INDEX_HEADER_SIZE];
    data_read(data, sizeof header, header);
    if (header[0] != IMAGE_SIGNATURE) {
        trace("invalid image signature");
        return;
    }
    const uint8_t flags = header[1];
    const uint8_t width = header[2];
    const uint8_t height = header[3];
#ifndef GRAPHICS_NO_INDEXED_IMAGE
    uint8_t index_gran = header[4];
    const uint8_t index_size = header[5];
#else
    uint8_t index_gran = 0;
#endif
    data += IMAGE_HEADER_SIZE;

    if (full) {
#ifndef GRAPHICS_NO_HORIZONTAL_IMAGE_REGION
        right = width;
#endif
        bottom = height;
    }

    if (y > display_page_yend || (y + (bottom - top)) < display_page_ystart) {
        // out page page, either completely before or after.
        return;
    }

#ifdef RUNTIME_CHECKS
#ifdef GRAPHICS_NO_HORIZONTAL_IMAGE_REGION
    right = width;
#endif
    if (x + right - left >= DISPLAY_WIDTH || y + bottom - top >= DISPLAY_HEIGHT) {
        trace("out of bounds");
        return;
    }
    if (left > right || top > bottom || right > width || bottom > height) {
        trace("region out of bounds");
        return;
    }
#ifndef GRAPHICS_NO_INDEXED_IMAGE
    if ((flags & IMAGE_FLAG_INDEXED) && index_gran == 0) {
        trace("index granularity == 0 in indexed image");
        return;
    }
#endif
#endif //RUNTIME_CHECKS

    // Find the image position and bounds for the current page
    uint8_t page_y;
    uint8_t page_top;
    uint8_t page_bottom;
    if (flags & IMAGE_FLAG_INDEXED) {
#ifndef GRAPHICS_NO_INDEXED_IMAGE
        // Image is indexed, we can skip parts of it that go before the current page or the
        // specified top coordinate.
        data += IMAGE_INDEX_HEADER_SIZE;
        data_ptr_t index_pos = data;
        data += index_size;

        uint8_t index_buf[IMAGE_INDEX_BUFFER_SIZE];
        uint8_t index_buf_pos = sizeof index_buf;

        // To use the index, it's necessary to iterate over all pages that the image was drawn on
        // to know how many rows were drawn on those pages, in order to find the first row to draw
        // on the current page.
        uint8_t page_start = 0;
        uint8_t page_end = max_page_height() - 1;
        uint8_t index_y = 0;
        uint8_t actual_top = top;
        while (true) {
            // If y is larger than page end, then the current page is the one on which image starts.
            if (y <= page_end) {
                // Get the Y coordinate of the index entry immediately before the actual top position.
                // at the same time, increment the data pointer to point to this data.
                const uint8_t bound = saturate_sub(actual_top, index_gran);
                if (bound > 0) {
                    // If bound == 0, the 1st index entry has already been read as part of header,
                    // so the data pointer is already at the right position, and index_y is already 0.
                    for (; index_y <= bound; index_y += index_gran) {
                        if (index_buf_pos == sizeof index_buf) {
                            // index buffer is empty, read more data.
                            data_read(index_pos, sizeof index_buf, index_buf);
                            index_pos += sizeof index_buf;
                            index_buf_pos = 0;
                        }
                        data += (uint16_t) index_buf[index_buf_pos++] + 1;
                    }
                }

                page_y = saturate_sub(y, page_start);
                page_top = actual_top - index_y;
                page_bottom = page_top + min((uint8_t) (page_end - page_start - page_y),
                                             (uint8_t) (bottom - actual_top));

                if (page_end == display_page_yend) {
                    break;
                }

                uint8_t actual_bottom = index_y + page_bottom;
                actual_top = actual_bottom + 1;
            }

            page_start += max_page_height();
            page_end += max_page_height();
            if (page_end >= DISPLAY_HEIGHT) {
                page_end = DISPLAY_HEIGHT - 1;
            }
        }
#elif defined(RUNTIME_CHECKS)
        trace("decoding support for indexed images is disabled");
        return;
#endif //GRAPHICS_NO_INDEXED_IMAGE
    } else {
#ifndef GRAPHICS_NO_UNINDEXED_IMAGE
        // Not indexed: the whole image data is iterated on for every page.
        const uint8_t first_y = max(display_page_ystart, y);
        page_y = saturate_sub(y, display_page_ystart);
        page_top = top + (first_y - y);
        page_bottom = page_top + display_page_yend - first_y;
        if (page_bottom > bottom || bottom < page_top) {
            page_bottom = bottom;
        }
        // set to zero to make sure the state of the decoder in never reset on index bound.
        index_gran = 0;
#elif defined(RUNTIME_CHECKS)
        trace("decoding support for unindexed images is disabled");
        return;
#endif //GRAPHICS_NO_UNINDEXED_IMAGE
    }

    // decode selected image data and draw image on page
    if (flags & IMAGE_FLAG_BINARY) {
#ifndef GRAPHICS_NO_1BIT_IMAGE

        if (flags & IMAGE_FLAG_RAW) {
#ifndef GRAPHICS_NO_1BIT_RAW_IMAGE
            graphics_image_1bit_raw(data, x, page_y,
                                    left, page_top, right, page_bottom,
                                    width, index_gran);
#elif defined(RUNTIME_CHECKS)
            trace("decoding support for 1-bit raw images is disabled");
#endif //GRAPHICS_NO_1BIT_RAW_IMAGE

        } else {

#ifndef GRAPHICS_NO_1BIT_MIXED_IMAGE
            graphics_image_1bit_mixed(data, x, page_y,
                                      left, page_top, right, page_bottom,
                                      width, index_gran);
#elif defined(RUNTIME_CHECKS)
            trace("decoding support for 1-bit mixed images is disabled");
#endif //GRAPHICS_NO_1BIT_MIXED_IMAGE
        }
#elif defined(RUNTIME_CHECKS)
        trace("decoding support for 1-bit images is disabled");

#endif //GRAPHICS_NO_1BIT_IMAGE
    } else {
#ifndef GRAPHICS_NO_4BIT_IMAGE

        if (flags & IMAGE_FLAG_RAW) {
#ifndef GRAPHICS_NO_4BIT_RAW_IMAGE
            graphics_image_4bit_raw(data, x, page_y,
                                    left, page_top, right, page_bottom,
                                    width, index_gran);
#elif defined(RUNTIME_CHECKS)
            trace("decoding support for 4-bit raw images is disabled");
#endif //GRAPHICS_NO_4BIT_RAW_IMAGE

        } else {

#ifndef GRAPHICS_NO_4BIT_MIXED_IMAGE
            graphics_image_4bit_mixed(data, x, page_y,
                                      left, page_top, right, page_bottom,
                                      width, index_gran);
#elif defined(RUNTIME_CHECKS)
            trace("decoding support for 4-bit mixed images is disabled");
#endif //GRAPHICS_NO_4BIT_MIXED_IMAGE

        }

#elif defined(RUNTIME_CHECKS)
        trace("decoding support for 1-bit images is disabled");
#endif //GRAPHICS_NO_4BIT_IMAGE

    }
}

void graphics_image(const graphics_image_t data, const disp_x_t x, const disp_y_t y) {
    graphics_image_internal(data, x, y, 0, 0, 0, 0, true);
}

#ifndef GRAPHICS_NO_HORIZONTAL_IMAGE_REGION
void graphics_image_region(graphics_image_t data, disp_x_t x, disp_y_t y,
                           uint8_t left, uint8_t top, uint8_t right, uint8_t bottom) {
    graphics_image_internal(data, x, y, left, top, right, bottom, false);
}
#else

void graphics_image_region(graphics_image_t data, disp_x_t x, disp_y_t y,
                           uint8_t top, uint8_t bottom) {
    graphics_image_internal(data, x, y, 0, top, 0, bottom, false);
}

#endif //GRAPHICS_NO_HORIZONTAL_IMAGE_REGION

void graphics_glyph(int8_t x, int8_t y, char c) {
#ifdef RUNTIME_CHECKS
    if (font.addr == 0) {
        trace("no font set");
        return;
    }
#endif

    int8_t curr_x = x;
    int8_t curr_y = (int8_t) (y - display_page_ystart);
    if (curr_y >= page_height()) {
        return;  // glyph fully out of page
    }

    // get the glyph position with glyph data.
    uint8_t pos = c;
    if (pos < FONT_RANGE0_START) {
        // 0x00-0x20
        return;
    }
    if (pos > FONT_RANGE0_END) {
        if (pos < FONT_RANGE1_START) {
            // 0x60-0x9f
            return;
        }
        // 0xa0-0xff
        pos -= FONT_RANGE1_START - FONT_RANGE0_LEN;
    } else {
        // 0x21-0x7f
        pos -= FONT_RANGE0_START;
    }
    if (pos > font.glyph_count) {
        // Not encoded by the font data. This is the defined path for the space symbol notably,
        // but should probably be reported for non-blank other symbols.
#ifdef RUNTIME_CHECKS
        if (!isspace(c) && !iscntrl(c)) {
            trace("unencoded character %#02x", c);
        }
#endif
        return;
    }

    // read all glyph data
    data_ptr_t addr = font.addr + pos * font.glyph_size;
    uint8_t buf[FONT_MAX_GLYPH_SIZE];
    data_read(addr, font.glyph_size, buf);

    uint8_t byte_pos = font.glyph_size - 1;
    uint8_t bits = 8;
    uint8_t line_left = font.width;

    // apply glyph offset
    uint16_t first_byte = buf[byte_pos];
    first_byte <<= font.offset_bits;
    curr_y = (int8_t) (curr_y + (first_byte >> 8));
    if (curr_y >= page_height()) {
        // Y offset brought top of glyph outside of page.
        return;
    }
    bits -= font.offset_bits;
    uint8_t byte = first_byte & 0xff;
    // decode remaining bits in first byte
    goto glyph_read;

    while (byte_pos--) {
        bits = 8;
        byte = buf[byte_pos];
glyph_read:
        while (bits--) {
            if ((byte & 0x80) && curr_x >= 0 && curr_y >= 0) {
                // Pixel is set and on display.
                graphics_pixel_fast(curr_x, curr_y);
            }
            --line_left;
            if (line_left == 0) {
                // End of glyph line, either naturally or because
                // we're past the right side of display. Start next line.
                line_left = font.width;
                curr_x = x;
                ++curr_y;
                if (curr_y >= page_height()) {
                    return;  // out of page or display
                }
            } else {
#ifdef __AVR__
                ++curr_x;  // will overflow to -128 on AVR
#else
                if (__builtin_add_overflow(curr_x, 1, &curr_x)) {
                    curr_x = INT8_MIN;
                }
#endif
            }
            byte <<= 1;
        }
    }
}

void graphics_text(int8_t x, const int8_t y, const char* text) {
#ifdef RUNTIME_CHECKS
    if (font.addr == 0) {
        trace("no font set");
        return;
    }
    if (x < -128 + font.width || y < -128 + font.line_spacing) {
        trace("position out of bounds");
        return;
    }
#endif
    if (y + font.height + font.offset_max < display_page_ystart || y > display_page_yend) {
        return;  // out of page
    }
    while (*text) {
        graphics_glyph(x, y, *text++);
        int8_t next_x;
        if (__builtin_add_overflow(x, font.width + GRAPHICS_GLYPH_SPACING, &next_x)) {
            return;
        }
        x = next_x;
    }
}

void graphics_text_wrap(const int8_t x, const int8_t y, const uint8_t wrap_x, const char* text) {
#ifdef RUNTIME_CHECKS
    if (font.addr == 0) {
        trace("no font set");
        return;
    }
    if (wrap_x > DISPLAY_WIDTH || wrap_x < x) {
        trace("wrap_x out of bounds");
        return;
    }
    if (x < -128 + 2 * font.width || y < -128 + font.line_spacing) {
        trace("position out of bounds");
        return;
    }
#endif
    int8_t curr_x = x;
    int8_t curr_y = y;
    if (y > display_page_yend) {
        return;  // text fully out of page
    }

    if (x + font.width > wrap_x) {
        // text width less than glyph width, no text can be drawn.
        return;
    }
    const uint8_t glyph_spacing = font.width + GRAPHICS_GLYPH_SPACING;

    const char* next_wrap_pos = 0;
    while (*text) {
        if (!next_wrap_pos) {
            // find the last space before text line wraps.
            int8_t glyph_end_x;
            bool overflow = __builtin_add_overflow(x, font.glyph_size, &glyph_end_x);
            const char* pos = text;
            while (true) {
                if (!*pos) {
                    // end of text, "wrap" past the end.
                    next_wrap_pos = pos;
                    break;
                } else if (*pos++ == ' ') {
                    next_wrap_pos = pos;
                } else if (overflow || glyph_end_x >= wrap_x) {
                    // exit loop if past the wrap guide, or past the right side of display.
                    // don't exit loop if character is a space, instead skip
                    // all spaces at the end of the line.
                    break;
                }
                overflow |= __builtin_add_overflow(glyph_end_x, glyph_spacing, &glyph_end_x);
            }
            if (!next_wrap_pos) {
                // no space within text line, wrap on last character
                next_wrap_pos = pos - 1;
            }
            if (curr_y + font.height + font.offset_max < display_page_ystart) {
                // text line doesn't appear on page, skip it entirely.
                text = next_wrap_pos;
            }
        } else if (text == next_wrap_pos) {
            // wrap line
            int8_t new_y;
            bool overflow = __builtin_add_overflow(curr_y, font.line_spacing, &new_y);
            if (overflow || new_y > display_page_yend) {
                return;  // out of page or display
            }
            curr_y = new_y;
            curr_x = x;
            next_wrap_pos = 0;
        } else {
            graphics_glyph(curr_x, curr_y, *text++);
            curr_x = (int8_t) (curr_x + glyph_spacing);  // never overflows
        }
    }
}

uint8_t graphics_text_width(const char* text) {
    if (!*text) {
        return 0;
    }
    uint8_t width = 0;
    const uint8_t glyph_spacing = font.width + GRAPHICS_GLYPH_SPACING;
    while (*text++) {
        width += glyph_spacing;
        if (width >= DISPLAY_WIDTH + GRAPHICS_GLYPH_SPACING) {
            // text width greater or equal to display width
            return DISPLAY_WIDTH;
        }
    }
    return width - GRAPHICS_GLYPH_SPACING;
}

uint8_t graphics_glyph_width(void) {
    return font.width;
}

uint8_t graphics_text_height(void) {
    return font.height;
}

uint8_t graphics_text_max_height(void) {
    return font.height + font.offset_max;
}
