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

#include <sys/init.h>
#include <sys/power.h>
#include <sys/sound.h>
#include <sys/display.h>
#include <sys/led.h>
#include <sys/input.h>
#include <sim/power.h>
#include <sim/time.h>
#include <sim/sound.h>

#include <pthread.h>
#include <math.h>

#define SYSTICK_RATE (1.0 / SYSTICK_FREQUENCY)
#define POWER_MONITOR_RATE 1.0

static bool rtc_enabled;
static bool power_monitor_enabled;

static double last_time_update;
static double last_power_monitor_update;

static void* callback_systick(void* arg) {
    while (true) {
        const double time = time_sim_get();

        // RTC update
        // Of course the OS can't keep up at the 256 Hz rate, make up for any missed updates
        // by calling time_update() multiple times.
        long systick_elapsed = (long) ((time - last_time_update) / SYSTICK_RATE);
        if (systick_elapsed > 10) {
            // If 10 updates were missed it's probably not normal, do a single update.
            systick_elapsed = 1;
            last_time_update = time - SYSTICK_RATE;
        }
        while (systick_elapsed--) {
            if (rtc_enabled) {
                time_update();
            }
            last_time_update += SYSTICK_RATE;
        }

        // Power monitor update
        if (time - last_power_monitor_update >= POWER_MONITOR_RATE) {
            if (power_monitor_enabled) {
                power_monitor_update();
            }
            last_power_monitor_update = time;
        }
        time_sleep(500);
    }
    return 0;
}

void init(void) {
    init_wakeup();

    pthread_t thread;
    pthread_create(&thread, NULL, callback_systick, NULL);
}

void init_sleep(void) {
    rtc_enabled = false;
    power_monitor_enabled = false;

    // disable all peripherals to reduce current consumption
    power_set_15v_reg_enabled(false);
    display_set_enabled(false);
    display_sleep();
    sound_set_output_enabled(false);
    sound_close_stream();
    flash_sleep();
    led_clear();
}

void init_wakeup(void) {
    rtc_enabled = true;
    last_time_update = time_sim_get();

    // check battery level on startup
    power_start_sampling();
    power_wait_for_sample();
    power_schedule_sleep_if_low_battery(false);
    last_power_monitor_update = time_sim_get();
    power_monitor_enabled = true;

    // initialize display
    display_init();
    power_set_15v_reg_enabled(true);
    display_set_enabled(true);

    input_update_state_immediate();
    input_reset_inactivity();

    // initialize sound output
    sound_set_output_enabled(true);
    sound_open_stream();

    flash_wakeup();
}
