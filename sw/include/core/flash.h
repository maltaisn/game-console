
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

#ifndef CORE_FLASH_H
#define CORE_FLASH_H

#include <core/defs.h>
#include <stdint.h>

/** Address in flash. */
typedef uint24_t flash_t;

/**
 * Read a number of bytes from flash starting from an address,
 * relative to the start of app data address.
 * The bytes are copied to the destination buffer.
 * If reading past the end of flash, the address will be wrapped around.
 */
void flash_read(flash_t address, uint16_t length, void* dest);

#include <sim/flash.h>

#endif //CORE_FLASH_H
