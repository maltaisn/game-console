
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

#ifndef SYS_DATA_H
#define SYS_DATA_H

#include <sys/defs.h>
#include <stdint.h>

#define DATA_EEPROM_MASK 0x100000
#define DATA_FLASH_MASK 0x200000

#define data_mcu(ptr) ((data_ptr_t) (intptr_t) (ptr))
#define data_flash(addr) ((data_ptr_t) (addr) | DATA_FLASH_MASK)
#define data_eeprom(addr) ((data_ptr_t) (addr) | DATA_EEPROM_MASK)

#ifdef SIMULATION
typedef uintptr_t data_ptr_t;
#else
typedef uint24_t data_ptr_t;
#endif

/**
 * Provides an unified interface for reading data from program memory,
 * RAM, internal EEPROM, external flash and external EEPROM.
 * Addresses in the interface are 24-bit and a mask indicates the data space.
 * Data is copied from the address to the destination buffer.
 */
void data_read(data_ptr_t address, uint16_t length, uint8_t dest[static length]);

#endif //SYS_DATA_H
