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

#include <core/graphics.h>
#include <core/flash.h>

#ifdef SIMULATION
#include <stdio.h>
#include <core/trace.h>
#else
#include <avr/io.h>
#include <util/crc16.h>
#endif

#include <string.h>

#define CODE_PAGE_SIZE 256
#define APP_INDEX_SIZE 32

uint8_t sys_boot_loaded_app_id;

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
} __attribute__((packed)) app_flash_t;

static BOOTLOADER_ONLY app_flash_t _app_index[APP_INDEX_SIZE];
static BOOTLOADER_ONLY uint8_t _app_count;

void load_read_index(void) {
    // check signature
    uint16_t signature;
    sys_flash_read_absolute(0, sizeof signature, &signature);
    if (signature != SYS_FLASH_SIGNATURE) {
        // flash wasn't initialized, so there are no apps.
        return;
    }

    // read index
    uint16_t address = SYS_FLASH_INDEX_ADDR;
    app_flash_t *index = _app_index;
    _app_count = 0;
    for (uint8_t i = 0; i < APP_INDEX_SIZE; ++i) {
        sys_flash_read_absolute(address, sizeof(app_flash_t), index);
        if (index->id != APP_ID_NONE && index->boot_version == BOOT_VERSION) {
            ++index;
            ++_app_count;
        }
        address += SYS_FLASH_INDEX_ENTRY_SIZE;
    }
}

uint8_t load_get_loaded(void) {
#ifdef SIMULATION
    // in simulation the loaded app is fixed at compilation.
    return APP_ID;
#else
    return sys_boot_loaded_app_id;
#endif
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
    uint8_t buf[CODE_PAGE_SIZE];
    uint16_t crc = 0xffff;
    uint8_t code_pages = (app->code_size + 255) >> 8;
    for (uint8_t i = 0; i < code_pages; ++i) {
        // read code page from flash
        sys_flash_read_absolute(src, sizeof buf, dst);

        // write program memory page
        memcpy(dst, buf, sizeof buf);
        _PROTECTED_WRITE(NVMCTRL.CTRLA, NVMCTRL_CMD_PAGEERASEWRITE_gc);
        while (NVMCTRL.STATUS & NVMCTRL_FBUSY_bm);

        // update the CRC
        uint8_t j = 0;
        do {
            crc = _crc_ccitt_update(crc, *dst++);
        } while (j++ < CODE_PAGE_SIZE - 1);

        src += sizeof buf;
    }

    return crc == app->crc_code;
#endif
}

void _load_app(uint8_t index) {
    app_flash_t *app = &_app_index[index];

    uint8_t curr_app[3];
    sys_eeprom_read_absolute(SYS_EEPROM_APP_ID_ADDR, sizeof curr_app, &curr_app);
    if (memcmp(app, curr_app, sizeof curr_app) != 0) {
        // if currently loaded app matches exactly with app entry, do not load it unnecessarily.
        // otherwise load the code from flash and program memory.
        if (!_flash_app_code(app)) {
            // CRC check failed, app code from last app is probably corrupt! Mark no app as loaded.
            uint8_t app_id = APP_ID_NONE;
            sys_eeprom_write_absolute(SYS_EEPROM_APP_ID_ADDR, 1, &app_id);
            return;
        }
        // persist the ID of the new loaded app.
        sys_eeprom_write_absolute(SYS_EEPROM_APP_ID_ADDR, sizeof curr_app, &app);
    }

    sys_display_init_page(app->page_height);
    sys_flash_set_offset(app->address + app->code_size);
    sys_eeprom_set_location(app->eeprom_offset, app->eeprom_size);

    sys_boot_loaded_app_id = app->id;

    __callback_setup();
}

graphics_image_t load_get_app_image(uint8_t index) {
    app_flash_t *app = &_app_index[index];
    return data_flash(app->address + app->code_size);
}

uint8_t load_get_app_count(void) {
    return _app_count;
}
