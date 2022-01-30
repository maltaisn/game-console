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

#include <sim/display.h>
#include <sim/time.h>

#include <sys/display.h>

#include <core/trace.h>

#include <memory.h>
#include <stdio.h>
#include <png.h>

#ifndef SIMULATION_HEADLESS

#include <GL/glut.h>

#endif

disp_y_t display_page_ystart;
disp_y_t display_page_yend;
uint8_t display_page_height;

#ifdef SIMULATION_HEADLESS
#define lock_display_mutex()
#define unlock_display_mutex()
#else
static pthread_mutex_t display_mutex;
#define lock_display_mutex() (pthread_mutex_lock(&display_mutex))
#define unlock_display_mutex() (pthread_mutex_unlock(&display_mutex))
#endif

#define GUARD_BYTE 0xfc

static struct {
    uint8_t buffer[DISPLAY_SIZE];
    size_t buffer_size;
    uint8_t data[DISPLAY_SIZE];
    uint8_t max_page_height;
    uint8_t* data_ptr;
    bool enabled;
    bool internal_vdd_enabled;
    bool inverted;
    bool dimmed;
    uint8_t contrast;
    display_gpio_t gpio_mode;
} display;

void display_init(void) {
    display.max_page_height = ((uint8_t) ((DISPLAY_HEIGHT - 1) / DISPLAY_PAGES + 1));
    display.data_ptr = 0;
    display.internal_vdd_enabled = true;
    display.enabled = false;
    display.inverted = false;
    display.dimmed = false;
    display.contrast = DISPLAY_DEFAULT_CONTRAST;
    display.gpio_mode = DISPLAY_GPIO_OUTPUT_LO;

#ifndef SIMULATION_HEADLESS
    pthread_mutex_init(&display_mutex, 0);
#endif
}

void display_sleep(void) {
    display.internal_vdd_enabled = false;
}

void display_set_enabled(bool enabled) {
    display.enabled = enabled;
}

void display_set_inverted(bool inverted) {
    display.inverted = inverted;
}

void display_set_contrast(uint8_t contrast) {
    display.contrast = contrast;
}

void display_set_dimmed(bool dimmed) {
    display.dimmed = dimmed;
}

bool display_is_dimmed(void) {
    return display.dimmed;
}

uint8_t display_get_contrast(void) {
    return display.contrast;
}

void display_set_gpio(display_gpio_t mode) {
    display.gpio_mode = mode;
}

void display_set_dc(void) {
    // no-op
}

void display_clear_dc(void) {
    // no-op
}

void display_set_reset(void) {
    // no-op
}

void display_clear_reset(void) {
    // no-op
}

void display_clear(disp_color_t color) {
    lock_display_mutex();
    memset(display.data, color | color << 4, DISPLAY_SIZE);
    unlock_display_mutex();
}

void display_first_page(void) {
    lock_display_mutex();

    display_page_ystart = 0;
    display_page_yend = max_page_height() - 1;
    display_page_height = max_page_height();
    display.buffer_size = display_page_height * DISPLAY_NUM_COLS;
    display.data_ptr = display.data;

    // initialize guard bytes
    // the display buffer is the same size as the display to accomodate varying page height,
    // so fsanitize won't catch buffer overflows if buffer is written beyond its set size.
    memset(display.buffer + display.buffer_size, GUARD_BYTE,
           sizeof display.buffer - display.buffer_size);
}

bool display_next_page(void) {
    if (display.data_ptr == 0) {
        trace("next page called before first page");
        return false;
    }

    // check guard
    for (size_t i = display.buffer_size; i < DISPLAY_SIZE; ++i) {
        if (display.buffer[i] != GUARD_BYTE) {
            // guard byte smashed, show warning and equivalent position where it would be located.
            trace("data written beyond display buffer end at pos %zu (x=%zu, y=%zu)",
                  i, i % DISPLAY_NUM_COLS * 2, i / DISPLAY_NUM_COLS);
            break;
        }
    }

    // copy buffer to main display data
    size_t bytes_left = (DISPLAY_SIZE - (display.data_ptr - display.data));
    if (display.buffer_size > bytes_left) {
        display.buffer_size = bytes_left;
    }
    memcpy(display.data_ptr, display.buffer, display.buffer_size);
    display.data_ptr += display.buffer_size;

    // reset guard for new buffer size
    memset(display.buffer + display.buffer_size,
           GUARD_BYTE, sizeof display.buffer - display.buffer_size);

    // update page bounds
    display_page_ystart += max_page_height();
    display_page_yend += max_page_height();
    if (display_page_yend >= DISPLAY_HEIGHT) {
        display_page_yend = DISPLAY_HEIGHT - 1;
    }
    display_page_height = display_page_yend - display_page_ystart + 1;

    bool has_next_page = display_page_ystart < DISPLAY_HEIGHT;
    if (!has_next_page) {
        display.data_ptr = 0;

        unlock_display_mutex();

        // sleep to simulate update delay
        // maximum FPS on game console is about 50
        time_sleep(20000);
    }
    return has_next_page;
}

