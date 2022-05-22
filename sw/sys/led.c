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
#include <sys/defs.h>

#include <avr/io.h>

#ifdef BOOTLOADER

#include <boot/defs.h>
#include <core/led.h>

BOOTLOADER_KEEP uint8_t sys_led_blink_period;  // LED_BLINK_NONE at startup
BOOTLOADER_KEEP uint8_t sys_led_blink_counter;

void sys_led_blink_update(void) {
    if (sys_led_blink_period == LED_BLINK_NONE) {
        return;
    }
    ++sys_led_blink_counter;
    if (sys_led_blink_counter == sys_led_blink_period) {
        led_toggle();
        sys_led_blink_counter = 0;
    }
}

#endif

ALWAYS_INLINE
void sys_led_set(void) {
    VPORTC.OUT |= PIN0_bm;
}

ALWAYS_INLINE
void sys_led_clear(void) {
    VPORTC.OUT &= ~PIN0_bm;
}

ALWAYS_INLINE
void sys_led_toggle(void) {
    PORTC.OUTTGL = PIN0_bm;
}

void sys_led_blink(uint8_t ticks) {
    sys_led_blink_period = ticks;
    sys_led_blink_counter = 0;
}
