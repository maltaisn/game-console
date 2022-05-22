
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
#include <sys/defs.h>

#ifdef BOOTLOADER

#include <boot/display.h>
#include <boot/power.h>

#include <sys/time.h>
#include <sys/power.h>

#include <core/input.h>

#include <avr/io.h>
#include <avr/interrupt.h>

// input state update frequency in Hz.
#define UPDATE_FREQUENCY 64
#define UPDATE_PERIOD (SYSTICK_FREQUENCY / UPDATE_FREQUENCY)

#define INACTIVITY_COUNTDOWN_START (SYS_POWER_INACTIVE_COUNTDOWN_SLEEP - SYS_POWER_SLEEP_COUNTDOWN)
#define INACTIVITY_COUNTDOWN_DIM (SYS_POWER_INACTIVE_COUNTDOWN_DIM - SYS_POWER_SLEEP_COUNTDOWN)

volatile uint8_t sys_input_state;

static uint8_t _state0;
static uint8_t _state1;
static uint8_t _update_register;
static uint8_t _inactive_countdown;

ISR(PORTD_PORT_vect) {
    VPORTD.INTFLAGS = BUTTONS_ALL;
    if (_inactive_countdown == 0) {
        sys_power_schedule_sleep_cancel();
    }
    _inactive_countdown = INACTIVITY_COUNTDOWN_START;
}

void sys_input_update_state(void) {
    // 2 levels debouncing: new value is most common value among last two and new.
    // this is probably overkill since the buttons don't even bounce...
    if (_update_register == 0) {
        const uint8_t port = VPORTD.IN & BUTTONS_ALL;
        sys_input_state = (_state0 & port) |
                          (_state1 & port) |
                          (_state0 & _state1);
        _state1 = _state0;
        _state0 = port;
        _update_register = UPDATE_PERIOD - 1;
    } else {
        --_update_register;
    }
}

void sys_input_update_state_immediate(void) {
    const uint8_t port = VPORTD.IN & BUTTONS_ALL;
    sys_input_state = port;
    _state0 = port;
    _state1 = port;
}

void sys_input_dim_if_inactive(void) {
    sys_display_set_dimmed(_inactive_countdown <= INACTIVITY_COUNTDOWN_DIM);
}

void sys_input_reset_inactivity(void) {
    _inactive_countdown = INACTIVITY_COUNTDOWN_START;
}

void sys_input_update_inactivity(void) {
    if (_inactive_countdown == 0) {
        sys_power_schedule_sleep(SLEEP_CAUSE_INACTIVE, true, true);
    } else {
        --_inactive_countdown;
    }
}

#endif //BOOTLOADER

ALWAYS_INLINE
uint8_t sys_input_get_state(void) {
    return sys_input_state;
}
