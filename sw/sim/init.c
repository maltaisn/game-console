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
#include <sim/time.h>
#include <sim/sound.h>
#include <sim/flash.h>
#include <sim/eeprom.h>

void sys_init(void) {
    sim_eeprom_load_erased();
    sim_flash_load_erased();

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
    sys_flash_sleep();
    sys_led_clear();
}

void sys_init_wakeup(void) {
    // check battery level on startup
    sys_power_start_sampling();
    sys_power_wait_for_sample();
    sys_power_schedule_sleep_if_low_battery(false);
    sim_time_start();

    // initialize display
    sys_display_init();
    sys_power_set_15v_reg_enabled(true);
    sys_display_set_enabled(true);

    sys_input_update_state_immediate();
    sys_input_reset_inactivity();

    // initialize sound output
    sys_sound_set_output_enabled(true);
    sim_sound_open_stream();

    sys_flash_wakeup();
}
