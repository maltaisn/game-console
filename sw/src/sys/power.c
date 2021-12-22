
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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/atomic.h>

#define MUXPOS_CHARGE_STATUS ADC_MUXPOS_AIN7_gc
#define MUXPOS_VBAT_LEVEL ADC_MUXPOS_AIN6_gc

#define BATTERY_LEVEL_UNKNOWN 0

typedef enum sampler_state {
    STATE_DONE,
    STATE_STATUS,
    STATE_LEVEL,
} sampler_state_t;

// Minimum and maximum ADC readings for each battery status.
// If outside of these intervals, status is unknown.
// VREF = 2.5V
static const uint8_t BATTERY_STATUS_RANGES[] = {
        166, 210,   // no battery,  bat stat = 1.6 to 2.1 V
        4, 62,      // charging,    bat stat = 0.1 to 0.6 V
        211, 255,   // charged,     bat stat = 2.1 to 2.6 V
        0, 3,       // discharging, bat stat = 0 V
};

// Pre-calculated reference points to estimate battery charge left
// from battery voltage. Interpolated from test discharge curve.
// 0% corresponds to 3.3V and 100% to 4.05V.
// Battery percentage is linearly interpolated from these points.
// TODO: numbers for 250 mA load, to be recalculated.
static const uint16_t BATTERY_LEVEL_POINTS[] = {
        47579, 49598, 50895, 52481, 53683, 54259, 54788, 55509, 56278, 56999, 58393,
};

volatile sampler_state_t sampler_state;
volatile battery_status_t battery_status;

// battery level is split in two variables because sometimes we don't need to load
// both and being volatile, uint16_t would produce unoptimal results.
// if battery level is unknown, only battery_levelh is BATTERY_LEVEL_UNKNOWN.
volatile uint8_t battery_levell;
volatile uint8_t battery_levelh;

ISR(ADC0_RESRDY_vect) {
    uint16_t res = ADC0.RES;
    sampler_state_t state = sampler_state;
    if (state == STATE_STATUS) {
        // set battery status according to precalculated ranges.
        // if none match, battery status is unknown.
        res >>= 8;
        const uint8_t* ranges = BATTERY_STATUS_RANGES;
        battery_status_t new_status = BATTERY_UNKNOWN;
        for (uint8_t status = BATTERY_NONE ; status <= (uint8_t) BATTERY_DISCHARGING ; ++status) {
            uint8_t min = *ranges++;
            uint8_t max = *ranges++;
            if (res >= min && res <= max) {
                new_status = status;
                break;
            }
        }

        if (new_status == BATTERY_DISCHARGING) {
            // start conversion for battery level.
            sampler_state = STATE_LEVEL;
            PORTF.OUT |= PIN6_bm;  // enable switch for reading
            ADC0.MUXPOS = MUXPOS_VBAT_LEVEL;
            ADC0.COMMAND = ADC_STCONV_bm;
        } else {
            // no battery, or vbat is sourced from vbus,
            // in which case we can't know the battery voltage.
            sampler_state = STATE_DONE;
            battery_levelh = BATTERY_LEVEL_UNKNOWN;
        }
        battery_status = new_status;

    } else if (state == STATE_LEVEL) {
        PORTF.OUT &= ~PIN6_bm;
        battery_levell = res & 0xff;
        battery_levelh = res >> 8;
        sampler_state = STATE_DONE;
    }
}

void power_take_sample(void) {
    if (sampler_state == STATE_DONE) {
        sampler_state = STATE_STATUS;
        ADC0.MUXPOS = MUXPOS_CHARGE_STATUS;
        ADC0.COMMAND = ADC_STCONV_bm;
    }
}

void power_wait_for_sample(void) {
    while (battery_levelh == BATTERY_PERCENT_UNKNOWN);
}

battery_status_t power_get_battery_status(void) {
    return battery_status;
}

uint8_t power_get_battery_percent(void) {
    uint8_t levell;
    uint8_t levelh;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        levell = battery_levell;
        levelh = battery_levelh;
    }
    if (levelh == BATTERY_LEVEL_UNKNOWN) {
        return BATTERY_PERCENT_UNKNOWN;
    }
    uint16_t level = levelh << 8 | levell;

    // linearly interpolate battery percentage from precalculated points.
    uint16_t left = BATTERY_LEVEL_POINTS[0];
    if (level < left) {
        // battery level <0%
        return 0;
    }
    for (uint8_t i = 1 ; i < 11 ; ++i) {
        uint16_t right = BATTERY_LEVEL_POINTS[i];
        if (level < right) {
            return 10 * (i - 1) + ((level - left) * 10 + 5) / (right - left);
        }
        left = right;
    }
    // battery level >= 100%
    return 100;
}

uint16_t power_get_battery_voltage(void) {
    // <battery voltage> = <battery level> / 65535 * VREF / R11 * (R10 + R11)
    //                  = <battery level> * 6.936e-05
    uint8_t level = battery_levelh;
    if (level == BATTERY_LEVEL_UNKNOWN) {
        return 0;
    }
    return level * 18;
}

void sleep_if_low_battery(void) {
    if (power_get_battery_percent() == 0) {
        // battery is too low, put CPU to sleep
        // interrupts are disabled, only reset will wake up.
        cli();
        sleep_enable();
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_cpu();
    }
}
