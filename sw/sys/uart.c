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

#include "sys/uart.h"

#include <avr/io.h>
#include <avr/interrupt.h>

// As given by datasheet formula, Table 23-1, Rev. C 01/2021, for asynchronous USART with CLK2X=1
#define uart_baud_rate_reg(baud) ((uint16_t) ((64.0 * F_CPU / (8.0 * baud)) + 0.5))

#define STATE_TRANSMITTED 1
#define STATE_FAST 2

#ifndef DISABLE_COMMS
static uint8_t state;

static struct {
    uint8_t data[TX_BUFFER_SIZE];
    uint8_t head;
    volatile uint8_t tail;
} tx_buf;

static struct {
    uint8_t data[RX_BUFFER_SIZE];
    volatile uint8_t head;
    uint8_t tail;
} rx_buf;

ISR(USART0_DRE_vect) {
    uint8_t tail = tx_buf.tail;
    USART0.TXDATAL = tx_buf.data[tail];
    tail = (tail + 1) % TX_BUFFER_SIZE;
    tx_buf.tail = tail;
    if (tail == tx_buf.head) {
        USART0.CTRLA &= ~USART_DREIE_bm;
    }
}

ISR(USART0_RXC_vect) {
    uint8_t head = rx_buf.head;
    uint8_t new_head = (head + 1) % RX_BUFFER_SIZE;
    if (new_head == rx_buf.tail) {
        // buffer full, drop data. wait until data is read to reenable interrupt.
        USART0.CTRLA &= ~USART_RXCIE_bm;
    } else {
        rx_buf.data[head] = USART0.RXDATAL;
        rx_buf.head = new_head;
    }
}

void uart_write(uint8_t c) {
    state |= STATE_TRANSMITTED;
    if (tx_buf.tail == tx_buf.head && (USART0.STATUS & USART_DREIF_bm)) {
        // TX data register empty and buffer empty, transmit directly.
        USART0.STATUS = USART_TXCIF_bm;
        USART0.TXDATAL = c;
    } else {
        // append byte to buffer and transmit later.
        const uint8_t new_head = (tx_buf.head + 1) % TX_BUFFER_SIZE;
        while (new_head == tx_buf.tail);  // wait for interrupt to empty buffer.
        tx_buf.data[tx_buf.head] = c;
        tx_buf.head = new_head;
        USART0.CTRLA |= USART_DREIE_bm;
    }
}

uint8_t uart_read(void) {
    while (rx_buf.tail == rx_buf.head);  // wait for interrupt to fill buffer.
    const uint8_t c = rx_buf.data[rx_buf.tail];
    rx_buf.tail = (rx_buf.tail + 1) % RX_BUFFER_SIZE;
    USART0.CTRLA |= USART_RXCIE_bm;
    return c;
}

bool uart_available(void) {
    return rx_buf.tail != rx_buf.head;
}

void uart_flush(void) {
    if (!(state & STATE_TRANSMITTED)) {
        // nothing was transmitted, TXCIF bit will not be set.
        return;
    }
    // wait until TX data register is empty and until last transmission is complete.
    while ((USART0.CTRLA & USART_DREIE_bm) || !(USART0.STATUS & USART_TXCIF_bm));
}

void uart_set_fast_mode(void) {
    state |= STATE_FAST;
    USART0.BAUD = uart_baud_rate_reg(UART_BAUD_FAST);
}

void uart_set_normal_mode(void) {
    state &= ~STATE_FAST;
    USART0.BAUD = uart_baud_rate_reg(UART_BAUD);
}

bool uart_is_in_fast_mode(void) {
    return (state & STATE_FAST) != 0;
}
#endif //DISABLE_COMMS
