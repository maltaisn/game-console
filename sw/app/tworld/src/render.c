
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
#include <tworld.h>
#include <assets.h>
#include <input.h>

#include <core/graphics.h>
#include <core/sysui.h>
#include <core/dialog.h>

#include <stdio.h>
#include <inttypes.h>

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

    if (game.dialog_shown) {
        dialog_draw();
        if (s == GAME_STATE_LEVELS) {
            draw_levels_overlay();
        } else if (s == GAME_STATE_CONTROLS || s == GAME_STATE_CONTROLS_PLAY) {
            draw_controls_overlay();
        }
        if (s < GAME_STATE_LEVEL_PACKS || s > GAME_STATE_PLAY) {
            sysui_battery_overlay();
        }
    }
}
