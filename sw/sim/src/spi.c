
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

#include <sys/spi.h>

#include <memory.h>

void spi_transceive(uint16_t length, uint8_t data[length]) {
    // not implemented: receive all 0x00
    memset(data, 0x00, length);
}

void spi_transmit(uint16_t length, const uint8_t data[length]) {
    // not implemented.
}

void spi_select_flash(void) {
    // no-op
}

void spi_select_eeprom(void) {
    // no-op
}

void spi_select_display(void) {
    // no-op
}

void spi_deselect_flash(void) {
    // no-op
}

void spi_deselect_eeprom(void) {
    // no-op
}

void spi_deselect_display(void) {
    // no-op
}

void spi_deselect_all(void) {
    // no-op
}
