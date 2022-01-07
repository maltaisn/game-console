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
#include <sys/power.h>
#include <sys/input.h>

#include <core/sound.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

static volatile systime_t systick;

ISR(RTC_CNT_vect) {
    // called 256 times per second
    RTC.INTFLAGS = RTC_OVF_bm;
    systime_t time = systick + 1;
    systick = time;
    input_update_state();
    sound_update();
}

ISR(RTC_PIT_vect) {
    // called every second
    RTC.PITINTFLAGS = RTC_PI_bm;
    power_start_sampling();
    power_schedule_sleep_if_low_battery(true);
}

systime_t time_get() {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        return systick;
    }
    return 0;
}
