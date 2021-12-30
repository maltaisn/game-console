
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

#include "sys/time.h"

#include <GL/glut.h>
#include "sim/time.h"
#include "sim/input.h"
#include "sim/display.h"

static void window_draw(void) {
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glScalef((float) WINDOW_WIDTH / DISPLAY_WIDTH, (float) WINDOW_HEIGHT / DISPLAY_HEIGHT, 1);
    display_draw();
    glPopMatrix();
}

static void callback_display(void) {
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, 10);  // invert Y coordinate

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glLoadIdentity();
    window_draw();
    glPopMatrix();

    glutSwapBuffers();
}

static void callback_reshape(int width, int height) {
    glutReshapeWindow(WINDOW_WIDTH, WINDOW_HEIGHT);
}

static void callback_time_timer(int arg) {
    glutTimerFunc((unsigned int) (1000.0 / SYSTICK_FREQUENCY + 0.5), callback_time_timer, 0);
    time_update();
}

static void callback_redisplay_timer(int arg) {
    glutTimerFunc((unsigned int) (1000.0 / DISPLAY_FPS + 0.5), callback_redisplay_timer, 0);
    glutPostRedisplay();
}

void glut_init(void) {
    // double buffered, RGB display.
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("Game console simulator");
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glutDisplayFunc(callback_display);
    glutReshapeFunc(callback_reshape);

    glutKeyboardFunc(input_on_key_down);
    glutKeyboardUpFunc(input_on_key_up);
    glutSpecialFunc(input_on_key_down_special);
    glutSpecialUpFunc(input_on_key_up_special);
    glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);

    glutTimerFunc((unsigned int) (1000.0 / SYSTICK_FREQUENCY + 0.5), callback_time_timer, 0);
    glutTimerFunc((unsigned int) (1000.0 / DISPLAY_FPS + 0.5), callback_redisplay_timer, 0);
}