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

/*
 * This section generates a vector table for the callbacks, without assembly.
 * It simplifies the build a little but there's a few macro hacks.
 *
 * The generated vector table looks like:
 *
 *   00:	0c 94 15 02 	jmp	callback_loop_delegate   (this callback is used)
 *   04:	0c 94 99 00 	jmp	callback_draw_delegate   (this callback is also used)
 *   08:	08 95       	ret                          (this callback is unused)
 *
 * Each vector entry is in its own section which is marked with KEEP() in the linker script.
 */

#define CONCAT2(x, y) x##y
#define CONCAT3(x, y, z) x##y##z

#ifdef SIMULATION
#define ATTR_SIGNAL
#define CALLBACK_ATTR(name, ret_type) __attribute__((used))
#else
#define ATTR_SIGNAL __attribute__((signal))
#define CALLBACK_ATTR(name, ret_type) \
            __attribute__((section(".app." # name))) __attribute__((used))
#endif

/**
 * This defines the entry in the callback vector table named '__name'.
 * It also defines a weakly-linked empty function 'name' which is defined in the callbacks header.
 * The vector table function calls a noinline delegate function, so that the entry itself
 * is at most 4 bytes.
 */
#define CALLBACK(name, ret_type, delegate)                                 \
    CALLBACK_ATTR(name, ret_type) ret_type CONCAT2(__, name)(void) {       \
        return delegate();                                                 \
    }                                                                      \
    __attribute__((weak)) ret_type name(void) {                            \
        return (ret_type) 0;                                               \
    }

/**
 * Same as CALLBACK() but also defines the delegate.
 * The delegate calls the 'name' function, which is guaranteed to be inlined.
 */
#define CALLBACK_DELEGATE(name, ret_type, ...)               \
    __attribute__((noinline)) __VA_ARGS__                    \
    static ret_type CONCAT3(__, name, _delegate)(void) {     \
        return name();                                       \
    }                                                        \
    CALLBACK(name, ret_type, CONCAT3(__, name, _delegate))

// The callback vector table definition.

CALLBACK_DELEGATE(callback_loop, bool)

CALLBACK_DELEGATE(callback_draw, void)

CALLBACK_DELEGATE(callback_sleep, void)

CALLBACK_DELEGATE(callback_wakeup, void)

CALLBACK_DELEGATE(callback_sleep_scheduled, void)

// see uart.c for signal attribute explanation.
CALLBACK_DELEGATE(vector_uart_dre, void, ATTR_SIGNAL)

CALLBACK_DELEGATE(vector_uart_rxc, void, ATTR_SIGNAL)

// The setup callback doesn't need the delegate since it is placed last in the table,
// it can have any size.

CALLBACK(callback_setup, void, callback_setup)
