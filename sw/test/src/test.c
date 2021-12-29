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

#include <test.h>

#include <sys/main.h>
#include <sys/time.h>

#include <core/comm.h>
#include <stdbool.h>
#include "core/debug.h"
#include "sys/input.h"

static uint8_t last_state;

void setup(void) {

}

void loop(void) {
    comm_receive();

    uint8_t state = input_get_state();
    uint8_t mask = BUTTON0;
    for (uint8_t i = 0; i < BUTTONS_COUNT; ++i) {
        bool curr = state & mask;
        bool last = last_state & mask;
        if (curr && !last) {
            debug_print("Button ");
            debug_print_hex8(i);
            debug_print(" pressed\n");
        } else if (!curr && last) {
            debug_print("Button ");
            debug_print_hex8(i);
            debug_print(" released\n");
        }
        mask <<= 1;
    }
    last_state = state;
}
