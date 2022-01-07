
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

#ifdef SIMULATION

#ifndef SIM_POWER_H
#define SIM_POWER_H

#include <stdint.h>

#include <sys/power.h>

/**
 * Called every second.
 */
void power_monitor_update(void);

/**
 * Set current battery status.
 */
void power_set_battery_status(battery_status_t status);

/**
 * Set current battery level in percent.
 * Battery level is set to 100% by default and doesn't change.
 */
void power_set_battery_level(uint8_t level);

/**
 * Returns true if simulator is "sleeping".
 */
bool power_is_sleeping(void);

/**
 * Called to disable "sleep".
 * This will continue execution after the `power_enable_sleep()` call in the game loop thread.
 */
void power_disable_sleep(void);

#endif //SIM_POWER_H

#endif //SIMULATION
