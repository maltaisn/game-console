
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

#include "render.h"
#include "render_utils.h"
#include "game.h"
#include "assets.h"
#include "input.h"
#include "tworld_level.h"
#include "save.h"

#include <core/graphics.h>
#include <core/sysui.h>
#include <core/dialog.h>
#include <core/utils.h>
#include <sys/display.h>

#include <string.h>

#ifdef FPS_MONITOR
#include <core/fpsmon.h>
#endif

#define CONTROLS_COUNT 7

static const char* const CONTROL_NAMES[CONTROLS_COUNT] = {
        "Pause",
        "Go left",
        "Go right",
        "Go up",
        "Go down",
        "Show inventory",
        "View hint",
};
static const uint8_t CONTROL_BUTTONS[CONTROLS_COUNT] = {
        BUTTON_PAUSE,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_UP,
        BUTTON_DOWN,
        BUTTON_INVENTORY,
        BUTTON_ACTION,
};

#define display_ystart_for_page(page) ((page) * DISPLAY_PAGE_HEIGHT)

static void set_3x5_font(void) {
    graphics_set_font(ASSET_FONT_3X5_BUILTIN);
}

static void set_7x7_font(void) {
    graphics_set_font(ASSET_FONT_7X7);
}

static void set_5x7_font(void) {
    graphics_set_font(ASSET_FONT_5X7);
}

/**
 * Draw the inventory overlay at the bottom of the screen.
 * Also show the time left in at the top of the screen.
 */
static void draw_inventory_overlay(void) {
    // For performance, do early page check to avoid loading font and making unused draw calls.
    // When the inventory is shown, 19 fewer tiles are drawn, performance should be great.

    if (sys_display_page_ystart == display_ystart_for_page(0)) {
        // top bar indicating time left
        set_5x7_font();
        graphics_set_color(DISPLAY_COLOR_BLACK);
        graphics_fill_rect(1, 1, 126, 14);
        graphics_set_color(12);
        graphics_text(4, 4, "CHIPS");
        graphics_text(71, 4, "TIME");

        set_7x7_font();
        graphics_set_color(DISPLAY_COLOR_WHITE);
        char buf[4];
        uint16_to_str_zero_pad(buf, tworld.chips_left);
        graphics_text(38, 4, buf);
        format_time_left(buf, tworld.time_left);
        graphics_text(100, 4, buf);
        return;
    }

    if (sys_display_page_ystart >= display_ystart_for_page(4)) {
        // bottom bar
        set_7x7_font();
        graphics_set_color(DISPLAY_COLOR_BLACK);
        graphics_fill_rect(1, 99, 126, 28);
        graphics_set_color(12);
        graphics_text(28, 102, "INVENTORY");
    }
    if (sys_display_page_ystart == display_ystart_for_page(5)) {
        // draw inventory content
        uint8_t boot_mask = 1;
        disp_x_t x = 6;
        for (uint8_t i = 0; i < 4; ++i) {
            const tile_t key_tile = tworld.keys[i] > 0 ? tile_make_key(i) : TILE_FLOOR;
            draw_bottom_tile(x, 112, key_tile);
            const tile_t boot_tile = (tworld.boots & boot_mask) ? tile_make_boots(i) : TILE_FLOOR;
            draw_bottom_tile(x + 60, 112, boot_tile);
            x += GAME_TILE_SIZE;
            boot_mask <<= 1;
        }
    }
}

/**
 * Draw a 2 digit counter on the top right of the screen with the time left.
 */
static void draw_low_timer_overlay(void) {
    if (sys_display_page_ystart == display_ystart_for_page(0)) {
        graphics_set_color(DISPLAY_COLOR_BLACK);
        graphics_fill_rect(108, 1, 19, 10);
        set_7x7_font();
        char buf[4];
        buf[1] = '0';  // for when time < 10;
        uint8_to_str(buf, time_left_to_seconds(tworld.time_left));
        graphics_set_color(DISPLAY_COLOR_WHITE);
        graphics_text(111, 2, buf + 1);
    }
}

