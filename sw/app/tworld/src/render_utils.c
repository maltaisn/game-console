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

#include "render_utils.h"
#include "render.h"
#include "defs.h"
#include "assets.h"
#include "tworld_level.h"
#include "game.h"

#include <core/trace.h>
#include <sys/display.h>

/**
 * Considering there's 4 bytes needed to transfer the command and address to the flash,
 * different buffer size result in the following efficiency:
 * - 8: 66.7%
 * - 16: 79.7%
 * - 24: 84.5% (chosen size for buffer)
 * - 32: 87.8%
 * No extra data is ever read for lines not drawn on current page.
 */
#define TILE_BUFFER_SIZE 3

#define TOP_TILE_COLS 6
#define BOTTOM_TILE_COLS 8

#define TOP_TILE_ROW_SIZE (uint8_t) (TOP_TILE_COLS + 2)  // 2 extra bytes for alpha flags
#define BOTTOM_TILE_ROW_SIZE BOTTOM_TILE_COLS

#define TOP_TILE_BUFFER_SIZE (TILE_BUFFER_SIZE * TOP_TILE_ROW_SIZE)
#define BOTTOM_TILE_BUFFER_SIZE (TILE_BUFFER_SIZE * BOTTOM_TILE_ROW_SIZE)

#ifdef SIMULATION
#define LD_X(ptr, val) ((val) = *(ptr))
#define ST_X_INC(ptr, val) (*(ptr)++ = (val))
#else
// avr-gcc fails to generate ld/st instructions with post-increment on the X register.
// instead, it generates adiw/sbiw instructions to do it, which is pretty inefficient.
// These macros force the compiler to use the correct instructions.
#define LD_X(ptr, val) asm volatile("ld %0, %a1" : "=r" (val), "=x" (ptr) : "1" (ptr))
#define ST_X_INC(ptr, val) asm volatile("st %a0+, %1" : "=x" (ptr) : "r" (val), "0" (ptr))
#endif

void uint16_to_str_zero_pad(char buf[static 4], uint16_t n) {
#ifdef RUNTIME_CHECKS
    if (n >= 1000) {
        trace("invalid value");
        buf[0] = '\0';
        return;
    }
#endif //RUNTIME_CHECKS

    buf[0] = '0';
    buf[1] = '0';
    buf[3] = '\0';

    char* ptr = &buf[3];
    do {
        *(--ptr) = (char) (n % 10 + '0');
        n /= 10;
    } while (n);
}

void format_time_left(char buf[static 4], time_left_t time) {
    if (time == TIME_LEFT_NONE) {
        buf[0] = '-';
        buf[1] = '-';
        buf[2] = '-';
        buf[3] = '\0';
    } else {
        uint16_to_str_zero_pad(buf, time_left_to_seconds(time));
    }
}

grid_pos_t get_camera_pos(const grid_pos_t pos) {
    int8_t start = (int8_t) (pos - GAME_MAP_SIZE / 2);
    if (start < 0) {
        return 0;
    }
    int8_t end = (int8_t) (pos + GAME_MAP_SIZE / 2);
    if (end >= GRID_WIDTH) {
        return GRID_WIDTH - GAME_MAP_SIZE;
    }
    return start;
}

/*
 * It would be ill-advised to use a core function such as `graphics_image_4bit_raw` to draw the
 * tiles, since these functions are relatively generic and not optimized for this specific case.
 *
 * `graphics_image_4bit_raw` at an estimated 61 cycles/px is too slow for this application,
 * our critical threshold to reach 16 FPS is 31 cycles/px (8M cycles budget).
 * As such, extra optimizations have been made over that function:
 *
 * - Tiles always X aligned on a 2-pixel block by being 16 pixels wide.
 * - Tile size fixed to 16x14.
 * - Extra data is never read from flash.
 * - No header and no indexing needed.
 * - -O3 optimization instead of -Os
 *
 * Worst case performance analysis (with 100% tiles with a non-block actor):
 * - 342k cycles: 21.4 kB transferred for reading tile images
 * - 172k cycles: draw all pixels
 * - 140k cycles: send buffer to the display
 * - 25k cycles: misc
 * TOTAL: 680k cycles, ~12 FPS on a 8M cycles budget.
 *
 * Average case analysis (25% actors):
 * - 208k cycles: 13.1 kB transferred for reading tile images
 * - 70k cycles: draw all pixels
 * - 140k cycles: send buffer to the display
 * - 25k cycles: misc
 * TOTAL: 443k cycles, ~18 FPS on a 8M cycles budget.
 */

