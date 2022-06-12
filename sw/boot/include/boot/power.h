
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

#ifndef BOOT_POWER_H
#define BOOT_POWER_H

#include <stdint.h>
#include <stdbool.h>

#include <sys/power.h>

// flags used for scheduling sleep.
enum {
    SYS_SLEEP_SCHEDULE_ALLOW_WAKEUP = 1 << 0,
    SYS_SLEEP_SCHEDULE_COUNTDOWN = 1 << 1,
};

/**
 * Start taking sample for battery status & battery level (if discharging).
 * Sampling takes at least 110-220k cycles.
 * This gets called automatically on startup & every second by the PIT.
 */
void sys_power_start_sampling(void);

/**
 * End current sampling, if started.
 */
void sys_power_end_sampling(void);

/**
 * Wait until battery sample is ready.
 */
void sys_power_wait_for_sample(void);

/**
 * Using last ADC sample, contrast and average display color to estimate load,
 * as well as pre-established discharge voltage curve, estimate battery percentage.
 * The flags are passed to `sys_power_schedule_sleep` if battery is too low.
 * Must not be called within an interrupt.
 */
void sys_power_update_battery_level(uint8_t flags);

/**
 * Returns true when the average color should be computed on the next display refresh.
 */
bool sys_power_should_compute_display_color(void);

/**
 * Indicate to the power module that the average display color is done computing.
 */
void sys_power_on_display_color_computed(void);

/**
 * Enable or disable the +15V regulator for the display.
 * Must not be called within an interrupt.
 */
void sys_power_set_15v_reg_enabled(bool enabled);

/**
 * Schedule sleep with a cause (only if enabled).
 * When sleep is scheduled, a short countdown will start to allow the game to save its state.
 * If the countdown is not enabled and wake up is allowed, then this must not be called within an
 * interrupt because sleep will be enabled immediately.
 * Flags are: SYS_SLEEP_SCHEDULE_ALLOW_WAKEUP, SYS_SLEEP_SCHEDULE_COUNTDOWN, and all sleep causes.
 */
void sys_power_schedule_sleep(uint8_t flags);

/**
 * Cancel scheduled sleep if any.
 */
void sys_power_schedule_sleep_cancel(void);

/**
 * Enter sleep mode.
 */
void sys_power_enable_sleep(void);

#endif //BOOT_POWER_H
