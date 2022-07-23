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
#include <sys/spi.h>
#include <sys/defs.h>

enum {
    STATE_DIMMED = 1 << 0,
    STATE_AVERAGING_COLOR = 1 << 1,
};

#ifdef BOOTLOADER

#include <boot/defs.h>
#include <boot/display.h>
#include <boot/power.h>

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

// see core/graphics.c
#define nibble_swap(a) ((a) >> 4 | (a) << 4)
#define nibble_copy(a) ((a) >> 4 | nibble_swap(a))

__attribute__((section(".disp_buf"))) uint8_t sys_display_buffer[];

disp_y_t sys_display_page_height;
disp_y_t sys_display_curr_page_height;

uint8_t sys_display_state;
uint8_t sys_display_contrast;

// used for averaging display color once in a while.
// 24 bits of which 22 are used, lower 4 bits are always 0 and average is located at [21:18].
union {
    uint24_t value;
    uint8_t bytes[3];
} _color_accumulator;

#define color_accumulator_upper (_color_accumulator.bytes[2])
#define color_accumulator_full (_color_accumulator.value)

// Initialization sequence, see datasheet and examples
// OLED display model number is ER-OLED015-3, with a SSD1327 controller.
// Commented out lines correspond to values set at reset and thus not required to be set.
static const uint8_t _INIT_SEQUENCE[] = {
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
//      DISPLAY_SET_CONTRAST, DISPLAY_DEFAULT_CONTRAST,
        DISPLAY_SET_GRAYSCALE, 0, 1, 2, 3, 4, 5, 7, 9, 11, 13, 15, 17, 20, 23, 26, 30,
//      DISPLAY_GPIO, 0x02,
};

static const uint8_t _RESET_CURSOR_SEQUENCE[] = {
        DISPLAY_SET_COLUMN_ADDR, 0x00, DISPLAY_NUM_COLS - 1,
        DISPLAY_SET_ROW_ADDR, 0x00, DISPLAY_NUM_ROWS - 1,
};

ALWAYS_INLINE
static void sys_display_clear_dc(void) {
    VPORTC.OUT &= ~PIN3_bm;
}

ALWAYS_INLINE
static void sys_display_set_dc(void) {
    VPORTC.OUT |= PIN3_bm;
}

static void sys_display_reset(void) {
    VPORTC.OUT |= PIN2_bm;
    for (uint8_t i = 0; i < 2; ++i) {
        _delay_ms(1);
        PORTC.OUTTGL = PIN2_bm;
    }

    // reset state to remove dimmed status as it isn't restored.
    sys_display_state = 0;

    // resetting also resets internal contrast value but we won't set
    // sys_display_contrast here as we'd like to restore it afterwards.
}

static void sys_display_write_data(uint16_t length, const uint8_t data[static length]) {
    sys_spi_select_display();
    sys_spi_transmit(length, data);
    sys_spi_deselect_display();
}

static void sys_display_write_command1(uint8_t byte0) {
    sys_display_clear_dc();
    sys_display_write_data(1, &byte0);
}

static void sys_display_write_command2(uint8_t byte0, uint8_t byte1) {
    uint8_t data[2];
    data[0] = byte0;
    data[1] = byte1;
    sys_display_clear_dc();
    sys_display_write_data(2, data);
}

BOOTLOADER_NOINLINE
static void sys_display_set_contrast_internal(uint8_t contrast) {
    sys_display_write_command2(DISPLAY_SET_CONTRAST, contrast);
}

void sys_display_preinit(void) {
    // to avoid creating a .data section for the bootloader.
    sys_display_contrast = DISPLAY_DEFAULT_CONTRAST;
}

void sys_display_init(void) {
    sys_display_reset();
    sys_display_clear_dc();
    sys_display_write_data(sizeof _INIT_SEQUENCE, _INIT_SEQUENCE);
    // previous contrast was lost on reset, restore it.
    sys_display_set_contrast_internal(sys_display_contrast);
}

void sys_display_sleep(void) {
    // disable VDD regulator
    sys_display_write_command2(DISPLAY_FUNC_SEL_A, FUNC_SEL_A_INTERNAL_VDD_DISABLE);
}

void sys_display_set_enabled(bool enabled) {
    sys_display_write_command1(enabled ? DISPLAY_DISP_ON : DISPLAY_DISP_OFF);
}

void sys_display_set_inverted(bool inverted) {
    sys_display_write_command1(inverted ? DISPLAY_MODE_INVERTED : DISPLAY_MODE_NORMAL);
}

