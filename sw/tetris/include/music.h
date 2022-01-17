
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

#ifndef TETRIS_MUSIC_H
#define TETRIS_MUSIC_H

#include <core/sound.h>

#include <stdbool.h>
#include <stdint.h>

// delay in game ticks before starting a different music (= 500 ms)
#define MUSIC_START_DELAY 32

#define MUSIC_NONE 0

// tracks on which music is playing
#define MUSIC_TRACKS_STARTED (TRACK0_STARTED | TRACK1_STARTED)

enum {
    MUSIC_FLAG_LOOP = 1 << 0,
    MUSIC_FLAG_DELAYED = 1 << 1,
};

/**
 * Start music playback, if music is enabled.
 * Music can be looped, and can start with a fixed delay.
 */
void game_music_start(sound_t music, uint8_t flags);

/**
 * Set the music to loop after the currently playing music is finished.
 * Can be `MUSIC_NONE` to disable looping without stopping music immediately.
 */
void game_music_loop_next(sound_t music);

/**
 * Stop music playback.
 */
void game_music_stop(void);

/**
 * Update music playback:
 * - Start delayed music.
 * - Loop music if enabled.
 */
void game_music_update(uint8_t dt);

#endif //TETRIS_MUSIC_H
