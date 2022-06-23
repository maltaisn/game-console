/*
 * Copyright 2022 Nicolas Maltais
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

#include <core/fpsmon.h>
#include <core/graphics.h>
#include <core/utils.h>
#include <core/time.h>
#include <core/trace.h>

#include <sys/display.h>

#define MONITOR_PERIOD millis_to_ticks(1000)
#define DISPLAY_PAGE_COUNT (uint8_t) ((DISPLAY_HEIGHT + DISPLAY_PAGE_HEIGHT - 1) / DISPLAY_PAGE_HEIGHT)

static uint8_t frames_last_second;
static uint8_t pages_this_second;
static systime_t start_time;

void fpsmon_draw(void) {
    ++pages_this_second;

    if (sys_display_page_ystart == 0) {
        // drawing the first display page, update monitor.
        const systime_t time = time_get();
        const systime_t diff = time - start_time;
        if (diff >= MONITOR_PERIOD) {
            // account for time difference that may be greater than monitor period and
            // calculate frames rendered last second in tenths of frames.
            frames_last_second = (uint24_t) ((uint16_t) pages_this_second * 10) * MONITOR_PERIOD /
                                 (uint16_t) (diff * DISPLAY_PAGE_COUNT);
            pages_this_second = 0;
            start_time = time;
        }
    }

    graphics_set_color(DISPLAY_COLOR_BLACK);
    graphics_fill_rect(0, 122, 16, 6);

    graphics_set_font(ASSET_FONT_3X5_BUILTIN);
    graphics_set_color(DISPLAY_COLOR_WHITE);

    // format frames in "XX.X" format
    uint8_t frames = frames_last_second;
    char buf[6];
    buf[1] = ' ';
    buf[5] = '\0';
    buf[4] = (char) ((frames % 10) + '0');
    frames /= 10;
    uint8_to_str(buf, frames);
    buf[3] = '.';

    graphics_text(0, 123, buf + 1);
}

