
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

#define VBAT_MAX 4050
#define VBAT_MIN 3300

#define BATTERY_PERCENT_UNKNOWN 0xff

static battery_status_t battery_status = BATTERY_DISCHARGING;
static uint8_t battery_percent = 100;
static bool reg_15v_enabled;
static bool sleeping;

void power_take_sample(void) {
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

void sleep_if_low_battery(void) {
    if (battery_percent == 0) {
        power_set_15v_reg_enabled(false);
        led_clear();
        spi_deselect_all();
        sound_terminate();

        sleeping = true;
        puts("Low battery, sleep enabled.");
    }
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
