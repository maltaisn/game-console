
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

#include <sys/eeprom.h>
#include <sys/spi.h>

#define INSTRUCTION_WREN 0x06
#define INSTRUCTION_RDSR 0x05
#define INSTRUCTION_READ 0x03
#define INSTRUCTION_WRITE 0x02

#define STATUS_BUSY_MASK 0x01

#define PAGE_SIZE 32

/**
 * Wait until EEPROM status register indicates ready status.
 */
static void eeprom_wait_ready(void) {
    uint8_t rdsr_cmd[2];
    do {
        spi_select_eeprom();
        rdsr_cmd[0] = INSTRUCTION_RDSR;
        spi_transceive(2, rdsr_cmd);
        spi_deselect_eeprom();
    } while (rdsr_cmd[1] & STATUS_BUSY_MASK);
}

void eeprom_read(eeprom_t address, uint16_t length, uint8_t dest[length]) {
    uint8_t read_cmd[3];
    read_cmd[0] = INSTRUCTION_READ;
    read_cmd[1] = address >> 8;
    read_cmd[2] = address & 0xff;
    spi_select_eeprom();
    spi_transmit(3, read_cmd);
    spi_transceive(length, dest);
    spi_deselect_eeprom();
}

void eeprom_write(eeprom_t address, uint16_t length, const uint8_t src[length]) {
    const uint8_t wren_cmd = INSTRUCTION_WREN;
    uint8_t write_cmd[3];
    write_cmd[0] = INSTRUCTION_WRITE;

    while (length) {
        eeprom_wait_ready();

        spi_select_eeprom();
        spi_transmit(1, &wren_cmd);
        spi_deselect_eeprom();

        write_cmd[1] = address >> 8;
        write_cmd[2] = address & 0xff;
        spi_select_eeprom();
        spi_transmit(3, write_cmd);
        uint8_t page_length = PAGE_SIZE - address % PAGE_SIZE;
        if (page_length > length) {
            page_length = length;
        }
        spi_transmit(page_length, src);
        spi_deselect_eeprom();

        address += page_length;
        length -= page_length;
        src += page_length;
    }
    eeprom_wait_ready();
}
