
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

#include <sys/eeprom.h>

#include <core/defs.h>

void eeprom_read(eeprom_t address, uint8_t length, void* dest) {
    sys_eeprom_read_relative(address, length, dest);
}

void eeprom_write(eeprom_t address, uint8_t length, const void* src) {
    sys_eeprom_write_relative(address, length, src);
}

#ifdef BOOTLOADER

#include <boot/defs.h>

#include <sys/spi.h>
#include <stdlib.h>

#define INSTRUCTION_WREN 0x06
#define INSTRUCTION_RDSR 0x05
#define INSTRUCTION_READ 0x03
#define INSTRUCTION_WRITE 0x02

#define STATUS_BUSY_MASK 0x01

#define PAGE_SIZE 32

#endif //BOOTLOADER

// this buffer is located alongside the display buffer, but may not be at the same location
// in the bootloader and in the app, so prefix it with '_' to not include it in the boot symbols.
static SHARED_DISP_BUF uint8_t _eeprom_buf[255];

#ifdef BOOTLOADER

BOOTLOADER_KEEP eeprom_t sys_eeprom_offset;
BOOTLOADER_KEEP eeprom_t sys_eeprom_size;

/**
 * Wait until EEPROM status register indicates ready status.
 */
static void _eeprom_wait_ready(void) {
    uint8_t rdsr_cmd[2];
    do {
        sys_spi_select_eeprom();
        rdsr_cmd[0] = INSTRUCTION_RDSR;
        sys_spi_transceive(2, rdsr_cmd);
        sys_spi_deselect_eeprom();
    } while (rdsr_cmd[1] & STATUS_BUSY_MASK);
}

BOOTLOADER_NOINLINE
void sys_eeprom_read_absolute(eeprom_t address, uint8_t length, void* dest) {
    uint8_t read_cmd[3];
    read_cmd[0] = INSTRUCTION_READ;
    read_cmd[1] = address >> 8;
    read_cmd[2] = address & 0xff;
    sys_spi_select_eeprom();
    sys_spi_transmit(3, read_cmd);
    sys_spi_transceive(length, dest);
    sys_spi_deselect_eeprom();
}

BOOTLOADER_NOINLINE
void sys_eeprom_write_absolute(eeprom_t address, uint8_t length, const void* src) {
    const uint8_t wren_cmd = INSTRUCTION_WREN;
    uint8_t write_cmd[3];
    write_cmd[0] = INSTRUCTION_WRITE;

    while (length) {
        _eeprom_wait_ready();

        sys_spi_select_eeprom();
        sys_spi_transmit(1, &wren_cmd);
        sys_spi_deselect_eeprom();

        write_cmd[1] = address >> 8;
        write_cmd[2] = address & 0xff;
        sys_spi_select_eeprom();
        sys_spi_transmit(3, write_cmd);
        uint8_t page_length = PAGE_SIZE - address % PAGE_SIZE;
        if (page_length > length) {
            page_length = length;
        }
        sys_spi_transmit(page_length, src);
        sys_spi_deselect_eeprom();

        address += page_length;
        length -= page_length;
        src += page_length;
    }
    _eeprom_wait_ready();
}

void sys_eeprom_check_write(void) {
    uint8_t write_size;
    sys_eeprom_read_absolute(SYS_EEPROM_WRITE_SIZE_ADDR, 1, &write_size);
    if (write_size != 0) {
        // data was no fully copied; restore the old data from the buffer.
        eeprom_t addr_abs;
        sys_eeprom_read_absolute(SYS_EEPROM_WRITE_ADDR_ADDR, 2, &addr_abs);
        sys_eeprom_read_absolute(SYS_EEPROM_WRITE_BUF_ADDR, write_size, _eeprom_buf);
        sys_eeprom_write_absolute(addr_abs, write_size, _eeprom_buf);
        uint8_t zero = 0;
        sys_eeprom_write_absolute(SYS_EEPROM_WRITE_SIZE_ADDR, 1, &zero);
    }
}

#endif  //BOOTLOADER

void sys_eeprom_set_location(eeprom_t address, uint16_t size) {
    sys_eeprom_offset = address;
    sys_eeprom_size = size;
}

void sys_eeprom_read_relative(eeprom_t address, uint8_t length, void* dest) {
    sys_eeprom_read_absolute(address + sys_eeprom_offset, length, dest);
}

void sys_eeprom_write_relative(eeprom_t address, uint8_t length, const void* src) {
    if (address + length > sys_eeprom_size) {
        // write exceeds allocated space, truncate it.
        if (__builtin_sub_overflow(sys_eeprom_size, address, &length)) {
            // fully past allocated space, no write.
            return;
        }
    }
    if (length == 0) {
        return;
    }

    eeprom_t addr_abs = address + sys_eeprom_offset;
#if !defined(SIMULATION) || defined(SIM_MEMORY_ABSOLUTE)
    // copy old data to buffer
    sys_eeprom_read_absolute(addr_abs, length, _eeprom_buf);
    sys_eeprom_write_absolute(SYS_EEPROM_WRITE_BUF_ADDR, length, _eeprom_buf);

    // copy new data
    sys_eeprom_write_absolute(SYS_EEPROM_WRITE_ADDR_ADDR, 2, &addr_abs);
    sys_eeprom_write_absolute(SYS_EEPROM_WRITE_SIZE_ADDR, 1, &length);
    sys_eeprom_write_absolute(addr_abs, length, src);
    uint8_t zero = 0;
    sys_eeprom_write_absolute(SYS_EEPROM_WRITE_SIZE_ADDR, 1, &zero);
#else
    // copy new data
    sys_eeprom_write_absolute(addr_abs, length, src);
#endif
}
