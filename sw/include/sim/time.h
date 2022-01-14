
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

#ifndef SIM_TIME_H
#define SIM_TIME_H

#include <time.h>

/**
 * Initialize time module.
 */
void time_init(void);

/**
 * Do the things normally done in periodic RTC interrupt.
 */
void time_update(void);

/**
 * Return time elapsed since start of simulation, in seconds.
 */
double time_sim_get(void);

/**
 * Sleep thread for a number of us.
 */
void time_sleep(long us);

#endif //SIM_TIME_H

#endif //SIMULATION
