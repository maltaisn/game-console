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

#ifndef TETRIS_TETRIS_H
#define TETRIS_TETRIS_H

#include <stdint.h>
#include <stdbool.h>

#include <sys/defs.h>
#include <sys/time.h>

// game tick in number of system ticks, on which a state update is made and input is read.
// this is 64 ticks per second, or roughly 15.6 ms per tick.
#define GAME_TICK 4

// Various game delays, in game ticks.
#define DELAYED_AUTO_SHIFT 10
#define AUTO_REPEAT_RATE 2
#define ENTRY_DELAY 6

#define LOCK_DELAY 32
#define LOCK_MOVES 15

#define LINES_PER_LEVEL 10

#define PIECES_COUNT 7
#define BLOCKS_PER_PIECE 4
#define ROTATIONS_COUNT 4
#define LEVELS_COUNT 21

#define GRID_WIDTH 10
#define GRID_HEIGHT 22
#define GRID_SPAWN_ROW 20
#define PIECE_GRID_SIZE 5
#define SPAWN_PIECE_OFFSET 2
#define LAST_ROT_NONE 0xff

#define SOFT_DROP_PTS_PER_CELL 1
#define HARD_DROP_PTS_PER_CELL 2
#define BACK_TO_BACK_MULTIPLIER(pts) ((pts) * 3 / 2)  // *1.5
#define COMBO_POINTS 50
#define DIFFICULT_CLEAR_MIN_LINES 4

// all bonuses below are multiplied by this number
// bonuses values are defined in tetris.c
#define TETRIS_BONUS_MUL 100

extern const uint8_t TETRIS_PIECES_DATA[];

typedef enum {
    TETRIS_DIR_CW,
    TETRIS_DIR_CCW,
} tetris_rot_dir;

typedef enum {
    TETRIS_ROT_O,
    TETRIS_ROT_R,
    TETRIS_ROT_2,
    TETRIS_ROT_L,
} tetris_rot;

typedef enum {
    TETRIS_PIECE_I,
    TETRIS_PIECE_J,
    TETRIS_PIECE_L,
    TETRIS_PIECE_O,
    TETRIS_PIECE_S,
    TETRIS_PIECE_T,
    TETRIS_PIECE_Z,
    TETRIS_PIECE_GHOST,
    TETRIS_PIECE_NONE,
} tetris_piece;

typedef enum {
    TETRIS_TSPIN_NONE,
    TETRIS_TSPIN_PROPER,
    TETRIS_TSPIN_MINI,
} tetris_tspin;

typedef enum {
    TETRIS_FEATURE_HOLD = 1 << 0,
    TETRIS_FEATURE_GHOST = 1 << 1,
    TETRIS_FEATURE_WALL_KICKS = 1 << 2,
    TETRIS_FEATURE_TSPINS = 1 << 3,
} tetris_features_t;

typedef struct {
    uint8_t features;
    uint8_t preview_pieces;
} tetris_options_t;

typedef enum {
    TETRIS_FLAG_GAME_OVER = 1 << 0,
    TETRIS_FLAG_LAST_PERFECT = 1 << 1,
    TETRIS_FLAG_LAST_DIFFICULT = 1 << 2,
    TETRIS_FLAG_PIECE_AT_BOTTOM = 1 << 3,
    TETRIS_FLAG_PIECE_SWAPPED = 1 << 4,
} tetris_flags_t;

typedef struct {
    tetris_options_t options;
    tetris_piece grid[GRID_WIDTH][GRID_HEIGHT];

    uint8_t flags;

    uint8_t drop_delay;
    uint8_t lock_delay;
    uint8_t entry_delay;
    uint8_t level_drop_delay;
    uint8_t lock_moves;

    uint32_t score;
    uint16_t lines;
    uint16_t level;

    uint32_t last_points;
    uint8_t combo_count;
    uint8_t last_lines_cleared;
    tetris_tspin last_tspin;

    tetris_piece piece_bag[PIECES_COUNT * 2];
    uint8_t bag_pos;
    tetris_piece hold_piece;

    tetris_piece curr_piece;
    tetris_rot curr_piece_rot;
    int8_t curr_piece_x;
    int8_t curr_piece_y;
    uint8_t last_rot_offset;
} tetris_t;

extern tetris_t tetris;

void tetris_init(void);

void tetris_update(void);

void tetris_remove_piece(void);

void tetris_remove_ghost_piece(void);

bool tetris_can_place_piece(void);

void tetris_place_piece(void);

bool tetris_try_move(void);

void tetris_move_left(void);

void tetris_move_right(void);

void tetris_move_down(bool is_soft_drop);

void tetris_hard_drop(void);

void tetris_rotate_piece(tetris_rot_dir direction);

void tetris_lock_piece(void);

void tetris_hold_or_swap_piece(void);

void tetris_shuffle_bag(void);

void tetris_next_piece(void);

void tetris_spawn_piece(tetris_piece piece);

void tetris_update_score(tetris_tspin tspin, uint8_t lines_cleared);

tetris_tspin tetris_detect_tspin(void);

#endif //TETRIS_TETRIS_H
