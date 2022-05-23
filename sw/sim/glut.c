
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

#include <sim/glut.h>
#include <sim/display.h>
#include <sim/led.h>
#include <sim/power.h>
#include <sim/sound.h>

#include <core/display.h>
#include <core/trace.h>

#include <GL/freeglut.h>
#include <math.h>
#include <stdio.h>

#define PI 3.141592654f

#define LED_SEGMENTS 30
#define LED_RADIUS 7.5f

static void draw_background(void) {
    glBegin(GL_QUADS);
    glColor3f(0.15f, 0.15f, 0.15f);
    glVertex2f(0, 0);
    glVertex2f(0, WINDOW_HEIGHT);
    glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glVertex2f(WINDOW_WIDTH, 0);
    glEnd();
}

static void draw_display_frame(void) {
    glBegin(GL_QUADS);
    glColor3f(0, 0, 0);
    glVertex2f(45, 15);
    glVertex2f(45, WINDOW_HEIGHT - 15);
    glVertex2f(WINDOW_WIDTH - 15, WINDOW_HEIGHT - 15);
    glVertex2f(WINDOW_WIDTH - 15, 15);
    glEnd();
}

static void draw_led(float x, float y, float r, float g, float b, bool on) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glBegin(GL_TRIANGLE_FAN);
    if (on) {
        glColor3f(r, g, b);
    } else {
        glColor3f(0, 0, 0);
    }
    glVertex2f(0, 0);
    for (int i = 0; i <= LED_SEGMENTS; ++i) {
        float angle = (float) i / LED_SEGMENTS * 2 * PI;
        glVertex2f(cosf(angle) * LED_RADIUS, sinf(angle) * LED_RADIUS);
    }
    glEnd();
    glPopMatrix();
}

static void draw_display(void) {
    glPushMatrix();
    glTranslatef(50, 20, 0);
    glScalef(DISPLAY_PIXEL_SIZE, DISPLAY_PIXEL_SIZE, 1);
    sim_display_draw();
    glPopMatrix();
}

static void window_draw(void) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    draw_background();
    draw_display_frame();

    const battery_status_t vbat_status = power_get_battery_status();
    draw_led(22.5f, 30, 0.07f, 0.8f, 0.15f, vbat_status == BATTERY_CHARGED);
    draw_led(22.5f, 60, 1.0f, 0.2f, 0.2f, vbat_status == BATTERY_CHARGING);
    draw_led(22.5f, 90, 0.95f, 0.95f, 0.95f, sim_led_get());

    draw_display();
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

static void callback_mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && x >= 50 && y >= 20 &&
        x < 50 + DISPLAY_PIXEL_SIZE * DISPLAY_WIDTH &&
        y < 20 + DISPLAY_PIXEL_SIZE * DISPLAY_HEIGHT) {
        trace("display clicked at x = %d, y = %d",
               (x - 50) / DISPLAY_PIXEL_SIZE, (y - 20) / DISPLAY_PIXEL_SIZE);
    }
}

static void callback_redisplay_timer(int arg) {
    glutTimerFunc((unsigned int) (1000.0 / DISPLAY_FPS + 0.5), callback_redisplay_timer, 0);
    glutPostRedisplay();
}

static void callback_close(void) {
    sim_sound_terminate();
}

void glut_init(void) {
    // double buffered, RGB display.
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("Game console simulator");
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glutDisplayFunc(callback_display);
    glutTimerFunc((unsigned int) (1000.0 / DISPLAY_FPS + 0.5), callback_redisplay_timer, 0);
    glutCloseFunc(callback_close);

    glutMouseFunc(callback_mouse);
}

#endif //SIMULATION_HEADLESS