uint8_t* display_buffer(disp_x_t x, disp_y_t y) {
    return &display.buffer[y * DISPLAY_NUM_COLS + x / 2];
}

void display_set_page_height(uint8_t height) {
    display.max_page_height = height;
}

uint8_t display_get_page_height(void) {
    return display.max_page_height;
}

#ifndef SIMULATION_HEADLESS

static float get_pixel_opacity(disp_color_t color) {
    if (display.inverted) {
        color = DISPLAY_COLOR_WHITE - color;
    }
    float color_factor = (float) color / DISPLAY_COLOR_WHITE;
    float effective_contrast = (float) display.contrast / (display.dimmed ? 2.0f : 1.0f);
    float contrast_factor = effective_contrast / DISPLAY_MAX_CONTRAST * 0.8f + 0.2f;
    if (contrast_factor > 1.0f) contrast_factor = 1.0f;
    return color_factor * contrast_factor;
}

#endif

void display_draw(void) {
#ifndef SIMULATION_HEADLESS
    if (!display.enabled || !display.internal_vdd_enabled ||
        display.gpio_mode != DISPLAY_GPIO_OUTPUT_HI) {
        // display OFF, internal VDD is disabled, or 15V regulator is disabled, nothing shown.
        return;
    }

    const float pixel_size = 1 - DISPLAY_PIXEL_GAP;

    glPushMatrix();
    glTranslatef(DISPLAY_PIXEL_GAP / 2, DISPLAY_PIXEL_GAP / 2, 0);
    lock_display_mutex();
    const uint8_t* data_ptr = display.data;
    for (disp_y_t row = 0; row < DISPLAY_NUM_ROWS; ++row) {
        glBegin(GL_QUADS);
        disp_x_t x = 0;
        for (disp_x_t col = 0; col < DISPLAY_NUM_COLS; ++col) {
            const uint8_t block = *data_ptr++;
            for (int nib = 0; nib < 2; ++nib, ++x) {
                const disp_color_t color = nib ? block >> 4 : block & 0xf;
                const float opacity = get_pixel_opacity(color);
                if (opacity != 0) {
                    glColor4f(DISPLAY_COLOR_R, DISPLAY_COLOR_G, DISPLAY_COLOR_B, opacity);
                    float xf = (float) x;
                    glVertex2f(xf, 0);
                    glVertex2f(xf, pixel_size);
                    glVertex2f(xf + pixel_size, pixel_size);
                    glVertex2f(xf + pixel_size, 0);
                }
            }
        }
        glEnd();
        glTranslatef(0, 1, 0);
    }
    unlock_display_mutex();
    glPopMatrix();
#endif //SIMULATION_HEADLESS
}

void display_save(FILE* file) {
    if (!file) {
        return;
    }

    lock_display_mutex();

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, file);
    png_set_IHDR(png_ptr, info_ptr, DISPLAY_WIDTH, DISPLAY_HEIGHT, 4, PNG_COLOR_TYPE_GRAY,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_COMPRESSION_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    png_byte row[DISPLAY_NUM_COLS];
    const uint8_t* data_ptr = display.data;
    for (size_t y = 0; y < DISPLAY_HEIGHT; ++y) {
        for (size_t col = 0; col < DISPLAY_NUM_COLS; ++col) {
            // libpng requires data to be in reverse order vs. what is stored.
            // the most significant nibble comes first.
            row[col] = *data_ptr << 4 | *data_ptr >> 4;
            ++data_ptr;
        }
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    unlock_display_mutex();
}

const uint8_t* display_data(void) {
    return display.data;
}
