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

#include <sys/init.h>
#include "sys/uart.h"
#include "sys/power.h"

#include <avr/io.h>
#include <avr/interrupt.h>

static void init_registers(void) {
    // ====== CLOCK =====
    // 10 MHz clock (maximum for 2.8 V supply voltage)
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);

    // ====== PORT ======
    // TX, buzzer -, buzzer +, MOSI
    VPORTA.DIR |= PIN0_bm | PIN2_bm | PIN3_bm | PIN4_bm;
    // status LED, display SS, display reset, display D/C
    VPORTC.DIR |= PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;
    // flash SS, eeprom SS, enable VBAT level
    VPORTF.DIR |= PIN0_bm | PIN1_bm | PIN2_bm;

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

    // ====== RTC ======
    // interrupt every 1/256th s using 32.768 kHz internal clock for system time.
    while (RTC.STATUS != 0);
    RTC.PER = 0;
    RTC.INTCTRL = RTC_OVF_bm;
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;
    RTC.CTRLA = RTC_PRESCALER_DIV128_gc | RTC_RTCEN_bm;

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

static void init_battery_monitor(void) {
    // PIT: interrupt every 1 s for battery sampling.
    // note: battery monitor interrupt gets called 1 s after start so there's a check made before.
    while (RTC.PITSTATUS != 0);
    RTC.PITINTCTRL = RTC_PI_bm;
    RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;
}

void init(void) {
    init_registers();

    // check battery level on startup
    power_take_sample();
    power_wait_for_sample();
    sleep_if_low_battery();
    init_battery_monitor();
}