BOOTLOADER_NOINLINE
void sys_display_set_dimmed(bool dimmed) {
    uint8_t contrast = sys_display_contrast;
    if (dimmed) {
        if (sys_display_is_dimmed()) {
            // already dimmed
            return;
        }
        contrast /= 2;
        sys_display_state |= STATE_DIMMED;
    } else {
        if (!sys_display_is_dimmed()) {
            // already not dimmed.
            return;
        }
        sys_display_state &= ~STATE_DIMMED;
    }
    sys_display_set_contrast_internal(contrast);
}

void sys_display_set_gpio(sys_display_gpio_t mode) {
    sys_display_write_command2(DISPLAY_SET_GPIO, mode);
}

static void sys_display_reset_cursor(void) {
    sys_display_clear_dc();
    sys_display_write_data(sizeof _RESET_CURSOR_SEQUENCE, _RESET_CURSOR_SEQUENCE);
}

BOOTLOADER_NOINLINE
void sys_display_clear(disp_color_t color) {
    // fill 256 bytes buffer with color to use for transfer
    color = nibble_copy(color);
    uint8_t i = 0;
    do {
        sys_display_buffer[i++] = color;
    } while (i != 0);

    // write buffer until whole display has been cleared
    sys_display_reset_cursor();
    sys_display_set_dc();
    for (i = 0; i < DISPLAY_SIZE / 256; ++i) {
        sys_display_write_data(256, sys_display_buffer);
    }

    color_accumulator_upper = 0;
}

void sys_display_first_page(void) {
    sys_display_reset_cursor();

    sys_display_page_ystart = 0;
    sys_display_page_yend = sys_display_page_height - 1;
    sys_display_curr_page_height = sys_display_page_height;

    if (sys_power_should_compute_display_color()) {
        sys_display_state |= STATE_AVERAGING_COLOR;
        color_accumulator_full = 0;
    }
}

BOOTLOADER_NOINLINE
bool sys_display_next_page(void) {
    uint16_t data_length = sys_display_curr_page_height * DISPLAY_NUM_COLS;
    sys_display_set_dc();
    sys_display_write_data(data_length, sys_display_buffer);

    sys_display_page_ystart += sys_display_page_height;
    sys_display_page_yend += sys_display_page_height;

    if (sys_display_state & STATE_AVERAGING_COLOR) {
        // sum all pixel colors in this page
        const uint8_t* buf_ptr = sys_display_buffer;
        while (data_length--) {
            uint8_t block = *buf_ptr++;
            uint8_t block_swap = block >> 4 | block << 4;
            uint16_t block_sum_x16 = (block & 0xf0) + (block_swap & 0xf0);
            color_accumulator_full += block_sum_x16;
        }
        if (sys_display_page_ystart >= DISPLAY_HEIGHT) {
            // last page transmitted, exit averaging mode and notify power module.
            sys_display_state &= ~STATE_AVERAGING_COLOR;
            sys_power_on_display_color_computed();
        }
    }

    if (sys_display_page_yend >= DISPLAY_HEIGHT) {
        sys_display_page_yend = DISPLAY_HEIGHT - 1;
    }
    sys_display_curr_page_height = sys_display_page_yend - sys_display_page_ystart + 1;

    return sys_display_page_ystart < DISPLAY_HEIGHT;
}

uint8_t sys_display_get_average_color(void) {
    return (uint8_t) (color_accumulator_upper + 2) >> 2;
}

#else
void sys_display_set_contrast_internal(uint8_t);
#endif //BOOTLOADER

void sys_display_set_contrast(uint8_t contrast) {
    if (contrast == sys_display_contrast) {
        return;
    }
    sys_display_contrast = contrast;
    if (sys_display_state & STATE_DIMMED) {
        contrast /= 2;
    }
    sys_display_set_contrast_internal(contrast);
}

ALWAYS_INLINE
uint8_t sys_display_get_contrast(void) {
    return sys_display_contrast;
}

ALWAYS_INLINE
bool sys_display_is_dimmed(void) {
    return (sys_display_state & STATE_DIMMED) != 0;
}

ALWAYS_INLINE
void sys_display_init_page(uint8_t height) {
    sys_display_page_height = height;
}

ALWAYS_INLINE
uint8_t* sys_display_buffer_at(disp_x_t x, disp_y_t y) {
    return &sys_display_buffer[y * DISPLAY_NUM_COLS + x / 2];
}
