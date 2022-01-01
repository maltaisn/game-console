
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

#include <sys/data.h>
#include <sys/display.h>

#ifdef GRAPHICS_CHECKS
#ifdef SIMULATION

#include <stdio.h>

#define check_message(str) puts(str)
#else
#define check_message(str) // no message
#endif //SIMULATION
#endif //GRAPHICS_CHECKS

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
#define FONT_MAX_LINE_SPACING 15

#define FONT_HEADER_SIZE 5

#define GLYPH_SPACING 1

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

#define set_block_left(block) ((block) = ((block) & 0xf0) | color)
#define set_block_right(block) ((block) = ((block) & 0xf) | color << 4)
#define set_block_both(block) ((block) = (color | color << 4))

#define swap(T, a, b) do { \
        T temp = (a);      \
        (a) = (b);         \
        (b) = temp;        \
    } while (0)

void graphics_set_color(disp_color_t c) {
#ifdef GRAPHICS_CHECKS
    if (c > DISPLAY_COLOR_WHITE) {
        check_message("graphics_set_color: invalid color");
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
    uint8_t buf[FONT_HEADER_SIZE];
    const uint8_t* header = data_read(f, FONT_HEADER_SIZE, buf);
    font.addr = f + FONT_HEADER_SIZE;
    font.glyph_count = header[0];
    font.glyph_size = header[1];
    font.width = (header[2] & 0xf) + 1;
    font.height = (header[2] >> 4) + 1;
    font.offset_bits = header[3] & 0xf;
    font.offset_max = header[3] >> 4;
    font.line_spacing = header[4];

#ifdef GRAPHICS_CHECKS
    if (font.glyph_count > FONT_MAX_GLYPHS) {
        check_message("graphics_set_font: font has too many glyphs");
    }
    if (font.offset_bits > FONT_MAX_Y_OFFSET_BITS) {
        check_message("graphics_set_font: font Y offset bits out of bounds");
    }
    if (font.glyph_size < FONT_MIN_GLYPH_SIZE || font.glyph_size > FONT_MAX_GLYPH_SIZE) {
        check_message("graphics_set_font: font glyph size out of bounds");
    }
    if (font.offset_max >= (1 << font.offset_bits)) {
        check_message("graphics_set_font: max offset not coherent with offset bits");
    }
    if (font.glyph_size > FONT_MAX_LINE_SPACING) {
        check_message("graphics_set_font: line spacing out of bounds");
    }
#endif
}

graphics_font_t graphics_get_font(void) {
    return font.addr - FONT_HEADER_SIZE;
}

void graphics_clear(disp_color_t c) {
    c = c | c << 4;
    for (uint16_t i = DISPLAY_BUFFER_SIZE; i-- > 0;) {
        display_buffer[i] = c;
    }
}

static void graphics_pixel_fast(const disp_x_t x, const disp_y_t y) {
#ifdef GRAPHICS_CHECKS
    if (x >= DISPLAY_WIDTH || y >= PAGE_HEIGHT) {
        check_message("graphics_pixel_fast: drawing outside bounds");
        return;
    }
#endif
    uint8_t* buffer = &display_buffer[y * DISPLAY_NUM_COLS + x / 2];
    if (x & 1) {
        set_block_right(*buffer);
    } else {
        set_block_left(*buffer);
    }
}

void graphics_pixel(const disp_x_t x, const disp_y_t y) {
#ifdef GRAPHICS_CHECKS
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        check_message("graphics_pixel: drawing outside bounds");
        return;
    }
#endif
    if (y >= display_page_ystart && y < display_page_yend) {
        graphics_pixel_fast(x, y - display_page_ystart);
    }
}

static void graphics_hline_fast(disp_x_t x0, const disp_x_t x1, const disp_y_t y) {
    // preconditions: y must be in current page, 0 <= y < PAGE_HEIGHT.
#ifdef GRAPHICS_CHECKS
    if (x0 >= DISPLAY_WIDTH || x1 >= DISPLAY_WIDTH || y >= PAGE_HEIGHT) {
        check_message("graphics_hline_fast: outside of bounds");
        return;
    }
#endif
    uint8_t* buffer = &display_buffer[y * DISPLAY_NUM_COLS + x0 / 2];
    if (x0 & 1) {
        // handle half block at the start
        set_block_right(*buffer);
        ++buffer;
        ++x0;
    }
    for (disp_x_t x = x0; x < x1; x += 2) {
        set_block_both(*buffer);
        ++buffer;
    }
    if (!(x1 & 1)) {
        // handle half block at the end
        set_block_left(*buffer);
    }
}

void graphics_hline(disp_x_t x0, disp_x_t x1, const disp_y_t y) {
#ifdef GRAPHICS_CHECKS
    if (x0 >= DISPLAY_WIDTH || x1 >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        check_message("graphics_hline: outside of bounds");
        return;
    }
#endif
    if (y < display_page_ystart || y >= display_page_yend) {
        return; // completely out of page
    }
    if (x0 > x1) {
        swap(uint8_t, x0, x1);
    }
    graphics_hline_fast(x0, x1, y - display_page_ystart);
}

