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

#include "core/graphics.h"
#include "core/comm.h"

#define WIDTH 24
#define HEIGHT 32

static disp_x_t x = (DISPLAY_WIDTH - WIDTH) / 2;
static disp_y_t y = (DISPLAY_HEIGHT - HEIGHT) / 2;
static disp_color_t color = DISPLAY_COLOR_WHITE;
static systime_t last_move;

void setup(void) {

}

void loop(void) {
    comm_receive();

    static uint8_t last_state;
    uint8_t curr_state = input_get_state();

    systime_t time = time_get();
    if (last_move - time > millis_to_ticks(100)) {
        last_move = time;

        if (!(last_state & BUTTON0) && (curr_state & BUTTON0)) {
            // change the color
            --color;
            if (color == 0xff) {
                color = DISPLAY_COLOR_WHITE;
            }
        }
        if (curr_state & BUTTON1) {
            if (x > 0) --x;
        }
        if (curr_state & BUTTON2) {
            if (y > 0) --y;
        }
        if (curr_state & BUTTON3) {
            if (y < DISPLAY_HEIGHT - HEIGHT) ++y;
        }
        if (curr_state & BUTTON5) {
            if (x < DISPLAY_WIDTH - WIDTH) ++x;
        }

        last_state = curr_state;

        display_first_page();
        do {
            graphics_clear(DISPLAY_COLOR_BLACK);
            graphics_set_color(color);
            graphics_fill_rect(x, y, WIDTH, HEIGHT);
        } while (display_next_page());
        last_state = curr_state;
    }
}
