
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
#include <stdbool.h>

#define RX_BUFFER_SIZE 64
#define TX_BUFFER_SIZE 32

extern FILE uart_output;
extern FILE uart_input;

// Note: if buffer size is not zero, interrupts should always be enabled when using the
// following functions! This means none of them can be used within an interrupt.
// The reason is that interrupts are used to exit from infinite loops.

/**
 * Write a byte to the UART. The byte may be buffered or transmitted directly.
 * This function is never blocking. Interrupts must be enabled when called,
 * because the function waits for an interrupt in the case that buffer is full.
 */
void uart_write(char c);

/**
 * Read a byte from the UART. This function is blocking if buffer is empty.
 * If data is available to read and is not read, buffer will fill up and data will be lost.
 * Interrupts must be enabled when called, because the function waits for an interrupt in
 * the case that the buffer is empty.
 */
char uart_read(void);

/**
 * Returns true if data is available to be read from the RX buffer.
 * If this returns true, the next call to `uart_read` will not be blocking.
 */
bool uart_available(void);

/**
 * Wait until TX buffer is empty.
 * Not that this doesn't mean that data is finished transmitting, only that buffer is empty.
 * Interrupts must be enabled when this is called.
 */
void uart_flush(void);

#endif //UART_H
