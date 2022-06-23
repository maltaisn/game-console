
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

#include <core/graphics.h>
#include <core/sysui.h>
#include <core/dialog.h>
#include <core/utils.h>
#include <sys/display.h>

#include <string.h>

#ifdef FPS_MONITOR
#include <core/fpsmon.h>
#endif

#define ACTIVE_COLOR(cond) ((cond) ? 12 : 6)

#define CONTROLS_COUNT 7

static const char* const CONTROL_NAMES[CONTROLS_COUNT] = {
        "Pause",
        "Go left",
        "Go right",
        "Go up",
        "Go down",
        "Show items",
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

/**
 * Draw the game tile map.
 */
static void draw_game(void) {
    // get grid position of first tile shown on the top left.
    position_t pos = tworld_get_current_position();
    grid_pos_t xstart = get_camera_pos(pos.x);
    grid_pos_t ystart = get_camera_pos(pos.y);

    disp_y_t y = 1;
    const uint8_t xend = xstart + GAME_MAP_SIZE;
    const uint8_t yend = ystart + GAME_MAP_SIZE;
    for (grid_pos_t py = ystart; py < yend; ++py) {
        y += GAME_TILE_SIZE;  // at this point Y is the start coordinate of *next* tile.
        if (y < sys_display_page_ystart) {
            // before start of page.
            continue;
        }

        y -= GAME_TILE_SIZE;  // at this point Y is the start coordinate of *current* tile.
        if (y > sys_display_page_yend) {
            // after end of page.
            return;
        }

        disp_x_t x = 0;
        for (grid_pos_t px = xstart; px < xend; ++px) {
            tile_t tile = tworld_get_bottom_tile(px, py);
            actor_t actor = tworld_get_top_tile(px, py);
            if (!actor_is_block(actor)) {
                // don't draw a bottom tile if actor is a block (fully opaque image).
                draw_bottom_tile(x, y, tile);
            }
            if (actor_get_entity(actor) != ENTITY_NONE) {
                draw_top_tile(x, y, actor);
            }
            x += GAME_TILE_SIZE;
        }

        y += GAME_TILE_SIZE;
    }
}

/**
 * Draw the main menu screen.
 */
static void draw_main_menu(void) {
    // TODO
}

static void draw_vertical_navigation_arrows(uint8_t top_y) {
    graphics_set_color(ACTIVE_COLOR(game.pos_first_y > 0));
    graphics_image_1bit_mixed(ASSET_IMAGE_ARROW_UP, 62, top_y);
    graphics_set_color(ACTIVE_COLOR(game.pos_first_y <=
                                    (uint8_t) (game.pos_max_y - game.pos_shown_y)));
    graphics_image_1bit_mixed(ASSET_IMAGE_ARROW_DOWN, 62, 122);
}

/**
 * Draw the content for the level pack selection dialog.
 */
static void draw_level_packs_overlay(void) {
    draw_vertical_navigation_arrows(16);

    uint8_t index = game.pos_first_y;
    disp_y_t y = 21;
    for (uint8_t i = 0; i < LEVEL_PACKS_PER_SCREEN; ++i) {
        bool selected = index == game.pos_selection_y;
        graphics_set_color(ACTIVE_COLOR(selected));
        graphics_rect(4, y, 120, 23);

        graphics_set_color(selected ? 12 : 9);
        graphics_set_font(ASSET_FONT_5X7);

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
            graphics_set_font(ASSET_FONT_7X7);
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
    draw_vertical_navigation_arrows(25);
    graphics_set_font(ASSET_FONT_7X7);

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
 * Draw the content for the controls dialog.
 */
static void draw_controls_overlay(void) {
    graphics_set_font(ASSET_FONT_5X7);
    disp_y_t y = 25;
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
    if (s >= GAME_STATE_PLAY) {
        // there's no point in clearing the full display, most of it will be redrawn for the grid.
        // only clear the outer border on which tiles aren't drawn.
        graphics_set_color(DISPLAY_COLOR_BLACK);
        graphics_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        // also clear extra line at x=1, draw_bottom_tile does a bitwise or on the first column...
        graphics_vline(0, DISPLAY_HEIGHT - 1, 1);

        draw_game();

    } else {
        graphics_clear(DISPLAY_COLOR_BLACK);
        if (s <= GAME_STATE_CONTROLS) {
            draw_main_menu();
        }
    }

    if (game.flags & FLAG_DIALOG_SHOWN) {
        dialog_draw();
        if (s == GAME_STATE_LEVEL_PACKS) {
            draw_level_packs_overlay();
        } else if (s == GAME_STATE_LEVELS) {
            draw_levels_overlay();
        } else if (s == GAME_STATE_CONTROLS || s == GAME_STATE_CONTROLS_PLAY) {
            draw_controls_overlay();
        }
        if (s < GAME_STATE_LEVEL_PACKS || s > GAME_STATE_PLAY) {
            sysui_battery_overlay();
        }
    }

#ifdef FPS_MONITOR
    fpsmon_draw();
#endif
}