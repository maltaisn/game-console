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

#include <tetris.h>
#include <sound.h>
#include <assets.h>

#include <core/random.h>

#include <string.h>

#define MAX_WALL_KICKS 5
#define LAST_ROT_NONE 0xff

// piece data is encoded using 4 bytes per piece, per rotation.
// each byte encodes the (X,Y) position of the blocks in each nibble,
// (X=0xf0, Y=0x0f), relative to the bottom left of 5x5 grid.
// each piece rotates relative to the center block.
// the block bytes are ordered from top row to bottom row.
// see [https://tetris.wiki/File:SRS-true-rotations.png] for reference.
const uint8_t TETRIS_PIECES_DATA[PIECES_COUNT * ROTATIONS_COUNT * BLOCKS_PER_PIECE] = {
        0x12, 0x22, 0x32, 0x42,
        0x23, 0x22, 0x21, 0x20,
        0x02, 0x12, 0x22, 0x32,
        0x24, 0x23, 0x22, 0x21,
        0x13, 0x12, 0x22, 0x32,
        0x23, 0x33, 0x22, 0x21,
        0x12, 0x22, 0x32, 0x31,
        0x23, 0x22, 0x11, 0x21,
        0x33, 0x12, 0x22, 0x32,
        0x23, 0x22, 0x21, 0x31,
        0x12, 0x22, 0x32, 0x11,
        0x13, 0x23, 0x22, 0x21,
        0x23, 0x33, 0x22, 0x32,
        0x22, 0x32, 0x21, 0x31,
        0x12, 0x22, 0x11, 0x21,
        0x13, 0x23, 0x12, 0x22,
        0x23, 0x33, 0x12, 0x22,
        0x23, 0x22, 0x32, 0x31,
        0x22, 0x32, 0x11, 0x21,
        0x13, 0x12, 0x22, 0x21,
        0x23, 0x12, 0x22, 0x32,
        0x23, 0x22, 0x32, 0x21,
        0x12, 0x22, 0x32, 0x21,
        0x23, 0x12, 0x22, 0x21,
        0x13, 0x23, 0x22, 0x32,
        0x33, 0x22, 0x32, 0x21,
        0x12, 0x22, 0x21, 0x31,
        0x23, 0x12, 0x22, 0x11,
};

// offset data for rotation and wall kicking
// each group of 4 bytes encodes offset data for a single offset from all rotations
// (O, R, 2, L in order). there are MAX_WALL_KICKS groups, except for the O piece
// which has only one offset since it is guaranteed to succeed.
// each byte encodes the signed (X, Y) offset in its two nibbles (X=0xf0, Y=0x0f),
// with each coordinate offset by +8 to allow for negative numbers.
// note that the offset doesn't matter since the offsets are always taken as a diffential pair.
// see [https://tetris.wiki/Super_Rotation_System#How_Guideline_SRS_Really_Works]
static const uint8_t OFFSET_DATA_JLSTZ[MAX_WALL_KICKS * ROTATIONS_COUNT] = {
        0x88, 0x88, 0x88, 0x88,
        0x88, 0x98, 0x88, 0x78,
        0x88, 0x97, 0x88, 0x77,
        0x88, 0x8a, 0x88, 0x8a,
        0x88, 0x9a, 0x88, 0x7a,
};
static const uint8_t OFFSET_DATA_I[MAX_WALL_KICKS * ROTATIONS_COUNT] = {
        0x88, 0x78, 0x79, 0x89,
        0x78, 0x88, 0x99, 0x89,
        0xa8, 0x88, 0x69, 0x89,
        0x78, 0x89, 0x98, 0x87,
        0xa8, 0x86, 0x68, 0x8a,
};
static const uint8_t OFFSET_DATA_O[ROTATIONS_COUNT] = {
        0x88, 0x87, 0x77, 0x78,
};
static const uint8_t* OFFSET_DATA[PIECES_COUNT] = {
        OFFSET_DATA_I,
        OFFSET_DATA_JLSTZ,
        OFFSET_DATA_JLSTZ,
        OFFSET_DATA_O,
        OFFSET_DATA_JLSTZ,
        OFFSET_DATA_JLSTZ,
        OFFSET_DATA_JLSTZ,
};

