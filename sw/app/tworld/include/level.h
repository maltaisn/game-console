
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

#ifndef TWORLD_LEVEL_H
#define TWORLD_LEVEL_H

#include "assets.h"
#include "tworld.h"
#include "tworld_tile.h"
#include "tworld_actor.h"

#include <core/flash.h>

#include <stdint.h>
#include <stdbool.h>

#define LEVEL_PACK_COUNT ASSET_LEVEL_PACKS_SIZE
#define LEVEL_PACK_NAME_MAX_LENGTH 12
#define LEVEL_PACK_MAX_LEVELS 160

typedef uint8_t level_pack_idx_t;
typedef uint8_t level_idx_t;

/**
 * Data structure for a level pack.
 */
typedef struct {
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
 * Either pack data or level data is needed at a time, but never both.
 * To save on RAM, these two structures are placed in an union.
 */
typedef union {
    level_pack_info_t level_packs[LEVEL_PACK_COUNT];
    level_t level;
} level_data_t;

extern level_data_t tworld_data;

#define tworld (tworld_data.level)
#define tworld_packs (tworld_data.level_packs)

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
void level_get_title(char title[LEVEL_TITLE_MAX_LENGTH]);

/**
 * Returns the address in flash of the start of the hint for current level.
 * A level must be loaded before using this.
 */
flash_t level_get_hint(void);

/**
 * Copy trap linkage data from flash to array.
 * A level must be loaded before using this.
 */
void level_get_trap_linkage(link_t linkage[static LEVEL_LINKAGE_MAX_SIZE]);

/**
 * Copy cloner linkage data from flash to array.
 * A level must be loaded before using this.
 */
void level_get_cloner_linkage(link_t linkage[static LEVEL_LINKAGE_MAX_SIZE]);

/**
 * Set the current pack and current level for a password.
 * A level pack must be unlocked for the password to work for a level in it.
 * Returns true if password is valid and level was set.
 */
bool level_use_password(const char password[LEVEL_PASSWORD_LENGTH]);

#endif //TWORLD_LEVEL_H
