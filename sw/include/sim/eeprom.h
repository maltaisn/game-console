
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
 * Load EEPROM content as all erased bytes.
 * `sim_eeprom_free` should be called after memory becomes unused.
 */
void sim_eeprom_init(void);

/**
 * Free allocated EEPROM memory.
 */
void sim_eeprom_free(void);

/**
 * Load EEPROM content from the file.
 */
void sim_eeprom_load(const char* filename);

/**
 * Save EEPROM content to the file previously loaded.
 */
void sim_eeprom_save(void);

/**
 * Transceive SPI data by emulating EEPROM device.
 */
void sim_eeprom_spi_transceive(size_t length, uint8_t data[]);

/**
 * Reset SPI interface of emulated EEPROM device.
 */
void sim_eeprom_spi_reset(void);

#endif //SIM_EEPROM_H

#endif //SIMULATION
