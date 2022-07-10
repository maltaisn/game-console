
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

#ifndef TWORLD_RENDER_H
#define TWORLD_RENDER_H

#define LEVEL_PACKS_PER_SCREEN 4

#define LEVELS_PER_SCREEN_H 4
#define LEVELS_PER_SCREEN_V 3

#define HELP_TILE_PER_SCREEN 4
#define HELP_TILE_NAME_MAX_LENGTH 14

#define HINT_LINES_PER_SCREEN 5
#define HINT_TEXT_WIDTH 112

// Number of tiles shown in width and height on map.
#define GAME_MAP_SIZE 9
// The displayed size of a game tile in pixels.
#define GAME_TILE_SIZE 14
// The animation delay between variants (a power of two).
#define BOTTOM_ANIMATION_DELAY 0x4
// Number of tile variants
#define BOTTOM_TILE_VARIANTS 2

void draw(void);

#endif //TWORLD_RENDER_H
