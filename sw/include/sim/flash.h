
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

#ifdef SIMULATION

#ifndef SIM_FLASH_H
#define SIM_FLASH_H

#include <sys/flash.h>

#include <stdio.h>

/**
 * Returns a pointer to flash data at an address.
 * Note that this isn't 100% equivalent to `flash_read` since it won't wrap around the end.
 */
const uint8_t* flash_at(flash_t address);

/**
 * Load flash content from an array.
 */
void flash_load(flash_t address, size_t length, const uint8_t data[static length]);

/**
 * Load flash content from a file, starting at an address.
 */
void flash_load_file(flash_t address, FILE* file);

/**
 * Load flash content as all erased bytes.
 */
void flash_load_erased(void);

#endif //SIM_FLASH_H

#endif //SIMULATION