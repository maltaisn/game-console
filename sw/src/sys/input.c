
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

#include <sys/input.h>
#include <sys/time.h>

#include <avr/io.h>

#define UPDATE_PERIOD (SYSTICK_FREQUENCY / UPDATE_FREQUENCY)

static volatile uint8_t state;

static uint8_t state0;
static uint8_t state1;

static uint8_t update_register;

uint8_t input_get_state(void) {
    return state;
}

void input_update_state(void) {
    // 2 levels debouncing: new value is most common value among last two and new.
    // this is probably overkill since the buttons don't even bounce...
    if (update_register == 0) {
        const uint8_t port = VPORTD.IN & 0x3f;
        state = (state0 & port) | (state1 & port) | (state0 & state1);
        state1 = state0;
        state0 = port;
        update_register = UPDATE_PERIOD - 1;
    } else {
        --update_register;
    }
}