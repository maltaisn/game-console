
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

#ifndef SYS_DISPLAY_H
#define SYS_DISPLAY_H

#include <core/display.h>
#include <core/defs.h>

#include <stdint.h>
#include <stdbool.h>

// Number of columns (2 pixels per column)
#define DISPLAY_NUM_COLS 64
// Number of rows (1 pixel per row)
#define DISPLAY_NUM_ROWS 128
// Display RAM size
#define DISPLAY_SIZE ((uint16_t) (DISPLAY_NUM_COLS * DISPLAY_NUM_ROWS))

#define DISPLAY_DEFAULT_CONTRAST 0x7f

#ifdef SIMULATION
/** First Y coordinate for current page (inclusive), must not be changed directly. */
extern disp_y_t sys_display_page_ystart;
/** Last Y coordinate for current page (inclusive), must not be changed directly */
extern disp_y_t sys_display_page_yend;
#else
#include <avr/io.h>
#define sys_display_page_ystart (*((disp_y_t*) &GPIOR1))
#define sys_display_page_yend (*((disp_y_t*) &GPIOR2))
#endif

/**
 * Height of current page in pixels.
 * This can be accessed directly.
 */
extern uint8_t sys_display_curr_page_height;

/**
 * Maximum height for a display page in pixels.
 * All pages except the last one (and sometimes the last one too) have this height.
 * This can be accessed directly.
 */
extern uint8_t sys_display_page_height;

/**
 * The display page buffer. This buffer is assigned to a particular section and
 * its size is variable (may be different for the bootloader and the app).
 * `sys_display_buffer_at()` should always be used to access this buffer.
 */
extern uint8_t sys_display_buffer[];

extern uint8_t sys_display_state;
extern uint8_t sys_display_contrast;

// see core/display.h for documentation
void sys_display_set_inverted(bool inverted);

// see core/display.h for documentation
void sys_display_set_contrast(uint8_t contrast);

// see core/display.h for documentation
uint8_t sys_display_get_contrast(void);

/**
 * Returns true if the display is currently dimmed.
 */
bool sys_display_is_dimmed(void);

/**
 * Clear the whole display, not paged.
 * This also resets the computed average color.
 */
void sys_display_clear(disp_color_t c);

/**
 * Initialize display page size.
 * This must be called prior to `display_first_page` and before starting the app.
 */
void sys_display_init_page(uint8_t height);

/**
 * The display buffer used to write data for one page at a time before it is sent to the display.
 * The data in the buffer is in row-major order and only contains complete rows.
 * The first page is the topmost page and the first row of a page is the topmost row.
 * This functions returns a pointer to the display buffer at a page coordinate.
 * If X is odd, this returns a pointer to the pixel on the left, since there are two pixels per byte.
 * Page coordinate have the same x as display coordinates but a different y.
 */
uint8_t* sys_display_buffer_at(disp_x_t x, disp_y_t y);

#endif //SYS_DISPLAY_H
