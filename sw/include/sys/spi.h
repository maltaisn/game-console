
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
 * This should never be called in an interrupt, in case another transfer is in progress already.
 */
void sys_spi_transceive(uint16_t length, uint8_t data[]);

/**
 * Transmit data on the SPI bus. Length must be at least 1.
 * The CS line for the selected peripheral should be driven low before and after.
 * This should never be called in an interrupt, in case another transfer is in progress already.
 */
void sys_spi_transmit(uint16_t length, const uint8_t data[]);

/**
 * Transmit a single byte on the SPI bus.
 * The CS line for the selected peripheral should be driven low before and after.
 * This should never be called in an interrupt, in case another transfer is in progress already.
 */
void sys_spi_transmit_single(uint8_t byte);

// Peripheral selection
void sys_spi_select_flash(void);
void sys_spi_select_eeprom(void);
void sys_spi_select_display(void);

// Peripheral deselection
void sys_spi_deselect_flash(void);
void sys_spi_deselect_eeprom(void);
void sys_spi_deselect_display(void);
void sys_spi_deselect_all(void);

#endif //SYS_SPI_H
