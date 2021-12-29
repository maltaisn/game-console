
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

#include <sim/eeprom.h>
#include <sys/eeprom.h>

#include <stddef.h>
#include <memory.h>
#include <stdbool.h>

#define READ_BUFFER_SIZE 8192
#define ERASE_BYTE 0xff

static uint8_t eeprom[EEPROM_SIZE];

void eeprom_read(eeprom_t address, uint16_t length, uint8_t dest[length]) {
    if (address + length > EEPROM_SIZE) {
        // wrap around the end
        uint16_t wrap_after = EEPROM_SIZE - address;
        memcpy(dest, &eeprom[address], wrap_after);
        memcpy(&dest[wrap_after], eeprom, length - wrap_after);
    } else {
        memcpy(dest, &eeprom[address], length);
    }
}

void eeprom_write(eeprom_t address, uint16_t length, const uint8_t src[length]) {
    if (address + length > EEPROM_SIZE) {
        // wrap around the end
        uint16_t wrap_after = EEPROM_SIZE - address;
        memcpy(&eeprom[address], src, wrap_after);
        memcpy(eeprom, &src[wrap_after], length - wrap_after);
    } else {
        memcpy(&eeprom[address], src, length);
    }
}

void eeprom_load(FILE* file) {
    uint8_t* ptr = eeprom;
    while (true) {
        size_t n = READ_BUFFER_SIZE;
        if (ptr + n >= eeprom + EEPROM_SIZE) {
            n = (size_t) (EEPROM_SIZE - (ptrdiff_t) ptr + eeprom);
        }
        size_t read = fread(ptr, 1, n, file);
        if (read < READ_BUFFER_SIZE) {
            // end of file reached, short count.
            break;
        }
        ptr += read;
        if (ptr >= eeprom + EEPROM_SIZE) {
            // end of flash reached.
            break;
        }
    }
}

void eeprom_load_erased(void) {
    memset(eeprom, ERASE_BYTE, EEPROM_SIZE);
}
