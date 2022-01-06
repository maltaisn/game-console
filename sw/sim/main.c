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

#include "sys/main.h"
#include "sys/power.h"
#include "sys/display.h"

#include "sim/time.h"
#include "sim/glut.h"
#include "sim/eeprom.h"
#include "sim/flash.h"
#include "sim/led.h"
#include "sim/sound.h"

#include <stdbool.h>
#include <GL/glut.h>

#include <pthread.h>

static void* loop_thread(void* arg) {
    while (true) {
        loop();
    }
    return 0;
}

int main(int argc, char** argv) {
    // == hardware initialization (similar to sys/init.c)
    // initialize display
    display_init();
    display_set_enabled(true);

    // enable power
    power_set_15v_reg_enabled(true);

    // check battery level on startup
    power_take_sample();
    power_wait_for_sample();
    sleep_if_low_battery();

    // == simulator initialization
    // initialize memories as initially empty; they can be loaded from a file later.
    eeprom_load_erased();
    flash_load_erased();
    time_init();
    sound_init();
    sound_open_stream();

    glutInit(&argc, argv);
    glut_init();

    setup();

    pthread_t thread;
    pthread_create(&thread, NULL, loop_thread, NULL);

    glutMainLoop();
    return 0;
}
