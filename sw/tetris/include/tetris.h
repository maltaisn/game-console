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

#define GRID_WIDTH 10
#define GRID_HEIGHT 22

#define PIECES_COUNT 7
#define BLOCKS_PER_PIECE 4
#define ROTATIONS_COUNT 4

#define PIECE_GRID_SIZE 5

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

    uint24_t last_points;
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

/** Initialize the tetris state and start the game. */
void tetris_init(void);

/** Update the tetris state for a delta time in game ticks. */
void tetris_update(uint8_t dt);

/** Move the piece left if possible. */
void tetris_move_left(void);

/** Move the piece right if possible. */
void tetris_move_right(void);

/** Move the piece down if possible. If not, the piece is locked. */
void tetris_move_down(void);

/** Move the piece to the bottom and lock it. */
void tetris_hard_drop(void);

/** Rotate the piece in either direction, if possible. */
void tetris_rotate_piece(tetris_rot_dir direction);

/** Hold the current piece and spawn a new one, or swap it with the old already held. */
void tetris_hold_or_swap_piece(void);

#endif //TETRIS_TETRIS_H
