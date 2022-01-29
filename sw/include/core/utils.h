
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

#ifndef CORE_UTILS_H
#define CORE_UTILS_H

#include <stdint.h>

/**
 * Format 0-255 number to char buffer.
 * Returns a pointer within the buffer to start of the formatted string.
 */
const char* uint8_to_str(char buf[4], uint8_t n);

#endif //CORE_UTILS_H
