
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

#ifndef SYS_POWER_H
#define SYS_POWER_H

#include <stdint.h>
#include <stdbool.h>

#ifndef POWER_SLEEP_COUNTDOWN
#define POWER_SLEEP_COUNTDOWN 5
#endif

typedef enum battery_status {
    /** Battery status is unknown (not yet sampled or outside known values). */
    BATTERY_UNKNOWN = 0x00,
    /** Battery not connected. */
    BATTERY_NONE = 0x01,
    /** USB connected, battery is charging (red LED). */
    BATTERY_CHARGING = 0x02,
    /** USB connected, battery charge is complete (green LED). */
    BATTERY_CHARGED = 0x03,
    /** USB disconnected, battery is discharging. */
    BATTERY_DISCHARGING = 0x04,
} battery_status_t;

/**
 * Start taking sample for battery status & battery level (if discharging).
 * Sampling takes at least 110-220k cycles.
 * This gets called automatically on startup & every second by the PIT.
 */
void power_start_sampling(void);

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
 * Returns the estimated battery level.
 * Battery level must have been sampled previously with `power_start_sampling()`.
 * Battery level is only available if battery is currently discharging.
 * The level is a percentage from 0 to 100. The system should shutdown at level 0.
 * Must not be called within interrupt.
 */
uint8_t power_get_battery_percent(void);

/**
 * Returns the approximate battery voltage in mV, for debug purposes.
 * Battery must be discharging and battery level must have been sampled previously.
 * Must not be called within interrupt.
 */
uint16_t power_get_battery_voltage(void);

/**
 * Returns true if the +15V regulator is currently enabled.
 */
bool power_is_15v_reg_enabled(void);

/**
 * Enable or disable the +15V regulator for the display.
 * Must not be called within an interrupt.
 */
void power_set_15v_reg_enabled(bool enabled);

/**
 * Enable system sleep.
 * Interrupts are disabled so device cannot wake up from sleep until reset.
 */
void power_enable_sleep(void);

/**
 * Schedule sleep if battery is discharging and percentage is 0%.
 * If sleep is scheduled, a short countdown will start to allow the game to save its state.
 * Calling this function again will update the countdown, until it reaches 0 and sleep is enabled.
 * If `countdown` is false and battery is low, sleep will be enabled directly.
 * This function should not be called directly by game code.
 */
void power_schedule_sleep_if_low_battery(bool countdown);

/**
 * Returns true if sleep is scheduled.
 * There's a countdown on sleep to let the game save its state and display the power down UI.
 */
bool power_is_sleep_scheduled(void);

#endif //SYS_POWER_H
