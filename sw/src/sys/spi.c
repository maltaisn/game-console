
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

#include <avr/io.h>

void spi_transceive(uint16_t length, uint8_t data[length]) {
    // SPI is triple-buffered is the transmit direction and double buffered in the receive direction.
    // Here we only use one buffer level in the transmit direction.
    // 1. Write the first byte to be transmitted.
    // 2. Wait until data register is empty (DREIF=1) and write the second byte to be transmitted.
    // 3. Wait until first byte has been received (RXCIF=1) then read it.
    // 4. Repeat from step 2 until the (n-1) received byte.
    // 5. Wait until last byte has been received (RXCIF=1).
    SPI0.DATA = data[0];
    uint16_t pos = 0;
    while (--length) {
        while (!(SPI0.INTFLAGS & SPI_DREIF_bm));
        SPI0.DATA = data[pos + 1];
        while (!(SPI0.INTFLAGS & SPI_RXCIF_bm));
        data[pos++] = SPI0.DATA;
    }
    while (!(SPI0.INTFLAGS & SPI_RXCIF_bm));
    data[pos] = SPI0.DATA;
}

void spi_transmit(uint16_t length, const uint8_t data[length]) {
    // same as transceive but not receiving.
    SPI0.DATA = *data++;
    while (--length) {
        while (!(SPI0.INTFLAGS & SPI_DREIF_bm));
        SPI0.DATA = *data++;
        while (!(SPI0.INTFLAGS & SPI_RXCIF_bm));
        SPI0.DATA;
    }
    while (!(SPI0.INTFLAGS & SPI_RXCIF_bm));
    SPI0.DATA;
}
