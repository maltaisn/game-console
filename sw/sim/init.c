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

#include <boot/init.h>
#include <boot/flash.h>
#include <boot/display.h>
#include <boot/power.h>
#include <boot/sound.h>
#include <boot/input.h>

#include <sys/led.h>
#include <sys/spi.h>
#include <sys/display.h>

#include <sim/time.h>
#include <sim/sound.h>
#include <sim/flash.h>
#include <sim/eeprom.h>
#include <sim/uart.h>

void sys_init(void) {
    sim_time_init();
    sim_sound_init();

    sim_eeprom_init();
    sim_flash_init();

    sys_init_wakeup();
}

void sys_init_sleep(void) {
    // disable all peripherals to reduce current consumption
    sim_time_stop();
    sys_power_set_15v_reg_enabled(false);
    sys_display_set_enabled(false);
    sys_display_sleep();
    sys_sound_set_output_enabled(false);
    sim_sound_close_stream();
    sys_power_end_sampling();
    sys_flash_sleep();
    sys_led_clear();
}

void sys_init_wakeup(void) {
    // initialize display
    sys_display_init();
    sys_display_clear(DISPLAY_COLOR_BLACK);

    // check battery level
    sys_power_start_sampling();
    sys_power_wait_for_sample();
    // note: at this point display color is 0, so load should be about 0 too.
    // the first measurement isn't terribly precise, it's mostly an undervoltage protection.
    sys_power_update_battery_level(0);
    sim_time_start();

    // turn display on
    sys_power_set_15v_reg_enabled(true);
    sys_display_set_enabled(true);

    // update input immediately so that the wakeup button press is not registered.
    sys_input_update_state_immediate();
    sys_input_reset_inactivity();

    // initialize sound output
    sys_sound_set_output_enabled(true);
    sim_sound_open_stream();

    sys_flash_wakeup();
}

void sim_deinit(void) {
#ifdef SYS_UART_ENABLE
    sim_uart_end();
#endif
    sim_flash_free();
    sim_eeprom_free();
    sim_sound_terminate();
}
