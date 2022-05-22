
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

#ifndef SIM_SOUND_H
#define SIM_SOUND_H

#include <stdbool.h>

/**
 * Initialize sound output.
 */
void sim_sound_init(void);

/**
 * De-initialize sound output.
 */
void sim_sound_terminate(void);

/**
 * Open sound open stream.
 */
void sim_sound_open_stream(void);

/**
 * Close sound open stream.
 */
void sim_sound_close_stream(void);

/**
 * Returns true if sound output is currently enabled.
 */
bool sim_sound_is_output_enabled(void);

#endif //SIM_SOUND_H

#endif //SIMULATION
