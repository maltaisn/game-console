
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

#ifndef DEFS_H
#define DEFS_H

#ifdef __AVR__
typedef __uint24 uint24_t;
#else
#include <stdint.h>
typedef uint32_t uint24_t;
#endif

#ifdef __AVR__
typedef __int24 int24_t;
#else
#include <stdint.h>
typedef int32_t int24_t;
#endif

#endif //DEFS_H