
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

#include <core/power.h>

#define SYS_POWER_SLEEP_COUNTDOWN 3
#define SYS_POWER_INACTIVE_COUNTDOWN_SLEEP 90
#define SYS_POWER_INACTIVE_COUNTDOWN_DIM 60

extern volatile battery_status_t sys_power_battery_status;
extern volatile uint8_t sys_power_battery_percent;
extern volatile sleep_cause_t sys_power_sleep_cause;

/** Get average battery level ADC reading, for internal use. */
uint16_t sys_power_get_battery_level_average(void);

// see core/power.h for documentation
battery_status_t sys_power_get_battery_status(void);

// see core/power.h for documentation
uint8_t sys_power_get_battery_percent(void);

/**
 * Returns the approximate battery voltage in mV, for debug purposes.
 * Battery must be discharging and battery level must have been sampled previously.
 * Must not be called within interrupt.
 */
uint16_t sys_power_get_battery_voltage(void);

// see core/power.h for documentation
sleep_cause_t sys_power_get_scheduled_sleep_cause(void);

/**
 * Returns true if sleep countdown has expired and device is due to go to sleep.
 * This will return true at least once in main game loop before going to sleep.
 */
bool sys_power_is_sleep_due(void);

#endif //SYS_POWER_H
