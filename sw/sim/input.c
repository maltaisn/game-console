
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

#include <sys/input.h>
#include <sys/time.h>
#include <sys/display.h>
#include <sys/power.h>

#include <sim/power.h>

#include <core/trace.h>

#include <GL/glut.h>
#include <stdbool.h>

#define SPECIAL_MASK 0x80000000

#define INACTIVITY_COUNTDOWN_START (POWER_INACTIVE_COUNTDOWN_SLEEP - POWER_SLEEP_COUNTDOWN)
#define INACTIVITY_COUNTDOWN_DIM (POWER_INACTIVE_COUNTDOWN_DIM - POWER_SLEEP_COUNTDOWN)

static uint8_t state;
static uint8_t inactive_countdown;

static void reset_inactive_countdown(void) {
    if (inactive_countdown <= INACTIVITY_COUNTDOWN_DIM) {
        // screen was dimmed, reset contrast.
        display_set_dimmed(false);
    }
    inactive_countdown = INACTIVITY_COUNTDOWN_START;
    power_schedule_sleep_cancel();
}

static uint8_t get_key_state_mask(unsigned int key) {
    uint8_t mask;
    switch (key) {
        case 'q':
            mask = BUTTON0;
            break;
        case GLUT_KEY_LEFT | SPECIAL_MASK:
        case 'a':
            mask = BUTTON1;
            break;
        case GLUT_KEY_UP | SPECIAL_MASK:
        case 'w':
            mask = BUTTON2;
            break;
        case GLUT_KEY_DOWN | SPECIAL_MASK:
        case 's':
            mask = BUTTON3;
            break;
        case 'e':
            mask = BUTTON4;
            break;
        case GLUT_KEY_RIGHT | SPECIAL_MASK:
        case 'd':
            mask = BUTTON5;
            break;
        default:
            return 0;
    }
    reset_inactive_countdown();
    power_disable_sleep();
    return mask;
}

static void input_on_key_down(unsigned char key, int x, int y) {
    state |= get_key_state_mask(key);
}

static void input_on_key_up(unsigned char key, int x, int y) {
    state &= ~get_key_state_mask(key);
}

static void input_on_key_down_special(int key, int x, int y) {
    state |= get_key_state_mask(key | SPECIAL_MASK);
}

static void input_on_key_up_special(int key, int x, int y) {
    state &= ~get_key_state_mask(key | SPECIAL_MASK);
}

void input_init(void) {
    glutKeyboardFunc(input_on_key_down);
    glutKeyboardUpFunc(input_on_key_up);
    glutSpecialFunc(input_on_key_down_special);
    glutSpecialUpFunc(input_on_key_up_special);
    glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
}

uint8_t input_get_state(void) {
    return state;
}

void input_update_state(void) {
    // no-op, glut callbacks are used instead.
}

void input_reset_inactivity(void) {
#ifndef DISABLE_INACTIVE_SLEEP
    inactive_countdown = INACTIVITY_COUNTDOWN_START;
    display_set_dimmed(false);
#endif
}

void input_update_inactivity(void) {
#ifndef DISABLE_INACTIVE_SLEEP
    if (inactive_countdown == 0) {
        power_schedule_sleep(SLEEP_CAUSE_INACTIVE, true, true);
    } else {
        --inactive_countdown;
        if (inactive_countdown == INACTIVITY_COUNTDOWN_DIM) {
            display_set_dimmed(true);
            trace("input inactive, display dimmed");
        }
    }
#endif
}
