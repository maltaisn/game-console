
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

#ifndef SYS_LED_H
#define SYS_LED_H

#include <stdint.h>

#define LED_BLINK_NONE 0

/**
 * Turn the LED on.
 */
void led_set(void);

/**
 * Turn the LED off.
 */
void led_clear(void);

/**
 * Toggle the LED.
 */
void led_toggle(void);

/**
 * Blink the LED at a certain half-period in system ticks.
 * Note that setting, clearing and toggling the LED does not disable blinking.
 * Blinking must be disabled by calling `led_blink(LED_BLINK_NONE)`.
 */
void led_blink(uint8_t ticks);

/**
 * Called every tick to update LED blinking.
 */
void led_blink_update(void);

#endif //SYS_LED_H
