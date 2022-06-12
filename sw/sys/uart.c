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

#include <avr/io.h>

#ifdef BOOTLOADER

#include <avr/interrupt.h>

// The UART interrupts are naked to prevent the generation of the interrupt prologue and epilogue.
// Moreover, the callback is called from inline assembly to avoid saving and restoring all the
// registers like the compiler does for externally defined functions.
// These interrupt result in a single `jmp` instruction to the callback address.
// We are effectively "forwarding" the interrupt to the app.

// The callback itself is marked with __attribute__((signal)) to be treated as an interrupt.
// That means it will generate the same epilogue, prologue, and make sure to save all registers,
// just like an interrupt.

// Finally, the callback is called "vector" instead to avoid showing the -Wmisspelled-isr warning
// during compilation, since it annoyingly can't disabled in the linker, only in the compiler.

ISR(USART0_DRE_vect, ISR_NAKED) {
    asm volatile("jmp __callback_uart_dre");
}

ISR(USART0_RXC_vect, ISR_NAKED) {
    asm volatile("jmp __callback_uart_rxc");
}

#elif defined(SYS_UART_ENABLE)

enum {
    STATE_TRANSMITTED = (1 << 0),
};

static uint8_t state;

static struct {
    uint8_t data[SYS_UART_TX_BUFFER_SIZE];
    uint8_t head;
    volatile uint8_t tail;
} tx_buf;

static struct {
    uint8_t data[SYS_UART_RX_BUFFER_SIZE];
    volatile uint8_t head;
    uint8_t tail;
} rx_buf;

__attribute__((signal))
void __vector_uart_dre(void) {
    uint8_t tail = tx_buf.tail;
    USART0.TXDATAL = tx_buf.data[tail];
    tail = (tail + 1) % SYS_UART_TX_BUFFER_SIZE;
    tx_buf.tail = tail;
    if (tail == tx_buf.head) {
        USART0.CTRLA &= ~USART_DREIE_bm;
    }
}

__attribute__((signal))
void __vector_uart_rxc(void) {
    uint8_t head = rx_buf.head;
    uint8_t new_head = (head + 1) % SYS_UART_RX_BUFFER_SIZE;
    if (new_head == rx_buf.tail) {
        // buffer full, drop data. wait until data is read to reenable interrupt.
        USART0.CTRLA &= ~USART_RXCIE_bm;
    } else {
        rx_buf.data[head] = USART0.RXDATAL;
        rx_buf.head = new_head;
    }
}

void sys_uart_init(uint16_t baud_calc) {
    USART0.BAUD = baud_calc;
    USART0.CTRLB = USART_TXEN_bm | USART_RXEN_bm | USART_RXMODE_CLK2X_gc;
//  USART0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
    VPORTA.DIR |= PIN0_bm;  // TX pin
    USART0.CTRLA = USART_RXCIE_bm;
}

void sys_uart_set_baud(uint16_t baud_calc) {
    sys_uart_flush();
    USART0.BAUD = baud_calc;
}

void sys_uart_write(uint8_t c) {
    state |= STATE_TRANSMITTED;
    if (tx_buf.tail == tx_buf.head && (USART0.STATUS & USART_DREIF_bm)) {
        // TX data register empty and buffer empty, transmit directly.
        USART0.STATUS = USART_TXCIF_bm;
        USART0.TXDATAL = c;
    } else {
        // append byte to buffer and transmit later.
        const uint8_t new_head = (tx_buf.head + 1) % SYS_UART_TX_BUFFER_SIZE;
        while (new_head == tx_buf.tail);  // wait for interrupt to empty buffer.
        tx_buf.data[tx_buf.head] = c;
        tx_buf.head = new_head;
        USART0.CTRLA |= USART_DREIE_bm;
    }
}

uint8_t sys_uart_read(void) {
    while (rx_buf.tail == rx_buf.head);  // wait for interrupt to fill buffer.
    const uint8_t c = rx_buf.data[rx_buf.tail];
    rx_buf.tail = (rx_buf.tail + 1) % SYS_UART_RX_BUFFER_SIZE;
    USART0.CTRLA |= USART_RXCIE_bm;
    return c;
}

bool sys_uart_available(void) {
    return rx_buf.tail != rx_buf.head;
}

void sys_uart_flush(void) {
    if (!(state & STATE_TRANSMITTED)) {
        // nothing was transmitted, TXCIF bit will not be set.
        return;
    }
    // wait until TX data register is empty and until last transmission is complete.
    while ((USART0.CTRLA & USART_DREIE_bm) || !(USART0.STATUS & USART_TXCIF_bm));
}

#endif