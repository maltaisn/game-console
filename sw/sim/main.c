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

#ifndef SIMULATION_HEADLESS

#include <sys/main.h>
#include <sys/power.h>
#include <sys/init.h>
#include <sys/input.h>
#include <sys/uart.h>

#include <sim/time.h>
#include <sim/glut.h>
#include <sim/eeprom.h>
#include <sim/flash.h>
#include <sim/sound.h>
#include <sim/input.h>

#include <core/comm.h>
#include <core/sound.h>

#include <stdbool.h>
#include <GL/glut.h>

#include <pthread.h>
#include <unistd.h>

static void* loop_thread(void* arg) {
    while (true) {
#ifndef DISABLE_COMMS
        do {
            comm_receive();
            // in fast mode we're continuously decoding packets to avoid losing any data.
            // the loop will end when a fast mode disable packet is sent.
        } while (uart_is_in_fast_mode());
#endif
        sound_fill_track_buffers();
        input_dim_if_inactive();

        bool is_sleep_due = power_is_sleep_due();

        loop();

        if (is_sleep_due) {
            // if sleep was scheduled and is due, go to sleep.
            // loop() will have been called once with power_is_sleep_due() returning true
            // so that any special action can be taken.
            power_enable_sleep();
        }

        // 1 ms sleep (fixes responsiveness issues with keyboard input)
        time_sleep(1000);
    }
    return 0;
}

int main(int argc, char** argv) {
    // == simulator initialization
    time_init();
    sound_init();

    glutInit(&argc, argv);
    glut_init();
    input_init();

    // == main (equivalent to sys/main.c)
    init();
    setup();

    pthread_t thread;
    pthread_create(&thread, NULL, loop_thread, NULL);

    glutMainLoop();
    return 0;
}

#endif //SIMULATION_HEADLESS
