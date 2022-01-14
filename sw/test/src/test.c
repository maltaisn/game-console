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
#include <sys/input.h>
#include <sys/display.h>
#include <sys/time.h>
#include <sys/power.h>

#include <core/graphics.h>
#include <core/sound.h>
#include <core/sysui.h>
#include <core/trace.h>
#include <core/dialog.h>
#include <core/debug.h>

#include <sim/flash.h>

#include <stdio.h>

#define FPS 5

static bool dialog_shown;

static uint8_t last_input;

void setup(void) {
#ifdef SIMULATION
    FILE* file = fopen("assets.dat", "rb");
    flash_load_file(0, file);
    fclose(file);
#endif

    dialog_init_centered(108, 88);
    dialog_set_font(ASSET_FONT_FONT7X7, ASSET_FONT_FONT5X7, ASSET_FONT_FONT3X5);
    dialog.flags |= DIALOG_FLAG_DISMISSABLE;
    dialog.title = "GAME OPTIONS";
    dialog.pos_btn = "OK";
    dialog.neg_btn = "CANCEL";
    dialog.pos_result = 0;
    dialog.neg_result = 1;
    dialog.selection = DIALOG_SELECTION_POS;
    dialog_add_item_number("CONTRAST", 0, 10, 10, 7);
    dialog_add_item_button("New game", 2);
    dialog_add_item_button("Main menu", 3);
    static const char* GAME_MODES[] = {"Easy", "Normal", "Hard"};
    dialog_add_item_choice("GAME", 1, 3, GAME_MODES);
}

static void draw(void) {
    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        sound_set_output_enabled(false);
        sysui_battery_sleep();
        return;
    }

    graphics_clear(DISPLAY_COLOR_BLACK);
    if (dialog_shown) {
        dialog_draw();
    }
}

void loop(void) {
    // input
    uint8_t state = input_get_state();
    if (dialog_shown) {
        dialog_result_t result = dialog_handle_input(last_input, state);
        if (result != DIALOG_RESULT_NONE) {
            dialog_shown = false;
            trace("dialog result = %d", result);
        }
    } else {
        uint8_t clicked = state & ~last_input;
        if (clicked & BUTTON0) {
            dialog_shown = true;
        }
    }
    last_input = input_get_state();

    // drawing
    display_first_page();
    do {
        draw();
    } while (display_next_page());
}