static void draw_checks(const disp_x_t x, const disp_y_t y) {
#ifdef RUNTIME_CHECKS
    if (x & 1) {
        trace("X must be even");
        return;
    } else if (y <= sys_display_page_ystart - GAME_TILE_SIZE || y > sys_display_page_yend) {
        trace("tile drawn outside of page");
        return;
    }
#endif
}

// noinline to avoid inlining as part of -O3 optimization, which would give little benefit.
__attribute__((noinline))
AVR_OPTIMIZE void draw_bottom_tile(const disp_x_t x, const disp_y_t y, const tile_t tile) {
    draw_checks(x, y);

    // Bottom tiles can be animated by cycling through 2 variants, changing every 4 ticks.
    const uint8_t time_offset = (game.anim_state & 0x4) * 16;
    const uint8_t index = ASSET_TILESET_MAP_BOTTOM[(uint8_t) (tile + time_offset)];
#ifdef RUNTIME_CHECKS
    if (index == 0xff) {
        trace("invalid bottom tile");
        return;
    }
#endif

    flash_t addr = asset_tileset_bottom(index);
    uint8_t buf[BOTTOM_TILE_BUFFER_SIZE];
    uint8_t* buf_ptr;

    // limit Y range to current display page
    int8_t ystart = (int8_t) (y - sys_display_page_ystart);
    disp_y_t yend = ystart + GAME_TILE_SIZE;
    if (ystart < 0) {
        addr += (uint8_t) (BOTTOM_TILE_ROW_SIZE * (uint8_t) -ystart);
        ystart = 0;
    }
    if (yend > sys_display_curr_page_height) {
        yend = sys_display_curr_page_height;
    }

    uint8_t* disp_buf = sys_display_buffer_at(x, ystart);
    disp_y_t py = ystart;
    goto start;

    for (; py < yend; ++py) {
        if (buf_ptr == buf + BOTTOM_TILE_BUFFER_SIZE)
start:
            {
                // Fill buffer for number of rows left (or full buffer).
                // Each row in the buffer is 8 bytes and encodes the value for 2 pixels.
                uint8_t fill_rows = yend - py;
                uint8_t fill_bytes;
                if (fill_rows > TILE_BUFFER_SIZE) {
                    fill_bytes = BOTTOM_TILE_BUFFER_SIZE;
                } else {
                    fill_bytes = fill_rows * BOTTOM_TILE_ROW_SIZE;
                }
                flash_read(addr, fill_bytes, buf);
                addr += sizeof buf;
                buf_ptr = buf;
            }

        // Draw each pixel in the row.
        // Fully unrolled, this loop takes 2 cycles per pixel.
        // The tile data always has 0 on the first and last nibbles. The first block (of 2 pixels)
        // can be ORed with existing display data, and this isn't needed for the last block as we
        // assume the tiles will be drawn from left to right. Other blocks can be written directly.
        *disp_buf++ |= *buf_ptr++;
        for (uint8_t i = 0; i < (BOTTOM_TILE_ROW_SIZE - 1); ++i) {
            *disp_buf++ = *buf_ptr++;
        }

        disp_buf += DISPLAY_NUM_COLS - BOTTOM_TILE_COLS;
    }
}

