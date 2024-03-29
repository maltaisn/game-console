
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

#ifndef SYS_INPUT_H
#define SYS_INPUT_H

#include <stdint.h>
#include <core/defs.h>

extern uint8_t sys_input_state;
extern uint8_t sys_input_curr_state;
extern uint8_t sys_input_last_state;

// see core/input.h for documentation

void sys_input_latch(void);

uint8_t sys_input_get_state(void);

uint8_t sys_input_get_last_state(void);

#endif //SYS_INPUT_H
