
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

#ifndef SYS_FLASH_H
#define SYS_FLASH_H

#include <core/flash.h>

#include <stdint.h>
#include <stdbool.h>

/*
 * FLASH MEMORY LAYOUT
 *
 * App code and app data are both located in the external flash memory.
 * Both are written as a single block which forms the app image and is read by the bootloader to
 * load the app. An index is kept at the start of flash with the location of each app
 * and data associated to them.
 * The index has a fixed size of 32 entries. Unused entries have an app ID of 0.
 *
 * [0..1]: flash signature (0x6367)
 * [2..31]: --reserved--
 * [32..2079]: index entries
 * [2080..]: app data
 *
 * Each index entry has the following format:
 * [0]: app ID
 * [1..2]: image CRC (code & data)
 * [3..4]: code CRC
 * [5..6]: version
 * [7..8]: bootloader version that app was compiled against
 * [9..10]: code size in bytes
 * [11]: display page height
 * [12..13]: start address of EEPROM data space (0 if none)
 * [14..15]: size of EEPROM data space (0 if none)
 * [16..18]: start address of image in flash
 * [19..26]: --reserved--
 * [27..29]: total app size in bytes
 * [30..31]: build date ([0..4]=day, [5..8]=month, [9..15]=year since 2020)
 * [32..47]: name, ASCII encoding
 * [48..63]: author, ASCII encoding
 *
 * The total index size is 64 * 32 = 2048 bytes.
 */

/**
 * Flash size in bytes. Flash addresses are stored on 24 bits, but 1-bit is reserved to indicate
 * the location in unified data space. As such, a maximum of 8 MB of flash is supported.
 */
#define SYS_FLASH_SIZE ((flash_t) 0x100000)  // 1 MB

#define SYS_FLASH_SIGNATURE 0x6367

#define SYS_FLASH_INDEX_ADDR 32
#define SYS_FLASH_INDEX_ENTRY_SIZE 64
#define SYS_FLASH_DATA_START_ADDR 2080

extern flash_t sys_flash_offset;

/**
 * Set the offset to use for relative reads.
 * This allows the app code to be location-independant.
 */
void sys_flash_set_offset(flash_t address);

/**
 * Read a number of bytes from flash starting from an address.
 * The address is absolute in the flash memory space.
 * The bytes are copied to the destination buffer.
 * If reading past the end of flash, the address will be wrapped around.
 */
void sys_flash_read_absolute(flash_t address, uint16_t length, void* dest);

/**
 * Read a number of bytes from flash starting from an address.
 * The address is relative to the start of the app data space.
 * The bytes are copied to the destination buffer.
 * If reading past the end of flash, the address will be wrapped around.
 */
void sys_flash_read_relative(flash_t address, uint16_t length, void* dest);

#endif //SYS_FLASH_H
