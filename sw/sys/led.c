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

#include <sys/led.h>

#include <avr/io.h>

static uint8_t blink_period;
static uint8_t blink_counter;

void led_set(void) {
    VPORTC.OUT |= PIN0_bm;
}

void led_clear(void) {
    VPORTC.OUT &= ~PIN0_bm;
}

void led_toggle(void) {
    PORTC.OUTTGL = PIN0_bm;
}

void led_blink(uint8_t ticks) {
    blink_period = ticks;
    blink_counter = 0;
}

void led_blink_update(void) {
    if (blink_period == LED_BLINK_NONE) {
        return;
    }
    ++blink_counter;
    if (blink_counter == blink_period) {
        led_toggle();
        blink_counter = 0;
    }
}
