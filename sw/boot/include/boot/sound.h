
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

#ifndef BOOT_SOUND_H
#define BOOT_SOUND_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Enable or disable buzzer output.
 */
void sys_sound_set_output_enabled(bool enabled);

/**
 * Update sound state after one system time counter tick.
 * Implemented in the core.
 */
void sys_sound_update(void);

#endif //BOOT_SOUND_H
