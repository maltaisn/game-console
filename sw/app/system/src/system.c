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

#include <system.h>
#include <assets.h>
#include <comm.h>
#include <render.h>
#include <input.h>
#include <ui.h>

#include <sys/display.h>
#include <sys/uart.h>
#include <sys/flash.h>
#include <sys/eeprom.h>

#include <core/callback.h>
#include <core/data.h>
#include <core/dialog.h>
#include <core/time.h>
#include <core/defs.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

system_t state;

static systime_t last_draw_time;

void callback_setup(void) {
    // required for reasons highlighted in boot/src/main.c
    sys_display_init_page(DISPLAY_PAGE_HEIGHT);

    sys_uart_init(SYS_UART_BAUD_RATE(UART_BAUD));
    dialog_set_font(ASSET_FONT_7X7, ASSET_FONT_5X7);

    // mark both memories as dirty to do initial load.
    state.flags = SYSTEM_FLAG_EEPROM_DIRTY | SYSTEM_FLAG_FLASH_DIRTY;
    state.state = STATE_MAIN_MENU;
}

void callback_draw(void) {
    last_draw_time = time_get();
    draw();
}

bool callback_loop(void) {
    comm_receive();

    // reload index if flash or EEPROM have been modified.
    // this is also where they are loaded the first time.
    system_load_flash_index();
    system_load_eeprom_index();

    handle_input();

    if (!(state.flags & SYSTEM_FLAG_DIALOG_SHOWN)) {
        state.flags |= SYSTEM_FLAG_DIALOG_SHOWN;
        if (state.state == STATE_MAIN_MENU) {
            open_main_dialog();
        } else {
            system_init_position();
            open_sub_dialog(state.state);
        }
    }

    systime_t time = time_get();
    return time - last_draw_time > millis_to_ticks(1000.0 / DISPLAY_MAX_FPS);
}

// see flash memory layout in sys/flash.h
typedef struct {
    uint8_t id;
    uint32_t : 32;
    uint16_t app_version;
    uint16_t boot_version;
    uint16_t code_size;
    uint32_t : 24;
    uint16_t eeprom_size;
    uint64_t : 64;
    uint32_t : 24;
    uint32_t app_size : 24;
    uint16_t build_date;
    char name[16];
    char author[16];
} PACK_STRUCT flash_entry_t;

// see eeprom memory layout in sys/eeprom.h
typedef struct {
    uint8_t id;
    uint16_t : 16;
    uint16_t size;
} PACK_STRUCT eeprom_entry_t;

// points the the first element of the current index (for sorting).
static void* curr_index;
// indicates the size of each member of the current index array.
static uint8_t curr_index_sizeof;

static int memory_usage_compare(const void* ap, const void* bp) {
    uint8_t a_idx = *((const uint8_t*) ap);
    uint8_t b_idx = *((const uint8_t*) bp);
    // zero-initialized since uint24_t is uint32_t in simulator.
    uint24_t a = 0;
    uint24_t b = 0;
    memcpy(&a, (uint8_t*) curr_index + curr_index_sizeof * a_idx + 1, 3);
    memcpy(&b, (uint8_t*) curr_index + curr_index_sizeof * b_idx + 1, 3);
    if (a < b) {
        return 1;
    } else if (a > b) {
        return -1;
    } else {
        return 0;
    }
}

