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
#include "sys/led.h"

#include <sim/power.h>

static bool reversed = false;

void setup(void) {
#ifdef SIMULATION
    power_set_battery_status(BATTERY_CHARGED);
#endif
}

void loop(void) {
    comm_receive();

    display_first_page();
    static disp_y_t i = 0;
    do {
        graphics_clear(DISPLAY_COLOR_BLACK);
        graphics_set_color(DISPLAY_COLOR_WHITE);
        if (reversed) {
            if (i >= DISPLAY_WIDTH) {
                graphics_line(0, DISPLAY_WIDTH - (i - DISPLAY_WIDTH) - 1, DISPLAY_WIDTH - 1, i - DISPLAY_WIDTH);
            } else {
                graphics_line(i, 0, DISPLAY_WIDTH - i - 1, DISPLAY_HEIGHT - 1);
            }
        } else {
            if (i >= DISPLAY_WIDTH) {
                graphics_line(DISPLAY_WIDTH - 1, i - DISPLAY_WIDTH, 0, DISPLAY_WIDTH - (i - DISPLAY_WIDTH) - 1);
            } else {
                graphics_line(DISPLAY_WIDTH - i - 1, DISPLAY_HEIGHT - 1, i, 0);
            }
        }
    } while (display_next_page());

    systime_t start = time_get();
    while (time_get() - start < millis_to_ticks(10));

    if (!(i & 0xf)) {
        led_toggle();
    }

    ++i;
    if (i == 0) {
        reversed = !reversed;
    }
}
