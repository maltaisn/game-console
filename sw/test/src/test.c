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
#include "core/debug.h"

#ifdef SIMULATION
#include <stdio.h>
#include <sim/flash.h>
#endif

void setup(void) {

}

void loop(void) {
    comm_receive();

    debug_print("Hello world!\n");

    systime_t start = time_get();
    while (time_get() - start < millis_to_ticks(1000));
}
