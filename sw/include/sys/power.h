
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

#ifndef POWER_H
#define POWER_H

#include <stdint.h>

// battery level if battery is not discharging or if not sampled yet.
#define BATTERY_PERCENT_UNKNOWN 0xff

typedef enum battery_status {
    /** Battery status is unknown (not yet sampled or outside known values). */
    BATTERY_UNKNOWN,
    /** Battery not connected. */
    BATTERY_NONE,
    /** USB connected, battery is charging (red LED). */
    BATTERY_CHARGING,
    /** USB connected, battery charge is complete (green LED). */
    BATTERY_CHARGED,
    /** USB disconnected, battery is discharging. */
    BATTERY_DISCHARGING,
} battery_status_t;

/**
 * Start taking sample for battery status & battery level (if discharging).
 * Sampling takes at least 110-220k cycles.
 */
void power_take_sample(void);

/**
 * Wait until battery sample is ready.
 */
void power_wait_for_sample(void);

/**
 * Get current battery status.
 * Returns `BATTERY_UNKNOWN` status if not sampled yet or sampling failed.
 */
battery_status_t power_get_battery_status(void);

/**
 * If battery level was sampled previously, returns the battery level.
 * Otherwise, returns `BATTERY_PERCENT_UNKNOWN`.
 * Battery level is only available if battery is currently discharging.
 * The level is a percentage from 0 to 100. The system should shutdown at level 0.
 */
uint8_t power_get_battery_percent(void);

/**
 * If battery level was sampled previously, returns the approximate battery voltage in mV.
 * Otherwise, returns 0. Battery voltage is only available if battery is currently discharging.
 */
uint16_t power_get_battery_voltage(void);

/**
 * Enable sleep if battery percentage is 0%.
 * Interrupts are disabled so device cannot wake up from sleep until reset.
 */
void sleep_if_low_battery(void);

#endif //POWER_H