// for detecting T-spins, positions of the 4 corners of the T piece for spawn rotation,
// in same format as piece data. corners are given in clockwise order from front left corner.
// other rotations are derived by starting at a different index in the array.
// see [https://tetris.wiki/T-Spin]
static const uint8_t T_PIECE_CORNERS[4] = {
        0x13, 0x33, 0x31, 0x11
};

// milliseconds between each drop at each level.
// last value is used for level > LEVELS_COUNT.
static const uint8_t LEVELS_DROP_DELAY[LEVELS_COUNT] = {
        57, 52, 48, 44, 39, 35, 30, 23, 18, 12,
        11, 10, 9, 7, 6, 6, 5, 5, 4, 3, 3,
};

static const uint8_t TETRIS_LINE_CLEAR_PTS[] = {
        1, 3, 5, 8,
};
// additional points awarded for clearing (i+1) lines, leaving play field empty
static const uint8_t TETRIS_LINE_PERFECT_CLEAR_PTS[] = {
        7, 9, 13, 12,
};
// additional points awarded for doing a (mini) T-spin, clearing (i) lines.
static const uint8_t TETRIS_T_SPIN_PTS[] = {
        4, 7, 9, 11
};
static const uint8_t TETRIS_MINI_T_SPIN_PTS[] = {
        2, 3, 3, 3
};

tetris_t tetris;

static const uint8_t* get_curr_piece_data(void) {
    return &TETRIS_PIECES_DATA[(tetris.curr_piece * ROTATIONS_COUNT +
                                tetris.curr_piece_rot) * BLOCKS_PER_PIECE];
}

/**
 * Update score for a number of lines cleared and a T-spin.
 * All info about the last points made is stored for printing it.
 */
static void tetris_update_score(tetris_tspin tspin, uint8_t lines_cleared) {
    // maximum points that can be achieved in one shot is 130k (but not realistically achievable)
    uint24_t pts = 0;
    bool difficult;
    if (tspin != TETRIS_TSPIN_NONE) {
        // award T-spin (all T-spins clearing lines are considered difficult)
        game_sound_push(ASSET_SOUND_TSPIN);
        if (tspin == TETRIS_TSPIN_PROPER) {
            pts += TETRIS_T_SPIN_PTS[lines_cleared] * TETRIS_BONUS_MUL;
        } else {
            pts += TETRIS_MINI_T_SPIN_PTS[lines_cleared] * TETRIS_BONUS_MUL;
        }
        difficult = (lines_cleared > 0);
    } else {
        // normal cleared lines (considered difficult if 4 lines cleared)
        difficult = (lines_cleared >= DIFFICULT_CLEAR_MIN_LINES);
    }

    bool perfect_clear = false;
    if (lines_cleared > 0) {
        // check play field for perfect clear
        perfect_clear = true;
        for (uint8_t x = 0; x < GRID_WIDTH; ++x) {
            for (uint8_t y = 0; y < GRID_HEIGHT; ++y) {
                if (tetris.grid[x][y] != TETRIS_PIECE_NONE) {
                    perfect_clear = false;
                    break;
                }
            }
            if (!perfect_clear) {
                break;
            }
        }

        pts += TETRIS_LINE_CLEAR_PTS[lines_cleared - 1] * TETRIS_BONUS_MUL;
        if (perfect_clear) {
            game_sound_push(ASSET_SOUND_PERFECT);
            pts += TETRIS_LINE_PERFECT_CLEAR_PTS[lines_cleared - 1] * TETRIS_BONUS_MUL;
        } else {
            game_sound_push(ASSET_SOUND_CLEAR(lines_cleared - 1));
        }
    }

    // combo
    if (lines_cleared == 0) {
        tetris.combo_count = 0;
    } else {
        if (tetris.combo_count > 0) {
            game_sound_push(ASSET_SOUND_COMBO);
        }
        pts += tetris.combo_count * COMBO_POINTS;
        ++tetris.combo_count;
    }

    // level multiplier
    pts *= tetris.level + 1;

    // back-to-back multiplier
    if (difficult && (tetris.flags & TETRIS_FLAG_LAST_DIFFICULT)) {
        // back-to-back difficult line clear, use multiplier.
        pts = BACK_TO_BACK_MULTIPLIER(pts);
    }

    tetris.flags &= ~(TETRIS_FLAG_LAST_DIFFICULT | TETRIS_FLAG_LAST_PERFECT);
    if (tspin != TETRIS_TSPIN_NONE && difficult) {
        // only normal clears are considered for back-to-back multiplier.
        tetris.flags |= TETRIS_FLAG_LAST_DIFFICULT;
    }

    tetris.last_lines_cleared = lines_cleared;
    tetris.last_tspin = tspin;
    tetris.last_points = pts;
    if (perfect_clear) {
        tetris.flags |= TETRIS_FLAG_LAST_PERFECT;
    }

    tetris.score += pts;
}

