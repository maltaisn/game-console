
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

#include <sys/power.h>
#include <sys/defs.h>

#ifdef BOOTLOADER

#include <boot/defs.h>
#include <boot/init.h>
#include <boot/power.h>
#include <boot/display.h>
#include <boot/sound.h>
#include <boot/input.h>

#include <sys/callback.h>

#include <core/defs.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/atomic.h>

#define MUXPOS_CHARGE_STATUS ADC_MUXPOS_AIN7_gc
#define MUXPOS_VBAT_LEVEL ADC_MUXPOS_AIN6_gc

#define BATTERY_BUFFER_SIZE 8
#define BATTERY_BUFFER_SIZE_LOG2 3
#define BATTERY_BUFFER_HEAD_EMPTY 0xff

enum {
    STATE_15V_ENABLED = 1 << 0,
    STATE_SLEEP_SCHEDULED = 1 << 1,
    STATE_SLEEP_ALLOW_WAKEUP = 1 << 2,
};

typedef enum {
    STATE_DONE,
    STATE_STATUS,
    STATE_LEVEL,
} sampler_state_t;

// Minimum and maximum ADC readings for each battery status.
// If outside of these intervals, status is unknown.
// VREF = 2.5V
static const uint8_t _BATTERY_STATUS_RANGES[] = {
        130, 210,   // no battery,  bat stat = 1.3 to 2.1 V
        4, 62,      // charging,    bat stat = 0.1 to 0.6 V
        211, 255,   // charged,     bat stat = 2.1 to 2.6 V
        0, 3,       // discharging, bat stat = 0 V
};

// Pre-calculated reference points to estimate battery charge left
// from battery voltage. Interpolated from test discharge curve.
// 0% corresponds to 3.3V and 100% to 4.05V.
// Battery percentage is linearly interpolated from these points.
// TODO: numbers for 250 mA load, to be recalculated.
static const uint16_t _BATTERY_LEVEL_POINTS[] = {
        47579, 49598, 50895, 52481, 53683, 54259, 54788, 55509, 56278, 56999, 58393,
};

static volatile uint8_t _power_state;
static volatile sampler_state_t _sampler_state;  // STATE_DONE at startup
static uint16_t _battery_level_buf[BATTERY_BUFFER_SIZE];
static uint8_t _battery_level_head = BATTERY_BUFFER_HEAD_EMPTY;
static uint8_t _sleep_countdown;

volatile battery_status_t sys_power_battery_status;  // UNKNOWN at startup
volatile uint8_t sys_power_battery_percent;
volatile sleep_cause_t sys_power_sleep_cause;

/** Find the battery status from a voltage measurement and return it. */
static battery_status_t get_battery_status(uint8_t res) {
    // return battery status according to precalculated ranges.
    // if none match, battery status is unknown.
    const uint8_t* ranges = _BATTERY_STATUS_RANGES;
    for (uint8_t status = BATTERY_NONE; status <= (uint8_t) BATTERY_DISCHARGING; ++status) {
        uint8_t min = *ranges++;
        uint8_t max = *ranges++;
        if (res >= min && res <= max) {
            return status;
        }
    }
    return BATTERY_UNKNOWN;
}

/** Calculate and return battery percent from measurements in buffer,
 *  and predetermined voltage levels. */
static uint8_t get_battery_percent(uint16_t res) {
    // push new battery level to buffer
    const uint8_t head = _battery_level_head;
    if (head == BATTERY_BUFFER_HEAD_EMPTY) {
        // buffer is empty, fill it with first sample
        _battery_level_head = 0;
        for (uint8_t i = 0; i < BATTERY_BUFFER_SIZE; ++i) {
            _battery_level_buf[i] = res;
        }
    } else {
        _battery_level_buf[head] = res;
        _battery_level_head = (head + 1) % BATTERY_BUFFER_SIZE;
    }

    // linearly interpolate battery percentage from precalculated points.
    const uint16_t level = sys_power_get_battery_level_average();
    uint16_t left = _BATTERY_LEVEL_POINTS[0];
    if (level < left) {
        // battery level <0%
        return 0;
    }
    for (uint8_t i = 1; i < 11; ++i) {
        uint16_t right = _BATTERY_LEVEL_POINTS[i];
        if (level < right) {
            return 10 * (i - 1) + ((level - left) * 10 + 5) / (right - left);
        }
        left = right;
    }
    return 100;
}

ISR(ADC0_RESRDY_vect) {
    uint16_t res = ADC0.RES;
    const sampler_state_t state = _sampler_state;
    if (state == STATE_STATUS) {
        battery_status_t status = get_battery_status(res >> 8);
        sys_power_battery_status = status;
        if (status == BATTERY_DISCHARGING) {
            // start conversion for battery level.
            VPORTF.OUT |= PIN6_bm;  // enable switch for reading
            ADC0.MUXPOS = MUXPOS_VBAT_LEVEL;
            ADC0.COMMAND = ADC_STCONV_bm;
            _sampler_state = STATE_LEVEL;
        } else {
            // no battery, or vbat is sourced from vbus,
            // in which case we can't know the battery voltage.
            sys_power_battery_percent = 0;
            _battery_level_head = BATTERY_BUFFER_HEAD_EMPTY;
            _sampler_state = STATE_DONE;
        }

    } else if (state == STATE_LEVEL) {
        VPORTF.OUT &= ~PIN6_bm;
        sys_power_battery_percent = get_battery_percent(res);
        _sampler_state = STATE_DONE;
    }
}

