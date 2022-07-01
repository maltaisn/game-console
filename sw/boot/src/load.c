/*
 * Copyright 2022 Nicolas Maltais
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

#include <load.h>

#include <boot/defs.h>

#include <sys/eeprom.h>
#include <sys/callback.h>
#include <sys/display.h>
#include <sys/app.h>

#include <core/graphics.h>
#include <core/flash.h>

#ifdef SIMULATION
#include <stdio.h>
#include <core/trace.h>
#else
#include <core/led.h>
#include <core/input.h>
#include <avr/io.h>
#include <util/crc16.h>
#include <util/delay.h>
#endif

#include <string.h>

#define APP_ID_SYSTEM 0xff

#define CODE_PAGE_SIZE 128
#define APP_INDEX_SIZE 32

// see flash memory layout in sys/flash.h
typedef struct {
    uint8_t id;
    uint16_t crc_all;
    uint16_t crc_code;
    uint16_t app_version;
    uint16_t boot_version;
    uint16_t code_size;
    uint8_t page_height;
    eeprom_t eeprom_offset;
    uint16_t eeprom_size;
    flash_t address;
} PACK_STRUCT app_flash_t;

static BOOTLOADER_ONLY app_flash_t _app_index[APP_INDEX_SIZE];
static BOOTLOADER_ONLY uint8_t _app_count;

static BOOTLOADER_ONLY uint8_t _loaded_app_id[3];  // ID + full CRC
static BOOTLOADER_ONLY uint8_t _loaded_app_index;

static void _load_app_setup(uint8_t id) {
#ifndef SIMULATION
    sys_loaded_app_id = id;

    // latch input so that apps don't see a click immediately on startup.
    input_latch();

    __callback_setup();
#endif
}

void load_read_index(void) {
    _app_count = 0;
    _loaded_app_index = LOADED_APP_NONE;

    // check signature
    uint16_t signature;
    sys_flash_read_absolute(0, sizeof signature, &signature);

    if (signature == SYS_FLASH_SIGNATURE) {
        sys_eeprom_read_absolute(SYS_EEPROM_APP_ID_ADDR, sizeof _loaded_app_id, &_loaded_app_id);

        // signature correct, read index
        uint16_t address = SYS_FLASH_INDEX_ADDR;
        app_flash_t *index = _app_index;
        for (uint8_t i = 0; i < APP_INDEX_SIZE; ++i) {
            sys_flash_read_absolute(address, sizeof(app_flash_t), index);
            if (index->id != SYS_APP_ID_NONE && index->boot_version == BOOT_VERSION) {
                if (memcmp(index, _loaded_app_id, sizeof _loaded_app_id) == 0) {
                    _loaded_app_index = _app_count;
                }
                ++index;
                ++_app_count;
            }
            address += SYS_FLASH_INDEX_ENTRY_SIZE;
        }
    }

    if (_app_count == 0) {
        // either flash wasn't initialized, or bootloader was just updated and all the apps
        // target the old bootloader version... in this case we'll suppose that the debug app
        // was bundled along with the bootloader and we can just jump to it directly.
        // note that this means the debug app must partially initialize itself (display page height),
        // and may not store any data in flash and eeprom since the offset hasn't been set.
        _load_app_setup(APP_ID_SYSTEM);
    }
}

static bool _flash_app_code(app_flash_t *app) {
#ifdef SIMULATION
    // do nothing, this can't be done in simulation.
    trace("app with ID %d loaded.", app->id);
    return false;
#else
    // read app code from flash and write internal flash memory.
    // also do a CRC check after writing
    extern uint8_t __APP_START_ADDRESS;
    uint8_t* dst = &__APP_START_ADDRESS;
    flash_t src = app->address;
    uint16_t crc = 0xffff;
    uint8_t code_pages = (app->code_size + CODE_PAGE_SIZE - 1) / CODE_PAGE_SIZE;
    for (uint8_t i = code_pages; i-- > 0;) {
        // Read code page from flash and write it to flash directly.
        // Note that the last page is written completely even if partial and that's fine.
        sys_flash_read_absolute(src, CODE_PAGE_SIZE, dst);

        _PROTECTED_WRITE_SPM(NVMCTRL.CTRLA, NVMCTRL_CMD_PAGEERASEWRITE_gc);
        while (NVMCTRL.STATUS & NVMCTRL_FBUSY_bm);

        src += CODE_PAGE_SIZE;

        // update the CRC
        uint8_t page_size = (i == 0 ? app->code_size % CODE_PAGE_SIZE : CODE_PAGE_SIZE);
        for (uint8_t j = 0; j < page_size; ++j) {
            crc = _crc_ccitt_update(crc, *dst++);
        }
    }

    return crc == app->crc_code;
#endif //SIMULATION
}

void _load_app(uint8_t index) {
    app_flash_t *app = &_app_index[index];

    if (index != _loaded_app_index) {
        // if currently loaded app matches exactly with app entry, do not load it unnecessarily.
        // otherwise load the code from flash and program memory.
        if (!_flash_app_code(app)) {
            // CRC check failed, app code from last app is probably corrupt! Mark no app as loaded.
            uint8_t app_id = SYS_APP_ID_NONE;
            sys_eeprom_write_absolute(SYS_EEPROM_APP_ID_ADDR, 1, &app_id);

            // keep the LED on for a while to indicate the error
#ifndef SIMULATION
            led_set();
            _delay_ms(500);
            led_clear();
#endif
            return;
        }
        // persist the ID of the new loaded app.
        sys_eeprom_write_absolute(SYS_EEPROM_APP_ID_ADDR, sizeof _loaded_app_id, app);
    }

    sys_display_init_page(app->page_height);
    sys_flash_set_offset(app->address + app->code_size);
    sys_eeprom_set_location(app->eeprom_offset, app->eeprom_size);

    _load_app_setup(app->id);
}

graphics_image_t load_get_app_image(uint8_t index) {
    app_flash_t *app = &_app_index[index];
    return data_flash(app->address + app->code_size);
}

uint8_t load_get_app_count(void) {
    return _app_count;
}

uint8_t load_get_loaded_app_index(void) {
    return _loaded_app_index;
}
