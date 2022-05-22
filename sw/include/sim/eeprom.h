
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

#include <core/eeprom.h>
#include <stddef.h>

/*
 * When in simulator, the EEPROM content is typically stored to a file named 'eeprom.dat'.
 * The size of the simulated EEPROM is restricted to the space reserved by the app in its
 * configuration file. Thus, relative and absolute operations are always the same.
 */

/**
 * Load EEPROM content from an array.
 */
void sim_eeprom_load(eeprom_t address, size_t length, const uint8_t data[]);

/**
 * Load EEPROM content from a file.
 */
void sim_eeprom_load_file(const char* filename);

/**
 * Load EEPROM content as all erased bytes.
 */
void sim_eeprom_load_erased(void);

/**
 * Save EEPROM content to a file.
 */
void sim_eeprom_save(const char* filename);

#endif //SIM_EEPROM_H

#endif //SIMULATION