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

#include <sys/display.h>
#include <sys/defs.h>
#include <sys/spi.h>

#include <avr/io.h>
#include <util/delay.h>

// Fundamental Command Table, p. 33
#define DISPLAY_SET_COLUMN_ADDR       0x15
#define DISPLAY_SET_ROW_ADDR          0x75
#define DISPLAY_SET_CONTRAST          0x81
#define DISPLAY_SET_REMAP             0xa0
#define DISPLAY_SET_START_LINE        0xa1
#define DISPLAY_SET_DISP_OFFSET       0xa2
#define DISPLAY_MODE_NORMAL           0xa4
#define DISPLAY_MODE_ALL_ON           0xa5
#define DISPLAY_MODE_ALL_OFF          0xa6
#define DISPLAY_MODE_INVERTED         0xa7
#define DISPLAY_SET_MUX_RATIO         0xa8
#define DISPLAY_FUNC_SEL_A            0xab
#define DISPLAY_FUNC_SEL_B            0xd5
#define DISPLAY_DISP_OFF              0xae
#define DISPLAY_DISP_ON               0xaf
#define DISPLAY_SET_PHASE_LENGTH      0xb1
#define DISPLAY_SET_CLK               0xb3
#define DISPLAY_SET_GPIO                  0xb5
#define DISPLAY_SET_PRECHARGE_VOLTAGE 0xbc
#define DISPLAY_SET_PRECHARGE_PERIOD  0xb6
#define DISPLAY_SET_GRAYSCALE         0xb8
#define DISPLAY_SET_GRAYSCALE_DEFAULT 0xb9
#define DISPLAY_SET_VCOM              0xbe

// Flags for set remap command, p. 27-29, 33
#define REMAP_COL (1 << 0)
#define REMAP_NIBBLE (1 << 1)
#define REMAP_VERTICAL (1 << 2)
#define REMAP_COM (1 << 4)
#define REMAP_COM_SPLIT (1 << 6)

#define FUNC_SEL_A_INTERNAL_VDD_DISABLE 0x00
#define FUNC_SEL_A_INTERNAL_VDD_ENABLE 0x01

#ifdef SIMULATION
disp_y_t display_page_ystart;
disp_y_t display_page_yend;
#endif

#ifdef DISPLAY_LAST_PAGE_SHORT
NO_INIT disp_y_t display_page_height;
#endif

// see core/graphics.c
#define nibble_swap(a) ((a) >> 4 | (a) << 4)
#define nibble_copy(a) ((a) >> 4 | nibble_swap(a))

static __attribute__((section(".disp_buf"))) uint8_t buffer[DISPLAY_BUFFER_SIZE];

enum {
    STATE_DIMMED = 1 << 0,
};

static NO_INIT uint8_t display_state;
static NO_INIT uint8_t display_contrast;

// Initialization sequence, see datasheet and examples
// OLED display model number is ER-OLED015-3, with a SSD1327 controller.
// Commented out lines correspond to values set at reset and thus not required to be set.
static const uint8_t INIT_SEQUENCE[] = {
        DISPLAY_DISP_OFF,
        // To appear upstraight, the remap value is 0x53, but the display is upside down (&=~0x10)
        DISPLAY_SET_REMAP, REMAP_COM_SPLIT,
//      DISPLAY_SET_START_LINE, 0x00,
//      DISPLAY_SET_DISP_OFFSET, 0x00,
//      DISPLAY_SET_MUX_RATIO, DISPLAY_NUM_ROWS - 1,
//      DISPLAY_MODE_NORMAL,
        DISPLAY_FUNC_SEL_A, FUNC_SEL_A_INTERNAL_VDD_ENABLE,
        // Values given by manufacturer + attempt to linearize grayscale
        DISPLAY_SET_PHASE_LENGTH, 0x37,
        // DISPLAY_SET_CLK, 0x00,
        DISPLAY_FUNC_SEL_B, 0x02,
        DISPLAY_SET_PRECHARGE_PERIOD, 0x0d,
        DISPLAY_SET_PRECHARGE_VOLTAGE, 0x03,
        DISPLAY_SET_VCOM, 0x07,
        DISPLAY_SET_CONTRAST, DISPLAY_DEFAULT_CONTRAST,
        DISPLAY_SET_GRAYSCALE, 0, 1, 2, 3, 4, 5, 7, 9, 11, 13, 15, 17, 20, 23, 26, 30,
//      DISPLAY_GPIO, 0x02,
};

static const uint8_t RESET_CURSOR_SEQUENCE[] = {
        DISPLAY_SET_COLUMN_ADDR, 0x00, DISPLAY_NUM_COLS - 1,
        DISPLAY_SET_ROW_ADDR, 0x00, DISPLAY_NUM_ROWS - 1,
};