/**
 * Draw the game tile map.
 */
static void draw_game(void) {
    // get grid position of first tile shown on the top left.
    const position_t curr_pos = tworld_get_current_position();
    grid_pos_t xstart = get_camera_pos(curr_pos.x);
    grid_pos_t ystart = get_camera_pos(curr_pos.y);

    disp_y_t y = 1;
    const uint8_t xend = xstart + GAME_MAP_SIZE;
    uint8_t yend = ystart + GAME_MAP_SIZE;

    bool inventory_shown = game.flags & FLAG_INVENTORY_SHOWN;
    if (inventory_shown) {
        // top row and bottom 2 rows hidden, don't draw them.
        ystart += 1;
        yend -= 2;
        y += 14;
    }

    for (grid_pos_t py = ystart; py < yend; ++py) {
        y += GAME_TILE_SIZE;  // at this point Y is the start coordinate of *next* tile.
        if (y < sys_display_page_ystart) {
            // before start of page.
            continue;
        }

        y -= GAME_TILE_SIZE;  // at this point Y is the start coordinate of *current* tile.
        if (y > sys_display_page_yend) {
            // after end of page.
            break;
        }

        disp_x_t x = 0;
        for (grid_pos_t px = xstart; px < xend; ++px) {
            const position_t pos = {px, py};
            const tile_t tile = tworld_get_bottom_tile(pos);
            const actor_t actor = tworld_get_top_tile(pos);
            if (!actor_is_block(actor)) {
                // don't draw a bottom tile if actor is a block (fully opaque image).
                draw_bottom_tile(x, y, tile);
            }
            if (tworld_has_collided() && curr_pos.x == px && curr_pos.y == py) {
                // draw actor on top of chip or chip on top of actor in case of collision.
                draw_top_tile(x, y, tworld.collided_actor);
            }
            if (actor_get_entity(actor) != ENTITY_NONE) {
                draw_top_tile(x, y, actor);
            }
            x += GAME_TILE_SIZE;
        }

        y += GAME_TILE_SIZE;
    }

    if (inventory_shown) {
        draw_inventory_overlay();
    } else if (tworld.time_left <= LOW_TIMER_THRESHOLD) {
        draw_low_timer_overlay();
    }
}

/**
 * Draw the main menu screen.
 */
static void draw_main_menu(void) {
    graphics_image_4bit_mixed(ASSET_IMAGE_MENU, 0, 0);
}

/**
 * Draw the content for the level pack selection dialog.
 */
static void draw_level_packs_overlay(void) {
    draw_vertical_navigation_arrows(16, 122);

    uint8_t index = game.pos_first_y;
    disp_y_t y = 21;
    for (uint8_t i = 0; i < LEVEL_PACKS_PER_SCREEN; ++i) {
        bool selected = index == game.pos_selection_y;
        graphics_set_color(ACTIVE_COLOR(selected));
        graphics_rect(4, y, 120, 23);

        graphics_set_color(selected ? 12 : 9);
        set_5x7_font();

        graphics_image_t image;
        if (index == LEVEL_PACK_COUNT) {
            // Not a level pack, a button to enter a level password.
            graphics_text(30, (int8_t) (y + 7), "Enter password");
            image = ASSET_IMAGE_PACK_PASSWORD;

        } else {
            const level_pack_info_t* info = &tworld_packs.packs[index];

            // level pack progress: <completed>/<total>
            char buf[8];
            char* ptr0 = uint8_to_str(buf + 4, info->total_levels);
            char* ptr1 = uint8_to_str(ptr0 - 4, info->completed_levels);
            ptr0[-1] = '/';
            graphics_text(30, (int8_t) (y + 13), ptr1);

            // level pack name
            set_7x7_font();
            graphics_text(30, (int8_t) (y + 3), info->name);

            if (game.options.unlocked_packs & (1 << index)) {
                image = asset_image_pack_progress(
                        (uint16_t) info->completed_levels * 8 / info->total_levels);
            } else {
                image = ASSET_IMAGE_PACK_LOCKED;
            }
        }

        graphics_image_4bit_mixed(image, 8, y + 3);
        y += 25;

        if (index >= LEVEL_PACK_COUNT) {
            break;
        }
        ++index;
    }
}

