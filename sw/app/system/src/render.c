
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

#include <render.h>
#include <assets.h>
#include <system.h>

#include <core/graphics.h>
#include <core/dialog.h>
#include <core/sysui.h>
#include <core/power.h>

#include <sys/power.h>
#include <sys/flash.h>
#include <sys/eeprom.h>

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define PROGRESS_BAR_WIDTH 116

#define SIZE_BOUND 1024

#define min(a, b) ((a) < (b) ? (a) : (b))

static const char* const SIZE_UNIT[] = {"", "K", "M"};

static const char* const BATTERY_STATUS_NAME[] = {
        "Unknown",
        "No battery",
        "Charging",
        "Charged",
        "Discharging",
};

static void draw_progress_bar(disp_y_t y, uint8_t height, uint8_t width) {
    if (width > 0) {
        graphics_set_color(12);
        graphics_fill_rect(6, y, width, height);
    }
    if (width < PROGRESS_BAR_WIDTH) {
        graphics_set_color(3);
        graphics_fill_rect(6 + width, y, 116 - width, height);
    }
}

static void draw_progress_bar_with_text(disp_y_t y, uint8_t height, uint8_t percent) {
    uint8_t width = percent * PROGRESS_BAR_WIDTH / 100;
    draw_progress_bar(y, height, width);

    // write percentage on one side of the middle of the bar
    char buf[5];
    uint8_t len = sprintf(buf, "%d%%", percent);
    int8_t percent_x;
    if (percent > 50) {
        percent_x = (int8_t) (5 + width - len * 6);
        graphics_set_color(DISPLAY_COLOR_BLACK);
    } else {
        percent_x = (int8_t) (8 + width);
        graphics_set_color(DISPLAY_COLOR_WHITE);
    }
    graphics_text(percent_x, (int8_t) (y + 2), buf);
}

static void draw_nav_arrows(uint8_t y) {
    graphics_set_color(state.position == 0 ? 4 : 12);
    graphics_image_1bit_mixed(ASSET_IMAGE_ARROW_UP, 61, y);
    graphics_set_color(state.position == state.max_position ? 4 : 12);
    graphics_image_1bit_mixed(ASSET_IMAGE_ARROW_DOWN, 61, 97);
}

/**
 * Format a size in bytes to a human readable quantity with units and 2 or 3 significant digits.
 * The length of the result is returned (at most 8 chars including nul terminator).
 * The result is always floored. The maximum supported size is 16 MB.
 * Examples:
 *   999     --> "999 B".
 *   52689   --> "51.5 kB".
 *   1042954 --> "0.99 MB".
 */
static uint8_t format_readable_size(char buf[static 8], uint24_t size) {
    uint8_t scale;
    uint16_t int_part;
    if (size < 1000) {
        scale = 0;
        int_part = size;
    } else {
        scale = 1;
        while (size > (uint24_t) SIZE_BOUND * 1000) {
            size /= SIZE_BOUND;
            ++scale;
        }
        int_part = size / SIZE_BOUND;
    }

    const char* unit = SIZE_UNIT[scale];
    if (scale == 0 || int_part >= 100) {
        // no decimal separator
        return sprintf(buf, "%" PRIu16 " %sB", int_part, unit);
    } else {
        uint8_t int_part_low = (uint8_t) int_part;
        uint8_t frac_part = size % SIZE_BOUND * 100 / SIZE_BOUND;
        const char* zero_str = "";
        if (int_part_low >= 10) {
            frac_part /= 10;
        } else if (frac_part < 10) {
            zero_str = "0";
        }
        return sprintf(buf, "%" PRIu8 ".%s%" PRIu8 " %sB", int_part_low, zero_str, frac_part, unit);
    }
}

static void draw_apps_overlay(void) {
    draw_nav_arrows(17);
    graphics_set_font(ASSET_FONT_5X7);

    if (state.flash_usage.size == 0) {
        // no apps installed
        graphics_set_color(10);
        graphics_text(25, 54, "No apps found");
        return;
    }

    char buf[16];
    const app_flash_t* app = &state.flash_index[state.position];

    // name
    system_get_app_name(app->index, buf);
    uint8_t name_len = strlen(buf);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_text(6, 23, buf);
    // total size
    graphics_set_color(12);
    format_readable_size(buf, app->app_size);
    graphics_text(56, 56, buf);
    // code size
    format_readable_size(buf, app->code_size);
    graphics_text(56, 66, buf);
    // eeprom size
    format_readable_size(buf, app->eeprom_size);
    graphics_text(56, 76, buf);
    // build date
    sprintf(buf, "%" PRIu16 "-%02" PRIu8 "-%02" PRIu8, (app->build_date >> 9) + 2020,
            (app->build_date >> 5) & 0xf, app->build_date & 0x1f);
    graphics_text(56, 86, buf);

    graphics_set_font(ASSET_FONT_3X5_BUILTIN);
    // author
    graphics_set_color(15);
    graphics_text(6, 35, "BY");
    system_get_app_author(app->index, buf);
    graphics_text(18, 35, buf);
    // app ID
    graphics_set_color(12);
    sprintf(buf, "(%" PRIu8 ")", app->id);
    graphics_text((int8_t) (8 + 6 * name_len), 25, buf);
    // app version
    uint8_t version_len = sprintf(buf, "V%" PRIu16, app->app_version);
    graphics_text(6, 44, buf);
    // boot version
    graphics_set_color(app->boot_version == BOOT_VERSION ? 12 : 6);
    sprintf(buf, "(BOOT V%" PRIu16 ")", app->boot_version);
    graphics_text((int8_t) (12 + 4 * version_len), 44, buf);

    graphics_set_color(15);
    graphics_text(6, 57, "TOTAL SIZE");
    graphics_text(6, 67, "CODE SIZE");
    graphics_text(6, 77, "EEPROM SIZE");
    graphics_text(6, 87, "BUILD DATE");
}

