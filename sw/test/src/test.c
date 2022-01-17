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


#include <sys/main.h>
#include <sys/input.h>
#include <sys/display.h>
#include <sys/time.h>

#include <core/graphics.h>
#include <core/sound.h>
#include <core/sysui.h>
#include <core/trace.h>
#include <core/dialog.h>
#include <core/debug.h>

#include <sim/flash.h>
#include <sim/power.h>

#include <stdio.h>

void setup(void) {

}

static void draw(void) {
    graphics_clear(DISPLAY_COLOR_BLACK);

    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        sound_set_output_enabled(false);
        sysui_battery_sleep();
        return;
    }

//    graphics_image(ASSET_IMAGE_TIGER128, 0, 0);
    sysui_battery_overlay();
}

void loop(void) {
    systime_t time = time_get();
    while (time_get() - time < millis_to_ticks(200));

    // input
    uint8_t state = input_get_state();
#ifdef SIMULATION
    if (state & BUTTON0) {
        power_set_battery_status((power_get_battery_status() + 1) % (BATTERY_DISCHARGING + 1));
    } else if (state & BUTTON2) {
        uint8_t percent = power_get_battery_percent();
        if (percent < 100) {
            power_set_battery_percent(percent + 1);
        }
    } else if (state & BUTTON3) {
        uint8_t percent = power_get_battery_percent();
        if (percent > 0) {
            power_set_battery_percent(percent - 1);
        }
    }
#endif

    // drawing
    display_first_page();
    do {
        draw();
    } while (display_next_page());
}
