
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

#ifndef CORE_INPUT_H
#define CORE_INPUT_H

#include <stdint.h>

#define BUTTONS_COUNT 6

#define BUTTON0 (1 << 0)  // SW2
#define BUTTON1 (1 << 1)  // SW3
#define BUTTON2 (1 << 2)  // SW4
#define BUTTON3 (1 << 3)  // SW5
#define BUTTON4 (1 << 4)  // SW6
#define BUTTON5 (1 << 5)  // SW7

#define BUTTONS_ALL (BUTTON0 | BUTTON1 | BUTTON2 | BUTTON3 | BUTTON4 | BUTTON5)

/**
 * Latch the current input state. This also updates the last input state.
 */
void input_latch(void);

/**
 * Returns a bit field indicating which buttons are currently pressed.
 * This state is the state that was latched when calling `sys_input_latch`.
 */
uint8_t input_get_state(void);

/**
 * Returns a bit field indicating which buttons were pressed in the last latch event.
 * This state is the state that was latched before calling `sys_input_latch`.
 */
uint8_t input_get_last_state(void);

/**
 * Returns a bit field indicating which buttons were clicked by comparing
 * the current and last latch input states.
 */
uint8_t input_get_clicked(void);

#include <sim/input.h>

#endif //CORE_INPUT_H
