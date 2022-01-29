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

#include <assets.h>

#include <sys/main.h>
#include <sys/input.h>
#include <sys/display.h>
#include <sys/time.h>
#include <sys/power.h>

#include <core/graphics.h>
#include <core/sound.h>
#include <core/sysui.h>
#include <core/trace.h>
#include <core/dialog.h>
#include <core/debug.h>

#include <sim/flash.h>
#include <sim/power.h>

#include <stdio.h>
#include <inttypes.h>

void setup(void) {
#ifdef SIMULATION
    FILE* file = fopen("assets.dat", "rb");
    flash_load_file(0, file);
    fclose(file);
#endif
}

static const char* STATUS_NAMES[] = {
        "unknown",
        "none",
        "charging",
        "charged",
        "discharging",
};

static void draw(void) {
    graphics_clear(DISPLAY_COLOR_BLACK);

    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        sound_set_output_enabled(false);
        sysui_battery_sleep();
        return;
    }

    sysui_battery_overlay();

    char buf[32];
    graphics_set_font(ASSET_FONT_FONT5X7);
    sprintf(buf, "status = %s", STATUS_NAMES[power_get_battery_status()]);
    graphics_text(5, 10, buf);
    sprintf(buf, "level = %d%%", power_get_battery_percent());
    graphics_text(5, 20, buf);
    sprintf(buf, "voltage = %d mV", power_get_battery_voltage());
    graphics_text(5, 30, buf);
    sprintf(buf, "time = %" PRIu32, (uint32_t) time_get());
    graphics_text(5, 40, buf);
}

void loop(void) {
    systime_t time = time_get();
    while (time_get() - time < millis_to_ticks(20));

    // drawing
    display_first_page();
    do {
        draw();
    } while (display_next_page());
}

