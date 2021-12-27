
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

#include <stdbool.h>
#include <stdint.h>

#define RX_BUFFER_SIZE 64
#define TX_BUFFER_SIZE 32

/**
 * Write a byte to the UART. The byte may be buffered or transmitted directly.
 * This function is never blocking. Interrupts must be enabled when called,
 * because the function waits for an interrupt in the case that buffer is full.
 */
void uart_write(uint8_t c);

/**
 * Read a byte from the UART. This function is blocking if buffer is empty.
 * If data is available to read and is not read, buffer will fill up and data will be lost.
 * Interrupts must be enabled when called, because the function waits for an interrupt in
 * the case that the buffer is empty.
 */
uint8_t uart_read(void);

/**
 * Returns true if data is available to be read from the RX buffer.
 * If this returns true, the next call to `uart_read` will not be blocking.
 */
bool uart_available(void);

/**
 * Wait until TX buffer is empty and last transmission is complete.
 * Interrupts must be enabled when this is called.
 */
void uart_flush(void);

/**
 * Set UART in fast mode (use UART_BAUD_FAST baud rate).
 */
void uart_set_fast_mode(void);

/**
 * Set UART in normal mode (use UART_BAUD baud rate).
 */
void uart_set_normal_mode(void);

/**
 * Returns true if UART is currently in fast mode.
 */
bool uart_is_in_fast_mode(void);


#endif //UART_H
