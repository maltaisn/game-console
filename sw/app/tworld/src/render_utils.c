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

#include <core/trace.h>
#include <sys/display.h>

#include <string.h>

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

void format_time_left(char buf[static 4]) {
    buf[0] = '0';
    buf[1] = '0';
    buf[3] = '\0';

    if (tworld_is_level_untimed()) {
        buf[0] = '-';
        buf[1] = '-';
        buf[2] = '-';
        return;
    }

    char* ptr = &buf[3];
    uint16_t time_left = tworld_time_left_in_seconds();
    do {
        *(--ptr) = (char) (time_left % 10 + '0');
        time_left /= 10;
    } while (time_left);
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

AVR_OPTIMIZE void draw_bottom_tile(const disp_x_t x, const disp_y_t y, const tile_t tile) {
    draw_checks(x, y);

    flash_t addr = asset_tileset_bottom(ASSET_TILESET_MAP_BOTTOM[tile]);
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

AVR_OPTIMIZE void draw_top_tile(disp_x_t x, disp_y_t y, actor_t actor) {
    draw_checks(x, y);

    if (actor_is_block(actor)) {
        // block is a special case, the tile image is 14x14 and fully opaque,
        // in contrary to other actors which are 12x14 and partially transparent.
        draw_bottom_tile(x, y, TILE_BLOCK);
        return;
    }

    x += 2;

    flash_t addr = asset_tileset_top(ASSET_TILESET_MAP_TOP[actor]);
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
