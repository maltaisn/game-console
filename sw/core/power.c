
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

#include <core/power.h>

#include <sys/power.h>

battery_status_t power_get_battery_status(void) {
    return sys_power_get_battery_status();
}

uint8_t power_get_battery_percent(void) {
    return sys_power_get_battery_percent();
}

sleep_cause_t power_get_scheduled_sleep_cause(void) {
    return sys_power_get_scheduled_sleep_cause();
}
