
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
#define WRITE_BUFFER_SIZE 8192
#define ERASE_BYTE 0xff

static uint8_t eeprom[EXTERNAL_EEPROM_SIZE];

void eeprom_read(eeprom_t address, uint16_t length, uint8_t dest[static length]) {
    if (address + length > EXTERNAL_EEPROM_SIZE) {
        // wrap around the end
        uint16_t wrap_after = EXTERNAL_EEPROM_SIZE - address;
        memcpy(dest, &eeprom[address], wrap_after);
        memcpy(&dest[wrap_after], eeprom, length - wrap_after);
    } else {
        memcpy(dest, &eeprom[address], length);
    }
}

void eeprom_write(eeprom_t address, uint16_t length, const uint8_t src[static length]) {
    if (address + length > EXTERNAL_EEPROM_SIZE) {
        // wrap around the end
        uint16_t wrap_after = EXTERNAL_EEPROM_SIZE - address;
        memcpy(&eeprom[address], src, wrap_after);
        memcpy(eeprom, &src[wrap_after], length - wrap_after);
    } else {
        memcpy(&eeprom[address], src, length);
    }
}

const uint8_t* eeprom_at(eeprom_t address) {
    return &eeprom[address];
}

void eeprom_load(size_t length, const uint8_t data[static length]) {
    if (length > EXTERNAL_EEPROM_SIZE) {
        length = EXTERNAL_EEPROM_SIZE;
    }
    memcpy(eeprom, data, length);
}

void eeprom_load_file(FILE* file) {
    if (!file || ferror(file)) {
        return;
    }
    uint8_t* ptr = eeprom;
    while (true) {
        size_t n = READ_BUFFER_SIZE;
        if (ptr + n >= eeprom + EXTERNAL_EEPROM_SIZE) {
            n = (size_t) (EXTERNAL_EEPROM_SIZE - (ptrdiff_t) ptr + eeprom);
        }
        size_t read = fread(ptr, 1, n, file);
        ptr += read;
        if (read < READ_BUFFER_SIZE) {
            // end of file reached, short count.
            break;
        }
        if (ptr >= eeprom + EXTERNAL_EEPROM_SIZE) {
            // end of EEPROM reached.
            break;
        }
    }
    // erase the rest of memory
    memset(ptr, ERASE_BYTE, EXTERNAL_EEPROM_SIZE - (ptr - eeprom));
}

void eeprom_load_erased(void) {
    memset(eeprom, ERASE_BYTE, EXTERNAL_EEPROM_SIZE);
}

void eeprom_save(FILE* file) {
    if (!file || ferror(file)) {
        return;
    }

    size_t write_size = 0;
    for (size_t i = 0; i < EXTERNAL_EEPROM_SIZE; ++i) {
        if (eeprom[i] != ERASE_BYTE) {
            write_size = i + 1;
        }
    }

    uint8_t* ptr = eeprom;
    while (true) {
        size_t n = READ_BUFFER_SIZE;
        if (ptr + n >= eeprom + write_size) {
            n = (size_t) (write_size - (ptrdiff_t) ptr + eeprom);
        }
        size_t written = fwrite(ptr, 1, n, file);
        if (written < READ_BUFFER_SIZE) {
            // short count, unknown error occured.
            break;
        }
        ptr += written;
        if (ptr >= eeprom + EXTERNAL_EEPROM_SIZE) {
            // end of EEPROM reached.
            break;
        }
    }
}
