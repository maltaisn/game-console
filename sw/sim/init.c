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

#include <GL/glut.h>

static bool rtc_enabled = false;
static bool power_monitor_enabled = false;

static void callback_time_timer(int arg) {
    glutTimerFunc((unsigned int) (1000.0 / SYSTICK_FREQUENCY + 0.5), callback_time_timer, 0);
    if (rtc_enabled) {
        time_update();
    }
}

static void callback_power_monitor(int arg) {
    glutTimerFunc(1000, callback_power_monitor, 0);
    if (power_monitor_enabled) {
        power_monitor_update();
    }
}

void init(void) {
    init_wakeup();

    // this gives a 4 ms timer which is not terribly accurate (should be 3.91 ms) but will
    // work fine for the simulator purposes.
    glutTimerFunc((unsigned int) (1000.0 / SYSTICK_FREQUENCY + 0.5), callback_time_timer, 0);
    glutTimerFunc(1000, callback_power_monitor, 0);
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

    // check battery level on startup
    power_start_sampling();
    power_wait_for_sample();
    power_schedule_sleep_if_low_battery(false);
    power_monitor_enabled = true;

    // initialize display
    display_init();
    power_set_15v_reg_enabled(true);
    display_set_enabled(true);

    input_reset_inactivity();

    // initialize sound output
    sound_set_output_enabled(true);
    sound_open_stream();

    flash_wakeup();
}
