
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

#include <core/trace.h>

#include <stddef.h>
#include <memory.h>
#include <stdbool.h>
#include <stdlib.h>

#define READ_BUFFER_SIZE 8192
#define ERASE_BYTE 0xff

static uint8_t flash[SYS_FLASH_SIZE];
static bool powered_down;

void sys_flash_read_absolute(flash_t address, uint16_t length, void* dest) {
    sys_flash_read_relative(address, length, dest);
}

void sys_flash_sleep(void) {
    powered_down = true;
}

void sys_flash_wakeup(void) {
    powered_down = false;
}

void sys_flash_set_offset(flash_t address) {
    // nothing to do, flash is always absolute in simulation.
}

void sys_flash_read_relative(flash_t address, uint16_t length, void* dest) {
    if (powered_down) {
        trace("flash is in power down mode, cannot read");
        return;
    }
    if (address >= SYS_FLASH_SIZE) {
        return;
    } else if (address + length > SYS_FLASH_SIZE) {
        length = SYS_FLASH_SIZE - address;
    }
    memcpy(dest, &flash[address], length);
}

void sim_flash_load(flash_t address, size_t length, const uint8_t data[static length]) {
    if (length + address > SYS_FLASH_SIZE) {
        length = SYS_FLASH_SIZE - address;
    } else if (address > SYS_FLASH_SIZE) {
        return;
    }
    memcpy(flash + address, data, length);
}

void sim_flash_load_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file || ferror(file)) {
        trace("could not read flash file '%s'", filename);
        exit(1);
    }
    uint8_t* ptr = flash;
    while (true) {
        size_t n = READ_BUFFER_SIZE;
        if (ptr + n >= flash + SYS_FLASH_SIZE) {
            n = (size_t) (SYS_FLASH_SIZE - (ptrdiff_t) ptr + flash);
        }
        size_t read = fread(ptr, 1, n, file);
        ptr += read;
        if (read < READ_BUFFER_SIZE) {
            // end of file reached, short count.
            break;
        }
        if (ptr >= flash + SYS_FLASH_SIZE) {
            // end of flash reached.
            break;
        }
    }
    // erase the rest of memory
    memset(ptr, ERASE_BYTE, SYS_FLASH_SIZE - (ptr - flash));

    fclose(file);
}

void sim_flash_load_erased(void) {
    memset(flash, ERASE_BYTE, SYS_FLASH_SIZE);
}
