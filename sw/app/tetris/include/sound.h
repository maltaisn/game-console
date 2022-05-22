
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

#ifndef TETRIS_SOUND_H
#define TETRIS_SOUND_H

#include <core/sound.h>

// maximum number of sound scheduled to be played.
#define SOUND_MAX_SCHEDULED 4
// delay between sounds in game ticks
#define SOUND_START_DELAY 8
// track on which music is playing
#define SOUND_TRACKS_STARTED (TRACK2_STARTED)

/**
 * Clear the sound effect queue.
 */
void game_sound_clear(void);

/**
 * Schedule a new sound to be played next (on channel 2 only).
 * Maximum 4 sounds can be scheduled before the currently playing one finishes.
 */
void game_sound_push(sound_t sound);

/**
 * Update the sound scheduler.
 */
void game_sound_update(uint8_t dt);

#endif //TETRIS_SOUND_H