/**
 * Draw the content for the level selection dialog.
 */
static void draw_levels_overlay(void) {
    draw_vertical_navigation_arrows(25, 122);
    set_7x7_font();

    const level_pack_info_t* info = &tworld_packs.packs[game.current_pack];
    level_idx_t number = game.pos_first_y * LEVELS_PER_SCREEN_H;

    // draw level pack title
    uint8_t name_len = strlen(info->name);
    graphics_set_color(12);
    graphics_text((int8_t) (64 - name_len * 4), 16, info->name);

    // advance in completed levels bitset until position of first shown level.
    uint8_t byte = 0;
    uint8_t bits = 0;
    const uint8_t* completed = info->completed_array;
    uint8_t curr_level = 0;
    for (; curr_level < number; ++curr_level) {
        if (bits == 0) {
            byte = *completed++;
        }
        byte >>= 1;
        bits = (bits + 1) % 8;
    }

    // draw the level grid
    disp_y_t y = 31;
    uint8_t last_y = game.pos_first_y + LEVELS_PER_SCREEN_V;
    for (uint8_t i = game.pos_first_y; i < last_y; ++i) {
        disp_x_t x = 5;
        for (uint8_t j = 0; j < LEVELS_PER_SCREEN_H; ++j) {
            // determine the level box color
            if (bits == 0) {
                byte = *completed++;
            }
            uint8_t color;
            if (byte & 1) {
                // level completed
                color = 11;
            } else if (curr_level == info->last_unlocked) {
                // level unlocked
                color = 15;
            } else {
                // level locked
                color = 6;
            }
            graphics_set_color(color);

            // draw the level box
            graphics_rect(x, y, 28, 28);
            if (j == game.pos_selection_x && i == game.pos_selection_y) {
                graphics_rect(x - 1, y - 1, 30, 30);
            }

            // draw the level text
            ++curr_level;
            char buf[4];
            char* ptr = uint8_to_str(buf, curr_level);
            int8_t px = (int8_t) (x + (ptr - buf) * 4 + 3);
            graphics_text(px, (int8_t) (y + 10), ptr);

            if (curr_level == info->total_levels) {
                return;
            }

            x += 30;
            byte >>= 1;
            bits = (bits + 1) % 8;
        }
        y += 30;
    }
}

/**
 * Draw the content for the level info dialog.
 */
static void draw_level_info_overlay(void) {
    // level title, centered on 1-2 lines
    const flash_t title = level_get_title();
    const uint8_t lines = find_text_line_count(title, 122);
    const disp_y_t y = lines == 2 ? 35 : 40;
    graphics_set_color(DISPLAY_COLOR_WHITE);
    draw_text_wrap(3, y, 122, 2, title, true);

    graphics_set_color(10);
    set_3x5_font();
    graphics_text(22, 57, "CHIPS NEEDED");
    graphics_text(30, 66, "TIME LIMIT");
    graphics_text(34, 75, "BEST TIME");

    graphics_set_color(DISPLAY_COLOR_WHITE);
    set_7x7_font();
    char buf[4];
    uint16_to_str_zero_pad(buf, tworld.chips_left);
    graphics_text(74, 56, buf);
    format_time_left(buf, tworld.time_left);
    graphics_text(74, 65, buf);
    format_time_left(buf, get_best_level_time(game.current_level_pos));
    graphics_text(74, 74, buf);
}

/**
 * Draw the content for the level fail dialog.
 */
static void draw_level_fail_overlay(void) {
    // End cause text
    set_5x7_font();
    graphics_set_color(DISPLAY_COLOR_WHITE);
    const flash_t text = asset_end_cause(tworld.end_cause - 1);
    const uint8_t lines = find_text_line_count(text, 116);
    draw_text_wrap(6, 42 + (3 - lines) * 5, 116, 3, text, true);
}

