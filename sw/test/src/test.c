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

#include <test.h>

#include <sys/main.h>

#include "sys/input.h"
#include "sys/display.h"
#include "sys/time.h"

#include "sim/flash.h"

#include "core/graphics.h"
#include "core/comm.h"

static const char TEXT[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nullam "
                           "fermentum erat ut imperdiet blandit. Vivamus facilisis, risus in "
                           "semper tincidunt, lorem orci ullamcorper purus, sed viverra lacus "
                           "arcu id ex. Nulla facilisi. Aliquam ac est tempor enim eleifend "
                           "gravida eget at nibh. Aenean vel egestas nunc.";

static int8_t x = 10;
static int8_t y = 10;
static systime_t last_move;

void setup(void) {
#ifdef SIMULATION
    FILE* file = fopen("data/font/u8g2_font_6x10_tf_ext.dat", "r");
    flash_load_file(file);
    fclose(file);
#endif
}

void loop(void) {
    comm_receive();

    uint8_t curr_state = input_get_state();

    systime_t time = time_get();
    if (time - last_move > millis_to_ticks(30)) {
        last_move = time;

        if (curr_state & BUTTON1) {
            --x;
            printf("x = %d\n", x);
        }
        if (curr_state & BUTTON2) {
            --y;
            printf("y = %d\n", y);
        }
        if (curr_state & BUTTON3) {
            ++y;
            printf("y = %d\n", y);
        }
        if (curr_state & BUTTON5) {
            ++x;
            printf("x = %d\n", x);
        }

        display_first_page();
        do {
            graphics_clear(DISPLAY_COLOR_BLACK);
            graphics_set_color(DISPLAY_COLOR_WHITE);
            graphics_set_font(data_flash(0x000000));
            graphics_text_wrap(x, y, DISPLAY_WIDTH - 1, TEXT);
            graphics_set_color(7);
            if (x >= 0 && y >= 0) {
                graphics_pixel(x, y);
            }
            if (x > 0) {
                graphics_vline(0, DISPLAY_HEIGHT - 1, x - 1);
            }
            graphics_vline(0, DISPLAY_HEIGHT - 1, DISPLAY_WIDTH - 1);
        } while (display_next_page());
    }
}
