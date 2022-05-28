
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

#include <core/flash.h>
#include <stddef.h>

/*
 * When in simulator, the flash content is typically loaded from a file named 'assets.dat'
 * and contains only the data required by the app.
 * The size of the simulated flash is the same as the physical flash, but the data is starts
 * at address 0 instead of a specified offset. Thus, absolute and relative reads are the same.
 */

/**
 * Load flash content as all erased bytes.
 * `sim_flash_free` should be called after memory becomes unused.
 */
void sim_flash_init(void);

/**
 * Free allocated flash memory.
 */
void sim_flash_free(void);

/**
 * Load flash content from a file, starting an address 0.
 */
void sim_flash_load(const char* filename);

/**
 * Save flash content to the file previously loaded.
 */
void sim_flash_save(void);

/**
 * Transceive SPI data by emulating flash device.
 */
void sim_flash_spi_transceive(size_t length, uint8_t data[]);

/**
 * Reset SPI interface of emulated flash device.
 */
void sim_flash_spi_reset(void);

#endif //SIM_FLASH_H

#endif //SIMULATION