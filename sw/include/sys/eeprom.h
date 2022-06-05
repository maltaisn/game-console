
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

#ifndef SYS_EEPROM_H
#define SYS_EEPROM_H

#include <core/eeprom.h>
#include <stdint.h>

/*
 * EEPROM MEMORY LAYOUT
 *
 * Each app can reserve a fixed amount of space on the EEPROM. This allocation is stored in
 * the EEPROM index at the start of memory, along with some other information.
 * Allocation is done once when flashing the app on the device, the app cannot do it by itself,
 * nor can it reallocate to gain access to more space at runtime.
 * Moreover, all allocated locations in the EEPROM must be placed continuously and in the
 * same order as the index.
 *
 * [0..1]: EEPROM signature (0x6367)
 * [2]: ID of the currently loaded app.
 * [3..4]: CRC of the currently loaded app (of the whole app image, code & data)
 * [5..7]: --reserved--
 * [8..167]: index entries
 * [167..188]: --reserved--
 * [189]: if write in progress, length of write in bytes. Otherwise 0.
 * [190..191]: if write in progress, address of write.
 * [192..446]: buffer used to copy old data for atomic access.
 * [447]: --reserved--
 * [448..]: allocated app data
 *
 * Each index entry has the following format:
 * [0]: app ID (0 if entry not used)
 * [1..2]: data address
 * [3..4]: data size
 */

/**
 * EEPROM size in bytes. EEPROM addresses are stored on 16 bits,
 * meaning up to 64 kB of EEPROM is supported.
 */
#define SYS_EEPROM_SIZE ((eeprom_t) 0x1000)  // 4 KB

#define SYS_EEPROM_SIGNATURE 0x6367

#define SYS_EEPROM_APP_ID_ADDR 2
#define SYS_EEPROM_INDEX_ADDR 8
#define SYS_EEPROM_INDEX_ENTRY_SIZE 5
#define SYS_EEPROM_WRITE_SIZE_ADDR 189
#define SYS_EEPROM_WRITE_ADDR_ADDR 190
#define SYS_EEPROM_WRITE_BUF_ADDR 192
#define SYS_EEPROM_DATA_START_ADDR 448

extern eeprom_t sys_eeprom_offset;
extern eeprom_t sys_eeprom_size;

/**
 * Set the address and size of the allocated EEPROM section for the loaded app.
 */
void sys_eeprom_set_location(eeprom_t address, uint16_t size);

// see documentation in core/eeprom.h
void sys_eeprom_read_relative(eeprom_t address, uint8_t length, void* dest);

// see documentation in core/eeprom.h
void sys_eeprom_write_relative(eeprom_t address, uint8_t length, const void* src);

/**
 * Read a number of bytes from EEPROM starting from an address.
 * The bytes are copied to the destination buffer.
 * If reading past the end of EEPROM, the address will be wrapped around.
 */
void sys_eeprom_read_absolute(eeprom_t address, uint8_t length, void* dest);

/**
 * Write a number of bytes to EEPROM starting at an address.
 * The bytes are copied from the source buffer.
 * If writing past the end of EEPROM, the address will be wrapped around.
 */
void sys_eeprom_write_absolute(eeprom_t address, uint8_t length, const void* src);

#endif //SYS_EEPROM_H
