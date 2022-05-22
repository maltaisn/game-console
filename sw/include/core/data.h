
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

#ifndef CORE_DATA_H
#define CORE_DATA_H

#include <core/defs.h>
#include <stdint.h>

#define DATA_FLASH_MASK 0x800000

#define data_mcu(ptr) ((data_ptr_t) (uintptr_t) (ptr))
#define data_flash(addr) ((data_ptr_t) (addr) | DATA_FLASH_MASK)

#ifdef SIMULATION
typedef uintptr_t data_ptr_t;
#else
typedef uint24_t data_ptr_t;
#endif

/**
 * Provides an unified interface for reading data from program memory,
 * RAM, internal EEPROM, external flash (but not external EEPROM).
 * Addresses in the interface are 24-bit and a mask indicates the data space.
 * Data is copied from the address to the destination buffer.
 * Note that flash reads using this interface are always relative.
 */
void data_read(data_ptr_t address, uint16_t length, uint8_t dest[]);

#endif //CORE_DATA_H
