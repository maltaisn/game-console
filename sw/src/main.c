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

#include <stdbool.h>

#include <sys/uart.h>
#include <sys/power.h>
#include <sys/init.h>
#include <sys/led.h>
#include "sys/time.h"
#include "sys/input.h"

//#define TEST_BUTTON
//#define TEST_BATTERY
#define TEST_UART

#if defined(TEST_BATTERY)
static const char* status_names[] = {
        "unknown", "no battery", "charging", "charged", "discharging",
};

static inline void test(void) {
    systime_t last_time = 0;
    while (true) {
        systime_t time;
        do {
            time = time_get();
        } while (time - last_time < millis_to_ticks(1000));
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
#elif defined(TEST_BUTTON)
static inline void test(void) {
    uint8_t last_state = 0;
    while (true) {
        uint8_t curr_state = input_get_state();
        uint8_t mask = 1;
        bool pressed = false;
        for (uint8_t i = 0; i < BUTTONS_COUNT; ++i) {
            uint8_t curr = curr_state & mask;
            uint8_t last = last_state & mask;
            if (!last && curr) {
                printf("button %u pressed\n", i);
                pressed = true;
            } else if (last && !curr) {
                printf("button %u released\n", i);
            }
            mask <<= 1;
        }
        if (pressed) {
            led_toggle();
        }
        last_state = curr_state;
    }
}
#elif defined(TEST_UART)
static inline void test(void) {
    while (true) {
        // loopback
        led_clear();
        uint8_t data = uart_read();
        led_set();
        uart_write(data);
    }
}
#endif

int main(void) {
    init();

    stdout = &uart_output;
    test();
}
