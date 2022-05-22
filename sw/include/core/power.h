
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

#ifndef CORE_POWER_H
#define CORE_POWER_H

#include <stdint.h>

typedef enum {
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

    BATTERY_STATUS_COUNT
} battery_status_t;

typedef enum {
    /** No upcoming sleep. */
    SLEEP_CAUSE_NONE,
    /** Upcoming sleep due to inactivity. */
    SLEEP_CAUSE_INACTIVE,
    /** Upcoming sleep due to low battery level. */
    SLEEP_CAUSE_LOW_POWER,
} sleep_cause_t;

/**
 * Get current battery status.
 * Returns `BATTERY_UNKNOWN` status if not sampled yet or sampling failed.
 */
battery_status_t power_get_battery_status(void);

/**
 * Returns the estimated battery level.
 * Battery level must have been sampled previously with `power_start_sampling()`.
 * Battery level is only available if battery is currently discharging.
 * The level is a percentage from 0 to 100. The system should shutdown at level 0.
 * Must not be called within interrupt.
 */
uint8_t power_get_battery_percent(void);

/**
 * Returns the scheduled sleep cause, or `SLEEP_CAUSE_NONE` if sleep has not been scheduled.
 * There's a countdown of `POWER_DOWN_SLEEP` seconds on sleep to let the game save its state.
 */
sleep_cause_t power_get_scheduled_sleep_cause(void);

#include <sim/power.h>

#endif //CORE_POWER_H
