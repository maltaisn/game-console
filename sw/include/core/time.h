
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

#ifndef CORE_TIME_H
#define CORE_TIME_H

#include <core/defs.h>

#include <stdint.h>
#include <math.h>

#define SYSTICK_FREQUENCY 256

/**
 * Convert a number of milliseconds to a number of system ticks.
 * Note: this will return 0 if less than a system tick!
 */
#define millis_to_ticks(n) ((systime_t) lround((n) / 1000.0 * SYSTICK_FREQUENCY))

/**
 * Type used to store system time.
 * The 16-bit counter overflows after 256 seconds (4 min 16 sec).
 * As such, the system time should not be used as an absolute value because of frequent overflows,
 * but rather as a difference of a previous system time, in which case overflow has no impact
 * (e.g. 0xffff - 0x0001 correctly gives a result of 2 ticks. However, care must be taken to never
 * measure time difference greater than the system time maximum which would give a wrong result
 * (the system time counter must not overflow more than once since last time value was saved).
 */
typedef uint16_t systime_t;

/**
 * Returns the system time value.
 * The system time is incremented every 1/256th second.
 * Must not be called within an interrupt.
 */
systime_t time_get();

#include <sim/time.h>

#endif //CORE_TIME_H