/**
 * Returns the T-spin achieved once the piece is locked.
 */
static tetris_tspin tetris_detect_tspin(void) {
    if (!(tetris.options.features & TETRIS_FEATURE_TSPINS) ||
        tetris.curr_piece != TETRIS_PIECE_T || tetris.last_rot_offset == LAST_ROT_NONE) {
        // T-spins disabled, not a T piece, or last action wasn't a rotation: no T-spin.
        return TETRIS_TSPIN_NONE;
    }

    uint8_t front_corners = 0;
    uint8_t back_corners = 0;
    for (uint8_t i = 0; i < ROTATIONS_COUNT; ++i) {
        uint8_t pos = T_PIECE_CORNERS[(tetris.curr_piece_rot + i) % ROTATIONS_COUNT];
        int8_t x = (int8_t) (tetris.curr_piece_x + (pos >> 4));
        int8_t y = (int8_t) (tetris.curr_piece_y + (pos & 0xf));
        // all blocks out of the grid are considered set
        bool has_corner = x < 0 || y < 0 || x >= GRID_WIDTH || y >= GRID_HEIGHT
                          || tetris.grid[x][y] != TETRIS_PIECE_NONE;
        if (i < 2) {
            front_corners += has_corner;
        } else {
            back_corners += has_corner;
        }
    }

    if (front_corners == 2 && back_corners >= 1) {
        // two blocks on the front, at least one on the back.
        return TETRIS_TSPIN_PROPER;
    } else if (front_corners == 1 && back_corners == 2) {
        if (tetris.last_rot_offset == MAX_WALL_KICKS - 1) {
            // special case: mini t-spin except last rotation moved piece by 1x2 blocks, so proper t-spin
            return TETRIS_TSPIN_PROPER;
        } else {
            return TETRIS_TSPIN_MINI;
        }
    }

    // less than 3 corners: no T-spin
    return TETRIS_TSPIN_NONE;
}

/**
 * Returns true if the current piece can be placed on the grid without intersection.
 * The current piece and the ghost piece must be been removed first or this will fail.
 */
static bool tetris_can_place_piece(void) {
    const uint8_t* piece_data = get_curr_piece_data();
    for (uint8_t i = 0; i < BLOCKS_PER_PIECE; ++i) {
        uint8_t block = piece_data[i];
        int8_t x = (int8_t) (tetris.curr_piece_x + (block >> 4));
        int8_t y = (int8_t) (tetris.curr_piece_y + (block & 0xf));
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT ||
            tetris.grid[x][y] != TETRIS_PIECE_NONE) {
            // piece block is out of bounds, or on top of an existing block.
            return false;
        }
    }
    return true;
}

