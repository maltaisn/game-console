
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

#include <util/atomic.h>

#define BATTERY_BUFFER_SIZE 8

#ifdef BOOTLOADER

#include <boot/defs.h>
#include <boot/init.h>
#include <boot/power.h>
#include <boot/display.h>
#include <boot/sound.h>
#include <boot/input.h>

#include <sys/callback.h>
#include <sys/app.h>
#include <sys/display.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "core/led.h"

#define MUXPOS_CHARGE_STATUS ADC_MUXPOS_AIN7_gc
#define MUXPOS_VBAT_LEVEL ADC_MUXPOS_AIN6_gc

#define CALIB_LOAD_COEFF ((const int16_t*) (EEPROM_START + 0))  // 12 B total, Q5.11 coeffs.
#define CALIB_PERCENT_CORRECTION ((const int8_t*) (EEPROM_START + 12))  // 2 B total, int8 coeffs.
#define CALIB_DISCHARGE_CURVE ((const uint16_t*) (EEPROM_START + 14))  // 64 B total, uint16 values.
#define CALIB_DISCHARGE_POINTS 16
#define CALIB_DISCHARGE_POINT_DIST (256 / CALIB_DISCHARGE_POINTS)

// number of seconds between each battery percent updates.
#define BATTERY_PERCENT_UPDATE_PERIOD 10

typedef enum {
    STATE_DONE,
    STATE_STATUS,
    STATE_LEVEL,
} sampler_state_t;

#endif

enum {
    // sleep was scheduled and countdown timer is running.
    STATE_SLEEP_SCHEDULED = 1 << 0,
    // sleep was scheduled and device will be allowed to wakeup from sleep.
    STATE_SLEEP_ALLOW_WAKEUP = 1 << 1,
    // sleep is disabled globally
    STATE_SLEEP_DISABLE = 1 << 2,
    // the battery level monitor is waiting for the average display color value.
    STATE_COLOR_PENDING = 1 << 3,
    // the battery level monitor is ready to update the battery level.
    STATE_UPDATE_BATTERY_LEVEL = 1 << 4,
};

#ifdef BOOTLOADER

// Minimum and maximum ADC readings for each battery status.
// If outside of these intervals, status is unknown.
// VREF = 2.5V
static const uint8_t _BATTERY_STATUS_RANGES[] = {
        130, 210,   // no battery,  bat stat = 1.3 to 2.1 V
        4, 62,      // charging,    bat stat = 0.1 to 0.6 V
        211, 255,   // charged,     bat stat = 2.1 to 2.6 V
        0, 3,       // discharging, bat stat = 0 V
};

static volatile sampler_state_t _sampler_state;  // STATE_DONE at startup
static volatile uint8_t _sleep_countdown;
static uint8_t _battery_percent_buf[BATTERY_BUFFER_SIZE];
// the head is the index in the buffer where the next value will be written.
static uint8_t _battery_percent_buf_head;
static volatile uint8_t _battery_percent_buf_size;
// current averaged battery level, 0-255.
static uint8_t _battery_level;
// battery level update timer counter.
static uint8_t _battery_level_update;

volatile uint8_t sys_power_state;
volatile battery_status_t sys_power_battery_status;  // UNKNOWN at startup
volatile sleep_cause_t sys_power_sleep_cause;
volatile uint16_t sys_power_last_sample;

/** Find the battery status from a voltage measurement and return it. */
static battery_status_t find_battery_status(uint8_t res) {
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

static void reset_battery_state(void) {
    _battery_level = 0;
    _battery_percent_buf_size = 0;
    _battery_level_update = 0;
    _sampler_state = STATE_DONE;
}

ISR(ADC0_RESRDY_vect) {
    uint16_t res = ADC0.RES;
    const sampler_state_t state = _sampler_state;
    if (state == STATE_STATUS) {
        battery_status_t status = find_battery_status(res >> 8);
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
            reset_battery_state();
        }

    } else if (state == STATE_LEVEL) {
        VPORTF.OUT &= ~PIN6_bm;
        sys_power_last_sample = res;
        sys_power_state |= STATE_UPDATE_BATTERY_LEVEL;
        _sampler_state = STATE_DONE;
    }
}

ISR(RTC_PIT_vect) {
    // (called every second)
    RTC.PITINTFLAGS = RTC_PI_bm;

    sys_input_update_inactivity();

    uint8_t countdown = _sleep_countdown;
    uint8_t state = sys_power_state;
    if ((state & STATE_SLEEP_SCHEDULED) && countdown != 0) {
        _sleep_countdown = countdown - 1;
    }

    // indicate that a display color should be computed soon.
    sys_power_state = state | STATE_COLOR_PENDING;
}

