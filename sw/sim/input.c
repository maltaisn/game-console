
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

#include "sys/input.h"
#include "sys/time.h"

#include <GL/glut.h>
#include <stdbool.h>

#define SPECIAL_MASK 0x80000000

static uint8_t state;

uint8_t input_get_state(void) {
    return state;
}

void input_update_state(void) {
    // no-op, glut callbacks are used instead.
}

static uint8_t get_key_state_mask(unsigned int key) {
    switch (key) {
        case 'q':
            return BUTTON0;
        case GLUT_KEY_LEFT | SPECIAL_MASK:
        case 'a':
            return BUTTON1;
        case GLUT_KEY_UP | SPECIAL_MASK:
        case 'w':
            return BUTTON2;
        case GLUT_KEY_DOWN | SPECIAL_MASK:
        case 's':
            return BUTTON3;
        case 'e':
            return BUTTON4;
        case GLUT_KEY_RIGHT | SPECIAL_MASK:
        case 'd':
            return BUTTON5;
        default:
            return 0;
    }
}

void input_on_key_down(unsigned char key, int x, int y) {
    state |= get_key_state_mask(key);
}

void input_on_key_up(unsigned char key, int x, int y) {
    state &= ~get_key_state_mask(key);
}

void input_on_key_down_special(int key, int x, int y) {
    state |= get_key_state_mask(key | SPECIAL_MASK);
}

void input_on_key_up_special(int key, int x, int y) {
    state &= ~get_key_state_mask(key | SPECIAL_MASK);
}
