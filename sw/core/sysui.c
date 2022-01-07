
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

#include <core/graphics.h>

void sysui_battery_sleep(void) {
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_set_font(GRAPHICS_BUILTIN_FONT);
    graphics_text(30, 42, "LOW BATTERY LEVEL");
    graphics_text(33, 81, "SHUTTING DOWN...");
    graphics_set_color(11);
    graphics_rect(40, 52, 43, 24);
    graphics_rect(41, 53, 41, 22);
    graphics_fill_rect(84, 57, 4, 14);
    graphics_fill_rect(44, 56, 7, 16);
    graphics_set_color(DISPLAY_COLOR_BLACK);
    graphics_fill_rect(84, 59, 2, 10);
}
