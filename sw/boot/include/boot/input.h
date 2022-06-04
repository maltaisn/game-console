
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

#ifndef BOOT_INPUT_H
#define BOOT_INPUT_H

#include <stdint.h>

/**
 * Update the stored input state according to the current input state, debounced.
 */
void sys_input_update_state(void);

/**
 * Update the stored input state according to the current input state, without debouncing.
 * This also the latched current and last input state to the current input state.
 */
void sys_input_update_state_immediate(void);

/**
 * Dim display if input has been inactive for long enough.
 */
void sys_input_dim_if_inactive(void);

/**
 * Reset input inactivity counter (before display dim).
 */
void sys_input_reset_inactivity(void);

/**
 * Update input inactivity counter (before display dim).
 */
void sys_input_update_inactivity(void);

#endif //BOOT_INPUT_H
