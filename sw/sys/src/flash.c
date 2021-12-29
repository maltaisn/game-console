
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

#include <sys/flash.h>
#include <sys/spi.h>

#include <avr/io.h>

#define INSTRUCTION_READ 0x03

void flash_read(flash_t address, uint16_t length, uint8_t dest[length]) {
    uint8_t header[4];
    header[0] = INSTRUCTION_READ;
    header[1] = address >> 16;
    header[2] = (address >> 8) & 0xff;
    header[3] = address & 0xff;
    spi_select_flash();
    spi_transceive(4, header);
    spi_transceive(length, dest);
    spi_deselect_flash();
}
