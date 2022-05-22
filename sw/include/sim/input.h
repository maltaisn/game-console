
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

#ifdef SIMULATION

#ifndef SIM_INPUT_H
#define SIM_INPUT_H

/**
 * Initialize keyboard callbacks for input module.
 */
void sim_input_init(void);

#ifndef SIMULATION_HEADLESS

/**
 * Set input state (headless only).
 */
void sim_input_set_state(uint8_t state);

/**
 * Update input state to set buttons pressed (headless only).
 */
void sim_input_press(uint8_t button_mask);

/**
 * Update input state to set buttons released (headless only).
 */
void sim_input_release(uint8_t button_mask);

#endif //SIMULATION_HEADLESS

#endif //SIM_INPUT_H

#endif //SIMULATION