/**
 * Remove the ghost piece from the grid data.
 */
static void tetris_remove_ghost_piece(void) {
    if (!(tetris.options.features & TETRIS_FEATURE_GHOST)) {
        return;
    }
    for (uint8_t x = 0; x < GRID_WIDTH; ++x) {
        for (uint8_t y = 0; y < GRID_HEIGHT; ++y) {
            if (tetris.grid[x][y] == TETRIS_PIECE_GHOST) {
                tetris.grid[x][y] = TETRIS_PIECE_NONE;
            }
        }
    }
}

/**
 * Lock the current piece in place and update score,
 * then start delay for the entry of next piece.
 */
static void tetris_lock_piece(void) {
    tetris_remove_ghost_piece();

    tetris_tspin tspin = tetris_detect_tspin();

    // clear any full lines
    uint8_t lines_cleared = 0;
    for (uint8_t y = 0; y < GRID_HEIGHT; ++y) {
        bool line_full = true;
        for (uint8_t x = 0; x < GRID_WIDTH; ++x) {
            tetris_piece block = tetris.grid[x][y];
            if (block == TETRIS_PIECE_NONE) {
                line_full = false;
            }
            // shift line block to account for lines cleared below.
            tetris.grid[x][y] = TETRIS_PIECE_NONE;
            tetris.grid[x][y - lines_cleared] = block;
        }
        if (line_full) {
            ++lines_cleared;
        }
    }

    tetris_update_score(tspin, lines_cleared);

    // update level
    tetris.lines += lines_cleared;
    if (tetris.lines >= (tetris.level + 1) * LINES_PER_LEVEL) {
        ++tetris.level;
        if (tetris.level < LEVELS_COUNT) {
            tetris.level_drop_delay = LEVELS_DROP_DELAY[tetris.level];
        }
    }

    // spawn next piece after delay
    tetris.entry_delay = ENTRY_DELAY;
    tetris.curr_piece = TETRIS_PIECE_NONE;
}

/**
 * Place the current piece on the grid, as well as the ghost piece if enabled.
 * Also updates lock conditions and gravity drop delay.
 */
static void tetris_place_piece(void) {
    const uint8_t* piece_data = get_curr_piece_data();
    bool ghost_enabled = (tetris.options.features & TETRIS_FEATURE_GHOST) != 0;

    if (ghost_enabled) {
        // place ghost piece as low as possible
        int8_t piece_y = tetris.curr_piece_y;
        do {
            --tetris.curr_piece_y;
        } while (tetris_can_place_piece());
        ++tetris.curr_piece_y;
        for (uint8_t i = 0; i < BLOCKS_PER_PIECE; ++i) {
            uint8_t block = piece_data[i];
            int8_t x = (int8_t) (tetris.curr_piece_x + (block >> 4));
            int8_t y = (int8_t) (tetris.curr_piece_y + (block & 0xf));
            tetris.grid[x][y] = TETRIS_PIECE_GHOST;
        }
        tetris.curr_piece_y = piece_y;
    }

    bool piece_on_ghost = false;
    bool piece_was_at_bottom = (tetris.flags & TETRIS_FLAG_PIECE_AT_BOTTOM) != 0;
    tetris.flags &= ~TETRIS_FLAG_PIECE_AT_BOTTOM;
    for (uint8_t i = 0; i < BLOCKS_PER_PIECE; ++i) {
        uint8_t block = piece_data[i];
        int8_t x = (int8_t) (tetris.curr_piece_x + (block >> 4));
        int8_t y = (int8_t) (tetris.curr_piece_y + (block & 0xf));
        if (ghost_enabled && tetris.grid[x][y] == TETRIS_PIECE_GHOST) {
            // piece is on top of ghost!
            piece_on_ghost = true;
        }
        tetris.grid[x][y] = tetris.curr_piece;
        if (y == 0 || (tetris.grid[x][y - 1] != TETRIS_PIECE_NONE &&
                       tetris.grid[x][y - 1] != TETRIS_PIECE_GHOST)) {
            // piece data is encoded from top row to bottom row.
            // if there's a block below this one, that means the piece has reached the bottom.
            tetris.flags |= TETRIS_FLAG_PIECE_AT_BOTTOM;
        }
    }

    if (piece_on_ghost) {
        // piece is on top of ghost, so remove any ghost blocks left to not show it.
        tetris_remove_ghost_piece();
    }

    if (tetris.flags & TETRIS_FLAG_PIECE_AT_BOTTOM) {
        // update lock conditions
        tetris.lock_delay = LOCK_DELAY;
        --tetris.lock_moves;
        if (tetris.lock_moves == 0) {
            // all moves exhausted while piece was at bottom, lock.
            tetris_lock_piece();
        }
    } else if (piece_was_at_bottom) {
        // piece was at bottom but now isn't (can happen with due to wall kicks)
        // restart drop timer so piece can drop again.
        tetris.drop_delay = tetris.level_drop_delay;
    }
}

