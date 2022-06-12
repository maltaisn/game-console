
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
#include <boot/defs.h>

#include <sys/time.h>
#include <sys/power.h>

#include <core/input.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define INACTIVITY_COUNTDOWN_START (SYS_POWER_INACTIVE_COUNTDOWN_SLEEP - SYS_POWER_SLEEP_COUNTDOWN)
#define INACTIVITY_COUNTDOWN_DIM (SYS_POWER_INACTIVE_COUNTDOWN_DIM - SYS_POWER_SLEEP_COUNTDOWN)

// the input state, updated on every system tick.
uint8_t sys_input_state;

// the currently latched input state.
uint8_t sys_input_curr_state;
// the previously latched input state.
uint8_t sys_input_last_state;

// used interally for debouncing.
static uint8_t _state0;
static uint8_t _state1;

// not volatile since read modify write never occurs outside interrupt
static uint8_t _inactive_countdown;

ISR(PORTD_PORT_vect) {
    // this interrupt is triggered whenever the user presses a button.
    VPORTD.INTFLAGS = BUTTONS_ALL;
    if (_inactive_countdown == 0) {
        // sleep is currently scheduled, cancel it.
        sys_power_schedule_sleep_cancel();
    }
    _inactive_countdown = INACTIVITY_COUNTDOWN_START;
}

void sys_input_update_state(void) {
    // 2 levels debouncing: new value is most common value among last two and new.
    // this is probably overkill since the buttons don't even bounce...
    const uint8_t port = VPORTD.IN & BUTTONS_ALL;
    sys_input_state = (_state0 & port) |
                      (_state1 & port) |
                      (_state0 & _state1);
    _state1 = _state0;
    _state0 = port;
}

void sys_input_update_state_immediate(void) {
    const uint8_t port = VPORTD.IN & BUTTONS_ALL;
    sys_input_state = port;
    sys_input_curr_state = port;
    sys_input_last_state = port;
    _state0 = port;
    _state1 = port;
}

void sys_input_dim_if_inactive(void) {
    if (sys_power_is_sleep_enabled()) {
        sys_display_set_dimmed(_inactive_countdown <= INACTIVITY_COUNTDOWN_DIM);
    }
}

void sys_input_reset_inactivity(void) {
    _inactive_countdown = INACTIVITY_COUNTDOWN_START;
}

void sys_input_update_inactivity(void) {
    if (_inactive_countdown == 0) {
        sys_power_schedule_sleep(SLEEP_CAUSE_INACTIVE | SYS_SLEEP_SCHEDULE_ALLOW_WAKEUP |
                                 SYS_SLEEP_SCHEDULE_COUNTDOWN);
    } else {
        --_inactive_countdown;
    }
}

BOOTLOADER_NOINLINE
void sys_input_latch(void) {
    sys_input_last_state = sys_input_curr_state;
    sys_input_curr_state = sys_input_state;
}

#endif //BOOTLOADER

uint8_t sys_input_get_state(void) {
    return sys_input_curr_state;
}

uint8_t sys_input_get_last_state(void) {
    return sys_input_last_state;
}