ISR(RTC_PIT_vect) {
    // called every second
    RTC.PITINTFLAGS = RTC_PI_bm;

    sys_input_update_inactivity();

    if ((_power_state & STATE_SLEEP_SCHEDULED) && _sleep_countdown != 0) {
        --_sleep_countdown;
    }

    sys_power_start_sampling();
    sys_power_schedule_sleep_if_low_battery(true);
}

void sys_power_start_sampling(void) {
    if (_sampler_state == STATE_DONE) {
        _sampler_state = STATE_STATUS;
        ADC0.MUXPOS = MUXPOS_CHARGE_STATUS;
        ADC0.COMMAND = ADC_STCONV_bm;
    }
}

void sys_power_end_sampling(void) {
    if (_sampler_state != STATE_DONE) {
        _sampler_state = STATE_DONE;
    }
}

void sys_power_wait_for_sample(void) {
    while (_sampler_state != STATE_DONE);
}

BOOTLOADER_NOINLINE
uint16_t sys_power_get_battery_level_average(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        uint24_t sum = 0;
        for (uint8_t i = 0; i < BATTERY_BUFFER_SIZE; ++i) {
            sum += (uint24_t) _battery_level_buf[i];
        }
        return sum >> BATTERY_BUFFER_SIZE_LOG2;
    }
    return 0;
}

bool sys_power_is_15v_reg_enabled(void) {
    return (_power_state & STATE_15V_ENABLED) != 0;
}

void sys_power_set_15v_reg_enabled(bool enabled) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        if (enabled) {
            _power_state |= STATE_15V_ENABLED;
        } else {
            _power_state &= ~STATE_15V_ENABLED;
        }
    }
    sys_display_set_gpio(enabled ? SYS_DISPLAY_GPIO_OUTPUT_HI : SYS_DISPLAY_GPIO_OUTPUT_LO);
}

void sys_power_schedule_sleep(sleep_cause_t cause, bool allow_wakeup, bool countdown) {
    if (countdown) {
        ATOMIC_BLOCK(ATOMIC_FORCEON) {
            uint8_t state = _power_state;
            if (state & STATE_SLEEP_SCHEDULED) {
                // already scheduled
                return;
            }
            state |= STATE_SLEEP_SCHEDULED;
            if (allow_wakeup) {
                state |= STATE_SLEEP_ALLOW_WAKEUP;
            }
            _sleep_countdown = SYS_POWER_SLEEP_COUNTDOWN;
            sys_power_sleep_cause = cause;
            _power_state = state;
            __callback_sleep_scheduled();
        }
        return;
    }
    sys_power_enable_sleep();
}

void sys_power_schedule_sleep_if_low_battery(bool countdown) {
    if (sys_power_battery_status == BATTERY_DISCHARGING && power_get_battery_percent() == 0) {
        sys_power_schedule_sleep(SLEEP_CAUSE_LOW_POWER, false, countdown);
        // prevent the screen from being dimmed in the meantime.
        sys_input_reset_inactivity();
        // disable sound output since display has been replaced with low battery warning anyway.
        sys_sound_set_output_enabled(false);
    }
}

void sys_power_schedule_sleep_cancel(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        _power_state &= ~(STATE_SLEEP_SCHEDULED | STATE_SLEEP_ALLOW_WAKEUP);
    }
    sys_power_sleep_cause = SLEEP_CAUSE_NONE;
}

BOOTLOADER_NOINLINE
bool sys_power_is_sleep_due(void) {
    return (_power_state & STATE_SLEEP_SCHEDULED) && _sleep_countdown == 0;
}

void sys_power_enable_sleep(void) {
    __callback_sleep();

    // go to sleep
    sys_init_sleep();
    if (!(_power_state & STATE_SLEEP_ALLOW_WAKEUP)) {
        cli();
    }
    sys_power_schedule_sleep_cancel();
    sleep_cpu();

    // --> wake-up from sleep
    // reset power state because some time may have passed since device was put to sleep.
    sys_power_battery_status = BATTERY_UNKNOWN;
    _battery_level_head = BATTERY_BUFFER_HEAD_EMPTY;
    sys_init_wakeup();
    __callback_wakeup();
}

#endif //BOOTLOADER

ALWAYS_INLINE
battery_status_t sys_power_get_battery_status(void) {
    return sys_power_battery_status;
}

ALWAYS_INLINE
uint8_t sys_power_get_battery_percent(void) {
    return sys_power_battery_percent;
}

ALWAYS_INLINE
sleep_cause_t sys_power_get_scheduled_sleep_cause(void) {
    return sys_power_sleep_cause;
}

uint16_t sys_power_get_battery_voltage(void) {
    // <battery voltage mV> = <battery level> / 65535 * VREF / R11 * (R10 + R11) * 1000
    //                      = <battery level> * 6.93592e-02
    //                      = <battery level> * 4545.52 / 65536
    //                      = (<battery level> * 4545) // 65536 + 1 (considering level is around 53000)
    return (((uint32_t) sys_power_get_battery_level_average() * 4545) >> 16) + 1;
}
