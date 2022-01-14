
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

#include <stdint.h>
#include <stdbool.h>

#ifndef DISPLAY_BUFFER_SIZE
// Display buffer size can be increased to change update latency / RAM usage.
// The display buffer size must be a multiple of 64 to contain only complete rows.
// 1024 bytes: 8 pages of 128x16 px
// 2048 bytes: 4 pages of 128x32 px (default)
// 2752 bytes: 2 pages of 128x43 px, 1 page of 128x42 px
// 3072 bytes: 2 pages of 128x48 px, 1 page of 128x32 px
#define DISPLAY_BUFFER_SIZE 2048
#endif
#if (DISPLAY_BUFFER_SIZE / 64 * 64 != DISPLAY_BUFFER_SIZE)
#error "Display buffer size must be a multiple of 64"
#endif

// Number of pixels in width
#define DISPLAY_WIDTH 128
// Number of pixels in height
#define DISPLAY_HEIGHT 128
// Number of columns (2 pixels per column)
#define DISPLAY_NUM_COLS 64
// Number of rows (1 pixel per row)
#define DISPLAY_NUM_ROWS 128
// Display RAM size
#define DISPLAY_SIZE ((uint16_t) (DISPLAY_NUM_COLS * DISPLAY_NUM_ROWS))
// Number of pages given buffer size.
#define DISPLAY_PAGES ((uint8_t) (DISPLAY_SIZE / DISPLAY_BUFFER_SIZE))
// Height of each page in pixels.
#define PAGE_HEIGHT (DISPLAY_HEIGHT / DISPLAY_PAGES)

#define DISPLAY_COLOR_BLACK ((disp_color_t) 0)
#define DISPLAY_COLOR_WHITE ((disp_color_t) 15)

#define DISPLAY_DEFAULT_CONTRAST 0x7f

/** Display generic coordinate. */
typedef uint8_t disp_coord_t;
/** Display X coordinate. */
typedef disp_coord_t disp_x_t;
/** Display Y coordinate. */
typedef disp_coord_t disp_y_t;
/** Display "color" (grayscale level). */
typedef uint8_t disp_color_t;

/** First Y coordinate for current page (inclusive). */
extern disp_y_t display_page_ystart;
/** Last Y coordinate for current page (exclusive) */
extern disp_y_t display_page_yend;

typedef enum {
    DISPLAY_GPIO_DISABLE = 0b00,
    DISPLAY_GPIO_INPUT = 0b01,
    DISPLAY_GPIO_OUTPUT_LO = 0b10,
    DISPLAY_GPIO_OUTPUT_HI = 0b11,
} display_gpio_t;

/**
 * Initialize display. This resets the display and sets all registers through SPI commands.
 * The display RAM is initialized to zero but the buffer is NOT cleared.
 * The display is initially turned OFF and not inverted.
 */
void display_init(void);

/**
 * Disable internal VDD regulator to put display to sleep.
 * Re-initializing the display will turn it back on via display reset.
 */
void display_sleep(void);

/**
 * Turn the display on or off.
 */
void display_set_enabled(bool enabled);

/**
 * Set whether the display is inverted or not.
 */
void display_set_inverted(bool inverted);

/**
 * Set the display contrast. The default is DISPLAY_DEFAULT_CONTRAST.
 * Nothing is done if contrast is already at set value.
 */
void display_set_contrast(uint8_t contrast);

/**
 * Set whether screen dimming is enabled or not.
 */
void display_set_dimmed(bool dimmed);

/**
 * Get the display contrast.
 */
uint8_t display_get_contrast(void);

/**
 * Set the display GPIO mode.
 */
void display_set_gpio(display_gpio_t mode);

/**
 * Set D/C pin for display.
 */
void display_set_dc(void);

/**
 * Clear D/C pin for display.
 */
void display_clear_dc(void);

/**
 * Set reset pin for display.
 */
void display_set_reset(void);

/**
 * Clear reset pin for display.
 */
void display_clear_reset(void);

/**
 * Start updating display with the first page.
 * The display buffer is NOT cleared beforehand.
 */
void display_first_page(void);

/**
 * Flush display buffer and go to the next page.
 * The display buffer is NOT cleared afterwards.
 * If on the last page, this returns false, otherwise it returns true.
 */
bool display_next_page(void);

/**
 * The display buffer used to write data for one page at a time before it is sent to the display.
 * The data in the buffer is in row-major order and only contains complete rows.
 * The first page is the topmost page and the first row of a page is the topmost row.
 * This functions returns a pointer to the display buffer at a page coordinate.
 * If X is odd, this returns a pointer to the pixel on the left, since there are two pixels per byte.
 * Page coordinate have the same x as display coordinates but a different y.
 */
uint8_t* display_buffer(disp_x_t x, disp_y_t y);

#endif //SYS_DISPLAY_H
