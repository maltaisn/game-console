
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

#ifndef SYS_FLASH_H
#define SYS_FLASH_H

#include <sys/defs.h>
#include <stdint.h>
#include <stdbool.h>

#define FLASH_SIZE ((flash_t) 0x100000)  // 1 MB

/** Address in flash (20-bit). */
typedef uint24_t flash_t;

/**
 * Read a number of bytes from flash starting from an address.
 * The bytes are copied to the destination buffer.
 * If reading past the end of flash, the address will be wrapped around.
 */
void flash_read(flash_t address, uint16_t length, uint8_t dest[]);

/**
 * Enable deep power-down mode on flash device.
 */
void flash_sleep(void);

/**
 * Disable deep power-down mode on flash device.
 */
void flash_wakeup(void);

#endif //SYS_FLASH_H
