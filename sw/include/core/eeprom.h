
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

#ifndef CORE_EEPROM_H
#define CORE_EEPROM_H

#include <stdint.h>

/** Address in EEPROM. */
typedef uint16_t eeprom_t;

/**
 * Read a number of bytes from EEPROM starting from an address relative to the start of
 * allocated space. The bytes are copied to the destination buffer.
 * When reading past the end of allocated space, the read data is undefined.
 */
void eeprom_read(eeprom_t address, uint8_t length, void* dest);

/**
 * Write a number of bytes to EEPROM starting at an address relative to the start of
 * allocated space. The bytes are copied from the source buffer.
 * If writing past the end of allocated space, nothing will be written.
 *
 * This operation is atomic. Old data is first copied to a buffer, then new data is copied
 * to the specified location. If power goes out before end has finished, the old data will be
 * restored.
 */
void eeprom_write(eeprom_t address, uint8_t length, const void* src);

#include <sim/eeprom.h>

#endif //CORE_EEPROM_H
