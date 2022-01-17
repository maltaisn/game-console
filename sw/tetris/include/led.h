
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

#ifndef TETRIS_LED_H
#define TETRIS_LED_H

#include <stdint.h>

/**
 * Start blinking the LED with a period in systicks and a duration in game ticks.
 */
void game_led_start(uint8_t period, uint8_t duration);

/**
 * Stop blinking the LED and turn it off.
 */
void game_led_stop(void);

/**
 * Update the blinking LED logic.
 */
void game_led_update(uint8_t dt);

#endif //TETRIS_LED_H
