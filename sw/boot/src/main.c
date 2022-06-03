/*
 * Copyright 2022 Nicolas Maltais
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

#include <load.h>

#include <boot/defs.h>
#include <boot/init.h>
#include <boot/input.h>
#include <boot/power.h>
#include <boot/display.h>
#include <boot/eeprom.h>

#include <sys/callback.h>
#include <sys/sound.h>
#include <sys/power.h>
#include <sys/display.h>
#include <sys/app.h>

#include <core/input.h>
#include <core/sound.h>
#include <core/sysui.h>
#include <core/graphics.h>
#include <core/time.h>

#include <stdbool.h>

#ifdef SIMULATION
#include <sim/glut.h>
#include <sim/uart.h>

#include <sys/eeprom.h>

#include <core/flash.h>
#include <core/eeprom.h>

#include <GL/freeglut.h>
#include <pthread.h>
#include <unistd.h>

#define SYSTICK_RATE (1.0 / SYSTICK_FREQUENCY)
#define POWER_MONITOR_RATE 1.0

#define SIM_FLASH_FILE "dev/flash.dat"
#define SIM_EEPROM_FILE "dev/eeprom.dat"

#endif //SIMULATION

#ifndef SIMULATION
FUSES = {
        .WDTCFG = FUSE_WDTCFG_DEFAULT,
        .BODCFG = BOD_LVL_BODLEVEL0_gc | BOD_SAMPFREQ_1KHZ_gc |
                  BOD_ACTIVE_SAMPLED_gc | BOD_SLEEP_DIS_gc,
        .OSCCFG = FUSE_OSCCFG_DEFAULT,
        .SYSCFG0 = FUSE_SYSCFG0_DEFAULT,
        .SYSCFG1 = FUSE_SYSCFG1_DEFAULT,
        .APPEND = 0x00,
        .BOOTEND = 0x20,  // 0x20 * 256 = 8192 B
};
#endif //SIMULATION

#define DISPLAY_MAX_FPS 8

#define APPS_PER_SCREEN 2
#define APP_ITEM_HEIGHT 58

#define ACTIVE_COLOR(cond) ((cond) ? 12 : 4)

// boot-arrow-down.png, 5x3, 1-bit mixed, unindexed.
static const uint8_t _ARROW_DOWN[] = {0xf1, 0x10, 0x04, 0x02, 0x7d, 0x62, 0x00};

// boot-arrow-up.png, 5x3, 1-bit mixed, unindexed.
static const uint8_t _ARROW_UP[] = {0xf1, 0x10, 0x04, 0x02, 0x11, 0x6f, 0x40};

static BOOTLOADER_ONLY uint8_t _first_shown;
static BOOTLOADER_ONLY uint8_t _selected_index;
static BOOTLOADER_ONLY systime_t _last_draw_time;
static BOOTLOADER_ONLY uint8_t _last_input_state;

static void loop(void);
static void draw(void);
static void handle_input(void);

#ifdef SIMULATION
static void* loop_thread(void* arg) {
    while (true) {
        loop();

#ifdef SYS_UART_ENABLE
        sim_uart_listen();
#endif

        // 1 ms sleep (fixes responsiveness issues with keyboard input)
        sim_time_sleep(1000);
    }
    return 0;
}
#endif //SIMULATION

/**
 * This function is the entry point for both the bootloader and the simulator.
 * The main loop takes care of calling `loop` and `draw` callbacks among other things.
 */
int main(void) {
    sys_init();
    sys_display_init_page(DISPLAY_PAGE_HEIGHT);

    // initialize last input state to prevent pushed button registered as clicked
    // immediately on launch, like when an app has an exit button.
    _last_input_state = input_get_state();

#ifdef SIM_MEMORY_ABSOLUTE
#ifdef SIMULATION
    // for bootloader, load flash and eeprom from local files written by gcprog --local.
#if APP_ID == APP_ID_NONE
    sim_flash_load("../" SIM_FLASH_FILE);
    sim_eeprom_load("../" SIM_EEPROM_FILE);
#else
    sim_flash_load("../../" SIM_FLASH_FILE);
    sim_eeprom_load("../../" SIM_EEPROM_FILE);
#endif
#endif //SIMULATION
    sys_eeprom_check_write();
    load_read_index();
    _selected_index = load_get_loaded_app_index();
    if (_selected_index == LOADED_APP_NONE) {
        // no app is currently loaded, select first by default.
        _selected_index = 0;
    }
#endif //SIM_MEMORY_ABSOLUTE

#ifdef SIMULATION
    int argc = 0;
    glutInit(&argc, 0);
    glut_init();
    sim_input_init();

    // normally called when loading an app, but we never load apps in simulation.
    __callback_setup();

    // run the app in a separate thread. The reason this is done instead of calling loop() from
    // a GLUT timer is that when sleep is enabled, execution is expected to stop at that point
    // until wakeup.
    pthread_t thread;
    pthread_create(&thread, NULL, loop_thread, NULL);

    while (true) {
        glutMainLoopEvent();
        sim_time_update();
    }
#else
    while (true) {
        loop();
    }
#endif //SIMULATION
}