// noinline to avoid inlining as part of -O3 optimization, which would give little benefit.
__attribute__((noinline))
AVR_OPTIMIZE void draw_top_tile(disp_x_t x, disp_y_t y, actor_t actor) {
    draw_checks(x, y);

    if (actor_is_block(actor)) {
        // block is a special case, the tile image is 14x14 and fully opaque,
        // in contrary to other actors which are 12x14 and partially transparent.
        draw_bottom_tile(x, y, TILE_BLOCK);
        return;
    }

    x += 2;

    const uint8_t index = ASSET_TILESET_MAP_TOP[actor];
#ifdef RUNTIME_CHECKS
    if (index == 0xff) {
        trace("invalid top tile");
        return;
    }
#endif

    flash_t addr = asset_tileset_top(index);
    uint8_t buf[TOP_TILE_BUFFER_SIZE];
    uint8_t* buf_ptr;

    // limit Y range to current display page
    int8_t ystart = (int8_t) (y - sys_display_page_ystart);
    disp_y_t yend = ystart + GAME_TILE_SIZE;
    if (ystart < 0) {
        addr += (uint8_t) (TOP_TILE_ROW_SIZE * (uint8_t) -ystart);
        ystart = 0;
    }
    if (yend > sys_display_curr_page_height) {
        yend = sys_display_curr_page_height;
    }

    uint8_t* disp_buf = sys_display_buffer_at(x, ystart);
    disp_y_t py = ystart;
    goto start;

    for (; py < yend; ++py) {
        if (buf_ptr == buf + TOP_TILE_BUFFER_SIZE)
start:
            {
                // Fill buffer for number of rows left (or full buffer).
                // Each row in the buffer is 8 bytes and encodes the value for 2 pixels + alpha.
                // The row content is as follows:
                // [alpha 0-5] [pixels 0-1] [pixels 2-3] [pixels 4-5]
                // [alpha 6-11] [pixels 6-7] [pixels 8-9] [pixels 10-11]
                uint8_t fill_rows = yend - py;
                uint8_t fill_bytes;
                if (fill_rows > TILE_BUFFER_SIZE) {
                    fill_bytes = TOP_TILE_BUFFER_SIZE;
                } else {
                    fill_bytes = fill_rows * TOP_TILE_ROW_SIZE;
                }
                flash_read(addr, fill_bytes, buf);
                addr += sizeof buf;
                buf_ptr = buf;
            }

        // Draw each pixel in the row.
        // Fully unrolled this loop takes about 10 cycles per pixel.
        for (uint8_t i = 0; i < 2; ++i) {
            // For each pixel, a bit in the alpha byte indicates its alpha value.
            // Each alpha byte has information for the next 6 pixels.
            uint8_t alpha = *buf_ptr++;
            for (uint8_t j = 0; j < TOP_TILE_COLS / 2; ++j) {
                const uint8_t color = *buf_ptr++;
                uint8_t new_color;
                LD_X(disp_buf, new_color);
                if (alpha & 0x1) {
                    new_color = (new_color & ~0xf) | (color & 0xf);
                }
                alpha >>= 1;
                if (alpha & 0x1) {
                    new_color = (new_color & ~0xf0) | (color & 0xf0);
                }
                alpha >>= 1;
                ST_X_INC(disp_buf, new_color);
            }
        }

        disp_buf += DISPLAY_NUM_COLS - TOP_TILE_COLS;
    }
}

#define TEXT_UTILS_WIDTH 5  // width of text drawn for utility text functions
#define TEXT_UTILS_HEIGHT 10  // height of text drawn for text functions + line spacing

typedef struct {
    uint8_t width;
    uint8_t leading_spaces;
    bool end_of_text;
} line_width_result_t;

/**
 * Find number of chars in a text line stored in flash, to be drawn in a box of a certain width.
 * Any leading and trailing spaces are not counted.
 * If no breaking space is found before the end of the line, line is split in middle of word.
 */
