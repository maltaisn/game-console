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

#include <boot/defs.h>
#include <boot/init.h>
#include <boot/input.h>
#include <boot/power.h>
#include <boot/display.h>
#include <boot/eeprom.h>

#include <sys/callback.h>
#include <sys/sound.h>
#include <sys/power.h>

#include <core/input.h>
#include <core/sound.h>
#include <core/sysui.h>
#include <core/graphics.h>
#include <core/time.h>
#include <core/flash.h>
#include <core/eeprom.h>

#include <load.h>

#include <stdbool.h>

#ifdef SIMULATION
#include <GL/glut.h>
#include <sim/glut.h>
#include <pthread.h>
#include <unistd.h>
#endif

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

        // 1 ms sleep (fixes responsiveness issues with keyboard input)
        sim_time_sleep(1000);
    }
    return 0;
}
#endif

/**
 * This function is the entry point for both the bootloader and the simulator.
 * The main loop takes care of calling `loop` and `draw` callbacks among other things.
 */
int main(void) {
#ifdef SIMULATION
    sim_time_init();
    sim_sound_init();
#endif

    sys_init();
    sys_display_init_page(DISPLAY_PAGE_HEIGHT);

#if defined(SIMULATION) && APP_ID == APP_ID_NONE
    // for bootloader, load flash and eeprom from local files used by gcprog.
    sim_flash_load_file("../dev/flash.dat");
    sim_eeprom_load_file("../dev/flash.dat");
#endif

    sys_eeprom_check_write();

    load_read_index();

#ifdef SIMULATION
    sim_time_init();
    sim_sound_init();

    int argc = 0;
    glutInit(&argc, 0);
    glut_init();
    sim_input_init();

    // this part is equivalent to what the bootloader does.
    __callback_setup();

    pthread_t thread;
    pthread_create(&thread, NULL, loop_thread, NULL);

    glutMainLoop();
#else
    if (load_get_app_count() == 0) {
        // either flash wasn't initialized, or bootloader was just updated and all the apps
        // target the old bootloader version... in this case we'll suppose that the debug app
        // was bundled along with the bootloader and we can just jump to it directly.
        // note that this means the debug app must partially initialize itself (display page height),
        // and may not store any data in flash and eeprom since the offset hasn't been set.
        __callback_setup();
    }

    while (true) {
        loop();
    }
#endif
}

static void loop(void) {
    sys_sound_fill_track_buffers();
    sys_input_dim_if_inactive();

    bool is_sleep_due = sys_power_is_sleep_due();
    bool should_draw = false;

    if (load_get_loaded() != APP_ID_NONE) {
        // app active
        should_draw = __callback_loop();
    } else {
        // bootloader active
        handle_input();

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
    graphics_set_font(GRAPHICS_BUILTIN_FONT);
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

    if (load_get_loaded() != APP_ID_NONE) {
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
        if (load_get_app_count() > 0) {
            // load selected app
            _load_app(_selected_index);
        }
    }

    _last_input_state = state;
}

#endif //SIMULATION_HEADLESS