static void loop(void) {
    sys_sound_fill_track_buffers();
    sys_input_dim_if_inactive();

    bool is_sleep_due = sys_power_is_sleep_due();

    bool should_draw;
    if (sys_app_get_loaded_id() != SYS_APP_ID_NONE) {
        // app active
        should_draw = __callback_loop();
    } else {
        // bootloader active
        handle_input();

        if (sys_app_get_loaded_id() != SYS_APP_ID_NONE) {
            // app was just loaded, start loop over from app perspective.
            return;
        }

        systime_t time = time_get();
        should_draw = (time - _last_draw_time > millis_to_ticks(1000.0 / DISPLAY_MAX_FPS));
        if (should_draw) {
            _last_draw_time = time;
        }
    }

    if (is_sleep_due) {
        // if sleep was scheduled and is due, go to sleep.
        // loop() callback will have been called once with sys_power_is_sleep_due()
        // returning true so that any last minute special actions can be taken.
        sys_power_enable_sleep();
    }

    if (should_draw) {
        sys_display_first_page();
        do {
            draw();
        } while (sys_display_next_page());
    }
}

static void draw_low_battery_overlay(void) {
    graphics_clear(DISPLAY_COLOR_BLACK);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    graphics_set_font(ASSET_FONT_3X5_BUILTIN);
    graphics_text(30, 42, "LOW BATTERY LEVEL");
    graphics_text(33, 81, "SHUTTING DOWN...");
    graphics_set_color(11);
    graphics_rect(40, 52, 43, 24);
    graphics_rect(41, 53, 41, 22);
    graphics_fill_rect(84, 57, 4, 14);
    graphics_fill_rect(44, 56, 7, 16);
    graphics_set_color(DISPLAY_COLOR_BLACK);
    graphics_fill_rect(84, 59, 2, 10);
}

static void draw(void) {
    if (power_get_scheduled_sleep_cause() == SLEEP_CAUSE_LOW_POWER) {
        // show low battery screen before sleeping.
        draw_low_battery_overlay();
        return;
    }

    if (sys_app_get_loaded_id() != SYS_APP_ID_NONE) {
        // draw the app
        __callback_draw();
    } else {
        // draw the bootloader
        graphics_clear(DISPLAY_COLOR_BLACK);

        // draw the app list
        disp_y_t y = 5;
        uint8_t index =  _first_shown;
        for (uint8_t i = 0; i < APPS_PER_SCREEN; ++i) {
            if (index == load_get_app_count()) {
                break;
            }

            graphics_image_t image = load_get_app_image(index);
            graphics_set_color(ACTIVE_COLOR(index == _selected_index));
            graphics_rect(0, y, DISPLAY_WIDTH, APP_ITEM_HEIGHT);
            graphics_image_4bit_mixed(image, 2, y + 2);
            y += APP_ITEM_HEIGHT + 2;

            ++index;
        }

        // draw movement arrows
        graphics_set_color(ACTIVE_COLOR(_first_shown > 0));
        graphics_image_1bit_mixed(data_mcu(_ARROW_UP), 62, 0);
        graphics_set_color(ACTIVE_COLOR((int8_t) _first_shown <
                                        (int8_t) load_get_app_count() - 2));
        graphics_image_1bit_mixed(data_mcu(_ARROW_DOWN), 62, 125);

        sysui_battery_overlay();
    }
}

static void handle_input(void) {
    const uint8_t state = input_get_state();
    const uint8_t clicked = state & ~_last_input_state;
    _last_input_state = state;

    if (clicked & BUTTON2) {
        // move selection up
        if (_selected_index != 0) {
            --_selected_index;
            if (_first_shown > _selected_index) {
                --_first_shown;
            }
        }
    } else if (clicked & BUTTON3) {
        // move selection down
        if (_selected_index < load_get_app_count() - 1) {
            ++_selected_index;
            if (_first_shown + APPS_PER_SCREEN <= _selected_index) {
                ++_first_shown;
            }
        }
    } else if (clicked & BUTTON4) {
        // load selected app
        _load_app(_selected_index);
    }
}

#endif //SIMULATION_HEADLESS