static line_width_result_t find_text_line_width(flash_t text, uint8_t width) {
    char buf[16];
    const char* ptr;

#ifdef RUNTIME_CHECKS
    if (width < TEXT_UTILS_WIDTH) {
        trace("text box width too small");
    }
#endif

    // subtract width of last character from box width.
    ++width;

    line_width_result_t result;
    result.end_of_text = false;
    result.leading_spaces = 0;

    uint8_t line_chars = 0;  // number of chars currently in the line.
    uint8_t line_width = 0;  // current line width in pixels.
    uint8_t skipped_spaces = 0;  // number of spaces since first space of current run was found.
    uint8_t last_wrap_pos = 0;  // position of first space in current run.

    goto start;
    while (line_width <= width) {
        if (ptr == buf + sizeof buf)
start:
            {
                flash_read(text, sizeof buf, buf);
                text += sizeof buf;
                ptr = buf;
            }
        char c = *ptr++;

        if (c == '\0') {
            last_wrap_pos = line_chars;
            result.end_of_text = true;
            break;
        } else if (c == '\n') {
            // The line ends here. Include the '\n' in the line, the character is invisible anyway.
            last_wrap_pos = line_chars + 1;
            break;
        } else if (c <= ' ') {
            if (line_chars == 0) {
                // leading space
                ++result.leading_spaces;
                continue;
            } else if (skipped_spaces == 0) {
                last_wrap_pos = line_chars;
            }
            ++skipped_spaces;
            continue;
        }

        ++skipped_spaces; // +1 for current non-space character.
        line_chars += skipped_spaces;
        line_width += skipped_spaces * (TEXT_UTILS_WIDTH + GRAPHICS_GLYPH_SPACING);
        skipped_spaces = 0;
    }

    // if no space was found (no wrap position), split the line on the first word.
    result.width = (last_wrap_pos == 0 ? line_chars - 1 : last_wrap_pos);
    return result;
}

void draw_text_wrap(const disp_x_t x, disp_y_t y, const uint8_t width,
                    const uint8_t max_lines, flash_t text, const bool centered) {
    graphics_set_font(ASSET_FONT_5X7);
    char buf[24];
    for (uint8_t i = 0; i < max_lines; ++i) {
        const line_width_result_t result = find_text_line_width(text, width);

        text += result.leading_spaces;
        flash_read(text, result.width, buf);
        buf[result.width] = '\0';
        text += result.width;

        disp_x_t px = x;
        if (centered) {
            const uint8_t line_width = (uint8_t) (result.width * (TEXT_UTILS_WIDTH + 1)) - 1;
            px += (uint8_t) (width - line_width) / 2;
        }
        graphics_text((int8_t) px, (int8_t) y, buf);

        if (result.end_of_text) {
            break;
        }

        y += TEXT_UTILS_HEIGHT;
    }
}

flash_t find_text_line_start(flash_t text, uint8_t width, uint8_t line) {
    line_width_result_t result;
    for (uint8_t i = 0; i < line; ++i) {
        result = find_text_line_width(text, width);
        text += result.width + result.leading_spaces;
    }
    return text;
}

uint8_t find_text_line_count(flash_t text, const uint8_t width) {
    // iterate over the text by finding the number of chars in each wrapped lines.
    line_width_result_t result;
    uint8_t lines = 0;
    do {
        result = find_text_line_width(text, width);
        text += result.width + result.leading_spaces;
        ++lines;
    } while (!result.end_of_text);
    return lines;
}

void draw_vertical_navigation_arrows(uint8_t top_y, uint8_t bottom_y) {
    graphics_set_color(ACTIVE_COLOR(game.pos_first_y > 0));
    graphics_image_1bit_mixed(ASSET_IMAGE_ARROW_UP, 62, top_y);
    graphics_set_color(ACTIVE_COLOR((int8_t) game.pos_first_y <=
                                    (int8_t) (game.pos_max_y - game.pos_shown_y)));
    graphics_image_1bit_mixed(ASSET_IMAGE_ARROW_DOWN, 62, bottom_y);
}
