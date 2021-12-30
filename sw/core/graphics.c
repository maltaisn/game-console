
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

#include <sys/display.h>

static disp_color_t color;

void graphics_set_color(disp_color_t c) {
    color = c;
}

disp_color_t graphics_get_color(void) {
    return color;
}

void graphics_clear(disp_color_t c) {
    c = c | c << 4;
    for (uint16_t i = DISPLAY_BUFFER_SIZE ; i-- > 0 ;) {
        display_buffer[i] = c;
    }
}

void graphics_pixel(const disp_x_t x, const disp_y_t y) {
    if (y >= display_page_ystart && y <= display_page_yend) {
        const uint8_t y0 = y - display_page_ystart;
        uint8_t* buffer = &display_buffer[(y0 * DISPLAY_WIDTH + x) / 2];
        if (x & 1) {
            *buffer = (*buffer & 0xf) | color << 4;
        } else {
            *buffer = (*buffer & 0xf0) | color;
        }
    }
}

void graphics_rect(const disp_x_t x0, const disp_y_t y, const uint8_t w, const uint8_t h) {
    // TODO
}

void graphics_fill_rect(const disp_x_t x, const disp_y_t y, const uint8_t w, const uint8_t h) {
    uint8_t y1 = y + h;
    if (y + h <= display_page_ystart) return;  // completely out of page
    // get coordinates within current page
    const uint8_t y0 = y <= display_page_ystart ? 0 : y - display_page_ystart;
    y1 = y1 >= display_page_yend ? PAGE_HEIGHT : y1 - display_page_ystart;
    const uint8_t x1 = x + w;

    uint8_t* buffer = &display_buffer[(y0 * DISPLAY_WIDTH + x) / 2];
    for (uint8_t j = y0 ; j < y1 ; ++j) {
        for (uint8_t i = x ; i < x1 ; ++i) {
            if (i & 1) {
                *buffer = (*buffer & 0xf) | color << 4;
                ++buffer;
            } else {
                *buffer = (*buffer & 0xf0) | color;
            }
        }
        buffer += (DISPLAY_WIDTH - w + 1) / 2;
    }
}

void graphics_hline(disp_x_t x1, disp_x_t x2, disp_y_t y) {
    // TODO
}

void graphics_vline(disp_y_t y0, disp_y_t y1, disp_x_t x) {
    // TODO
}

void graphics_line(disp_x_t x1, disp_y_t y0, disp_x_t x2, disp_y_t y1) {
    // TODO
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
