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
#include <sys/display.h>
#include <sys/time.h>

#include <core/graphics.h>
#include <core/debug.h>

#include <sim/flash.h>

#include <stdio.h>
#include <inttypes.h>

void setup(void) {
#ifdef SIMULATION
    FILE* file = fopen("assets.dat", "rb");
    flash_load_file(0, file);
    fclose(file);
#endif
}

static void draw(void) {
    graphics_clear(DISPLAY_COLOR_BLACK);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_image(ASSET_IMAGE_TIGER128_MIXED, 0, 0);
}

void loop(void) {
    // drawing
    systime_t start = time_get();
    display_first_page();
    do {
        draw();
    } while (display_next_page());
    systime_t end = time_get();
    char buf[32];
    sprintf(buf, "time = %d ms\n", (uint16_t) ((end - start) * 1000 / 256));
    debug_print(buf);
}