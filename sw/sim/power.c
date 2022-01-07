
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
#include <sys/power.h>
#include <stdio.h>
#include "sys/display.h"
#include "sys/led.h"
#include "sys/spi.h"
#include "sim/sound.h"
#include "sys/sound.h"

#define VBAT_MAX 4050
#define VBAT_MIN 3300

#define BATTERY_PERCENT_UNKNOWN 0xff

static battery_status_t battery_status = BATTERY_DISCHARGING;
static uint8_t battery_percent = 100;
static bool reg_15v_enabled;
static bool sleep_scheduled = false;
static uint8_t sleep_countdown;
static bool sleeping;

void power_start_sampling(void) {
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

void power_set_battery_level(uint8_t level) {
    battery_percent = level;
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

bool power_is_sleeping(void) {
    return sleeping;
}

void power_enable_sleep(void) {
    sound_terminate();
    power_set_15v_reg_enabled(false);
    sound_set_output_enabled(false);
    flash_power_down();
    led_clear();
    spi_deselect_all();

    sleeping = true;
    puts("power_enable_sleep: sleep enabled");
}

void power_schedule_sleep_if_low_battery(bool countdown) {
#ifndef DISABLE_BAT_PROT
    if (sleep_scheduled) {
        --sleep_countdown;
        printf("power_schedule_sleep_if_low_battery: sleep countdown = %d\n", sleep_countdown);
        if (sleep_countdown) {
            return;
        }
    } else if (battery_status == BATTERY_DISCHARGING && power_get_battery_percent() == 0) {
        sleep_scheduled = true;
        if (countdown) {
            puts("power_schedule_sleep_if_low_battery: sleep scheduled");
            sound_set_output_enabled(false);
            sleep_countdown = POWER_SLEEP_COUNTDOWN;
            return;
        }
    } else {
        return;
    }
    power_enable_sleep();
#endif //DISABLE_BAT_PROT
}

bool power_is_sleep_scheduled(void) {
    return sleep_scheduled;
}