/**
 * Remove the current piece from the grid, as well as the ghost piece.
 */
static void tetris_remove_piece(void) {
    // remove blocks of current piece from grid.
    const uint8_t* piece_data = get_curr_piece_data();
    for (uint8_t i = 0; i < BLOCKS_PER_PIECE; ++i) {
        uint8_t block = piece_data[i];
        int8_t x = (int8_t) (tetris.curr_piece_x + (block >> 4));
        int8_t y = (int8_t) (tetris.curr_piece_y + (block & 0xf));
        tetris.grid[x][y] = TETRIS_PIECE_NONE;
    }
    tetris_remove_ghost_piece();
}

/**
 * Returns true and place the current piece if it can be placed.
 * Otherwise does nothing and returns false.
 */
static bool tetris_try_move(void) {
    if (tetris_can_place_piece()) {
        // replace piece, which was removed before move.
        tetris_place_piece();
        return true;
    }
    return false;
}

/**
 * Move the current piece one block down.
 * The move can be due to a soft drop or due to gravity.
 */
static void tetris_move_piece_down(bool is_soft_drop) {
    if (tetris.curr_piece == TETRIS_PIECE_NONE) {
        return;
    }

    tetris_remove_piece();
    --tetris.curr_piece_y;
    if (!tetris_try_move()) {
        // couldn't move down, lock piece, replace piece.
        ++tetris.curr_piece_y;
        tetris_place_piece();
        tetris_lock_piece();
    } else if (is_soft_drop) {
        tetris.score += SOFT_DROP_PTS_PER_CELL;
    }
    tetris.last_rot_offset = LAST_ROT_NONE;
}

/**
 * Shuffle the high half of the bag in place.
 */
static void tetris_shuffle_bag(void) {
    tetris_piece* bag = &tetris.piece_bag[PIECES_COUNT];
    for (uint8_t i = 0; i < PIECES_COUNT; ++i) {
        bag[i] = i;
    }
    for (uint8_t i = PIECES_COUNT - 1; i > 0; --i) {
        uint8_t pos = random8() % i;
        tetris_piece temp = bag[pos];
        bag[pos] = bag[i];
        bag[i] = temp;
    }
}

/**
 * Try to spawn a new piece. If spawned piece cannot be placed, the game is over.
 */
static void tetris_spawn_piece(tetris_piece piece) {
    // spawn new piece, in O rotation, centered, with lowest block on spawn row.
    tetris.curr_piece = piece;
    tetris.curr_piece_rot = TETRIS_ROT_O;
    tetris.curr_piece_x = (GRID_WIDTH - PIECE_GRID_SIZE) / 2;
    tetris.curr_piece_y = GRID_SPAWN_ROW - SPAWN_PIECE_OFFSET;
    tetris.drop_delay = tetris.level_drop_delay;
    tetris.lock_moves = LOCK_MOVES;
    tetris.flags &= ~(TETRIS_FLAG_PIECE_AT_BOTTOM | TETRIS_FLAG_PIECE_SWAPPED);
    if (tetris_can_place_piece()) {
        tetris_place_piece();
    } else {
        // can't place piece at spawn pos, game over.
        tetris.flags |= TETRIS_FLAG_GAME_OVER;
    }
}

