
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

#include <sim/flash.h>
#include <sys/flash.h>

#include <stddef.h>
#include <memory.h>
#include <stdbool.h>

#define READ_BUFFER_SIZE 8192
#define ERASE_BYTE 0xff

static uint8_t flash[FLASH_SIZE];

void flash_read(flash_t address, uint16_t length, uint8_t dest[length]) {
    if (address + length > FLASH_SIZE) {
        // wrap around the end
        uint16_t wrap_after = FLASH_SIZE - address;
        memcpy(dest, &flash[address], wrap_after);
        memcpy(&dest[wrap_after], flash, length - wrap_after);
    } else {
        memcpy(dest, &flash[address], length);
    }
}

const uint8_t* flash_at(flash_t address) {
    return &flash[address];
}

void flash_load(FILE* file) {
    uint8_t* ptr = flash;
    while (true) {
        size_t n = READ_BUFFER_SIZE;
        if (ptr + n >= flash + FLASH_SIZE) {
            n = (size_t) (FLASH_SIZE - (ptrdiff_t) ptr + flash);
        }
        size_t read = fread(ptr, 1, n, file);
        if (read < READ_BUFFER_SIZE) {
            // end of file reached, short count.
            break;
        }
        ptr += read;
        if (ptr >= flash + FLASH_SIZE) {
            // end of flash reached.
            break;
        }
    }
}

void flash_load_erased(void) {
    memset(flash, ERASE_BYTE, FLASH_SIZE);
}
