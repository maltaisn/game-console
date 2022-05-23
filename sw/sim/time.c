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

#include <sim/time.h>
#include <sys/time.h>
#include <core/power.h>

#include <boot/input.h>
#include <boot/sound.h>
#include <boot/led.h>

#ifndef SIMULATION_HEADLESS

#include <time.h>
#include <math.h>

#endif

#define SYSTICK_MAX 0xffffff
#define SYSTICK_RATE (1.0 / SYSTICK_FREQUENCY)
#define POWER_MONITOR_RATE 1.0

static bool rtc_enabled;
static bool power_monitor_enabled;

static double last_time_update;
static double last_power_monitor_update;

static void sim_time_update_single(void) {
    sys_input_update_state();
    sys_sound_update();
    sys_led_blink_update();
}

void sim_time_update(void) {
#ifdef SIMULATION_HEADLESS
    sim_time_update_single();
#else
    const double time = sim_time_get();

    // RTC update
    // Of course the OS can't keep up at the 256 Hz rate, make up for any missed updates
    // by calling sim_time_update() multiple times.
    long systick_elapsed = (long) ((time - last_time_update) / SYSTICK_RATE);
    if (systick_elapsed > 10) {
        // If 10 updates were missed it's probably not normal, do a single update.
        systick_elapsed = 1;
        last_time_update = time - SYSTICK_RATE;
    }
    if (systick_elapsed > 0) {
        while (systick_elapsed--) {
            if (rtc_enabled) {
                sim_time_update_single();
            }
            last_time_update += SYSTICK_RATE;
        }
    }

    // Power monitor update
    if (time - last_power_monitor_update >= POWER_MONITOR_RATE) {
        if (power_monitor_enabled) {
            sim_power_monitor_update();
        }
        last_power_monitor_update = time;
    }
#endif //SIMULATION_HEADLESS
}

void sim_time_start(void) {
    rtc_enabled = true;
    last_time_update = sim_time_get();
    last_power_monitor_update = sim_time_get();
    power_monitor_enabled = true;
}

void sim_time_stop(void) {
    rtc_enabled = false;
    power_monitor_enabled = false;
}

#ifndef SIMULATION_HEADLESS

static struct timespec start_time;

static double get_elapsed_time(const struct timespec* start, const struct timespec* end) {
    return (double) (end->tv_sec - start->tv_sec) +
           (double) (end->tv_nsec - start->tv_nsec) / 1e9;
}

systime_t sys_time_get() {
    return (systime_t) (lround(sim_time_get() * SYSTICK_FREQUENCY)) & SYSTICK_MAX;
}

void sim_time_init(void) {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

double sim_time_get(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return get_elapsed_time(&start_time, &time);
}

void sim_time_sleep(uint32_t us) {
    struct timespec remaining, request = {0, us * 1000};
    nanosleep(&request, &remaining);
}

#else

static systime_t systick;

void sim_time_init(void) {
    systick = 0;
}

systime_t sys_time_get() {
    return systick & SYSTICK_MAX;
}

double sim_time_get(void) {
    return ((double) systick / SYSTICK_FREQUENCY);
}

void sim_time_sleep(uint32_t us) {
    systick += us;
}

#endif //SIMULATION_HEADLESS