static void reset_display(void) {
    VPORTC.OUT |= PIN2_bm;
    for (uint8_t i = 0; i < 2; ++i) {
        _delay_ms(1);
        PORTC.OUTTGL = PIN2_bm;
    }
    display_state = 0;
    display_contrast = DISPLAY_DEFAULT_CONTRAST;
}

static void write_data(uint16_t length, const uint8_t data[static length]) {
    spi_select_display();
    spi_transmit(length, data);
    spi_deselect_display();
}

static void write_command1(uint8_t byte0) {
    display_clear_dc();
    write_data(1, &byte0);
}

static void write_command2(uint8_t byte0, uint8_t byte1) {
    uint8_t data[2];
    data[0] = byte0;
    data[1] = byte1;
    display_clear_dc();
    write_data(2, data);
}

void display_init(void) {
    reset_display();
    display_clear_dc();
    write_data(sizeof INIT_SEQUENCE, INIT_SEQUENCE);
}

void display_sleep(void) {
    // disable VDD regulator
    write_command2(DISPLAY_FUNC_SEL_A, FUNC_SEL_A_INTERNAL_VDD_DISABLE);
}

void display_set_enabled(bool enabled) {
    write_command1(enabled ? DISPLAY_DISP_ON : DISPLAY_DISP_OFF);
}

void display_set_inverted(bool inverted) {
    write_command1(inverted ? DISPLAY_MODE_INVERTED : DISPLAY_MODE_NORMAL);
}

static void display_set_contrast_internal(uint8_t contrast) {
    write_command2(DISPLAY_SET_CONTRAST, contrast);
}

void display_set_contrast(uint8_t contrast) {
    if (contrast == display_contrast) {
        return;
    }
    display_contrast = contrast;
    if (display_state & STATE_DIMMED) {
        contrast /= 2;
    }
    display_set_contrast_internal(contrast);
}

uint8_t display_get_contrast(void) {
    return display_contrast;
}

void display_set_dimmed(bool dimmed) {
    uint8_t contrast = display_contrast;
    if (dimmed) {
        if (display_is_dimmed()) {
            // already dimmed
            return;
        }
        contrast /= 2;
        display_state |= STATE_DIMMED;
    } else {
        if (!display_is_dimmed()) {
            // already not dimmed.
            return;
        }
        display_state &= ~STATE_DIMMED;
    }
    display_set_contrast_internal(contrast);
}

bool display_is_dimmed(void) {
    return (display_state & STATE_DIMMED) != 0;
}

void display_set_gpio(display_gpio_t mode) {
    write_command2(DISPLAY_SET_GPIO, mode);
}

void display_set_dc(void) {
    VPORTC.OUT |= PIN3_bm;
}

void display_clear_dc(void) {
    VPORTC.OUT &= ~PIN3_bm;
}

void display_set_reset(void) {
    VPORTC.OUT |= PIN2_bm;
}

void display_clear_reset(void) {
    VPORTC.OUT &= ~PIN2_bm;
}

static void reset_cursor(void) {
    display_clear_dc();
    write_data(sizeof RESET_CURSOR_SEQUENCE, RESET_CURSOR_SEQUENCE);
}

void display_clear(disp_color_t color) {
    // fill 256 bytes buffer with color to use for transfer
    color = nibble_copy(color);
    uint8_t i = 0;
    do {
        buffer[i++] = color;
    } while (i != 0);

    // write buffer until whole display has been cleared
    reset_cursor();
    display_set_dc();
    for (i = 0; i < DISPLAY_SIZE / 256; ++i) {
        write_data(256, buffer);
    }
}

void display_first_page(void) {
    reset_cursor();
    display_page_ystart = 0;
    display_page_yend = max_page_height() - 1;
#ifdef DISPLAY_LAST_PAGE_SHORT
    display_page_height = max_page_height();
#endif
}

bool display_next_page(void) {
    display_set_dc();
    write_data(page_height() * DISPLAY_NUM_COLS, buffer);
    display_page_ystart += max_page_height();
    display_page_yend += max_page_height();
    if (display_page_yend >= DISPLAY_HEIGHT) {
        display_page_yend = DISPLAY_HEIGHT - 1;
    }
#ifdef DISPLAY_LAST_PAGE_SHORT
    display_page_height = display_page_yend - display_page_ystart + 1;
#endif
    return display_page_ystart < DISPLAY_HEIGHT;
}

uint8_t* display_buffer(disp_x_t x, disp_y_t y) {
    return &buffer[y * DISPLAY_NUM_COLS + x / 2];
}
