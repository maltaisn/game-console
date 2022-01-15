
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

#define STR1(x) #x
#define STR(x) STR1(x)

#define TILE_WIDTH 6
#define TILE_HEIGHT 6

// outer color, inner color, tile I to Z
static const disp_color_t TILE_COLORS[] = {
        15, 1,  // I
        4,  1,  // J
        7,  10, // L
        10, 7,  // O
        6,  1,  // S
        15, 12, // T
        12, 15, // Z
};

static void draw_tile_block(disp_x_t x, disp_y_t y, tetris_piece piece) {
    if (piece == TETRIS_PIECE_GHOST) {
        graphics_set_color(6);
        graphics_image(ASSET_IMAGE_TILE_GHOST, x, y);
    } else {
        graphics_set_color(TILE_COLORS[piece * 2]);
        graphics_fill_rect(x, y, TILE_WIDTH, TILE_HEIGHT);
        graphics_set_color(TILE_COLORS[piece * 2 + 1]);
        graphics_rect(x + 1, y + 1, TILE_WIDTH - 2, TILE_HEIGHT - 2);
    }
}

static void draw_tile_block_part(disp_x_t x, tetris_piece piece) {
    graphics_set_color(TILE_COLORS[piece * 2]);
    graphics_fill_rect(x, 0, TILE_WIDTH, 2);
    graphics_set_color(TILE_COLORS[piece * 2 + 1]);
    graphics_hline(x + 1, x + TILE_WIDTH - 2, 0);
}

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
        draw_tile_block(px, py, piece);
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

static void write_last_clear_info(char buf[static 16]) {
    if (tetris.last_points > 0) {
        // <Perfect | Line> clear x<lines>
        int8_t info_y = 101;
        if (tetris.last_lines_cleared > 0) {
            memcpy(buf + 10, " X ", 10);
            char* line_clear_buf;
            if (tetris.flags & TETRIS_FLAG_LAST_PERFECT) {
                line_clear_buf = buf + 3;
                memcpy(line_clear_buf, "PERFECT", 7);
            } else {
                line_clear_buf = buf;
                memcpy(line_clear_buf, "LINE CLEAR", 10);
            }
            buf[12] = (char) ('0' + tetris.last_lines_cleared);
            graphics_text(66, info_y, line_clear_buf);
            info_y += 6;
        }

        // [Mini] T-spin
        if (tetris.last_tspin != TETRIS_TSPIN_NONE) {
            graphics_text(66, info_y, tetris.last_tspin == TETRIS_TSPIN_PROPER ?
                                      "T-SPIN" : "MINI T-SPIN");
            info_y += 6;
        }

        // Combo x<count>
        if (tetris.combo_count > 1) {
            char* combo_buf = format_number(tetris.combo_count, 3, buf) - 7;
            memcpy(combo_buf, "COMBO X", 7);
            graphics_text(66, info_y, combo_buf);
            info_y += 6;
        }

        // +<points>
        char* pts_buf = format_number(tetris.last_points, 7, buf) - 1;
        pts_buf[0] = '+';
        graphics_text(66, info_y, pts_buf);
    }
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
        graphics_set_color(4);
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
                draw_tile_block(block_x, block_y, piece);
            }
        }
        // row 21 (only partly shown)
        piece = *grid_ptr++;
        if (piece != TETRIS_PIECE_NONE) {
            draw_tile_block_part(block_x, piece);
        }
        block_x += TILE_WIDTH;
    }

    // last clear bonus info
    graphics_set_font(GRAPHICS_BUILTIN_FONT);
    write_last_clear_info(buf);

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

static void draw_main_menu(void) {
    // background art
    graphics_image(ASSET_IMAGE_MENU, 0, 0);

    // version info at the bottom left corner
    graphics_set_font(GRAPHICS_BUILTIN_FONT);
    graphics_set_color(10);
    graphics_text(1, 122, "V" STR(GAME_VERSION_MAJOR) "." STR(GAME_VERSION_MINOR));
}

void draw(void) {
    graphics_clear(DISPLAY_COLOR_BLACK);

    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        // low power sleep scheduled, show low battery UI before sleeping.
        sysui_battery_sleep();
        return;
    }

    game_state_t s = game.state;
    if (s < GAME_STATE_PLAY) {
        draw_main_menu();
    } else {
        draw_game();
    }

    if (game.dialog_shown) {
        dialog_draw();
        if (s == GAME_STATE_LEADERBOARD || s == GAME_STATE_LEADERBOARD_PLAY) {
            draw_leaderboard_overlay();
        } else if (s == GAME_STATE_CONTROLS || s == GAME_STATE_CONTROLS_PLAY) {
            draw_controls_overlay();
        }
        if (s == GAME_STATE_MAIN_MENU || s == GAME_STATE_PAUSE || s == GAME_STATE_GAME_OVER) {
            sysui_battery_overlay();
        }
    }
}

#define CONTROLS_COUNT 8

static const char* CONTROL_NAMES[CONTROLS_COUNT] = {
        "Pause",
        "Move left",
        "Move right",
        "Rotate left",
        "Rotate right",
        "Soft drop",
        "Hard drop",
        "Hold/swap",
};
static const uint8_t CONTROL_BUTTONS[CONTROLS_COUNT] = {
        BUTTON_PAUSE,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_ROT_CCW,
        BUTTON_ROT_CW,
        BUTTON_DOWN,
        BUTTON_HARD_DROP,
        BUTTON_HOLD,
};

void draw_controls_overlay(void) {
    graphics_set_font(ASSET_FONT_5X7);
    disp_y_t y = 25;
    for (uint8_t i = 0; i < CONTROLS_COUNT; ++i) {
        // control name text
        graphics_set_color(DISPLAY_COLOR_WHITE);
        graphics_text(30, (int8_t) y, CONTROL_NAMES[i]);

        // illustrate the 6 buttons with the one used by the control highlighted.
        uint8_t buttons = CONTROL_BUTTONS[i];
        uint8_t mask = BUTTON0;
        disp_x_t button_x = 15;
        for (uint8_t j = 0; j < 3; ++j) {
            disp_y_t button_y = y;
            for (uint8_t k = 0; k < 2; ++k) {
                graphics_set_color(buttons & mask ? DISPLAY_COLOR_WHITE : 6);
                graphics_fill_rect(button_x, button_y, 3, 3);
                button_y += 4;
                mask <<= 1;
            }
            button_x += 4;
        }
        y += 10;
    }
}

void draw_leaderboard_overlay(void) {
    graphics_set_font(GRAPHICS_BUILTIN_FONT);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    disp_y_t y = 25;
    for (uint8_t i = 0; i < game.leaderboard.size; ++i) {
        graphics_text(13, (int8_t) y, game.leaderboard.entries[i].name);
        y += 8;
    }

    char score_buf[9];
    graphics_set_font(ASSET_FONT_5X7);
    graphics_set_color(13);
    y = 24;
    for (uint8_t i = 0; i < game.leaderboard.size; ++i) {
        const char* buf = format_number(game.leaderboard.entries[i].score, 9, score_buf);
        // right align number
        uint8_t x = 68 + (buf - score_buf) * (graphics_glyph_width() + GRAPHICS_GLYPH_SPACING);
        graphics_text((int8_t) x, (int8_t) y, buf);
        y += 8;
    }
}
