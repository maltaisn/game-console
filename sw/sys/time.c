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

#include <sys/time.h>
#include <sys/defs.h>

#include <util/atomic.h>

#ifdef BOOTLOADER

#include <boot/input.h>
#include <boot/sound.h>
#include <boot/led.h>

#include <avr/io.h>
#include <avr/interrupt.h>

volatile systime_t sys_time_counter;

ISR(RTC_CNT_vect) {
    // called 256 times per second
    RTC.INTFLAGS = RTC_OVF_bm;

    systime_t time = sys_time_counter + 1;
    sys_time_counter = time;

    sys_input_update_state();
    sys_sound_update();
    sys_led_blink_update();
}

#endif // BOOTLOADER

ALWAYS_INLINE
systime_t sys_time_get() {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        return sys_time_counter;
    }
    return 0;
}
