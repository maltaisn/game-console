
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

#include <stdint.h>

#define EXTERNAL_EEPROM_SIZE ((eeprom_t) 0x1000)  // 4 KB

/** Address in EEPROM (12-bit). */
typedef uint16_t eeprom_t;

/**
 * Read a number of bytes from EEPROM starting from an address.
 * The bytes are copied to the destination buffer.
 * If reading past the end of EEPROM, the address will be wrapped around.
 */
void eeprom_read(eeprom_t address, uint16_t length, uint8_t dest[]);

/**
 * Write a number of bytes to EEPROM starting at an address.
 * The bytes are copied from the source buffer.
 * If writing past the end of EEPROM, the address will be wrapped around.
 */
void eeprom_write(eeprom_t address, uint16_t length, const uint8_t src[]);

#endif //SYS_EEPROM_H
