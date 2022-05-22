
/*
 * Copyright 2022 Nicolas Maltais
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

#ifndef SYS_CALLBACK_H
#define SYS_CALLBACK_H

#include <stdbool.h>

// These function definitions are the internally called version of the callbacks.
// These point to an entry in the callback vector table in the app code.

// see core/callbacks.h for callback documentation.

bool __callback_loop(void);

void __callback_draw(void);

void __callback_sleep(void);

void __callback_wakeup(void);

void __callback_sleep_scheduled(void);

void __vector_uart_dre(void);

void __vector_uart_rxc(void);

void __callback_setup(void);

#endif //SYS_CALLBACK_H
