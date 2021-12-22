
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

#ifndef UART_H
#define UART_H

#include <stdio.h>

#define RX_BUFFER_SIZE 64
#define TX_BUFFER_SIZE 64

extern FILE uart_output;
extern FILE uart_input;

// Note: if buffer size is not zero, interrupts should always be enabled when using the
// following functions! This means none of them can be used within an interrupt.
// The reason is that interrupts are used to exit from infinite loops.

void uart_write(char c);
char uart_read(void);
void uart_flush(void);

#endif //UART_H
