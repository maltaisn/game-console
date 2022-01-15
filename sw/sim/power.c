
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

#include <sim/power.h>
#include <sim/sound.h>
#include <sim/time.h>

#include <sys/power.h>
#include <sys/display.h>
#include <sys/spi.h>
#include <sys/input.h>
#include <sys/init.h>

#include <core/trace.h>

#include <stdio.h>
#include <stdatomic.h>
#include "sys/led.h"

#define VBAT_MAX 4050
#define VBAT_MIN 3300

#define BATTERY_PERCENT_UNKNOWN 0xff

static battery_status_t battery_status = BATTERY_DISCHARGING;
static uint8_t battery_percent = 100;
static bool reg_15v_enabled;
static bool sleep_scheduled = false;
static bool sleep_allow_wakeup;
static sleep_cause_t sleep_cause;
static uint8_t sleep_countdown;

static volatile atomic_bool sleeping = false;

void power_monitor_update(void) {
    input_update_inactivity();

    if (sleep_scheduled && sleep_countdown != 0) {
        --sleep_countdown;
        trace("sleep countdown = %d", sleep_countdown);
    }

    power_start_sampling();
    power_schedule_sleep_if_low_battery(true);
}

void power_start_sampling(void) {
    // no-op
}

void power_end_sampling(void) {
    // no-op
}

void power_wait_for_sample(void) {
    // no-op
}

battery_status_t power_get_battery_status(void) {
    return battery_status;
}

uint8_t power_get_battery_percent(void) {
    if (battery_status == BATTERY_DISCHARGING) {
        return battery_percent;
    } else {
        return BATTERY_PERCENT_UNKNOWN;
    }
}

uint16_t power_get_battery_voltage(void) {
    // not very realistic, but just linearly interpolate voltage from percent level.
    return VBAT_MIN + (VBAT_MAX - VBAT_MIN) * battery_percent / 100;
}

void power_set_battery_status(battery_status_t status) {
    battery_status = status;
}

void power_set_battery_percent(uint8_t percent) {
    battery_percent = percent;
}

bool power_is_15v_reg_enabled(void) {
    return reg_15v_enabled;
}

void power_set_15v_reg_enabled(bool enabled) {
    if (!sleeping) {
        display_set_gpio(enabled ? DISPLAY_GPIO_OUTPUT_HI : DISPLAY_GPIO_OUTPUT_LO);
        reg_15v_enabled = enabled;
    }
}

void power_schedule_sleep(sleep_cause_t cause, bool allow_wakeup, bool countdown) {
    if (countdown) {
        if (sleep_scheduled) {
            // already scheduled
            return;
        }
        trace("sleep scheduled, cause = %d, allow_wakeup = %d, countdown = %d",
              cause, allow_wakeup, countdown);
        sleep_scheduled = true;
        sleep_cause = cause;
        sleep_allow_wakeup = allow_wakeup;
        sleep_countdown = POWER_SLEEP_COUNTDOWN;
        power_callback_sleep_scheduled();
        return;
    }
    power_enable_sleep();
}

void power_schedule_sleep_if_low_battery(bool countdown) {
#ifndef DISABLE_BATTERY_PROTECTION
    if (battery_status == BATTERY_DISCHARGING && power_get_battery_percent() == 0) {
        power_schedule_sleep(SLEEP_CAUSE_LOW_POWER, false, countdown);
        // prevent the screen from being dimmed in the meantime.
        input_reset_inactivity();
    }
#endif
}

void power_schedule_sleep_cancel(void) {
    if (sleep_scheduled) {
        sleep_scheduled = false;
        sleep_cause = SLEEP_CAUSE_NONE;
        trace("scheduled sleep cancelled");
    }
}

sleep_cause_t power_get_scheduled_sleep_cause(void) {
    return sleep_cause;
}

bool power_is_sleep_due(void) {
    return sleep_scheduled && sleep_countdown == 0;
}

bool power_is_sleeping(void) {
    return sleeping;
}

void power_disable_sleep(void) {
    sleeping = false;
}

void power_enable_sleep(void) {
    power_callback_sleep();

    sleep_scheduled = false;
    sleep_cause = SLEEP_CAUSE_NONE;

    init_sleep();

    // go to sleep
    sleeping = true;
    trace("sleep enabled");
    while (sleeping || !sleep_allow_wakeup) {
        // sleep thread for 100 ms, better than busy loop.
        time_sleep(100000);
    }

    // --> wake-up from sleep
    trace("sleep disabled");
    init_wakeup();
    power_callback_wakeup();
}

void __attribute__((weak)) power_callback_sleep(void) {
    // do nothing
}

void __attribute__((weak)) power_callback_wakeup(void) {
    // do nothing
}

void __attribute__((weak)) power_callback_sleep_scheduled(void) {
    // do nothing
}
