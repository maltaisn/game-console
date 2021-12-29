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

#include <sys/uart.h>

static bool fast_mode;

void uart_write(uint8_t c) {
    // not implemented
}

uint8_t uart_read(void) {
    // not implemented
    return 0;
}

bool uart_available(void) {
    return false;
}

void uart_flush(void) {
    // not implemented
}

void uart_set_fast_mode(void) {
    fast_mode = true;
}

void uart_set_normal_mode(void) {
    fast_mode = false;
}

bool uart_is_in_fast_mode(void) {
    return fast_mode;
}
