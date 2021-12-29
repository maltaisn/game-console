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
#include "sys/init.h"

#include "sim/time.h"
#include "sim/glut.h"
#include "sim/eeprom.h"
#include "sim/flash.h"

#include <stdbool.h>
#include <GL/glut.h>

#include <pthread.h>

static void* loop_thread(void* arg) {
    while (true) {
        loop();
    }
    return 0;
}

int main(void) {
    // initialize memories as initially empty; they can be loaded from a file later.
    eeprom_load_erased();
    flash_load_erased();

    time_init();

    glut_init();

    pthread_t thread;
    pthread_create(&thread, NULL, loop_thread, NULL);

    glutMainLoop();
}
