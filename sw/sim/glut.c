
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

#include "sim/glut.h"

#include <GL/glut.h>
#include "sim/time.h"
#include "sim/input.h"

static void window_draw(void) {
    // TODO
}

static void callback_display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, 10);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    window_draw();
    glPopMatrix();
    glutSwapBuffers();
}

static void time_update_wrapper(int arg) {
    time_update();
    glutTimerFunc(4, time_update_wrapper, 0);
}

void glut_init(void) {
    char* argv[1];
    int argc = 1;
    argv[0] = 0;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(512, 512);
    glutCreateWindow("Game console simulator");

    glutDisplayFunc(callback_display);

    glutTimerFunc(4, time_update_wrapper, 0);

    glutKeyboardFunc(input_on_key_down);
    glutKeyboardUpFunc(input_on_key_up);
    glutSpecialFunc(input_on_key_down_special);
    glutSpecialUpFunc(input_on_key_up_special);
    glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
}