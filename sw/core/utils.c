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

#ifdef BOOTLOADER

#include <core/utils.h>
#include <boot/defs.h>

BOOTLOADER_NOINLINE
char* uint8_to_str(char buf[static 4], uint8_t n) {
    char* ptr = &buf[3];
    *ptr = '\0';
    do {
        *(--ptr) = (char) (n % 10 + '0');
        n /= 10;
    } while (n);
    return ptr;
}

#endif //BOOTLOADER