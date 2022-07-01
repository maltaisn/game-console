
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

#ifndef TWORLD_TWORLD_LEVEL_H
#define TWORLD_TWORLD_LEVEL_H

#define LEVEL_PACK_COUNT ASSET_LEVEL_PACKS_SIZE
#define LEVEL_PACK_NAME_MAX_LENGTH 12
#define LEVEL_PACK_MAX_LEVELS 160

// these maximum string lengths include the nul terminator
#define LEVEL_TITLE_MAX_LENGTH 40
#define LEVEL_HINT_MAX_LENGTH 128
#define LEVEL_PASSWORD_LENGTH 5

#include "assets.h"
#include "tworld.h"
#include "tworld_tile.h"
#include "tworld_actor.h"

#include <core/flash.h>

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t level_pack_idx_t;
typedef uint8_t level_idx_t;

enum {
    LEVEL_PACK_FLAG_UNLOCKED = 1 << 0,
};

/**
 * Data structure for a level pack.
 */
typedef struct {
    uint8_t flags;
    // Position in the global level list (for EEPROM).
    uint16_t pos;
    // Number of levels in the pack
    uint8_t total_levels;
    // Number of completed levels in the pack
    uint8_t completed_levels;
    // Index of the last unlocked level
    level_idx_t last_unlocked;
    // Bitset indicating which levels have been completed, little-endian.
    uint8_t completed_array[(LEVEL_PACK_MAX_LEVELS + 7) / 8];
    // Nul terminated pack name.
    char name[LEVEL_PACK_NAME_MAX_LENGTH];
} level_pack_info_t;

/**
 * Groups all data loaded when a level isn't loaded and not currently playing a level.
 */
typedef struct {
    level_pack_info_t packs[LEVEL_PACK_COUNT];
    char password_buf[LEVEL_PASSWORD_LENGTH];
} level_packs_t;

/**
 * Either pack data or level data is needed at a time, but never both.
 * To save on RAM, these two structures are placed in an union.
 */
typedef union {
    level_packs_t packs;
    level_t level;
} level_data_t;

extern level_data_t tworld_data;

#define tworld tworld_data.level
#define tworld_packs tworld_data.packs

/**
 * Load all the level packs.
 * The result is stored in `tworld_data.level_packs`.
 */
void level_read_packs(void);

/**
 * Load the currently selected level from flash and initialize it.
 * The result is stored in `tworld_data.level`.
 */
void level_read_level(void);

/**
 * Copy the level password from flash to a buffer.
 * A level must be loaded before using this.
 */
void level_get_password(char password[LEVEL_PASSWORD_LENGTH]);

/**
 * Copy the level title from flash to a buffer.
 * A level must be loaded before using this.
 */
flash_t level_get_title(void);

/**
 * Returns the address in flash of the start of the hint for current level.
 * A level must be loaded before using this.
 */
flash_t level_get_hint(void);

/**
 * Copy trap and cloner links data from flash to global array.
 * A level must be loaded before using this.
 */
void level_get_links(void);

/**
 * Set the current pack and current level for the entered password.
 * A level pack must be unlocked for the password to work for a level in it.
 * Returns true if password is valid and level was set.
 */
bool level_use_password(void);

/**
 * Returns true if specified level in pack is unlocked.
 */
bool level_is_unlocked(const level_pack_info_t *info, level_idx_t level);

#endif //TWORLD_TWORLD_LEVEL_H
