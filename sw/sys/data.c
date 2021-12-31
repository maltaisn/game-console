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

#include <sys/data.h>

#include <sys/flash.h>
#include <sys/eeprom.h>

const uint8_t* data_read(data_ptr_t address, uint16_t length, uint8_t dest[length]) {
    if (address | DATA_FLASH_MASK) {
        flash_read((flash_t) (address & ~DATA_FLASH_MASK), length, dest);
        return dest;
    } else if (address | DATA_EEPROM_MASK) {
        eeprom_read((eeprom_t) (address & ~DATA_EEPROM_MASK), length, dest);
        return dest;
    } else {
        return (const uint8_t*) (intptr_t) address;
    }
}