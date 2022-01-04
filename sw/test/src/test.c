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

static systime_t last_move;
static uint8_t last_state;

static uint8_t x, y;
static bool binary;

void setup(void) {
#ifdef SIMULATION
    FILE* file = fopen("data/flash.dat", "r");
    flash_load_file(file);
    fclose(file);
#endif
}

void loop(void) {
    comm_receive();

    uint8_t curr_state = input_get_state();

    systime_t time = time_get();
    if (time - last_move > millis_to_ticks(10)) {
        last_move = time;

        if (curr_state & BUTTON1) {
            if (x > 0) {
                --x;
            }
        }
        if (curr_state & BUTTON2) {
            if (y > 0) {
                --y;
            }
        }
        if (curr_state & BUTTON3) {
            if (y < 128) {
                ++y;
            }
        }
        if (curr_state & BUTTON5) {
            if (x < 128) {
                ++x;
            }
        }
        if ((curr_state & BUTTON4) && !(last_state & BUTTON4)) {
            binary = !binary;
        }

        last_state = curr_state;

        display_first_page();
        do {
            graphics_clear(DISPLAY_COLOR_BLACK);
            graphics_set_color(DISPLAY_COLOR_WHITE);
            if (binary) {
                graphics_image_region(data_flash(0x60af), 0, 0, x, y, x + 127, y + 127);
            } else {
                graphics_image_region(data_flash(0), 0, 0, x, y, x + 127, y + 127);
            }
        } while (display_next_page());
    }
}