void system_load_flash_index(void) {
    if (!(state.flags & SYSTEM_FLAG_FLASH_DIRTY)) {
        return;
    }
    state.flags &= ~SYSTEM_FLAG_FLASH_DIRTY;

    // check signature
    uint16_t signature;
    sys_flash_read_absolute(0, sizeof signature, &signature);
    if (signature != SYS_FLASH_SIGNATURE) {
        // flash wasn't initialized yet, no apps.
        state.flash_usage.size = 0;
        state.flash_usage.total = 0;
        return;
    }

    // read index from flash
    uint16_t address = SYS_FLASH_INDEX_ADDR;
    app_flash_t* index = state.flash_index;
    flash_entry_t entry;
    uint8_t size = 0;
    state.flash_usage.total = SYS_FLASH_DATA_START_ADDR;
    for (uint8_t i = 0; i < APP_INDEX_SIZE; ++i) {
        // read flash entry
        sys_flash_read_absolute(address, sizeof(entry), &entry);
        if (entry.id != APP_ID_NONE) {
            index->id = entry.id;
            index->app_version = entry.app_version;
            index->boot_version = entry.boot_version;
            index->app_version = entry.app_version;
            index->boot_version = entry.boot_version;
            index->app_size = entry.app_size;
            index->code_size = entry.code_size;
            index->eeprom_size = entry.eeprom_size;
            index->build_date = entry.build_date;
            index->index = i;
            ++index;

            state.flash_usage.total += entry.app_size;
            state.flash_usage.index[size] = size;
            ++size;
        }
        address += SYS_FLASH_INDEX_ENTRY_SIZE;
    }
    state.flash_usage.size = size;

    // sort entries by size in the usage index (qsort has an outrageous code size but whatever)
    curr_index = state.flash_index;
    curr_index_sizeof = sizeof(state.flash_index) / APP_INDEX_SIZE;
    qsort(state.flash_usage.index, size, sizeof(uint8_t), memory_usage_compare);

    system_init_position();
}

void system_load_eeprom_index(void) {
    if (!(state.flags & SYSTEM_FLAG_EEPROM_DIRTY)) {
        return;
    }
    state.flags &= ~SYSTEM_FLAG_EEPROM_DIRTY;

    // check signature
    uint16_t signature;
    sys_eeprom_read_absolute(0, sizeof signature, &signature);
    if (signature != SYS_EEPROM_SIGNATURE) {
        // EEPROM wasn't initialized yet, no apps.
        state.eeprom_usage.size = 0;
        state.eeprom_usage.total = 0;
        return;
    }

    // read index from EEPROM
    uint8_t address = SYS_EEPROM_INDEX_ADDR;
    app_eeprom_t* index = state.eeprom_index;
    eeprom_entry_t entry;
    uint8_t size = 0;
    state.eeprom_usage.total = SYS_EEPROM_DATA_START_ADDR;
    for (uint8_t i = 0; i < APP_INDEX_SIZE; ++i) {
        // read flash entry
        sys_eeprom_read_absolute(address, sizeof(entry), &entry);
        if (entry.id != APP_ID_NONE) {
            index->id = entry.id;
            index->size = entry.size;
            ++index;

            state.eeprom_usage.total += entry.size;
            state.eeprom_usage.index[size] = size;
            ++size;
        }
        address += SYS_EEPROM_INDEX_ENTRY_SIZE;
    }
    state.eeprom_usage.size = size;

    // sort entries by size in the usage index
    curr_index = state.eeprom_index;
    curr_index_sizeof = sizeof(state.eeprom_index) / APP_INDEX_SIZE;
    qsort(state.eeprom_usage.index, size, sizeof(uint8_t), memory_usage_compare);

    system_init_position();
}

void system_init_position(void) {
    state.position = 0;
    state.max_position = 0;
    if (state.state == STATE_APPS) {
        if (state.flash_usage.size > 0) {
            state.max_position = state.flash_usage.size - 1;
        }
    } else if (state.state == STATE_BATTERY) {
        // no navigation in battery dialog.
        return;
    } else {
        uint8_t size = (state.state == STATE_FLASH ?
                        &state.flash_usage : &state.eeprom_usage)->size;
        if (size > MEMORY_APPS_PER_SCREEN) {
            state.max_position = size - MEMORY_APPS_PER_SCREEN;
        }
    }
}

void system_get_app_name(uint8_t pos, char name[static 16]) {
    sys_flash_read_absolute(SYS_FLASH_INDEX_ADDR + SYS_FLASH_INDEX_ENTRY_SIZE * pos + 32, 16, name);
}

void system_get_app_author(uint8_t pos, char name[static 16]) {
    sys_flash_read_absolute(SYS_FLASH_INDEX_ADDR + SYS_FLASH_INDEX_ENTRY_SIZE * pos + 48, 16, name);
}

void system_get_app_name_by_id(uint8_t id, char name[static 16]) {
    for (uint8_t i = 0; i < APP_INDEX_SIZE; ++i) {
        const app_flash_t *app = &state.flash_index[i];
        if (app->id == id) {
            system_get_app_name(app->index, name);
            return;
        }
    }
    sprintf(name, "Unknown [%" PRIu8 "]", id);
}
