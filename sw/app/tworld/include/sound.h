
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

#ifndef TWORLD_SOUND_H
#define TWORLD_SOUND_H

#include <core/sound.h>

// track on which sound is playing
#define SOUND_TRACKS_STARTED (TRACK2_STARTED)

/**
 * Play a new sound. The previous sound is stopped immediately.
 */
void game_sound_play(sound_t sound);

#endif //TWORLD_SOUND_H
