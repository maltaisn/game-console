
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

#include <core/eeprom.h>
#include <sys/eeprom.h>
#include <core/trace.h>

#include <memory.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define READ_BUFFER_SIZE 8192
#define WRITE_BUFFER_SIZE 8192
#define ERASE_BYTE 0xff

#ifdef SIM_EEPROM_ABSOLUTE
#define EEPROM_SIZE SYS_EEPROM_SIZE
#else
#define EEPROM_SIZE EEPROM_RESERVED_SPACE
#endif

static uint8_t eeprom[EEPROM_SIZE];

void sys_eeprom_read_absolute(eeprom_t address, uint8_t length, void* dest) {
    if (address >= EEPROM_SIZE) {
        return;
    } else if (address + length >= EEPROM_SIZE) {
        length = EEPROM_SIZE - address;
    }
    memcpy(dest, &eeprom[address], length);
}

void sys_eeprom_write_absolute(eeprom_t address, uint8_t length, const void* src) {
    if (address >= EEPROM_SIZE) {
        return;
    } else if (address + length >= EEPROM_SIZE) {
        length = EEPROM_SIZE - address;
    }
    memcpy(&eeprom[address], src, length);
}

void sys_eeprom_check_write(void) {
    // nothing to do, write is considered always atomic in simulator.
}

void sys_eeprom_set_location(eeprom_t address, uint16_t size) {
    // nothing to do, read is always absolute in simulator.
}

void sys_eeprom_read_relative(eeprom_t address, uint8_t length, void* dest) {
    if (address >= EEPROM_SIZE) {
        return;
    } else if (address + length >= EEPROM_SIZE) {
        length = EEPROM_SIZE - address;
    }
    memcpy(dest, &eeprom[address], length);
}

void sys_eeprom_write_relative(eeprom_t address, uint8_t length, const void* src) {
    if (address >= EEPROM_SIZE) {
        return;
    } else if (address + length >= EEPROM_SIZE) {
        length = EEPROM_SIZE - address;
    }
    memcpy(&eeprom[address], src, length);
}

void sim_eeprom_load(eeprom_t address, size_t length, const uint8_t data[static length]) {
    if (length + address > EEPROM_SIZE) {
        length = EEPROM_SIZE - address;
    } else if (address > EEPROM_SIZE) {
        return;
    }
    memcpy(eeprom, data, length);
}

void sim_eeprom_load_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file || ferror(file)) {
        trace("could not read EERPOM file '%s'", filename);
        return;
    }
    uint8_t* ptr = eeprom;
    while (true) {
        size_t n = READ_BUFFER_SIZE;
        if (ptr + n >= eeprom + EEPROM_SIZE) {
            n = (size_t) (EEPROM_SIZE - (ptrdiff_t) ptr + eeprom);
        }
        size_t read = fread(ptr, 1, n, file);
        ptr += read;
        if (read < READ_BUFFER_SIZE) {
            // end of file reached, short count.
            break;
        }
        if (ptr >= eeprom + EEPROM_SIZE) {
            // end of EEPROM reached.
            break;
        }
    }
    // erase the rest of memory
    memset(ptr, ERASE_BYTE, EEPROM_SIZE - (ptr - eeprom));

    fclose(file);
}

void sim_eeprom_load_erased(void) {
#if defined(SIM_EEPROM_ABSOLUTE) || EEPROM_RESERVED_SPACE > 0
    memset(eeprom, ERASE_BYTE, EEPROM_SIZE);
#endif
}

void sim_eeprom_save(const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file || ferror(file)) {
        return;
    }

    size_t write_size = 0;
    for (size_t i = 0; i < EEPROM_SIZE; ++i) {
        if (eeprom[i] != ERASE_BYTE) {
            write_size = i + 1;
        }
    }

    uint8_t* ptr = eeprom;
    while (true) {
        size_t n = WRITE_BUFFER_SIZE;
        if (ptr + n >= eeprom + write_size) {
            n = (size_t) (write_size - (ptrdiff_t) ptr + eeprom);
        }
        size_t written = fwrite(ptr, 1, n, file);
        if (written < WRITE_BUFFER_SIZE) {
            // short count, unknown error occured.
            break;
        }
        ptr += written;
        if (ptr >= eeprom + EEPROM_SIZE) {
            // end of EEPROM reached.
            break;
        }
    }

    fclose(file);
}
