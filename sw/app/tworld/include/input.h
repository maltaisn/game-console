
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

#ifndef TETRIS_INPUT_H
#define TETRIS_INPUT_H

#include "game.h"

// Keybindings, can be a single button or two buttons
#define BUTTON_LEFT      (BUTTON1)
#define BUTTON_RIGHT     (BUTTON5)
#define BUTTON_UP        (BUTTON2)
#define BUTTON_DOWN      (BUTTON3)
#define BUTTON_PAUSE     (BUTTON0 | BUTTON4)
#define BUTTON_INVENTORY (BUTTON4)
#define BUTTON_ACTION    (BUTTON0)

/**
 * Handle dialog input, including navigation between dialogs and previewing options.
 */
game_state_t game_handle_input_dialog(void);

/**
 * Handle tworld game input.
 */
game_state_t game_handle_input_tworld(void);

/**
 * Ignore currently pressed buttons until they are released.
 */
void game_ignore_current_input(void);

#endif //TETRIS_INPUT_H
