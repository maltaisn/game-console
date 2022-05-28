
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

#include <core/flash.h>

#include <sys/flash.h>
#include <sys/defs.h>

void flash_read(flash_t address, uint16_t length, void* dest) {
    sys_flash_read_relative(address, length, dest);
}

#ifdef BOOTLOADER

#include <boot/defs.h>
#include <sys/spi.h>

#define INSTRUCTION_READ 0x03
#define INSTRUCTION_POWER_DOWN_ENABLE 0xb9
#define INSTRUCTION_POWER_DOWN_DISABLE 0xab

flash_t sys_flash_offset;

BOOTLOADER_NOINLINE
void sys_flash_read_absolute(flash_t address, uint16_t length, void* dest) {
    uint8_t header[4];
    header[0] = INSTRUCTION_READ;
    header[1] = address >> 16;
    header[2] = (address >> 8) & 0xff;
    header[3] = address & 0xff;
    sys_spi_select_flash();
    sys_spi_transceive(4, header);
    sys_spi_transceive(length, dest);
    sys_spi_deselect_flash();
}

void sys_flash_sleep(void) {
    sys_spi_select_flash();
    sys_spi_transmit_single(INSTRUCTION_POWER_DOWN_ENABLE);
    sys_spi_deselect_flash();
}

void sys_flash_wakeup(void) {
    sys_spi_select_flash();
    sys_spi_transmit_single(INSTRUCTION_POWER_DOWN_DISABLE);
    sys_spi_deselect_flash();
}

#endif //BOOTLOADER

ALWAYS_INLINE
void sys_flash_set_offset(flash_t address) {
    sys_flash_offset = address;
}

ALWAYS_INLINE
void sys_flash_read_relative(flash_t address, uint16_t length, void* dest) {
    sys_flash_read_absolute(address + sys_flash_offset, length, dest);
}
