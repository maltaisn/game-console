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

#include <init.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <uart.h>

void init(void) {
    // ====== CLOCK =====
    // 10 MHz clock (maximum for 2.8 V supply voltage)
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);

    // ====== PORT ======
    VPORTA.DIR |= PIN0_bm | PIN2_bm | PIN3_bm | PIN4_bm; // TX, buzzer -, buzzer +, MOSI
    VPORTC.DIR |= PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;  // status LED, display SS, display reset, display D/C
    VPORTF.DIR |= PIN0_bm | PIN1_bm | PIN2_bm;  // flash SS, eeprom SS, enable VBAT level

    // ====== USART ======
    USART0.BAUD = (uint16_t) ((64.0 * F_CPU / (16.0 * UART_BAUD)) + 0.5);
    USART0.CTRLB = USART_TXEN_bm | USART_RXEN_bm;
#if RX_BUFFER_SIZE > 0
    USART0.CTRLA = USART_RXCIE_bm;
#endif

    // ====== SPI ======
    // TODO

    // ====== TCA ======
    // TODO

    // ====== TCB ======
    // TODO

    // === ADC & VREF ===
    // 10-bit resolution, 64 samples accumulation, 78 kHz ADC clock,
    // use 2V5 voltage reference & enable result ready interrupt.
    VREF.CTRLA = VREF_ADC0REFSEL_2V5_gc;
    ADC0.CTRLA = ADC_RESSEL_10BIT_gc;
    ADC0.CTRLB = ADC_SAMPNUM_ACC64_gc;
    ADC0.CTRLC = ADC_SAMPCAP_bm | ADC_REFSEL_INTREF_gc | ADC_PRESC_DIV128_gc;
    ADC0.INTCTRL = ADC_RESRDY_bm;
    ADC0.CTRLA |= ADC_ENABLE_bm;

    // enable interrupts
    sei();
}