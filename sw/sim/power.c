
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

#include <boot/init.h>
#include <boot/power.h>
#include <boot/display.h>
#include <boot/input.h>
#include <boot/sound.h>

#include <sys/callback.h>
#include <sys/power.h>

#include <core/trace.h>

#include <stdio.h>
#include <stdatomic.h>

#define VBAT_MAX 4050
#define VBAT_MIN 3300

#define VBAT_MAX_ADC 58393
#define VBAT_MIN_ADC 47579

#define BATTERY_PERCENT_UNKNOWN 0xff

static battery_status_t battery_status = BATTERY_DISCHARGING;
static uint8_t battery_percent = 100;
static bool reg_15v_enabled;
static bool sleep_scheduled = false;
static bool sleep_allow_wakeup;
static sleep_cause_t sleep_cause;
static uint8_t sleep_countdown;

static volatile atomic_bool sleeping = false;

void sys_power_start_sampling(void) {
    // no-op
}

void sys_power_end_sampling(void) {
    // no-op
}

void sys_power_wait_for_sample(void) {
    // no-op
}

battery_status_t sys_power_get_battery_status(void) {
    return battery_status;
}

uint16_t sys_power_get_battery_level_average(void) {
    return VBAT_MIN_ADC + (VBAT_MAX_ADC - VBAT_MIN_ADC) * battery_percent / 100;
}

uint8_t sys_power_get_battery_percent(void) {
    if (battery_status == BATTERY_DISCHARGING) {
        return battery_percent;
    } else {
        return BATTERY_PERCENT_UNKNOWN;
    }
}

uint16_t sys_power_get_battery_voltage(void) {
    // not very realistic, but just linearly interpolate voltage from percent level.
    return VBAT_MIN + (VBAT_MAX - VBAT_MIN) * battery_percent / 100;
}

bool sys_power_is_15v_reg_enabled(void) {
    return reg_15v_enabled;
}

void sys_power_set_15v_reg_enabled(bool enabled) {
    if (!sleeping) {
        sys_display_set_gpio(enabled ? SYS_DISPLAY_GPIO_OUTPUT_HI : SYS_DISPLAY_GPIO_OUTPUT_LO);
        reg_15v_enabled = enabled;
    }
}

void sys_power_schedule_sleep(sleep_cause_t cause, bool allow_wakeup, bool countdown) {
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
        sleep_countdown = SYS_POWER_SLEEP_COUNTDOWN;
        __callback_sleep_scheduled();
        return;
    }
    sys_power_enable_sleep();
}

void sys_power_schedule_sleep_if_low_battery(bool countdown) {
    if (battery_status == BATTERY_DISCHARGING && power_get_battery_percent() == 0) {
        sys_power_schedule_sleep(SLEEP_CAUSE_LOW_POWER, false, countdown);
        // prevent the screen from being dimmed in the meantime.
        sys_input_reset_inactivity();
        // disable sound output since display has been replaced with low battery warning anyway.
        sys_sound_set_output_enabled(false);
    }
}

void sys_power_schedule_sleep_cancel(void) {
    if (sleep_scheduled) {
        sleep_scheduled = false;
        sleep_cause = SLEEP_CAUSE_NONE;
        trace("scheduled sleep cancelled");
    }
}

sleep_cause_t sys_power_get_scheduled_sleep_cause(void) {
    return sleep_cause;
}

bool sys_power_is_sleep_due(void) {
    return sleep_scheduled && sleep_countdown == 0;
}

void sys_power_enable_sleep(void) {
    __callback_sleep();

    sleep_scheduled = false;
    sleep_cause = SLEEP_CAUSE_NONE;

    sys_init_sleep();

    // go to sleep
    sleeping = true;
    trace("sleep enabled");
    while (sleeping || !sleep_allow_wakeup) {
        // sleep thread for 100 ms, better than busy loop.
        sim_time_sleep(100000);
    }

    // --> wake-up from sleep
    trace("sleep disabled");
    sys_init_wakeup();
    __callback_wakeup();
}

void sim_power_monitor_update(void) {
    sys_input_update_inactivity();

    if (sleep_scheduled && sleep_countdown != 0) {
        --sleep_countdown;
        trace("sleep countdown = %d", sleep_countdown);
    }

    sys_power_start_sampling();
    sys_power_schedule_sleep_if_low_battery(true);
}

void sim_power_set_battery_status(battery_status_t status) {
    battery_status = status;
}

void sim_power_set_battery_percent(uint8_t percent) {
    battery_percent = percent;
}

bool sim_power_is_sleeping(void) {
    return sleeping;
}

void sim_power_disable_sleep(void) {
    sleeping = false;
}

