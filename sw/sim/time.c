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

#include "sim/time.h"
#include "sys/time.h"

#include "sys/power.h"

#include <time.h>
#include "sys/input.h"
#include "core/sound.h"

#define POWER_SAMPLE_PERIOD 1.0

#define SYSTICK_MAX 0xffffff

static clock_t start_time;
static clock_t last_power_sample;

void time_init(void) {
    const clock_t time = clock();
    start_time = time;
    last_power_sample = time;
}

void time_update(void) {
    const clock_t time = clock();
    input_update_state();
    sound_update();

    if ((double) (time - last_power_sample) / CLOCKS_PER_SEC >= POWER_SAMPLE_PERIOD) {
        power_take_sample();
        sleep_if_low_battery();
        last_power_sample = time;
    }
}

systime_t time_get() {
    return (systime_t) ((double) (clock() - start_time) /
                        CLOCKS_PER_SEC * SYSTICK_FREQUENCY) & SYSTICK_MAX;
}
