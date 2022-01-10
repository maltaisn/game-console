
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

#include <render.h>
#include <game.h>
#include <tetris.h>
#include <ui.h>
#include <assets.h>

#include <sys/power.h>

#include <core/graphics.h>
#include <core/sysui.h>
#include <core/dialog.h>

#include <string.h>

#define TILE_WIDTH 6
#define TILE_HEIGHT 6

#define tile_asset(piece) (ASSET_TILE + (piece) * ASSET_TILE_OFFSET)

/**
 * Draw a centered piece in a 24x12 rectangle with the top left corner at (x, y).
 */
static void draw_centered_piece_at(disp_x_t x, disp_y_t y, tetris_piece piece) {
    if (piece == TETRIS_PIECE_NONE) {
        return;
    }

    // offset since piece data is a 5x5 grid and we're drawing from pos (1,1)
    x -= TILE_WIDTH;
    y -= TILE_HEIGHT;
    // do some adjustments to center piece depending on its type
    if (piece == TETRIS_PIECE_I) {
        y -= TILE_HEIGHT / 2;
    } else if (piece != TETRIS_PIECE_O) {
        x += TILE_WIDTH / 2;
    }

    // draw each block in piece.
    const uint8_t* piece_data = &TETRIS_PIECES_DATA[piece * ROTATIONS_COUNT * BLOCKS_PER_PIECE];
    for (uint8_t i = 0; i < BLOCKS_PER_PIECE; ++i) {
        uint8_t block = piece_data[i];
        disp_x_t px = x + (block >> 4) * TILE_WIDTH;
        disp_y_t py = y + (PIECE_GRID_SIZE - (block & 0xf) - 1) * TILE_HEIGHT;
        graphics_image(tile_asset(piece), px, py);
    }
}

static char* format_number(uint32_t n, uint8_t len, char buf[static len]) {
    buf[--len] = '\0';
    do {
        buf[--len] = (char) (n % 10 + '0');
        n /= 10;
    } while (n != 0);
    return &buf[len];
}

static void format_number_pad(uint32_t n, uint8_t len, char buf[static len]) {
    buf[--len] = '\0';
    do {
        buf[--len] = (char) (n % 10 + '0');
        n /= 10;
    } while (len);
}

static void draw_game(void) {
    char buf[16];
    uint8_t preview_pieces = tetris.options.preview_pieces;
    bool hold_piece = tetris.options.features & TETRIS_FEATURE_HOLD;

    // score
    format_number_pad(tetris.score, 9, buf);
    graphics_set_color(11);
    graphics_set_font(ASSET_FONT_7X7);
    graphics_text(65, 2, buf);


    // game area frame
    graphics_set_color(4);
    graphics_vline(0, 127, 0);
    graphics_vline(0, 127, 63);

    // next pieces (except immediate next)
    uint8_t hold_piece_y;
    if (preview_pieces > 0) {
        uint8_t next_height = (preview_pieces - 1) * 15 + 3;
        if (preview_pieces > 1) {
            graphics_rect(98, 35, 30, next_height);
            uint8_t piece_y = 38;
            for (uint8_t i = 1; i < preview_pieces; ++i) {
                draw_centered_piece_at(101, piece_y, tetris.piece_bag[tetris.bag_pos + i]);
                piece_y += 15;
            }
        }
        if (preview_pieces > 3) {
            hold_piece_y = next_height + 17;
        } else {
            hold_piece_y = 62;
        }
    } else {
        hold_piece_y = 35;
    }

    // held piece
    if (hold_piece) {
        graphics_rect(66, hold_piece_y, 30, 18);
        draw_centered_piece_at(69, hold_piece_y + 3, tetris.hold_piece);
    }

    // immediate next piece
    if (preview_pieces > 0) {
        graphics_set_color(7);
        graphics_rect(66, 35, 30, 18);
        draw_centered_piece_at(69, 38, tetris.piece_bag[tetris.bag_pos]);
    }

    // game grid (top row is not drawn)
    disp_x_t block_x = 3;
    const tetris_piece* grid_ptr = (const tetris_piece*) tetris.grid;
    for (uint8_t i = 0; i < GRID_WIDTH; ++i) {
        tetris_piece piece;
        disp_y_t block_y = DISPLAY_HEIGHT;
        // rows 0-20
        for (uint8_t j = 0; j < GRID_HEIGHT - 1; ++j) {
            block_y -= TILE_HEIGHT;
            piece = *grid_ptr++;
            if (piece != TETRIS_PIECE_NONE) {
                graphics_image(tile_asset(piece), block_x, block_y);
            }
        }
        // row 21 (only partly shown)
        piece = *grid_ptr++;
        if (piece != TETRIS_PIECE_NONE) {
            graphics_image_region(tile_asset(piece), block_x, 0, 0, 4, TILE_WIDTH - 1, 5);
        }
        block_x += TILE_WIDTH;
    }

    // last clear bonus info
    graphics_set_font(GRAPHICS_BUILTIN_FONT);
    // TODO

    // level
    char* level_buf = format_number(tetris.level, 3, buf) - 6;
    memcpy(level_buf, "LEVEL ", 6);
    graphics_set_color(11);
    graphics_text(67, 11, level_buf);

    // lines cleared
    char* lines_buf = format_number(tetris.lines, 6, buf);
    if (tetris.lines == 1) {
        memcpy(buf + 5, " LINE", 6);
    } else {
        memcpy(buf + 5, " LINES", 7);
    }
    graphics_text(67, 18, lines_buf);

    // hold piece & next piece text
    graphics_set_color(13);
    if (preview_pieces > 0) {
        const char* next_text;
        if (preview_pieces == 1) {
            next_text = "NEXT PIECE";
        } else {
            next_text = "NEXT PIECES";
        }
        graphics_text(66, 28, next_text);
    }
    if (hold_piece) {
        graphics_text(66, (int8_t) (hold_piece_y - 7), "HOLD");
    }
}

void draw(void) {
    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        // low power sleep scheduled, show low battery UI before sleeping.
        sysui_battery_sleep();
        return;
    }

    graphics_clear(DISPLAY_COLOR_BLACK);

    if (game.state < GAME_STATE_PLAY) {
        // TODO draw main menu image
    } else {
        draw_game();
    }

    if (game.dialog_shown) {
        dialog_draw();
        if (game.state == GAME_STATE_LEADERBOARD) {
            draw_leaderboard_overlay();
        } else if (game.state == GAME_STATE_CONTROLS || game.state == GAME_STATE_CONTROLS_PLAY) {
            draw_controls_overlay();
        }
    }
}


