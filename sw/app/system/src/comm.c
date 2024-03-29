
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

#include <comm.h>
#include <assets.h>
#include <system.h>

#include <sys/uart.h>
#include <sys/spi.h>
#include <sys/power.h>
#include <sys/display.h>

#include <core/defs.h>
#include <core/time.h>
#include <core/led.h>
#include <core/graphics.h>
#include <core/sound.h>
#include <core/display.h>

#ifdef SIMULATION
#include <sim/uart.h>
#include <sim/flash.h>
#include <sim/eeprom.h>
#endif

#define LOCK_BLINK_DURATION 250  // ms

enum {
    SPI_CS_FLASH = 0x0,
    SPI_CS_EEPROM = 0x1,
    SPI_CS_DISPLAY = 0x2,
};

// When a packet is being received, the comm_receive function will block until the packet has been
// fully received. Hence, the payload buffer can share memory with the display buffer.
static SHARED_DISP_BUF uint8_t payload[PAYLOAD_MAX_SIZE];

static bool locked;

static void comm_transmit(uint8_t type, uint8_t payload_length) {
    sys_uart_write(PACKET_SIGNATURE);
    sys_uart_write(type);
    sys_uart_write(payload_length + PACKET_HEADER_SIZE - 1);
    const uint8_t* ptr = payload;
    while (payload_length--) {
        sys_uart_write(*ptr++);
    }
}

static void handle_packet_version(void) {
    payload[0] = APP_VERSION & 0xff;
    payload[1] = APP_VERSION >> 8;
    payload[2] = BOOT_VERSION & 0xff;
    payload[3] = BOOT_VERSION >> 8;
    payload[4] = VERSION_PROG_COMP & 0xff;
    payload[5] = VERSION_PROG_COMP >> 8;
    comm_transmit(PACKET_VERSION, 6);
}

static void handle_packet_spi(uint8_t data_length) {
    // assert CS line for the selected peripheral
    const uint8_t options = payload[0];
    const uint8_t cs = options & 0x3;
    if (cs == SPI_CS_FLASH) {
        sys_spi_select_flash();
        state.flags |= SYSTEM_FLAG_FLASH_DIRTY;
    } else if (cs == SPI_CS_EEPROM) {
        sys_spi_select_eeprom();
        state.flags |= SYSTEM_FLAG_EEPROM_DIRTY;
    } else if (cs == SPI_CS_DISPLAY) {
        sys_spi_select_display();
    }

    // transceive SPI data
    sys_spi_transceive(data_length - 1, payload + 1);
    comm_transmit(PACKET_SPI, data_length);

    // if last transfer, deassert the CS line.
    if (options & 0x80) {
        sys_spi_deselect_all();
    }
}

static void handle_packet_lock(void) {
    if (payload[0] == 0xff) {
        locked = true;
    } else if (payload[0] == 0x00) {
        locked = false;
    }
    comm_transmit(PACKET_LOCK, 0);
}

static void handle_packet_sleep(void) {
    if (payload[0] == 0x00) {
        sys_power_set_sleep_enabled(false);
    } else if (payload[0] == 0xff) {
        sys_power_set_sleep_enabled(true);
    }
    comm_transmit(PACKET_SLEEP, 0);
}

static void handle_packet_battery_info(void) {
    payload[0] = sys_power_get_battery_status();
    payload[1] = sys_power_get_battery_percent();

    uint16_t voltage = sys_power_get_battery_voltage();
    payload[2] = voltage & 0xff;
    payload[3] = voltage >> 8;

    uint16_t adc_value = sys_power_get_battery_last_reading();
    payload[4] = adc_value & 0xff;
    payload[5] = adc_value >> 8;

    comm_transmit(PACKET_BATTERY_INFO, 6);
}

static void handle_packet_battery_calib(void) {
    if (payload[0] == 0x00) {
        state.flags &= ~SYSTEM_FLAG_BATTERY_CALIBRATION;
    } else if (payload[0] == 0xff) {
        state.flags |= SYSTEM_FLAG_BATTERY_CALIBRATION;
        state.battery_calib_color = DISPLAY_COLOR_BLACK;
        sound_set_volume(SOUND_VOLUME_OFF);
        display_set_contrast(DISPLAY_DEFAULT_CONTRAST);
    }
    comm_transmit(PACKET_BATTERY_CALIB, 0);
}

static void handle_packet_battery_load(void) {
    if (!(state.flags & SYSTEM_FLAG_BATTERY_CALIBRATION)) {
        return;
    }

    uint8_t contrast = payload[0];
    disp_color_t color = payload[1];

    if (color > DISPLAY_COLOR_WHITE) {
        color = DISPLAY_COLOR_WHITE;
    }

    display_set_contrast(contrast);
    state.battery_calib_color = color;

    comm_transmit(PACKET_BATTERY_LOAD, 0);
}

static void comm_receive_internal(void) {
    if (!sys_uart_available()) return;
    if (sys_uart_read() != PACKET_SIGNATURE) return;

    const packet_type_t type = sys_uart_read();
    const uint8_t packet_length = sys_uart_read();
    uint8_t payload_length = packet_length < PACKET_HEADER_SIZE ? 0 :
                             packet_length - PACKET_HEADER_SIZE + 1;

    uint8_t count = payload_length;
    uint8_t* ptr = payload;
    while (count--) {
        *ptr++ = sys_uart_read();
    }

    if (type == PACKET_VERSION) {
        handle_packet_version();
    } else if (type == PACKET_SPI) {
        handle_packet_spi(payload_length);
    } else if (type == PACKET_LOCK) {
        handle_packet_lock();
    } else if (type == PACKET_SLEEP) {
        handle_packet_sleep();
    } else if (type == PACKET_BATTERY_INFO) {
        handle_packet_battery_info();
    } else if (type == PACKET_BATTERY_CALIB) {
        handle_packet_battery_calib();
    } else if (type == PACKET_BATTERY_LOAD) {
        handle_packet_battery_load();
    }
}

static systime_t last_time;

void comm_receive(void) {
    do {
        comm_receive_internal();

        if (locked) {
            // blink the LED as an indicator that the device is locked.
            systime_t time = time_get();
            if (time - last_time > millis_to_ticks(LOCK_BLINK_DURATION)) {
                led_toggle();
                last_time = time;
            }
        }

#ifdef SIMULATION
        // listen for connection lost (also done in main loop).
        sim_uart_listen();
#endif

    } while (locked);

    led_clear();
}

#ifdef SIMULATION
void sim_uart_connection_lost_callback(void) {
    // Flash and EEPROM may have been changed, save them.
    sim_flash_save();
    sim_eeprom_save();
}
#endif