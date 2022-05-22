
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
#define BUTTON_DOWN      (BUTTON3)
#define BUTTON_ROT_CW    (BUTTON4)
#define BUTTON_ROT_CCW   (BUTTON0)
#define BUTTON_HOLD      (BUTTON2)
#define BUTTON_HARD_DROP (BUTTON1 | BUTTON5)
#define BUTTON_PAUSE     (BUTTON0 | BUTTON4)

// Buttons for which delayed auto-shift is enabled.
#define DAS_MASK         (BUTTON1 | BUTTON3 | BUTTON5)
// Disallowed DAS mask (if all bits in mask are set, all DAS are disabled)
#define DAS_DISALLOWED (BUTTON_LEFT | BUTTON_RIGHT)

// If a single button is pressed, wait for this delay in game ticks for a
// second button click to create a two-buttons combination.
// This does introduce a 50 ms delay between click and action.
#define BUTTON_COMBINATION_DELAY 2

// Delay after button press before delayed auto-shift is enabled.
#define DAS_DELAY 12
// Delay after which action occurs when delayed auto-shift is active.
#define AUTO_REPEAT_RATE 4

/**
 * Handle dialog input, including navigation between dialogs and previewing options.
 */
game_state_t game_handle_input_dialog(void);

/**
 * Handle tetris game input, delayed auto-shift, etc.
 */
game_state_t game_handle_input_tetris(void);

/**
 * Ignore currently pressed buttons until they are released.
 */
void game_ignore_current_input(void);

#endif //TETRIS_INPUT_H
