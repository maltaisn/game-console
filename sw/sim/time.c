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

#include <sys/input.h>
#include <sys/led.h>

#include <core/sound.h>

#define SYSTICK_MAX 0xffffff

void time_update(void) {
    input_update_state();
    sound_update();
    led_blink_update();
}

#ifndef SIMULATION_HEADLESS

#include <time.h>
#include <math.h>

static struct timespec start_time;

static double get_elapsed_time(const struct timespec* start, const struct timespec* end) {
    return (double) (end->tv_sec - start->tv_sec) +
           (double) (end->tv_nsec - start->tv_nsec) / 1e9;
}

void time_init(void) {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

systime_t time_get() {
    return (systime_t) (lround(time_sim_get() * SYSTICK_FREQUENCY)) & SYSTICK_MAX;
}

double time_sim_get(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return get_elapsed_time(&start_time, &time);
}

void time_sleep(uint32_t us) {
    struct timespec remaining, request = {0, us * 1000};
    nanosleep(&request, &remaining);
}

#else

static systime_t systick;

void time_init(void) {
    systick = 0;
}

systime_t time_get() {
    return systick & SYSTICK_MAX;
}

double time_sim_get(void) {
    return ((double) systick / SYSTICK_FREQUENCY);
}

void time_sleep(uint32_t us) {
    systick += us;
}

#endif //SIMULATION_HEADLESS