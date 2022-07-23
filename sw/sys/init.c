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

#ifdef BOOTLOADER

#include <boot/init.h>
#include <boot/display.h>
#include <boot/flash.h>
#include <boot/input.h>
#include <boot/power.h>
#include <boot/sound.h>

#include <sys/sound.h>
#include <sys/spi.h>
#include <sys/display.h>
#include <sys/led.h>
#include <sys/reset.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

FUSES = {
        .WDTCFG = FUSE_WDTCFG_DEFAULT,
        .BODCFG = BOD_LVL_BODLEVEL0_gc | BOD_SAMPFREQ_1KHZ_gc |
                  BOD_ACTIVE_SAMPLED_gc | BOD_SLEEP_DIS_gc,
        .OSCCFG = FUSE_OSCCFG_DEFAULT,
        .SYSCFG0 = FUSE_SYSCFG0_DEFAULT,
        .SYSCFG1 = FUSE_SYSCFG1_DEFAULT,
        .APPEND = 0x00,
        .BOOTEND = 8448 / 256,
};

static void sys_init_registers(void) {
    // ====== CLOCK ======
    // 10 MHz clock (maximum for 2.8 V supply voltage)
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);

    // ====== CPU ======
    // the boot section has the interrupt vector table.
    _PROTECTED_WRITE(CPUINT.CTRLA, CPUINT_IVSEL_bm);

    // ====== PORT ======
    // TX, buzzer -, buzzer +, MOSI, SCK
    VPORTA.DIR = PIN2_bm | PIN3_bm | PIN4_bm | PIN6_bm;
    // status LED, display SS, display reset, display D/C
    VPORTC.DIR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;
    // flash SS, eeprom SS, enable VBAT level
    VPORTF.DIR = PIN0_bm | PIN1_bm | PIN6_bm;

    // set buzzer H-bridge inputs high initially
    // there are hardware pull-down so better to set low to avoid sound artifacts on startup.
    VPORTA.OUT = 0;
    // set all CS lines high
    sys_spi_deselect_all();

    // note: both edges is needed for asynchronous sensing, needed to wake up from deep power down.
    PORTD.PIN0CTRL = PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN1CTRL = PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN2CTRL = PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN3CTRL = PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN4CTRL = PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN5CTRL = PORT_ISC_BOTHEDGES_gc;

    // ====== SPI ======
    // master, 5 MHz SCK, mode 0, MSB first, buffered, no interrupts.
    SPI0.CTRLB = SPI_BUFEN_bm | SPI_MODE_0_gc | SPI_SSD_bm;
    SPI0.CTRLA = SPI_MASTER_bm | SPI_CLK2X_bm | SPI_PRESC_DIV4_gc | SPI_ENABLE_bm;

    // ====== TCA ======
    // Prescaler 2, split mode, single slope PWM on compare channel 3.
    // PWM is output on PA3 for buzzer. Only high timer is used, low timer is unused.
    TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;
    TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP0EN_bm;
    TCA0.SPLIT.HPER = SYS_SOUND_PWM_MAX - 1;
    TCA0.SPLIT.HCMP0 = 0;
    TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV2_gc;

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
    //RTC.CTRLA is set in sys_init_wakeup()

    // === ADC & VREF ===
    // 10-bit resolution, 64 samples accumulation, 78 kHz ADC clock,
    // use 2V5 voltage reference & enable result ready interrupt.
    VREF.CTRLA = VREF_ADC0REFSEL_2V5_gc;
    ADC0.CTRLB = ADC_SAMPNUM_ACC64_gc;
    ADC0.CTRLC = ADC_SAMPCAP_bm | ADC_REFSEL_INTREF_gc | ADC_PRESC_DIV128_gc;
    ADC0.INTCTRL = ADC_RESRDY_bm;

    // === SLEEP ===
    sleep_enable();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    // enable interrupts
    sei();
}

static void sys_init_power_monitor(void) {
    // PIT: interrupt every 1 s for battery sampling and sleep countdown.
    while (RTC.PITSTATUS != 0);
    RTC.PITINTCTRL = RTC_PI_bm;
    RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;
}

void sys_init(void) {
    uint8_t reset_flags = RSTCTRL.RSTFR;
    if (reset_flags == 0) {
        // dirty reset, reset cleanly.
        sys_led_set();
        _delay_ms(1000);
        sys_reset_system();
    }
    RSTCTRL.RSTFR = reset_flags;

    sys_init_registers();
    sys_display_preinit();
    sys_init_wakeup();
}

void sys_init_sleep(void) {
    RTC.CTRLA = 0;
    RTC.PITCTRLA = 0;

    // disable all peripherals to reduce current consumption
    sys_power_set_15v_reg_enabled(false);
    sys_display_set_enabled(false);
    sys_display_sleep();
    sys_sound_set_output_enabled(false);
    sys_power_end_sampling();
    sys_flash_sleep();
    sys_led_clear();
}

void sys_init_wakeup(void) {
    // initialize display
    sys_display_init();
    sys_display_clear(DISPLAY_COLOR_BLACK);

    // check battery level
    ADC0.CTRLA = ADC_RESSEL_10BIT_gc | ADC_ENABLE_bm;
    sys_power_start_sampling();
    sys_power_wait_for_sample();
    // note: at this point display color is 0, so load should be about 0 too.
    // the first measurement isn't terribly precise, it's mostly an undervoltage protection.
    sys_power_update_battery_level(0);
    sys_init_power_monitor();

    // turn display on
    sys_power_set_15v_reg_enabled(true);
    sys_display_set_enabled(true);

    // update input immediately so that the wakeup button press is not registered.
    sys_input_update_state_immediate();
    sys_input_reset_inactivity();

    // initialize sound output
    sys_sound_update_output_state();
    sys_sound_set_channel_volume(2, SOUND_CHANNEL2_VOLUME0);

    sys_flash_wakeup();

    while (RTC.STATUS & RTC_CTRLABUSY_bm);
    RTC.CTRLA = RTC_PRESCALER_DIV128_gc | RTC_RTCEN_bm;
}

#endif //BOOTLOADER
