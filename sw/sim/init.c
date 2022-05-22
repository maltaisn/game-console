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
#include <sim/power.h>
#include <sim/time.h>
#include <sim/sound.h>
#include <sim/flash.h>
#include <sim/eeprom.h>

#include <pthread.h>

#define SYSTICK_RATE (1.0 / SYSTICK_FREQUENCY)
#define POWER_MONITOR_RATE 1.0

static bool rtc_enabled;
static bool power_monitor_enabled;

static double last_time_update;
static double last_power_monitor_update;

#ifndef SIMULATION_HEADLESS
static void* callback_systick(void* arg) {
    while (true) {
        const double time = sim_time_get();

        // RTC update
        // Of course the OS can't keep up at the 256 Hz rate, make up for any missed updates
        // by calling sim_time_update() multiple times.
        long systick_elapsed = (long) ((time - last_time_update) / SYSTICK_RATE);
        if (systick_elapsed > 10) {
            // If 10 updates were missed it's probably not normal, do a single update.
            systick_elapsed = 1;
            last_time_update = time - SYSTICK_RATE;
        }
        if (systick_elapsed > 0) {
            while (systick_elapsed--) {
                if (rtc_enabled) {
                    sim_time_update();
                }
                last_time_update += SYSTICK_RATE;
            }
        }

        // Power monitor update
        if (time - last_power_monitor_update >= POWER_MONITOR_RATE) {
            if (power_monitor_enabled) {
                sim_power_monitor_update();
            }
            last_power_monitor_update = time;
        }
        sim_time_sleep(500);
    }
    return 0;
}
#endif

void sys_init(void) {
    sim_eeprom_load_erased();
    sim_flash_load_erased();

    sys_init_wakeup();

#ifndef SIMULATION_HEADLESS
    pthread_t thread;
    pthread_create(&thread, NULL, callback_systick, NULL);
#endif
}

void sys_init_sleep(void) {
    rtc_enabled = false;
    power_monitor_enabled = false;

    // disable all peripherals to reduce current consumption
    sys_power_set_15v_reg_enabled(false);
    sys_display_set_enabled(false);
    sys_display_sleep();
    sys_sound_set_output_enabled(false);
    sim_sound_close_stream();
    sys_flash_sleep();
    sys_led_clear();
}

void sys_init_wakeup(void) {
    rtc_enabled = true;
    last_time_update = sim_time_get();

    // check battery level on startup
    sys_power_start_sampling();
    sys_power_wait_for_sample();
    sys_power_schedule_sleep_if_low_battery(false);
    last_power_monitor_update = sim_time_get();
    power_monitor_enabled = true;

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
