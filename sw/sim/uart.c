/*
 * Copyright 2022 Nicolas Maltais
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

#ifdef SYS_UART_ENABLE

void sys_uart_init(uint16_t baud_calc) {
    // no-op
}

void sys_uart_set_baud(uint16_t baud_calc) {
    // no-op
}

void sys_uart_write(uint8_t c) {
    // no-op
}

uint8_t sys_uart_read(void) {
    // no data ever available
    return 0;
}

bool sys_uart_available(void) {
    // no data ever available
    return false;
}

void sys_uart_flush(void) {
    // no-op
}

#endif