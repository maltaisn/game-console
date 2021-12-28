/*
 * Copyright 2021 Nicolas Maltais
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

#include <main.h>

#include <stdbool.h>

#include <sys/init.h>
#include <sys/comm.h>
#include <sys/sound.h>
#include <sys/flash.h>

#include <util/delay.h>
#include "sys/input.h"
#include "sys/led.h"

static const flash_t music_data[] = {
        0x0000, 0x04c6, 0x0946, 0x1221, 0x1617,
};
static const uint8_t tracks[] = {
        7, 3, 7, 7, 7,
};

static void load_music(uint8_t selected) {
    uint8_t tempo;
    flash_t addr = music_data[selected];
    flash_read(addr, 1, &tempo);

    sound_stop(TRACKS_STARTED_ALL);
    sound_set_tempo(tempo);
    sound_start(tracks[selected]);
    sound_load(addr + 1);
}

int main(void) {
    init();

    sound_set_volume(SOUND_VOLUME_1);

    uint8_t last_input = 0;
    uint8_t selected = 0;
    while (true) {
        comm_receive();

        if (!sound_check_tracks(TRACKS_PLAYING_ALL)) {
            // loop music if not playing
            load_music(selected);
        }

        uint8_t input = input_get_state();
        uint8_t mask = 1;
        for (uint8_t i = 0; i < 5; ++i) {
            if ((last_input & mask) && !(input & mask)) {
                // change music
                selected = i;
                load_music(selected);
                break;
            }
            mask <<= 1;
        }
        if ((last_input & BUTTON5) && !(input & BUTTON5)) {
            // special action button
            if (sound_check_tracks(TRACKS_STARTED_ALL)) {
                sound_stop(TRACKS_STARTED_ALL);
            } else {
                sound_start(TRACKS_STARTED_ALL);
            }
//            sound_volume_t vol = sound_get_volume();
//            if (vol == SOUND_VOLUME_3) {
//                vol = SOUND_VOLUME_OFF;
//            } else if (vol == SOUND_VOLUME_OFF) {
//                vol = SOUND_VOLUME_0;
//            } else {
//                vol += SOUND_VOLUME_INCREMENT;
//            }
//            sound_set_volume(vol);
        }
        last_input = input;

        // emulate game loop delay
        _delay_ms(30);
    }
}
