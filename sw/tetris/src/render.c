
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

#define TILE_WIDTH 6
#define TILE_HEIGHT 6

#define tile_asset(piece) (ASSET_TILE + (piece) * ASSET_TILE_OFFSET)

static void draw_piece_at(disp_x_t x, disp_y_t y, tetris_piece piece, tetris_rot rot) {
    const uint8_t* piece_data = &TETRIS_PIECES_DATA[(piece * ROTATIONS_COUNT + rot) * BLOCKS_PER_PIECE];
    for (uint8_t i = 0; i < BLOCKS_PER_PIECE; ++i) {
        uint8_t block = piece_data[i];
        disp_x_t px = x + (block >> 4) * TILE_WIDTH;
        disp_y_t py = y + (PIECE_GRID_SIZE - (block & 0xf) - 1) * TILE_HEIGHT;
        graphics_image(tile_asset(piece), px, py);
    }
}

static void draw_game(void) {
    // TODO draw game state

    // score
    // TODO

    // game area frame
    graphics_set_color(4);
    graphics_vline(0, 127, 1);
    graphics_vline(0, 127, 64);

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

    // preview pieces
    // TODO

    // held piece
    // TODO

    // last clear bonus info
    // TODO

    // level
    // TODO
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


