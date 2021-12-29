
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

#ifndef SYS_SPI_H
#define SYS_SPI_H

#include <stdint.h>

/**
 * Transmit and receive data on the SPI bus. Length must be at least 1.
 * The CS line for the selected peripheral should be driven low before and after.
 */
void spi_transceive(uint16_t length, uint8_t data[length]);

/**
 * Transmit data on the SPI bus. Length must be at least 1.
 * The CS line for the selected peripheral should be driven low before and after.
 */
void spi_transmit(uint16_t length, const uint8_t data[length]);

// Peripheral selection
void spi_select_flash(void);
void spi_select_eeprom(void);
void spi_select_display(void);

// Peripheral deselection
void spi_deselect_flash(void);
void spi_deselect_eeprom(void);
void spi_deselect_display(void);
void spi_deselect_all(void);

#endif //SYS_SPI_H
