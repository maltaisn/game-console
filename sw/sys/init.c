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

#include "sys/init.h"
#include "sys/power.h"
#include "sys/uart.h"
#include "sys/sound.h"
#include "sys/spi.h"
#include "sys/display.h"

#include <avr/io.h>
#include <avr/interrupt.h>

static void init_registers(void) {
    // ====== CLOCK =====
    // 10 MHz clock (maximum for 2.8 V supply voltage)
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);

    // ====== PORT ======
    // TX, buzzer -, buzzer +, MOSI, SCK
    VPORTA.DIR |= PIN0_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN6_bm;
    // status LED, display SS, display reset, display D/C
    VPORTC.DIR |= PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;
    // flash SS, eeprom SS, enable VBAT level
    VPORTF.DIR |= PIN0_bm | PIN1_bm | PIN2_bm;

    // set buzzer H-bridge inputs low
    VPORTA.OUT = PIN2_bm | PIN3_bm;
    // set all CS lines high
    spi_deselect_all();

    // ====== USART ======
    uart_set_normal_mode();
    USART0.CTRLB = USART_TXEN_bm | USART_RXEN_bm | USART_RXMODE_CLK2X_gc;
    USART0.CTRLA = USART_RXCIE_bm;
    CPUINT.LVL1VEC = USART0_RXC_vect_num;

    // ====== SPI ======
    // master, 5 MHz SCK, mode 0, MSB first, buffered, no interrupts.
    SPI0.CTRLB = SPI_BUFEN_bm | SPI_MODE_0_gc | SPI_SSD_bm;
    SPI0.CTRLA = SPI_MASTER_bm | SPI_CLK2X_bm | SPI_PRESC_DIV4_gc | SPI_ENABLE_bm;

    // ====== TCA ======
    // Prescaler 2, split mode, single slope PWM on compare channel 3.
    // PWM is output on PA3 for buzzer. Only high timer is used, low timer is unused.
    TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;
    TCA0.SPLIT.HPER = SOUND_PWM_MAX;
    TCA0.SPLIT.HCMP0 = 0;
    TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc;
    // Set the event system channel 0 to PA3, with event user EVOUTA pin (PA2).
    // PA2 will be inverted when sound is enabled to drive H-bridge setup.
    EVSYS.CHANNEL0 = EVSYS_GENERATOR_PORT0_PIN3_gc;
    EVSYS.USEREVOUTA = EVSYS_CHANNEL_CHANNEL0_gc;

    // ====== TCB ======
    // Used for each sound channel. Prescaler = 2, periodic interrupt mode.
    TCB0.CTRLA = TCB_CLKSEL_CLKDIV2_gc;
    TCB0.INTCTRL = TCB_CAPT_bm;

    TCB1.CTRLA = TCB_CLKSEL_CLKDIV2_gc;
    TCB1.INTCTRL = TCB_CAPT_bm;

    TCB2.CTRLA = TCB_CLKSEL_CLKDIV2_gc;
    TCB2.INTCTRL = TCB_CAPT_bm;

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

    // initialize display
    display_init();
    power_set_15v_reg_enabled(true);
    display_set_enabled(true);
}
