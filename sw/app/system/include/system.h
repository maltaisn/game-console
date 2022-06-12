
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

#ifndef SYSTEM_SYSTEM_H
#define SYSTEM_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include <core/defs.h>
#include <core/flash.h>
#include <core/eeprom.h>
#include <core/graphics.h>

// display frames per second
#ifdef SIMULATION
#define DISPLAY_MAX_FPS 24  // faster for debugging
#else
#define DISPLAY_MAX_FPS 8
#endif

#define APP_INDEX_SIZE 32
#define APP_ID_NONE 0

typedef enum {
    STATE_APPS = 0,
    STATE_FLASH = 1,
    STATE_EEPROM = 2,
    STATE_BATTERY = 3,

    STATE_MAIN_MENU,
    STATE_TERMINATE,
} state_t;

enum {
    SYSTEM_FLAG_DIALOG_SHOWN = 1 << 0,
    SYSTEM_FLAG_FLASH_DIRTY = 1 << 1,
    SYSTEM_FLAG_EEPROM_DIRTY = 1 << 2,
    SYSTEM_FLAG_BATTERY_CALIBRATION = 1 << 3,
};

typedef struct {
    uint8_t id;
    uint24_t app_size;
    uint16_t app_version;
    uint16_t boot_version;
    uint16_t code_size;
    uint16_t eeprom_size;
    uint16_t build_date;
    uint8_t index;
} app_flash_t;

typedef struct {
    uint8_t id;
    uint24_t size;
} app_eeprom_t;

typedef struct {
    uint8_t size;
    uint24_t total;
    uint8_t index[APP_INDEX_SIZE];
} mem_usage_t;

typedef struct {
    // general
    uint8_t flags;
    state_t state;
    state_t last_state;
    // flash index
    app_flash_t flash_index[APP_INDEX_SIZE];
    mem_usage_t flash_usage;
    // eeprom index
    app_eeprom_t eeprom_index[APP_INDEX_SIZE];
    mem_usage_t eeprom_usage;
    // sorted usage indices
    // used by sub dialogs to keep track of current position.
    uint8_t position;
    uint8_t max_position;
    // battery calibration
    disp_color_t battery_calib_color;
} system_t;

extern system_t state;

/**
 * If flash is marked as dirty, load or reload flash index.
 */
void system_load_flash_index(void);

/**
 * If EEPROM is marked as dirty, load or reload EEPROM index.
 */
void system_load_eeprom_index(void);

/**
 * Reset current position and set max position for current dialog.
 */
void system_init_position(void);

/**
 * Read the app name from the flash index, knowing its position in it.
 */
void system_get_app_name(uint8_t pos, char name[static 16]);

/**
 * Read the app author from the flash index, knowing its position in it.
 */
void system_get_app_author(uint8_t pos, char name[static 16]);

/**
 * Read the app name from the flash index, knowing its ID.
 * If the app is not in the flash index, a string with the formatted ID is returned: "Unknown (ID)".
 */
void system_get_app_name_by_id(uint8_t id, char name[static 16]);

#endif //SYSTEM_SYSTEM_H
