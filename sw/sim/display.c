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
#include <GL/glut.h>
#include <png.h>

#define GUARD_SIZE 8192

disp_y_t display_page_ystart;
disp_y_t display_page_yend;

static struct {
    uint8_t guard0[GUARD_SIZE];
    uint8_t buffer[DISPLAY_BUFFER_SIZE];
    uint8_t guard1[GUARD_SIZE];
} disp_buffer;

static uint8_t guard_byte;

pthread_mutex_t display_mutex;

static uint8_t disp_data[DISPLAY_SIZE];
static uint8_t* disp_data_ptr;
static bool disp_enabled;
static bool disp_internal_vdd_enabled;
static bool disp_inverted;
static bool disp_dimmed;
static uint8_t disp_contrast;
static display_gpio_t disp_gpio_mode;

void display_init(void) {
    disp_internal_vdd_enabled = true;
    disp_enabled = false;
    disp_inverted = false;
    disp_dimmed = false;
    disp_contrast = DISPLAY_DEFAULT_CONTRAST;
    disp_gpio_mode = DISPLAY_GPIO_OUTPUT_LO;

    disp_data_ptr = 0;

    pthread_mutex_init(&display_mutex, 0);
}

void display_sleep(void) {
    disp_internal_vdd_enabled = false;
}

void display_set_enabled(bool enabled) {
    disp_enabled = enabled;
}

void display_set_inverted(bool inverted) {
    disp_inverted = inverted;
}

void display_set_contrast(uint8_t contrast) {
    disp_contrast = contrast;
}

void display_set_dimmed(bool dimmed) {
    disp_dimmed = dimmed;
}

uint8_t display_get_contrast(void) {
    return disp_contrast;
}

void display_set_gpio(display_gpio_t mode) {
    disp_gpio_mode = mode;
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

void display_first_page(void) {
    display_page_ystart = 0;
    display_page_yend = PAGE_HEIGHT;
    disp_data_ptr = disp_data;
    pthread_mutex_lock(&display_mutex);

    // initialize guards (the guard byte changes every time)
    memset(&disp_buffer.guard0, guard_byte, GUARD_SIZE);
    memset(&disp_buffer.guard1, guard_byte, GUARD_SIZE);
}

bool display_next_page(void) {
    if (disp_data_ptr == 0) {
        puts("Next page called before first page");
        return false;
    }
    memcpy(disp_data_ptr, disp_buffer.buffer, DISPLAY_BUFFER_SIZE);
    display_page_ystart += PAGE_HEIGHT;
    display_page_yend += PAGE_HEIGHT;
    disp_data_ptr += DISPLAY_BUFFER_SIZE;

    bool has_next_page = display_page_ystart < DISPLAY_HEIGHT;
    if (!has_next_page) {
        disp_data_ptr = 0;

        // check guards
        for (size_t i = 0; i < GUARD_SIZE; ++i) {
            if (disp_buffer.guard0[i] != guard_byte) {
                trace("guard before display buffer smashed at pos %zu", i);
                break;
            }
            if (disp_buffer.guard1[i] != guard_byte) {
                trace("guard after display buffer smashed at pos %zu", i);
                break;
            }
        }
        ++guard_byte;

        pthread_mutex_unlock(&display_mutex);

        // sleep to simulate update delay
        // maximum FPS on game console is about 50
        time_sleep(20000);
    }
    return has_next_page;
}

uint8_t* display_buffer(disp_x_t x, disp_y_t y) {
    return &disp_buffer.buffer[y * DISPLAY_NUM_COLS + x / 2];
}

static float get_pixel_opacity(disp_color_t color) {
    if (disp_inverted) {
        color = DISPLAY_COLOR_WHITE - color;
    }
    float color_factor = (float) color / DISPLAY_COLOR_WHITE;
    float effective_contrast = (float) disp_contrast / (disp_dimmed ? 2.0f : 1.0f);
    float contrast_factor = effective_contrast / DISPLAY_MAX_CONTRAST * 0.8f + 0.2f;
    if (contrast_factor > 1.0f) contrast_factor = 1.0f;
    return color_factor * contrast_factor;
}

void display_draw(void) {
    if (!disp_enabled || !disp_internal_vdd_enabled || disp_gpio_mode != DISPLAY_GPIO_OUTPUT_HI) {
        // display OFF, internal VDD is disabled, or 15V regulator is disabled, nothing shown.
        return;
    }

    const float pixel_size = 1 - DISPLAY_PIXEL_GAP;

    glPushMatrix();
    glTranslatef(DISPLAY_PIXEL_GAP / 2, DISPLAY_PIXEL_GAP / 2, 0);
    pthread_mutex_lock(&display_mutex);
    const uint8_t* data_ptr = disp_data;
    for (disp_y_t row = 0 ; row < DISPLAY_NUM_ROWS ; ++row) {
        glBegin(GL_QUADS);
        disp_x_t x = 0;
        for (disp_x_t col = 0 ; col < DISPLAY_NUM_COLS ; ++col) {
            const uint8_t block = *data_ptr++;
            for (int nib = 0 ; nib < 2 ; ++nib, ++x) {
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
    pthread_mutex_unlock(&display_mutex);
    glPopMatrix();
}

void display_save(FILE* file) {
    if (!file) {
        return;
    }

    pthread_mutex_lock(&display_mutex);

    // libpng requires data to be in reverse order vs. what is stored.
    // the most significant nibble comes first.
    png_byte image[DISPLAY_SIZE];
    for (size_t i = 0; i < DISPLAY_SIZE; ++i) {
        image[i] = disp_data[i] << 4 | disp_data[i] >> 4;
    }
    png_bytep row_pointers[DISPLAY_HEIGHT];
    for (size_t i = 0; i < DISPLAY_HEIGHT; ++i) {
        row_pointers[i] = (png_bytep) &image[DISPLAY_NUM_COLS * i];
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, file);
    png_set_IHDR(png_ptr, info_ptr, DISPLAY_WIDTH, DISPLAY_HEIGHT, 4, PNG_COLOR_TYPE_GRAY,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_COMPRESSION_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    pthread_mutex_unlock(&display_mutex);
}
