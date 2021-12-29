
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

#include <stdio.h>

/**
 * Load EEPROM content from binary file.
 */
void eeprom_load(FILE* file);

/**
 * Load EEPROM content as all erased bytes.
 */
void eeprom_load_erased(void);

/**
 * Load EEPROM content from binary file.
 */
void eeprom_save(const char* filename);

#endif //SIM_EEPROM_H

#endif //SIMULATION