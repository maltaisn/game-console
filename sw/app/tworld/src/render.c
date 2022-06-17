
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
#include <game.h>
#include <assets.h>
#include <input.h>
#include <level.h>

#include <core/graphics.h>
#include <core/sysui.h>
#include <core/dialog.h>
#include <core/utils.h>

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
    // TODO
}

/**
 * Draw the main menu screen.
 */
static void draw_main_menu(void) {
    // TODO
}

#define LEVEL_PACKS_PER_SCREEN 4

/**
 * Draw the content for the level pack selection dialog.
 */
static void draw_level_packs_overlay(void) {
    graphics_set_color(ACTIVE_COLOR(game.pos_first_y > 0));
    graphics_image_1bit_mixed(ASSET_IMAGE_ARROW_UP, 62, 16);
    graphics_set_color(ACTIVE_COLOR((int8_t) game.pos_first_y <=
                                    LEVEL_PACK_COUNT - LEVEL_PACKS_PER_SCREEN));
    graphics_image_1bit_mixed(ASSET_IMAGE_ARROW_DOWN, 62, 122);

    uint8_t index = game.pos_first_y;
    disp_y_t y = 21;
    for (uint8_t i = 0; i < LEVEL_PACKS_PER_SCREEN; ++i) {
        graphics_set_color(ACTIVE_COLOR(index == game.pos_selection_y));
        graphics_rect(4, y, 120, 23);

        graphics_set_color(12);
        graphics_set_font(ASSET_FONT_5X7);

        graphics_image_t image;
        if (index == LEVEL_PACK_COUNT) {
            // Not a level pack, a button to enter a level password.
            graphics_text(30, (int8_t) (y + 7), "Enter password");
            image = ASSET_IMAGE_PACK_PASSWORD;

        } else {
            const level_pack_info_t* info = &tworld_packs[index];

            // level pack progress: <completed>/<total>
            char buf[8];
            char *ptr0 = uint8_to_str(buf + 4, info->total_levels);
            char *ptr1 = uint8_to_str(ptr0 - 4, info->completed_levels);
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

        if (index > LEVEL_PACK_COUNT) {
            break;
        }
        ++index;
    }
}

/**
 * Draw the content for the level selection dialog.
 */
static void draw_levels_overlay(void) {

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
    graphics_clear(DISPLAY_COLOR_BLACK);

    game_state_t s = game.state;
    if (s <= GAME_STATE_CONTROLS) {
        draw_main_menu();
    } else if (s >= GAME_STATE_PLAY) {
        draw_game();
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
}