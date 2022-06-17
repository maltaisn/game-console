
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

#include <stdint.h>

#define LEVEL_PACK_COUNT ASSET_LEVEL_PACKS_SIZE
#define LEVEL_PACK_NAME_MAX_LENGTH 12
#define LEVEL_PACK_MAX_LEVELS 160

typedef uint8_t level_pack_idx_t;
typedef uint8_t level_idx_t;

typedef struct {
    uint8_t total_levels;
    uint8_t completed_levels;
    level_idx_t last_unlocked;
    uint8_t completed_array[(LEVEL_PACK_MAX_LEVELS + 7) / 8];
    char name[LEVEL_PACK_NAME_MAX_LENGTH];
} level_pack_info_t;

typedef struct {
    // TODO
    uint8_t temp;
} level_t;

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
 * Load the currently selected level.
 * The result is stored in `tworld_data.level`.
 */
void level_read_level(void);

#endif //TWORLD_LEVEL_H
