
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

static disp_color_t color;
static graphics_font_t font;

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
    font = f;
}

graphics_font_t graphics_get_font(void) {
    return font;
}

void graphics_clear(disp_color_t c) {
    c = c | c << 4;
    for (uint16_t i = DISPLAY_BUFFER_SIZE; i-- > 0;) {
        display_buffer[i] = c;
    }
}

static void graphics_pixel_fast(const disp_x_t x, const disp_y_t y) {
#ifdef GRAPHICS_CHECKS
    if (x >= DISPLAY_WIDTH || y < display_page_ystart || y >= display_page_yend) {
        check_message("graphics_pixel_fast: drawing outside bounds");
        return;
    }
#endif
    const uint8_t y0 = y - display_page_ystart;
    uint8_t* buffer = &display_buffer[y0 * DISPLAY_NUM_COLS + x / 2];
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
        graphics_pixel_fast(x, y);
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
                    graphics_pixel_fast(y0, x0);
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
                        graphics_pixel_fast(x0, y0);
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
                    graphics_pixel_fast(x0, y0);
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

void graphics_text(disp_x_t x, disp_y_t y, const char* text) {
    // TODO
}

void graphics_text_num(disp_x_t x, disp_y_t y, uint32_t num) {
    // TODO
}
