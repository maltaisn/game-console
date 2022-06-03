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

#include <ui.h>

#include <core/dialog.h>

#define SUB_DIALOG_COUNT 4

static const char* const SUB_DIALOG_TITLES[SUB_DIALOG_COUNT] = {
        "APPS",
        "FLASH",
        "EEPROM",
        "BATTERY",
};

static const uint8_t SUB_DIALOG_HEIGHT[SUB_DIALOG_COUNT] = {
        113,
        113,
        113,
        92,
};

void open_main_dialog(void) {
    dialog_init_centered(114, 81);
    dialog.title = "SYSTEM STATUS";
    if (state.last_state == STATE_MAIN_MENU) {
        dialog.selection = 0;
    } else {
        // restore selection to previously selected item.
        dialog.selection = (uint8_t) state.last_state;
    }

    for (uint8_t i = 0; i < SUB_DIALOG_COUNT; ++i) {
        dialog_add_item_button(SUB_DIALOG_TITLES[i], i);
    }
    dialog_add_item_button("EXIT", STATE_TERMINATE);
}

void open_sub_dialog(state_t s) {
    uint8_t height = SUB_DIALOG_HEIGHT[s];
    dialog_init(2, (DISPLAY_HEIGHT - height - 10) / 2, 124, height);
    dialog.title = SUB_DIALOG_TITLES[s];
    dialog.pos_btn = "OK";
    dialog.pos_result = STATE_MAIN_MENU;
    dialog.dismiss_result = STATE_MAIN_MENU;
    dialog.flags = DIALOG_FLAG_DISMISSABLE;
    dialog.selection = DIALOG_SELECTION_POS;
}
