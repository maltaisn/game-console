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
#include <assets.h>

#include <sys/main.h>

#include <sys/input.h>
#include <sys/display.h>
#include <sys/time.h>
#include <sys/power.h>

#include <sim/flash.h>
#include <sim/power.h>

#include <core/graphics.h>
#include <core/sound.h>
#include <core/sysui.h>

#include <stdio.h>

static systime_t last_move;
static uint8_t last_state;

static uint8_t x, y;
static bool binary;

void setup(void) {
#ifdef SIMULATION
    FILE* file = fopen("assets.dat", "r");
    flash_load_file(file);
    fclose(file);
#endif

    sound_set_tempo(encode_bpm_tempo(120));
    sound_set_volume(SOUND_VOLUME_2);
    sound_start(TRACKS_STARTED_ALL);

}

static const char* STATUS_NAMES[] = {
        [BATTERY_UNKNOWN] = "Unknown",
        [BATTERY_NONE] = "None",
        [BATTERY_CHARGING] = "Charging",
        [BATTERY_CHARGED] = "Charged",
        [BATTERY_DISCHARGING] = "Discharging",
};

static void draw(void) {
    display_first_page();
    do {
        graphics_clear(DISPLAY_COLOR_BLACK);
        graphics_set_color(DISPLAY_COLOR_WHITE);

        if (power_is_sleep_scheduled()) {
            sysui_battery_sleep();
        } else {
            if (binary) {
                graphics_image_region(data_flash(ASSET_IMG_TIGER_BIN), 0, 0, x, y, x + 127,
                                      y + 127);
            } else {
                graphics_image_region(data_flash(ASSET_IMG_TIGER), 0, 0, x, y, x + 127, y + 127);
            }

            graphics_set_font(data_flash(ASSET_FNT_FONT6X9));
            char buf[6];
            sprintf(buf, "%d%%", power_get_battery_percent());
            graphics_set_color(DISPLAY_COLOR_BLACK);
            graphics_text(10, 10, buf);

            graphics_set_font(data_flash(ASSET_FNT_FONT5X7));
            graphics_text(10, 20, STATUS_NAMES[power_get_battery_status()]);
        }

    } while (display_next_page());
}

void loop(void) {
    uint8_t curr_state = input_get_state();

    if (!sound_check_tracks(TRACKS_PLAYING_ALL)) {
        sound_load(ASSET_SOUND_MUSIC);
    }

    systime_t time = time_get();
    if (time - last_move > millis_to_ticks(10)) {
        if (!power_is_sleep_scheduled()) {
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
#ifdef SIMULATION
            if ((curr_state & BUTTON4) && !(last_state & BUTTON4)) {
                power_set_battery_status((power_get_battery_status() + 1) % (BATTERY_DISCHARGING + 1));
            }
            if ((curr_state & BUTTON0) && !(last_state & BUTTON0)) {
                uint8_t percent = power_get_battery_percent();
                if (percent != 0) {
                    power_set_battery_level(percent - 10);
                }
            }
#endif
            last_state = curr_state;
        }

        draw();
    }
}