/**
 * Draw memory dialog overlay for memory with the `usage` struct.
 * `index` points to the start of the memory index, with the first member of each element being
 * a 1-byte app ID, followed by 3-byte member indicating the size in bytes.
 * `sizeof_index` indicates the size of the index array elements.
 * `total` is the memory size in 256 bytes blocks.
 */
static void draw_memory_overlay(const mem_usage_t* usage, const void* index,
                                uint8_t sizeof_index, uint16_t total_blocks) {
    char buf[16];
    uint24_t total = (uint24_t) total_blocks << 8;

    draw_nav_arrows(46);

    graphics_set_font(ASSET_FONT_5X7);
    draw_progress_bar_with_text(24, 11, (uint32_t) usage->total * 100 / total);

    // total usage / total available
    uint8_t usage_len = format_readable_size(buf, usage->total);
    int8_t usage_pos = (int8_t) (9 + usage_len * 6);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_text(6, 37, buf);
    graphics_set_color(10);
    graphics_glyph(usage_pos, 37, '/');
    format_readable_size(buf, total);
    graphics_text((int8_t) (usage_pos + 9), 37, buf);

    // app names in list
    uint8_t items_count = min(usage->size, MEMORY_APPS_PER_SCREEN);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    int8_t y = 50;
    for (uint8_t i = state.position, end = state.position + items_count; i < end; ++i) {
        uint8_t app_id = *((uint8_t*) index + sizeof_index * usage->index[i]);
        system_get_app_name_by_id(app_id, buf);
        graphics_text(6, y, buf);
        y += 16;
    }

    if (usage->size == 0) {
        graphics_set_color(10);
        graphics_text(25, 69, "No apps found");
    }

    graphics_set_font(ASSET_FONT_3X5_BUILTIN);
    graphics_text(6, 17, "TOTAL USAGE");

    if (usage->size > 0) {
        // progress bars and size in list
        uint24_t max_size = 0;
        memcpy(&max_size, (uint8_t*) index + sizeof_index * usage->index[0] + 1, 3);
        y = 53;
        for (uint8_t i = 0; i < items_count; ++i) {
            uint24_t size = 0;
            memcpy(&size, (uint8_t*) index + sizeof_index * usage->index[i + state.position] + 1,
                   3);
            uint8_t size_len = format_readable_size(buf, size);
            graphics_set_color(10);
            graphics_text((int8_t) (123 - size_len * 4), y, buf);
            y += 7;
            draw_progress_bar(y, 4, size * PROGRESS_BAR_WIDTH / max_size);
            y += 9;
        }
    }
}

static void draw_battery_overlay(void) {
    char buf[8];

    battery_status_t status = power_get_battery_status();

    graphics_set_font(ASSET_FONT_5X7);
    if (status == BATTERY_DISCHARGING) {
        draw_progress_bar_with_text(38, 11, power_get_battery_percent());
    }
    graphics_set_color(12);
    graphics_text(6, 61, BATTERY_STATUS_NAME[status]);

    if (status == BATTERY_DISCHARGING) {
        uint16_t voltage = sys_power_get_battery_voltage();
        sprintf(buf, "%" PRIu16 " mV", voltage);
    } else {
        strcpy(buf, "--");
    }
    graphics_text(6, 81, buf);

    graphics_set_font(ASSET_FONT_3X5_BUILTIN);
    if (status != BATTERY_DISCHARGING) {
        // draw empty bar since we can't know the level in this status.
        graphics_set_color(3);
        graphics_fill_rect(6, 38, 116, 11);
        graphics_set_color(10);
        graphics_text(38, 41, "NOT AVAILABLE");
    }
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_text(6, 31, "BATTERY LEVEL");
    graphics_text(6, 54, "BATTERY STATUS");
    graphics_text(6, 74, "BATTERY VOLTAGE");
}

void draw(void) {
    graphics_clear(DISPLAY_COLOR_BLACK);

    dialog_draw();

    sysui_battery_overlay();

    // all dialog content is drawn as an overlay in the content area,
    // since the core dialog library doesn't provide the UI elements needed.
    if (state.state == STATE_APPS) {
        draw_apps_overlay();
    } else if (state.state == STATE_FLASH) {
        draw_memory_overlay(&state.flash_usage, state.flash_index,
                            sizeof(state.flash_index) / APP_INDEX_SIZE, SYS_FLASH_SIZE / 256);
    } else if (state.state == STATE_EEPROM) {
        draw_memory_overlay(&state.eeprom_usage, state.eeprom_index,
                            sizeof(state.eeprom_index) / APP_INDEX_SIZE, SYS_EEPROM_SIZE / 256);
    } else if (state.state == STATE_BATTERY) {
        draw_battery_overlay();
    }
}
