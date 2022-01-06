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

#include "sys/led.h"
#include <stdbool.h>

static bool led_on;

void led_set(void) {
    led_on = true;
}

void led_clear(void) {
    led_on = false;
}

void led_toggle(void) {
    led_on = !led_on;
}

bool led_get(void) {
    return led_on;
}
