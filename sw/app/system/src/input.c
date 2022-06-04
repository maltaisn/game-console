
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

#include <input.h>

#include <core/app.h>
#include <core/dialog.h>

void handle_input(void) {
    input_latch();

    // handle horizontal movement within sub dialogs.
    if (state.state < STATE_MAIN_MENU) {
        uint8_t clicked = input_get_clicked();
        if (clicked & BUTTON_UP) {
            if (state.position > 0) {
                --state.position;
            }
        } else if (clicked & BUTTON_DOWN) {
            if (state.position < state.max_position) {
                ++state.position;
            }
        }
    }

    // handle dialog input
    dialog_result_t res = dialog_handle_input();
    if (res != DIALOG_RESULT_NONE) {
        if (res == STATE_TERMINATE) {
            app_terminate();
        }
        state.last_state = state.state;
        state.state = res;
        state.flags &= ~SYSTEM_FLAG_DIALOG_SHOWN;
    }
}
