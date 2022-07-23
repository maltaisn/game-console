
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

#include <assets.h>
#include <music.h>
#include <game.h>

#include <core/sound.h>

static sound_t current_music;
static sound_t loop_music;
static uint8_t music_start_delay;

void game_music_start_level_music(uint8_t flags) {
    sound_t music = (game.current_level & 1) ? ASSET_MUSIC_THEME1 : ASSET_MUSIC_THEME0;
    game_music_start(music, flags);
}

void game_music_start(sound_t music, uint8_t flags) {
    if (current_music != music && ((game.options.features & GAME_FEATURE_MUSIC) ||
                                   ((flags & MUSIC_FLAG_SOUND_EFFECT) &&
                                    (game.options.features & GAME_FEATURE_SOUND_EFFECTS)))) {
        current_music = music;
        // if not delayed, still put a delay of 1
        music_start_delay = flags & MUSIC_FLAG_DELAYED ? MUSIC_START_DELAY : 1;
        sound_stop(MUSIC_TRACKS_STARTED);
        if (flags & MUSIC_FLAG_LOOP) {
            loop_music = music;
        } else {
            loop_music = MUSIC_NONE;
        }
    }
}

void game_music_loop_next(sound_t music) {
    if (game.options.features & GAME_FEATURE_MUSIC) {
        loop_music = music;
    }
}

void game_music_stop(void) {
    sound_stop(MUSIC_TRACKS_STARTED);
    current_music = MUSIC_NONE;
    loop_music = MUSIC_NONE;
}

void game_music_update(uint8_t dt) {
    if (music_start_delay > 0) {
        // music started but start delay not elapsed yet.
        if (music_start_delay > dt) {
            music_start_delay -= dt;
            return;
        }
        music_start_delay = 0;

    } else if (!sound_check_tracks(TRACKS_PLAYING_ALL)) {
        // music finished playing, restart it if any.
        if (loop_music == MUSIC_NONE) {
            current_music = MUSIC_NONE;
            return;
        }
        current_music = loop_music;

    } else {
        return;
    }
    sound_load(current_music);
    sound_start(MUSIC_TRACKS_STARTED);
}