void graphics_vline(disp_y_t y0, disp_y_t y1, const disp_x_t x) {
#ifdef GRAPHICS_CHECKS
    if (y0 >= DISPLAY_HEIGHT || y1 >= DISPLAY_HEIGHT || x >= DISPLAY_WIDTH) {
        check_message("graphics_vline: outside of bounds");
        return;
    }
#endif
    if (y0 > y1) {
        swap(uint8_t, y0, y1);
    }
    if (y1 < display_page_ystart) {
        return;  // completely out of page
    }
    y0 = y0 <= display_page_ystart ? 0 : y0 - display_page_ystart;
    y1 = y1 >= display_page_yend ? PAGE_HEIGHT : y1 - display_page_ystart;
    uint8_t* buffer = &display_buffer[y0 * DISPLAY_NUM_COLS + x / 2];
    if (x & 1) {
        // on the right side of blocks
        for (uint8_t y = y0; y <= y1; ++y) {
            set_block_right(*buffer);
            buffer += DISPLAY_NUM_COLS;
        }
    } else {
        // on the left side of blocks
        for (uint8_t y = y0; y <= y1; ++y) {
            set_block_left(*buffer);
            buffer += DISPLAY_NUM_COLS;
        }
    }
}

void graphics_line(disp_x_t x0, disp_y_t y0, disp_x_t x1, disp_y_t y1) {
#ifdef GRAPHICS_CHECKS
    if (x0 >= DISPLAY_WIDTH || x1 >= DISPLAY_WIDTH ||
        y0 >= DISPLAY_HEIGHT || y1 >= DISPLAY_HEIGHT) {
        check_message("graphics_vline: outside of bounds");
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
            for (; x0 <= x1 && x0 < display_page_yend; ++x0) {
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
            if (y0 < display_page_yend) {
                for (; x0 <= x1; ++x0) {
                    if (y0 >= display_page_ystart) {
                        graphics_pixel_fast(x0, y0 - display_page_ystart);
                    }
                    err = (int8_t) (err - dy);
                    if (err < 0) {
                        ++y0;
                        if (y0 >= display_page_yend) {
                            break; // out of page
                        }
                        err = (int8_t) (err + dx);
                    }
                }
            }
        } else if (y0 >= display_page_ystart) {
            // octants 5, 8
            for (; x0 <= x1; ++x0) {
                if (y0 < display_page_yend) {
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
#ifdef GRAPHICS_CHECKS
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT ||
        x + w > DISPLAY_WIDTH || x + h > DISPLAY_HEIGHT) {
        check_message("graphics_rect: drawing outside bounds");
        return;
    }
#endif
    const uint8_t right = x + w - 1;
    const uint8_t bottom = y + h - 1;
    graphics_hline(x + 1, right - 1, y);
    graphics_hline(x + 1, right - 1, bottom);
    graphics_vline(y, bottom, x);
    graphics_vline(y, bottom, right);
}

void graphics_fill_rect(const disp_x_t x, const disp_y_t y, const uint8_t w, const uint8_t h) {
#ifdef GRAPHICS_CHECKS
    if (x + w > DISPLAY_WIDTH || y + h > DISPLAY_HEIGHT) {
        check_message("graphics_fill_rect: outside of bounds");
        return;
    }
#endif
    uint8_t y1 = y + h;
    if (y + h <= display_page_ystart) {
        return;  // completely out of page
    }
    // get coordinates within current page
    const uint8_t y0 = y <= display_page_ystart ? 0 : y - display_page_ystart;
    y1 = y1 >= display_page_yend ? PAGE_HEIGHT : y1 - display_page_ystart;
    const uint8_t x1 = x + w - 1;
    for (uint8_t i = y0; i < y1; ++i) {
        graphics_hline_fast(x, x1, i);
    }
}

void graphics_circle(disp_x_t x, disp_y_t y, uint8_t r) {
    // TODO
}

void graphics_filled_circle(disp_x_t x, disp_y_t y, uint8_t r) {
    // TODO
}

void graphics_ellipse(disp_x_t x, disp_y_t y, uint8_t rx, uint8_t ry) {
    // TODO
}

void graphics_filled_ellipse(disp_x_t x, disp_y_t y, uint8_t rx, uint8_t ry) {
    // TODO
}

void graphics_image(data_ptr_t data, uint8_t w, uint8_t h) {
    // TODO
}

void graphics_image_part(data_ptr_t data, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    // TODO
}

void graphics_glyph(int8_t x, int8_t y, char c) {
    int8_t curr_x = x;
    int8_t curr_y = (int8_t) (y - display_page_ystart);
    if (curr_y >= PAGE_HEIGHT) {
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
        // not encoded.
        return;
    }

    // read all glyph data
    data_ptr_t addr = font.addr + pos * font.glyph_size;
    uint8_t data[FONT_MAX_GLYPH_SIZE];
    data_read(addr, font.glyph_size, data);

    uint8_t byte_pos = font.glyph_size - 1;
    uint8_t bits = 8;
    uint8_t line_left = font.width;

    // apply glyph offset
    uint16_t first_byte = data[byte_pos];
    first_byte <<= font.offset_bits;
    y = (int8_t) (y + (first_byte >> 8));
    bits -= font.offset_bits;
    uint8_t byte = first_byte & 0xff;
    // decode remaining bits in first byte
    goto glyph_read;

    while (byte_pos--) {
        bits = 8;
        byte = data[byte_pos];
        glyph_read:
        while (bits--) {
            if ((byte & 0x80) && curr_x >= 0 && curr_y >= 0) {
                // Pixel is set and on display.
                graphics_pixel_fast(curr_x, curr_y);
            }
            ++curr_x;
            --line_left;
            if (line_left == 0) {
                // End of glyph line, either naturally or because
                // we're past the right side of display. Start next line.
                line_left = font.width;
                curr_x = x;
                ++curr_y;
                if (curr_y >= PAGE_HEIGHT) {
                    return;  // out of page or display
                }
            }
            byte <<= 1;
        }
    }
}

void graphics_text(int8_t x, const int8_t y, const char* text) {
#ifdef GRAPHICS_CHECKS
    if (x < -128 + font.width || y < -128 + font.line_spacing) {
        check_message("graphics_text: position out of bounds");
        return;
    }
#endif
    if (y + font.height + font.offset_max < display_page_ystart || y > display_page_yend) {
        return;  // out of page
    }
    while (*text) {
        graphics_glyph(x, y, *text++);
        int8_t next_x = (int8_t) (x + font.width + GLYPH_SPACING);
        if (next_x < x) {
            return;
        }
        x = next_x;
    }
}

void graphics_text_wrap(const int8_t x, const int8_t y, const uint8_t wrap_x, const char* text) {
#ifdef GRAPHICS_CHECKS
    if (wrap_x > DISPLAY_WIDTH || wrap_x < x) {
        check_message("graphics_text_wrap: wrap_x out of bounds");
        return;
    }
    if (x < -128 + 2 * font.width || y < -128 + font.line_spacing) {
        check_message("graphics_text_wrap: position out of bounds");
        return;
    }
#endif
    int8_t curr_x = x;
    int8_t curr_y = y;
    if (y >= display_page_yend) {
        return;  // text fully out of page
    }

    if (x + font.width > wrap_x) {
        // text width less than glyph width, no text can be drawn.
        return;
    }
    const uint8_t glyph_spacing = font.width + GLYPH_SPACING;

    const char* next_wrap_pos = 0;
    while (*text) {
        if (!next_wrap_pos) {
            // find the last space before text line wraps.
            int8_t glyph_end_x = (int8_t) (x + font.glyph_size);
            const char* pos = text;
            while (true) {
                if (!*pos) {
                    // end of text, "wrap" past the end.
                    next_wrap_pos = pos;
                    break;
                } else if (*pos++ == ' ') {
                    next_wrap_pos = pos;
                } else if (glyph_end_x >= wrap_x || glyph_end_x < x) {
                    // exit loop if past the wrap guide, or past the right side of display.
                    // don't exit loop if character is a space, instead skip
                    // all spaces at the end of the line.
                    break;
                }
                glyph_end_x = (int8_t) (glyph_end_x + glyph_spacing);
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
            const int8_t new_y = (int8_t) (curr_y + font.line_spacing);
            if (new_y < y || new_y >= display_page_yend) {
                return;  // out of page or display
            }
            curr_y = new_y;
            curr_x = x;
            next_wrap_pos = 0;
        } else {
            graphics_glyph(curr_x, curr_y, *text++);
            curr_x = (int8_t) (curr_x + glyph_spacing);
        }
    }
}

uint8_t graphics_text_width(const char* text) {
    if (!*text) {
        return 0;
    }
    uint8_t width = 0;
    const uint8_t glyph_spacing = font.width + GLYPH_SPACING;
    while (*text++) {
        width += glyph_spacing;
        if (width >= DISPLAY_WIDTH + GLYPH_SPACING) {
            // text width greater or equal to display width
            return DISPLAY_WIDTH;
        }
    }
    return width - GLYPH_SPACING;
}

void graphics_text_num(int8_t x, int8_t y, int32_t num) {
    char buf[12];  // -2147483648\0
    buf[11] = '\0';
    char* ptr = &buf[11];
    bool negative = num < 0;
    if (negative) {
        num = -num;
    }
    do {
        *(--ptr) = (char) (num % 10 + '0');
        num /= 10;
    } while (num);
    if (negative) {
        *(--ptr) = '-';
    }
    graphics_text(x, y, ptr);
}
