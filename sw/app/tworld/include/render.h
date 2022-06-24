
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

#define GAME_MAP_SIZE 9
#define GAME_TILE_SIZE 14

// low timer overlay will be shown if time left is less than this value.
#define LOW_TIMER_THRESHOLD (20 * TICKS_PER_SECOND)

void draw(void);

#endif //TWORLD_RENDER_H
