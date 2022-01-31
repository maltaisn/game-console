
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

#include <core/sysui.h>
#include <core/graphics.h>
#include <core/utils.h>

#include <sys/power.h>

__attribute__((weak)) const uint8_t SYSUI_BATTERY_OUTLINE[] = {
        0xf1, 0x01, 0x09, 0x06, 0xc1, 0x20, 0x1c, 0x03, 0x40, 0x38, 0x07, 0x00, 0xc3, 0x00
};
__attribute__((weak)) const uint8_t SYSUI_BATTERY_ICONS[][SYSUI_BATTERY_ICON_SIZE] = {
        {0xf1, 0x01, 0x04, 0x02, 0x76, 0x5b, 0x40},  // unknown
        {0xf1, 0x01, 0x04, 0x02, 0x02, 0x50, 0x00},  // none
        {0xf1, 0x01, 0x04, 0x02, 0x04, 0x5a, 0x40},  // charging
        {0xf1, 0x01, 0x04, 0x02, 0x56, 0x5a, 0x40},  // charged
};

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

void sysui_battery_overlay(void) {
    battery_status_t status = power_get_battery_status();
    uint8_t percent = power_get_battery_percent();
    uint8_t width;
    if (status == BATTERY_DISCHARGING) {
        if (percent == 100) {
            width = 28;
        } else if (percent >= 10) {
            width = 24;
        } else {
            width = 20;
        }
    } else {
        width = 12;
    }

    // background & frame
    uint8_t left = 128 - width;
    graphics_set_color(DISPLAY_COLOR_BLACK);
    graphics_fill_rect(left + 1, 120, width - 1, 8);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_vline(120, 127, left);
    graphics_hline(left, 127, 119);

    graphics_image(data_mcu(SYSUI_BATTERY_OUTLINE), 118, 121);
    if (status == BATTERY_DISCHARGING) {
        graphics_set_font(GRAPHICS_BUILTIN_FONT);
        char buf[4];
        graphics_text((int8_t) (130 - width), 122, uint8_to_str(buf, percent));
        graphics_glyph(114, 122, '%');
        if (percent >= 17) {
            graphics_fill_rect(120, 123, percent / 17, 3);
        }
    } else {
        graphics_image(data_mcu(SYSUI_BATTERY_ICONS[status]), 120, 123);
    }
}