void sys_power_start_sampling(void) {
    if (_sampler_state == STATE_DONE) {
        _sampler_state = STATE_STATUS;
        ADC0.MUXPOS = MUXPOS_CHARGE_STATUS;
        ADC0.COMMAND = ADC_STCONV_bm;
    }
}

void sys_power_end_sampling(void) {
    _sampler_state = STATE_DONE;
}

void sys_power_wait_for_sample(void) {
    while (_sampler_state != STATE_DONE);
}

/**
 * Using precalculated coefficients stored in EEPROM, estimate the current
 * load factor (0-255) from the display contrast and color (other factors are ignored).
 * A load factor of 0 is the highest load and 255 is the lowest.
 */
static uint8_t estimate_load(void) {
    uint8_t var[3];
    var[0] = sys_display_get_contrast();
    var[1] = sys_display_get_average_color();
    var[2] = 1;
    int16_t res = 0;
    const int16_t* coeffs = CALIB_LOAD_COEFF;
    for (uint8_t i = 0; i < 3; ++i) {
        for (uint8_t j = 0; j <= i; ++j) {
            int16_t coeff = *coeffs++;
            int24_t term_fix = (int24_t) ((uint16_t) var[i] * var[j]) * coeff;
            res += (int16_t) ((uint24_t) term_fix >> 8);
        }
    }
    if (res < 0) {
        return 255;
    }
    uint16_t ures = (uint16_t) res >> 3;
    if (ures > 255) {
        return 0;
    }
    return 255 - ures;
}

/**
 * Using predefined discharge voltage curve stored in EEPROM, estimate
 * the battery level (0-255) and return it.
 */
static uint8_t estimate_battery_level(uint8_t load) {
    uint16_t adc;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        adc = sys_power_last_sample;
    }

    uint16_t last_middle;
    const uint16_t* points = CALIB_DISCHARGE_CURVE;
    uint8_t level = 256 - CALIB_DISCHARGE_POINT_DIST;
    for (uint8_t i = 0; i < CALIB_DISCHARGE_POINTS; ++i) {
        uint16_t lower = *points++;
        uint16_t range = *points++;
        uint16_t middle = lower + (((uint24_t) range * load) >> 8);
        int16_t diff = (int16_t) (adc - middle);
        if (diff > 0) {
            if (i == 0) {
                // level over 100%
                return 255;
            }
            return ((uint16_t) diff * CALIB_DISCHARGE_POINT_DIST /
                    (uint16_t) (last_middle - middle)) + level;
        }
        last_middle = middle;
        level -= CALIB_DISCHARGE_POINT_DIST;
    }
    // level below 0%
    return 0;
}

/**
 * Performs a final linear adjustment on estimated battery level to correct the
 * obvious deviation of the previous calculation from the actual battery level.
 */
static uint8_t calculate_battery_percent(uint8_t level) {
    int16_t adjusted = level + ((int16_t) CALIB_PERCENT_CORRECTION[0] * level >> 8);
    adjusted += CALIB_PERCENT_CORRECTION[1];
    if (adjusted < 0) {
        return 0;
    }
    if (adjusted > 255) {
        return 255;
    }
    return adjusted;
}

/**
 * Add a new battery percentage to the cyclic buffer, overwriting the oldest one if full.
 * Also update the average battery percent value.
 */
static void push_new_battery_percent(uint8_t percent) {
    uint8_t head = _battery_percent_buf_head;
    uint8_t size = _battery_percent_buf_size;

    // store new value
    _battery_percent_buf[head] = percent;
    _battery_percent_buf_head = (uint8_t) (head + 1) % BATTERY_BUFFER_SIZE;
    if (size < BATTERY_BUFFER_SIZE) {
        ++size;
        _battery_percent_buf_size = size;
    }

    // calculate average percentage
    uint16_t percent_sum = 0;
    for (uint8_t i = 0; i < size; ++i) {
        percent_sum += _battery_percent_buf[head];
        head = (uint8_t) (head - 1) % BATTERY_BUFFER_SIZE;
    }
    _battery_level = percent_sum / size;
}

static void schedule_sleep_if_low_battery(uint8_t flags) {
    if (sys_power_battery_status == BATTERY_DISCHARGING && _battery_level == 0) {
        sys_power_schedule_sleep(SLEEP_CAUSE_LOW_POWER | flags);
        // prevent the screen from being dimmed in the meantime.
        sys_input_reset_inactivity();
        // disable sound output since display has been replaced with low battery warning anyway.
        sys_sound_set_output_enabled(false);
    }
}

