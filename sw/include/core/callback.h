
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

#ifndef CORE_CALLBACK_H
#define CORE_CALLBACK_H

#include <stdbool.h>

/**
 * Function called at the start of the app.
 * Note that this is the only callback that MUST be implemented.
 */
void callback_setup(void);

/**
 * Function called in a loop during app execution.
 * Returns true if display should be refreshed,
 * in which case `callback_draw` will be invoked next.
 */
bool callback_loop(void);

/**
 * Function called to render the display.
 * This function should execute the same every time since it will be
 * called in a loop for each display page!
 */
void callback_draw(void);

/**
 * Function called right before CPU is put to sleep.
 */
void callback_sleep(void);

/**
 * Function called right after CPU wakes up from sleep.
 */
void callback_wakeup(void);

/**
 * Function called when sleep is scheduled.
 */
void callback_sleep_scheduled(void);

/**
 * Called from the UART data register empty interrupt, if UART is enabled.
 * This should be treated the same as an interrupt body.
 * The "__vector" naming is required to avoid a compiler warning.
 */
void __vector_uart_dre(void);

/**
 * Called from the UART receive complete interrupt, if UART is enabled.
 * This should be treated the same as an interrupt body.
 * The "__vector" naming is required to avoid a compiler warning.
 */
void __vector_uart_rxc(void);

#endif //CORE_CALLBACK_H
