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
#include <sim/display.h>

#include <memory.h>
#include <GL/glut.h>
#include <stdio.h>

uint8_t display_buffer[DISPLAY_BUFFER_SIZE];
disp_y_t display_page_ystart;
disp_y_t display_page_yend;

static uint8_t disp_data[DISPLAY_SIZE];
static uint8_t* disp_data_ptr;
static bool disp_enabled;
static bool disp_inverted;
static uint8_t disp_contrast;
static display_gpio_t disp_gpio_mode;

void display_init(void) {
    disp_enabled = false;
    disp_inverted = false;
    disp_contrast = DISPLAY_DEFAULT_CONTRAST;
    disp_gpio_mode = DISPLAY_GPIO_OUTPUT_LO;

    display_clear(DISPLAY_COLOR_BLACK);

    disp_data_ptr = 0;
}

void display_clear(disp_color_t color) {
    memset(disp_data, color | color << 4, DISPLAY_SIZE);
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

void display_set_gpio(display_gpio_t mode) {
    disp_gpio_mode = mode;
}

void display_first_page(void) {
    display_page_ystart = 0;
    display_page_yend = PAGE_HEIGHT;
    disp_data_ptr = disp_data;
}

bool display_next_page(void) {
    if (disp_data_ptr == 0) {
        puts("Next page called before first page");
        return false;
    }
    memcpy(disp_data_ptr, display_buffer, DISPLAY_BUFFER_SIZE);
    display_page_ystart += PAGE_HEIGHT;
    display_page_yend += PAGE_HEIGHT;
    disp_data_ptr += DISPLAY_BUFFER_SIZE;

    bool has_next_page = display_page_ystart < DISPLAY_HEIGHT;
    if (!has_next_page) {
        disp_data_ptr = 0;
    }
    return has_next_page;
}

static float get_pixel_opacity(disp_color_t color) {
    if (disp_inverted) {
        color = DISPLAY_COLOR_WHITE - color;
    }
    float color_factor = (float) color / DISPLAY_COLOR_WHITE;
    float contrast_factor = (float) disp_contrast / DISPLAY_MAX_CONTRAST * 0.8f + 0.2f;
    if (contrast_factor > 1.0f) contrast_factor = 1.0f;
    return color_factor * contrast_factor;
}

void display_draw(void) {
    if (!disp_enabled || disp_gpio_mode != DISPLAY_GPIO_OUTPUT_HI) {
        // display OFF or 15V regulator is disabled, nothing shown.
        return;
    }

    // copy display data to limit artifacts due to updating it while rendering.
    uint8_t data[DISPLAY_SIZE];
    memcpy(data, disp_data, DISPLAY_SIZE);

    const float pixel_size = 1 - DISPLAY_PIXEL_GAP;

    glPushMatrix();
    glTranslatef(DISPLAY_PIXEL_GAP / 2, DISPLAY_PIXEL_GAP / 2, 0);
    const uint8_t* data_ptr = data;
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
    glPopMatrix();
}