/**
 * Draw the content for the level complete dialog.
 */
static void draw_level_complete_overlay(void) {
    set_3x5_font();
    graphics_set_color(10);
    graphics_text(34, 43, "TIME LEFT");
    graphics_text(34, 52, "BEST TIME");
    graphics_set_color(8);
    graphics_text(32, 64, "PASSWORD");

    set_7x7_font();
    char buf[5];
    format_time_left(buf, tworld.time_left);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_text(74, 42, buf);
    format_time_left(buf, get_best_level_time(game.current_level_pos));
    graphics_text(74, 51, buf);
    level_get_password(buf);
    graphics_set_color(10);
    graphics_text(68, 63, buf);
}

/**
 * Draw the content for the hint dialog.
 */
static void draw_hint_overlay(void) {
    draw_vertical_navigation_arrows(34, 90);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    const flash_t hint = find_text_line_start(
            level_get_hint(), HINT_TEXT_WIDTH, game.pos_selection_y);
    draw_text_wrap(8, 39, HINT_TEXT_WIDTH, HINT_LINES_PER_SCREEN, hint, false);
}

/**
 * Draw the content for the controls dialog.
 */
static void draw_controls_overlay(void) {
    set_5x7_font();
    disp_y_t y = 28;
    for (uint8_t i = 0; i < CONTROLS_COUNT; ++i) {
        // control name text
        graphics_set_color(DISPLAY_COLOR_WHITE);
        graphics_text(30, (int8_t) y, CONTROL_NAMES[i]);

        // illustrate the 6 buttons with the one used by the control highlighted.
        uint8_t buttons = CONTROL_BUTTONS[i];
        uint8_t mask = BUTTON0;
        disp_x_t button_x = 15;
        for (uint8_t j = 0; j < 3; ++j) {
            disp_y_t button_y = y;
            for (uint8_t k = 0; k < 2; ++k) {
                graphics_set_color(buttons & mask ? DISPLAY_COLOR_WHITE : 6);
                graphics_fill_rect(button_x, button_y, 3, 3);
                button_y += 4;
                mask <<= 1;
            }
            button_x += 4;
        }
        y += 10;
    }
}

void draw(void) {
    game_state_t s = game.state;
    if (s >= GAME_SSEP_LEVEL_BG) {
        // there's no point in clearing the full display, most of it will be redrawn for the grid.
        // only clear the outer border on which tiles aren't drawn.
        graphics_set_color(DISPLAY_COLOR_BLACK);
        graphics_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        // also clear extra line at x=1, draw_bottom_tile does a bitwise or on the first column...
        graphics_vline(0, DISPLAY_HEIGHT - 1, 1);

        draw_game();

    } else if (s <= GAME_SSEP_COVER_BG) {
        draw_main_menu();
    } else {
        graphics_clear(DISPLAY_COLOR_BLACK);
    }

    if (game.flags & FLAG_DIALOG_SHOWN) {
        dialog_draw();
        if (s == GAME_STATE_LEVEL_PACKS) {
            draw_level_packs_overlay();
        } else if (s == GAME_STATE_LEVELS) {
            draw_levels_overlay();
        } else if (s == GAME_STATE_LEVEL_INFO) {
            draw_level_info_overlay();
        } else if (s == GAME_STATE_LEVEL_FAIL) {
            draw_level_fail_overlay();
        } else if (s == GAME_STATE_LEVEL_COMPLETE) {
            draw_level_complete_overlay();
        } else if (s == GAME_STATE_HINT) {
            draw_hint_overlay();
        } else if (s == GAME_STATE_CONTROLS || s == GAME_STATE_CONTROLS_PLAY) {
            draw_controls_overlay();
        }
        if (!(s >= GAME_SSEP_NO_BAT_START && s <= GAME_SSEP_NO_BAT_END)) {
            sysui_battery_overlay();
        }
    }

#ifdef FPS_MONITOR
    fpsmon_draw();
#endif
}
