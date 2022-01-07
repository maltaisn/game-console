
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

#ifndef SYS_INPUT_H
#define SYS_INPUT_H

#include <stdint.h>

// input state update frequency in Hz.
#define UPDATE_FREQUENCY 64

#define BUTTONS_COUNT 6

#define BUTTON0 (1 << 0)  // SW2
#define BUTTON1 (1 << 1)  // SW3
#define BUTTON2 (1 << 2)  // SW4
#define BUTTON3 (1 << 3)  // SW5
#define BUTTON4 (1 << 4)  // SW6
#define BUTTON5 (1 << 5)  // SW7

/**
 * Returns a bitfield indicating the current (debounced) state of input.
 * A 1 bit indicates that the button is pressed.
 */
uint8_t input_get_state(void);

/**
 * Update current input state.
 * This is called on systick update.
 */
void input_update_state(void);

/**
 * Reset inactivity countdown timer and undim screen.
 */
void input_reset_inactivity(void);

/**
 * Called every second to update inactivity countdown.
 */
void input_update_inactivity(void);

#endif //SYS_INPUT_H
