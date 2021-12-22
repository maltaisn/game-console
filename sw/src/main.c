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

#include <main.h>
#include <util/delay.h>
#include <stdbool.h>

#include <sys/uart.h>
#include <sys/power.h>
#include <sys/init.h>
#include <sys/led.h>
#include "sys/time.h"

static const char* status_names[] = {
        "unknown", "no battery", "charging", "charged", "discharging",
};

int main(void) {
    init();

    stdout = &uart_output;

    systime_t last_time = 0;
    while (true) {
        power_take_sample();
        systime_t time;
        do {
            time = time_get();
        } while (time - last_time < 256);
        last_time = time;

        battery_status_t status = power_get_battery_status();
        if (status == BATTERY_DISCHARGING) {
            uint8_t percent = power_get_battery_percent();
            uint16_t voltage = power_get_battery_voltage();
            printf("status = %s, level = %u%% (%u mV)\n", status_names[status], percent, voltage);
        } else {
            printf("status = %s\n", status_names[status]);
        }
        led_toggle();
    }
}
