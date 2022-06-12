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

#include <core/callback.h>
#include <sys/callback.h>

#include <stdbool.h>

#define CALLBACK(name, delegate, ret_type)        \
    __attribute__((weak)) ret_type name(void) {   \
        return (ret_type) 0;                      \
    }                                             \
    ret_type delegate(void) {                     \
        return name();                            \
    }                                             \


CALLBACK(callback_setup, __callback_setup, void)

CALLBACK(callback_loop, __callback_loop, bool)

CALLBACK(callback_draw, __callback_draw, void)

CALLBACK(callback_sleep, __callback_sleep, void)

CALLBACK(callback_wakeup, __callback_wakeup, void)

CALLBACK(callback_sleep_scheduled, __callback_sleep_scheduled, void)

CALLBACK(__vector_uart_dre, __callback_uart_dre, void)

CALLBACK(__vector_uart_rxc, __calback_uart_rxc, void)