/**
 * Place next piece by choosing one in the bag and spawning it.
 */
static void tetris_next_piece(void) {
    // get next piece in bag
    if (tetris.bag_pos == PIECES_COUNT) {
        // end of current set of pieces, move next set and shuffle a new one in its place.
        memcpy(&tetris.piece_bag[0], &tetris.piece_bag[PIECES_COUNT],
               PIECES_COUNT * sizeof(tetris_piece));
        tetris_shuffle_bag();
        tetris.bag_pos = 0;
    }

    tetris_spawn_piece(tetris.piece_bag[tetris.bag_pos++]);
}

void tetris_init(void) {
    for (int8_t x = 0; x < GRID_WIDTH; ++x) {
        for (int8_t y = 0; y < GRID_HEIGHT; ++y) {
            tetris.grid[x][y] = TETRIS_PIECE_NONE;
        }
    }

    tetris.flags = 0;

    tetris.drop_delay = 0;
    tetris.lock_delay = 0;
    tetris.entry_delay = 0;
    tetris.level_drop_delay = LEVELS_DROP_DELAY[0];
    tetris.lock_moves = 0;

    tetris.score = 0;
    tetris.lines = 0;
    tetris.level = 0;

    tetris.last_points = 0;
    tetris.combo_count = 0;
    tetris.last_lines_cleared = 0;
    tetris.last_tspin = TETRIS_TSPIN_NONE;

    tetris.bag_pos = PIECES_COUNT;
    tetris.hold_piece = TETRIS_PIECE_NONE;
    tetris.curr_piece = TETRIS_PIECE_NONE;

    tetris_shuffle_bag();
    tetris_next_piece();
}

void tetris_update(uint8_t dt) {
    if (tetris.flags & TETRIS_FLAG_GAME_OVER) {
        // game over, nothing to update.
        return;
    }

    if (tetris.entry_delay > 0) {
        // last piece was locked, new piece not spawned yet.
        if (tetris.entry_delay > dt) {
            tetris.entry_delay -= dt;
        } else {
            tetris.entry_delay = 0;
            tetris_next_piece();
        }
    } else if (tetris.flags & TETRIS_FLAG_PIECE_AT_BOTTOM) {
        // piece is at bottom, but wasn't locked yet.
        if (tetris.lock_delay > dt) {
            tetris.lock_delay -= dt;
        } else {
            tetris.lock_delay = 0;
            // lock delay elapsed, lock piece.
            // input is invalidated (by not handling).
            tetris_lock_piece();
            return;
        }
    } else {
        // piece is falling
        if (tetris.drop_delay > dt) {
            tetris.drop_delay -= dt;
        } else {
            // drop delay elapsed, drop piece one block.
            tetris.drop_delay = tetris.level_drop_delay;
            tetris_move_piece_down(true);
        }
    }
}

void tetris_move_left(void) {
    if (tetris.curr_piece == TETRIS_PIECE_NONE) {
        return;
    }

    tetris_remove_piece();
    --tetris.curr_piece_x;
    if (!tetris_try_move()) {
        // couldn't move left, undo move, replace piece.
        ++tetris.curr_piece_x;
        tetris_place_piece();
    }
    tetris.last_rot_offset = LAST_ROT_NONE;
}

void tetris_move_right(void) {
    if (tetris.curr_piece == TETRIS_PIECE_NONE) {
        return;
    }

    tetris_remove_piece();
    ++tetris.curr_piece_x;
    if (!tetris_try_move()) {
        // couldn't move left, undo move, replace piece.
        --tetris.curr_piece_x;
        tetris_place_piece();
    }
    tetris.last_rot_offset = LAST_ROT_NONE;
}


