
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

#include <led.h>

#include <core/led.h>

static uint8_t led_blink_duration;

void game_led_start(uint8_t period, uint8_t duration) {
    led_blink(period);
    led_blink_duration = duration;
}

void game_led_stop(void) {
    led_blink(LED_BLINK_NONE);
    led_clear();
    led_blink_duration = 0;
}

void game_led_update(uint8_t dt) {
    // stop blinking LED if period elapsed
    if (led_blink_duration > 0) {
        if (led_blink_duration > dt) {
            led_blink_duration -= dt;
        } else {
            led_blink_duration = 0;
            led_blink(LED_BLINK_NONE);
            led_clear();
        }
    }
}
