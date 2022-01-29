
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

#ifndef SIM_EEPROM_H
#define SIM_EEPROM_H

#include <sys/eeprom.h>

#include <stdio.h>
#include <stdint.h>

/**
 * Returns a pointer to EEPROM data at an address.
 * Note that this isn't 100% equivalent to `eeprom_read` since it won't wrap around the end.
 */
const uint8_t* eeprom_at(eeprom_t address);

/**
 * Load EEPROM content from an array.
 */
void eeprom_load(size_t length, const uint8_t data[]);

/**
 * Load EEPROM content from a file.
 */
void eeprom_load_file(FILE* file);

/**
 * Load EEPROM content as all erased bytes.
 */
void eeprom_load_erased(void);

/**
 * Save EEPROM content to a file.
 */
void eeprom_save(FILE* file);

#endif //SIM_EEPROM_H

#endif //SIMULATION