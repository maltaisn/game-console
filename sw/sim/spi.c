
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
#include <sys/flash.h>
#include <sys/eeprom.h>
#include <sys/display.h>

#include <core/trace.h>

#include <memory.h>

typedef enum {
    DEVICE_NONE,
    DEVICE_FLASH,
    DEVICE_EEPROM,
    DEVICE_DISPLAY,
} spi_device_t;

spi_device_t selected_device;

void sys_spi_transceive(uint16_t length, uint8_t data[static length]) {
    if (length == 0) {
        return;
    }
    switch (selected_device) {
        case DEVICE_FLASH:
            sim_flash_spi_transceive(length, data);
            break;
        case DEVICE_EEPROM:
            sim_eeprom_spi_transceive(length, data);
            break;
        case DEVICE_DISPLAY:
            sim_display_spi_transceive(length, data);
            break;
        default:
            // no device selected
            break;
    }
}

void sys_spi_transmit(uint16_t length, const uint8_t data[static length]) {
    // copy data locally to use be able to transceive normally.
    uint8_t local_data[length];
    memcpy(local_data, data, length);
    sys_spi_transceive(length, local_data);
}

void sys_spi_transmit_single(uint8_t byte) {
    sys_spi_transmit(1, &byte);
}

static bool select_spi_device(spi_device_t device) {
    if (selected_device == device) {
        // Don't reselect the device as selecting has side effects.
        return false;
    }
    if (selected_device != DEVICE_NONE) {
        trace("cannot select SPI device, another device already selected");
        return false;
    }
    selected_device = device;
    return true;
}

void sys_spi_select_flash(void) {
    if (select_spi_device(DEVICE_FLASH)) {
        sim_flash_spi_reset();
    }
}

void sys_spi_select_eeprom(void) {
    if (select_spi_device(DEVICE_EEPROM)) {
        sim_eeprom_spi_reset();
    }
}

void sys_spi_select_display(void) {
    if (select_spi_device(DEVICE_DISPLAY)) {
        sim_display_spi_reset();
    }
}

void sys_spi_deselect_flash(void) {
    if (selected_device == DEVICE_FLASH) {
        selected_device = DEVICE_NONE;
    }
}

void sys_spi_deselect_eeprom(void) {
    if (selected_device == DEVICE_EEPROM) {
        selected_device = DEVICE_NONE;
    }
}

void sys_spi_deselect_display(void) {
    if (selected_device == DEVICE_DISPLAY) {
        selected_device = DEVICE_NONE;
    }
}

void sys_spi_deselect_all(void) {
    sys_spi_deselect_flash();
    sys_spi_deselect_eeprom();
    sys_spi_deselect_display();
}