void tetris_move_down(void) {
    tetris_move_piece_down(false);
}

void tetris_hard_drop(void) {
    if (tetris.curr_piece == TETRIS_PIECE_NONE) {
        return;
    }

    // move piece down until it can't be placed, at which point bottom is reached.
    uint8_t cells_dropped = 0;
    tetris_remove_piece();
    do {
        --tetris.curr_piece_y;
        ++cells_dropped;
    } while (tetris_can_place_piece());

    game_sound_push(ASSET_SOUND_HARD_DROP);

    // undo last drop and replace piece, then lock it.
    ++tetris.curr_piece_y;
    tetris.last_rot_offset = LAST_ROT_NONE;
    tetris.score += (cells_dropped - 1) * HARD_DROP_PTS_PER_CELL;
    tetris_place_piece();
    tetris_lock_piece();
}

void tetris_rotate_piece(tetris_rot_dir direction) {
    if (tetris.curr_piece == TETRIS_PIECE_NONE) {
        return;
    }

    tetris_remove_piece();

    const tetris_rot old_rot = tetris.curr_piece_rot;
    const int8_t old_x = tetris.curr_piece_x;
    const int8_t old_y = tetris.curr_piece_y;

    tetris_rot new_rot;
    if (direction == TETRIS_DIR_CW) {
        new_rot = (old_rot == TETRIS_ROT_L ? TETRIS_ROT_O : old_rot + 1);
    } else {
        new_rot = (old_rot == TETRIS_ROT_O ? TETRIS_ROT_L : old_rot - 1);
    }
    tetris.curr_piece_rot = new_rot;

    // do wall kicks: try a few offsets before failing to rotate.
    // the first wall kick offsets are just the normal rotation.
    const uint8_t* src_offset = &OFFSET_DATA[tetris.curr_piece][old_rot];
    const uint8_t* dst_offset = &OFFSET_DATA[tetris.curr_piece][new_rot];
    for (uint8_t i = 0; i < MAX_WALL_KICKS; ++i) {
        int8_t ox = (int8_t) ((*src_offset >> 4) - (*dst_offset >> 4));
        int8_t oy = (int8_t) ((*src_offset & 0xf) - (*dst_offset & 0xf));
        tetris.curr_piece_x = (int8_t) (old_x + ox);
        tetris.curr_piece_y = (int8_t) (old_y + oy);
        if (tetris_try_move()) {
            tetris.last_rot_offset = i;
            return;
        }
        src_offset += ROTATIONS_COUNT;
        dst_offset += ROTATIONS_COUNT;

        if (!(tetris.options.features & TETRIS_FEATURE_WALL_KICKS)) {
            // wall kicks disabled, only try basic rotation
            break;
        }
    }

    // rotation failed, replace piece
    tetris.curr_piece_rot = old_rot;
    tetris.curr_piece_x = old_x;
    tetris.curr_piece_y = old_y;
    tetris_place_piece();
}

void tetris_hold_or_swap_piece(void) {
    if (tetris.curr_piece == TETRIS_PIECE_NONE) {
        return;
    }

    if (!(tetris.options.features & TETRIS_FEATURE_HOLD) ||
        tetris.flags & TETRIS_FLAG_PIECE_SWAPPED) {
        // cannot swap piece twice, already swapped or option disabled.
        return;
    }

    game_sound_push(ASSET_SOUND_HOLD);

    tetris_remove_piece();
    if (tetris.hold_piece == TETRIS_PIECE_NONE) {
        // no currently held piece, spawn a new one.
        tetris.hold_piece = tetris.curr_piece;
        tetris_next_piece();
    } else {
        // spawn held piece, hold current piece.
        tetris_piece piece = tetris.hold_piece;
        tetris.hold_piece = tetris.curr_piece;
        tetris_spawn_piece(piece);
    }
    tetris.flags |= TETRIS_FLAG_PIECE_SWAPPED;
}