void sys_power_update_battery_level(uint8_t flags) {
    // make sure display color and ADC sample are available first.
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        uint8_t state = sys_power_state;
        if (!(state & STATE_UPDATE_BATTERY_LEVEL)) {
            // battery is not discharging, level is unavailable.
            return;
        }
        sys_power_state = state & ~STATE_UPDATE_BATTERY_LEVEL;
    }

    // limit update frequency.
    if (_battery_level_update > 0) {
        --_battery_level_update;
        return;
    }
    _battery_level_update = BATTERY_PERCENT_UPDATE_PERIOD - 1;

    uint8_t load = estimate_load();
    uint8_t level = estimate_battery_level(load);
    uint8_t percent = calculate_battery_percent(level);

    push_new_battery_percent(percent);
    schedule_sleep_if_low_battery(flags);
}

bool sys_power_should_compute_display_color(void) {
    return (sys_power_state & STATE_COLOR_PENDING) != 0;
}

void sys_power_on_display_color_computed(void) {
    // At this point the battery voltage should approximatively reflect the load,
    // so start sampling the ADC to be able to determine the battery level.
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        sys_power_state &= ~STATE_COLOR_PENDING;
    }
    sys_power_start_sampling();
}

void sys_power_set_15v_reg_enabled(bool enabled) {
    sys_display_set_gpio(enabled ? SYS_DISPLAY_GPIO_OUTPUT_HI : SYS_DISPLAY_GPIO_OUTPUT_LO);
}

void sys_power_schedule_sleep(uint8_t flags) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        uint8_t state = sys_power_state;
        if (state & STATE_SLEEP_DISABLE) {
            return;
        }
        if (flags & SYS_SLEEP_SCHEDULE_COUNTDOWN) {
            if (state & STATE_SLEEP_SCHEDULED) {
                // already scheduled
                return;
            }
            state |= STATE_SLEEP_SCHEDULED;
            if (flags & SYS_SLEEP_SCHEDULE_ALLOW_WAKEUP) {
                state |= STATE_SLEEP_ALLOW_WAKEUP;
            }
            _sleep_countdown = SYS_POWER_SLEEP_COUNTDOWN;
            sys_power_sleep_cause = flags & 0x3;
            sys_power_state = state;
            if (sys_app_get_loaded_id() != SYS_APP_ID_NONE) {
                __callback_sleep_scheduled();
            }
            return;
        }
    }
    sys_power_enable_sleep();
}

void sys_power_schedule_sleep_cancel(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        sys_power_state &= ~(STATE_SLEEP_SCHEDULED | STATE_SLEEP_ALLOW_WAKEUP);
    }
    sys_power_sleep_cause = SLEEP_CAUSE_NONE;
}

BOOTLOADER_NOINLINE
bool sys_power_is_sleep_due(void) {
    return (sys_power_state & STATE_SLEEP_SCHEDULED) && _sleep_countdown == 0;
}

void sys_power_enable_sleep(void) {
    if (sys_app_get_loaded_id() != SYS_APP_ID_NONE) {
        // The callbacks are located in the app code section, so when no app is loaded,
        // they must not be called. The same applies to the other callbacks here.
        __callback_sleep();
    }

    // go to sleep
    sys_init_sleep();
    if (!(sys_power_state & STATE_SLEEP_ALLOW_WAKEUP)) {
        // by disabling interrupts, there will be no way to wakeup from sleep.
        cli();
    }
    sleep_cpu();

    // --> wake-up from sleep
    // reset power state because some time may have passed since device was put to sleep,
    // and we also want to force the computation of battery level.
    sys_power_state = 0;
    reset_battery_state();

    sys_init_wakeup();
    // note that sys_power_enable_sleep() may be called from sys_init_wakeup, but the risk of
    // recursion is limited because if this happens, device won't wakeup again (low battery).

    if (sys_app_get_loaded_id() != SYS_APP_ID_NONE) {
        __callback_wakeup();
    }
}

BOOTLOADER_NOINLINE
uint8_t sys_power_get_battery_percent(void) {
    // percent = ceil(level / 255 * 20) * 5 = ((level * 20 + 255) >> 8) * 5
    uint16_t level0 = ((uint16_t) _battery_level * (100 / BATTERY_PERCENT_GRANULARITY) + 255);
    return (uint8_t) (level0 >> 8) * BATTERY_PERCENT_GRANULARITY;
}

#endif //BOOTLOADER

void sys_power_set_sleep_enabled(bool enabled) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        if (enabled) {
            sys_power_state &= ~STATE_SLEEP_DISABLE;
        } else {
            sys_power_state |= STATE_SLEEP_DISABLE;
        }
    }
}

ALWAYS_INLINE
bool sys_power_is_sleep_enabled(void) {
    return (sys_power_state & STATE_SLEEP_DISABLE) == 0;
}

ALWAYS_INLINE
battery_status_t sys_power_get_battery_status(void) {
    return sys_power_battery_status;
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
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        return (uint16_t) (((uint32_t) sys_power_last_sample * 4545) >> 16) + 1;
    }
    return 0;
}

uint16_t sys_power_get_battery_last_reading(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        return sys_power_last_sample;
    }
    return 0;
}