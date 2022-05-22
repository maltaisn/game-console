
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
#include <sys/time.h>

/**
 * Initialize time module.
 */
void sim_time_init(void);

/**
 * Do an update of the time module, must be called once every system tick.
 */
void sim_time_update(void);

/**
 * Return time elapsed since start of simulation, in seconds.
 */
double sim_time_get(void);

/**
 * Sleep thread for a number of us.
 * In headless mode, this increases the current time instantly.
 */
void sim_time_sleep(uint32_t us);

#endif //SIM_TIME_H

#endif //SIMULATION
