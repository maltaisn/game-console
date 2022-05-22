
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

#include <sound.h>
#include <game.h>

static struct {
    sound_t data[SOUND_MAX_SCHEDULED];
    uint8_t head;
    uint8_t tail;
    uint8_t delay;
    bool was_not_emptied;
} scheduler;

void game_sound_clear(void) {
    scheduler.tail = scheduler.head;
    scheduler.delay = 0;
    scheduler.was_not_emptied = false;
}

void game_sound_push(sound_t sound) {
    if (game.options.features & GAME_FEATURE_SOUND_EFFECTS) {
        scheduler.data[scheduler.head] = sound;
        scheduler.head = (scheduler.head + 1) % SOUND_MAX_SCHEDULED;
        if (!sound_check_tracks(TRACK2_PLAYING)) {
            scheduler.was_not_emptied = true;
        }
    }
}

void game_sound_update(uint8_t dt) {
    if (scheduler.delay > 0) {
        if (scheduler.delay > dt) {
            scheduler.delay -= dt;
        } else {
            scheduler.delay = 0;
            sound_load(scheduler.data[scheduler.tail]);
            scheduler.tail = (scheduler.tail + 1) % SOUND_MAX_SCHEDULED;
        }
    } else if (!sound_check_tracks(TRACK2_PLAYING)) {
        if (scheduler.tail == scheduler.head) {
            // empty scheduler
            scheduler.was_not_emptied = false;
            return;
        } else if (!scheduler.was_not_emptied) {
            // nothing was playing before, play the sound immediately.
            scheduler.delay = 1;
        } else {
            scheduler.delay = SOUND_START_DELAY;
        }
    }
}
