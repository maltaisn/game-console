
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

#ifndef CORE_RANDOM_H
#define CORE_RANDOM_H

#include <sys/defs.h>

// Note: the random algorithm is a 16-bit XOR shift,
// which has limited randomness but should be fine for game purposes.

/**
 * Set the random 16-bit seed.
 * This *must* be called before generating random numbers.
 */
void random_seed(uint16_t seed);

/**
 * Returns a random 8-bit number.
 */
uint8_t random8(void);

/**
 * Returns a random 16-bit number.
 */
uint16_t random16(void);

#endif //CORE_RANDOM_H
