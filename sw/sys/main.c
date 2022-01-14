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

#include <sys/main.h>
#include <sys/init.h>
#include <sys/power.h>

#include <core/comm.h>
#include <core/sound.h>

#include <stdbool.h>

int main(void) {
    init();
    setup();
    while (true) {
#ifndef DISABLE_COMMS
        comm_receive();
#endif
        sound_fill_track_buffers();

        bool is_sleep_due = power_is_sleep_due();

        loop();

        if (is_sleep_due) {
            // if sleep was scheduled and is due, go to sleep.
            // loop() will have been called once with power_is_sleep_due() returning true
            // so that any special action can be taken.
            power_enable_sleep();
        }
    }
}
