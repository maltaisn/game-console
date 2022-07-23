
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

#ifndef BOOT_DISPLAY_H
#define BOOT_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SYS_DISPLAY_GPIO_DISABLE = 0b00,
    SYS_DISPLAY_GPIO_INPUT = 0b01,
    SYS_DISPLAY_GPIO_OUTPUT_LO = 0b10,
    SYS_DISPLAY_GPIO_OUTPUT_HI = 0b11,
} sys_display_gpio_t;

/**
 * Initialize display for the first time.
 * Should only be called on power up.
 */
void sys_display_preinit(void);

/**
 * Initialize display. This resets the display and sets all registers through SPI commands.
 * The display RAM is initialized to zero but the buffer is NOT cleared.
 * The display is initially turned OFF and not inverted.
 */
void sys_display_init(void);

/**
 * Disable internal VDD regulator to put display to sleep.
 * Re-initializing the display will turn it back on via display reset.
 */
void sys_display_sleep(void);

/**
 * Turn the display on or off.
 */
void sys_display_set_enabled(bool enabled);

/**
 * Dim or undim the display, updating the effective contrast.
 * Note that dimmed status isn't restored upon wakeup from sleep.
 */
void sys_display_set_dimmed(bool dimmed);

/**
 * Set the display GPIO mode.
 */
void sys_display_set_gpio(sys_display_gpio_t mode);

/**
 * Start updating display with the first page.
 * The display buffer is NOT cleared beforehand.
 */
void sys_display_first_page(void);

/**
 * Flush display buffer and go to the next page.
 * The display buffer is NOT cleared afterwards.
 * If on the last page, this returns false, otherwise it returns true.
 */
bool sys_display_next_page(void);

/**
 * Returns the computed average display color.
 * This value is only computed on the next refresh after
 * `sys_power_should_compute_color()` returns true.
 */
uint8_t sys_display_get_average_color(void);

#endif //BOOT_DISPLAY_H
