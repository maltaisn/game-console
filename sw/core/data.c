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

#include <core/data.h>

#include <sys/data.h>

#include <boot/defs.h>

BOOTLOADER_NOINLINE
void data_read(data_ptr_t address, uint16_t length, uint8_t dest[static length]) {
    sys_data_read(address, length, dest);
}

#endif  //BOOTLOADER
