
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

#ifndef SYS_UART_H
#define SYS_UART_H

#include <stdbool.h>
#include <stdint.h>

/*
 * To use the UART module, a SYS_UART_ENABLE define must be set during compilation.
 * This is to avoid the declaration of the UART callbacks when it's not used.
 * The size of the RX and TX buffer can be customized.
 */

#ifndef SYS_UART_RX_BUFFER_SIZE
// The maximum supported buffer size is 256 bytes
#define SYS_UART_RX_BUFFER_SIZE 64
#endif

#ifndef SYS_UART_TX_BUFFER_SIZE
// The maximum supported buffer size is 256 bytes
#define SYS_UART_TX_BUFFER_SIZE 32
#endif

#ifdef SIMULATION
#define SYS_UART_BAUD_RATE(baud) ((baud) / 100)
#else
// As given by datasheet formula, Table 23-1, Rev. C 01/2021, for asynchronous USART with CLK2X=1
#define SYS_UART_BAUD_RATE(baud) ((uint16_t) ((64.0 * F_CPU / (8.0 * baud)) + 0.5))
#endif

/**
 * Initialize the UART interface with a baud rate.
 * This must be called before reading or writing anything.
 */
void sys_uart_init(uint16_t baud_calc);

/**
 * Set the baud rate to use for UART communication.
 * This should be done first or after flushing to ensure the buffers are empty.
 * Use the `SYS_UART_BAUD_RATE` macro to compute the argument value.
 */
void sys_uart_set_baud(uint16_t baud_calc);

/**
 * Write a byte to the UART. The byte may be buffered or transmitted directly.
 * This function is never blocking. Interrupts must be enabled when called,
 * because the function waits for an interrupt in the case that buffer is full.
 */
void sys_uart_write(uint8_t c);

/**
 * Read a byte from the UART. This function is blocking if buffer is empty.
 * If data is available to read and is not read, buffer will fill up and data will be lost.
 * Interrupts must be enabled when called, because the function waits for an interrupt in
 * the case that the buffer is empty.
 */
uint8_t sys_uart_read(void);

/**
 * Returns true if data is available to be read from the RX buffer.
 * If this returns true, the next call to `uart_read` will not be blocking.
 */
bool sys_uart_available(void);

/**
 * Wait until TX buffer is empty and last transmission is complete.
 * Interrupts must be enabled when this is called.
 */
void sys_uart_flush(void);

#endif //SYS_UART_H
