
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

#ifndef CORE_DEFS_H
#define CORE_DEFS_H

#include <stdint.h>

#if !defined(__AVR__) || defined(__CLION_IDE__)
#include <stdint.h>
typedef uint32_t uint24_t;
typedef int32_t int24_t;
#else
typedef __uint24 uint24_t;
typedef __int24 int24_t;
#endif

// Temporary data placed alongside display buffer, to save RAM.
// This data must not be used across or during display refresh.
#ifdef SIMULATION
#define SHARED_DISP_BUF
#else
#define SHARED_DISP_BUF __attribute__((section(".shared_disp_buf")))
#endif

#endif //CORE_DEFS_H
