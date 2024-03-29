
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

#ifndef TWORLD_MUSIC_H
#define TWORLD_MUSIC_H

#include <core/sound.h>

#include <stdbool.h>
#include <stdint.h>

// delay in game ticks before starting a different music (= 310 ms)
#define MUSIC_START_DELAY 5

#define MUSIC_NONE 0

// tracks on which music is playing
#define MUSIC_TRACKS_STARTED (TRACK0_STARTED | TRACK1_STARTED)

enum {
    // Indicates that music will loop when started.
    MUSIC_FLAG_LOOP = 1 << 0,
    // Indicates that music will start with a delay.
    MUSIC_FLAG_DELAYED = 1 << 1,
    // Indicates that music should be considered a sound effect as well.
    MUSIC_FLAG_SOUND_EFFECT = 1 << 2,
};

/**
 * Start music playback for current level (depends on level number).
 */
void game_music_start_level_music(uint8_t flags);

/**
 * Start music playback, if music is enabled.
 * Music can be looped, and can start with a fixed delay.
 */
void game_music_start(sound_t music, uint8_t flags);

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

#endif //TWORLD_MUSIC_H
